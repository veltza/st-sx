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
static void bs_createmask(void);
static char bs_avgcolor(char *t);
static void bs_downsample(char *, char *);
static void bs_drawrect(char *, int, int, int, int);
static void bs_drawlineup(char *);
static void bs_drawlinedown(char *);
static void bs_drawlineleft(char *);
static void bs_drawlineright(char *);
static void bs_drawhorizfadingline(char *, int);
static void bs_drawvertfadingline(char *, int);
static void bs_drawcircle(char *, int);
static void bs_drawcurves(char *);
static void bs_copysymbol(char *, char *);

struct {
	int cw, ch;
	int cx, cy;
	int lw;
	int factor;
	int charoffset;
	char *data;
} ssbuf = { .factor = 5 };

struct {
	int cw, ch;
	int cx, cy;
	int mw, mh;
	int lw;
	Picture mask;
} bsyms;

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
	int i, s, lw;
	char *buf;

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

	bsyms.cw = win.cw;
	bsyms.ch = win.ch;
	bsyms.lw = lw;
	bsyms.cx = DIV(bsyms.cw - bsyms.lw, 2);
	bsyms.cy = DIV(bsyms.ch - bsyms.lw, 2);

	ssbuf.cw = bsyms.cw * ssbuf.factor;
	ssbuf.ch = bsyms.ch * ssbuf.factor;
	ssbuf.cx = bsyms.cx * ssbuf.factor;
	ssbuf.cy = bsyms.cy * ssbuf.factor;
	ssbuf.lw = bsyms.lw * ssbuf.factor;
	ssbuf.charoffset = ssbuf.cw * ssbuf.ch;
	ssbuf.data = xmalloc(ssbuf.charoffset * LEN(branchsymbols));
	memset(ssbuf.data, 0, ssbuf.charoffset * LEN(branchsymbols));

	bs_drawcircle(ssbuf.data + BSCM_INDX * ssbuf.charoffset, 1);
	bs_drawcurves(ssbuf.data);

	for (buf = ssbuf.data, i = 0; i < LEN(branchsymbols); i++) {
		s = branchsymbols[i];
		if (s & BSLR)
			bs_drawlineright(buf);
		if (s & BSLL)
			bs_drawlineleft(buf);
		if (s & BSLD)
			bs_drawlinedown(buf);
		if (s & BSLU)
			bs_drawlineup(buf);
		if (s & (BSFR | BSFL))
			bs_drawhorizfadingline(buf, s & BSFL);
		if (s & (BSFD | BSFU))
			bs_drawvertfadingline(buf, s & BSFU);
		if (s & BSABR)
			bs_copysymbol(buf, ssbuf.data + BSABR_INDX * ssbuf.charoffset);
		if (s & BSABL)
			bs_copysymbol(buf, ssbuf.data + BSABL_INDX * ssbuf.charoffset);
		if (s & BSATR)
			bs_copysymbol(buf, ssbuf.data + BSATR_INDX * ssbuf.charoffset);
		if (s & BSATL)
			bs_copysymbol(buf, ssbuf.data + BSATL_INDX * ssbuf.charoffset);
		if (s & BSCM)
			bs_copysymbol(buf, ssbuf.data + BSCM_INDX * ssbuf.charoffset);
		if (s & BSCN)
			bs_drawcircle(buf, 0);
		buf += ssbuf.charoffset;
	}

	bs_createmask();
	free(ssbuf.data);
	return 1;
}

void
bs_createmask(void)
{
	char *maskbuf;
	Pixmap maskpixmap;
	XImage *ximage;
	GC gc;

	bsyms.mw = bsyms.cw;
	bsyms.mh = bsyms.ch * LEN(branchsymbols);
	maskbuf = xmalloc(bsyms.mw * bsyms.mh);

	bs_downsample(ssbuf.data, maskbuf);

	if (bsyms.mask)
		XRenderFreePicture(xdpy, bsyms.mask);

	maskpixmap = XCreatePixmap(xdpy, XftDrawDrawable(xd), bsyms.mw, bsyms.mh, 8);
	bsyms.mask = XRenderCreatePicture(xdpy, maskpixmap,
			XRenderFindStandardFormat(xdpy, PictStandardA8), 0, 0);

	gc = XCreateGC(xdpy, maskpixmap, 0, 0);
	ximage = XCreateImage(xdpy, xvis, 8, ZPixmap, 0, maskbuf,
				bsyms.mw, bsyms.mh, 8, bsyms.mw);
	XPutImage(xdpy, maskpixmap, gc, ximage, 0, 0, 0, 0, bsyms.mw, bsyms.mh);

	XFreeGC(xdpy, gc);
	XFreePixmap(xdpy, maskpixmap);
	XDestroyImage(ximage); /* frees maskbuf as well */
}

char
bs_avgintensity(char *buf)
{
	int x, y, sum, f = ssbuf.factor;

	for (sum = 0, y = 0; y < f; y++, buf += ssbuf.cw)
		for (x = 0; x < f; x++)
			sum += buf[x];

	return (255 * sum + (f * f / 2)) / (f * f);
}

void
bs_downsample(char *buf, char *mask)
{
	int x, y, f = ssbuf.factor, h = ssbuf.ch * LEN(branchsymbols);
	int yoffset = ssbuf.cw * ssbuf.factor;

	for (y = 0; y < h; y += f, buf += yoffset)
		for (x = 0; x < ssbuf.cw; x += f)
			*mask++ = bs_avgintensity(&buf[x]);
}

