#define SS_FACTOR  5

typedef struct {
	int cw, ch;    /* character width and height without margins */
	int cx, cy;    /* left and top edges where centered lines are drawn */
	int lw;        /* line width */
	int xmargin;   /* x-margin */
	int charwidth; /* character width including margins */
	int factor;    /* subpixel factor */
	int numchars;  /* number of characters */
	int cols;      /* number of columns in buffer */
	int rows;      /* number of rows in buffer */
	int width;     /* buffer width in pixels */
	int height;    /* buffer height in pixels */
	uchar *data;   /* picture data */
	Picture mask;  /* picture mask */
} BDBuffer;

int bd_initbuffer(BDBuffer *buf, int cw, int ch, int cx, int cy, int lw, int xmargin, int numchars, int factor);
void bd_createmask(BDBuffer *buf);
void bd_getmaskcoords(BDBuffer *buf, int idx, int *x, int *y);
uchar *bd_getsymbol(BDBuffer *buf, int idx);
uchar bd_avgintensity(uchar *data, int cw, int f);
void bd_downsample(BDBuffer *dstbuf, int dstidx, BDBuffer *srcbuf, int srcidx, int numchars);
void bd_drawrect(BDBuffer *buf, int idx, int x, int y, int w, int h, int alpha);
void bd_drawlineup(BDBuffer *buf, int idx);
void bd_drawlinedown(BDBuffer *buf, int idx);
void bd_drawlineleft(BDBuffer *buf, int idx);
void bd_drawlineright(BDBuffer *buf, int idx);
void bd_drawroundedcorners(BDBuffer *buf, int br, int bl, int tl, int tr);
void bd_drawcircle(BDBuffer *buf, int idx, int fill);
void bd_drawhorizfadingline(BDBuffer *buf, int idx, int left);
void bd_drawvertfadingline(BDBuffer *buf, int idx, int up);
void bd_drawhdashes(BDBuffer *buf, int idx, int n, int heavy);
void bd_drawvdashes(BDBuffer *buf, int idx, int n, int heavy);
void bd_drawdiagonals(BDBuffer *buf, int lr, int rl, int cross);
void bd_drawblockpatterns(BDBuffer *buf, int idx, uchar *blockpatterns, int len, int rows);
void bd_drawtriangle(BDBuffer *buf, int idx, int ax, int ay, int bx, int by, int cx, int cy, int alpha);
void bd_copysymbol(BDBuffer *buf, int dstidx, int srcidx, int fliphoriz);
void bd_erasesymbol(BDBuffer *buf, int idx);
void bd_errormsg(char *msg);
