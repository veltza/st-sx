/**
 * boxdraw_extra draws dashes, diagonals, sextants, octants and proper rounded
 * corners that the original boxdraw patch doesn't implement or draw properly.
 */
static int be_generatesymbols(BDBuffer *buf, int bold);
static void be_drawlegacy(BDBuffer *ssbuf, BDBuffer *buf);
static int be_drawwedges(BDBuffer *ssbuf, BDBuffer *buf, int idx);
static void be_drawwedge(BDBuffer *ssbuf, BDBuffer *buf, int idx, int x1, int y1, int x2, int y2, int x3, int y3, int block, int invert);
static int be_draw_vertical_one_eighth_blocks(BDBuffer *buf, int idx);
static int be_draw_horizontal_one_eighth_blocks(BDBuffer *buf, int idx);
static int be_draw_one_eighth_frames(BDBuffer *buf, int idx);
static int be_draw_horizontal_one_eighth_block_1358(BDBuffer *buf, int idx);
static int be_draw_upper_one_eighth_blocks(BDBuffer *buf, int idx);
static int be_draw_right_one_eighth_blocks(BDBuffer *buf, int idx);
static int be_draw_medium_shades(BDBuffer *buf, int idx);
static int be_draw_checker_board_fill(BDBuffer *ssbuf, BDBuffer *buf, int idx);
static int be_draw_heavy_horizontal_fill(BDBuffer *buf, int idx);
static int be_draw_diagonal_fill(BDBuffer *ssbuf, BDBuffer *buf, int idx);
static int be_draw_triangular_medium_shades(BDBuffer *ssbuf, BDBuffer *buf, int idx);
static int be_draw_left_thirds_blocks(BDBuffer *buf, int idx);
static int be_draw_one_quarter_blocks(BDBuffer *buf, int idx);

struct {
	BDBuffer norm;
	BDBuffer bold;
} bdextra;

/* public API */

void
drawextrasymbol(int x, int y, int w, int h, XftColor *fg, ushort symbol, int bold)
{
	Picture src;
	int maskx, masky;
	BDBuffer *buf = bold ? &bdextra.bold : &bdextra.norm;

	if (!be_generatesymbols(buf, bold))
		return;

	if ((src = XftDrawSrcPicture(xd, fg))) {
		bd_getmaskcoords(buf, symbol, &maskx, &masky);
		XRenderComposite(xdpy, PictOpOver,
			src, buf->mask, XftDrawPicture(xd),
			0, 0, maskx, masky, x - buf->xmargin, y, buf->charwidth, buf->ch);
	}
}

void
initextrasymbols(void)
{
	int i;

	for (i = 0; i < LEN(boxlegacy); i++)
		boxlegacy[i] = (i < BE_LEGACY_LEN-6) ? i + 1 : 0;

	boxlegacy[0x93] = 0;                 /* U+1FB93 - reserved */
	boxlegacy[0xCE] = BE_LEGACY_LEN - 5; /* U+1FBCE */
	boxlegacy[0xCF] = BE_LEGACY_LEN - 4; /* U+1FBCF */
	boxlegacy[0xE4] = BE_LEGACY_LEN - 3; /* U+1FBE4 */
	boxlegacy[0xE5] = BE_LEGACY_LEN - 2; /* U+1FBE5 */
	boxlegacy[0xE6] = BE_LEGACY_LEN - 1; /* U+1FBE6 */
	boxlegacy[0xE7] = BE_LEGACY_LEN;     /* U+1FBE7 */
}

/* implementation */

