/**
 * boxdraw_extra draws dashes, diagonals, sextants, octants and proper rounded
 * corners that the original boxdraw patch doesn't implement or draw properly.
 */
static int be_generatesymbols(BDBuffer *buf, int bold);
static void be_drawwedge(BDBuffer *ssbuf, BDBuffer *buf, int idx, int x1, int y1, int x2, int y2, int x3, int y3, int invert);
static void be_drawwedgerect(BDBuffer *ssbuf, BDBuffer *buf, int idx, int x1, int y1, int x2, int y2, int x3, int y3, int recty, int invert);
static void be_drawwedges(BDBuffer *ssbuf, BDBuffer *buf);

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

	/* FIXME: when the characters U+1FB70 .. U+1FB9F are implemented */
	for (i = 0; i < LEN(boxlegacy); i++)
		boxlegacy[i] = (i < BE_LEGACY_LEN-2) ? i + 1 : 0;
	boxlegacy[0x9A] = BE_LEGACY_LEN - 1; /* U+1FB9A */
	boxlegacy[0X9B] = BE_LEGACY_LEN;     /* U+1FB9B */
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

	/* sextants and octants */
	bd_drawblockpatterns(buf, BE_SEXTANTS_IDX, boxdatasextants, BE_SEXTANTS_LEN, 3);
	bd_drawblockpatterns(buf, BE_OCTANTS_IDX, boxdataoctants, BE_OCTANTS_LEN, 4);

	/* wedges */
	be_drawwedges(&ssbuf, buf);

	bd_createmask(buf);
	free(ssbuf.data);
	return 1;
}

void
be_drawwedge(BDBuffer *ssbuf, BDBuffer *buf, int idx, int x1, int y1, int x2, int y2, int x3, int y3, int invert)
{
	bd_drawrect(ssbuf, 0, 0, 0, ssbuf->cw, ssbuf->ch, invert ? 255 : 0);
	bd_drawtriangle(ssbuf, 0, x1, y1, x2, y2, x3, y3, invert ? 0 : 255);
	bd_downsample(buf, idx, ssbuf, 0, 1);
}

void
be_drawwedgerect(BDBuffer *ssbuf, BDBuffer *buf, int idx, int x1, int y1, int x2, int y2, int x3, int y3, int recty, int invert)
{
	int ry1 = buf->ch * recty / 3, ry2 = buf->ch * (recty + 1) / 3;

	be_drawwedge(ssbuf, buf, idx, x1, y1, x2, y2, x3, y3, invert);
	bd_drawrect(buf, idx, 0, ry1, buf->cw, ry2 - ry1, invert ? 0 : 255);
}

