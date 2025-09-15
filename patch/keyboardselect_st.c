#define PCRE2_CODE_UNIT_WIDTH 32
#include <pcre2.h>
#include <wctype.h>

enum keyboardselect_mode {
	KBDS_MODE_MOVE    = 0,
	KBDS_MODE_SELECT  = 1<<1,
	KBDS_MODE_LSELECT = 1<<2,
	KBDS_MODE_FIND    = 1<<3,
	KBDS_MODE_SEARCH  = 1<<4,
	KBDS_MODE_FLASH   = 1<<5,
	KBDS_MODE_REGEX   = 1<<6,
	KBDS_MODE_URL     = 1<<7,
};

enum cursor_wrap {
	KBDS_WRAP_NONE    = 0,
	KBDS_WRAP_LINE    = 1<<0,
	KBDS_WRAP_EDGE    = 1<<1,
};

typedef struct {
	int x;
	int y;
	Line line;
	int len;
} KCursor;

typedef struct {
	unsigned int start;
	unsigned int len;
	wchar_t *matched_substring;
} RegexResult;

typedef struct {
	KCursor c;
	unsigned int len;
	wchar_t *matched_substring;
} RegexKCursor;

typedef struct {
	RegexKCursor *array;
	size_t used;
	size_t size;
} RegexKCursorArray;

typedef struct {
	KCursor c;
	char *url;
} UrlKCursor;

typedef struct {
	UrlKCursor *array;
	size_t used;
	size_t size;
} UrlKCursorArray;

struct {
	int cx;
	int len;
	int maxlen;
	int dir;
	int wordonly;
	int directsearch;
	int ignorecase;
	Glyph *str;
} kbds_searchobj;

typedef struct {
	Rune *array;
	size_t used;
	size_t size;
} CharArray;

typedef struct {
	KCursor *array;
	size_t used;
	size_t size;
} KCursorArray;

static int kbds_in_use, kbds_quant;
static int kbds_seltype = SEL_REGULAR;
static int kbds_mode;
static int kbds_finddir, kbds_findtill;
static int kbds_scrolldownonexit;
static Rune kbds_findchar;
static KCursor kbds_c, kbds_oc;
static CharArray flash_next_char_record, flash_used_label, flash_used_double_label;
static KCursorArray flash_kcursor_record;
static KCursorArray flash_kcursor_match;
static RegexKCursorArray regex_kcursor_record;
static UrlKCursorArray url_kcursor_record;
static int hit_input_first = 0;
static Rune hit_input_first_label;

static const char *flash_key_label[] = {
	"j", "f", "d", "k", "l", "h", "g", "a", "s", "o",
	"i", "e", "u", "n", "c", "m", "r", "p", "b", "t",
	"w", "v", "x", "y", "q", "z",
};

static const char *flash_double_key_label[] = {
	"au", "ai", "ao", "ah", "aj", "ak", "al", "an",
	"su", "si", "so", "sh", "sj", "sk", "sl", "sn",
	"du", "di", "do", "dh", "dj", "dk", "dl", "dn",
	"fu", "fi", "fo", "fh", "fj", "fk", "fl", "fn",
	"gu", "gi", "go", "gh", "gj", "gk", "gl", "gn",
	"eu", "ei", "eo", "eh", "ej", "ek", "el", "en",
	"ru", "ri", "ro", "rh", "rj", "rk", "rl", "rn",
	"cu", "ci", "co", "ch", "cj", "ck", "cl", "cn",
	"wu", "wi", "wo", "wh", "wj", "wk", "wl", "wn",
	"tu", "ti", "to", "th", "tj", "tk", "tl", "tn",
	"vu", "vi", "vo", "vh", "vj", "vk", "vl", "vn",
	"xu", "xi", "xo", "xh", "xj", "xk", "xl", "xn",
	"bu", "bi", "bo", "bh", "bj", "bk", "bl", "bn",
	"qu", "qi", "qo", "qh", "qj", "qk", "ql", "qn",

	"ap", "ay", "am", "sp", "sy", "sm", "dp", "dy",
	"dm", "fp", "fy", "fm", "gp", "gy", "gm", "ep",
	"ey", "em", "rp", "ry", "rm", "cp", "cy", "cm",
	"wp", "wy", "wm", "tp", "ty", "tm", "vp", "vy",
	"vm", "xp", "xy", "xm", "bp", "by", "bm", "qp",
	"qy", "qm"
};

void
init_url_kcursor_array(UrlKCursorArray *a, size_t initialSize) {
	a->array = (UrlKCursor *)xmalloc(initialSize * sizeof(UrlKCursor));
	a->used = 0;
	a->size = initialSize;
}

void
insert_url_kcursor_array(UrlKCursorArray *a, UrlKCursor element) {
	if (a->used == a->size) {
		size_t newSize = a->size == 0 ? 1 : a->size * 2;
		UrlKCursor *newArray = (UrlKCursor *)xrealloc(a->array, newSize * sizeof(UrlKCursor));
		a->array = newArray;
		a->size = newSize;
	}
	a->array[a->used++] = element;
}

void
reset_url_kcursor_array(UrlKCursorArray *a) {
	for (int i = 0; i < a->used; i++) {
		free(a->array[i].url);
	}
	free(a->array);
	a->array = NULL;
	a->used = 0;
	a->size = 0;
}

void
init_regex_kcursor_array(RegexKCursorArray *a, size_t initialSize) {
	a->array = (RegexKCursor *)xmalloc(initialSize * sizeof(RegexKCursor));
	a->used = 0;
	a->size = initialSize;
}

void
insert_regex_kcursor_array(RegexKCursorArray *a, RegexKCursor element) {
	if (a->used == a->size) {
		size_t newSize = a->size == 0 ? 1 : a->size * 2;
		RegexKCursor *newArray = (RegexKCursor *)xrealloc(a->array, newSize * sizeof(RegexKCursor));
		a->array = newArray;
		a->size = newSize;
	}
	a->array[a->used++] = element;
}

void
reset_regex_kcursor_array(RegexKCursorArray *a) {
	for (int i = 0; i < regex_kcursor_record.used; i++) {
		free(regex_kcursor_record.array[i].matched_substring);
	}
	free(a->array);
	a->array = NULL;
	a->used = 0;
	a->size = 0;
}

void
init_char_array(CharArray *a, size_t initialSize) {
	a->array = (Rune *)xmalloc(initialSize * sizeof(Rune));
	a->used = 0;
	a->size = initialSize;
}

void
insert_char_array(CharArray *a, Rune element) {
	if (a->used == a->size) {
		a->size *= 2;
		a->array = (Rune *)xrealloc(a->array, a->size * sizeof(Rune));
	}
	a->array[a->used++] = element;
}

void
reset_char_array(CharArray *a) {
	free(a->array);
	a->array = NULL;
	a->used = 0;
	a->size = 0;
}

void
init_kcursor_array(KCursorArray *a, size_t initialSize) {
	a->array = (KCursor *)xmalloc(initialSize * sizeof(KCursor));
	a->used = 0;
	a->size = initialSize;
}

void
insert_kcursor_array(KCursorArray *a, KCursor element) {
	if (a->used == a->size) {
		size_t newSize = a->size == 0 ? 1 : a->size * 2;
		KCursor *newArray = (KCursor *)xrealloc(a->array, newSize * sizeof(KCursor));
		a->array = newArray;
		a->size = newSize;
	}
	a->array[a->used++] = element;
}

void
reset_kcursor_array(KCursorArray *a) {
	free(a->array);
	a->array = NULL;
	a->used = 0;
	a->size = 0;
}

int
is_in_flash_used_label(Rune label) {
	int i;
	for ( i = 0; i < flash_used_label.used; i++) {
		if (label == flash_used_label.array[i]) {
			return 1;
		}
	}
	return 0;
}

int
is_in_flash_used_double_label(Rune label) {
	int i;
	for ( i = 0; i < flash_used_double_label.used; i++) {
		if (label == flash_used_double_label.array[i]) {
			return 1;
		}
	}
	return 0;
}


int
is_in_flash_next_char_record(Rune label) {
	Rune nc;
	int i, ignorecase = (kbds_searchobj.ignorecase && label == towlower(label));
	label = ignorecase ? casefold(label) : label;
	for (i = 0; i < flash_next_char_record.used; i++) {
		nc = flash_next_char_record.array[i];
		nc = ignorecase ? casefold(nc) : nc;
		if (nc == label)
			return 1;
	}
	return 0;
}

