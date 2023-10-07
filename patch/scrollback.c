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
