#define SS_FACTOR  5

typedef struct {
	int cw, ch;
	int cx, cy;
	int lw;
	int xmargin;
	int width;
	int factor;
	int numchars;
	int charoffset;
	char *data;
	Picture mask;
} BDBuffer;

void bd_initbuffer(BDBuffer *buf, int cw, int ch, int cx, int cy, int lw, int xmargin, int numchars, int factor);
void bd_createmask(BDBuffer *buf);
char bd_avgintensity(char *data, int cw, int f);
void bd_downsample(BDBuffer *dstbuf, int dstidx, BDBuffer *srcbuf, int srcidx, int numchars);
void bd_drawrect(BDBuffer *buf, int idx, int x, int y, int w, int h);
void bd_drawlineup(BDBuffer *buf, int idx);
void bd_drawlinedown(BDBuffer *buf, int idx);
void bd_drawlineleft(BDBuffer *buf, int idx);
void bd_drawlineright(BDBuffer *buf, int idx);
void bd_drawcorners(BDBuffer *buf, int br, int bl, int tl, int tr);
void bd_drawcircle(BDBuffer *buf, int idx, int fill);
void bd_drawhorizfadingline(BDBuffer *buf, int idx, int left);
void bd_drawvertfadingline(BDBuffer *buf, int idx, int up);
void bd_drawhdashes(BDBuffer *buf, int idx, int n, int heavy);
void bd_drawvdashes(BDBuffer *buf, int idx, int n, int heavy);
void bd_drawdiagonals(BDBuffer *buf, int lr, int rl, int cross);
void bd_copysymbol(BDBuffer *buf, int dstidx, int srcidx);