void
kbds_drawstatusbar(int y)
{
	static char *modes[] = {
		" MOVE ", "", " SELECT ", " RSELECT ", " LSELECT ",
		" SEARCH FW ", " SEARCH BW ", " FIND FW ", " FIND BW ",
		" FLASH ", " REGEX ", "  URL "
	};
	static char quant[20] = { ' ' };
	static Glyph g;
	int i, n, m;
	int mlen, qlen;

	if (!kbds_in_use)
		return;

	g.mode = 0;
	g.fg = kbselectfg;
	g.bg = kbselectbg;

	/* draw the mode */
	if (y == 0) {
		if (kbds_isurlmode())
			m = 11;
		else if (kbds_isregexmode())
			m = 10;
		else if (kbds_isflashmode())
			m = 9;
		else if (kbds_issearchmode())
			m = 5 + (kbds_searchobj.dir < 0 ? 1 : 0);
		else if (kbds_mode & KBDS_MODE_FIND)
			m = 7 + (kbds_finddir < 0 ? 1 : 0);
		else if (kbds_mode & KBDS_MODE_SELECT)
			m = 2 + (kbds_seltype == SEL_RECTANGULAR ? 1 : 0);
		else
			m = kbds_mode;
		mlen = strlen(modes[m]);
		qlen = kbds_quant ? snprintf(quant+1, sizeof quant-1, "%i", kbds_quant) + 1 : 0;
		/* do not draw the mode if the cursor is behind it. */
		if (kbds_c.y != y || kbds_c.x < term.col - qlen - mlen) {
			for (n = mlen, i = term.col-1; i >= 0 && n > 0; i--) {
				g.u = modes[m][--n];
				xdrawglyph(&g, i, y);
			}
			for (n = qlen; i >= 0 && n > 0; i--) {
				g.u = quant[--n];
				xdrawglyph(&g, i, y);
			}
		}
	}

	/* draw the search bar */
	if (y == term.row-1 && (kbds_issearchmode() || kbds_isflashmode())) {
		/* search bar */
		for (g.u = ' ', i = 0; i < term.col; i++)
			xdrawglyph(&g, i, y);
		/* search direction */
		g.u = (kbds_searchobj.dir > 0) ? '/' : '?';
		xdrawglyph(&g, 0, y);
		/* search string and cursor */
		for (i = 0; i < kbds_searchobj.len; i++) {
			g.u = kbds_searchobj.str[i].u;
			g.mode = kbds_searchobj.str[i].mode;
			if (g.mode & ATTR_WDUMMY)
				continue;
			if (g.mode & ATTR_WIDE) {
				MODBIT(g.mode, i == kbds_searchobj.cx, ATTR_REVERSE);
				xdrawglyph(&g, i + 1, y);
			} else if (i == kbds_searchobj.cx) {
				g.mode = ATTR_WIDE;
				xdrawglyph(&g, i + 1, y);
				g.mode = ATTR_REVERSE;
				xdrawglyph(&g, i + 1, y);
			} else if (g.u != ' ') {
				g.mode = ATTR_WIDE;
				xdrawglyph(&g, i + 1, y);
			}
		}
		g.u = ' ';
		g.mode = (i == kbds_searchobj.cx) ? ATTR_REVERSE : 0;
		xdrawglyph(&g, i + 1, y);
	}
}

void
kbds_deletechar(void)
{
	int w, size;
	int cx = kbds_searchobj.cx;

	if (cx >= kbds_searchobj.len)
		return;

	w = (cx < kbds_searchobj.len-1 && kbds_searchobj.str[cx].mode & ATTR_WIDE) ? 2 : 1;
	size = kbds_searchobj.maxlen - cx - w;

	if (size > 0)
		memmove(&kbds_searchobj.str[cx], &kbds_searchobj.str[cx+w], size * sizeof(Glyph));

	kbds_searchobj.len -= w;
}

int
kbds_insertchar(Rune u)
{
	int w = (wcwidth(u) > 1) ? 2 : 1;
	int cx = kbds_searchobj.cx;
	int size = kbds_searchobj.maxlen - cx - w;

	if (u < 0x20 || cx + w > kbds_searchobj.maxlen)
		return 0;

	if (size > 0)
		memmove(&kbds_searchobj.str[cx+w], &kbds_searchobj.str[cx], size * sizeof(Glyph));

	kbds_searchobj.str[cx].u = u;
	kbds_searchobj.str[cx].mode = (w > 1) ? ATTR_WIDE : ATTR_NULL;
	if (w > 1) {
		kbds_searchobj.str[cx+1].u = 0;
		kbds_searchobj.str[cx+1].mode = ATTR_WDUMMY;
	}

	kbds_searchobj.len = MIN(kbds_searchobj.len + w, kbds_searchobj.maxlen);
	if (kbds_searchobj.str[kbds_searchobj.len-1].mode & ATTR_WIDE)
		kbds_searchobj.len--;

	kbds_searchobj.cx = MIN(kbds_searchobj.cx + w, kbds_searchobj.len);
	return 1;
}

void
kbds_pasteintosearch(const char *data, int len, int append)
{
	static char buf[BUFSIZ];
	static int buflen;
	Rune u;
	int l, n, charsize;

	if (!append)
		buflen = 0;

	for (; len > 0; len -= l, data += l) {
		l = MIN(sizeof(buf) - buflen, len);
		memmove(buf + buflen, data, l);
		buflen += l;
		for (n = 0; n < buflen; n += charsize) {
			if (IS_SET(MODE_UTF8)) {
				/* process a complete utf8 char */
				charsize = utf8decode(buf + n, &u, buflen - n);
				if (charsize == 0)
					break;
			} else {
				u = buf[n] & 0xFF;
				charsize = 1;
			}
			kbds_insertchar(u);
		}
		buflen -= n;
		/* keep any incomplete UTF-8 byte sequence for the next call */
		if (buflen > 0)
			memmove(buf, buf + n, buflen);
	}
	term.dirty[term.row-1] = 1;
}

int
kbds_top(void)
{
	return IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf + term.scr;
}

int
kbds_bot(void)
{
	return IS_SET(MODE_ALTSCREEN) ? term.row-1 : term.row-1 + term.scr;
}

int
kbds_iswrapped(KCursor *c)
{
	return c->len > 0 && (c->line[c->len-1].mode & ATTR_WRAP);
}

int
kbds_isselectmode(void)
{
	return kbds_in_use && (kbds_mode & (KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
}

int
kbds_issearchmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_SEARCH);
}

int
kbds_isflashmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_FLASH);
}

int
kbds_isregexmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_REGEX);
}

int
kbds_isurlmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_URL);
}

void
kbds_setmode(int mode)
{
	kbds_mode = mode;
	term.dirty[0] = 1;
}

void
kbds_selecttext(void)
{
	if (kbds_isselectmode()) {
		if (kbds_mode & KBDS_MODE_LSELECT)
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
		else
			selextend(kbds_c.x, kbds_c.y, kbds_seltype, 0);
		if (sel.mode == SEL_IDLE)
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
	}
}

void
kbds_copytoclipboard(void)
{
	if (kbds_mode & KBDS_MODE_LSELECT) {
		selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 1);
		sel.type = SEL_REGULAR;
	} else {
		selextend(kbds_c.x, kbds_c.y, kbds_seltype, 1);
	}
	xsetsel(getsel());
}

void
kbds_clearhighlights(void)
{
	int x, y;
	Line line;
	Rune u;

	for (y = (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf); y < term.row; y++) {
		line = TLINEABS(y);
		for (x = 0; x < term.col; x++) {
			if ((kbds_isurlmode()||kbds_isregexmode()) && line[x].mode & ATTR_FLASH_LABEL && hit_input_first == 1 && is_in_flash_used_label(line[x].u) == 1) {
				line[x].mode &= ~ATTR_FLASH_LABEL;
				u = line[x].u;
				line[x].u = line[x].ubk;
				line[x].ubk = u; //backup the first hit label for judge in double hit
				continue;
			}
			if ((kbds_isurlmode()||kbds_isregexmode()) && line[x].mode & ATTR_FLASH_LABEL && hit_input_first == 1 && is_in_flash_used_double_label(line[x].u) == 1 && line[x-1].ubk == hit_input_first_label) {
				continue;
			}
			if(hit_input_first == 0)
				line[x].mode &= ~ATTR_HIGHLIGHT;
			if (line[x].mode & ATTR_FLASH_LABEL) {
				line[x].mode &= ~ATTR_FLASH_LABEL;
				line[x].u = line[x].ubk;
			}
		}
	}
	tfulldirt();
}

void
kbds_moveto(int x, int y)
{
	if (y < 0)
		kscrollup(&((Arg){ .i = -y }));
	else if (y >= term.row)
		kscrolldown(&((Arg){ .i = y - term.row + 1 }));
	kbds_c.x = (x < 0) ? 0 : (x > term.col-1) ? term.col-1 : x;
	kbds_c.y = (y < 0) ? 0 : (y > term.row-1) ? term.row-1 : y;
	kbds_c.line = TLINE(kbds_c.y);
	kbds_c.len = tlinelen(kbds_c.line);
	if (kbds_c.x > 0 && (kbds_c.line[kbds_c.x].mode & ATTR_WDUMMY))
		kbds_c.x--;
	detecturl(kbds_c.x, kbds_c.y, 1);
}