int
be_generatesymbols(BDBuffer *buf, int bold)
{
	const uchar *bm = boxmisc;
	BDBuffer ssbuf;
	int cx, cy;
	int mwh = MIN(win.cw, win.ch);
	int base_lw = MAX(1, DIV(mwh, 8));
	int lw = (bold && mwh >= 6) ? MAX(base_lw + 1, DIV(3 * base_lw, 2)) : base_lw;

	if (buf->cw == win.cw && buf->ch == win.ch && buf->lw == lw)
		return 1;

	if (!XftDefaultHasRender(xdpy)) {
		bd_errormsg("boxdraw_extra: XRender is not available");
		return 0;
	}

	cx = DIV(win.cw - lw, 2);
	cy = DIV(win.ch - lw, 2);
	if (!bd_initbuffer(buf, win.cw, win.ch, cx, cy, lw, lw, BE_EXTRA_LEN, 1)) {
		bd_errormsg("boxdraw_extra: cannot allocate character buffer");
		return 0;
	} else if (!bd_initbuffer(&ssbuf, win.cw, win.ch, cx, cy, lw, lw, BE_MISC_LEN, SS_FACTOR)) {
		bd_errormsg("boxdraw_extra: cannot allocate mask buffer");
		free(buf->data);
		return 0;
	}

	/* dashes, diagonals, rounded corners */
	bd_drawhdashes(&ssbuf, bm[BE_HDASH2], 2, 0);
	bd_drawhdashes(&ssbuf, bm[BE_HDASH3], 3, 0);
	bd_drawhdashes(&ssbuf, bm[BE_HDASH4], 4, 0);
	bd_drawhdashes(&ssbuf, bm[BE_HDASH2_HEAVY], 2, 1);
	bd_drawhdashes(&ssbuf, bm[BE_HDASH3_HEAVY], 3, 1);
	bd_drawhdashes(&ssbuf, bm[BE_HDASH4_HEAVY], 4, 1);
	bd_drawvdashes(&ssbuf, bm[BE_VDASH2], 2, 0);
	bd_drawvdashes(&ssbuf, bm[BE_VDASH3], 3, 0);
	bd_drawvdashes(&ssbuf, bm[BE_VDASH4], 4, 0);
	bd_drawvdashes(&ssbuf, bm[BE_VDASH2_HEAVY], 2, 1);
	bd_drawvdashes(&ssbuf, bm[BE_VDASH3_HEAVY], 3, 1);
	bd_drawvdashes(&ssbuf, bm[BE_VDASH4_HEAVY], 4, 1);
	bd_drawroundedcorners(&ssbuf, bm[BE_ARC_DR], bm[BE_ARC_DL], bm[BE_ARC_UL], bm[BE_ARC_UR]);
	bd_drawdiagonals(&ssbuf, bm[BE_DIAG_LR], bm[BE_DIAG_RL], bm[BE_DIAG_CROSS]);
	bd_downsample(buf, 0, &ssbuf, 0, BE_MISC_LEN);

	/* octants */
	bd_drawblockpatterns(buf, BE_OCTANTS_IDX, boxdataoctants, BE_OCTANTS_LEN, 4);

	/* legacy characters (sextants, wedges, etc.)  */
	be_drawlegacy(&ssbuf, buf);

	bd_createmask(buf);
	free(ssbuf.data);
	return 1;
}

void
be_drawlegacy(BDBuffer *ssbuf, BDBuffer *buf)
{
	int idx = BE_LEGACY_IDX;

	/* sextants U+1FB00..U+1FB3B */
	bd_drawblockpatterns(buf, idx, boxdatasextants, BE_SEXTANTS_LEN, 3);
	idx += BE_SEXTANTS_LEN;

	/* wedges and other charactesr */
	idx = be_drawwedges(ssbuf, buf, idx); /* U+1FB3C..U+1FB6F and U+1FB9A..U+1FB9B */
	idx = be_draw_vertical_one_eighth_blocks(buf, idx);        /* U+1FB70..U+1FB75 */
	idx = be_draw_horizontal_one_eighth_blocks(buf, idx);      /* U+1FB76..U+1FB7B */
	idx = be_draw_one_eighth_frames(buf, idx);                 /* U+1FB7C..U+1FB80 */
	idx = be_draw_horizontal_one_eighth_block_1358(buf, idx);  /* U+1FB81 */
	idx = be_draw_upper_one_eighth_blocks(buf, idx);           /* U+1FB82..U+1FB86 */
	idx = be_draw_right_one_eighth_blocks(buf, idx);           /* U+1FB87..U+1FB8B */
	idx = be_draw_medium_shades(buf, idx);                     /* U+1FB8C..U+1FB94 */
	idx = be_draw_checker_board_fill(ssbuf, buf, idx);         /* U+1FB95..U+1FB96 */
	idx = be_draw_heavy_horizontal_fill(buf, idx);             /* U+1FB97 */
	idx = be_draw_diagonal_fill(ssbuf, buf, idx);              /* U+1FB98..U+1FB99 */
	idx += 2;                                                  /* U+1FB9A..U+1FB9B (wedge chars) */
	idx = be_draw_triangular_medium_shades(ssbuf, buf, idx);   /* U+1FB9C..U+1FB9F */
	idx = be_draw_left_thirds_blocks(buf, idx);                /* U+1FBCE..U+1FBCF */
	idx = be_draw_one_quarter_blocks(buf, idx);                /* U+1FBE4..U+1FBE7 */
}

