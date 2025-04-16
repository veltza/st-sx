void
inithyperlinks(void)
{
	int i, cap;
	Hyperlinks *tmp;

	for (i = 0; i < 2; i++) {
		term.hyperlinks = xmalloc(sizeof(Hyperlinks));
		memset(term.hyperlinks, 0, sizeof(Hyperlinks));

		term.hyperlinks->capacity = (i == 0) ? hyperlinkcache_pri : hyperlinkcache_alt;
		LIMIT(term.hyperlinks->capacity, 0, 65536);

		cap = MAX(term.hyperlinks->capacity, 1);
		term.hyperlinks->urls = xmalloc(cap * sizeof(*term.hyperlinks->urls));
		memset(term.hyperlinks->urls, 0, cap * sizeof(*term.hyperlinks->urls));

		tmp = term.hyperlinks;
		term.hyperlinks = term.hyperlinks_alt;
		term.hyperlinks_alt = tmp;
	}
}