int
kbds_moveforward(KCursor *c, int dx, int wrap)
{
	KCursor n = *c;

	n.x += dx;
	if (n.x >= 0 && n.x < term.col && (n.line[n.x].mode & ATTR_WDUMMY))
		n.x += dx;

	if (n.x < 0) {
		if (!wrap || --n.y < kbds_top())
			return 0;
		n.line = TLINE(n.y);
		n.len = tlinelen(n.line);
		if ((wrap & KBDS_WRAP_LINE) && kbds_iswrapped(&n))
			n.x = n.len-1;
		else if (wrap & KBDS_WRAP_EDGE)
			n.x = term.col-1;
		else
			return 0;
		n.x -= (n.x > 0 && (n.line[n.x].mode & ATTR_WDUMMY)) ? 1 : 0;
	} else if (n.x >= term.col) {
		if (((wrap & KBDS_WRAP_EDGE) ||
		    ((wrap & KBDS_WRAP_LINE) && kbds_iswrapped(&n))) && ++n.y <= kbds_bot()) {
			n.line = TLINE(n.y);
			n.len = tlinelen(n.line);
			n.x = 0;
		} else {
			return 0;
		}
	} else if (n.x >= n.len && dx > 0 && (wrap & KBDS_WRAP_LINE)) {
		if (n.x == n.len && kbds_iswrapped(&n) && n.y < kbds_bot()) {
			++n.y;
			n.line = TLINE(n.y);
			n.len = tlinelen(n.line);
			n.x = 0;
		} else if (!(wrap & KBDS_WRAP_EDGE)) {
			return 0;
		}
	}
	*c = n;
	return 1;
}

int
kbds_isdelim(KCursor c, int xoff, wchar_t *delims)
{
	if (xoff && !kbds_moveforward(&c, xoff, KBDS_WRAP_LINE))
		return 1;
	return wcschr(delims, c.line[c.x].u) != NULL;
}

void
kbds_jumptoprompt(int dy)
{
	int x = 0, y = kbds_c.y + dy, bot, prevscr;
	Line line;

	for (bot = kbds_bot(); bot > kbds_top(); bot--) {
		if (tlinelen(TLINE(bot)) > 0)
			break;
	}

	if ((dy > 0 && y > bot) || IS_SET(MODE_ALTSCREEN))
		return;

	LIMIT(y, kbds_top(), bot);

	for (; y >= kbds_top() && y <= bot; y += dy) {
		for (line = TLINE(y), x = 0; x < term.col; x++) {
			if (line[x].extra & EXT_FTCS_PROMPT1_START)
				goto found;
		}
		x = 0;
	}

found:
	LIMIT(y, kbds_top(), bot);
	kbds_moveto(x, y);

	/* align the prompt to the top unless select mode is on */
	if (!kbds_isselectmode()) {
		prevscr = term.scr;
		kscrolldown(&((Arg){ .i = kbds_c.y }));
		kbds_moveto(kbds_c.x, kbds_c.y + term.scr - prevscr);
	}
}

int
kbds_ismatch(KCursor c)
{
	KCursor p, m = c;
	int i, next;

	if (c.x + kbds_searchobj.len > c.len && (!kbds_iswrapped(&c) || c.y >= kbds_bot()))
		return 0;

	if (kbds_searchobj.wordonly && !kbds_isdelim(c, -1, kbds_sdelim))
		return 0;

	for (next = 0, i = 0; i < kbds_searchobj.len; i++) {
		if (kbds_searchobj.str[i].mode & ATTR_WDUMMY)
			continue;
		if ((next++ && !kbds_moveforward(&c, 1, KBDS_WRAP_LINE)) ||
		    (!kbds_searchobj.ignorecase && kbds_searchobj.str[i].u != c.line[c.x].u) ||
		    (kbds_searchobj.ignorecase && casefold(kbds_searchobj.str[i].u) != casefold(c.line[c.x].u)))
			return 0;
	}

	if (kbds_searchobj.wordonly && !kbds_isdelim(c, 1, kbds_sdelim))
		return 0;

	for (c = m, i = 0; i < kbds_searchobj.len; i++) {
		if (!(kbds_searchobj.str[i].mode & ATTR_WDUMMY)) {
			c.line[c.x].mode |= ATTR_HIGHLIGHT;
			kbds_moveforward(&c, 1, KBDS_WRAP_LINE | KBDS_WRAP_EDGE);
		}
	}

	if (kbds_isflashmode()) {
		/* Move the cursor to the end of the previous line if the line
		 * was not wrapped, because we want to keep the label on the
		 * same line as the match. */
		if (c.x == 0 || c.y == term.row-1) {
			p = c;
			p.x = 0;
			if (kbds_moveforward(&p, -1, KBDS_WRAP_LINE | KBDS_WRAP_EDGE) &&
			    (!kbds_iswrapped(&p) || c.y == term.row-1)) {
				c = p;
			}
		}
		c.line[c.x].ubk = c.line[c.x].u;
		insert_char_array(&flash_next_char_record, c.line[c.x].u);
		insert_kcursor_array(&flash_kcursor_record, c);
		insert_kcursor_array(&flash_kcursor_match, m);
	}

	return 1;
}

int
kbds_searchall(void)
{
	KCursor c;
	int count = 0;
	int i, j, is_invalid_label;
	CharArray valid_label;
	Rune nc;

	init_char_array(&flash_next_char_record, 1);
	init_char_array(&valid_label, 1);
	init_char_array(&flash_used_label, 1);
	init_kcursor_array(&flash_kcursor_record, 1);
	init_kcursor_array(&flash_kcursor_match, 1);

	if (!kbds_searchobj.len)
		return 0;

	int begin = kbds_isflashmode() ? 0 : kbds_top();
	int end = kbds_isflashmode() ? MAX(term.row-2, 0) : kbds_bot();

	for (c.y = begin; c.y <= end; c.y++) {
		c.line = TLINE(c.y);
		c.len = tlinelen(c.line);
		for (c.x = 0; c.x < c.len; c.x++)
			count += kbds_ismatch(c);
	}

	for (i = 0; i < LEN(flash_key_label); i++) {
		is_invalid_label = 0;
		for ( j = 0; j < flash_next_char_record.used; j++) {
			nc = flash_next_char_record.array[j];
			if ((!kbds_searchobj.ignorecase && nc == *flash_key_label[i]) ||
			    (kbds_searchobj.ignorecase && casefold(nc) == casefold(*flash_key_label[i]))) {
				is_invalid_label = 1;
				break;
			}
		}
		if (is_invalid_label == 0) {
			insert_char_array(&valid_label, *flash_key_label[i]);
		}
	}

	for ( i = 0; i < flash_kcursor_record.used; i++) {
		if (i < valid_label.used) {
			flash_kcursor_record.array[i].line[flash_kcursor_record.array[i].x].mode |= ATTR_FLASH_LABEL;
			insert_char_array(&flash_used_label, valid_label.array[i]);
			flash_kcursor_record.array[i].line[flash_kcursor_record.array[i].x].u = valid_label.array[i];
		}
	}

	reset_char_array(&valid_label);

	tfulldirt();

	return count;
}

int apply_regex_result(KCursor c, RegexResult result) {
	KCursor m;
	RegexKCursor regex_kcursor, rd;
	KCursor target_cursor;
	int i;
	int is_cross_match;
	int is_same_value_regex = 0;

	target_cursor.y = c.y;
	target_cursor.line = TLINE(target_cursor.y);
	target_cursor.len = tlinelen(target_cursor.line);
	target_cursor.x = c.x;

	// get the real position of match cursor
	for(i = 0; i < result.start; i++) {
		if (target_cursor.line[target_cursor.x].mode & ATTR_WDUMMY) {
			i--;
		}
		kbds_moveforward(&target_cursor, 1, KBDS_WRAP_LINE);
	}

	m.y = target_cursor.y;
	m.line = TLINE(m.y);
	m.len = tlinelen(m.line);
	m.x = target_cursor.x;
	regex_kcursor.c = m;
	regex_kcursor.len = result.len;
	regex_kcursor.matched_substring = result.matched_substring;
	regex_kcursor.c.line[regex_kcursor.c.x].ubk = regex_kcursor.c.line[regex_kcursor.c.x].u;
	is_cross_match = 0;
	// check the match position is cross match
	for (i = 0; i < regex_kcursor_record.used; i++) {
		rd = regex_kcursor_record.array[i];

		if (regex_kcursor.c.y == rd.c.y &&
			(regex_kcursor.c.x == rd.c.x ||
				(regex_kcursor.c.x > rd.c.x && regex_kcursor.c.x <= (rd.c.x + rd.len - 1)) ||
				(regex_kcursor.c.x < rd.c.x && (regex_kcursor.c.x + regex_kcursor.len -1) >= rd.c.x)
			)
		   ) {
			is_cross_match = 1;
			break;
		}

		// check if the matched string is already in the cache
		if (enable_regex_same_label && wcscmp(regex_kcursor.matched_substring, regex_kcursor_record.array[i].matched_substring) == 0) {
			is_same_value_regex = 1;
		}
	}
	if (is_cross_match == 0) { // if new position match, record it
		insert_regex_kcursor_array(&regex_kcursor_record, regex_kcursor);
	} else {
		free(regex_kcursor.matched_substring);
	}

	if (is_same_value_regex || is_cross_match)
		return 0;
	else
		return 1;
}

