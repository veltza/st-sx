static int maxpoints;
static int maxrects;
static XPoint *points;
XRectangle *rects;
BDBuffer curlbuf;

static inline void
allocpoints(int wavelen, int wavepoints)
{
	int max = ((term.col * win.cw + wavelen - 2) / wavelen + 1) * wavepoints + 1;
	if (max > maxpoints) {
		maxpoints = max;
		free(points);
		points = xmalloc(sizeof(XPoint) * maxpoints);
	}
}

static inline void
allocrects(void)
{
	int max = term.col * 2;
	if (max > maxrects) {
		maxrects = max;
		free(rects);
		rects = xmalloc(sizeof(XRectangle) * maxrects);
	}
}

static inline void
drawuclines(GC gc, int npoints, int wx, int winy, int width)
{
	XRectangle r = { .x = 0, .y = 0, .width = width, .height = win.ch };
	XSetClipRectangles(xw.dpy, gc, wx, winy, &r, 1, Unsorted);
	XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), gc, points, npoints, CoordModePrevious);

	if (undercurl_extra_thickness && ((win.ch / (undercurl_thickness_threshold/2)) % 2)) {
		points[0].x++;
		XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), gc, points, npoints, CoordModePrevious);
	}
}

static int
createcurlmask(int ch, int lw)
{
	BDBuffer ssbuf;
	int f, x, y, y1, y2, wy, cols;
	double wa, cosx;
	uchar *src, *dst;

	if (curlbuf.cw == win.cw && curlbuf.ch == ch && curlbuf.lw == lw)
		return 1;

	if (!XftDefaultHasRender(xdpy)) {
		bd_errormsg("undercurl: XRender is not available");
		return 0;
	}

	/* If the curl height is small, we use a higher subpixel resolution to improve
	 * roundness. Otherwise, we prefer a lower resolution because it improves
	 * brightness. */
	f = (ch < 4) ? 5 : 4;

	cols = DisplayWidth(xdpy, xw.scr) / win.cw;
	if (!bd_initbuffer(&curlbuf, win.cw, ch, 0, 0, lw, 0, cols, 1)) {
		bd_errormsg("undercurl: cannot allocate character buffer");
		return 0;
	} else if (!bd_initbuffer(&ssbuf, win.cw, ch, 0, 0, lw, 0, 1, f)) {
		bd_errormsg("undercurl: cannot allocate supersample buffer");
		free(curlbuf.data);
		return 0;
	}

	/* The curl is rendered with a line width that is a couple of subpixels larger
	 * than the thickness, because downsampling tends to dim the brightness and we
	 * need to compensate for that. Other terminals prefer to use the Xiaolin Wu's
	 * line algorithm instead of downsampling, which may produce better brightness
	 * and quality.*/
	lw = ssbuf.lw + 2;
	wa = (ssbuf.ch - lw) / 2.0;
	for (x = 0; x < ssbuf.cw; x++) {
		cosx = cos(2.0 * M_PI * x / ssbuf.cw);
		wy = wa - wa * cosx + (cosx < 0 ? 0.5 : 0);
		y1 = MAX(wy, 0);
		y2 = MIN(wy + lw, ssbuf.ch);
		for (y = y1;  y < y2; y++)
			ssbuf.data[y * ssbuf.width + x] = 255;
	}
	bd_downsample(&curlbuf, 0, &ssbuf, 0, 1);

	/* Repeat the curl */
	src = curlbuf.data;
	for (y = 0; y < curlbuf.ch; y++, src = dst) {
		dst = src + curlbuf.cw;
		for (x = curlbuf.width - curlbuf.cw; x > 0; x--)
			*dst++ = *src++;
	}

	bd_createmask(&curlbuf);
	free(ssbuf.data);
	return 1;
}

/* public API */

static void
undercurlcurly(Color *fg, int x, int y, int h, int len, int lw)
{
	Picture src;
	int n, w;

	if (!createcurlmask(h, lw))
		return;

	if (!(src = XftDrawSrcPicture(xd, fg)))
		return;

	while (len > 0) {
		n = MIN(len, curlbuf.cols);
		w = curlbuf.cw * n;
		XRenderComposite(xdpy, PictOpOver, src, curlbuf.mask,
			XftDrawPicture(xd), 0, 0, 0, 0, x, y, w, h);
		x += w;
		len -= n;
	}
}

static void
undercurlspiky(GC gc, int wx, int wy, int wh, int winy, int width)
{
	int i, x, x2 = wx + width;
	int ds = wh - 1, dy = -ds, wl = ds * 2;

	allocpoints(wl, 2);

	x = (wx - borderpx) / wl * wl + borderpx;
	points[0].x = x;
	points[0].y = wy + ds;

	for (i = 1; x < x2 && i < maxpoints; x += ds, dy = -dy, i++) {
		points[i].x = ds;
		points[i].y = dy;
	}
	drawuclines(gc, i, wx, winy, width);
}

static void
undercurlcapped(GC gc, int wx, int wy, int wh, int winy, int width)
{
	int i, x, x2 = wx + width;
	int ds = wh - 1, dp = MAX((ds+1)/2, 2), dy = ds, wl = ds*2 + dp*2;

	allocpoints(wl, 4);

	x = (wx - borderpx) / wl * wl + borderpx;
	points[0].x = x;
	points[0].y = wy;

	for (i = 1; x < x2 && i < maxpoints-2; x += ds+dp, dy = -dy) {
		points[i].x = ds; points[i++].y = dy;
		points[i].x = dp; points[i++].y = 0;
	}
	drawuclines(gc, i, wx, winy, width);
}

static void
undercurldotted(GC gc, int wx, int wy, int wlw, int charlen, int hyperlink)
{
	int ww = win.cw;
	unsigned int i, x, hw = MAX((ww + 4)/8, wlw);
	unsigned int numrects = charlen * 2;

	allocrects();

	/* center the dots */
	if (hyperlink)
		wx += MAX(ww/2 - hw, 0) / 2;
	else
		wx += MAX((ww + 4)/8, 1);

	for (x = 4*wx, i = 0; i < numrects; i++, x += 2*ww) {
		rects[i] = (XRectangle) {
			.x = x/4,
			.y = wy,
			.width = hyperlink ? hw : (x + ww)/4 - x/4,
			.height = wlw
		};
	}

	XFillRectangles(xw.dpy, XftDrawDrawable(xw.draw), gc, rects, i);
}

static void
undercurldashed(GC gc, int wx, int wy, int wlw, int charlen)
{
	int i, ww = win.cw;
	int spc = ww / 2;
	int width = ww - spc;
	int hwidth = width - width / 2;

	allocrects();

	/* first half-length dash */
	rects[0] = (XRectangle) {
		.x = wx,
		.y = wy,
		.width = hwidth,
		.height = wlw
	};

	/* full-length dashes */
	for (wx += hwidth + spc, i = 1; i < charlen; i++, wx += ww) {
		rects[i] = (XRectangle) {
			.x = wx,
			.y = wy,
			.width = width,
			.height = wlw
		};
	}

	/* last half-length dash */
	rects[i++] = (XRectangle) {
		.x = wx,
		.y = wy,
		.width = width - hwidth,
		.height = wlw
	};

	XFillRectangles(xw.dpy, XftDrawDrawable(xw.draw), gc, rects, i);
}