int
be_drawwedges(BDBuffer *ssbuf, BDBuffer *buf, int idx)
{
	int cw = ssbuf->cw, ch = ssbuf->ch, cy = DIV(ch, 2);
	int x0 = 0, x1 = DIV(cw, 2);
	int y0 = 0, y1 = ch / 3, y2 = ch * 2 / 3;
	int x0e = x1 - 1, x1e = cw - 1;
	int y0e = y1 - 1, y1e = y2 - 1, y2e = ch - 1;
	int i, invert;

	bd_erasesymbol(ssbuf, 0);
	for (invert = 0, i = 0; i < 2; i++, invert ^= 1) {
		be_drawwedge(ssbuf, buf, idx++, x0, y2, x0, y2e, x0e, y2e, 0, invert);     /* U+1FB3C: '🬼', U+1FB52: '🭒' */
		be_drawwedge(ssbuf, buf, idx++, x0, y2, x0, y2e, x1e, y2e, 0, invert);     /* U+1FB3D: '🬽', U+1FB53; '🭓' */
		be_drawwedge(ssbuf, buf, idx++, x0, y1, x0, y2e, x0e, y2e, 0, invert);     /* U+1FB3E: '🬾', U+1FB54: '🭔' */
		be_drawwedge(ssbuf, buf, idx++, x0, y1, x0, y2e, x1e, y2e, 0, invert);     /* U+1FB3F: '🬿', U+1FB55: '🭕' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y2e, x0e, y2e, 0, invert);     /* U+1FB40: '🭀', U+1FB56: '🭖' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y0e, x0e, y0, 0, invert ^ 1);  /* U+1FB41: '🭁', U+1FB57: '🭗' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y0e, x1e, y0, 0, invert ^ 1);  /* U+1FB42: '🭂', U+1FB58: '🭘' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y1e, x0e, y0, 0, invert ^ 1);  /* U+1FB43: '🭃', U+1FB59: '🭙' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y1e, x1e, y0, 0, invert ^ 1);  /* U+1FB44: '🭄', U+1FB5A: '🭚' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y2e, x0e, y0, 0, invert ^ 1);  /* U+1FB45: '🭅', U+1FB5B: '🭛' */
		if (i == 0)
			be_drawwedge(ssbuf, buf, idx++, x1e, y1, x0, y1e, x1e, y1e, 3, 0); /* U+1FB46: '🭆' */
		else
			be_drawwedge(ssbuf, buf, idx++, x0, y1, x0, y1e, x1e, y1, 1, 0);   /* U+1FB5C: '🭜' */
		be_drawwedge(ssbuf, buf, idx++, x1e, y2, x1, y2e, x1e, y2e, 0, invert);    /* U+1FB47: '🭇', U+1FB5D: '🭝' */
		be_drawwedge(ssbuf, buf, idx++, x1e, y2, x0, y2e, x1e, y2e, 0, invert);    /* U+1FB48: '🭈', U+1FB5E: '🭞' */
		be_drawwedge(ssbuf, buf, idx++, x1e, y1, x1, y2e, x1e, y2e, 0, invert);    /* U+1FB49: '🭉', U+1FB5F: '🭟' */
		be_drawwedge(ssbuf, buf, idx++, x1e, y1, x0, y2e, x1e, y2e, 0, invert);    /* U+1FB4A: '🭊', U+1FB60: '🭠' */
		be_drawwedge(ssbuf, buf, idx++, x1e, y0, x1, y2e, x1e, y2e, 0, invert);    /* U+1FB4B: '🭋', U+1FB61: '🭡' */
		be_drawwedge(ssbuf, buf, idx++, x1, y0, x1e, y0, x1e, y0e, 0, invert ^ 1); /* U+1FB4C: '🭌', U+1FB62: '🭢' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x1e, y0, x1e, y0e, 0, invert ^ 1); /* U+1FB4D: '🭍', U+1FB63: '🭣' */
		be_drawwedge(ssbuf, buf, idx++, x1, y0, x1e, y0, x1e, y1e, 0, invert ^ 1); /* U+1FB4E: '🭎', U+1FB64: '🭤' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x1e, y0, x1e, y1e, 0, invert ^ 1); /* U+1FB4F: '🭏', U+1FB65: '🭥' */
		be_drawwedge(ssbuf, buf, idx++, x1, y0, x1e, y2e, x1e, y0, 0, invert ^ 1); /* U+1FB50: '🭐', U+1FB66: '🭦' */
		if (i == 0)
			be_drawwedge(ssbuf, buf, idx++, x0, y1, x0, y1e, x1e, y1e, 3, 0);  /* U+1FB51: '🭑' */
		else
			be_drawwedge(ssbuf, buf, idx++, x0, y1, x1e, y1e, x1e, y1, 1, 0);  /* U+1FB67: '🭧' */
	}
	for (invert = 1, i = 0; i < 2; i++, invert ^= 1) {
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x0, y2e, x1, cy, 0, invert);       /* U+1FB68: '🭨', U+1FB6C: '🭬' */
		be_drawwedge(ssbuf, buf, idx++, x0, y0, x1, cy, x1e, y0, 0, invert);       /* U+1FB69: '🭩', U+1FB6D: '🭭' */
		be_drawwedge(ssbuf, buf, idx++, x1e, y0, x1, cy, x1e, y2e, 0, invert);     /* U+1FB6A: '🭪', U+1FB6E: '🭮' */
		be_drawwedge(ssbuf, buf, idx++, x0, y2e, x1, cy, x1e, y2e, 0, invert);     /* U+1FB6B: '🭫', U+1FB6F: '🭯' */
	}

	/* U+1FB9A: '🮚' */
	bd_drawrect(ssbuf, 0, 0, 0, ssbuf->cw, ssbuf->ch, 0);
	bd_drawtriangle(ssbuf, 0, x0, y0, x1, cy, x1e, y0, 255);
	bd_drawtriangle(ssbuf, 0, x0, y2e, x1, cy, x1e, y2e, 255);
	bd_downsample(buf, idx + 42, ssbuf, 0, 1);

	/* U+1FB9B: '🮛' */
	bd_drawrect(ssbuf, 0, 0, 0, ssbuf->cw, ssbuf->ch, 0);
	bd_drawtriangle(ssbuf, 0, x0, y0, x0, y2e, x1, cy, 255);
	bd_drawtriangle(ssbuf, 0, x1e, y0, x1, cy, x1e, y2e, 255);
	bd_downsample(buf, idx + 43, ssbuf, 0, 1);

	return idx;
}