int get_position_from_regex(KCursor c, char *pattern_mb, wchar_t *wstr) {
	RegexResult result;
	result.matched_substring = NULL;
	int new_count = 0;
	int label_need = 0;

	// check if the pattern contains any subpatterns
	int num_subpatterns = 0;
	for (int i = 0; pattern_mb[i] != '\0'; ++i) {
		if (pattern_mb[i] == '(') {
			num_subpatterns++;
		}
	}

	// if there are no subpatterns, exit with an error
	if (num_subpatterns == 0) {
		printf("No subpatterns found in pattern: %s\n", pattern_mb);
		return 0;
	}

	// convert the pattern into wide character string
	size_t pattern_len = mbstowcs(NULL, pattern_mb, 0) + 1;
	wchar_t *pattern = xmalloc(pattern_len * sizeof(wchar_t));

	mbstowcs(pattern, pattern_mb, pattern_len);

	// convert the pattern into PCRE2_UCHAR32
	PCRE2_UCHAR32 *wpattern = xmalloc(pattern_len * sizeof(PCRE2_UCHAR32));

	for (size_t i = 0; i < pattern_len; i++) {
		wpattern[i] = (PCRE2_UCHAR32)pattern[i];
	}

	// create the regex object
	int errorcode;
	PCRE2_SIZE erroffset;
	pcre2_code *re = pcre2_compile(wpattern, PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroffset, NULL);
	free(pattern);
	free(wpattern);
	if (!re) {
		PCRE2_UCHAR buffer[256];
		pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
		fprintf(stderr, "PCRE2 compilation failed at offset %zu: %ls\n", erroffset, (wchar_t *)buffer);
		return 0;
	}

	// convert the text into PCRE2_UCHAR32
	size_t len = wcslen(wstr);
	PCRE2_UCHAR32 *wtext = xmalloc((len + 1) * sizeof(PCRE2_UCHAR32));

	for (size_t i = 0; i < len; i++) {
		wtext[i] = (PCRE2_UCHAR32)wstr[i];
	}
	wtext[len] = 0;

	pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
	PCRE2_SIZE start_offset = 0;
	while (start_offset < len) {
		int ret = pcre2_match(re, wtext, len, start_offset, 0, match_data, NULL);
		if (ret >= 0) {
			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
			result.start = ovector[2];
			result.len = ovector[3] - ovector[2];

			// get the matched string
			wchar_t *match_str = xmalloc((result.len + 1) * sizeof(wchar_t));

			wcsncpy(match_str, wstr + result.start, result.len);
			match_str[result.len] = L'\0';
			result.matched_substring = match_str;
			new_count = apply_regex_result(c, result);
			label_need = label_need + new_count;
			start_offset = ovector[1];
		} else if (ret == PCRE2_ERROR_NOMATCH) {
			break;
		} else {
			break;
		}
	}

	// free the regex object and the converted string
	pcre2_match_data_free(match_data);
	pcre2_code_free(re);
	free(wtext);
	return label_need;
}

int
kbds_ismatch_regex(unsigned int begin, unsigned int end, unsigned int len)
{
	wchar_t *target_str;
	unsigned int i,j;
	char *pattern;
	unsigned h = 0;
	KCursor c,begin_c;

	if (len == 0)
		return 0;

	target_str = xmalloc((len + 1) * sizeof(wchar_t));
	begin_c.y = begin;
	begin_c.line = TLINE(begin);
	begin_c.len = tlinelen(begin_c.line);
	begin_c.x = 0;
	int label_need = 0;
	int new_count = 0;

	for (c.y = begin; c.y <= end; c.y++) {
		c.line = TLINE(c.y);
		c.len = tlinelen(c.line);
		for (j = 0; j < c.len; j++) {
			if (!(c.line[j].mode & ATTR_WDUMMY) ) {
				target_str[h] = (wchar_t)c.line[j].u;
				h++;
			}
		}
		target_str[h] = L'\0';
	}

	for (i=0; pattern_list[i] != NULL; i++) {
		pattern = pattern_list[i];
		new_count = get_position_from_regex(begin_c,pattern, target_str);
		label_need = label_need + new_count;
	}
	free(target_str);
	return label_need;
}

int
kbds_search_regex(void)
{
	KCursor c;
	unsigned int i,j;
	unsigned int is_exists_str;
	unsigned int is_exists_str_index = 0;
	unsigned int count = 0;
	unsigned int begin_y = 0;
	unsigned int str_len = 0;
	int label_need = 0;
	int new_count = 0;

	init_char_array(&flash_used_label, 1);
	init_char_array(&flash_used_double_label, 1);
	init_regex_kcursor_array(&regex_kcursor_record, 1);

	// read a full line to match regex
	for (c.y = 0; c.y <= term.row - 1; c.y++) {
		c.line = TLINE(c.y);
		c.len = tlinelen(c.line);
		str_len = str_len + c.len;
		if (!kbds_iswrapped(&c) || c.y == term.row - 1) {
			new_count = kbds_ismatch_regex(begin_y,c.y,str_len);
			label_need = label_need + new_count;
			begin_y = c.y + 1;
			str_len = 0;
		}
	}

	Glyph *label_pos1, *label_pos2, *same_value_pos1,*same_value_pos2;
	char label1, label2;

	// assign label to the matched string
	for ( i = 0; i < regex_kcursor_record.used; i++) {
		// Check whether the label is used up
		if (label_need > LEN(flash_key_label) - 1 && count >= LEN(flash_double_key_label)) {
			break;
		} else if(label_need <= LEN(flash_key_label) - 1 && count >= LEN(flash_double_key_label)) {
			break;
		}

		is_exists_str = 0;

		label_pos1 = &regex_kcursor_record.array[i].c.line[regex_kcursor_record.array[i].c.x];
		label_pos2 = &regex_kcursor_record.array[i].c.line[regex_kcursor_record.array[i].c.x + 1];
		label1 = flash_double_key_label[count][0];
		label2 = flash_double_key_label[count][1];

		if (i == 0) { // first match
			if (label_need > LEN(flash_key_label) - 1) { // double label
				label_pos1->mode |= ATTR_FLASH_LABEL;
				label_pos1->ubk = label_pos1->u;
				label_pos1->u = label1;
				label_pos2->mode |= ATTR_FLASH_LABEL;
				label_pos2->ubk = label_pos2->u;
				label_pos2->u = label2;
				insert_char_array(&flash_used_label, label1);
				insert_char_array(&flash_used_double_label, label2);
				count++;
				continue;
			} else { // single label
				label_pos1->mode |= ATTR_FLASH_LABEL;
				label_pos1->ubk = label_pos1->u;
				label_pos1->u = *flash_key_label[count];
				insert_char_array(&flash_used_label, *flash_key_label[count]);
				count++;
				continue;
			}
		}

		for (int j = 0; j < i; j++) { // check if the matched string is already in the cache
			if (enable_regex_same_label == 0)
				break;
			if(wcscmp(regex_kcursor_record.array[i].matched_substring,regex_kcursor_record.array[j].matched_substring) == 0) {
				is_exists_str = 1;
				is_exists_str_index = j;
				break;
			}
		}

		// record the first hit pos of the same url
		same_value_pos1 = &regex_kcursor_record.array[is_exists_str_index].c.line[regex_kcursor_record.array[is_exists_str_index].c.x];
		same_value_pos2 = &regex_kcursor_record.array[is_exists_str_index].c.line[regex_kcursor_record.array[is_exists_str_index].c.x+1];


		if(label_need > LEN(flash_key_label) - 1) {  // double label
			label_pos1->mode |= ATTR_FLASH_LABEL;
			label_pos1->ubk = label_pos1->u;
			label_pos2->mode |= ATTR_FLASH_LABEL;
			label_pos2->ubk = label_pos2->u;

			if (is_exists_str == 0) { // new value match, use new label
				label_pos1->u = label1;
				label_pos2->u = label2;
				insert_char_array(&flash_used_label, label1);
				insert_char_array(&flash_used_double_label, label2);
				count++;
			} else { // same value match, use same label of the first hit
				label_pos1->u = same_value_pos1->u;
				label_pos2->u = same_value_pos2->u;
			}
		} else {  // single label
			label_pos1->mode |= ATTR_FLASH_LABEL;
			label_pos1->ubk = label_pos1->u;
			label_pos1->u = *flash_key_label[count];
			if (is_exists_str == 0) { // new value match, use new label
				label_pos1->u = *flash_key_label[count];
				insert_char_array(&flash_used_label, *flash_key_label[count]);
				count++;
			} else { // same value match, use same label of the first hit
				label_pos1->u = same_value_pos1->u;
			}
		}
	}

	// highlight the matched string
	KCursor temp_c;
	for ( i = 0; i < regex_kcursor_record.used;i++) {
		temp_c.y = regex_kcursor_record.array[i].c.y;
		temp_c.line = TLINE(temp_c.y);
		temp_c.len = tlinelen(temp_c.line);
		temp_c.x = regex_kcursor_record.array[i].c.x;
		for ( j = 0; j < regex_kcursor_record.array[i].len; j++) {
			temp_c.line[temp_c.x].mode |= ATTR_HIGHLIGHT;
			kbds_moveforward(&temp_c, 1, KBDS_WRAP_LINE);
		}
	}

	hit_input_first = 0; // begin hit first label
	tfulldirt();

	return count;
}

