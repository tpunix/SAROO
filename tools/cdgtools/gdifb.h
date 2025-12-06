

extern unsigned char *gdifbuf;
#define gdifb_llen  (2048*4)

int gdifb_init(int w, int h);
void gdifb_flush(void);
int gdifb_exit(int force);


void gdifb_textout(int x, int y, char *text);
void gdifb_pixel(int x, int y, int color);
void gdifb_line(int x1, int y1, int x2, int y2, int color);
void gdifb_rect(int x1, int y1, int x2, int y2, int color);
void gdifb_box(int x1, int y1, int x2, int y2, int color);

int gdifb_waitkey(void);