void
be_drawwedge(BDBuffer *ssbuf, BDBuffer *buf, int idx, int x1, int y1, int x2, int y2, int x3, int y3, int block, int invert)
{
	bd_drawrect(ssbuf, 0, 0, 0, ssbuf->cw, ssbuf->ch, invert ? 255 : 0);
	bd_drawtriangle(ssbuf, 0, x1, y1, x2, y2, x3, y3, invert ? 0 : 255);
	if (block) {
		y1 = ssbuf->ch * (block - 1) / 3;
		y2 = ssbuf->ch * block / 3;
		bd_drawrect(ssbuf, 0, 0, y1, ssbuf->cw, y2 - y1, invert ? 0 : 255);
	}
	bd_downsample(buf, idx, ssbuf, 0, 1);
}

int
be_draw_vertical_one_eighth_blocks(BDBuffer *buf, int idx)
{
	int i;

	/* U+1FB70..U+1FB75: '🭰', '🭱', '🭲', '🭳', '🭴', '🭵' */
	for (i = 1; i < 7; i++)
		bd_drawrect(buf, idx++, DIV(buf->cw * i, 8), 0, buf->lw, buf->ch, 255);

	return idx;
}

int
be_draw_horizontal_one_eighth_blocks(BDBuffer *buf, int idx)
{
	int i, lh = MAX(buf->ch / 8, 1);

	/* U+1FB76..U+1FB7B: '🭶', '🭷', '🭸', '🭹', '🭺', '🭻' */
	for (i = 1; i < 7; i++)
		bd_drawrect(buf, idx++, 0, DIV(buf->ch * i, 8), buf->cw, lh, 255);

	return idx;
}

