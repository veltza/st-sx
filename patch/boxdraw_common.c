/**
 * Common functions for creating box drawing characters.
 */

int
bd_initbuffer(BDBuffer *buf, int cw, int ch, int cx, int cy, int lw, int xmargin, int numchars, int factor)
{
	buf->cw = cw * factor;
	buf->ch = ch * factor;
	buf->cx = cx * factor;
	buf->cy = cy * factor;
	buf->lw = lw * factor;
	buf->xmargin = xmargin * factor;
	buf->charwidth = buf->cw + buf->xmargin * 2;
	buf->factor = factor;
	buf->numchars = numchars;
	buf->cols = MIN(DisplayWidth(xdpy, xw.scr) * factor / buf->charwidth, numchars);
	if (buf->cols <= 0)
		return 0;
	buf->rows = (numchars + buf->cols - 1) / buf->cols;
	buf->width = buf->cols * buf->charwidth;
	buf->height = buf->rows * buf->ch;
	if (!(buf->data = xmalloc(buf->width * buf->height)))
		return 0;
	memset(buf->data, 0, buf->width * buf->height);
	return 1;
}

void
bd_createmask(BDBuffer *buf)
{
	Pixmap maskpixmap;
	XImage *ximage;
	GC gc;
	int w = buf->width, h = buf->height;

	if (buf->mask)
		XRenderFreePicture(xdpy, buf->mask);

	maskpixmap = XCreatePixmap(xdpy, XftDrawDrawable(xd), w, h, 8);
	buf->mask = XRenderCreatePicture(xdpy, maskpixmap,
			XRenderFindStandardFormat(xdpy, PictStandardA8), 0, 0);

	gc = XCreateGC(xdpy, maskpixmap, 0, 0);
	ximage = XCreateImage(xdpy, xvis, 8, ZPixmap, 0, (char *)buf->data, w, h, 8, w);
	XPutImage(xdpy, maskpixmap, gc, ximage, 0, 0, 0, 0, w, h);

	XFreeGC(xdpy, gc);
	XFreePixmap(xdpy, maskpixmap);
	XDestroyImage(ximage); /* frees buf->data as well */
}

void
bd_getmaskcoords(BDBuffer *buf, int idx, int *x, int *y)
{
	*x = idx % buf->cols * buf->charwidth;
	*y = idx / buf->cols * buf->ch;
}

uchar *
bd_getsymbol(BDBuffer *buf, int idx)
{
	int col = idx % buf->cols;
	int row = idx / buf->cols;
	return buf->data + col * buf->charwidth + row * buf->ch * buf->width;
}


uchar
bd_avgintensity(uchar *data, int w, int f)
{
	uint x, y, sum;

	for (sum = 0, y = 0; y < f; y++, data += w)
		for (x = 0; x < f; x++)
			sum += (uint)data[x];

	return (sum + (f * f / 2)) / (f * f);
}

void
bd_downsample(BDBuffer *dstbuf, int dstidx, BDBuffer *srcbuf, int srcidx, int numchars)
{
	uchar *dst, *src;
	int i, x, y;
	int f = srcbuf->factor, cw = srcbuf->charwidth, ch = srcbuf->ch;
	int sw = srcbuf->width, dw = dstbuf->width - dstbuf->charwidth;

	for (i = 0; i < numchars; i++) {
		dst = bd_getsymbol(dstbuf, dstidx + i);
		src = bd_getsymbol(srcbuf, srcidx + i);
		for (y = 0; y < ch; y += f, src += sw * f, dst += dw)
			for (x = 0; x < cw; x += f)
				*dst++ = bd_avgintensity(&src[x], sw, f);
	}
}

void
bd_drawrect(BDBuffer *buf, int idx, int x, int y, int w, int h, int alpha)
{
	uchar *data = bd_getsymbol(buf, idx) + buf->xmargin;
	int x1 = MAX(0, x);
	int y1 = MAX(0, y);
	int x2 = MIN(buf->cw, x + w);
	int y2 = MIN(buf->ch, y + h);

	if (x1 >= buf->cw || y1 >= buf->ch || x2 < 0 || y2 < 0)
		return;

	data += y1 * buf->width + x1;
	for (y = y1; y < y2; y++, data += buf->width)
		memset(data, alpha, x2 - x1);
}