void copy_regex_result(wchar_t *wstr) {
	size_t mb_size = wcstombs(NULL, wstr, 0) + 1;
	char *mb_str = (char *)xmalloc(mb_size * sizeof(char));
	wcstombs(mb_str, wstr, mb_size);
	xsetsel(mb_str);
}

int
kbds_search_url(void)
{
	KCursor c, m;
	UrlKCursor url_kcursor;
	unsigned int h, i;
	unsigned int count = 0;
	char *url;
	int is_exists_url = 0;
	int repeat_exists_url_index = 0;
	int head = 0;
	int head_hit = 0;
	int bottom_hit = 0;
	int hit_url_y;
	unsigned int label_need = 0;

	init_char_array(&flash_used_label, 1);
	init_char_array(&flash_used_double_label, 1);
	init_url_kcursor_array(&url_kcursor_record, 1);

	for (c.y = 0; c.y <= term.row - 1; c.y++) {
		c.line = TLINE(c.y);
		c.len = tlinelen(c.line);

		for (c.x = 0; c.x < c.len; c.x++) {
			url = detecturl(c.x,c.y,0);
			if (!url && !head_hit) {
				continue;
			} else if (!head_hit) { // find the first char which is belong to a url
				head = c.x;
				head_hit = 1;
				hit_url_y = c.y;
			}

			// find the last char which is belong to the url
			if (head_hit && (!url || ((!kbds_iswrapped(&c) || c.y == term.row-1) && c.x == c.len - 1))) {
				bottom_hit = 1;
			}

			// complete one url match
			if (head_hit && bottom_hit) {
				url = detecturl(head,hit_url_y,0);
				if (url) {
					is_exists_url = 0;
					// check if the url is already in the cache
					for (h = 0; h < url_kcursor_record.used; h++) {
						if (enable_url_same_label == 0) // if disable same label
							break;
						if (strcmp(url_kcursor_record.array[h].url, url) == 0) {
							is_exists_url = 1;
							repeat_exists_url_index = h;
							break;
						}
					}
					// calculate the number of labels needed
					if (!is_exists_url) {
						label_need ++;
					}
					// record the position of the url
					m.x = head;
					m.y = hit_url_y;
					m.line = TLINE(hit_url_y);
					m.len = tlinelen(m.line);
					url_kcursor.c = m;
					url_kcursor.url = strdup(url);
					insert_url_kcursor_array(&url_kcursor_record, url_kcursor);
				}
				// reset to match the next url
				head = 0;
				head_hit = 0;
				bottom_hit = 0;
			}
		}
	}

	Glyph *label_pos1, *label_pos2, *same_value_pos1,*same_value_pos2;
	char label1, label2;

	// assign label to the matched url
	for ( i = 0; i < url_kcursor_record.used; i++) {
		// Check whether the label is used up
		if (label_need > LEN(flash_key_label) - 1 && count >= LEN(flash_double_key_label)) {
			break;
		} else if(label_need <= LEN(flash_key_label) - 1 && count >= LEN(flash_double_key_label)) {
			break;
		}

		is_exists_url = 0;
		label_pos1 = &url_kcursor_record.array[i].c.line[url_kcursor_record.array[i].c.x];
		label_pos2 = &url_kcursor_record.array[i].c.line[url_kcursor_record.array[i].c.x + 1];
		label1 = flash_double_key_label[count][0];
		label2 = flash_double_key_label[count][1];

		if (i == 0) { // first match
			if (label_need > LEN(flash_key_label) - 1) { // double label
				label_pos1->mode |= ATTR_FLASH_LABEL;
				label_pos1->ubk = label_pos1->u;
				label_pos1->u = label1;
				label_pos2->mode |= ATTR_FLASH_LABEL;
				label_pos2->ubk = label_pos2->u;
				label_pos2->u = label2;
				insert_char_array(&flash_used_label, label1);
				insert_char_array(&flash_used_double_label, label2);
				count++;
				continue;
			} else { // single label
				label_pos1->mode |= ATTR_FLASH_LABEL;
				label_pos1->ubk = label_pos1->u;
				label_pos1->u = *flash_key_label[count];
				insert_char_array(&flash_used_label, *flash_key_label[count]);
				count++;
				continue;
			}
		}

		// check if the url is already in the cache
		for (h = 0; h < i; h++) {
			if (enable_url_same_label == 0)
				break;
			if (strcmp(url_kcursor_record.array[h].url, url_kcursor_record.array[i].url) == 0) {
				is_exists_url = 1;
				repeat_exists_url_index = h; // record the index of the first hit
				break;
			}
		}

		// record the first hit pos of the same url
		same_value_pos1 = &url_kcursor_record.array[repeat_exists_url_index].c.line[url_kcursor_record.array[repeat_exists_url_index].c.x];
		same_value_pos2 = &url_kcursor_record.array[repeat_exists_url_index].c.line[url_kcursor_record.array[repeat_exists_url_index].c.x+1];

		if(label_need > LEN(flash_key_label) - 1) {  // double label
			label_pos1->mode |= ATTR_FLASH_LABEL;
			label_pos1->ubk = label_pos1->u;
			label_pos2->mode |= ATTR_FLASH_LABEL;
			label_pos2->ubk = label_pos2->u;

			if (is_exists_url == 0) { // new value match, use new label
				label_pos1->u = label1;
				label_pos2->u = label2;
				insert_char_array(&flash_used_label, label1);
				insert_char_array(&flash_used_double_label, label2);
				count++;
			} else { // same value match, use same label of the first hit
				label_pos1->u = same_value_pos1->u;
				label_pos2->u = same_value_pos2->u;
			}
		} else {  // single label
			label_pos1->mode |= ATTR_FLASH_LABEL;
			label_pos1->ubk = label_pos1->u;
			label_pos1->u = *flash_key_label[count];
			if (is_exists_url == 0) { // new value match, use new label
				label_pos1->u = *flash_key_label[count];
				insert_char_array(&flash_used_label, *flash_key_label[count]);
				count++;
			} else { // same value match, use same label of the first hit
				label_pos1->u = same_value_pos1->u;
			}
		}
	}

	hit_input_first = 0; // begin hit first label
	tfulldirt();

	return count;
}

void
jump_to_label(Rune label) {
	int i;

	// double label hit
	if (kbds_isurlmode() && flash_used_double_label.used > 0) {
		for ( i = 0; i < url_kcursor_record.used; i++) {
			// hit first label
			if (hit_input_first == 0 && label == url_kcursor_record.array[i].c.line[url_kcursor_record.array[i].c.x].u) {
				hit_input_first = 1;
				hit_input_first_label = label;
				return;
			}
			// hit second label
			if (hit_input_first == 1 && hit_input_first_label == url_kcursor_record.array[i].c.line[url_kcursor_record.array[i].c.x].ubk && label == url_kcursor_record.array[i].c.line[url_kcursor_record.array[i].c.x + 1].u) {
				hit_input_first = 0;
				kbds_clearhighlights();
				openUrlOnClick(url_kcursor_record.array[i].c.x, url_kcursor_record.array[i].c.y, url_opener);
				return;
			}
		}
	} else if (kbds_isurlmode()) { // single label hit
		for ( i = 0; i < url_kcursor_record.used; i++) {
			if (label == url_kcursor_record.array[i].c.line[url_kcursor_record.array[i].c.x].u) {
				kbds_clearhighlights();
				hit_input_first = 0;
				openUrlOnClick(url_kcursor_record.array[i].c.x, url_kcursor_record.array[i].c.y, url_opener);
				return;
			}
		}
	}

	// double label hit
	if (kbds_isregexmode() && flash_used_double_label.used > 0) {
		for ( i = 0; i < regex_kcursor_record.used; i++) {
			// hit first label
			if (hit_input_first == 0 && label == regex_kcursor_record.array[i].c.line[regex_kcursor_record.array[i].c.x].u) {
				hit_input_first = 1;
				hit_input_first_label = label;
				return;
			}
			// hit second label
			if (hit_input_first == 1 && hit_input_first_label == regex_kcursor_record.array[i].c.line[regex_kcursor_record.array[i].c.x].ubk && label == regex_kcursor_record.array[i].c.line[regex_kcursor_record.array[i].c.x + 1].u) {
				hit_input_first = 0;
				kbds_clearhighlights();
				copy_regex_result(regex_kcursor_record.array[i].matched_substring);
				return;
			}
		}
	} else if (kbds_isregexmode()) { // single label hit
		for ( i = 0; i < regex_kcursor_record.used; i++) {
			if (label == regex_kcursor_record.array[i].c.line[regex_kcursor_record.array[i].c.x].u) {
				kbds_clearhighlights();
				hit_input_first = 0;
				copy_regex_result(regex_kcursor_record.array[i].matched_substring);
				return;
			}
		}
	}

	for ( i = 0; i < flash_kcursor_record.used; i++) {
		if (label == flash_kcursor_record.array[i].line[flash_kcursor_record.array[i].x].u) {
			kbds_clearhighlights();
			kbds_moveto(flash_kcursor_match.array[i].x, flash_kcursor_match.array[i].y);
		}
	}
}

