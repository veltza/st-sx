static void
deleteoldhyperlinks(int percent)
{
	Line line;
	int i, j, i1, i2, i3, i4, n, x, y;
	Hyperlinks *links = term.hyperlinks;

	if (!links || links->count <= 0 || percent <= 0)
		return;

	/* calculate how many old hyperlinks need to be deleted */
	n = MAX(links->count * MIN(percent, 100) / 100, 1);

	/* calculate the hyperlink indexes that will be deleted */
	i1 = i3 = (links->head - links->count + 1 + links->capacity) % links->capacity;
	i2 = i4 = i1 + n - 1;
	if (i2 >= links->capacity) {
		i2 = links->capacity - 1;
		i3 = 0;
		i4 -= links->capacity;
	}

	/* delete the hyperlink urls */
	for (i = i1; i < i1 + n; i++) {
		j = i % links->capacity;
		if (links->urls[j]) {
			free(links->urls[j]);
			links->urls[j] = NULL;
		}
	}

	/* remove the hyperlinks from the screen buffer and the scrollback */
	for (y = (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf); y < term.row; y++) {
		line = TLINEABS(y);
		for (x = 0; x < term.col; x++) {
			if (line[x].mode & ATTR_HYPERLINK) {
				i = line[x].hlink;
				if ((i >= i1 && i <= i2) || (i >= i3 && i <= i4))
					line[x].mode &= ~ATTR_HYPERLINK;
			}
		}
	}

	tfulldirt();
	links->count -= n;
}

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

	deleteoldhyperlinks(100);
}

void
parsehyperlink(int narg, char *param, char *url)
{
	char *id;
	int i, plen;
	int max_url_len = 2048;
	Hyperlinks *links = term.hyperlinks;

	/* close the current hyperlink */
	term.c.attr.mode &= ~ATTR_HYPERLINK;

	/* exit if this is the closing sequence */
	if (narg < 2 || url[0] == '\0')
		return;

	if (strlen(url) > max_url_len) {
		fprintf(stderr, "erresc (OSC 8): url is too long\n");
		return;
	}

	/* parse id */
	plen = strlen(param);
	for (id = NULL, i = 0; i < plen;) {
		if (strncmp(&param[i], "id=", 3) == 0)
			id = &param[i + 3];
		for (; i < plen && param[i] != ':'; i++)
			;
		param[i++] = '\0';
	}

	/* keep using the current hyperlink if the id and url match */
	if (id && id[0] && links->lastid[0] && links->count > 0) {
		if (!strcmp(id, links->lastid) && links->urls[links->head] &&
		    !strcmp(url, links->urls[links->head])) {
			term.c.attr.mode |= ATTR_HYPERLINK;
			term.c.attr.hlink = links->head;
			return;
		}
	}

	/* If the hyperlink buffer is full, delete 10 percent of old links */
	if (links->count >= links->capacity)
		deleteoldhyperlinks(10);

	links->count++;
	links->head = (links->head + 1) % links->capacity;
	links->urls[links->head] = xstrdup(url);
	snprintf(links->lastid, sizeof(links->lastid), "%s", id ? id : "\0");

	term.c.attr.mode |= ATTR_HYPERLINK;
	term.c.attr.hlink = links->head;
}