void
be_drawwedges(BDBuffer *ssbuf, BDBuffer *buf)
{
	int cw = ssbuf->cw, ch = ssbuf->ch, cy = DIV(ch, 2);
	int x0 = 0, x1 = DIV(cw, 2);
	int y0 = 0, y1 = ch / 3, y2 = ch * 2 / 3;
	int x0e = x1 - 1, x1e = cw - 1;
	int y0e = y1 - 1, y1e = y2 - 1, y2e = ch - 1;
	int i, j, invert = 0;

	bd_erasesymbol(ssbuf, 0);
	for (i = BE_WEDGES_IDX, invert = 0, j = 0; j < 2; j++, invert ^= 1) {
		be_drawwedge(ssbuf, buf, i++, x0, y2, x0, y2e, x0e, y2e, invert);  /* U+1FB3C, U+1FB52 */
		be_drawwedge(ssbuf, buf, i++, x0, y2, x0, y2e, x1e, y2e, invert);  /* U+1FB3D, U+1FB53 */
		be_drawwedge(ssbuf, buf, i++, x0, y1, x0, y2e, x0e, y2e, invert);  /* U+1FB3E, U+1FB54 */
		be_drawwedge(ssbuf, buf, i++, x0, y1, x0, y2e, x1e, y2e, invert);  /* U+1FB3F, U+1FB55 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y2e, x0e, y2e, invert);  /* U+1FB40, U+1FB56 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y0e, x0e, y0, invert ^ 1);  /* U+1FB41, U+1FB57 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y0e, x1e, y0, invert ^ 1);  /* U+1FB42, U+1FB58 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y1e, x0e, y0, invert ^ 1);  /* U+1FB43, U+1FB59 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y1e, x1e, y0, invert ^ 1);  /* U+1FB44, U+1FB5A */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y2e, x0e, y0, invert ^ 1);  /* U+1FB45, U+1FB5B */
		if (j == 0)
			be_drawwedgerect(ssbuf, buf, i++, x1e, y1, x0, y1e, x1e, y1e, 2, 0);  /* U+1FB46 */
		else
			be_drawwedgerect(ssbuf, buf, i++, x0, y1, x0, y1e, x1e, y1, 0, 0);  /* U+1FB5C */
		be_drawwedge(ssbuf, buf, i++, x1e, y2, x1, y2e, x1e, y2e, invert);  /* U+1FB47, U+1FB5D */
		be_drawwedge(ssbuf, buf, i++, x1e, y2, x0, y2e, x1e, y2e, invert);  /* U+1FB48, U+1FB5E */
		be_drawwedge(ssbuf, buf, i++, x1e, y1, x1, y2e, x1e, y2e, invert);  /* U+1FB49, U+1FB5F */
		be_drawwedge(ssbuf, buf, i++, x1e, y1, x0, y2e, x1e, y2e, invert);  /* U+1FB4A, U+1FB60 */
		be_drawwedge(ssbuf, buf, i++, x1e, y0, x1, y2e, x1e, y2e, invert);  /* U+1FB4B, U+1FB61 */
		be_drawwedge(ssbuf, buf, i++, x1, y0, x1e, y0, x1e, y0e, invert ^ 1);  /* U+1FB4C, U+1FB62 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x1e, y0, x1e, y0e, invert ^ 1);  /* U+1FB4D, U+1FB63 */
		be_drawwedge(ssbuf, buf, i++, x1, y0, x1e, y0, x1e, y1e, invert ^ 1);  /* U+1FB4E, U+1FB64 */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x1e, y0, x1e, y1e, invert ^ 1);  /* U+1FB4F, U+1FB65 */
		be_drawwedge(ssbuf, buf, i++, x1, y0, x1e, y2e, x1e, y0, invert ^ 1);  /* U+1FB50, U+1FB66 */
		if (j == 0)
			be_drawwedgerect(ssbuf, buf, i++, x0, y1, x0, y1e, x1e, y1e, 2, 0);  /* U+1FB51 */
		else
			be_drawwedgerect(ssbuf, buf, i++, x0, y1, x1e, y1e, x1e, y1, 0, 0);  /* U+1FB67 */
	}
	for (invert = 1, j = 0; j < 2; j++, invert ^= 1) {
		be_drawwedge(ssbuf, buf, i++, x0, y0, x0, y2e, x1, cy, invert);  /* U+1FB68, U+1FB6C */
		be_drawwedge(ssbuf, buf, i++, x0, y0, x1, cy, x1e, y0, invert);  /* U+1FB69, U+1FB6D */
		be_drawwedge(ssbuf, buf, i++, x1e, y0, x1, cy, x1e, y2e, invert);  /* U+1FB6A, U+1FB6E */
		be_drawwedge(ssbuf, buf, i++, x0, y2e, x1, cy, x1e, y2e, invert);  /* U+1FB6B, U+1FB6F */
	}

	/* U+1FB9A */
	bd_drawrect(ssbuf, 0, 0, 0, ssbuf->cw, ssbuf->ch, 0);
	bd_drawtriangle(ssbuf, 0, x0, y0, x1, cy, x1e, y0, 255);
	bd_drawtriangle(ssbuf, 0, x0, y2e, x1, cy, x1e, y2e, 255);
	bd_downsample(buf, i++, ssbuf, 0, 1);

	/* U+1FB9B */
	bd_drawrect(ssbuf, 0, 0, 0, ssbuf->cw, ssbuf->ch, 0);
	bd_drawtriangle(ssbuf, 0, x0, y0, x0, y2e, x1, cy, 255);
	bd_drawtriangle(ssbuf, 0, x1e, y0, x1, cy, x1e, y2e, 255);
	bd_downsample(buf, i++, ssbuf, 0, 1);
}