void
bs_drawrect(char *buf, int x, int y, int w, int h)
{
	int x1 = MAX(0, x);
	int y1 = MAX(0, y);
	int x2 = MIN(ssbuf.cw, x + w);
	int y2 = MIN(ssbuf.ch, y + h);

	if (x1 >= ssbuf.cw || y1 >= ssbuf.ch || x2 < 0 || y2 < 0)
		return;

	buf += y1 * ssbuf.cw + x1;
	for (y = y1; y < y2; y++, buf += ssbuf.cw)
		memset(buf, 1, x2 - x1);
}

void
bs_drawlineup(char *buf)
{
	int cx = ssbuf.cx, cy = ssbuf.cy, lw = ssbuf.lw;
	bs_drawrect(buf, cx, 0, lw, cy + lw);
}

void
bs_drawlinedown(char *buf)
{
	int cx = ssbuf.cx, cy = ssbuf.cy, ch = ssbuf.ch, lw = ssbuf.lw;
	bs_drawrect(buf, cx, cy, lw, ch - cy);
}

void
bs_drawlineleft(char *buf)
{
	int cx = ssbuf.cx, cy = ssbuf.cy, lw = ssbuf.lw;
	bs_drawrect(buf, 0, cy, cx + lw, lw);
}

void
bs_drawlineright(char *buf)
{
	int cx = ssbuf.cx, cy = ssbuf.cy, cw = ssbuf.cw, lw = ssbuf.lw;
	bs_drawrect(buf, cx, cy, cw - cx, lw);
}

void
bs_drawhorizfadingline(char *buf, int left)
{
	int i, x, sz, steps = 4;
	int cy = ssbuf.cy, cw = ssbuf.cw, lw = ssbuf.lw;

	for (i = 0; i < steps; i++) {
		sz = cw * (steps - i) / (steps * steps + steps);
		x = i * cw / steps;
		if (left)
			x = cw - x - sz;
		bs_drawrect(buf, x, cy, sz, lw);
	}
}

void
bs_drawvertfadingline(char *buf, int up)
{
	int i, y, sz, steps = 5;
	int cx = ssbuf.cx, ch = ssbuf.ch, lw = ssbuf.lw;

	for (i = 0; i < steps; i++) {
		sz = ch * (steps - i) / (steps * steps + steps);
		y = i * ch / steps;
		if (up)
			y = ch - y - sz;
		bs_drawrect(buf, cx, y, lw, sz);
	}
}

void
bs_drawcircle(char *buf, int fill)
{
	int lw = bsyms.lw, f = ssbuf.factor;
	int d, d1, d2, ox, oy, scale, rw, rh, r, x, y;

	if (lw & 1) {
		ox = (bsyms.cw / 2 * f) + (f / 2);
		oy = (bsyms.ch / 2 * f) + (f / 2);
		scale = (bsyms.cw < 9) ? 100 : 90;
	} else {
		ox = (bsyms.cw + 1) / 2 * f;
		oy = (bsyms.ch + 1) / 2 * f;
		scale = (bsyms.cw < 10) ? 100 : 90;
	}

	rw = ssbuf.cw - ox;
	rh = ssbuf.ch - oy;
	r = MIN(rw, rh) * scale / 100 - 1;

	d1 = fill ? 0 : (r - lw * f) * (r - lw * f);
	d2 = r * r;

	for (y = 0; y < ssbuf.ch; y++, buf += ssbuf.cw) {
		for (x = 0; x < ssbuf.cw; x++) {
			d = (ox-x)*(ox-x) + (oy-y)*(oy-y);
			if (d1 <= d && d <= d2)
				buf[x] = 1;
			else if (d < d1)
				buf[x] = 0;
		}
	}
}

void
bs_drawcurves(char *buf)
{
	int lw = ssbuf.lw, f = ssbuf.factor;
	int ox1 = ssbuf.cx, oy1 = ssbuf.cy;
	int ox2 = ox1 + lw - 1, oy2 = oy1 + lw - 1;
	int c, cx, cy, d, d1, d2, rw, rh, r, x, y;
	char *abr = buf + BSABR_INDX * ssbuf.charoffset;
	char *abl = buf + BSABL_INDX * ssbuf.charoffset;
	char *atr = buf + BSATR_INDX * ssbuf.charoffset;
	char *atl = buf + BSATL_INDX * ssbuf.charoffset;

	bs_drawlineup(atr);
	bs_drawlineup(atl);
	bs_drawlinedown(abr);
	bs_drawlinedown(abl);
	bs_drawlineright(abr);
	bs_drawlineright(atr);
	bs_drawlineleft(abl);
	bs_drawlineleft(atl);

	rw = ssbuf.cw - ox1;
	rh = ssbuf.ch - oy1;
	r = MIN(rw, rh);

	d1 = (r - lw - (f / 2));
	d1 = d1 * d1;
	d2 = r * r;

	cx = cy = r - 1;

	for (y = 0; y < r; y++) {
		for (x = 0; x < r; x++) {
			d = (cx-x)*(cx-x) + (cy-y)*(cy-y);
			c = (d1 <= d && d <= d2);
			abr[(oy1 + y) * ssbuf.cw + ox1 + x] = c;
			abl[(oy1 + y) * ssbuf.cw + ox2 - x] = c;
			atr[(oy2 - y) * ssbuf.cw + ox1 + x] = c;
			atl[(oy2 - y) * ssbuf.cw + ox2 - x] = c;
		}
	}
}

void
bs_copysymbol(char *dest, char *src)
{
	int i, size = ssbuf.cw * ssbuf.ch;

	for (i = 0; i < size; i++)
		dest[i] |= src[i];
}
