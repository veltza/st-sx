int url_x1, url_y1, url_x2, url_y2 = -1;
int url_draw, url_click, url_maxcol;

/* () and [] can appear in urls, but excluding them here will reduce false
 * positives when figuring out where a given url ends.
 */
static char urlchars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._~:/?#@!$&'*+,;=%";

#define ISVALIDURLCHAR(c)    ((c) < 128 && strchr(urlchars, (int)(c)) != NULL)

/* find the end of the wrapped line */
static int
findeowl(Line line)
{
	int i = term.col - 1;

	do {
		if (line[i].mode & ATTR_WRAP)
			return i;
	} while (!(line[i].mode & ATTR_SET) && --i >= 0);

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
	int x1, y1, x2, y2;
	int i = sizeof(url)/2+1, j = sizeof(url)/2;
	int row_start = row, col_start = col;
	int minrow = tisaltscr() ? 0 : term.scr - term.histf;
	int maxrow = tisaltscr() ? term.row - 1 : term.scr + term.row - 1;

	/* clear previously underlined url */
	if (draw)
		clearurl();

	url_maxcol = 0;
	line = TLINE(row);

	if (!ISVALIDURLCHAR(line[col].u))
		return NULL;

	/* find the first character of url */
	do {
		x1 = col_start, y1 = row_start;
		url_maxcol = MAX(url_maxcol, x1);
		url[--i] = line[col_start].u;
		if (--col_start < 0) {
			if (--row_start < minrow || (col_start = findeowl(TLINE(row_start))) < 0)
				break;
			line = TLINE(row_start);
		}
	} while (ISVALIDURLCHAR(line[col_start].u) && i > 0);

	/* early detection */
	if (url[i] != 'h')
		return NULL;

	/* find the last character of url */
	line = TLINE(row);
	do {
		x2 = col, y2 = row;
		url_maxcol = MAX(url_maxcol, x2);
		url[j++] = line[col].u;
		if (line[col++].mode & ATTR_WRAP) {
			if (++row > maxrow)
				break;
			col = 0;
			line = TLINE(row);
		}
	} while (col < term.col && ISVALIDURLCHAR(line[col].u) && j < sizeof(url)-1);

	url[j] = 0;

	if (strncmp("https://", &url[i], 8) && strncmp("http://", &url[i], 7))
		return NULL;

	/* Ignore some trailing characters to improve detection. */
	/* Alacritty and many other terminals also ignore these. */
	if (strchr(",.;:?!", (int)(url[j-1])) != NULL) {
		x2 = MAX(x2-1, 0);
		url[j-1] = 0;
	}

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
