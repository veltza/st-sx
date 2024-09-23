struct {
	int x1, y1;
	int x2, y2;
	int draw;
	int click;
} activeurl = { .y2 = -1 };

static char urlchars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._~:/?#@!$&'*+,;=%()[]";

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
	while (activeurl.y1 <= activeurl.y2 && activeurl.y1 < term.row)
		term.dirty[activeurl.y1++] = 1;
	activeurl.y2 = -1;
}

char *
detecturl(int col, int row, int draw)
{
	static char url[2048];
	Line line;
	int b, e, x1, y1, x2, y2;
	int i = sizeof(url)/2+1, j = sizeof(url)/2;
	int row_start = row, col_start = col;
	int parentheses = 0, brackets = 0;
	int minrow = tisaltscr() ? 0 : term.scr - term.histf;
	int maxrow = tisaltscr() ? term.row - 1 : term.scr + term.row - 1;

	/* clear previously underlined url */
	if (draw)
		clearurl();

	line = TLINE(row);

	if (!ISVALIDURLCHAR(line[col].u))
		return NULL;

	/* find the first character of a possible url */
	do {
		x1 = col_start, y1 = row_start;
		url[--i] = line[col_start].u;
		if (--col_start < 0) {
			if (--row_start < minrow || (col_start = findeowl(TLINE(row_start))) < 0)
				break;
			line = TLINE(row_start);
		}
	} while (ISVALIDURLCHAR(line[col_start].u) && i > 0);

	/* find the last character of a possible url */
	line = TLINE(row);
	do {
		url[j++] = line[col].u;
		if (line[col++].mode & ATTR_WRAP) {
			if (++row > maxrow)
				break;
			col = 0;
			line = TLINE(row);
		}
	} while (col < term.col && ISVALIDURLCHAR(line[col].u) && j < sizeof(url)-1);

	url[j] = 0;

	/* find the protocol and confirm this is the url */
	for (b = sizeof(url)/2; b >= i; b--) {
		if ((url[b] == 'f' && !strncmp("file:/", &url[b], 6)) ||
		    (url[b] == 'h' && (!strncmp("https://", &url[b], 8) ||
		                       !strncmp("http://", &url[b], 7)))) {
			break;
		}
	}
	if (b < i)
		return NULL;

	/* if the url contains extra closing parentheses or brackets,
	 * we can assume that they do not belong in the url */
	for (e = b + 7; e < j; e++) {
		if (url[e] == '(') {
			parentheses++;
		} else if (url[e] == '[') {
			brackets++;
		} else if ((url[e] == ')' && --parentheses < 0) ||
		           (url[e] == ']' && --brackets < 0)) {
			url[e] = 0;
			break;
		}
	}

	/* Ignore some trailing characters to improve detection.
	 * (Alacritty and many other terminals also ignore these) */
	while (strchr(",.;:?!'([", (int)(url[e-1])) != NULL)
		url[--e] = 0;

	if (e <= sizeof(url)/2)
		return NULL;

	/* underline the url (see xdrawglyphfontspecs() in x.c) */
	if (draw) {
		x1 += b - i;
		y1 += x1 / term.col;
		x1 %= term.col;
		x2 = x1 + e - b - 1;
		y2 = y1 + x2 / term.col;
		x2 %= term.col;
		activeurl.x1 = (y1 >= 0) ? x1 : 0;
		activeurl.x2 = (y2 < term.row) ? x2 : term.col-1;
		activeurl.y1 = MAX(y1, 0);
		activeurl.y2 = MIN(y2, term.row-1);
		activeurl.draw = 1;
		for (y1 = activeurl.y1; y1 <= activeurl.y2; y1++)
			term.dirty[y1] = 1;
	}

	return &url[b];
}

void
openUrlOnClick(int col, int row, char* url_opener)
{
	extern char **environ;
	pid_t junk;
	char *url = detecturl(col, row, 1);
	char *argv[] = { url_opener, url, NULL };
	char fileurl[1024];
	char thishost[_POSIX_HOST_NAME_MAX];
	int hostlen;

	if (!url)
		return;

	/* Don't try to open file urls that point to a different machine.
	 * We also remove the localhost from file urls, because some openers like
	 * kde-open don't work with the hostname. */
	if (!strncmp("file://", url, 7) && url[7] != '/') {
		if (gethostname(thishost, sizeof(thishost)) < 0)
			thishost[0] = '\0';
		if ((hostlen = strlen(thishost)) < sizeof(thishost)-1) {
			thishost[hostlen++] = '/';
			thishost[hostlen] = '\0';
		}
		if (strncmp("localhost/", &url[7], 10) && strncmp(thishost, &url[7], hostlen))
			return;
		snprintf(fileurl, sizeof(fileurl), "file://%s", strchr(&url[7], '/'));
		argv[1] = fileurl;
	}

	posix_spawnp(&junk, argv[0], NULL, NULL, argv, environ);
}