void
bd_drawlineup(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, lw = buf->lw;
	bd_drawrect(buf, idx, cx, 0, lw, cy + lw, 255);
}

void
bd_drawlinedown(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, ch = buf->ch, lw = buf->lw;
	bd_drawrect(buf, idx, cx, cy, lw, ch - cy, 255);
}

void
bd_drawlineleft(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, lw = buf->lw;
	bd_drawrect(buf, idx, 0, cy, cx + lw, lw, 255);
}

void
bd_drawlineright(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, cw = buf->cw, lw = buf->lw;
	bd_drawrect(buf, idx, cx, cy, cw - cx, lw, 255);
}

void
bd_drawroundedcorners(BDBuffer *buf, int br, int bl, int tl, int tr)
{
	int lw = buf->lw, f = buf->factor;
	int ox1 = buf->cx, oy1 = buf->cy;
	int ox2 = ox1 + lw - 1, oy2 = oy1 + lw - 1;
	int c, cx, cy, d, d1, d2, rw, rh, r, x, y;
	uchar *cbr = bd_getsymbol(buf, br) + buf->xmargin;
	uchar *cbl = bd_getsymbol(buf, bl) + buf->xmargin;
	uchar *ctl = bd_getsymbol(buf, tl) + buf->xmargin;
	uchar *ctr = bd_getsymbol(buf, tr) + buf->xmargin;

	bd_drawlineup(buf, tl);
	bd_drawlineup(buf, tr);
	bd_drawlinedown(buf, bl);
	bd_drawlinedown(buf, br);
	bd_drawlineright(buf, tr);
	bd_drawlineright(buf, br);
	bd_drawlineleft(buf, tl);
	bd_drawlineleft(buf, bl);

	rw = buf->cw - ox1;
	rh = buf->ch - oy1;
	r = MIN(rw, rh);

	d1 = r - lw - (f / 4);
	d1 = d1 * d1;
	d2 = r * r;

	cx = cy = r - 1;

	for (y = 0; y < r; y++) {
		for (x = 0; x < r; x++) {
			d = (cx-x)*(cx-x) + (cy-y)*(cy-y);
			c = (d1 <= d && d <= d2) ? 255 : 0;
			cbr[(oy1 + y) * buf->width + ox1 + x] = c;
			cbl[(oy1 + y) * buf->width + ox2 - x] = c;
			ctr[(oy2 - y) * buf->width + ox1 + x] = c;
			ctl[(oy2 - y) * buf->width + ox2 - x] = c;
		}
	}
}

void
bd_drawcircle(BDBuffer *buf, int idx, int fill)
{
	uchar *data = bd_getsymbol(buf, idx) + buf->xmargin;
	int f = buf->factor, lw = buf->lw / f;
	int cw = buf->cw / f, ch = buf->ch / f;
	int d, d1, d2, ox, oy, scale, rw, rh, r, x, y;

	if (lw & 1) {
		ox = (cw / 2 * f) + (f / 2);
		oy = (ch / 2 * f) + (f / 2);
		scale = (cw < 9) ? 100 : 90;
	} else {
		ox = (cw + 1) / 2 * f;
		oy = (ch + 1) / 2 * f;
		scale = (cw < 10) ? 100 : 90;
	}

	rw = buf->cw - ox;
	rh = buf->ch - oy;
	r = MIN(rw, rh) * scale / 100 - 1;

	d1 = 0;
	if (!fill) {
		d1 = r - lw * f;
		if (lw == 1 && cw > 8)
			d1 = (d1 - f / 4) * (d1 - f / 4);
		else if (lw == 1 && cw > 6)
			d1 = d1 * d1 - f * 3;
		else
			d1 = d1 * d1;
	}
	d2 = r * r;

	for (y = 0; y < buf->ch; y++, data += buf->width) {
		for (x = 0; x < buf->cw; x++) {
			d = (ox-x)*(ox-x) + (oy-y)*(oy-y);
			if (d1 <= d && d <= d2)
				data[x] = 255;
			else if (d < d1)
				data[x] = 0;
		}
	}
}