void
clear_flash_cache(void) {
	reset_char_array(&flash_next_char_record);
	reset_char_array(&flash_used_label);
	reset_kcursor_array(&flash_kcursor_record);
	reset_kcursor_array(&flash_kcursor_match);
}

void
clear_regex_cache(void) {
	hit_input_first = 0;
	reset_regex_kcursor_array(&regex_kcursor_record);
	reset_char_array(&flash_used_label);
	reset_char_array(&flash_used_double_label);
}

void
clear_url_cache(void) {
	hit_input_first = 0;
	reset_url_kcursor_array(&url_kcursor_record);
	reset_char_array(&flash_used_label);
	reset_char_array(&flash_used_double_label);
}

void
kbds_searchnext(int dir)
{
	KCursor c = kbds_c, n = kbds_c;
	int wrapped = 0;

	if (!kbds_searchobj.len) {
		kbds_quant = 0;
		return;
	}

	if (dir < 0 && c.x > c.len)
		c.x = c.len;

	for (kbds_quant = MAX(kbds_quant, 1); kbds_quant > 0;) {
		if (!kbds_moveforward(&c, dir, KBDS_WRAP_LINE)) {
			c.y += dir;
			if (c.y < kbds_top())
				c.y = kbds_bot(), wrapped++;
			else if (c.y > kbds_bot())
				c.y = kbds_top(), wrapped++;
			if (wrapped > 1)
				break;;
			c.line = TLINE(c.y);
			c.len = tlinelen(c.line);
			c.x = (dir < 0 && c.len > 0) ? c.len-1 : 0;
			c.x -= (c.x > 0 && (c.line[c.x].mode & ATTR_WDUMMY)) ? 1 : 0;
		}
		if (kbds_ismatch(c)) {
			n = c;
			kbds_quant--;
		}
	}

	kbds_moveto(n.x, n.y);
	kbds_quant = 0;
}

void
kbds_searchwordorselection(int dir)
{
	int ney;
	KCursor c = kbds_c;

	kbds_searchobj.cx = kbds_searchobj.len = 0;
	kbds_clearhighlights();

	if (kbds_isselectmode()) {
		c.x = sel.nb.x;
		c.y = sel.nb.y;
		c.line = TLINE(c.y);
		c.len = tlinelen(c.line);
		ney = (kbds_seltype == SEL_RECTANGULAR) ? sel.nb.y : sel.ne.y;
	} else {
		while (kbds_isdelim(c, 0, kbds_sdelim)) {
			if (!kbds_moveforward(&c, 1, KBDS_WRAP_LINE))
				return;
		}
		while (!kbds_isdelim(c, -1, kbds_sdelim))
			kbds_moveforward(&c, -1, KBDS_WRAP_LINE);
	}

	kbds_searchobj.maxlen = term.col;
	for (kbds_c = c; kbds_searchobj.len < kbds_searchobj.maxlen;) {
		if (!kbds_insertchar(c.line[c.x].u) ||
		    !kbds_moveforward(&c, 1, KBDS_WRAP_LINE) ||
		    (kbds_isselectmode() && ((c.x > sel.ne.x && c.y == ney) || c.y > ney)) ||
		    (!kbds_isselectmode() && kbds_isdelim(c, 0, kbds_sdelim)))
			break;
	}

	kbds_searchobj.dir = dir;
	kbds_searchobj.ignorecase = 1;
	kbds_searchobj.wordonly = !kbds_isselectmode();
	selclear();
	kbds_setmode(KBDS_MODE_MOVE);
	kbds_moveto(kbds_c.x, kbds_c.y);
	kbds_searchall();
	kbds_searchnext(kbds_searchobj.dir);
}

void
kbds_findnext(int dir, int repeat)
{
	KCursor prev, c = kbds_c, n = kbds_c;
	int skipfirst, yoff = 0;

	if (c.len <= 0 || kbds_findchar == 0) {
		kbds_quant = 0;
		return;
	}

	if (dir < 0 && c.x > c.len)
		c.x = c.len;

	kbds_quant = MAX(kbds_quant, 1);
	skipfirst = (kbds_quant == 1 && repeat && kbds_findtill);

	while (kbds_quant > 0) {
		prev = c;
		if (!kbds_moveforward(&c, dir, KBDS_WRAP_LINE))
			break;
		if (c.line[c.x].u == kbds_findchar) {
			if (skipfirst && prev.x == kbds_c.x && prev.y == kbds_c.y) {
				skipfirst = 0;
				continue;
			}
			n.x = kbds_findtill ? prev.x : c.x;
			n.y = c.y;
			yoff = kbds_findtill ? prev.y - c.y : 0;
			kbds_quant--;
		}
	}

	kbds_moveto(n.x, n.y);
	kbds_moveto(kbds_c.x, kbds_c.y + yoff);
	kbds_quant = 0;
}

void
kbds_nextword(int start, int dir, wchar_t *delims)
{
	KCursor c = kbds_c, n = kbds_c;
	int xoff = start ? -1 : 1;

	if (dir < 0 && c.x > c.len)
		c.x = c.len;
	else if (dir > 0 && c.x >= c.len && c.len > 0)
		c.x = c.len-1;

	for (kbds_quant = MAX(kbds_quant, 1); kbds_quant > 0;) {
		if (!kbds_moveforward(&c, dir, KBDS_WRAP_LINE)) {
			c.y += dir;
			if (c.y < kbds_top() || c.y > kbds_bot())
				break;
			c.line = TLINE(c.y);
			c.len = tlinelen(c.line);
			c.x = (dir < 0 && c.len > 0) ? c.len-1 : 0;
			c.x -= (c.x > 0 && (c.line[c.x].mode & ATTR_WDUMMY)) ? 1 : 0;
		}
		if (c.len > 0 &&
		    !kbds_isdelim(c, 0, delims) && kbds_isdelim(c, xoff, delims)) {
			n = c;
			kbds_quant--;
		}
	}

	kbds_moveto(n.x, n.y);
	kbds_quant = 0;
}

int
kbds_drawcursor(void)
{
	if (kbds_in_use &&
	    (kbds_c.y != term.row-1 || !kbds_issearchmode()) &&
	    !(kbds_searchobj.directsearch && kbds_isurlmode()) &&
	    !(kbds_searchobj.directsearch && kbds_isregexmode())) {
		xdrawcursor(kbds_c.x, kbds_c.y, TLINE(kbds_c.y)[kbds_c.x],
		            kbds_oc.x, kbds_oc.y, TLINE(kbds_oc.y));
		kbds_oc = kbds_c;
	}
	return term.scr != 0 || kbds_in_use;
}

int
kbds_getcursor(int *cx, int *cy)
{
	if (!kbds_in_use)
		return 0;

	if (kbds_issearchmode()) {
		*cx = kbds_searchobj.cx + 1;
		*cy = term.row - 1;
	} else {
		*cx = kbds_c.x;
		*cy = kbds_c.y;
	}
	return 1;
}

