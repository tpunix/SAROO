
#include <windows.h>
#include <vfw.h>
#include <stdio.h>
#include <stdlib.h>


#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>



typedef struct {
	AVFormatContext *fmt_ctx;

	int video_idx;
	int audio_idx;
	int frames;

	AVCodecParameters *v_par;
	AVCodecContext *v_ctx;
	AVCodec* v_codec;

	AVCodecParameters *a_par;
	AVCodecContext *a_ctx;
	AVCodec* a_codec;

}INPUT_CONTEXT;


typedef struct {
	// video params
	int width;
	int height;
	int frame_rate;
	int key_frame;
	int quality;
	int bit_rate;

	// audio_params
	int sample_rate;
	int nb_channels;
	int sample_res;

	enum AVSampleFormat sample_fmt;


	BITMAPINFOHEADER bmpi;
	BITMAPINFOHEADER bmpo;
	COMPVARS cv;
	ICCOMPRESS icCompress;

	AVFormatContext *fmt_ctx;
	AVStream *vst;
	AVStream *ast;

}OUTPUT_CONTEXT;


INPUT_CONTEXT ictx;

OUTPUT_CONTEXT octx;

struct SwrContext *swr_ctx;


AVFilterContext *vf_sink_ctx;
AVFilterContext *vf_src_ctx;
AVFilterGraph *vf_graph;

AVFilterContext *af_sink_ctx;
AVFilterContext *af_src_ctx;
AVFilterGraph *af_graph;


int crop_t = -1;
int crop_b = -1;
int crop_l = -1;
int crop_r = -1;
int scale_w = -1;
int scale_h = -1;
int fps = -1;

int a_f = -1;
int a_ch = -1;

int e_q = -1;
int e_key = -1;
int e_br = -1;

/******************************************************************************/


int load_input(char *filename)
{
	int retv;

	retv = avformat_open_input(&ictx.fmt_ctx, filename, NULL, NULL);
	if(retv<0){
		printf("Failed to open input file: %s\n", av_err2str(retv));
		return -1;
	}

	// 获取流信息
	if(avformat_find_stream_info(ictx.fmt_ctx, NULL)<0){
		printf("Failed to find stream info.\n");
		return -1;
	}

    // 查找视频流
	ictx.video_idx = -1;
	ictx.audio_idx = -1;
	for(int i=0; i<ictx.fmt_ctx->nb_streams; i++){
		if(ictx.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && ictx.video_idx==-1){
			ictx.video_idx = i;
		}else if(ictx.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && ictx.audio_idx==-1){
			ictx.audio_idx = i;
		}
	}
	if(ictx.video_idx==-1){
		printf("No video stream found.\n");
		return -1;
	}

	AVStream* st = ictx.fmt_ctx->streams[ictx.video_idx];
	ictx.v_par = st->codecpar;
	ictx.frames = st->nb_frames;

	printf("\nStream %d: Video [%s]\n", ictx.video_idx, avcodec_get_name(ictx.v_par->codec_id));
	printf("    Res: %dx%d\n", ictx.v_par->width, ictx.v_par->height);
	printf("    Frame rate: %f\n", av_q2d(ictx.v_par->framerate));
	printf("    Bit rate: %lld\n", ictx.v_par->bit_rate);
	printf("    Frame count: %d\n", ictx.frames);
	printf("    Duration: %s\n", av_ts2timestr(st->duration, &st->time_base));

	// 视频解码器
	ictx.v_codec = (AVCodec*)avcodec_find_decoder(ictx.v_par->codec_id);
	if(!ictx.v_codec){
		printf("未找到视频解码器! 编码ID: %d\n", ictx.v_par->codec_id);
		return -2;
	}

	ictx.v_ctx = avcodec_alloc_context3(ictx.v_codec);
	if(!ictx.v_ctx){
		printf("无法分配视频解码器上下文!\n");
		return -3;
	}

	if(avcodec_parameters_to_context(ictx.v_ctx, ictx.v_par)<0){
		printf("无法复制视频编码参数!\n");
		return -4;
	}

	if(avcodec_open2(ictx.v_ctx, ictx.v_codec, NULL)<0){
		printf("无法打开视频解码器!\n");
		return -5;
	}


	// 音频解码器（如果存在）
	if(ictx.audio_idx!=-1){
		AVStream *st = ictx.fmt_ctx->streams[ictx.audio_idx];
		ictx.a_par = st->codecpar;

		printf("\nStream %d: Audio [%s]\n", ictx.audio_idx, avcodec_get_name(ictx.a_par->codec_id));
		printf("    Sample rate: %d\n", ictx.a_par->sample_rate);
		printf("    Channels: %d\n", ictx.a_par->ch_layout.nb_channels);
		printf("    Bit rate: %lld\n", ictx.v_par->bit_rate);
		printf("    Duration: %s\n", av_ts2timestr(st->duration, &st->time_base));

		ictx.a_codec = (AVCodec*)avcodec_find_decoder(ictx.a_par->codec_id);
		if(!ictx.a_codec){
			printf("未找到音频解码器! ID=%d\n", ictx.a_par->codec_id);
		}else{
			ictx.a_ctx = avcodec_alloc_context3(ictx.a_codec);
			if(!ictx.a_ctx){
				printf("无法分配音频解码器上下文!\n");
				return -6;
			}

			if(avcodec_parameters_to_context(ictx.a_ctx, ictx.a_par)<0){
				printf("无法复制音频编码参数!\n");
				return -7;
			}

			if(avcodec_open2(ictx.a_ctx, ictx.a_codec, NULL)<0){
				printf("无法打开音频解码器!\n");
				return -8;
			}
		}
	}

	return 0;
}


