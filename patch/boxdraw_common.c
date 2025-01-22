/**
 * Common functions for creating box drawing characters.
 */

void
bd_initbuffer(BDBuffer *buf, int cw, int ch, int cx, int cy, int lw, int xmargin, int numchars, int factor)
{
	buf->cw = cw * factor;
	buf->ch = ch * factor;
	buf->cx = cx * factor;
	buf->cy = cy * factor;
	buf->lw = lw * factor;
	buf->xmargin = xmargin * factor;
	buf->width = buf->cw + buf->xmargin * 2;
	buf->factor = factor;
	buf->numchars = numchars;
	buf->charoffset = buf->width * buf->ch;
	buf->data = xmalloc(buf->charoffset * numchars);
	memset(buf->data, 0, buf->charoffset * numchars);
}

void
bd_createmask(BDBuffer *buf)
{
	Pixmap maskpixmap;
	XImage *ximage;
	GC gc;
	int w = buf->width, h = buf->ch * buf->numchars;

	if (buf->mask)
		XRenderFreePicture(xdpy, buf->mask);

	maskpixmap = XCreatePixmap(xdpy, XftDrawDrawable(xd), w, h, 8);
	buf->mask = XRenderCreatePicture(xdpy, maskpixmap,
			XRenderFindStandardFormat(xdpy, PictStandardA8), 0, 0);

	gc = XCreateGC(xdpy, maskpixmap, 0, 0);
	ximage = XCreateImage(xdpy, xvis, 8, ZPixmap, 0, buf->data, w, h, 8, w);
	XPutImage(xdpy, maskpixmap, gc, ximage, 0, 0, 0, 0, w, h);

	XFreeGC(xdpy, gc);
	XFreePixmap(xdpy, maskpixmap);
	XDestroyImage(ximage); /* frees buf->data as well */
}

char
bd_avgintensity(char *data, int w, int f)
{
	int x, y, sum;

	for (sum = 0, y = 0; y < f; y++, data += w)
		for (x = 0; x < f; x++)
			sum += data[x];

	return (255 * sum + (f * f / 2)) / (f * f);
}

void
bd_downsample(BDBuffer *dstbuf, int dstidx, BDBuffer *srcbuf, int srcidx, int numchars)
{
	char *dst = dstbuf->data + dstidx * dstbuf->charoffset;
	char *src = srcbuf->data + srcidx * srcbuf->charoffset;
	int x, y, f = srcbuf->factor, w = srcbuf->width, h = srcbuf->ch * numchars;

	for (y = 0; y < h; y += f, src += w * f)
		for (x = 0; x < w; x += f)
			*dst++ = bd_avgintensity(&src[x], w, f);
}

void
bd_drawrect(BDBuffer *buf, int idx, int x, int y, int w, int h)
{
	char *data = buf->data + idx * buf->charoffset;
	int x1 = MAX(0, x + buf->xmargin);
	int y1 = MAX(0, y);
	int x2 = MIN(buf->width, x + w + buf->xmargin);
	int y2 = MIN(buf->ch, y + h);

	if (x1 >= buf->width || y1 >= buf->ch || x2 < 0 || y2 < 0)
		return;

	data += y1 * buf->width + x1;
	for (y = y1; y < y2; y++, data += buf->width)
		memset(data, 1, x2 - x1);
}

void
bd_drawlineup(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, lw = buf->lw;
	bd_drawrect(buf, idx, cx, 0, lw, cy + lw);
}

void
bd_drawlinedown(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, ch = buf->ch, lw = buf->lw;
	bd_drawrect(buf, idx, cx, cy, lw, ch - cy);
}

void
bd_drawlineleft(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, lw = buf->lw;
	bd_drawrect(buf, idx, 0, cy, cx + lw, lw);
}

void
bd_drawlineright(BDBuffer *buf, int idx)
{
	int cx = buf->cx, cy = buf->cy, cw = buf->cw, lw = buf->lw;
	bd_drawrect(buf, idx, cx, cy, cw - cx, lw);
}