int
be_draw_one_eighth_frames(BDBuffer *buf, int idx)
{
	int cw = buf->cw, ch = buf->ch, lw = buf->lw, lh = MAX(ch / 8, 1);

	/* U+1FB7C: '🭼' */
	bd_drawrect(buf, idx, 0, 0, lw, ch, 255);
	bd_drawrect(buf, idx++, 0, ch - lh, cw, lh, 255);

	/* U+1FB7D: '🭽' */
	bd_drawrect(buf, idx, 0, 0, lw, ch, 255);
	bd_drawrect(buf, idx++, 0, 0, cw, lh, 255);

	/* U+1FB7E: '🭾' */
	bd_drawrect(buf, idx, cw - lw, 0, lw, ch, 255);
	bd_drawrect(buf, idx++, 0, 0, cw, lh, 255);

	/* U+1FB7F: '🭿' */
	bd_drawrect(buf, idx, cw - lw, 0, lw, ch, 255);
	bd_drawrect(buf, idx++, 0, ch - lh, cw, lh, 255);

	/* U+1FB80: '🮀' */
	bd_drawrect(buf, idx, 0, 0, cw, lh, 255);
	bd_drawrect(buf, idx++, 0, ch - lh, cw, lh, 255);

	return idx;
}

int
be_draw_horizontal_one_eighth_block_1358(BDBuffer *buf, int idx)
{
	int i, cw = buf->cw, ch = buf->ch, lh = MAX(ch / 8, 1);

	/* U+1FB81: '🮁' */
	for (i = 0; i < 5; i += 2)
		bd_drawrect(buf, idx, 0, DIV((ch - lh) * i, 8), cw, lh, 255);
	bd_drawrect(buf, idx++, 0, ch - lh, cw, lh, 255);

	return idx;
}

int
be_draw_upper_one_eighth_blocks(BDBuffer *buf, int idx)
{
	int i, cw = buf->cw, ch = buf->ch;

	/* U+1FB82..U+1FB86: '🮂', '🮃', '🮄', '🮅', '🮆' */
	for (i = 2; i < 8; i++) {
		if (i != 4)
			bd_drawrect(buf, idx++, 0, 0, cw, DIV(ch * i, 8), 255);
	}

	return idx;
}

int
be_draw_right_one_eighth_blocks(BDBuffer *buf, int idx)
{
	int i, cw = buf->cw, ch = buf->ch;

	/* U+1FB87..U+1FB8B: '🮇', '🮈', '🮉', '🮊', '🮋' */
	for (i = 2; i < 8; i++) {
		if (i != 4)
			bd_drawrect(buf, idx++, cw - DIV(cw * i, 8), 0, DIV(cw * i, 8), ch, 255);
	}

	return idx;
}

int
be_draw_medium_shades(BDBuffer *buf, int idx)
{
	int cw = buf->cw, ch = buf->ch, w2 = DIV(cw, 2), h2 = DIV(ch, 2);

	/* U+1FB8C: '🮌' */
	bd_drawrect(buf, idx++, 0, 0, w2, ch, 128);

	/* U+1FB8D: '🮍' */
	bd_drawrect(buf, idx++, w2, 0, cw - w2, ch, 128);

	/* U+1FB8E: '🮎' */
	bd_drawrect(buf, idx++, 0, 0, cw, h2, 128);

	/* U+1FB8F: '🮏' */
	bd_drawrect(buf, idx++, 0, h2, cw, ch - h2, 128);

	/* U+1FB90: '🮐' */
	bd_drawrect(buf, idx++, 0, 0, cw, ch, 128);

	/* U+1FB91: '🮑' */
	bd_drawrect(buf, idx, 0, 0, cw, h2, 255);
	bd_drawrect(buf, idx++, 0, h2, cw, ch - h2, 128);

	/* U+1FB92: '🮒' */
	bd_drawrect(buf, idx, 0, 0, cw, h2, 128);
	bd_drawrect(buf, idx++, 0, h2, cw, ch - h2, 255);

	/* U+1FB93 - reserved */
	idx++;

	/* U+1FB94: '🮔' */
	bd_drawrect(buf, idx, 0, 0, w2, ch, 128);
	bd_drawrect(buf, idx++, w2, 0, cw - w2, ch, 255);

	return idx;
}

int
be_draw_checker_board_fill(BDBuffer *ssbuf, BDBuffer *buf, int idx)
{
	int i, j, x1, x2, y1, y2;

	/* U+1FB95: '🮕' */
	bd_erasesymbol(ssbuf, 0);
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 4; i += 2) {
			x1 = ssbuf->cw * (i + (j & 1)) / 4;
			x2 = ssbuf->cw * (i + (j & 1) + 1) / 4;
			y1 = ssbuf->ch * j / 4;
			y2 = ssbuf->ch * (j + 1) / 4;
			bd_drawrect(ssbuf, 0, x1, y1, x2 - x1, y2 - y1, 255);
		}
	}
	bd_downsample(buf, idx, ssbuf, 0, 1);

	/* U+1FB96: '🮖' */
	bd_copysymbol(buf, idx+1, idx, 1);

	return idx + 2;
}

