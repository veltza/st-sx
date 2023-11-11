/* Undercurl slope types */
enum undercurl_slope_type {
	UNDERCURL_SLOPE_ASCENDING = 0,
	UNDERCURL_SLOPE_TOP_CAP = 1,
	UNDERCURL_SLOPE_DESCENDING = 2,
	UNDERCURL_SLOPE_BOTTOM_CAP = 3
};

static int isSlopeRising (int x, int iPoint, int waveWidth)
{
	//    .     .     .     .
	//   / \   / \   / \   / \
	//  /   \ /   \ /   \ /   \
	// .     .     .     .     .

	// Find absolute `x` of point
	x += iPoint * (waveWidth/2);

	// Find index of absolute wave
	int absSlope = x / ((float)waveWidth/2);

	return (absSlope % 2);
}

static int getSlope (int x, int iPoint, int waveWidth)
{
	// Sizes: Caps are half width of slopes
	//    1_2       1_2       1_2      1_2
	//   /   \     /   \     /   \    /   \
	//  /     \   /     \   /     \  /     \
	// 0       3_0       3_0      3_0       3_
	// <2->    <1>         <---6---->

	// Find type of first point
	int firstType;
	x -= (x / waveWidth) * waveWidth;
	if (x < (waveWidth * (2.f/6.f)))
		firstType = UNDERCURL_SLOPE_ASCENDING;
	else if (x < (waveWidth * (3.f/6.f)))
		firstType = UNDERCURL_SLOPE_TOP_CAP;
	else if (x < (waveWidth * (5.f/6.f)))
		firstType = UNDERCURL_SLOPE_DESCENDING;
	else
		firstType = UNDERCURL_SLOPE_BOTTOM_CAP;

	// Find type of given point
	int pointType = (iPoint % 4);
	pointType += firstType;
	pointType %= 4;

	return pointType;
}

static void
undercurlspiky(GC gc, int wx, int wy, int ww, int wh, int width)
{
	int winx = wx;

	if (wh <= 0)
		return;

	// Make the underline corridor larger
	wh *= 2;

	// Set the angle of the slope to 45°
	ww = wh;

	// Position of wave is independent of word, it's absolute
	if (ww > 2)
		wx = (wx / (ww/2)) * (ww/2);

	int marginStart = winx - wx;

	// Calculate number of points with floating precision
	float n = width;			// Width of word in pixels
	n = (n / ww) * 2;			// Number of slopes (/ or \)
	n += 2;						// Add two last points
	int npoints = n;			// Convert to int

	// Total length of underline
	float waveLength = 0;

	if (npoints >= 3) {
		// We add an aditional slot in case we use a bonus point
		XPoint *points = xmalloc(sizeof(XPoint) * (npoints + 1));

		// First point (Starts with the word bounds)
		points[0] = (XPoint) {
			.x = wx + marginStart,
			.y = (isSlopeRising(wx, 0, ww))
				? (wy - marginStart + ww/2.f)
				: (wy + marginStart)
		};

		// Second point (Goes back to the absolute point coordinates)
		points[1] = (XPoint) {
			.x = (ww/2.f) - marginStart,
			.y = (isSlopeRising(wx, 1, ww))
				? (ww/2.f - marginStart)
				: (-ww/2.f + marginStart)
		};
		waveLength += (ww/2.f) - marginStart;

		// The rest of the points
		for (int i = 2; i < npoints-1; i++) {
			points[i] = (XPoint) {
				.x = ww/2,
				.y = (isSlopeRising(wx, i, ww))
					? wh/2
					: -wh/2
			};
			waveLength += ww/2;
		}

		// Last point
		points[npoints-1] = (XPoint) {
			.x = ww/2,
			.y = (isSlopeRising(wx, npoints-1, ww))
				? wh/2
				: -wh/2
		};
		waveLength += ww/2;

		// End
		if (waveLength < width) { // Add a bonus point?
			int marginEnd = width - waveLength;
			points[npoints] = (XPoint) {
				.x = marginEnd,
				.y = (isSlopeRising(wx, npoints, ww))
					? (marginEnd)
					: (-marginEnd)
			};

			npoints++;
		} else if (waveLength > width) { // Is last point too far?
			int marginEnd = waveLength - width;
			points[npoints-1].x -= marginEnd;
			if (isSlopeRising(wx, npoints-1, ww))
				points[npoints-1].y -= (marginEnd);
			else
				points[npoints-1].y += (marginEnd);
		}

		// Draw the lines
		XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), gc, points, npoints,
				CoordModePrevious);

		// Draw a second underline with an offset of 1 pixel
		if (undercurl_extra_thickness) {
			if (((win.ch / (undercurl_thickness_threshold/2)) % 2)) {
				points[0].x++;
				XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), gc, points,
						npoints, CoordModePrevious);
			}
		}

		// Free resources
		free(points);
	}
}