void
bd_drawhorizfadingline(BDBuffer *buf, int idx, int left)
{
	int i, x, sz, steps = 4;
	int cy = buf->cy, cw = buf->cw, lw = buf->lw;

	for (i = 0; i < steps; i++) {
		sz = cw * (steps - i) / (steps * steps + steps);
		x = i * cw / steps;
		if (left)
			x = cw - x - sz;
		bd_drawrect(buf, idx, x, cy, sz, lw, 255);
	}
}

void
bd_drawvertfadingline(BDBuffer *buf, int idx, int up)
{
	int i, y, sz, steps = 5;
	int cx = buf->cx, ch = buf->ch, lw = buf->lw;

	for (i = 0; i < steps; i++) {
		sz = ch * (steps - i) / (steps * steps + steps);
		y = i * ch / steps;
		if (up)
			y = ch - y - sz;
		bd_drawrect(buf, idx, cx, y, lw, sz, 255);
	}
}

void
bd_drawhdashes(BDBuffer *buf, int idx, int n, int heavy)
{
	int i, w, x1, x2, f = buf->factor, cw = buf->cw / f, s = 1;
	int y1 = buf->cy - (heavy ? buf->lw : 0);
	int y2 = buf->cy + (heavy ? buf->lw : 0) + buf->lw;

	if (cw < 4) {
		bd_drawrect(buf, idx, 0, y1, buf->cw, y2 - y1, 255);
		return;
	}

	if (cw < 7 || (cw < 12 && n >= 3) || (cw <= 16 && n == 4)) {
		n = (cw < 6) ? 2 : n;
		n = (cw < 8 && n == 4) ? 3 : n;
		w = cw / n;
		for (i = 0; i < n; i++) {
			x1 = i * cw / n;
			x2 = (i + 1) * cw / n;
			bd_drawrect(buf, idx, x1 * f, y1, (x2 - x1 - s) * f, y2 - y1, 255);
		}
		return;
	}

	for (i = 0; i < n; i++) {
		x1 = i * buf->cw / n;
		x2 = (i + 1) * buf->cw / n;
		w = x2 - x1;
		s = w * 30 / 100;
		bd_drawrect(buf, idx, x1, y1, w - s, y2 - y1, 255);
	}
}

void
bd_drawvdashes(BDBuffer *buf, int idx, int n, int heavy)
{
	int i, s, h, y1, y2, ch = buf->ch;
	int x1 = buf->cx - (heavy ? buf->lw : 0);
	int x2 = buf->cx + (heavy ? buf->lw : 0) + buf->lw;

	for (i = 0; i < n; i++) {
		y1 = i * ch / n;
		y2 = (i + 1) * ch / n;
		h = y2 - y1;
		s = h * 40 / 100;
		bd_drawrect(buf, idx, x1, y1 + s/2, x2 - x1, h - s, 255);
	}
}

void
bd_drawdiagonals(BDBuffer *buf, int lr, int rl, int cross)
{
	uchar *datalr = bd_getsymbol(buf, lr);
	uchar *datarl = bd_getsymbol(buf, rl);
	int w = MAX(1, buf->cw), h = MAX(1, buf->ch);
	int j, x, y, lw = buf->lw * ((buf->lw / buf->factor > 1) ? 10 : 11) / 4;
	int cw = buf->charwidth, ch = buf->ch;

	for (x = 0; x < cw; x++) {
		y = (x - buf->xmargin) * h / w;
		for (j = y - lw/2; j < y + lw - lw/2; j++) {
			if (j >= 0 && j < ch) {
				datalr[j * buf->width + x] = 255;
				datarl[j * buf->width + cw - x - 1] = 255;
			}
		}
	}
	bd_copysymbol(buf, cross, lr, 0);
	bd_copysymbol(buf, cross, rl, 0);
}