/******************************************************************************/

// 视频裁剪与缩放
int transform_init(void)
{
	char filters_desc[512];
	int p = 0;
	int step = 0;

	AVRational tb  = ictx.fmt_ctx->streams[ictx.video_idx]->time_base;
	AVRational sar = ictx.v_ctx->sample_aspect_ratio;
	AVRational fr  = ictx.v_par->framerate;
	p += sprintf(filters_desc+p,
		"buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[step_%d]; ",
		ictx.v_ctx->width, ictx.v_ctx->height, ictx.v_ctx->pix_fmt,
		tb.num, tb.den, sar.num, sar.den, fr.num, fr.den, step
	);

	octx.frame_rate = av_q2d(ictx.v_par->framerate)+0.5;
	if(fps!=-1){
		ictx.frames = ictx.frames * fps / octx.frame_rate;
		p += sprintf(filters_desc+p, "[step_%d]fps=%d[step_%d]; ", step, fps, step+1);
		step += 1;
		octx.frame_rate = fps;
	}


	if(crop_t!=-1){
		int crop_w = ictx.v_par->width - crop_l - crop_r;
		int crop_h = ictx.v_par->height - crop_t - crop_b;
		p += sprintf(filters_desc+p, "[step_%d]crop=%d:%d:%d:%d[step_%d]; ", step, crop_w, crop_h, crop_l, crop_t, step+1);
		step += 1;
	}

	if(scale_w==-1 && scale_h==-1){
		octx.width = ictx.v_par->width;
		octx.height = ictx.v_par->height;
	}else{
		if(scale_w!=-1 && scale_h!=-1){
			octx.width = scale_w;
			octx.height = scale_h;
		}else if(scale_w!=-1){
			float ow = ictx.v_par->width;
			float oh = ictx.v_par->height;
			float nw = scale_w;
			int nh = 0.5+nw*oh/ow;
			octx.width = scale_w;
			octx.height = nh;
		}else if(scale_h!=-1){
			float ow = ictx.v_par->width;
			float oh = ictx.v_par->height;
			float nh = scale_w;
			int nw = 0.5+ow*nh/oh;
			octx.width = nw;
			octx.height = scale_h;
		}
		p += sprintf(filters_desc+p, "[step_%d]scale=%d:%d[step_%d]; ", step, octx.width, octx.height, step+1);
		step += 1;
	}

	p += sprintf(filters_desc+p, "[step_%d]%s[step_%d]; ", step, "vflip", step+1);
	step += 1;

	// 3: AV_PIX_FMT_BGR24
	p += sprintf(filters_desc+p, "[step_%d]%s", step, "buffersink=pix_fmts=03000000");
	step += 1;

	printf("\nVideo Filter: [%s]\n", filters_desc);
	printf("Video output: %dx%d  fps=%d  frames=%d\n", octx.width, octx.height, octx.frame_rate, ictx.frames);


	if(e_q!=-1){
		if(e_q>100)
			e_q = 100;
		octx.quality = e_q*100;
	}else{
		octx.quality = 10000;
	}

	if(e_key!=-1){
		octx.key_frame = e_key;
	}else{
		if(octx.frame_rate>20){
			octx.key_frame = octx.frame_rate/3;
		}else if(octx.frame_rate>10){
			octx.key_frame = octx.frame_rate/2;
		}else{
			octx.key_frame = octx.frame_rate;
		}
	}

	if(e_br!=-1){
		octx.bit_rate = e_br;
	}else{
		octx.bit_rate = 0;
	}


	if(a_f!=-1){
		octx.sample_rate = a_f;
	}else{
		octx.sample_rate = ictx.a_par->sample_rate;
	}

	if(a_ch!=-1){
		octx.nb_channels = a_ch;
	}else{
		octx.nb_channels = ictx.a_par->ch_layout.nb_channels;
	}

	octx.sample_res = 16;

	///////////////////////////////////////////////////////////////////////////////

	int retv = 0;

	vf_graph = avfilter_graph_alloc();
	if (!vf_graph) {
		return AVERROR(ENOMEM);
	}

	AVFilterInOut *outputs, *inputs;
	retv = avfilter_graph_parse2(vf_graph, filters_desc, &inputs, &outputs);
	if(retv<0)
		return retv;


	vf_src_ctx  = avfilter_graph_get_filter(vf_graph, "Parsed_buffer_0");

	char sink_name[32];
	sprintf(sink_name, "Parsed_buffersink_%d", step);
	vf_sink_ctx = avfilter_graph_get_filter(vf_graph, sink_name);
#if 0
	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_BGR24, AV_PIX_FMT_NONE };
	retv = av_opt_set_int_list(vf_sink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	if(retv<0){
		av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
		return -1;
	}
#endif

	retv = avfilter_graph_config(vf_graph, NULL);
	if(retv<0)
		return retv;


	///////////////////////////////////////////////////////////////////////////////


	if(ictx.audio_idx!=-1){
		printf("Audio output: %d %dch\n", octx.sample_rate, octx.nb_channels);

		af_graph = avfilter_graph_alloc();
		if (!af_graph) {
			return AVERROR(ENOMEM);
		}

		char orig_layout[32];
		av_channel_layout_describe(&ictx.a_ctx->ch_layout, orig_layout, 32);

		char *new_layout;
		if(octx.nb_channels==2){
			new_layout = "stereo";
			octx.sample_fmt = AV_SAMPLE_FMT_S16P;
		}else{
			new_layout = "mono";
			octx.sample_fmt = AV_SAMPLE_FMT_S16;
		}

		const char *orig_fmts = av_get_sample_fmt_name(ictx.a_ctx->sample_fmt);

		tb = ictx.fmt_ctx->streams[ictx.audio_idx]->time_base;

		int samples = octx.sample_rate/4;

		sprintf(filters_desc,
			"abuffer=sample_rate=%d:sample_fmt=%s:channel_layout=%s:time_base=%d/%d[step_0]; "
			"[step_0]aformat=sample_rates=%d:sample_fmts=s16p:channel_layouts=%s[step_1]; "
			"[step_1]asetnsamples=n=%d[step_2]; "
			"[step_2]abuffersink",
			ictx.a_ctx->sample_rate, orig_fmts, orig_layout, tb.num, tb.den,
			octx.sample_rate, new_layout, samples
		);

		printf("Audio filter: '%s'\n", filters_desc);

		AVFilterInOut *aout, *ain;
		retv = avfilter_graph_parse2(af_graph, filters_desc, &ain, &aout);
		if(retv<0)
			return retv;

		retv = avfilter_graph_config(af_graph, NULL);
		if(retv<0)
			return retv;

		af_src_ctx  = avfilter_graph_get_filter(af_graph, "Parsed_abuffer_0");
		af_sink_ctx = avfilter_graph_get_filter(af_graph, "Parsed_abuffersink_3");
	}

	return 0;
}