static void
undercurlcapped(GC gc, int wx, int wy, int ww, int wh, int width)
{
	int winx = wx;

	// Cap is half of wave width
	float capRatio = 0.5f;

	// Make the underline corridor larger
	wh *= 2;

	// Set the angle of the slope to 45°
	ww = (wh > 0) ? wh : 1;

	ww *= 1 + capRatio; // Add a bit of width for the cap

	// Position of wave is independent of word, it's absolute
	wx = (wx / ww) * ww;

	float marginStart;
	switch(getSlope(winx, 0, ww)) {
		case UNDERCURL_SLOPE_ASCENDING:
			marginStart = winx - wx;
			break;
		case UNDERCURL_SLOPE_TOP_CAP:
			marginStart = winx - (wx + (ww * (2.f/6.f)));
			break;
		case UNDERCURL_SLOPE_DESCENDING:
			marginStart = winx - (wx + (ww * (3.f/6.f)));
			break;
		case UNDERCURL_SLOPE_BOTTOM_CAP:
			marginStart = winx - (wx + (ww * (5.f/6.f)));
			break;
	}

	// Calculate number of points with floating precision
	float n = width;			// Width of word in pixels
								//					   ._.
	n = (n / ww) * 4;			// Number of points (./   \.)
	n += 2;						// Add two last points
	int npoints = n;			// Convert to int

	// Position of the pen to draw the lines
	float penX = 0;
	float penY = 0;

	if (npoints >= 3) {
		XPoint *points = xmalloc(sizeof(XPoint) * (npoints + 1));

		// First point (Starts with the word bounds)
		penX = winx;
		switch (getSlope(winx, 0, ww)) {
			case UNDERCURL_SLOPE_ASCENDING:
				penY = wy + wh/2.f - marginStart;
				break;
			case UNDERCURL_SLOPE_TOP_CAP:
				penY = wy;
				break;
			case UNDERCURL_SLOPE_DESCENDING:
				penY = wy + marginStart;
				break;
			case UNDERCURL_SLOPE_BOTTOM_CAP:
				penY = wy + wh/2.f;
				break;
		}
		points[0].x = penX;
		points[0].y = penY;

		// Second point (Goes back to the absolute point coordinates)
		switch (getSlope(winx, 1, ww)) {
			case UNDERCURL_SLOPE_ASCENDING:
				penX += ww * (1.f/6.f) - marginStart;
				penY += 0;
				break;
			case UNDERCURL_SLOPE_TOP_CAP:
				penX += ww * (2.f/6.f) - marginStart;
				penY += -wh/2.f + marginStart;
				break;
			case UNDERCURL_SLOPE_DESCENDING:
				penX += ww * (1.f/6.f) - marginStart;
				penY += 0;
				break;
			case UNDERCURL_SLOPE_BOTTOM_CAP:
				penX += ww * (2.f/6.f) - marginStart;
				penY += -marginStart + wh/2.f;
				break;
		}
		points[1].x = penX;
		points[1].y = penY;

		// The rest of the points
		for (int i = 2; i < npoints; i++) {
			switch (getSlope(winx, i, ww)) {
				case UNDERCURL_SLOPE_ASCENDING:
				case UNDERCURL_SLOPE_DESCENDING:
					penX += ww * (1.f/6.f);
					penY += 0;
					break;
				case UNDERCURL_SLOPE_TOP_CAP:
					penX += ww * (2.f/6.f);
					penY += -wh / 2.f;
					break;
				case UNDERCURL_SLOPE_BOTTOM_CAP:
					penX += ww * (2.f/6.f);
					penY += wh / 2.f;
					break;
			}
			points[i].x = penX;
			points[i].y = penY;
		}

		// End
		float waveLength = penX - winx;
		if (waveLength < width) { // Add a bonus point?
			int marginEnd = width - waveLength;
			penX += marginEnd;
			switch(getSlope(winx, npoints, ww)) {
				case UNDERCURL_SLOPE_ASCENDING:
				case UNDERCURL_SLOPE_DESCENDING:
					//penY += 0;
					break;
				case UNDERCURL_SLOPE_TOP_CAP:
					penY += -marginEnd;
					break;
				case UNDERCURL_SLOPE_BOTTOM_CAP:
					penY += marginEnd;
					break;
			}

			points[npoints].x = penX;
			points[npoints].y = penY;

			npoints++;
		} else if (waveLength > width) { // Is last point too far?
			int marginEnd = waveLength - width;
			points[npoints-1].x -= marginEnd;
			switch(getSlope(winx, npoints-1, ww)) {
				case UNDERCURL_SLOPE_TOP_CAP:
					points[npoints-1].y += marginEnd;
					break;
				case UNDERCURL_SLOPE_BOTTOM_CAP:
					points[npoints-1].y -= marginEnd;
					break;
				default:
					break;
			}
		}

		// Draw the lines
		XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), gc, points, npoints,
				CoordModeOrigin);

		// Draw a second underline with an offset of 1 pixel
		if (undercurl_extra_thickness) {
			if ( ((win.ch / (undercurl_thickness_threshold/2)) % 2)) {
				for (int i = 0; i < npoints; i++)
					points[i].x++;

				XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), gc, points,
						npoints, CoordModeOrigin);
			}
		}

		// Free resources
		free(points);
	}
}

