#include <wchar.h>

struct {
	int x1, y1;
	int x2, y2;
	int draw;
	int click;
	int hlink;
	int hinty;
	int mousey;
	int cursory;
} activeurl = { .y2 = -1, .hlink = -1 };

struct {
	char *protocols;
	int count;
	int offset;
} urlprefixes;

static char validurlchars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._~:/?#@!$&'*+,;=%()[]";

#define ISVALIDURLCHAR(c)    ((c) < 128 && strchr(validurlchars, (int)(c)) != NULL)
#define MAX_URL   2085       /* maximum url length including NUL */

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

static int
isprotocolsupported(char *url)
{
	int offset = urlprefixes.offset;
	char *prot = urlprefixes.protocols;
	char *end = prot + urlprefixes.count * offset;
	char *p, *u;

	for (; prot < end; prot += offset) {
		for (p = prot, u = url; *p && *p == *u; p++, u++);
		if (*p == '\0')
			return 1;
	}
	return 0;
}

void
parseurlprotocols(void)
{
	int n;
	char *dst, *end, *next, *prot, *tail;

	if (urlprefixes.protocols)
		free(urlprefixes.protocols);
	urlprefixes.protocols = NULL;
	urlprefixes.count = 0;
	urlprefixes.offset = 0;

	end = url_protocols + strlen(url_protocols);
	for (n = 0, prot = url_protocols; prot < end; prot = next+1, n++) {
		if (!(next = strchr(prot, ',')))
			next = end;
		urlprefixes.offset = MAX(urlprefixes.offset, next - prot);
	}
	if (n == 0)
		return;

	urlprefixes.offset++;
	urlprefixes.protocols = xmalloc(n * urlprefixes.offset);

	for (n = 0, prot = url_protocols; prot < end; prot = next+1) {
		if (!(next = strchr(prot, ',')))
			next = end;
		for (; *prot == ' ' || *prot == '\t'; prot++);
		for (tail = next-1; tail > prot && (*tail == ' ' || *tail == '\t'); tail--);
		if (prot <= tail) {
			dst = urlprefixes.protocols + n * urlprefixes.offset;
			while (prot <= tail)
				*dst++ = *prot++;
			*dst = '\0';
			n++;
		}
	}
	urlprefixes.count = n;
}

void
clearurl(int clearhyperlinkhint)
{
	while (activeurl.y1 <= activeurl.y2 && activeurl.y1 < term.row)
		term.dirty[activeurl.y1++] = 1;

	if (clearhyperlinkhint && activeurl.hlink >= 0) {
		if (activeurl.hinty >= 0 && activeurl.hinty < term.row)
			term.dirty[activeurl.hinty] = 1;
		activeurl.hlink = -1;
	}

	activeurl.y2 = -1;
}

char *
detecthyperlink(int col, int row, int draw)
{
	Line line;
	int x, y, y1 = row, y2 = row;
	Hyperlinks *links = term.hyperlinks;
	int hlink = TLINE(row)[col].hlink;
	char *url = (hlink < links->capacity) ? links->items[hlink].url : NULL;

	if (!draw || !url)
		return url;

	/* If the url spans multiple lines, search the first and last lines */
	for (y = 0; y < term.row; y++) {
		if (y != row) {
			line = TLINE(y);
			for (x = 0; x < term.col; x++) {
				if (line[x].mode & ATTR_HYPERLINK && line[x].hlink == hlink) {
					y1 = (y < y1) ? y : y1;
					y2 = (y > y2) ? y : y2;
					term.dirty[y] = 1;
					break;
				}
			}
		}
	}
	term.dirty[row] = 1;

	activeurl.y1 = y1;
	activeurl.y2 = y2;
	activeurl.hlink = hlink;
	activeurl.hinty = -1;
	activeurl.mousey = row;
	activeurl.cursory = -1;
	activeurl.draw = 1;
	return url;
}

char *
detecturl(int col, int row, int draw)
{
	static char url[MAX_URL * 2];
	Line line;
	int b, e, x1, y1, x2, y2;
	int i = sizeof(url)/2+1, j = sizeof(url)/2;
	int row_start = row, col_start = col;
	int parentheses = 0, brackets = 0;
	int minrow = tisaltscr() ? 0 : term.scr - term.histf;
	int maxrow = tisaltscr() ? term.row - 1 : term.scr + term.row - 1;

	/* clear previously underlined url */
	if (draw)
		clearurl(1);

	line = TLINE(row);

	if ((line[col].mode & ATTR_HYPERLINK) ||
	    (line[col].mode & ATTR_WDUMMY && col > 0 && line[col-1].mode & ATTR_HYPERLINK))
		return detecthyperlink((line[col].mode & ATTR_WDUMMY) ? col-1 : col, row, draw);

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
		if (isprotocolsupported(&url[b]))
			break;
	}
	if (b < i)
		return NULL;

	/* if the url contains extra closing parentheses or brackets,
	 * we can assume that they do not belong in the url */
	for (e = b + 1; e < j; e++) {
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

	if (e > b + MAX_URL - 1) {
		e = b + MAX_URL - 1;
		url[e] = 0;
	}

	if (e <= sizeof(url)/2)
		return NULL;

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
		activeurl.hlink = -1;
		activeurl.draw = 1;
		for (y1 = activeurl.y1; y1 <= activeurl.y2; y1++)
			term.dirty[y1] = 1;
	}

	return &url[b];
}