/******************************************************************************/

int encoder_init(void)
{
	AVIFileInit();

	// 设置输入格式
	octx.bmpi.biSize = sizeof(BITMAPINFOHEADER);
	octx.bmpi.biWidth = octx.width;
	octx.bmpi.biHeight = octx.height;
	octx.bmpi.biPlanes = 1;
	octx.bmpi.biBitCount = 24;
	octx.bmpi.biCompression = BI_RGB;
	octx.bmpi.biSizeImage = octx.width * octx.height * 3;

	// 设置输出格式
	octx.bmpo.biSize = sizeof(BITMAPINFOHEADER);
	octx.bmpo.biWidth = octx.width;
	octx.bmpo.biHeight = octx.height;
	octx.bmpo.biPlanes = 1;
	octx.bmpo.biBitCount = 24;
	octx.bmpo.biCompression = mmioFOURCC('c', 'v', 'i', 'd');;

	octx.cv.dwFlags = ICMF_COMPVARS_VALID;
	octx.cv.fccHandler = mmioFOURCC('c', 'v', 'i', 'd');
	octx.cv.lQ = octx.quality;
	octx.cv.lKey = octx.key_frame;
	octx.cv.lDataRate = octx.bit_rate;

	printf("Cinepak: Quality=%d  KeyRate=%d\n\n", octx.quality/100, octx.key_frame);

	octx.cv.hic = ICOpen(ICTYPE_VIDEO, octx.cv.fccHandler, ICMODE_COMPRESS);
	if(octx.cv.hic==0){
		printf("Failed to open codec 'cvid'!");
		return -1;
	}

	// 验证压缩格式
	if(ICCompressGetFormat(octx.cv.hic, &octx.bmpi, &octx.bmpo) < 0) {
		printf("Failed to get output format.\n");
		return -2;
	}

	return 0;
}


