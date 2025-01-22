/**
 * An extension to the boxdraw patch that draws branch drawing symbols.
 *
 * Specification of the symbols:
 *   https://github.com/kovidgoyal/kitty/pull/7681
 *   https://github.com/rbong/flog-symbols
 *
 * Some Vim/Neovim plugins that already use the symbols:
 *   https://github.com/rbong/vim-flog
 *   https://github.com/isakbm/gitgraph.nvim
 *   https://github.com/NeogitOrg/neogit
 *
 * Notes
 *   - Branch drawing symbols is an independent symbol set that does not have
 *     to be compatible with box drawing characters. So you can set a different
 *     line thickness with boxdraw_branch_thickness if you like.
 *   - The XRender library is required for this extension.
 *
 * Bug reports:
 *   https://github.com/veltza/st-sx
 */

static int bs_generatesymbols(void);

BDBuffer bsyms;

/* public API */

void
drawbranchsymbol(int x, int y, int w, int h, XftColor *fg, ushort symbol)
{
	Picture src;

	if (!bs_generatesymbols())
		return;

	/* Horizontal and vertical lines don't need anti-aliasing, so we can
	 * draw them much faster with XftDrawRect() */
	if (symbol == BSLH_IDX) {
		XftDrawRect(xd, fg, x, y + bsyms.cy, w, bsyms.lw);
		return;
	} else if (symbol == BSLV_IDX) {
		XftDrawRect(xd, fg, x + bsyms.cx, y, bsyms.lw, h);
		return;
	}

	/* Draw the rest of the symbols with XRender and anti-aliasing */
	if ((src = XftDrawSrcPicture(xd, fg)))
		XRenderComposite(xdpy, PictOpOver,
			src, bsyms.mask, XftDrawPicture(xd),
			0, 0, 0, bsyms.ch * symbol, x, y, bsyms.cw, bsyms.ch);
}

/* implementation */

int
bs_generatesymbols(void)
{
	static int errorsent;
	int i, s, cx, cy, lw;
	BDBuffer ssbuf;

	lw = (boxdraw_branch_thickness > 0)
		? boxdraw_branch_thickness
		: MAX(1, DIV(MIN(win.cw, win.ch), 8));

	if (bsyms.cw == win.cw && bsyms.ch == win.ch && bsyms.lw == lw)
		return 1;

	if (!XftDefaultHasRender(xdpy)) {
		if (!errorsent)
			fprintf(stderr, "boxdraw_branch: XRender is not available\n");
		errorsent = 1;
		return 0;
	}

	cx = DIV(win.cw - lw, 2);
	cy = DIV(win.ch - lw, 2);
	bd_initbuffer(&bsyms, win.cw, win.ch, cx, cy, lw, 0, LEN(branchsymbols), 1);
	bd_initbuffer(&ssbuf, win.cw, win.ch, cx, cy, lw, 0, LEN(branchsymbols), SS_FACTOR);

	bd_drawcircle(&ssbuf, BSCM_INDX, 1);
	bd_drawcorners(&ssbuf, BSABR_INDX, BSABL_INDX, BSATL_INDX, BSATR_INDX);

	for (i = 0; i < LEN(branchsymbols); i++) {
		s = branchsymbols[i];
		if (s & BSLR)
			bd_drawlineright(&ssbuf, i);
		if (s & BSLL)
			bd_drawlineleft(&ssbuf, i);
		if (s & BSLD)
			bd_drawlinedown(&ssbuf, i);
		if (s & BSLU)
			bd_drawlineup(&ssbuf, i);
		if (s & (BSFR | BSFL))
			bd_drawhorizfadingline(&ssbuf, i, s & BSFL);
		if (s & (BSFD | BSFU))
			bd_drawvertfadingline(&ssbuf, i, s & BSFU);
		if (s & BSABR)
			bd_copysymbol(&ssbuf, i, BSABR_INDX);
		if (s & BSABL)
			bd_copysymbol(&ssbuf, i, BSABL_INDX);
		if (s & BSATR)
			bd_copysymbol(&ssbuf, i, BSATR_INDX);
		if (s & BSATL)
			bd_copysymbol(&ssbuf, i, BSATL_INDX);
		if (s & BSCM)
			bd_copysymbol(&ssbuf, i,  BSCM_INDX);
		if (s & BSCN)
			bd_drawcircle(&ssbuf, i, 0);
	}

	bd_downsample(&bsyms, 0, &ssbuf, 0, bsyms.numchars);
	bd_createmask(&bsyms);
	free(ssbuf.data);
	return 1;
}