void
drawurl(Color *fg, Mode basemode, int x, int y, int charlen, int yoff, int thickness)
{
	Line line;
	int i, j, x1, x2, xu, yu, wu, hlink;

	if (basemode & ATTR_INVISIBLE)
		fg = &dc.col[defaultfg];

	/* underline hyperlink */
	if (activeurl.hlink >= 0) {
		if (!(basemode & ATTR_HYPERLINK))
			return;
		line = TLINE(y);
		x2 = x + charlen;
		for (i = x; i < x2; i = j) {
			hlink = line[i].hlink;
			for (j = i + 1; j < x2; j++) {
				if (hlink != line[j].hlink && !(line[j].mode & ATTR_WDUMMY))
					break;
			}
			if (hlink == activeurl.hlink) {
				wu = (j - i) * win.cw;
				xu = borderpx + i * win.cw;
				yu = borderpx + y * win.ch;
				XftDrawRect(xw.draw, fg, xu,
					yu + win.cyo + dc.font.ascent + yoff, wu, thickness);
			}
		}
		return;
	}

	/* underline regular url */
	x1 = (y == activeurl.y1) ? activeurl.x1 : 0;
	x2 = (y == activeurl.y2) ? MIN(activeurl.x2, term.col-1) : term.col-1;
	if (x + charlen > x1 && x <= x2) {
		x1 = MAX(x, x1);
		wu = (x2 - x1 + 1) * win.cw;
		xu = borderpx + x1 * win.cw;
		yu = borderpx + y * win.ch;
		XftDrawRect(xw.draw, fg, xu,
			yu + win.cyo + dc.font.ascent + yoff, wu, thickness);
	}
}

void
drawhyperlinkhint(void)
{
	static Glyph g;
	char *url;
	int charsize;
	int i, x, y, w, ulen;
	int bot = term.row - 1;

	if (!showhyperlinkhint || !activeurl.draw || activeurl.hlink < 0 ||
	    !(url = term.hyperlinks->items[activeurl.hlink].url))
		return;

	y = (activeurl.mousey == bot || activeurl.cursory == bot) ? bot - 1 : bot;
	y = (activeurl.mousey == y || activeurl.cursory == y) ? y - 1 : y;
	if ((activeurl.hinty = y) < 0)
		return;

	g.mode = 0;
	g.fg = hyperlinkhintfg;
	g.bg = hyperlinkhintbg;

	ulen = strlen(url);
	for (i = 0, x = 0; i < ulen && x < term.col; i += charsize, x += w) {
		charsize = utf8decode(url + i, &g.u, ulen - i);
		if (charsize == 0)
			break;
		w = wcwidth(g.u);
		MODBIT(g.mode, (w > 1), ATTR_WIDE);
		xdrawglyph(&g, x, y);
	}

	if (x >= term.col && i < ulen) {
		/* ellipsis */
		g.u = 0x2026;
		g.mode &= ~ATTR_WIDE;
		xdrawglyph(&g, term.col - 1, y);
	} else if (x < term.col) {
		/* right padding and separator */
		w = MAX(win.cw/2, 1);
		XftDrawRect(xw.draw, &dc.col[hyperlinkhintbg],
			borderpx + x * win.cw, borderpx + y * win.ch, w, win.ch);
		XftDrawRect(xw.draw, &dc.col[defaultbg],
			borderpx + x * win.cw + w, borderpx + y * win.ch, 1, win.ch);
	}
}

void
openUrlOnClick(int col, int row, char* url_opener)
{
	char *url = detecturl(col, row, 1);
	char *argv[] = { url_opener, url, NULL };
	char fileurl[MAX_URL];
	char thishost[_POSIX_HOST_NAME_MAX];
	int hostlen;

	if (!url)
		return;

	if (!isprotocolsupported(url)) {
		fprintf(stderr, "error: protocol is not supported: '%s'\n", url);
		return;
	}

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

	switch (fork()) {
	case -1:
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		break;
	case 0:
		switch (fork()) {
		case -1:
			fprintf(stderr, "fork failed: %s\n", strerror(errno));
			_exit(1);
			break;
		case 0:
			setsid();
			execvp(argv[0], argv);
			_exit(1);
			break;
		default:
			_exit(0);
		}
	default:
		break;
	}
}

void
copyUrlOnClick(int col, int row)
{
	char *dup, *file, *url, thishost[_POSIX_HOST_NAME_MAX];
	int hostlen;

	if ((url = file = detecturl(col, row, 1)) == NULL)
		return;

	dup = strdup(url);

	if (!strncmp(url, "file:/", 6)) {
		/* remove the file protocol and hostname from local file */
		if (url[6] != '/') {           /* file:/ */
			file = url + 5;
		} else if (url[7] == '/') {    /* file:/// */
			file = url + 7;
		} else if (!strncmp(&url[7], "localhost/", 10)) {
			file = url + 16;
		} else {
			if (gethostname(thishost, sizeof(thishost)) < 0)
				thishost[0] = '\0';
			hostlen = strlen(thishost);
			if (hostlen > 0 && !strncmp(&url[7], thishost, hostlen) && url[7+hostlen] == '/')
				file = url + 7 + hostlen;
		}
		if (file != url)
			urldecode(file, dup, strlen(dup));
	} else if (!strncmp(url, "vscode://file/", 14)) {
		/* remove the vscode file protocol from vscode url */
		urldecode(url+13, dup, strlen(dup));
	}

	xsetsel(dup);
}