/******************************************************************************/


int output_init(char *filename)
{
	int retv;

	retv = avformat_alloc_output_context2(&octx.fmt_ctx, NULL, "film_cpk", filename);
	if(retv<0){
		printf("Failed to allocate output context! %s\n", av_err2str(retv));
		return -1;
	}

	// 创建视频流
	octx.vst = avformat_new_stream(octx.fmt_ctx, NULL);
	if(!octx.vst){
		printf("Failed to create video stream!\n");
		return -2;
	}

	// 设置视频流参数 (Cinepak)
	octx.vst->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	octx.vst->codecpar->codec_id = AV_CODEC_ID_CINEPAK;
	octx.vst->codecpar->width = octx.width;
	octx.vst->codecpar->height = octx.height;
	octx.vst->codecpar->format = AV_PIX_FMT_RGB24;
	octx.vst->time_base = (AVRational){1, octx.frame_rate};

	// 创建输出音频流（如果有音频）
	if(ictx.a_ctx) {
		octx.ast = avformat_new_stream(octx.fmt_ctx, NULL);
		if(!octx.ast) {
			printf("无法创建输出音频流!\n");
			return -3;
		}
		octx.ast->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
		octx.ast->codecpar->codec_id = AV_CODEC_ID_PCM_S16BE_PLANAR;
		octx.ast->codecpar->sample_rate = octx.sample_rate;
		octx.ast->codecpar->bits_per_raw_sample = ictx.a_ctx->bits_per_raw_sample;
		octx.ast->codecpar->bits_per_coded_sample = ictx.a_ctx->bits_per_coded_sample;
		octx.ast->codecpar->ch_layout = ictx.a_ctx->ch_layout;
		octx.ast->codecpar->format = AV_SAMPLE_FMT_S16P;
		octx.ast->codecpar->frame_size = 0; // PCM 无固定帧大小
		octx.ast->time_base = (AVRational){1, octx.sample_rate};
	}

	// 打开输出文件
	if(!(octx.fmt_ctx->oformat->flags & AVFMT_NOFILE)){
		retv = avio_open(&octx.fmt_ctx->pb, octx.fmt_ctx->url, AVIO_FLAG_WRITE);
		if(retv<0){
			printf("Failed to open output file! %s\n", av_err2str(retv));
			return -1;
		}
	}

	// 写入文件头
	retv = avformat_write_header(octx.fmt_ctx, NULL);
	if(retv<0){
		printf("Failed to write header! %s\n", av_err2str(retv));
		return -1;
	}

	return 0;
}



