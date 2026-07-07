void
setnetwmicon(void)
{
	Imlib_Image image = imlib_load_image(ICON);

	if (!image)
		return;

	imlib_context_set_image(image);

	const uint32_t *pixels = imlib_image_get_data_for_reading_only();
	const int width  = imlib_image_get_width();
	const int height = imlib_image_get_height();
	const int size = width * height + 2;

	long *icon = xmalloc(size * sizeof(long));

	/* set the width and height of the icon */
	int i = 0;
	icon[i++] = width;
	icon[i++] = height;

	/* copy the image pixels to the icon */
	for (int j = 0; j < width * height; j++)
		icon[i++] = (ulong)pixels[j];

	/* set _net_wm_icon */
	xw.netwmicon = XInternAtom(xw.dpy, "_NET_WM_ICON", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmicon, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)icon, size);

	free(icon);

	imlib_free_image();
}