void
bd_drawblockpatterns(BDBuffer *buf, int idx, uchar *blockpatterns, int len, int rows)
{
	int i, row, x1, x2, y1, y2, cx = DIV(buf->cw, 2);
	uchar pattern;

	for (i = 0; i < len; i++) {
		pattern = blockpatterns[i];
		for (row = 0; row < rows; row++, pattern >>= 2) {
			if (pattern & 3) {
				x1 = (pattern & 1) ? 0 : cx;
				x2 = (pattern & 2) ? buf->cw : cx;
				y1 = DIV(buf->ch * row, rows);
				y2 = DIV(buf->ch * (row + 1), rows);
				bd_drawrect(buf, idx + i, x1, y1, x2 - x1, y2 - y1, 255);
			}
		}
	}
}

void
bd_drawtriangle(BDBuffer *buf, int idx, int ax, int ay, int bx, int by, int cx, int cy, int alpha)
{
	uchar *data = bd_getsymbol(buf, idx) + buf->xmargin;
	int xl, xr, y;
	double x1 = ax, y1 = ay;
	double x2 = bx, y2 = by;
	double x3 = cx, y3 = cy;
	double sx1, sx2, dx1, dx2, t;

	#define SWAP(a, b) { t = a; a = b; b = t; }
	#define RENDERLINE(a, b) { \
			xl = MIN(a, b); \
			xr = MAX(a, b); \
			memset(data + xl, alpha, xr - xl + 1); \
		}

	if (y1 > y2) {
		SWAP(y1, y2); SWAP(x1, x2);
	}
	if (y2 > y3) {
		SWAP(y2, y3); SWAP(x2, x3);
		if (y1 > y2) {
			SWAP(y1, y2); SWAP(x1, x2);
		}
	}
	data += (int)y1 * buf->width;

	if (y1 == y2 && y2 == y3) {
		RENDERLINE(MIN(MIN(x1, x2), x3), MAX(MAX(x1, x2), x3));
		return;
	}

	/* top half of triangle */
	sx1 = x1;
	dx1 = (x3 - x1) / (y3 - y1);
	if (y1 < y2) {
		sx2 = x1;
		dx2 = (x2 - x1) / (y2 - y1);
		for (y = y1; y < (int)y2; y++, sx1 += dx1, sx2 += dx2, data += buf->width) {
			RENDERLINE(sx1, sx2);
		}
		if (y2 == y3) {
			RENDERLINE(x2, x3);
			return;
		}
	}

	/* bottom half of triangle */
	sx2 = x2;
	dx2 = (x3 - x2) / (y3 - y2);
	for (y = y2; y < (int)y3; y++, sx1 += dx1, sx2 += dx2, data += buf->width) {
		RENDERLINE(sx1, sx2);
	}
	data[(int)x3] = alpha;

	#undef SWAP
	#undef RENDERLINE
}

void
bd_copysymbol(BDBuffer *buf, int dstidx, int srcidx, int fliphoriz)
{
	uchar *dst = bd_getsymbol(buf, dstidx) + (fliphoriz ? buf->charwidth-1 : 0);
	uchar *src = bd_getsymbol(buf, srcidx);
	int x, y, cw = buf->charwidth, ch = buf->ch;
	int srcw = buf->width - cw;
	int dstw = fliphoriz ? buf->width + cw : srcw;
	int dstinc = fliphoriz ? -1 : 1;

	for (y = 0; y < ch; y++, dst += dstw, src += srcw)
		for (x = 0; x < cw; x++, dst += dstinc, src++)
			*dst |= *src;
}

void
bd_erasesymbol(BDBuffer *buf, int idx)
{
	int y;
	uchar *data = bd_getsymbol(buf, idx);

	for (y = 0; y < buf->ch; y++, data += buf->width)
		memset(data, 0, buf->charwidth);
}

void
bd_errormsg(char *msg)
{
	static int errorsent;

	if (!errorsent)
		fprintf(stderr, "%s\n", msg);
	errorsent = 1;
}
