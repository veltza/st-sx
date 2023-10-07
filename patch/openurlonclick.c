int url_x1, url_y1, url_x2, url_y2 = -1;
int url_draw, url_click, url_maxcol;

static int
isvalidurlchar(Rune u)
{
	/* () and [] can appear in urls, but excluding them here will reduce false
	 * positives when figuring out where a given url ends. See copyurl patch.
	 */
	static char urlchars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789-._~:/?#@!$&'*+,;=%";
	return u < 128 && strchr(urlchars, (int)u) != NULL;
}

/* find the end of the wrapped line */
static int
findeowl(Line line)
{
	int col = term.col - 1;

	do {
		if (line[col].mode & ATTR_WRAP)
			return col;
	} while (line[col].u == ' ' && --col >= 0);
	return -1;
}

void
clearurl(void)
{
	while (url_y1 <= url_y2 && url_y1 < term.row)
		term.dirty[url_y1++] = 1;
	url_y2 = -1;
}

char *
detecturl(int col, int row, int draw)
{
	static char url[2048];
	Line line;
	int x1, y1, x2, y2, wrapped;
	int i = sizeof(url)/2+1, j = sizeof(url)/2-1;
	int row_start = row, col_start = col;
	int minrow = term.scr - term.histf, maxrow = term.scr + term.row - 1;

	if (tisaltscr())
		minrow = 0, maxrow = term.row - 1;
	url_maxcol = 0;

	/* clear previously underlined url */
	if (draw)
		clearurl();

	if (!isvalidurlchar(TLINE(row)[col].u))
		return NULL;

	/* find the first character of url */
	line = TLINE(row_start);
	do {
		x1 = col_start, y1 = row_start;
		url_maxcol = MAX(url_maxcol, x1);
		url[--i] = line[col_start].u;
		if (--col_start < 0) {
			if (--row_start < minrow || (col_start = findeowl(TLINE(row_start))) < 0)
				break;
			line = TLINE(row_start);
		}
	} while (i > 0 && isvalidurlchar(line[col_start].u));

	/* early detection */
	if (url[i] != 'h')
		return NULL;

	/* find the last character of url */
	line = TLINE(row);
	do {
		x2 = col, y2 = row;
		url_maxcol = MAX(url_maxcol, x2);
		url[++j] = line[col].u;
		wrapped = line[col].mode & ATTR_WRAP;
		if (++col >= term.col || wrapped) {
			col = 0;
			if (++row > maxrow || !wrapped)
				break;
			line = TLINE(row);
		}
	} while (j < sizeof(url)-2 && isvalidurlchar(line[col].u));

	/* Ignore some trailing characters to improve detection. */
	/* Alacritty and many other terminals also do this. */
	if (url[j] == ',' || url[j] == '.' || url[j] == ';' || url[j] == ':' ||
	    url[j] == '?' || url[j] == '!') {
		x2 = MAX(x2-1, 0);
		j--;
	}

	url[++j] = 0;

	if (strncmp("https://", &url[i], 8) && strncmp("http://", &url[i], 7))
		return NULL;

	/* underline url (see xdrawglyphfontspecs() in x.c) */
	if (draw) {
		url_x1 = (y1 >= 0) ? x1 : 0;
		url_x2 = (y2 < term.row) ? x2 : url_maxcol;
		url_y1 = MAX(y1, 0);
		url_y2 = MIN(y2, term.row-1);
		url_draw = 1;
		for (y1 = url_y1; y1 <= url_y2; y1++)
			term.dirty[y1] = 1;
	}

	return &url[i];
}

void
openUrlOnClick(int col, int row, char* url_opener)
{
	char *url = detecturl(col, row, 1);
	if (url) {
		extern char **environ;
		pid_t junk;
		char *argv[] = { url_opener, url, NULL };
		posix_spawnp(&junk, argv[0], NULL, NULL, argv, environ);
	}
}
