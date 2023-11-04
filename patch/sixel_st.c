extern sixel_state_t sixel_st;
extern char *opt_alpha;
extern int focused;
extern int sixelbufferlen;
extern const int sixelbuffersize;
extern unsigned char *sixelbuffer;

void
dcshandle(void)
{
	unsigned char r, g, b;
	int bgcolor;
	float usedAlpha = (opt_alpha) ? strtof(opt_alpha, NULL)
	                              : focused ? alpha : alphaUnfocused;

	switch (csiescseq.mode[0]) {
	default:
		fprintf(stderr, "erresc: unknown csi ");
		csidump();
		/* die(""); */
		break;
	case 'q': /* DECSIXEL */
		xgetcolor(term.c.attr.bg, &r, &g, &b);
		bgcolor = (r << 16) | (g << 8) | (b);
		bgcolor |= (int)(255.0 * (term.c.attr.bg == defaultbg ? usedAlpha : 1.0)) << 24;
		if (sixel_parser_init(&sixel_st, (255 << 24), bgcolor, 1, win.cw, win.ch) != 0)
			perror("sixel_parser_init() failed");
		if (!sixelbuffer && !(sixelbuffer = malloc(sixelbuffersize)))
			perror("sixelbuffer allocation failed");
		sixelbufferlen = 0;
		term.mode |= MODE_SIXEL;
		break;
	}
}

void
scroll_images(int n) {
	ImageList *im;
	int top = IS_SET(MODE_ALTSCREEN) ? 0 : term.scr - HISTSIZE;

	for (im = term.images; im; im = im->next) {
		im->y += n;

		/* check if the current sixel has exceeded the maximum
		 * draw distance, and should therefore be deleted */
		if (im->y + im->rows <= top) {
			//fprintf(stderr, "im@0x%08x exceeded maximum distance\n");
			im->should_delete = 1;
		}
	}
}
