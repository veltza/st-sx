void
inithyperlinks(void)
{
	int i, size;
	Hyperlinks *tmp;
	HyperlinkHT *hashtable;

	for (i = 0; i < 2; i++) {
		term.hyperlinks = xmalloc(sizeof(Hyperlinks));
		memset(term.hyperlinks, 0, sizeof(Hyperlinks));

		term.hyperlinks->capacity = (i == 0) ? hyperlinkcache_pri : hyperlinkcache_alt;
		LIMIT(term.hyperlinks->capacity, 0, 65535);

		if (term.hyperlinks->capacity > 0) {
			/* items */
			size = term.hyperlinks->capacity * sizeof(*term.hyperlinks->items);
			term.hyperlinks->items = xmalloc(size);
			memset(term.hyperlinks->items, 0, size);
			/* hashtable */
			hashtable = &term.hyperlinks->hashtable;
			hashtable->capacity = term.hyperlinks->capacity;
			size = hashtable->capacity * sizeof(*hashtable->buckets);
			hashtable->buckets = xmalloc(size);
			memset(hashtable->buckets, -1, size);
		}

		tmp = term.hyperlinks;
		term.hyperlinks = term.hyperlinks_alt;
		term.hyperlinks_alt = tmp;
	}
}