/******************************************************************************/


void show_usage(void)
{
	printf("\nUsage:\n");
	printf("\tcpmk  [-crop t,b,l,r] [-scale wxh] [-sw w] [-sh h] [-fps n]"
			"\t      [-af freq] [-ach ch] [-q n] [-key k] [-br n] in_file out_file\n");
	printf("\t\t-crop    t,b,l,r        裁剪源视频. 参数依次为'上下左右'\n");
	printf("\t\t-scale   wxh            缩放源视频. \n");
	printf("\t\t-sw      w              按比例缩放源视频至w宽度.\n");
	printf("\t\t-sh      h              按比例缩放源视频至h高度.\n");
	printf("\t\t-fps     n              调整源视频帧率.\n");
	printf("\n");
	printf("\t\t-af      freq           调整源音频采样率.\n");
	printf("\t\t-ach     ch             调整源音频通道数\n");
	printf("\n");
	printf("\t\t-q       n              设置编码质量, 1-100.\n");
	printf("\t\t-key     k              设置编码关键帧, 每k帧一个关键帧.\n");
	printf("\t\t-br      n              设置编码码率.\n");
	printf("\n\n");
	system("pause");
}


int main(int argc, char *argv[])
{
	int retv;
	int ap = 1;
	char *input_file=NULL;
	char *output_file=NULL;

	if(argc<2){
		show_usage();
		return -1;
	}

	while(ap<argc){
		if(!strcmp(argv[ap], "-h")){
			show_usage();
			return 0;
		}else if(!strcmp(argv[ap], "-crop")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			retv = sscanf(argv[ap], "%d,%d,%d,%d", &crop_t, &crop_b, &crop_l, &crop_r);
			if(retv!=4){
				printf("Bad crop params!\n");
				goto _invalid_arg;
			}
		}else if(!strcmp(argv[ap], "-scale")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			retv = sscanf(argv[ap], "%dx%d", &scale_w, &scale_h);
			if(retv!=2){
				printf("Bad scale params!\n");
				goto _invalid_arg;
			}
		}else if(!strcmp(argv[ap], "-sw")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			scale_w = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-sh")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			scale_h = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-fps")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			fps = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-af")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			a_f = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-ach")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			a_ch = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-q")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			e_q = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-key")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			e_key = atoi(argv[ap]);
		}else if(!strcmp(argv[ap], "-br")){
			ap += 1;
			if(ap>=argc || argv[ap][0]=='-')
				goto _invalid_arg;
			e_br = atoi(argv[ap]);
		}else if(argv[ap][0]!='-'){
			if(input_file==NULL){
				input_file = argv[ap];
			}else if(output_file==NULL){
				output_file = argv[ap];
			}
		}else{
			printf("Invalid arg! [%s]\n", argv[ap]);
_invalid_arg:
			show_usage();
			exit(-1);
		}
		ap += 1;
	}

	if(input_file==NULL){
		printf("No input file!\n");
		return -1;
	}

	///////////////////////////////////////////////////////////////////////////////

	memset(&ictx, 0, sizeof(ictx));
	memset(&octx, 0, sizeof(ictx));

	retv = load_input(input_file);
	if(retv)
		return retv;


	retv = transform_init();
	if(retv)
		return retv;


	retv = encoder_init();
	if(retv)
		return retv;


	if(output_file==NULL){
		return 0;
	}
	retv = output_init("output.cpk");
	if(retv)
		return retv;


	///////////////////////////////////////////////////////////////////////////////




	// FFmpeg 帧和包
	AVFrame *v_frame = av_frame_alloc();
	AVFrame *vf_frame = av_frame_alloc();
	AVFrame *a_frame = av_frame_alloc();
	AVFrame *af_frame = av_frame_alloc();
	AVPacket *pkt = av_packet_alloc();

	// 分配 RGB24 缓冲区
	uint8_t *rgb_buffer = (uint8_t*)malloc(octx.bmpi.biSizeImage*2);
	uint8_t *vin_buffer[2] = {
		rgb_buffer,
		rgb_buffer+octx.bmpi.biSizeImage
	};
	int vp = 0;

	uint8_t *out_buffer = (uint8_t*)malloc(octx.bmpi.biSizeImage);

    // 读取和处理帧
    int frame_count = 0;
	int key_count = 0;
    while(av_read_frame(ictx.fmt_ctx, pkt)>=0){
        if(pkt->stream_index==ictx.video_idx){
            // 解码视频帧
            retv = avcodec_send_packet(ictx.v_ctx, pkt);
            if(retv<0) {
                printf("Failed to send packet! %s\n", av_err2str(retv));
                break;
            }
			av_packet_unref(pkt);

            while(retv>=0){
                retv = avcodec_receive_frame(ictx.v_ctx, v_frame);
                if(retv==AVERROR(EAGAIN) || retv==AVERROR_EOF){
                    break;
                }else if(retv<0) {
                    printf("Failed to receive frame! %s\n", av_err2str(retv));
                    break;
                }

				v_frame->pts = v_frame->best_effort_timestamp;
				retv = av_buffersrc_add_frame_flags(vf_src_ctx, v_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
				if(retv){
					av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
					break;
				}

				while(1){
					retv = av_buffersink_get_frame(vf_sink_ctx, vf_frame);
					if(retv==AVERROR(EAGAIN) || retv==AVERROR_EOF){
						break;
					}else if(retv<0){
						break;
					}

					printf("  %d/%d  %d%%...      \r", frame_count, ictx.frames, frame_count*100/ictx.frames);
					memcpy(vin_buffer[vp], vf_frame->data[0], octx.bmpi.biSizeImage);

					// VfW 压缩
					DWORD dwFlags = (key_count==0)? ICCOMPRESS_KEYFRAME : 0;
					DWORD aviFlags = 0;
					DWORD request_size = octx.bit_rate*1024/octx.frame_rate;
					DWORD result = ICCompress(octx.cv.hic, dwFlags, &octx.bmpo, out_buffer, &octx.bmpi, vin_buffer[vp],
									NULL, &aviFlags, frame_count, request_size, octx.quality, &octx.bmpi, vin_buffer[vp^1]);
					if (result != ICERR_OK) {
							printf("Failed to compress frame %d.\n", frame_count);
							break;
					}
					key_count += 1;
					if(key_count==octx.key_frame){
							key_count = 0;
					}
					vp ^= 1;

					int size = octx.bmpo.biSizeImage;
					int isKeyFrame = (aviFlags&AVIIF_KEYFRAME)? 1: 0;

					// 创建输出包
					AVPacket out_pkt = { 0 };
					retv = av_new_packet(&out_pkt, size);
					if(retv<0){
							printf("Failed to allocate output packet! %s\n", av_err2str(retv));
							break;
					}
					memcpy(out_pkt.data, out_buffer, size);

					int old_pts = (int)vf_frame->pts;
					out_pkt.stream_index = octx.vst->index;
					out_pkt.pts = av_rescale_q(frame_count, (AVRational){1, octx.frame_rate}, octx.vst->time_base);
					//out_pkt.pts = frame_count*1000/octx.frame_rate;
					out_pkt.dts = out_pkt.pts;
					out_pkt.duration = av_rescale_q(1, (AVRational){1, octx.frame_rate}, octx.vst->time_base);
					//out_pkt.duration = 1000/octx.frame_rate;
					out_pkt.flags = isKeyFrame ? AV_PKT_FLAG_KEY : 0;

					//printf(" Video PTS: %d->%d  Duration: %d\n", old_pts, (int)out_pkt.pts, (int)out_pkt.duration);

					// 写入包
					retv = av_interleaved_write_frame(octx.fmt_ctx, &out_pkt);
					av_packet_unref(&out_pkt);
					if(retv<0){
						printf("Failed to write frame %d: %s\n", frame_count, av_err2str(retv));
						frame_count++;
						break;
					}

					frame_count++;
				}
			}

		}else if(pkt->stream_index==ictx.audio_idx && ictx.a_ctx){
            retv = avcodec_send_packet(ictx.a_ctx, pkt);
            if(retv<0){
                printf("无法发送音频包! %s\n", av_err2str(retv));
                break;
            }
			av_packet_unref(pkt);

			while(retv>=0){
				retv = avcodec_receive_frame(ictx.a_ctx, a_frame);
				if (retv==AVERROR(EAGAIN) || retv==AVERROR_EOF){
					break;
				}else if(retv<0){
					fprintf(stderr, "无法接收音频帧: %s\n", av_err2str(retv));
					break;
				}

				a_frame->pts = a_frame->best_effort_timestamp;
				retv = av_buffersrc_add_frame_flags(af_src_ctx, a_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
				if(retv){
					av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
					break;
				}

				while(1){
					retv = av_buffersink_get_frame(af_sink_ctx, af_frame);
					if(retv==AVERROR(EAGAIN) || retv==AVERROR_EOF){
						break;
					}else if(retv<0){
						break;
					}


					// 创建输出音频包
					int out_size = af_frame->nb_samples * octx.nb_channels * 2;
					AVPacket out_pkt = { 0 };
					retv = av_new_packet(&out_pkt, out_size);
					if(retv<0){
						printf("无法分配输出音频包! %s\n", av_err2str(retv));
						break;
					}

					// 合并Planr数据并转换为BigEndian
					uint8_t *abuf0 = af_frame->data[0];
					uint8_t *dst0 = out_pkt.data;
					int ch_size = af_frame->nb_samples * 2;
					for(int i=0; i<ch_size; i+=2){
						dst0[i+0] = abuf0[i+1];
						dst0[i+1] = abuf0[i+0];
					}
					if(octx.nb_channels==2){
						uint8_t *abuf1 = af_frame->data[1];
						uint8_t *dst1 = out_pkt.data+ch_size;
						for(int i=0; i<ch_size; i+=2){
							dst1[i+0] = abuf1[i+1];
							dst1[i+1] = abuf1[i+0];
						}
					}

					int old_pts = (int)af_frame->pts;
					out_pkt.stream_index = octx.ast->index;
					out_pkt.pts = av_rescale_q(af_frame->pts, (AVRational){1, octx.sample_rate}, octx.ast->time_base);
					//out_pkt.pts = af_frame->pts*1000/octx.sample_rate;
					out_pkt.dts = out_pkt.pts;
					out_pkt.duration = av_rescale_q(af_frame->nb_samples, (AVRational){1, octx.sample_rate}, octx.ast->time_base);
					//out_pkt.duration = af_frame->nb_samples*1000/octx.sample_rate;

					//printf(" Audio PTS: %d->%d  Duration: %d\n", old_pts, (int)out_pkt.pts, (int)out_pkt.duration);

	                // 交错写入音频包
					retv = av_interleaved_write_frame(octx.fmt_ctx, &out_pkt);
	  				av_packet_unref(&out_pkt);
					if(retv<0){
						printf("无法写入音频帧! %s\n", av_err2str(retv));
						break;
					}
				}
			}
		}
	}


	// 写入文件尾
	av_write_trailer(octx.fmt_ctx);
	avio_closep(&octx.fmt_ctx->pb);
	avformat_free_context(octx.fmt_ctx);

	printf("\nVideo encoding completed.\n");
	return 0;
}