int
be_draw_heavy_horizontal_fill(BDBuffer *buf, int idx)
{
	/* U+1FB97: '🮗' (same as U+1CDB7) */
	bd_copysymbol(buf, idx, BE_OCTANTS_IDX + 183, 0);

	return idx + 1;
}

int
be_draw_diagonal_fill(BDBuffer *ssbuf, BDBuffer *buf, int idx)
{
	int i, j, x1, x2, stripes = 8;
	double x, len, dx = ssbuf->cw / (double)ssbuf->ch;

	if (buf->cw < 12)
		stripes = 4;
	else if (buf->cw < 16)
		stripes = 6;

	/* U+1FB98: '🮘' */
	bd_erasesymbol(ssbuf, 0);
	for (j = 0; j < stripes; j += 2) {
		x = ssbuf->cw * j / stripes;
		len = ssbuf->cw * (j + 1) / stripes - x;
		for (i = 0; i < ssbuf->ch; i++) {
			x1 = (x < 0) ? ssbuf->cw - x : x;
			x2 = x1 + len;
			bd_drawrect(ssbuf, 0, x1, i, MIN(x2, ssbuf->cw) - x1, 1, 255);
			if (x2 > ssbuf->cw)
				bd_drawrect(ssbuf, 0, 0, i, x2 - ssbuf->cw, 1, 255);
			if ((x += dx) >= ssbuf->cw)
				x -= ssbuf->cw;
		}
	}
	bd_downsample(buf, idx, ssbuf, 0, 1);

	/* U+1FB99: '🮙' */
	bd_copysymbol(buf, idx+1, idx, 1);

	return idx + 2;
}

int
be_draw_triangular_medium_shades(BDBuffer *ssbuf, BDBuffer *buf, int idx)
{
	int cw = ssbuf->cw, ch = ssbuf->ch;

	/* U+1FB9C: '🮜' */
	bd_erasesymbol(ssbuf, 0);
	bd_drawtriangle(ssbuf, 0, 0, 0, 0, ch-1, cw-1, 0, 128);
	bd_downsample(buf, idx++, ssbuf, 0, 1);

	/* U+1FB9D: '🮝' */
	bd_erasesymbol(ssbuf, 0);
	bd_drawtriangle(ssbuf, 0, 0, 0, cw-1, ch-1, cw-1, 0, 128);
	bd_downsample(buf, idx++, ssbuf, 0, 1);

	/* U+1FB9E: '🮞' */
	bd_erasesymbol(ssbuf, 0);
	bd_drawtriangle(ssbuf, 0, cw-1, 0, 0, ch-1, cw-1, ch-1, 128);
	bd_downsample(buf, idx++, ssbuf, 0, 1);

	/* U+1FB9F: '🮟' */
	bd_erasesymbol(ssbuf, 0);
	bd_drawtriangle(ssbuf, 0, 0, 0, 0, ch-1, cw-1, ch-1, 128);
	bd_downsample(buf, idx++, ssbuf, 0, 1);

	return idx;
}

int
be_draw_left_thirds_blocks(BDBuffer *buf, int idx)
{
	/* U+1FBCE: '🯎' */
	bd_drawrect(buf, idx++, 0, 0, DIV(buf->cw * 2, 3), buf->ch, 255);

	/* U+1FBCF '🯏' */
	bd_drawrect(buf, idx++, 0, 0, DIV(buf->cw * 1, 3), buf->ch, 255);

	return idx;
}

int
be_draw_one_quarter_blocks(BDBuffer *buf, int idx)
{
	int cw = buf->cw, ch = buf->ch, w2 = DIV(cw, 2), h2 = DIV(ch, 2);
	int cx = DIV(buf->cw - w2, 2), cy = DIV(buf->ch - h2, 2);

	/* U+1FBE4: '🯤' */
	bd_drawrect(buf, idx++, cx, 0, w2, h2, 255);

	/* U+1FBE5: '🯥' */
	bd_drawrect(buf, idx++, cx, h2, w2, ch - h2, 255);

	/* U+1FBE6: '🯦' */
	bd_drawrect(buf, idx++, 0, cy, w2, h2, 255);

	/* U+1FBE7: '🯧' */
	bd_drawrect(buf, idx++, cw - w2, cy, w2, h2, 255);

	return idx;
}
