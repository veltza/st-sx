void
kscrolldown(const Arg* a)
{
	int n = a->i;

	if (!term.scr || IS_SET(MODE_ALTSCREEN))
		return;

	if (n < 0)
		n = MAX(term.row / -n, 1);

	if (n <= term.scr) {
		term.scr -= n;
	} else {
		n = term.scr;
		term.scr = 0;
	}

	if (sel.ob.x != -1 && !sel.alt)
		selmove(-n); /* negate change in term.scr */
	tfulldirt();

	scroll_images(-1*n);

	if (n > 0)
		restoremousecursor();
}

void
kscrollup(const Arg* a)
{
	int n = a->i;

	if (!term.histf || IS_SET(MODE_ALTSCREEN))
		return;

	if (n < 0)
		n = MAX(term.row / -n, 1);

	if (term.scr + n <= term.histf) {
		term.scr += n;
	} else {
		n = term.histf - term.scr;
		term.scr = term.histf;
	}

	if (sel.ob.x != -1 && !sel.alt)
		selmove(n); /* negate change in term.scr */
	tfulldirt();

	scroll_images(n);

	if (n > 0)
		restoremousecursor();
}

void increasehistorysize(int newsize, int col)
{
	int i, oldsize = term.histsize;

	if (newsize <= term.histsize || term.histsize >= term.histlimit || col <= 0)
		return;

	while (newsize > term.histsize)
		term.histsize += MIN_HISTSIZE;
	term.histsize = MIN(term.histsize, term.histlimit);
	term.hist = xrealloc(term.hist, term.histsize * sizeof(*term.hist));

	for (i = oldsize; i < term.histsize; i++)
		term.hist[i] = xmalloc(col * sizeof(Glyph));
}

void sethistorylimit(int limit)
{
	LIMIT(limit, 0, MAX_HISTSIZE);
	term.histlimit = limit;
}