int
kbds_keyboardhandler(KeySym ksym, char *buf, int len, int forcequit)
{
	int i, q, dy, ox, oy, eol, islast, prevscr, count, charsize;
	int alt = IS_SET(MODE_ALTSCREEN);
	Line line;
	Rune u;

	if (kbds_isurlmode()) {
		switch (ksym) {
		case XK_Escape:
			kbds_searchobj.len = 0;
			kbds_setmode(kbds_mode & ~KBDS_MODE_URL);
			clear_url_cache();
			kbds_clearhighlights();
			/* If the direct search is aborted, we just go to the next switch
			 * statement and exit the keyboard selection mode immediately */
			if (kbds_searchobj.directsearch || forcequit)
				break;
			return 0;
		default:
			if (len > 0) {
				utf8decode(buf, &u, len);
				if ((is_in_flash_used_label(u) == 1 && hit_input_first == 0) || (is_in_flash_used_double_label(u) == 1 && hit_input_first == 1)) {
					jump_to_label(u);
					if (hit_input_first == 1) {
						kbds_clearhighlights();
						return 0;
					}
					kbds_searchobj.len = 0;
					kbds_setmode(kbds_mode & ~KBDS_MODE_URL);
					clear_url_cache();
					if (kbds_searchobj.directsearch) {
						kbds_in_use = kbds_quant = 0;
						free(kbds_searchobj.str);
						return MODE_KBDSELECT;
					}
				}
				return 0;
			}
			break;
		}
	}

	if (kbds_isregexmode()) {
		switch (ksym) {
		case XK_Escape:
			kbds_searchobj.len = 0;
			kbds_setmode(kbds_mode & ~KBDS_MODE_REGEX);
			clear_regex_cache();
			kbds_clearhighlights();
			/* If the direct search is aborted, we just go to the next switch
			 * statement and exit the keyboard selection mode immediately */
			if (kbds_searchobj.directsearch || forcequit)
				break;
			return 0;
		default:
			if (len > 0) {
				utf8decode(buf, &u, len);
				if ((is_in_flash_used_label(u) == 1 && hit_input_first == 0) || (is_in_flash_used_double_label(u) == 1 && hit_input_first == 1)) {
					jump_to_label(u);
					if (hit_input_first == 1) {
						kbds_clearhighlights();
						return 0;
					}
					kbds_searchobj.len = 0;
					kbds_setmode(kbds_mode & ~KBDS_MODE_REGEX);
					clear_regex_cache();
					if (kbds_searchobj.directsearch) {
						kbds_in_use = kbds_quant = 0;
						free(kbds_searchobj.str);
						return MODE_KBDSELECT;
					}
				}
				return 0;
			}
			break;
		}
	}

	if (kbds_isflashmode()) {
		switch (ksym) {
		case XK_Escape:
			kbds_searchobj.len = 0;
			kbds_setmode(kbds_mode & ~KBDS_MODE_FLASH);
			clear_flash_cache();
			kbds_clearhighlights();
			/* If the direct search is aborted, we just go to the next switch
			 * statement and exit the keyboard selection mode immediately */
			if (kbds_searchobj.directsearch || forcequit)
				break;
			return 0;
		case XK_BackSpace:
			if (kbds_searchobj.cx == 0)
				break;
			kbds_clearhighlights();
			kbds_searchobj.cx = MAX(kbds_searchobj.cx-1, 0);
			if (kbds_searchobj.str[kbds_searchobj.cx].mode & ATTR_WDUMMY)
				kbds_searchobj.cx = MAX(kbds_searchobj.cx-1, 0);
			if (ksym == XK_BackSpace)
				kbds_deletechar();
			for (kbds_searchobj.ignorecase = 1, i = 0; i < kbds_searchobj.len; i++) {
				if (kbds_searchobj.str[i].u != towlower(kbds_searchobj.str[i].u)) {
					kbds_searchobj.ignorecase = 0;
					break;
				}
			}
			kbds_searchobj.wordonly = 0;
			count = kbds_searchall();
			return 0;
		default:
			if (len > 0) {
				utf8decode(buf, &u, len);
				if (is_in_flash_used_label(u) == 1) {
					jump_to_label(u);
					kbds_searchobj.len = 0;
					kbds_setmode(kbds_mode & ~KBDS_MODE_FLASH);
					clear_flash_cache();
					kbds_selecttext();
					return 0;
				} else if (kbds_searchobj.len > 0 && is_in_flash_next_char_record(u) == 0) {
					return 0;
				} else {
					clear_flash_cache();
				}
				kbds_clearhighlights();
				kbds_insertchar(u);
				for (kbds_searchobj.ignorecase = 1, i = 0; i < kbds_searchobj.len; i++) {
					if (kbds_searchobj.str[i].u != towlower(kbds_searchobj.str[i].u)) {
						kbds_searchobj.ignorecase = 0;
						break;
					}
				}
				kbds_searchobj.wordonly = 0;
				count = kbds_searchall();
				return 0;
			}
			break;
		}
	}

	if (kbds_issearchmode() && !forcequit) {
		switch (ksym) {
		case XK_Escape:
			kbds_searchobj.len = 0;
			/* FALLTHROUGH */
		case XK_Return:
			/* smart case */
			for (kbds_searchobj.ignorecase = 1, i = 0; i < kbds_searchobj.len; i++) {
				if (kbds_searchobj.str[i].u != towlower(kbds_searchobj.str[i].u)) {
					kbds_searchobj.ignorecase = 0;
					break;
				}
			}
			kbds_searchobj.wordonly = 0;
			count = kbds_searchall();
			kbds_searchnext(kbds_searchobj.dir);
			kbds_selecttext();
			kbds_setmode(kbds_mode & ~KBDS_MODE_SEARCH);
			if (count == 0 && kbds_searchobj.directsearch)
				ksym = XK_Escape;
			break;
		case XK_Delete:
		case XK_KP_Delete:
			kbds_deletechar();
			break;
		case XK_BackSpace:
			if (kbds_searchobj.cx == 0)
				break;
			/* FALLTHROUGH */
		case XK_Left:
		case XK_KP_Left:
			kbds_searchobj.cx = MAX(kbds_searchobj.cx-1, 0);
			if (kbds_searchobj.str[kbds_searchobj.cx].mode & ATTR_WDUMMY)
				kbds_searchobj.cx = MAX(kbds_searchobj.cx-1, 0);
			if (ksym == XK_BackSpace)
				kbds_deletechar();
			break;
		case XK_Right:
		case XK_KP_Right:
			kbds_searchobj.cx = MIN(kbds_searchobj.cx+1, kbds_searchobj.len);
			if (kbds_searchobj.cx < kbds_searchobj.len &&
			    kbds_searchobj.str[kbds_searchobj.cx].mode & ATTR_WDUMMY)
				kbds_searchobj.cx++;
			break;
		case XK_Home:
		case XK_KP_Home:
			kbds_searchobj.cx = 0;
			break;
		case XK_End:
		case XK_KP_End:
			kbds_searchobj.cx = kbds_searchobj.len;
			break;
		default:
			for (i = 0; i < len; i += charsize) {
				charsize = utf8decode(buf + i, &u, len - i);
				if (charsize == 0)
					break;
				kbds_insertchar(u);
			}
			break;
		}
		/* If the direct search is aborted, we just go to the next switch
		 * statement and exit the keyboard selection mode immediately */
		if (!(ksym == XK_Escape && kbds_searchobj.directsearch)) {
			term.dirty[term.row-1] = 1;
			return 0;
		}
	} else if ((kbds_mode & KBDS_MODE_FIND) && !forcequit) {
		kbds_findchar = 0;
		switch (ksym) {
		case XK_Escape:
		case XK_Return:
			kbds_quant = 0;
			break;
		default:
			if (len < 1)
				return 0;
			utf8decode(buf, &kbds_findchar, len);
			kbds_findnext(kbds_finddir, 0);
			kbds_selecttext();
			break;
		}
		kbds_setmode(kbds_mode & ~KBDS_MODE_FIND);
		return 0;
	}

	switch (ksym) {
	case XK_ACTIVATE:
		kbds_searchobj.str = xmalloc(term.col * sizeof(Glyph));
		kbds_searchobj.cx = kbds_searchobj.len = 0;
		kbds_scrolldownonexit = 0;
		kbds_in_use = 1;
		kbds_moveto(term.c.x, term.c.y);
		kbds_oc = kbds_c;
		kbds_setmode(KBDS_MODE_MOVE);
		return MODE_KBDSELECT;
	case XK_V:
		if (kbds_mode & KBDS_MODE_LSELECT) {
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		} else if (kbds_mode & KBDS_MODE_SELECT) {
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
			sel.ob.x = 0;
			tfulldirt();
			kbds_setmode((kbds_mode ^ KBDS_MODE_SELECT) | KBDS_MODE_LSELECT);
		} else {
			selstart(0, kbds_c.y, 0);
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
			kbds_setmode(kbds_mode | KBDS_MODE_LSELECT);
		}
		break;
	case XK_v:
		if (kbds_mode & KBDS_MODE_SELECT) {
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		} else if (kbds_mode & KBDS_MODE_LSELECT) {
			selextend(kbds_c.x, kbds_c.y, kbds_seltype, 0);
			kbds_setmode((kbds_mode ^ KBDS_MODE_LSELECT) | KBDS_MODE_SELECT);
		} else {
			selstart(kbds_c.x, kbds_c.y, 0);
			kbds_setmode(kbds_mode | KBDS_MODE_SELECT);
		}
		break;
	case XK_S:
		if (!(kbds_mode & KBDS_MODE_LSELECT)) {
			kbds_seltype ^= (SEL_REGULAR | SEL_RECTANGULAR);
			selextend(kbds_c.x, kbds_c.y, kbds_seltype, 0);
		}
		break;
	case XK_o:
	case XK_O:
		ox = sel.ob.x; oy = sel.ob.y;
		if (kbds_mode & KBDS_MODE_SELECT) {
			if (kbds_seltype == SEL_RECTANGULAR && ksym == XK_O) {
				selstart(kbds_c.x, oy, 0);
				kbds_moveto(ox, kbds_c.y);
			} else {
				selstart(kbds_c.x, kbds_c.y, 0);
				kbds_moveto(ox, oy);
			}
		} else if (kbds_mode & KBDS_MODE_LSELECT) {
			selstart(0, kbds_c.y, 0);
			selextend(term.col-1, kbds_c.y, SEL_RECTANGULAR, 0);
			kbds_moveto(kbds_c.x, oy);
		}
		break;
	case XK_y:
	case XK_Y:
		if (kbds_isselectmode()) {
			kbds_copytoclipboard();
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		}
		break;
	case XK_SEARCHFW:
	case XK_SEARCHBW:
	case XK_slash:
	case XK_KP_Divide:
	case XK_question:
		kbds_searchobj.directsearch = (ksym == XK_SEARCHFW || ksym == XK_SEARCHBW);
		kbds_searchobj.dir = (ksym == XK_question || ksym == XK_SEARCHBW) ? -1 : 1;
		kbds_searchobj.cx = kbds_searchobj.len = 0;
		kbds_searchobj.maxlen = term.col - 2;
		if (kbds_searchobj.directsearch && term.scr == 0)
			kbds_scrolldownonexit = 1;
		kbds_setmode(kbds_mode | KBDS_MODE_SEARCH);
		kbds_clearhighlights();
		return 0;
	case XK_FLASH:
	case XK_s:
		kbds_searchobj.directsearch = (ksym == XK_FLASH);
		kbds_searchobj.dir = 1;
		kbds_searchobj.cx = kbds_searchobj.len = 0;
		kbds_searchobj.maxlen = term.col - 2;
		kbds_quant = 0;
		kbds_setmode(kbds_mode | KBDS_MODE_FLASH);
		kbds_clearhighlights();
		return 0;
	case XK_REGEX:
	case XK_p:
		kbds_searchobj.directsearch = (ksym == XK_REGEX);
		kbds_searchobj.dir = 1;
		kbds_quant = 0;
		kbds_setmode(kbds_mode | KBDS_MODE_REGEX);
		kbds_clearhighlights();
		kbds_search_regex();
		if (kbds_searchobj.directsearch)
			clearurl(1);
		return 0;
	case XK_URL:
		kbds_searchobj.directsearch = (ksym == XK_URL);
		kbds_searchobj.dir = 1;
		kbds_quant = 0;
		kbds_setmode(kbds_mode | KBDS_MODE_URL);
		kbds_clearhighlights();
		kbds_search_url();
		if (kbds_searchobj.directsearch)
			clearurl(1);
		return 0;
	case XK_q:
	case XK_Escape:
		if (!kbds_in_use)
			return 0;
		if (kbds_quant && !forcequit) {
			kbds_quant = 0;
			break;
		}
		selclear();
		if (kbds_isselectmode() && !forcequit) {
			kbds_setmode(KBDS_MODE_MOVE);
			break;
		}
		kbds_setmode(KBDS_MODE_MOVE);
		/* FALLTHROUGH */
	case XK_Return:
		if (kbds_isselectmode())
			kbds_copytoclipboard();
		kbds_in_use = kbds_quant = 0;
		free(kbds_searchobj.str);
		kbds_clearhighlights();
		if (kbds_scrolldownonexit)
			kscrolldown(&((Arg){ .i = term.histf }));
		return MODE_KBDSELECT;
	case XK_n:
	case XK_N:
		kbds_searchnext(ksym == XK_n ? kbds_searchobj.dir : -kbds_searchobj.dir);
		break;
	case XK_asterisk:
	case XK_KP_Multiply:
	case XK_numbersign:
		kbds_searchwordorselection(ksym == XK_numbersign ? -1 : 1);
		break;
	case XK_BackSpace:
		kbds_moveto(0, kbds_c.y);
		break;
	case XK_exclam:
		kbds_moveto(term.col/2, kbds_c.y);
		break;
	case XK_underscore:
		kbds_moveto(term.col-1, kbds_c.y);
		break;
	case XK_dollar:
	case XK_A:
		eol = kbds_c.len-1;
		line = kbds_c.line;
		islast = (kbds_c.x == eol || (kbds_c.x == eol-1 && (line[eol-1].mode & ATTR_WIDE)));
		if (islast && kbds_iswrapped(&kbds_c) && kbds_c.y < kbds_bot())
			kbds_moveto(tlinelen(TLINE(kbds_c.y+1))-1, kbds_c.y+1);
		else
			kbds_moveto(islast ? term.col-1 : eol, kbds_c.y);
		break;
	case XK_asciicircum:
	case XK_I:
		for (i = 0; i < kbds_c.len && kbds_c.line[i].u == ' '; i++)
			;
		kbds_moveto((i < kbds_c.len) ? i : 0, kbds_c.y);
		break;
	case XK_End:
	case XK_KP_End:
		kbds_moveto(kbds_c.x, term.row-1);
		break;
	case XK_Home:
	case XK_KP_Home:
	case XK_H:
		kbds_moveto(kbds_c.x, 0);
		break;
	case XK_M:
		kbds_moveto(kbds_c.x, alt ? (term.row-1) / 2
		                          : MIN(term.c.y + term.scr, term.row-1) / 2);
		break;
	case XK_L:
		kbds_moveto(kbds_c.x, alt ? term.row-1
		                          : MIN(term.c.y + term.scr, term.row-1));
		break;
	case XK_Page_Up:
	case XK_KP_Page_Up:
	case XK_K:
		prevscr = term.scr;
		kscrollup(&((Arg){ .i = term.row }));
		kbds_moveto(kbds_c.x, alt ? 0
		                          : MAX(0, kbds_c.y - term.row + term.scr - prevscr));
		break;
	case XK_Page_Down:
	case XK_KP_Page_Down:
	case XK_J:
		prevscr = term.scr;
		kscrolldown(&((Arg){ .i = term.row }));
		kbds_moveto(kbds_c.x, alt ? term.row-1
		                          : MIN(MIN(term.c.y + term.scr, term.row-1),
		                                    kbds_c.y + term.row + term.scr - prevscr));
		break;
	case XK_g:
		kscrollup(&((Arg){ .i = term.histf }));
		kbds_moveto(kbds_c.x, 0);
		break;
	case XK_G:
		kscrolldown(&((Arg){ .i = term.histf }));
		kbds_moveto(kbds_c.x, alt ? term.row-1 : term.c.y);
		break;
	case XK_b:
	case XK_B:
		kbds_nextword(1, -1, (ksym == XK_b) ? kbds_sdelim : kbds_ldelim);
		break;
	case XK_w:
	case XK_W:
		kbds_nextword(1, +1, (ksym == XK_w) ? kbds_sdelim : kbds_ldelim);
		break;
	case XK_e:
	case XK_E:
		kbds_nextword(0, +1, (ksym == XK_e) ? kbds_sdelim : kbds_ldelim);
		break;
	case XK_z:
		prevscr = term.scr;
		dy = kbds_c.y - (term.row-1) / 2;
		if (dy <= 0)
			kscrollup(&((Arg){ .i = -dy }));
		else
			kscrolldown(&((Arg){ .i = dy }));
		kbds_moveto(kbds_c.x, kbds_c.y + term.scr - prevscr);
		break;
	case XK_f:
	case XK_F:
	case XK_t:
	case XK_T:
		kbds_finddir = (ksym == XK_f || ksym == XK_t) ? 1 : -1;
		kbds_findtill = (ksym == XK_t || ksym == XK_T) ? 1 : 0;
		kbds_setmode(kbds_mode | KBDS_MODE_FIND);
		return 0;
	case XK_semicolon:
	case XK_r:
		kbds_findnext(kbds_finddir, 1);
		break;
	case XK_comma:
	case XK_R:
		kbds_findnext(-kbds_finddir, 1);
		break;
	case XK_Z:
		kbds_jumptoprompt(-1);
		break;
	case XK_X:
		kbds_jumptoprompt(1);
		break;
	case XK_u:
		openUrlOnClick(kbds_c.x, kbds_c.y, url_opener);
		break;
	case XK_U:
		copyUrlOnClick(kbds_c.x, kbds_c.y);
		break;
	case XK_0:
	case XK_KP_0:
		if (!kbds_quant) {
			kbds_moveto(0, kbds_c.y);
			break;
		}
		/* FALLTHROUGH */
	default:
		if (ksym >= XK_0 && ksym <= XK_9) {                 /* 0-9 keyboard */
			q = (kbds_quant * 10) + (ksym ^ XK_0);
			kbds_quant = q <= 99999999 ? q : kbds_quant;
			term.dirty[0] = 1;
			return 0;
		} else if (ksym >= XK_KP_0 && ksym <= XK_KP_9) {    /* 0-9 numpad */
			q = (kbds_quant * 10) + (ksym ^ XK_KP_0);
			kbds_quant = q <= 99999999 ? q : kbds_quant;
			term.dirty[0] = 1;
			return 0;
		} else if (ksym == XK_k || ksym == XK_h)
			i = ksym & 1;
		else if (ksym == XK_l || ksym == XK_j)
			i = ((ksym & 6) | 4) >> 1;
		else if (ksym >= XK_KP_Left && ksym <= XK_KP_Down)
			i = ksym - XK_KP_Left;
		else if ((XK_Home & ksym) != XK_Home || (i = (ksym ^ XK_Home) - 1) > 3)
			return 0;

		kbds_quant = (kbds_quant ? kbds_quant : 1);

		if (i & 1) {
			kbds_c.y += kbds_quant * (i & 2 ? 1 : -1);
		} else {
			for (;kbds_quant > 0; kbds_quant--) {
				if (!kbds_moveforward(&kbds_c, (i & 2) ? 1 : -1,
					    KBDS_WRAP_LINE | KBDS_WRAP_EDGE))
					break;
			}
		}
		kbds_moveto(kbds_c.x, kbds_c.y);
	}
	kbds_selecttext();
	kbds_quant = 0;
	term.dirty[0] = 1;
	return 0;
}
