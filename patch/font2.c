int
xloadsparefont(FcPattern *pattern, int flags)
{
	FcPattern *match;
	FcResult result;

	match = FcFontMatch(NULL, pattern, &result);
	if (!match) {
		return 1;
	}

	if (!(frc[frclen].font = XftFontOpenPattern(xw.dpy, match))) {
		FcPatternDestroy(match);
		return 1;
	}

	frc[frclen].flags = flags;
	/* Believe U+0000 glyph will present in each default font */
	frc[frclen].unicodep = 0;
	frclen++;

	return 0;
}

void
xloadsparefonts(void)
{
	FcPattern *pattern;
	double fontval;
	int fc, i;
	char **fp, **fonts;

	if (frclen != 0)
		die("can't embed spare fonts. cache isn't empty");

	/* Calculate count of spare fonts in .Xresources and defrag the table */
	for (fc = 0, i = 0; i < FONT2_XRESOURCES_SIZE; i++) {
		if (font2_xresources[i]) {
			font2_xresources[fc] = font2_xresources[i];
			if (i > fc)
				font2_xresources[i] = NULL;
			fc++;
		}
	}
	fonts = font2_xresources;

	/* Calculate count of spare fonts in config.h, if .Xresources are not used */
	if (fc == 0) {
		fc = sizeof(font2) / sizeof(*font2);
		if (fc == 0)
			return;
		fonts = font2;
	}

	/* Allocate memory for cache entries. */
	if (frccap < 4 * fc) {
		frccap += 4 * fc - frccap;
		frc = xrealloc(frc, frccap * sizeof(Fontcache));
	}

	for (fp = fonts; fp - fonts < fc; ++fp) {

		if (**fp == '-')
			pattern = XftXlfdParse(*fp, False, False);
		else
			pattern = FcNameParse((FcChar8 *)*fp);

		if (!pattern)
			die("can't open spare font %s\n", *fp);

		if (defaultfontsize > 0 && defaultfontsize != usedfontsize) {
			if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) ==
					FcResultMatch) {
				fontval *= usedfontsize / defaultfontsize;
				FcPatternDel(pattern, FC_PIXEL_SIZE);
				FcPatternDel(pattern, FC_SIZE);
				FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fontval);
			} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) ==
					FcResultMatch) {
				fontval *= usedfontsize / defaultfontsize;
				FcPatternDel(pattern, FC_PIXEL_SIZE);
				FcPatternDel(pattern, FC_SIZE);
				FcPatternAddDouble(pattern, FC_SIZE, fontval);
			}
		}

		FcPatternAddBool(pattern, FC_SCALABLE, 1);

		FcConfigSubstitute(NULL, pattern, FcMatchPattern);
		XftDefaultSubstitute(xw.dpy, xw.scr, pattern);

		if (xloadsparefont(pattern, FRC_NORMAL))
			die("can't open spare font %s\n", *fp);

		FcPatternDel(pattern, FC_SLANT);
		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
		if (xloadsparefont(pattern, FRC_ITALIC))
			die("can't open spare font %s\n", *fp);

		FcPatternDel(pattern, FC_WEIGHT);
		FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
		if (xloadsparefont(pattern, FRC_ITALICBOLD))
			die("can't open spare font %s\n", *fp);

		FcPatternDel(pattern, FC_SLANT);
		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
		if (xloadsparefont(pattern, FRC_BOLD))
			die("can't open spare font %s\n", *fp);

		FcPatternDestroy(pattern);
	}
}
