#define EMPTY 65535

/* djb2 hash function */
static ushort
hash(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; // hash * 33 + c

	return hash % term.hyperlinks->hashtable.capacity;
}

static ushort
findhyperlink(const char *id, const char *url)
{
	ushort current;
	Hyperlinks *links = term.hyperlinks;

	if (!id || !id[0])
		return EMPTY;

	current = links->hashtable.buckets[hash(id)];

	while (current != EMPTY) {
		if (strcmp(links->items[current].id, id) == 0 &&
		    strcmp(links->items[current].url, url) == 0) {
			return current;
		}
		current = links->items[current].next;
	}

	return EMPTY;
}

static void
deletehyperlink(ushort hlink)
{
	ushort idx, current, next, prev = EMPTY;
	Hyperlinks *links = term.hyperlinks;

	/* Remove the hyperlink from the hash table if it has an id */
	if (links->items[hlink].id && links->items[hlink].id[0]) {
		idx = hash(links->items[hlink].id);
		current = links->hashtable.buckets[idx];
		while (current != EMPTY) {
			next = links->items[current].next;
			if (current == hlink) {
				if (prev == EMPTY)
					links->hashtable.buckets[idx] = next;
				else
					links->items[prev].next = next;
				break;
			}
			prev = current;
			current = next;
		}
	}

	free(links->items[hlink].id);
	links->items[hlink].id = NULL;
	links->items[hlink].url = NULL;
	links->items[hlink].next = EMPTY;
	links->count--;
}

static void
deletehyperlinksbypercent(int percent)
{
	Line line;
	int i, beg1, end1, beg2, end2, n, x, y;
	Hyperlinks *links = term.hyperlinks;

	if (!links || links->count <= 0 || percent <= 0)
		return;

	/* Calculate how many old hyperlinks need to be deleted */
	n = MAX(links->count * MIN(percent, 100) / 100, 1);

	/* Calculate the hyperlink indexes that will be deleted */
	beg1 = beg2 = (links->head - links->count + 1 + links->capacity) % links->capacity;
	end1 = end2 = beg1 + n - 1;
	if (end1 >= links->capacity) {
		end1 = links->capacity - 1;
		beg2 = 0;
		end2 -= links->capacity;
	}

	/* Delete hyperlinks */
	for (i = beg1; i < beg1 + n; i++)
		deletehyperlink(i % links->capacity);

	/* Remove the hyperlinks from the screen buffer and the scrollback */
	for (y = (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf); y < term.row; y++) {
		line = TLINEABS(y);
		for (x = 0; x < term.col; x++) {
			if (line[x].mode & ATTR_HYPERLINK) {
				i = line[x].hlink;
				if ((beg1 <= i && i <= end1) || (beg2 <= i && i <= end2))
					line[x].mode &= ~ATTR_HYPERLINK;
			}
		}
	}
	tfulldirt();
}

static ushort
inserthyperlink(const char *id, const char *url)
{
	char emptystr[1];
	ushort idx;
	size_t idlen, urllen;
	Hyperlinks *links = term.hyperlinks;

	/* If the hyperlink cache is full, delete 10 percent of old links */
	if (links->count >= links->capacity)
		deletehyperlinksbypercent(10);

	links->count++;
	links->head = (links->head + 1) % links->capacity;

	id = id ? id : emptystr;
	url = url ? url : emptystr;
	emptystr[0] = '\0';

	/* Allocate memory for the id and url */
	idlen = strlen(id);
	urllen = strlen(url);
	links->items[links->head].id = xmalloc(idlen + urllen + 2);
	links->items[links->head].url = links->items[links->head].id + idlen + 1;
	links->items[links->head].next = EMPTY;
	memcpy(links->items[links->head].id, id, idlen + 1);
	memcpy(links->items[links->head].url, url, urllen + 1);

	/* If the hyperlink has an id, we can store it in the hash table */
	if (id[0]) {
		/* We put the hyperlink at the beginning of the hash chain so
		 * that it can be reused if there are duplicates */
		idx = hash(id);
		links->items[links->head].next = links->hashtable.buckets[idx];
		links->hashtable.buckets[idx] = links->head;
	}

	return links->head;
}

/**
 * Delete all hyperlinks. If checkscreen is set, nothing is deleted if there
 * are hyperlinks on the screen or in the scrollback buffer.
 */
void
deletehyperlinks(int checkscreen)
{
	int x, y;
	Line line;

	if (checkscreen) {
		for (y = (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf); y < term.row; y++) {
			line = TLINEABS(y);
			for (x = 0; x < term.col; x++) {
				if (line[x].mode & ATTR_HYPERLINK)
					return;
			}
		}
	}

	deletehyperlinksbypercent(100);

	/* Make sure the last hyperlink is not left open */
	term.c.attr.mode &= ~ATTR_HYPERLINK;
}

void
parsehyperlink(int narg, char *param, char *url)
{
	char *id;
	int i, dist, len;
	ushort hlink;
	Hyperlinks *links = term.hyperlinks;
	const size_t max_id = 255;
	const size_t max_url = 2084;

	/* Close the current hyperlink */
	term.c.attr.mode &= ~ATTR_HYPERLINK;

	/* Exit if this is the closing sequence */
	if (narg < 2 || url[0] == '\0')
		return;

	if (strlen(url) > max_url) {
		fprintf(stderr, "erresc (OSC 8): url is too long\n");
		return;
	}

	/* Parse id */
	len = strlen(param);
	for (id = NULL, i = 0; i < len;) {
		if (param[i] == 'i' && param[i+1] == 'd' && param[i+2] == '=')
			id = &param[i+3];
		for (; i < len && param[i] != ':'; i++)
			;
		param[i++] = '\0';
	}
	if (id && strlen(id) > max_id)
		id[max_id] = '\0';

	/* If the hyperlink is already in the cache and it is not too far away,
	 * we can reuse it. But we should not reuse hyperlinks that are at the
	 * bottom of the cache because they may be deleted soon. */
	if ((hlink = findhyperlink(id, url)) != EMPTY) {
		dist = (links->head >= hlink) ? links->head - hlink
		                              : links->head + links->capacity - hlink;
		if (dist > links->capacity / 4)
			hlink = EMPTY;
	}

	if (hlink == EMPTY)
		hlink = inserthyperlink(id, url);

	term.c.attr.mode |= ATTR_HYPERLINK;
	term.c.attr.hlink = hlink;
}
