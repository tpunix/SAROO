
#include "main.h"


/******************************************************************************/

int lang_id = 0;

#define LANG_STR_NR 19
#define LANG_NR 8

typedef struct _str_entry
{
	u32 hash;
	char *str;
	int index;
	struct _str_entry *next;
}STR_ENTRY;

STR_ENTRY lang_str[LANG_STR_NR];
STR_ENTRY *lang_str_table[64];


/******************************************************************************/


char *lang_zhcn[LANG_STR_NR] = {
	"选择游戏(%d/%d)",
	"选择文件(%d/%d)",
	"游戏启动中......",
	"游戏启动失败! %d",
	"加载文件中......",
	"文件加载失败! %d",
	"选择游戏",
	"系统CD播放器",
	"运行光盘游戏",
	"串口调试工具",
	"运行二进制文件",
	"固件升级",
	"升级中,请勿断电...",
	"升级失败!",
	"升级完成,请重新开机!",
	"SAROO Boot Menu",
	"检查光盘中......",
	"未发现光盘!",
	"不是游戏光盘!",
};


char *lang_en[LANG_STR_NR] = {
	"Select Game(%d/%d)",
	"Select File(%d/%d)",
	"Game Booting ......",
	"Game Boot Failed! %d",
	"Loading File ......",
	"Load File Failed! %d",
	"Select Game",
	"System CDPlayer",
	"Load Game Disc",
	"Serial Debug Shell",
	"Load Binary",
	"Firm Update",
	"Updating... Don't PowerOff!",
	"Update Failed!",
	"Update Finish! Please PowerOn again!",
	"SAROO Boot Menu",
	"Checking Disc ......",
	"No Disc Found!",
	"Not a Game Disc!",
};


char *lang_ptbr[LANG_STR_NR] = {
	"Selecionar Jogo(%d/%d)",
	"Selecionar Arquivo(%d/%d)",
	"Inicializando Jogo ......",
	"Erro ao inicializar o jogo! %d",
	"Carregando arquivo ......",
	"Erro ao carregar o arquivo! %d",
	"Selecionar Jogo",
	"Tocador de CD do Sistema",
	"Carregar o Disco do Jogo",
	"Shell de Depuração serial",
	"Carregando Binário",
	"Atualizar a Firmware",
	"Atualizando... Não desligue o sistema!",
	"Erro ao atualizar!",
	"Finalizado, reinicie o sistema!",
	"SAROO Menu de Inicialização",
	"Verificando o disco ......",
	"Sem CD!",
	"Não é um disco de jogo!",
};


char *lang_ja[LANG_STR_NR] = {
	"ゲームリスト(%d/%d)",
	"ファイルリスト(%d/%d)",
	"ロード中......",
	"ゲームを起動できない！ %d",
	"ファイルをロード中......",
	"ファイルを起動できない！ %d",
	"ゲームリスト",
	"CDプレイヤー",
	"CDROMを起動",
	"シリアルポートデバッグツール",
	"バイナリファイルを起動",
	"ファームウェアのアップデート",
	"アップデート中、電源を切らないでください...",
	"アップデート失敗！",
	"アップデート完了、再起動してください！",
	"SAROO ブートメニュー",
	"ディスクを確認しています......,
	"ディスクが入っていません",
	"ゲームディスクではありません,
};


char **lang_cur;


/******************************************************************************/

u32 str_hash(char *str)
{
	int len = strlen(str);
	u32 hash = 5381;
	int i;

	for(i=0; i<len; i++){
		hash = ((hash<<5)+hash) ^ (u8)str[i];
	}

	return hash;
}

char *TT(char *str)
{
	if(lang_id==0 || lang_id>=LANG_NR)
		return str;

	u32 hash = str_hash(str);
	STR_ENTRY *entry = lang_str_table[hash&0xff];
	while(entry){
		if(hash==entry->hash){
			return lang_cur[entry->index];
		}
		entry = entry->next;
	}

	return str;
}

/******************************************************************************/


void lang_init(void)
{
	int i;

	memset(lang_str_table, 0, sizeof(lang_str_table));
	memset(lang_str, 0, sizeof(lang_str));

	for(i=0; i<LANG_STR_NR; i++){
		lang_str[i].str = lang_zhcn[i];
		lang_str[i].hash = str_hash(lang_zhcn[i]);
		lang_str[i].index = i;

		int t = (lang_str[i].hash)&0xff;
		if(lang_str_table[t]){
			lang_str[i].next = lang_str_table[t];
		}
		lang_str_table[t] = &lang_str[i];

		//printk("%2d: %08x %s\n", i, lang_str[i].hash, lang_str[i].str);
	}

	lang_cur = NULL;
	if(lang_id==1){
		lang_cur = lang_en;
	}else if(lang_id==2){
		lang_cur = lang_ptbr;
	}else if(lang_id==3){
		lang_cur = lang_ja;
	}
}