static void
undercurlcurly(GC gc, int wx, int wy, int ww, int wh, int charlen)
{
	int narcs = charlen * 2 + 1;
	XArc *arcs = xmalloc(sizeof(XArc) * narcs);

	int i = 0;
	for (i = 0; i < charlen-1; i++) {
		arcs[i*2] = (XArc) {
			.x = wx + win.cw * i + ww / 4,
			.y = wy,
			.width = win.cw / 2,
			.height = wh,
			.angle1 = 0,
			.angle2 = 180 * 64
		};
		arcs[i*2+1] = (XArc) {
			.x = wx + win.cw * i + ww * 0.75,
			.y = wy,
			.width = win.cw/2,
			.height = wh,
			.angle1 = 180 * 64,
			.angle2 = 180 * 64
		};
	}
	// Last wave
	arcs[i*2] = (XArc) {wx + ww * i + ww / 4, wy, ww / 2, wh,
	0, 180 * 64 };
	// Last wave tail
	arcs[i*2+1] = (XArc) {wx + ww * i + ww * 0.75, wy, ceil(ww / 2.),
	wh, 180 * 64, 90 * 64};
	// First wave tail
	i++;
	arcs[i*2] = (XArc) {wx - ww/4 - 1, wy, ceil(ww / 2.), wh, 270 * 64,
	90 * 64 };

	XDrawArcs(xw.dpy, XftDrawDrawable(xw.draw), gc, arcs, narcs);
	free(arcs);
}

static void
undercurldotted(GC gc, int wx, int wy, int ww, int wlw, int charlen)
{
	unsigned int i, x;
	unsigned int numrects = charlen * 2;
	XRectangle *rects = xmalloc(sizeof(XRectangle) * numrects);

	wx += MAX(ww/8, 1); /* center the dots */

	for (x = 4*wx, i = 0; i < numrects; i++, x += 2*ww) {
		rects[i] = (XRectangle) {
			.x = x/4,
			.y = wy,
			.width = (x + ww)/4 - x/4,
			.height = wlw
		};
	}

	XFillRectangles(xw.dpy, XftDrawDrawable(xw.draw), gc, rects, i);
	free(rects);
}

static void
undercurldashed(GC gc, int wx, int wy, int ww, int wlw, int charlen)
{
	int i;
	int spc = ww / 2;
	int width = ww - spc;
	int hwidth = width - width / 2;
	XRectangle *rects = xmalloc(sizeof(XRectangle) * (charlen + 1));

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
	free(rects);
}
