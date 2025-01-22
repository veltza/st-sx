/**
 * boxdraw_extra draws dashes, diagonals and proper rounded corners that
 * the original boxdraw patch doesn't implement or draw properly.
 */
static int be_generatesymbols(BDBuffer *buf, int bold);

struct {
	BDBuffer norm;
	BDBuffer bold;
} bdextra;

/* public API */

void
drawextrasymbol(int x, int y, int w, int h, XftColor *fg, ushort symbol, int bold)
{
	Picture src;
	BDBuffer *buf = bold ? &bdextra.bold : &bdextra.norm;

	if (!be_generatesymbols(buf, bold))
		return;

	if ((src = XftDrawSrcPicture(xd, fg)))
		XRenderComposite(xdpy, PictOpOver,
			src, buf->mask, XftDrawPicture(xd),
			0, 0, 0, buf->ch * symbol, x - buf->xmargin, y, buf->width, buf->ch);
}

/* implementation */

int
be_generatesymbols(BDBuffer *buf, int bold)
{
	static int errorsent;
	const unsigned char *bde = boxdataextra;
	BDBuffer ssbuf;
	int i, cx, cy;
	int mwh = MIN(win.cw, win.ch);
	int base_lw = MAX(1, DIV(mwh, 8));
	int lw = (bold && mwh >= 6) ? MAX(base_lw + 1, DIV(3 * base_lw, 2)) : base_lw;

	if (buf->cw == win.cw && buf->ch == win.ch && buf->lw == lw)
		return 1;

	if (!XftDefaultHasRender(xdpy)) {
		if (!errorsent)
			fprintf(stderr, "boxdraw_extra: XRender is not available\n");
		errorsent = 1;
		return 0;
	}

	cx = DIV(win.cw - lw, 2);
	cy = DIV(win.ch - lw, 2);
	bd_initbuffer(buf, win.cw, win.ch, cx, cy, lw, lw, BE_NUM_CHARS, 1);
	bd_initbuffer(&ssbuf, win.cw, win.ch, cx, cy, lw, lw, BE_NUM_CHARS, SS_FACTOR);

	bd_drawhdashes(&ssbuf, bde[BE_HDASH2], 2, 0);
	bd_drawhdashes(&ssbuf, bde[BE_HDASH3], 3, 0);
	bd_drawhdashes(&ssbuf, bde[BE_HDASH4], 4, 0);
	bd_drawhdashes(&ssbuf, bde[BE_HDASH2_HEAVY], 2, 1);
	bd_drawhdashes(&ssbuf, bde[BE_HDASH3_HEAVY], 3, 1);
	bd_drawhdashes(&ssbuf, bde[BE_HDASH4_HEAVY], 4, 1);
	bd_drawvdashes(&ssbuf, bde[BE_VDASH2], 2, 0);
	bd_drawvdashes(&ssbuf, bde[BE_VDASH3], 3, 0);
	bd_drawvdashes(&ssbuf, bde[BE_VDASH4], 4, 0);
	bd_drawvdashes(&ssbuf, bde[BE_VDASH2_HEAVY], 2, 1);
	bd_drawvdashes(&ssbuf, bde[BE_VDASH3_HEAVY], 3, 1);
	bd_drawvdashes(&ssbuf, bde[BE_VDASH4_HEAVY], 4, 1);
	bd_drawcorners(&ssbuf, bde[BE_ARC_DR], bde[BE_ARC_DL], bde[BE_ARC_UL], bde[BE_ARC_UR]);
	bd_drawdiagonals(&ssbuf, bde[BE_DIAG_LR], bde[BE_DIAG_RL], bde[BE_DIAG_CROSS]);

	bd_downsample(buf, 0, &ssbuf, 0, buf->numchars);
	bd_createmask(buf);
	free(ssbuf.data);
	return 1;
}