void
bd_drawcorners(BDBuffer *buf, int br, int bl, int tl, int tr)
{
	int lw = buf->lw, f = buf->factor;
	int ox1 = buf->cx, oy1 = buf->cy;
	int ox2 = ox1 + lw - 1, oy2 = oy1 + lw - 1;
	int c, cx, cy, d, d1, d2, rw, rh, r, x, y;
	char *abr = buf->data + br * buf->charoffset + buf->xmargin;
	char *abl = buf->data + bl * buf->charoffset + buf->xmargin;
	char *atl = buf->data + tl * buf->charoffset + buf->xmargin;
	char *atr = buf->data + tr * buf->charoffset + buf->xmargin;

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

	d1 = (r - lw - (f / 2));
	d1 = d1 * d1;
	d2 = r * r;

	cx = cy = r - 1;

	for (y = 0; y < r; y++) {
		for (x = 0; x < r; x++) {
			d = (cx-x)*(cx-x) + (cy-y)*(cy-y);
			c = (d1 <= d && d <= d2);
			abr[(oy1 + y) * buf->width + ox1 + x] = c;
			abl[(oy1 + y) * buf->width + ox2 - x] = c;
			atr[(oy2 - y) * buf->width + ox1 + x] = c;
			atl[(oy2 - y) * buf->width + ox2 - x] = c;
		}
	}
}

void
bd_drawcircle(BDBuffer *buf, int idx, int fill)
{
	char *data = buf->data + idx * buf->charoffset + buf->xmargin;
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

	d1 = fill ? 0 : (r - lw * f) * (r - lw * f);
	d2 = r * r;

	for (y = 0; y < buf->ch; y++, data += buf->width) {
		for (x = 0; x < buf->cw; x++) {
			d = (ox-x)*(ox-x) + (oy-y)*(oy-y);
			if (d1 <= d && d <= d2)
				data[x] = 1;
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
		bd_drawrect(buf, idx, x, cy, sz, lw);
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
		bd_drawrect(buf, idx, cx, y, lw, sz);
	}
}

void
bd_drawhdashes(BDBuffer *buf, int idx, int n, int heavy)
{
	int i, w, x1, x2, f = buf->factor, cw = buf->cw / f, s = 1;
	int y1 = buf->cy - (heavy ? buf->lw : 0);
	int y2 = buf->cy + (heavy ? buf->lw : 0) + buf->lw;

	if (cw < 4)
		return bd_drawrect(buf, idx, 0, y1, buf->cw, y2 - y1);

	if (cw < 7 || (cw < 12 && n >= 3) || (cw <= 16 && n == 4)) {
		n = (cw < 6) ? 2 : n;
		n = (cw < 8 && n == 4) ? 3 : n;
		w = cw / n;
		for (i = 0; i < n; i++) {
			x1 = i * cw / n;
			x2 = (i + 1) * cw / n;
			bd_drawrect(buf, idx, x1 * f, y1, (x2 - x1 - s) * f, y2 - y1);
		}
		return;
	}

	for (i = 0; i < n; i++) {
		x1 = i * buf->cw / n;
		x2 = (i + 1) * buf->cw / n;
		w = x2 - x1;
		s = w * 30 / 100;
		bd_drawrect(buf, idx, x1, y1, w - s, y2 - y1);
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
		bd_drawrect(buf, idx, x1, y1 + s/2, x2 - x1, h - s);
	}
}

void
bd_drawdiagonals(BDBuffer *buf, int lr, int rl, int cross)
{
	char *datalr = buf->data + lr * buf->charoffset;
	char *datarl = buf->data + rl * buf->charoffset;
	int w = MAX(1, buf->cw), h = MAX(1, buf->ch);
	int j, x, y, lw = buf->lw * ((buf->lw / buf->factor > 1) ? 10 : 11) / 4;

	for (x = 0; x < buf->width; x++) {
		y = (x - buf->xmargin) * h / w;
		for (j = y - lw/2; j < y + lw - lw/2; j++) {
			if (j >= 0 && j < buf->ch) {
				datalr[j * buf->width + x] = 1;
				datarl[j * buf->width + buf->width - x - 1] = 1;
			}
		}
	}
	bd_copysymbol(buf, cross, lr);
	bd_copysymbol(buf, cross, rl);
}

void
bd_copysymbol(BDBuffer *buf, int dstidx, int srcidx)
{
	char *dst = buf->data + dstidx * buf->charoffset;
	char *src = buf->data + srcidx * buf->charoffset;
	int i, size = buf->width * buf->ch;

	for (i = 0; i < size; i++)
		dst[i] |= src[i];
}
