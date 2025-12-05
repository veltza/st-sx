/* See LICENSE for license details. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <X11/keysym.h>
#include <X11/X.h>

#include "st.h"
#include "win.h"
#include "sixel.h"

#if   defined(__linux)
 #include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
 #include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
 #include <libutil.h>
#endif

/* Arbitrary sizes */
#define UTF_INVALID   0xFFFD
#define UTF_SIZ       4
#define ESC_BUF_SIZ   (128*UTF_SIZ)
#define ESC_ARG_SIZ   16
#define SUB_ARG_SIZ   5
#define STR_BUF_SIZ   ESC_BUF_SIZ
#define STR_ARG_SIZ   40
#define STR_TERM_ST   "\033\\"
#define STR_TERM_BEL  "\007"

/* macros */
#define IS_SET(flag)    ((term.mode & (flag)) != 0)
#define ISCONTROLC0(c)  (BETWEEN(c, 0, 0x1f) || (c) == 0x7f)
#define ISCONTROLC1(c)  (BETWEEN(c, 0x80, 0x9f))
#define ISCONTROL(c)    (ISCONTROLC0(c) || ISCONTROLC1(c))
#define ISDELIM(u)      (u && wcschr(worddelimiters, u))

enum term_mode {
	MODE_WRAP         = 1 << 0,
	MODE_INSERT       = 1 << 1,
	MODE_ALTSCREEN    = 1 << 2,
	MODE_CRLF         = 1 << 3,
	MODE_ECHO         = 1 << 4,
	MODE_PRINT        = 1 << 5,
	MODE_UTF8         = 1 << 6,
	MODE_SIXEL        = 1 << 7,
	MODE_SIXEL_CUR_RT = 1 << 8,
	MODE_SIXEL_SDM    = 1 << 9,
	MODE_SIXEL_PRIVATE_PALETTE = 1 << 10,
	MODE_RESIZE_NOTIFICATIONS  = 1 << 11
};

enum scroll_mode {
	SCROLL_RESIZE = -1,
	SCROLL_NOSAVEHIST = 0,
	SCROLL_SAVEHIST = 1
};

enum cursor_movement {
	CURSOR_SAVE,
	CURSOR_LOAD
};

enum cursor_state {
	CURSOR_DEFAULT  = 0,
	CURSOR_WRAPNEXT = 1,
	CURSOR_ORIGIN   = 2,
	CURSOR_PROMPT2  = 4
};

enum charset {
	CS_GRAPHIC0,
	CS_GRAPHIC1,
	CS_UK,
	CS_USA,
	CS_MULTI,
	CS_GER,
	CS_FIN
};

enum escape_state {
	ESC_START      = 1,
	ESC_CSI        = 2,
	ESC_STR        = 4,  /* DCS, OSC, PM, APC */
	ESC_ALTCHARSET = 8,
	ESC_STR_END    = 16, /* a final string was encountered */
	ESC_TEST       = 32, /* Enter in test mode */
	ESC_UTF8       = 64,
	ESC_DCS        =128,
};

typedef struct {
	int mode;
	int type;
	int snap;
	/*
	 * Selection variables:
	 * nb – normalized coordinates of the beginning of the selection
	 * ne – normalized coordinates of the end of the selection
	 * ob – original coordinates of the beginning of the selection
	 * oe – original coordinates of the end of the selection
	 */
	struct {
		int x, y;
	} nb, ne, ob, oe;

	int alt;
} Selection;

typedef struct {
	int count;
	int value[SUB_ARG_SIZ];
} Subarg;

/* CSI Escape sequence structs */
/* ESC '[' [[ [<priv>] <arg> [;]] <mode> [<mode>]] */
typedef struct {
	char buf[ESC_BUF_SIZ];      /* raw string */
	size_t len;                 /* raw string length */
	int arg[ESC_ARG_SIZ];
	int narg;                   /* nb of args */
	Subarg subarg[ESC_ARG_SIZ]; /* colon-separated subarguments */
	char mode[2];
	char priv;
} CSIEscape;

/* STR Escape sequence structs */
/* ESC type [[ [<priv>] <arg> [;]] <mode>] ESC '\' */
typedef struct {
	char type;             /* ESC type ... */
	char *buf;             /* allocated raw string */
	size_t siz;            /* allocation size */
	size_t len;            /* raw string length */
	char *args[STR_ARG_SIZ];
	int narg;              /* nb of args */
	char *term;            /* terminator: ST or BEL */
} STREscape;

static void execsh(char *, char **);
static void stty(char **);
static void sigchld(int);
static void ttywriteraw(const char *, size_t);

static void csidump(void);
static void csihandle(void);
static void dcshandle(void);
static void initsixel(void);
static void createsixel(void);
static inline void readsubargs(char **, int);
static void csiparse(void);
static inline void csireset(void);
static void osc_color_response(int, int, int);
static void write_da(void);
static int eschandle(uchar);
static void strdump(void);
static void strhandle(void);
static void strparse(void);
static void strreset(void);

static void tprinter(char *, size_t);
static void tdumpsel(void);
static void tdumpline(int);
static void tdump(void);
static void tclearregion(int, int, int, int, int);
static void tcursor(int);
static inline void tclearglyph(Glyph *, int);
static void tresetcursor(void);
static void tdeletechar(int);
static void tdeleteimages(void);
static void tdeleteline(int);
static void tinsertblank(int);
static void tinsertblankline(int);
static int tlinelen(Line len);
static int tiswrapped(Line line);
static char *tgetglyphs(char *, const Glyph *, const Glyph *);
static size_t tgetline(char *, const Glyph *);
static void tmoveto(int, int);
static void tmoveato(int, int);
static void tnewline(int);
static void tputtab(int);
static void tputc(Rune);
static void treset(void);
static void tscrollup(int, int, int, int);
static void tscrolldown(int, int);
static void treflow(int, int);
static void rscrolldown(int);
static void tresizedef(int, int);
static void tresizealt(int, int);
static void tsetattr(const int *, int);
static void tsetchar(Rune, const Glyph *, int, int);
static void tsetdirt(int, int);
static void tsetscroll(int, int);
static inline void tsetsixelattr(Line line, int x1, int x2);
static void tswapscreen(void);
static void tloaddefscreen(int, int);
static void tloadaltscreen(int, int);
static void tsetmode(int, int, const int *, int);
static int twrite(const char *, int, int);
static void tcontrolcode(uchar );
static void tdectest(char );
static void tdefutf8(char);
static int32_t tdefcolor(const int *, int *, int);
static void tdeftran(char);
static void tstrsequence(uchar);
static void selnormalize(void);
static void selscroll(int, int, int);
static void selmove(int);
static void selremove(void);
static inline int regionselected(int, int, int, int);
static void selsnap(int *, int *, int);
static void sendresizenotification(void);

static inline char utf8encodebyte(Rune, size_t);
static inline size_t utf8validate(Rune *, size_t);

static char *base64dec(const char *);
static char base64dec_getc(const char **);

static ssize_t xwrite(int, const char *, size_t);

/* Globals */
static Selection sel;
static CSIEscape csiescseq;
static STREscape strescseq;
static int iofd = 1;
static int cmdfd;
static int csdfd;
static pid_t pid;
sixel_state_t sixel_st;

static const uchar utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
static const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const Rune utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
static const Rune utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static int su;
static int twrite_aborted;
struct timespec sutv;

#include "patch/st_include.h"

static void
tsync_begin(void)
{
	clock_gettime(CLOCK_MONOTONIC, &sutv);
	su = 1;
}

static void
tsync_end(void)
{
	su = 0;
}

int
tinsync(uint timeout)
{
	struct timespec now;
	if (su && !clock_gettime(CLOCK_MONOTONIC, &now)
	       && TIMEDIFF(now, sutv) >= timeout)
		su = 0;
	return su;
}

ssize_t
xwrite(int fd, const char *s, size_t len)
{
	size_t aux = len;
	ssize_t r;

	while (len > 0) {
		r = write(fd, s, len);
		if (r < 0)
			return r;
		len -= r;
		s += r;
	}

	return aux;
}

void *
xmalloc(size_t len)
{
	void *p;

	if (!(p = malloc(len)))
		die("malloc: %s\n", strerror(errno));

	return p;
}

void *
xrealloc(void *p, size_t len)
{
	if ((p = realloc(p, len)) == NULL)
		die("realloc: %s\n", strerror(errno));

	return p;
}

char *
xstrdup(const char *s)
{
	char *p;
	if ((p = strdup(s)) == NULL)
		die("strdup: %s\n", strerror(errno));

	return p;
}

size_t
utf8decode(const char *c, Rune *u, size_t clen)
{
	static uchar utflen[] = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
	};
	size_t i, len;
	Rune udecoded;

	*u = UTF_INVALID;
	if (!clen)
		return 0;

	udecoded = c[0] & 0xff;
	len = utflen[udecoded >> 3];
	if (len <= 1) {
		*u = len ? udecoded : UTF_INVALID;
		return 1;
	}

	udecoded &= ~utfmask[len];
	clen = MIN(clen, len);
	for (i = 1; i < clen; ++i) {
		if ((c[i] & 0xC0) != 0x80)
			return i;
		udecoded = (udecoded << 6) | (c[i] & 0x3F);
	}
	if (i < len)
		return 0;

	*u = (!BETWEEN(udecoded, utfmin[len], utfmax[len]) || BETWEEN(udecoded, 0xD800, 0xDFFF))
	        ? UTF_INVALID : udecoded;
	return len;
}

size_t
utf8encode(Rune u, char *c)
{
	size_t len, i;

	len = utf8validate(&u, 0);
	if (len > UTF_SIZ)
		return 0;

	for (i = len - 1; i != 0; --i) {
		c[i] = utf8encodebyte(u, 0);
		u >>= 6;
	}
	c[0] = utf8encodebyte(u, len);

	return len;
}

char
utf8encodebyte(Rune u, size_t i)
{
	return utfbyte[i] | (u & ~utfmask[i]);
}

size_t
utf8validate(Rune *u, size_t i)
{
	if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
		*u = UTF_INVALID;
	for (i = 1; *u > utfmax[i]; ++i)
		;

	return i;
}

char
base64dec_getc(const char **src)
{
	while (**src && !isprint((unsigned char)**src))
		(*src)++;
	return **src ? *((*src)++) : '=';  /* emulate padding if string ends */
}

char *
base64dec(const char *src)
{
	size_t in_len = strlen(src);
	char *result, *dst;
	static const char base64_digits[256] = {
		[43] = 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
		0, 0, 0, -1, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0,
		0, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
	};

	if (in_len % 4)
		in_len += 4 - (in_len % 4);
	result = dst = xmalloc(in_len / 4 * 3 + 1);
	while (*src) {
		int a = base64_digits[(unsigned char) base64dec_getc(&src)];
		int b = base64_digits[(unsigned char) base64dec_getc(&src)];
		int c = base64_digits[(unsigned char) base64dec_getc(&src)];
		int d = base64_digits[(unsigned char) base64dec_getc(&src)];

		/* invalid input. 'a' can be -1, e.g. if src is "\n" (c-str) */
		if (a == -1 || b == -1)
			break;

		*dst++ = (a << 2) | ((b & 0x30) >> 4);
		if (c == -1)
			break;
		*dst++ = ((b & 0x0f) << 4) | ((c & 0x3c) >> 2);
		if (d == -1)
			break;
		*dst++ = ((c & 0x03) << 6) | d;
	}
	*dst = '\0';
	return result;
}

void
selinit(void)
{
	sel.mode = SEL_IDLE;
	sel.snap = 0;
	sel.ob.x = -1;
}

int
tlinelen(Line line)
{
	int i = term.col - 1;

	/* We are using a different algorithm on the alt screen because an
	 * application might use spaces to clear the screen and in that case it is
	 * impossible to find the end of the line when every cell has the ATTR_SET
	 * attribute. The second algorithm is more accurate on the main screen and
	 * and we can use it there. */
	if (IS_SET(MODE_ALTSCREEN))
		for (; i >= 0 && !(line[i].mode & ATTR_WRAP) && line[i].u == ' '; i--);
	else
		for (; i >= 0 && !(line[i].mode & (ATTR_SET | ATTR_WRAP)); i--);

	return i + 1;
}

int
tiswrapped(Line line)
{
	int len = tlinelen(line);

	return len > 0 && (line[len - 1].mode & ATTR_WRAP);
}

char *
tgetglyphs(char *buf, const Glyph *gp, const Glyph *lgp)
{
	while (gp <= lgp)
		if (gp->mode & ATTR_WDUMMY) {
			gp++;
		} else {
			buf += utf8encode((gp++)->u, buf);
		}
	return buf;
}

size_t
tgetline(char *buf, const Glyph *fgp)
{
	char *ptr;
	const Glyph *lgp = &fgp[term.col - 1];

	while (lgp > fgp && !(lgp->mode & (ATTR_SET | ATTR_WRAP)))
		lgp--;
	ptr = tgetglyphs(buf, fgp, lgp);
	if (!(lgp->mode & ATTR_WRAP))
		*(ptr++) = '\n';
	return ptr - buf;
}

void
selstart(int col, int row, int snap)
{
	selclear();
	sel.mode = SEL_EMPTY;
	sel.type = SEL_REGULAR;
	sel.alt = IS_SET(MODE_ALTSCREEN);
	sel.snap = snap;
	sel.oe.x = sel.ob.x = col;
	sel.oe.y = sel.ob.y = row;
	selnormalize();

	if (sel.snap != 0)
		sel.mode = SEL_READY;
	tsetdirt(sel.nb.y, sel.ne.y);
}

void
selextend(int col, int row, int type, int done)
{
	int oldey, oldex, oldsby, oldsey, oldtype;

	if (sel.mode == SEL_IDLE)
		return;
	if (done && sel.mode == SEL_EMPTY) {
		selclear();
		return;
	}

	oldey = sel.oe.y;
	oldex = sel.oe.x;
	oldsby = sel.nb.y;
	oldsey = sel.ne.y;
	oldtype = sel.type;

	sel.oe.x = col;
	sel.oe.y = row;
	sel.type = type;
	selnormalize();

	if (oldey != sel.oe.y || oldex != sel.oe.x ||
	    oldtype != sel.type || sel.mode == SEL_EMPTY)
		tsetdirt(MIN(sel.nb.y, oldsby), MAX(sel.ne.y, oldsey));

	sel.mode = done ? SEL_IDLE : SEL_READY;
}

void
selnormalize(void)
{
	int i;

	if (sel.type == SEL_REGULAR && sel.ob.y != sel.oe.y) {
		sel.nb.x = sel.ob.y < sel.oe.y ? sel.ob.x : sel.oe.x;
		sel.ne.x = sel.ob.y < sel.oe.y ? sel.oe.x : sel.ob.x;
	} else {
		sel.nb.x = MIN(sel.ob.x, sel.oe.x);
		sel.ne.x = MAX(sel.ob.x, sel.oe.x);
	}
	sel.nb.y = MIN(sel.ob.y, sel.oe.y);
	sel.ne.y = MAX(sel.ob.y, sel.oe.y);

	selsnap(&sel.nb.x, &sel.nb.y, -1);
	selsnap(&sel.ne.x, &sel.ne.y, +1);

	/* expand selection over line breaks */
	if (sel.type == SEL_RECTANGULAR)
		return;

	i = tlinelen(TLINE(sel.nb.y));
	if (sel.nb.x > i)
		sel.nb.x = i;
	if (sel.ne.x >= tlinelen(TLINE(sel.ne.y)))
		sel.ne.x = term.col - 1;
}

int
regionselected(int x1, int y1, int x2, int y2)
{
	if (sel.ob.x == -1 || sel.mode == SEL_EMPTY ||
	    sel.alt != IS_SET(MODE_ALTSCREEN) || sel.nb.y > y2 || sel.ne.y < y1)
		return 0;

	return (sel.type == SEL_RECTANGULAR) ? sel.nb.x <= x2 && sel.ne.x >= x1
		: (sel.nb.y != y2 || sel.nb.x <= x2) &&
		  (sel.ne.y != y1 || sel.ne.x >= x1);
}

int
selected(int x, int y)
{
	return regionselected(x, y, x, y);
}

void
selsnap(int *x, int *y, int direction)
{
	int newx, newy;
	int rtop = 0, rbot = term.row - 1;
	int delim, prevdelim, maxlen;
	const Glyph *gp, *prevgp;

	if (!IS_SET(MODE_ALTSCREEN))
		rtop += -term.histf + term.scr, rbot += term.scr;

	switch (sel.snap) {
	case SNAP_WORD:
		/*
		 * Snap around if the word wraps around at the end or
		 * beginning of a line.
		 */
		maxlen = (TLINE(*y)[term.col-2].mode & ATTR_WRAP) ? term.col-1 : term.col;
		LIMIT(*x, 0, maxlen - 1);
		prevgp = &TLINE(*y)[*x];
		prevdelim = ISDELIM(prevgp->u);
		for (;;) {
			newx = *x + direction;
			newy = *y;
			if (!BETWEEN(newx, 0, maxlen - 1)) {
				newy += direction;
				if (!BETWEEN(newy, rtop, rbot))
					break;

				if (!tiswrapped(TLINE(direction > 0 ? *y : newy)))
					break;

				maxlen = (TLINE(newy)[term.col-2].mode & ATTR_WRAP) ? term.col-1 : term.col;
				newx = direction > 0 ? 0 : maxlen - 1;
			}

			gp = &TLINE(newy)[newx];
			delim = ISDELIM(gp->u);
			if (!(gp->mode & ATTR_WDUMMY) && (delim != prevdelim
					|| (delim && gp->u != prevgp->u)))
				break;

			*x = newx;
			*y = newy;
			if (!(gp->mode & ATTR_WDUMMY)) {
				prevgp = gp;
				prevdelim = delim;
			}
		}
		break;
	case SNAP_LINE:
		/*
		 * Snap around if the the previous line or the current one
		 * has set ATTR_WRAP at its end. Then the whole next or
		 * previous line will be selected.
		 */
		*x = (direction < 0) ? 0 : term.col - 1;
		if (direction < 0) {
			for (; *y > rtop; *y -= 1) {
				if (!tiswrapped(TLINE(*y-1)))
					break;
			}
		} else if (direction > 0) {
			for (; *y < rbot; *y += 1) {
				if (!tiswrapped(TLINE(*y)))
					break;
			}
		}
		break;
	}
}

char *
getsel(void)
{
	char *str, *ptr;
	int y, lastx, linelen;
	const Glyph *gp, *lgp;

	if (sel.ob.x == -1 || sel.alt != IS_SET(MODE_ALTSCREEN))
		return NULL;

	str = xmalloc((term.col + 1) * (sel.ne.y - sel.nb.y + 1) * UTF_SIZ);
	ptr = str;

	/* append every set & selected glyph to the selection */
	for (y = sel.nb.y; y <= sel.ne.y; y++) {
		Line line = TLINE(y);

		if ((linelen = tlinelen(line)) == 0) {
			*ptr++ = '\n';
			continue;
		}

		if (sel.type == SEL_RECTANGULAR) {
			gp = &line[sel.nb.x];
			lastx = sel.ne.x;
		} else {
			gp = &line[sel.nb.y == y ? sel.nb.x : 0];
			lastx = (sel.ne.y == y) ? sel.ne.x : term.col-1;
		}
		lgp = &line[MIN(lastx, linelen-1)];

		ptr = tgetglyphs(ptr, gp, lgp);
		/*
		 * Copy and pasting of line endings is inconsistent
		 * in the inconsistent terminal and GUI world.
		 * The best solution seems like to produce '\n' when
		 * something is copied from st and convert '\n' to
		 * '\r', when something to be pasted is received by
		 * st.
		 * FIXME: Fix the computer world.
		 */
		if ((y < sel.ne.y || lastx >= linelen) &&
		    (!(lgp->mode & ATTR_WRAP) || sel.type == SEL_RECTANGULAR))
			*ptr++ = '\n';
	}
	*ptr = '\0';
	return str;
}

void
selclear(void)
{
	if (sel.ob.x == -1)
		return;
	selremove();
	tsetdirt(sel.nb.y, sel.ne.y);
}

void
selremove(void)
{
	sel.mode = SEL_IDLE;
	sel.ob.x = -1;
}

void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void
execsh(char *cmd, char **args)
{
	char *sh, *prog, *arg;
	const struct passwd *pw;

	errno = 0;
	if ((pw = getpwuid(getuid())) == NULL) {
		if (errno)
			die("getpwuid: %s\n", strerror(errno));
		else
			die("who are you?\n");
	}

	if ((sh = getenv("SHELL")) == NULL)
		sh = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

	if (args) {
		prog = args[0];
		arg = NULL;
	} else if (scroll) {
		prog = scroll;
		arg = utmp ? utmp : sh;
	} else if (utmp) {
		prog = utmp;
		arg = NULL;
	} else {
		prog = sh;
		arg = NULL;
	}
	DEFAULT(args, ((char *[]) {prog, arg, NULL}));

	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("SHELL", sh, 1);
	setenv("HOME", pw->pw_dir, 1);
	setenv("TERM", termname, 1);
	setenv("COLORTERM", "truecolor", 1);

	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGALRM, SIG_DFL);

	execvp(prog, args);
	_exit(1);
}

void
sigchld(int a)
{
	int stat, exitcode = 0;
	pid_t p;

	while ((p = waitpid(-1, &stat, WNOHANG)) > 0) {
		if (p == pid) {
			if (WIFEXITED(stat) && WEXITSTATUS(stat)) {
				fprintf(stderr, "child exited with status %d\n", WEXITSTATUS(stat));
				exitcode = 1;
			} else if (WIFSIGNALED(stat)) {
				fprintf(stderr, "child terminated due to signal %d\n", WTERMSIG(stat));
				exitcode = 1;
			}
			if (term.hold_at_exit) {
				tsethold(TTYWRITE);
				return;
			}
			close(csdfd);
			_exit(exitcode);
		}
	}
}

void
stty(char **args)
{
	char cmd[_POSIX_ARG_MAX], **p, *q, *s;
	size_t n, siz;

	if ((n = strlen(stty_args)) > sizeof(cmd)-1)
		die("incorrect stty parameters\n");
	memcpy(cmd, stty_args, n);
	q = cmd + n;
	siz = sizeof(cmd) - n;
	for (p = args; p && (s = *p); ++p) {
		if ((n = strlen(s)) > siz-1)
			die("stty parameter length too long\n");
		*q++ = ' ';
		memcpy(q, s, n);
		q += n;
		siz -= n + 1;
	}
	*q = '\0';
	if (system(cmd) != 0)
		perror("Couldn't call stty");
}

int
ttynew(const char *line, char *cmd, const char *out, char **args)
{
	int m, s;
	struct sigaction sa;

	if (out) {
		term.mode |= MODE_PRINT;
		iofd = (!strcmp(out, "-")) ?
			  1 : open(out, O_WRONLY | O_CREAT, 0666);
		if (iofd < 0) {
			fprintf(stderr, "Error opening %s:%s\n",
				out, strerror(errno));
		}
	}

	if (line) {
		if ((cmdfd = open(line, O_RDWR)) < 0)
			die("open line '%s' failed: %s\n",
			    line, strerror(errno));
		dup2(cmdfd, 0);
		stty(args);
		return cmdfd;
	}

	/* seems to work fine on linux, openbsd and freebsd */
	if (openpty(&m, &s, NULL, NULL, NULL) < 0)
		die("openpty failed: %s\n", strerror(errno));

	switch (pid = fork()) {
	case -1:
		die("fork failed: %s\n", strerror(errno));
		break;
	case 0:
		close(iofd);
		close(m);
		setsid(); /* create a new process group */
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		if (ioctl(s, TIOCSCTTY, NULL) < 0)
			die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
		if (s > 2)
			close(s);
#ifdef __OpenBSD__
		if (pledge("stdio getpw proc exec", NULL) == -1)
			die("pledge\n");
#endif
		execsh(cmd, args);
		break;
	default:
#ifdef __OpenBSD__
		if (pledge("stdio rpath tty proc exec inet unix ps", NULL) == -1)
			die("pledge\n");
#endif
		fcntl(m, F_SETFD, FD_CLOEXEC);
		csdfd = s;
		cmdfd = m;
		memset(&sa, 0, sizeof(sa));
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sa.sa_handler = sigchld;
		sigaction(SIGCHLD, &sa, NULL);
		break;
	}
	return cmdfd;
}

int
ttyread_pending(void)
{
	return twrite_aborted && !(term.hold & TTYREAD);
}

size_t
ttyread(void)
{
	static char buf[BUFSIZ];
	static int buflen = 0;
	int ret, written;

	/* append read bytes to unprocessed bytes */
	ret = twrite_aborted ? 1 : read(cmdfd, buf+buflen, LEN(buf)-buflen);

	if (ret <= 0) {
		if (term.hold_at_exit) {
			tsethold(TTYREAD|TTYWRITE);
			return 1;
		}
		if (ret < 0)
			die("couldn't read from shell: %s\n", strerror(errno));
		exit(0);
	}

	buflen += twrite_aborted ? 0 : ret;
	written = twrite(buf, buflen, 0);
	buflen -= written;
	/* keep any incomplete UTF-8 byte sequence for the next call */
	if (buflen > 0)
		memmove(buf, buf + written, buflen);
	return ret;
}

void
ttywrite(const char *s, size_t n, int may_echo)
{
	const char *next;

	kscrolldown(&((Arg){ .i = term.scr }));

	if (term.hold & TTYWRITE)
		return;

	if (may_echo && IS_SET(MODE_ECHO))
		twrite(s, n, 1);

	if (!IS_SET(MODE_CRLF)) {
		ttywriteraw(s, n);
		return;
	}

	/* This is similar to how the kernel handles ONLCR for ttys */
	while (n > 0) {
		if (*s == '\r') {
			next = s + 1;
			ttywriteraw("\r\n", 2);
		} else {
			next = memchr(s, '\r', n);
			DEFAULT(next, s + n);
			ttywriteraw(s, next - s);
		}
		n -= next - s;
		s = next;
	}
}

void
ttywriteraw(const char *s, size_t n)
{
	fd_set wfd, rfd;
	ssize_t r;
	size_t lim = 256;

	/*
	 * Remember that we are using a pty, which might be a modem line.
	 * Writing too much will clog the line. That's why we are doing this
	 * dance.
	 * FIXME: Migrate the world to Plan 9.
	 */
	while (n > 0) {
		FD_ZERO(&wfd);
		FD_ZERO(&rfd);
		FD_SET(cmdfd, &wfd);
		FD_SET(cmdfd, &rfd);

		/* Check if we can write. */
		if (pselect(cmdfd+1, &rfd, &wfd, NULL, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			if (term.hold_at_exit)
				goto write_error;
			die("select failed: %s\n", strerror(errno));
		}
		if (FD_ISSET(cmdfd, &wfd)) {
			/*
			 * Only write the bytes written by ttywrite() or the
			 * default of 256. This seems to be a reasonable value
			 * for a serial line. Bigger values might clog the I/O.
			 */
			if ((r = write(cmdfd, s, (n < lim)? n : lim)) < 0)
				goto write_error;
			if (r < n) {
				/*
				 * We weren't able to write out everything.
				 * This means the buffer is getting full
				 * again. Empty it.
				 */
				if (n < lim)
					lim = ttyread();
				n -= r;
				s += r;
			} else {
				/* All bytes have been written. */
				break;
			}
		}
		if (FD_ISSET(cmdfd, &rfd))
			lim = ttyread();
	}
	return;

write_error:
	if (term.hold_at_exit) {
		tsethold(TTYWRITE);
		return;
	}
	die("write error on tty: %s\n", strerror(errno));
}

void
ttyresize(int tw, int th)
{
	struct winsize w;

	if (term.hold)
		return;

	sendresizenotification();

	w.ws_row = term.row;
	w.ws_col = term.col;
	w.ws_xpixel = tw;
	w.ws_ypixel = th;
	if (ioctl(cmdfd, TIOCSWINSZ, &w) < 0)
		fprintf(stderr, "Couldn't set window size: %s\n", strerror(errno));
}

void
ttyhangup(void)
{
	/* Send SIGHUP to shell */
	kill(pid, SIGHUP);
}

void
sendresizenotification(void)
{
	int n;
	char buf[128];

	if (!IS_SET(MODE_RESIZE_NOTIFICATIONS))
		return;

	n = snprintf(buf, sizeof buf, "\033[48;%d;%d;%d;%dt",
	             term.row, term.col, win.th, win.tw);
	ttywrite(buf, n, 0);
}

int
tattrset(int attr)
{
	int i, j;
	Line line;

	for (i = 0; i < term.row; i++) {
		line = TLINE(i);
		for (j = 0; j < term.col; j++) {
			if (line[j].mode & attr)
				return 1;
		}
	}

	return 0;
}

int
tisaltscr(void)
{
	return IS_SET(MODE_ALTSCREEN);
}

void
tsethold(int state)
{
	term.hold |= state;
	tsync_end();
}

void
tsetdirt(int top, int bot)
{
	int i;

	LIMIT(top, 0, term.row-1);
	LIMIT(bot, 0, term.row-1);

	for (i = top; i <= bot; i++)
		term.dirty[i] = 1;
}

void
tsetdirtattr(int attr)
{
	int i, j;
	Line line;

	for (i = 0; i < term.row; i++) {
		line = TLINE(i);
		for (j = 0; j < term.col; j++) {
			if (line[j].mode & attr) {
				term.dirty[i] = 1;
				break;
			}
		}
	}
}

void
tsetsixelattr(Line line, int x1, int x2)
{
	for (; x1 <= x2; x1++)
		line[x1].extra |= EXT_SIXEL;
}

void
tfulldirt(void)
{
	for (int i = 0; i < term.row; i++)
		term.dirty[i] = 1;
}

void
tcursor(int mode)
{
	static TCursor c[2];
	int alt = IS_SET(MODE_ALTSCREEN);

	if (mode == CURSOR_SAVE) {
		c[alt] = term.c;
	} else if (mode == CURSOR_LOAD) {
		term.c = c[alt];
		tmoveto(c[alt].x, c[alt].y);
		term.c.state = c[alt].state;
	}
}

void
tresetcursor(void)
{
	term.c = (TCursor){ { .mode = ATTR_NULL, .fg = defaultfg, .bg = defaultbg },
	                    .x = 0, .y = 0, .state = CURSOR_DEFAULT };
}

void
treset(void)
{
	uint i;
	int x, y;

	tresetcursor();

	memset(term.tabs, 0, term.col * sizeof(*term.tabs));
	for (i = tabspaces; i < term.col; i += tabspaces)
		term.tabs[i] = 1;
	term.top = 0;
	term.bot = term.row - 1;
	term.mode = MODE_WRAP|MODE_UTF8;
	memset(term.trantbl, CS_USA, sizeof(term.trantbl));
	term.charset = 0;
	selremove();
	for (i = 0; i < 2; i++) {
		tcursor(CURSOR_SAVE); /* reset saved cursor */
		for (y = 0; y < term.row; y++)
			for (x = 0; x < term.col; x++)
				tclearglyph(&term.line[y][x], 0);
		tdeleteimages();
		deletehyperlinks(0);
		tswapscreen();
	}
	tfulldirt();

	for (i = 0; i < term.histsize; i++)
		free(term.hist[i]);
	free(term.hist);
	term.hist = NULL;
	term.histf = 0;
	term.histi = -1;
	term.scr = 0;
	term.histsize = 0;
	increasehistorysize(MIN_HISTSIZE, term.col);

	MODBIT(term.mode, 1, MODE_SIXEL_PRIVATE_PALETTE);
	sixel_parser_set_default_colors(&sixel_st);
}

void
tnew(int col, int row)
{
	int i, j;

	for (i = 0; i < 2; i++) {
		term.line = xmalloc(row * sizeof(Line));
		for (j = 0; j < row; j++)
			term.line[j] = xmalloc(col * sizeof(Glyph));
		term.col = col, term.row = row;
		tswapscreen();
	}
	term.dirty = xmalloc(row * sizeof(*term.dirty));
	term.dirtyimg = xmalloc(row * sizeof(*term.dirtyimg));
	term.tabs = xmalloc(col * sizeof(*term.tabs));
	treset();
}

void
tswapscreen(void)
{
	static Line *altline;
	static int altcol, altrow;
	Line *tmpline = term.line;
	int tmpcol = term.col, tmprow = term.row;
	ImageList *im = term.images;
	Hyperlinks *tmplinks = term.hyperlinks;

	term.line = altline;
	term.col = altcol, term.row = altrow;
	altline = tmpline;
	altcol = tmpcol, altrow = tmprow;
	term.mode ^= MODE_ALTSCREEN;

	term.images = term.images_alt;
	term.images_alt = im;

	term.hyperlinks = term.hyperlinks_alt;
	term.hyperlinks_alt = tmplinks;
}

void
tloaddefscreen(int clear, int loadcursor)
{
	int col, row, alt = IS_SET(MODE_ALTSCREEN);

	restoremousecursor();

	if (alt) {
		if (clear) {
			tclearregion(0, 0, term.col-1, term.row-1, 1);
			tdeleteimages();
			deletehyperlinks(0);
		}
		col = term.col, row = term.row;
		tswapscreen();
	}
	if (loadcursor)
		tcursor(CURSOR_LOAD);
	if (alt)
		tresizedef(col, row);
}

void
tloadaltscreen(int clear, int savecursor)
{
	int col, row, def = !IS_SET(MODE_ALTSCREEN);

	restoremousecursor();

	if (savecursor)
		tcursor(CURSOR_SAVE);
	if (def) {
		col = term.col, row = term.row;
		kscrolldown(&((Arg){ .i = term.scr }));
		tswapscreen();
		tresizealt(col, row);
	}
	if (clear) {
		tclearregion(0, 0, term.col-1, term.row-1, 1);
		tdeleteimages();
		deletehyperlinks(0);
	}
}

void
tscrolldown(int top, int n)
{
	restoremousecursor();

	int i, bot = term.bot;
	int scr = IS_SET(MODE_ALTSCREEN) ? 0 : term.scr;
	int itop = top + scr, ibot = bot + scr;
	Line temp;
	ImageList *im, *next;

	if (n <= 0)
		return;
	n = MIN(n, bot-top+1);

	tsetdirt(top + scr, bot + scr);
	tclearregion(0, bot-n+1, term.col-1, bot, 1);

	for (i = bot; i >= top+n; i--) {
		temp = term.line[i];
		term.line[i] = term.line[i-n];
		term.line[i-n] = temp;
	}

	/* move images, if they are inside the scrolling region */
	for (im = term.images; im; im = next) {
		next = im->next;
		if (im->y >= itop && im->y <= ibot) {
			im->y += n;
			if (im->y > ibot)
				delete_image(im);
		}
	}

	if (sel.ob.x != -1 && sel.alt == IS_SET(MODE_ALTSCREEN))
		selscroll(top, bot, n);
}

void
tscrollup(int top, int bot, int n, int mode)
{
	int i, j, s;
	int alt = IS_SET(MODE_ALTSCREEN);
	int savehist = !alt && term.histlimit && top == 0 && mode != SCROLL_NOSAVEHIST;
	int scr = alt ? 0 : term.scr;
	int itop = top + scr, ibot = bot + scr;
	Line temp;
	ImageList *im, *next;

	restoremousecursor();

	if (n <= 0)
		return;
	n = MIN(n, bot-top+1);

	if (savehist) {
		increasehistorysize(term.histf + n, term.col);
		for (i = 0; i < n; i++) {
			term.histi = (term.histi + 1) % term.histsize;
			temp = term.hist[term.histi];
			for (j = 0; j < term.col; j++)
				tclearglyph(&temp[j], 1);
			term.hist[term.histi] = term.line[i];
			term.line[i] = temp;
		}
		term.histf = MIN(term.histf + n, term.histsize);
		s = n;
		if (term.scr) {
			j = term.scr;
			term.scr = MIN(j + n, term.histsize);
			s = j + n - term.scr;
		}
		if (mode != SCROLL_RESIZE)
			tfulldirt();
	} else {
		tclearregion(0, top, term.col-1, top+n-1, 1);
		if (mode != SCROLL_RESIZE)
			tsetdirt(top + scr, bot + scr);
	}

	for (i = top; i <= bot-n; i++) {
		temp = term.line[i];
		term.line[i] = term.line[i+n];
		term.line[i+n] = temp;
	}

	if (alt || !savehist) {
		/* move images, if they are inside the scrolling region */
		for (im = term.images; im; im = next) {
			next = im->next;
			if (im->y >= itop && im->y <= ibot) {
				im->y -= n;
				if (im->y < itop)
					delete_image(im);
			}
		}
	} else {
		/* move images, if they are inside the scrolling region or scrollback */
		for (im = term.images; im; im = next) {
			next = im->next;
			im->y -= scr;
			if (im->y < 0) {
				im->y -= n;
			} else if (im->y >= top && im->y <= bot) {
				im->y -= n;
				if (im->y < top)
					im->y -= top; // move to scrollback
			}
			if (im->y < -term.histsize)
				delete_image(im);
			else
				im->y += term.scr;
		}
	}

	if (sel.ob.x != -1 && sel.alt == alt) {
		if (!savehist) {
			selscroll(top, bot, -n);
		} else if (s > 0) {
			selmove(-s);
			if (-term.scr + sel.nb.y < -term.histf)
				selremove();
		}
	}
}

void
selmove(int n)
{
	sel.ob.y += n, sel.nb.y += n;
	sel.oe.y += n, sel.ne.y += n;
}

void
selscroll(int top, int bot, int n)
{
	/* turn absolute coordinates into relative */
	top += term.scr, bot += term.scr;

	if (BETWEEN(sel.nb.y, top, bot) != BETWEEN(sel.ne.y, top, bot)) {
		selclear();
	} else if (BETWEEN(sel.nb.y, top, bot)) {
		selmove(n);
		if (sel.nb.y < top || sel.ne.y > bot)
			selclear();
	}
}

void
tnewline(int first_col)
{
	int y = term.c.y;

	if (y == term.bot) {
		tscrollup(term.top, term.bot, 1, SCROLL_SAVEHIST);
	} else {
		y++;
	}
	tmoveto(first_col ? 0 : term.c.x, y);
}

void
readsubargs(char **p, int argidx)
{
	int i;
	long int v;
	char *np;

	for (i = 0; **p == ':'; *p = np) {
		++*p;
		np = NULL;
		v = strtol(*p, &np, 10);
		if (np == *p)
			v = 0;
		if (v == LONG_MAX || v == LONG_MIN)
			v = -1;
		if (i < SUB_ARG_SIZ)
			csiescseq.subarg[argidx].value[i++] = v;
	}
	csiescseq.subarg[argidx].count = i;
}

void
csiparse(void)
{
	char *p = csiescseq.buf, *np;
	long int v;

	csiescseq.narg = 0;
	csiescseq.priv = 0;
	if (*p == '?') {
		csiescseq.priv = 1;
		p++;
	}

	csiescseq.buf[csiescseq.len] = '\0';
	while (p < csiescseq.buf+csiescseq.len) {
		np = NULL;
		v = strtol(p, &np, 10);
		if (np == p)
			v = 0;
		if (v == LONG_MAX || v == LONG_MIN)
			v = -1;
		csiescseq.arg[csiescseq.narg++] = v;
		p = np;
		readsubargs(&p, csiescseq.narg-1);
		if (*p != ';' || csiescseq.narg == ESC_ARG_SIZ)
			break;
		p++;
	}
	csiescseq.mode[0] = *p++;
	csiescseq.mode[1] = (p < csiescseq.buf+csiescseq.len) ? *p : '\0';
}

/* for absolute user moves, when decom is set */
void
tmoveato(int x, int y)
{
	tmoveto(x, y + ((term.c.state & CURSOR_ORIGIN) ? term.top: 0));
}

void
tmoveto(int x, int y)
{
	int miny, maxy;

	if (term.c.state & CURSOR_ORIGIN) {
		miny = term.top;
		maxy = term.bot;
	} else {
		miny = 0;
		maxy = term.row - 1;
	}
	term.c.state &= ~CURSOR_WRAPNEXT;
	term.c.x = LIMIT(x, 0, term.col-1);
	term.c.y = LIMIT(y, miny, maxy);
}

void
tsetchar(Rune u, const Glyph *attr, int x, int y)
{
	static const char *vt100_0[62] = { /* 0x41 - 0x7e */
		"↑", "↓", "→", "←", "█", "▚", "☃", /* A - G */
		0, 0, 0, 0, 0, 0, 0, 0, /* H - O */
		0, 0, 0, 0, 0, 0, 0, 0, /* P - W */
		0, 0, 0, 0, 0, 0, 0, " ", /* X - _ */
		"◆", "▒", "␉", "␌", "␍", "␊", "°", "±", /* ` - g */
		"␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺", /* h - o */
		"⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬", /* p - w */
		"│", "≤", "≥", "π", "≠", "£", "·", /* x - ~ */
	};
	uint32_t ftcs;

	/*
	 * The table is proudly stolen from rxvt.
	 */
	if (term.trantbl[term.charset] == CS_GRAPHIC0 &&
	   BETWEEN(u, 0x41, 0x7e) && vt100_0[u - 0x41])
		utf8decode(vt100_0[u - 0x41], &u, UTF_SIZ);

	if (term.line[y][x].mode & ATTR_WIDE) {
		if (x+1 < term.col) {
			term.line[y][x+1].u = ' ';
			term.line[y][x+1].mode &= ~ATTR_WDUMMY;
		}
	} else if (term.line[y][x].mode & ATTR_WDUMMY) {
		if (x > 0) {
			term.line[y][x-1].u = ' ';
			term.line[y][x-1].mode &= ~ATTR_WIDE;
		}
	}

	ftcs = term.line[y][x].extra & (EXT_FTCS_PROMPT1_START | EXT_FTCS_PROMPT1_INPUT);
	term.dirty[y] = 1;
	term.line[y][x] = *attr;
	term.line[y][x].u = u;
	term.line[y][x].mode |= ATTR_SET;
	term.line[y][x].extra |= ftcs;

	if (isboxdraw(u))
		term.line[y][x].mode |= ATTR_BOXDRAW;
}

void
tclearglyph(Glyph *gp, int usecurattr)
{
	if (usecurattr) {
		gp->fg = term.c.attr.fg;
		gp->bg = term.c.attr.bg;
	} else {
		gp->fg = defaultfg;
		gp->bg = defaultbg;
	}
	gp->mode = ATTR_NULL;
	gp->extra = 0;
	gp->u = ' ';
}

void
tclearregion(int x1, int y1, int x2, int y2, int usecurattr)
{
	int x, y;

	/* regionselected() takes relative coordinates */
	if (regionselected(x1, y1+term.scr, x2, y2+term.scr))
		selclear();

	for (y = y1; y <= y2; y++) {
		term.dirty[y] = 1;
		for (x = x1; x <= x2; x++)
			tclearglyph(&term.line[y][x], usecurattr);
	}
}

void
tdeletechar(int n)
{
	int src, dst, size;
	Line line;

	if (n <= 0)
		return;
	dst = term.c.x;
	src = MIN(term.c.x + n, term.col);
	size = term.col - src;
	line = term.line[term.c.y];

	line[term.col-1].mode &= ~ATTR_WRAP;
	line[term.col-2].mode &= ~ATTR_WRAP;

	if (src < term.col && (line[src].mode & ATTR_WDUMMY)) {
		line[src].u = ' ';
		line[src].mode &= ~ATTR_WDUMMY;
	}
	if ((line[dst].mode & ATTR_WDUMMY) && dst > 0) {
		line[dst-1].u = ' ';
		line[dst-1].mode &= ~ATTR_WIDE;
	}

	if (size > 0) { /* otherwise src would point beyond the array
	                   https://stackoverflow.com/questions/29844298 */
		memmove(&line[dst], &line[src], size * sizeof(Glyph));
	}
	for (dst += size; dst < term.col; dst++)
		tclearglyph(&line[dst], 1);

	if (regionselected(term.c.x, term.c.y+term.scr, term.col-1, term.c.y+term.scr))
		selclear();

	term.dirty[term.c.y] = 1;
	term.c.state &= ~CURSOR_WRAPNEXT;
}

void
tinsertblank(int n)
{
	int src, dst, size;
	Line line;

	if (n <= 0)
		return;
	dst = MIN(term.c.x + n, term.col);
	src = term.c.x;
	size = term.col - dst;
	line = term.line[term.c.y];

	if (line[src].mode & ATTR_WDUMMY) {
		line[src].u = ' ';
		line[src].mode &= ~ATTR_WDUMMY;
		if (src > 0) {
			line[src-1].u = ' ';
			line[src-1].mode &= ~ATTR_WIDE;
		}
	}

	if (size > 0) { /* otherwise dst would point beyond the array */
		memmove(&line[dst], &line[src], size * sizeof(Glyph));
	}
	do {
		tclearglyph(&line[src], 1);
	} while (++src < dst);

	if (line[term.col-1].mode & ATTR_WIDE) {
		line[term.col-1].u = ' ';
		line[term.col-1].mode &= ~ATTR_WIDE;
	}

	if (regionselected(term.c.x, term.c.y+term.scr, term.col-1, term.c.y+term.scr))
		selclear();

	term.dirty[term.c.y] = 1;
	term.c.state &= ~CURSOR_WRAPNEXT;
}

void
tinsertblankline(int n)
{
	if (BETWEEN(term.c.y, term.top, term.bot)) {
		tscrolldown(term.c.y, n);
		tmoveto(0, term.c.y);
	}
}

void
tdeleteimages(void)
{
	ImageList *im, *next;

	for (im = term.images; im; im = next) {
		next = im->next;
		delete_image(im);
	}
}

void
tdeleteline(int n)
{
	if (BETWEEN(term.c.y, term.top, term.bot)) {
		tscrollup(term.c.y, term.bot, n, SCROLL_NOSAVEHIST);
		tmoveto(0, term.c.y);
	}
}

int32_t
tdefcolor(const int *attr, int *npar, int l)
{
	Subarg *subarg = &csiescseq.subarg[*npar];
	int32_t color = -1;
	int code = attr[*npar];
	int subidx;
	uint r, g, b;

	/* use colon-separated subarguments if present */
	if (subarg->count > 0) {
		subidx = 0;
		if (subarg->count > 4 && subarg->value[0] == 2) {
			/* ignore colorspace-id */
			subarg->value[1] = 2;
			subidx = 1;
		}
		l = subarg->count;
		attr = subarg->value;
		npar = &subidx;
	} else if (++*npar >= l) {
		fprintf(stderr, "erresc(%d): incorrect number of arguments\n", code);
		return color;
	}

	switch (attr[*npar]) {
	case 2: /* direct color in RGB space */
		if (*npar + 3 >= l) {
			fprintf(stderr, "erresc(%d): incorrect number of arguments\n", code);
			*npar = l;
			break;
		}
		r = attr[++*npar];
		g = attr[++*npar];
		b = attr[++*npar];
		if (!BETWEEN(r, 0, 255) || !BETWEEN(g, 0, 255) || !BETWEEN(b, 0, 255))
			fprintf(stderr, "erresc(%d): bad rgb color (%u,%u,%u)\n", code, r, g, b);
		else
			color = TRUECOLOR(r, g, b);
		break;
	case 5: /* indexed color */
		if (++*npar >= l) {
			fprintf(stderr, "erresc(%d): incorrect number of arguments\n", code);
			break;
		}
		if (!BETWEEN(attr[*npar], 0, 255))
			fprintf(stderr, "erresc(%d): bad color index %d\n", code, attr[*npar]);
		else
			color = attr[*npar];
		break;
	case 0: /* implemented defined (only foreground) */
	case 1: /* transparent */
	case 3: /* direct color in CMY space */
	case 4: /* direct color in CMYK space */
	default:
		fprintf(stderr, "erresc(%d): unknown color type %d\n", code, attr[*npar]);
		--*npar;
		break;
	}

	return color;
}

void
tsetattr(const int *attr, int l)
{
	int i, utype;
	int32_t color;

	for (i = 0; i < l; i++) {
		switch (attr[i]) {
		case 0:
			term.c.attr.mode &= ~(
				ATTR_BOLD       |
				ATTR_FAINT      |
				ATTR_ITALIC     |
				ATTR_UNDERLINE  |
				ATTR_BLINK      |
				ATTR_REVERSE    |
				ATTR_INVISIBLE  |
				ATTR_STRUCK     );
			term.c.attr.fg = defaultfg;
			term.c.attr.bg = defaultbg;
			term.c.attr.extra = 0;
			break;
		case 1:
			term.c.attr.mode |= ATTR_BOLD;
			break;
		case 2:
			term.c.attr.mode |= ATTR_FAINT;
			break;
		case 3:
			term.c.attr.mode |= ATTR_ITALIC;
			break;
		case 4:
			utype = (csiescseq.subarg[i].count > 0) ? csiescseq.subarg[i].value[0] : 1;
			utype = (!undercurl_style && utype >= 3) ? 0 : utype;
			LIMIT(utype, 0, 5);
			term.c.attr.extra = (term.c.attr.extra & ~UNDERLINE_TYPE_MASK) |
			                     (utype << UNDERLINE_TYPE_SHIFT);
			MODBIT(term.c.attr.mode, utype > 0, ATTR_UNDERLINE);
			break;
		case 5: /* slow blink */
			/* FALLTHROUGH */
		case 6: /* rapid blink */
			term.c.attr.mode |= ATTR_BLINK;
			break;
		case 7:
			term.c.attr.mode |= ATTR_REVERSE;
			break;
		case 8:
			term.c.attr.mode |= ATTR_INVISIBLE;
			break;
		case 9:
			term.c.attr.mode |= ATTR_STRUCK;
			break;
		case 22:
			term.c.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT);
			break;
		case 23:
			term.c.attr.mode &= ~ATTR_ITALIC;
			break;
		case 24:
			term.c.attr.mode &= ~ATTR_UNDERLINE;
			break;
		case 25:
			term.c.attr.mode &= ~ATTR_BLINK;
			break;
		case 27:
			term.c.attr.mode &= ~ATTR_REVERSE;
			break;
		case 28:
			term.c.attr.mode &= ~ATTR_INVISIBLE;
			break;
		case 29:
			term.c.attr.mode &= ~ATTR_STRUCK;
			break;
		case 38:
			if ((color = tdefcolor(attr, &i, l)) >= 0)
				term.c.attr.fg = color;
			break;
		case 39: /* set foreground color to default */
			term.c.attr.fg = defaultfg;
			break;
		case 48:
			if ((color = tdefcolor(attr, &i, l)) >= 0)
				term.c.attr.bg = color;
			break;
		case 49: /* set background color to default */
			term.c.attr.bg = defaultbg;
			break;
		case 58:
			if ((color = tdefcolor(attr, &i, l)) >= 0) {
				term.c.attr.extra = (term.c.attr.extra & ~UNDERLINE_COLOR_MASK) |
					(IS_TRUECOL(color) ? EXT_UNDERLINE_COLOR_RGB : EXT_UNDERLINE_COLOR_PALETTE) |
					(color & 0xffffff);
			}
			break;
		case 59: /* reset underline color */
			term.c.attr.extra &= ~UNDERLINE_COLOR_MASK;
			break;
		default:
			if (BETWEEN(attr[i], 30, 37)) {
				term.c.attr.fg = attr[i] - 30;
			} else if (BETWEEN(attr[i], 40, 47)) {
				term.c.attr.bg = attr[i] - 40;
			} else if (BETWEEN(attr[i], 90, 97)) {
				term.c.attr.fg = attr[i] - 90 + 8;
			} else if (BETWEEN(attr[i], 100, 107)) {
				term.c.attr.bg = attr[i] - 100 + 8;
			} else {
				fprintf(stderr,
					"erresc(default): gfx attr %d unknown\n",
					attr[i]);
				csidump();
			}
			break;
		}
	}
}

void
tsetscroll(int t, int b)
{
	int temp;

	LIMIT(t, 0, term.row-1);
	LIMIT(b, 0, term.row-1);
	if (t > b) {
		temp = t;
		t = b;
		b = temp;
	}
	term.top = t;
	term.bot = b;
}

void
tsetmode(int priv, int set, const int *args, int narg)
{
	const int *lim;

	for (lim = args + narg; args < lim; ++args) {
		if (priv) {
			switch (*args) {
			case 1: /* DECCKM -- Cursor key */
				xsetmode(set, MODE_APPCURSOR);
				break;
			case 5: /* DECSCNM -- Reverse video */
				xsetmode(set, MODE_REVERSE);
				break;
			case 6: /* DECOM -- Origin */
				MODBIT(term.c.state, set, CURSOR_ORIGIN);
				tmoveato(0, 0);
				break;
			case 7: /* DECAWM -- Auto wrap */
				MODBIT(term.mode, set, MODE_WRAP);
				if (!set)
					term.c.state &= ~CURSOR_WRAPNEXT;
				break;
			case 0:  /* Error (IGNORED) */
			case 2:  /* DECANM -- ANSI/VT52 (IGNORED) */
			case 3:  /* DECCOLM -- Column  (IGNORED) */
			case 4:  /* DECSCLM -- Scroll (IGNORED) */
			case 8:  /* DECARM -- Auto repeat (IGNORED) */
			case 18: /* DECPFF -- Printer feed (IGNORED) */
			case 19: /* DECPEX -- Printer extent (IGNORED) */
			case 42: /* DECNRCM -- National characters (IGNORED) */
			case 12: /* att610 -- Start blinking cursor (IGNORED) */
				break;
			case 25: /* DECTCEM -- Text Cursor Enable Mode */
				xsetmode(!set, MODE_HIDE);
				break;
			case 9:    /* X10 mouse compatibility mode */
				xsetpointermotion(0);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEX10);
				break;
			case 1000: /* 1000: report button press */
				xsetpointermotion(0);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEBTN);
				break;
			case 1002: /* 1002: report motion on button press */
				xsetpointermotion(0);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEMOTION);
				break;
			case 1003: /* 1003: enable all mouse motions */
				xsetpointermotion(set);
				xsetmode(0, MODE_MOUSE);
				xsetmode(set, MODE_MOUSEMANY);
				break;
			case 1004: /* 1004: send focus events to tty */
				xsetmode(set, MODE_FOCUS);
				break;
			case 1006: /* 1006: extended reporting mode */
				xsetmode(set, MODE_MOUSESGR);
				break;
			case 1034: /* 1034: enable 8-bit mode for keyboard input */
				xsetmode(set, MODE_8BIT);
				break;
			case 1049: /* swap screen & set/restore cursor as xterm */
			case 47: /* swap screen */
			case 1047: /* swap screen, clearing alternate screen */
				if (!allowaltscreen)
					break;
				if (set)
					tloadaltscreen(*args != 47, *args == 1049);
				else
					tloaddefscreen(*args != 47, *args == 1049);
				break;
			case 1048: /* save/restore cursor (like DECSC/DECRC) */
				if (!allowaltscreen)
					break;
				tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
				break;
			case 2004: /* 2004: bracketed paste mode */
				xsetmode(set, MODE_BRCKTPASTE);
				break;
			case 2026: /* Synchronized-Update */
				if (set)
					tsync_begin();  /* BSU */
				else
					tsync_end();  /* ESU */
				break;
			case 2048: /* In-Band Window Resize Notifications */
				MODBIT(term.mode, set, MODE_RESIZE_NOTIFICATIONS);
				sendresizenotification();
				break;
			/* Not implemented mouse modes. See comments there. */
			case 1001: /* mouse highlight mode; can hang the
				      terminal by design when implemented. */
			case 1005: /* UTF-8 mouse mode; will confuse
				      applications not supporting UTF-8
				      and luit. */
			case 1015: /* urxvt mangled mouse mode; incompatible
				      and can be mistaken for other control
				      codes. */
				break;
			case 80: /* DECSDM -- Sixel Display Mode */
				MODBIT(term.mode, set, MODE_SIXEL_SDM);
				break;
			case 1070: /* Use private color registers for each sixel */
				MODBIT(term.mode, set, MODE_SIXEL_PRIVATE_PALETTE);
				break;
			case 8452: /* sixel scrolling leaves cursor to right of graphic */
				MODBIT(term.mode, set, MODE_SIXEL_CUR_RT);
				break;
			default:
				fprintf(stderr,
					"erresc: unknown private set/reset mode %d\n",
					*args);
				break;
			}
		} else {
			switch (*args) {
			case 0:  /* Error (IGNORED) */
				break;
			case 2:
				xsetmode(set, MODE_KBDLOCK);
				break;
			case 4:  /* IRM -- Insertion-replacement */
				MODBIT(term.mode, set, MODE_INSERT);
				break;
			case 12: /* SRM -- Send/Receive */
				MODBIT(term.mode, !set, MODE_ECHO);
				break;
			case 20: /* LNM -- Linefeed/new line */
				MODBIT(term.mode, set, MODE_CRLF);
				break;
			default:
				fprintf(stderr,
					"erresc: unknown set/reset mode %d\n",
					*args);
				break;
			}
		}
	}
}

void
csihandle(void)
{
	char buf[40];
	int n, x;
	int pi, pa;
	ImageList *im, *next;

	switch (csiescseq.mode[0]) {
	default:
	unknown:
		fprintf(stderr, "erresc: unknown csi ");
		csidump();
		/* die(""); */
		break;
	case '@': /* ICH -- Insert <n> blank char */
		DEFAULT(csiescseq.arg[0], 1);
		tinsertblank(csiescseq.arg[0]);
		break;
	case 'A': /* CUU -- Cursor <n> Up */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x, term.c.y-csiescseq.arg[0]);
		break;
	case 'B': /* CUD -- Cursor <n> Down */
	case 'e': /* VPR --Cursor <n> Down */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x, term.c.y+csiescseq.arg[0]);
		break;
	case 'i': /* MC -- Media Copy */
		switch (csiescseq.arg[0]) {
		case 0:
			tdump();
			break;
		case 1:
			tdumpline(term.c.y);
			break;
		case 2:
			tdumpsel();
			break;
		case 4:
			term.mode &= ~MODE_PRINT;
			break;
		case 5:
			term.mode |= MODE_PRINT;
			break;
		}
		break;
	case 'c': /* DA -- Device Attributes */
		if (csiescseq.arg[0] == 0)
			write_da();
		break;
	case 'b': /* REP -- if last char is printable print it <n> more times */
		LIMIT(csiescseq.arg[0], 1, 65535);
		if (term.lastc)
			while (csiescseq.arg[0]-- > 0)
				tputc(term.lastc);
		break;
	case 'C': /* CUF -- Cursor <n> Forward */
	case 'a': /* HPR -- Cursor <n> Forward */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x+csiescseq.arg[0], term.c.y);
		break;
	case 'D': /* CUB -- Cursor <n> Backward */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(term.c.x-csiescseq.arg[0], term.c.y);
		break;
	case 'E': /* CNL -- Cursor <n> Down and first col */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(0, term.c.y+csiescseq.arg[0]);
		break;
	case 'F': /* CPL -- Cursor <n> Up and first col */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(0, term.c.y-csiescseq.arg[0]);
		break;
	case 'g': /* TBC -- Tabulation clear */
		switch (csiescseq.arg[0]) {
		case 0: /* clear current tab stop */
			term.tabs[term.c.x] = 0;
			break;
		case 3: /* clear all the tabs */
			memset(term.tabs, 0, term.col * sizeof(*term.tabs));
			break;
		default:
			goto unknown;
		}
		break;
	case 'G': /* CHA -- Move to <col> */
	case '`': /* HPA */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveto(csiescseq.arg[0]-1, term.c.y);
		break;
	case 'H': /* CUP -- Move to <row> <col> */
	case 'f': /* HVP */
		DEFAULT(csiescseq.arg[0], 1);
		DEFAULT(csiescseq.arg[1], 1);
		tmoveato(csiescseq.arg[1]-1, csiescseq.arg[0]-1);
		break;
	case 'I': /* CHT -- Cursor Forward Tabulation <n> tab stops */
		DEFAULT(csiescseq.arg[0], 1);
		tputtab(csiescseq.arg[0]);
		break;
	case 'J': /* ED -- Clear screen */
		switch (csiescseq.arg[0]) {
		case 0: /* below */
			tclearregion(term.c.x, term.c.y, term.col-1, term.c.y, 1);
			if (term.c.y < term.row-1)
				tclearregion(0, term.c.y+1, term.col-1, term.row-1, 1);
			term.c.state &= ~CURSOR_WRAPNEXT;
			break;
		case 1: /* above */
			if (term.c.y >= 1)
				tclearregion(0, 0, term.col-1, term.c.y-1, 1);
			tclearregion(0, term.c.y, term.c.x, term.c.y, 1);
			term.c.state &= ~CURSOR_WRAPNEXT;
			break;
		case 2: /* screen */
			term.c.state &= ~CURSOR_WRAPNEXT;
			if (IS_SET(MODE_ALTSCREEN)) {
				tclearregion(0, 0, term.col-1, term.row-1, 1);
				tdeleteimages();
				deletehyperlinks(0);
				break;
			}
			/* vte does this:
			tscrollup(0, term.row-1, term.row, SCROLL_SAVEHIST); */
			/* alacritty does this: */
			for (n = term.row-1; n >= 0 && tlinelen(term.line[n]) == 0; n--)
				;
			for (im = term.images; im; im = im->next)
				n = MAX(im->y - term.scr, n);
			if (n >= 0)
				tscrollup(0, term.row-1, n+1, SCROLL_SAVEHIST);
			tscrollup(0, term.row-1, term.row-n-1, SCROLL_NOSAVEHIST);
			break;
		case 3: /* scrollback */
			if (IS_SET(MODE_ALTSCREEN))
				break;
			kscrolldown(&((Arg){ .i = term.scr }));
			term.scr = 0;
			term.histf = 0;
			term.histi = -1;
			for (im = term.images; im; im = next) {
				next = im->next;
				if (im->y < 0)
					delete_image(im);
			}
			deletehyperlinks(1);
			break;
		case 6: /* sixels */
			tdeleteimages();
			tfulldirt();
			break;
		default:
			goto unknown;
		}
		break;
	case 'K': /* EL -- Clear line */
		switch (csiescseq.arg[0]) {
		case 0: /* right */
			/* Workaround for GNU grep:
			 * Sometimes grep emits EL when the cursor is in the wrap
			 * state, which drops the last character of the line. To
			 * prevent this from happening, we don't clear the line in
			 * the wrap state. VTE terminals use the same solution.
			 * https://bugzilla.gnome.org/show_bug.cgi?id=740789
			 */
			if (!(term.c.state & CURSOR_WRAPNEXT))
				tclearregion(term.c.x, term.c.y, term.col - 1, term.c.y, 1);
			break;
		case 1: /* left */
			tclearregion(0, term.c.y, term.c.x, term.c.y, 1);
			term.c.state &= ~CURSOR_WRAPNEXT;
			break;
		case 2: /* all */
			tclearregion(0, term.c.y, term.col-1, term.c.y, 1);
			term.c.state &= ~CURSOR_WRAPNEXT;
			break;
		}
		break;
	case 'S':
		/* XTSMGRAPHICS */
		if (csiescseq.priv) {
			if (csiescseq.narg > 1) {
				pi = csiescseq.arg[0];
				pa = csiescseq.arg[1];
				if (pi == 1 && (pa == 1 || pa == 2 || pa == 4)) {
					/* number of sixel color registers */
					/* (read, reset and read the maximum value give the same response) */
					n = snprintf(buf, sizeof buf, "\033[?1;0;%dS", DECSIXEL_PALETTE_MAX);
					ttywrite(buf, n, 1);
					break;
				} else if (pi == 2 && (pa == 1 || pa == 2 || pa == 4)) {
					/* sixel graphics geometry (in pixels) */
					/* (read, reset and read the maximum value give the same response) */
					n = snprintf(buf, sizeof buf, "\033[?2;0;%d;%dS",
					             MIN(term.col * win.cw, DECSIXEL_WIDTH_MAX),
					             MIN(term.row * win.ch, DECSIXEL_HEIGHT_MAX));
					ttywrite(buf, n, 1);
					break;
				}
				/* the number of color registers and sixel geometry can't be changed */
				n = snprintf(buf, sizeof buf, "\033[?%d;3;0S", pi); /* failure */
				ttywrite(buf, n, 1);
			}
			goto unknown;
		}
		/* SU -- Scroll <n> line up */
		DEFAULT(csiescseq.arg[0], 1);
		/* xterm, urxvt, alacritty save this in history */
		tscrollup(term.top, term.bot, csiescseq.arg[0], SCROLL_SAVEHIST);
		break;
	case 'T': /* SD -- Scroll <n> line down */
		DEFAULT(csiescseq.arg[0], 1);
		tscrolldown(term.top, csiescseq.arg[0]);
		break;
	case 'L': /* IL -- Insert <n> blank lines */
		DEFAULT(csiescseq.arg[0], 1);
		tinsertblankline(csiescseq.arg[0]);
		break;
	case 'l': /* RM -- Reset Mode */
		tsetmode(csiescseq.priv, 0, csiescseq.arg, csiescseq.narg);
		break;
	case 'M': /* DL -- Delete <n> lines */
		DEFAULT(csiescseq.arg[0], 1);
		tdeleteline(csiescseq.arg[0]);
		break;
	case 'X': /* ECH -- Erase <n> char */
		if (csiescseq.arg[0] < 0)
			return;
		DEFAULT(csiescseq.arg[0], 1);
		x = MIN(term.c.x + csiescseq.arg[0], term.col) - 1;
		tclearregion(term.c.x, term.c.y, x, term.c.y, 1);
		term.c.state &= ~CURSOR_WRAPNEXT;
		break;
	case 'P': /* DCH -- Delete <n> char */
		DEFAULT(csiescseq.arg[0], 1);
		tdeletechar(csiescseq.arg[0]);
		break;
	case 'Z': /* CBT -- Cursor Backward Tabulation <n> tab stops */
		DEFAULT(csiescseq.arg[0], 1);
		tputtab(-csiescseq.arg[0]);
		break;
	case 'd': /* VPA -- Move to <row> */
		DEFAULT(csiescseq.arg[0], 1);
		tmoveato(term.c.x, csiescseq.arg[0]-1);
		break;
	case 'h': /* SM -- Set terminal mode */
		tsetmode(csiescseq.priv, 1, csiescseq.arg, csiescseq.narg);
		break;
	case 'm': /* SGR -- Terminal attribute (color) */
		if (csiescseq.priv)
			goto unknown;
		tsetattr(csiescseq.arg, csiescseq.narg);
		break;
	case 'n': /* DSR -- Device Status Report */
		switch (csiescseq.arg[0]) {
		case 5: /* Status Report "OK" `0n` */
			ttywrite("\033[0n", sizeof("\033[0n") - 1, 0);
			break;
		case 6: /* Report Cursor Position (CPR) "<row>;<column>R" */
			n = snprintf(buf, sizeof(buf), "\033[%i;%iR",
			               term.c.y+1, term.c.x+1);
			ttywrite(buf, n, 0);
			break;
		default:
			goto unknown;
		}
		break;
	case '$':
		/* DECRQM -- DEC Request Mode (private) */
		if (csiescseq.mode[1] == 'p' && csiescseq.priv) {
			switch (csiescseq.arg[0]) {
			case 80:
				/* Sixel Display Mode  */
				ttywrite(IS_SET(MODE_SIXEL_SDM) ? "\033[?80;1$y"
				                                : "\033[?80;2$y", 9, 0);
				break;
			case 1070:
				/* Use private color registers for each sixel */
				/* https://invisible-island.net/xterm/ctlseqs/ctlseqs.html */
				ttywrite(IS_SET(MODE_SIXEL_PRIVATE_PALETTE) ? "\033[?1070;1$y"
				                                            : "\033[?1070;2$y", 11, 0);
				break;
			case 2026:
				/* Synchronized Output */
				/* https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004036 */
				ttywrite(su ? "\033[?2026;1$y" : "\033[?2026;2$y", 11, 0);
				break;
			case 2048:
				/* In-Band Window Resize Notifications */
				/* https://gist.github.com/rockorager/e695fb2924d36b2bcf1fff4a3704bd83 */
				ttywrite(IS_SET(MODE_RESIZE_NOTIFICATIONS) ? "\033[?2048;1$y"
				                                           : "\033[?2048;2$y", 11, 0);
				break;
			case 8452:
				/* Sixel scrolling leaves cursor to right of graphic */
				ttywrite(IS_SET(MODE_SIXEL_CUR_RT) ? "\033[?8452;1$y"
				                                   : "\033[?8452;2$y", 11, 0);
				break;
			default:
				goto unknown;
			}
			break;
		}
		goto unknown;
	case 'r': /* DECSTBM -- Set Scrolling Region */
		if (csiescseq.priv) {
			goto unknown;
		} else {
			DEFAULT(csiescseq.arg[0], 1);
			DEFAULT(csiescseq.arg[1], term.row);
			tsetscroll(csiescseq.arg[0]-1, csiescseq.arg[1]-1);
			tmoveato(0, 0);
		}
		break;
	case 's': /* DECSC -- Save cursor position (ANSI.SYS) */
		if (csiescseq.priv)
			goto unknown;
		tcursor(CURSOR_SAVE);
		break;
	case 't': /* XTWINOPS */
		switch (csiescseq.arg[0]) {
		case 14: /* text area size in pixels */
			if (csiescseq.narg > 1)
				goto unknown;
			n = snprintf(buf, sizeof buf, "\033[4;%d;%dt",
			             term.row * win.ch, term.col * win.cw);
			ttywrite(buf, n, 1);
			break;
		case 16: /* character cell size in pixels */
			n = snprintf(buf, sizeof buf, "\033[6;%d;%dt", win.ch, win.cw);
			ttywrite(buf, n, 1);
			break;
		case 18: /* size of the text area in characters */
			n = snprintf(buf, sizeof buf, "\033[8;%d;%dt", term.row, term.col);
			ttywrite(buf, n, 1);
			break;
		case 22: /* pust current title on stack */
			switch (csiescseq.arg[1]) {
			case 0:
			case 1:
			case 2:
				xpushtitle();
				break;
			default:
				goto unknown;
			}
			break;
		case 23: /* pop last title from stack */
			switch (csiescseq.arg[1]) {
			case 0:
			case 1:
			case 2:
				xsettitle(NULL, 1);
				break;
			default:
				goto unknown;
			}
			break;
		default:
			goto unknown;
		}
		break;
	case 'u': /* DECRC -- Restore cursor position (ANSI.SYS) */
		if (csiescseq.priv)
			goto unknown;
		tcursor(CURSOR_LOAD);
		break;
	case ' ':
		switch (csiescseq.mode[1]) {
		case 'q': /* DECSCUSR -- Set Cursor Style */
			if (xsetcursor(csiescseq.arg[0]))
				goto unknown;
			break;
		default:
			goto unknown;
		}
		break;
	}
}

void
csidump(void)
{
	size_t i;
	uint c;

	fprintf(stderr, (term.esc & ESC_DCS) ? "ESCP" : "ESC[");
	for (i = 0; i < csiescseq.len; i++) {
		c = csiescseq.buf[i] & 0xff;
		if (isprint(c)) {
			putc(c, stderr);
		} else if (c == '\n') {
			fprintf(stderr, "(\\n)");
		} else if (c == '\r') {
			fprintf(stderr, "(\\r)");
		} else if (c == 0x1b) {
			fprintf(stderr, "(\\e)");
		} else {
			fprintf(stderr, "(%02x)", c);
		}
	}
	putc('\n', stderr);
}

void
csireset(void)
{
	csiescseq.len = 0;
	memset(&csiescseq.arg, 0, sizeof(csiescseq.arg));
}

void
osc_color_response(int num, int index, int is_osc4)
{
	int n;
	char buf[32];
	unsigned char r, g, b;

	if (xgetcolor(is_osc4 ? num : index, &r, &g, &b)) {
		fprintf(stderr, "erresc: failed to fetch %s color %d\n",
		        is_osc4 ? "osc4" : "osc",
		        is_osc4 ? num : index);
		return;
	}

	n = snprintf(buf, sizeof buf, "\033]%s%d;rgb:%02x%02x/%02x%02x/%02x%02x%s",
	             is_osc4 ? "4;" : "", num, r, r, g, g, b, b, strescseq.term);
	if (n < 0 || n >= sizeof(buf)) {
		fprintf(stderr, "error: %s while printing %s response\n",
		        n < 0 ? "snprintf failed" : "truncation occurred",
		        is_osc4 ? "osc4" : "osc");
	} else {
		ttywrite(buf, n, 1);
	}
}

void
write_da(void)
{
	ttywrite(vtiden, strlen(vtiden)-1, 0); /* omit last 'c' */
	if (allowwindowops)
		ttywrite(";52", 3, 0);
	ttywrite("c", 1, 0);
}

void
strhandle(void)
{
	char *p = NULL, *dec;
	int i, j, x, narg, par, dirty, prompt;
	const struct { int idx; char *str; } osc_table[] = {
		{ defaultfg, "foreground" },
		{ defaultbg, "background" },
		{ defaultcs, "cursor" }
	};

	term.esc &= ~(ESC_STR_END|ESC_STR);

	switch (strescseq.type) {
	case ']': /* OSC -- Operating System Command */
		strparse();
		par = (narg = strescseq.narg) ? atoi(strescseq.args[0]) : 0;
		switch (par) {
		case 0: /* change window title and icon name */
			if (narg > 1) {
				xsettitle(strescseq.args[1], 0);
				xseticontitle(strescseq.args[1]);
			}
			return;
		case 1: /* change icon name */
			if (narg > 1)
				xseticontitle(strescseq.args[1]);
			return;
		case 2: /* change window title */
			if (narg > 1)
				xsettitle(strescseq.args[1], 0);
			return;
		case 52: /* manipulate selection data */
			if (narg > 2 && allowwindowops) {
				dec = base64dec(strescseq.args[2]);
				if (dec) {
					xsetsel(dec);
					xclipcopy();
				} else {
					fprintf(stderr, "erresc: invalid base64\n");
				}
			}
			return;
		case 7: /* set working directory for the new terminal window */
			osc7parsecwd((const char *)strescseq.args[1]);
			return;
		case 8: /* hyperlink */
			if (!disablehyperlinks && term.hyperlinks->capacity > 0)
				parsehyperlink(narg-1, strescseq.args[1], strescseq.args[2]);
			return;
		case 10: /* set dynamic VT100 text foreground color */
		case 11: /* set dynamic VT100 text background color */
		case 12: /* set dynamic text cursor color */
			if (narg < 2)
				break;
			p = strescseq.args[1];
			if ((j = par - 10) < 0 || j >= LEN(osc_table))
				break; /* shouldn't be possible */

			if (!strcmp(p, "?")) {
				osc_color_response(par, osc_table[j].idx, 0);
			} else if (xsetcolorname(osc_table[j].idx, p)) {
				fprintf(stderr, "erresc (OSC %d): invalid %s color: %s\n",
				        par, osc_table[j].str, p);
			} else {
				tfulldirt();
			}
			return;
		case 4: /* color set */
			for (dirty = 0, i = 1; i < narg; i += 2) {
				j = atoi(strescseq.args[i]);
				p = (i + 1 < narg) ? strescseq.args[i+1] : NULL;
				if (p && !strcmp(p, "?")) {
					osc_color_response(j, 0, 1);
				} else if (!p || xsetcolorname(j, p)) {
					fprintf(stderr, "erresc (OSC 4): invalid color j=%d, p=%s\n",
					        j, p ? p : "(null)");
				} else if (!dirty) {
					tfulldirt();
					dirty = 1;
				}
			}
			return;
		case 104: /* color reset */
			if (narg == 1 || (narg == 2 && !strescseq.args[1][0])) {
				/* if no parameters, reset all 256 colors */
				for (i = 0; i < 256; i++)
					xsetcolorname(i, NULL);
				tfulldirt();
				return;
			}

			for (dirty = 0, i = 1; i < narg; i++) {
				if (!strescseq.args[i][0])
					continue;
				j = atoi(strescseq.args[i]);
				if (xsetcolorname(j, NULL)) {
					fprintf(stderr, "erresc (OSC 104): invalid color j=%d\n", j);
				} else if (!dirty) {
					tfulldirt();
					dirty = 1;
				}
			}
			return;
		case 110: /* reset dynamic VT100 text foreground color */
		case 111: /* reset dynamic VT100 text background color */
		case 112: /* reset dynamic text cursor color */
			if ((j = par - 110) < 0 || j >= LEN(osc_table))
				break; /* shouldn't be possible */
			if (xsetcolorname(osc_table[j].idx, NULL)) {
				fprintf(stderr, "erresc (OSC %d): %s color not found\n", par, osc_table[j].str);
			} else {
				tfulldirt();
			}
			return;
		case 133: /* semantic prompts */
			if (narg < 2)
				break;
			for (prompt = 0, i = 2; i < narg; i++) {
				p = strescseq.args[i];
				if (p[0] == 'k' && p[1] == '=')
					prompt = (p[2] == 'i' && !p[3]) ? 1 : 2;
			}
			switch (*strescseq.args[1]) {
			case 'A':
			case 'P':
				/* start of shell prompt PS1 */
				if (prompt == 2) {
					term.c.state |= CURSOR_PROMPT2;
					break;
				}
				term.line[term.c.y][term.c.x].extra |= EXT_FTCS_PROMPT1_START;
				term.c.state &= ~CURSOR_PROMPT2;
				break;
			case 'B':
				/* start of prompt input PS1 */
				if (prompt == 2 || (prompt == 0 && (term.c.state & CURSOR_PROMPT2)))
					break;
				/* we store the input flag in the previous character
				 * because otherwise shell might erase it */
				x = (term.c.state & CURSOR_WRAPNEXT) ? term.c.x : MAX(term.c.x - 1, 0);
				if (x > 0 && (term.line[term.c.y][x].mode & ATTR_WDUMMY))
					x--;
				term.line[term.c.y][x].extra |= EXT_FTCS_PROMPT1_INPUT;
				break;
			default:
				/* fprintf(stderr, "erresc: unknown OSC 133 argument: %c\n", *strescseq.args[1]); */
				break;
			}
			return;
		}
		break;
	case 'k': /* old title set compatibility */
		strescseq.buf[strescseq.len] = '\0';
		xsettitle(strescseq.buf, 0);
		return;
	case 'P': /* DCS -- Device Control String */
		dcshandle();
		return;
	case '_': /* APC -- Application Program Command */
	case '^': /* PM -- Privacy Message */
		return;
	}

	fprintf(stderr, "erresc: unknown str ");
	strdump();
}

void
strparse(void)
{
	int c;
	char *p = strescseq.buf;

	strescseq.narg = 0;
	strescseq.buf[strescseq.len] = '\0';

	if (*p == '\0')
		return;

	if (strescseq.type == ']' && p[1] == ';') {
		if (p[0] <= '2' || p[0] == '7') {
			/* preserve semicolons in window titles, icon names and OSC 7 sequences */
			strescseq.args[strescseq.narg++] = p;
			strescseq.args[strescseq.narg++] = p + 2;
			p[1] = '\0';
			return;
		} else if (p[0] == '8') {
			/* preserve semicolons in hyperlinks (OSC 8) */
			strescseq.args[strescseq.narg++] = p;
			*(++p) = '\0';
			if (*(++p)) {
				strescseq.args[strescseq.narg++] = p;
				if ((p = strchr(p, ';')) != NULL) {
					strescseq.args[strescseq.narg++] = p + 1;
					p[0] = '\0';
				}
			}
			return;
		}
	}

	while (strescseq.narg < STR_ARG_SIZ) {
		strescseq.args[strescseq.narg++] = p;
		while ((c = *p) != ';' && c != '\0')
			++p;
		if (c == '\0')
			return;
		*p++ = '\0';
	}
}

void
strdump(void)
{
	size_t i;
	uint c;

	fprintf(stderr, "ESC%c", strescseq.type);
	for (i = 0; i < strescseq.len; i++) {
		c = strescseq.buf[i] & 0xff;
		if (c == '\0') {
			putc('\n', stderr);
			return;
		} else if (isprint(c)) {
			putc(c, stderr);
		} else if (c == '\n') {
			fprintf(stderr, "(\\n)");
		} else if (c == '\r') {
			fprintf(stderr, "(\\r)");
		} else if (c == 0x1b) {
			fprintf(stderr, "(\\e)");
		} else {
			fprintf(stderr, "(%02x)", c);
		}
	}
	fprintf(stderr, (strescseq.term[0] == 0x1b) ? "ESC\\\n" : "BEL\n");
}

void
strreset(void)
{
	strescseq = (STREscape){
		.buf = xrealloc(strescseq.buf, STR_BUF_SIZ),
		.siz = STR_BUF_SIZ,
		.term = STR_TERM_ST,
	};
}

void
sendbreak(const Arg *arg)
{
	if (!term.hold && tcsendbreak(cmdfd, 0))
		perror("Error sending break");
}

void
tprinter(char *s, size_t len)
{
	if (iofd != -1 && xwrite(iofd, s, len) < 0) {
		perror("Error writing to output file");
		close(iofd);
		iofd = -1;
	}
}

void
toggleprinter(const Arg *arg)
{
	term.mode ^= MODE_PRINT;
}

void
printscreen(const Arg *arg)
{
	tdump();
}

void
printsel(const Arg *arg)
{
	tdumpsel();
}

void
tdumpsel(void)
{
	char *ptr;

	if ((ptr = getsel())) {
		tprinter(ptr, strlen(ptr));
		free(ptr);
	}
}

void
tdumpline(int n)
{
	char str[(term.col + 1) * UTF_SIZ];

	tprinter(str, tgetline(str, &term.line[n][0]));
}

void
tdump(void)
{
	int i;

	for (i = 0; i < term.row; ++i)
		tdumpline(i);
}

void
tputtab(int n)
{
	uint x = term.c.x;

	if (n > 0) {
		while (x < term.col && n--)
			for (++x; x < term.col && !term.tabs[x]; ++x)
				/* nothing */ ;
	} else if (n < 0) {
		while (x > 0 && n++)
			for (--x; x > 0 && !term.tabs[x]; --x)
				/* nothing */ ;
	}
	term.c.x = LIMIT(x, 0, term.col-1);
}

void
tdefutf8(char ascii)
{
	if (ascii == 'G')
		term.mode |= MODE_UTF8;
	else if (ascii == '@')
		term.mode &= ~MODE_UTF8;
}

void
tdeftran(char ascii)
{
	static char cs[] = "0B";
	static int vcs[] = {CS_GRAPHIC0, CS_USA};
	char *p;

	if ((p = strchr(cs, ascii)) == NULL) {
		fprintf(stderr, "esc unhandled charset: ESC ( %c\n", ascii);
	} else {
		term.trantbl[term.icharset] = vcs[p - cs];
	}
}

void
tdectest(char c)
{
	int x, y;

	if (c == '8') { /* DEC screen alignment test. */
		for (x = 0; x < term.col; ++x) {
			for (y = 0; y < term.row; ++y)
				tsetchar('E', &term.c.attr, x, y);
		}
	}
}

void
tstrsequence(uchar c)
{
	strreset();

	switch (c) {
	case 0x90:   /* DCS -- Device Control String */
		c = 'P';
		term.esc |= ESC_DCS;
		break;
	case 0x9f:   /* APC -- Application Program Command */
		c = '_';
		break;
	case 0x9e:   /* PM -- Privacy Message */
		c = '^';
		break;
	case 0x9d:   /* OSC -- Operating System Command */
		c = ']';
		break;
	}
	strescseq.type = c;
	term.esc |= ESC_STR;
}

void
tcontrolcode(uchar ascii)
{
	switch (ascii) {
	case '\t':   /* HT */
		tputtab(1);
		return;
	case '\b':   /* BS */
		tmoveto(term.c.x-1, term.c.y);
		return;
	case '\r':   /* CR */
		tmoveto(0, term.c.y);
		return;
	case '\f':   /* LF */
	case '\v':   /* VT */
	case '\n':   /* LF */
		/* go to first col if the mode is set */
		tnewline(IS_SET(MODE_CRLF));
		return;
	case '\a':   /* BEL */
		if (term.esc & ESC_STR_END) {
			/* backwards compatibility to xterm */
			strescseq.term = STR_TERM_BEL;
			strhandle();
		} else {
			xbell();
		}
		break;
	case '\033': /* ESC */
		csireset();
		term.esc &= ~(ESC_CSI|ESC_ALTCHARSET|ESC_TEST);
		term.esc |= ESC_START;
		return;
	case '\016': /* SO (LS1 -- Locking shift 1) */
	case '\017': /* SI (LS0 -- Locking shift 0) */
		term.charset = 1 - (ascii - '\016');
		return;
	case '\032': /* SUB */
		tsetchar('?', &term.c.attr, term.c.x, term.c.y);
		/* FALLTHROUGH */
	case '\030': /* CAN */
		csireset();
		break;
	case '\005': /* ENQ (IGNORED) */
	case '\000': /* NUL (IGNORED) */
	case '\021': /* XON (IGNORED) */
	case '\023': /* XOFF (IGNORED) */
	case 0177:   /* DEL (IGNORED) */
		return;
	case 0x80:   /* TODO: PAD */
	case 0x81:   /* TODO: HOP */
	case 0x82:   /* TODO: BPH */
	case 0x83:   /* TODO: NBH */
	case 0x84:   /* TODO: IND */
		break;
	case 0x85:   /* NEL -- Next line */
		tnewline(1); /* always go to first col */
		break;
	case 0x86:   /* TODO: SSA */
	case 0x87:   /* TODO: ESA */
		break;
	case 0x88:   /* HTS -- Horizontal tab stop */
		term.tabs[term.c.x] = 1;
		break;
	case 0x89:   /* TODO: HTJ */
	case 0x8a:   /* TODO: VTS */
	case 0x8b:   /* TODO: PLD */
	case 0x8c:   /* TODO: PLU */
	case 0x8d:   /* TODO: RI */
	case 0x8e:   /* TODO: SS2 */
	case 0x8f:   /* TODO: SS3 */
	case 0x91:   /* TODO: PU1 */
	case 0x92:   /* TODO: PU2 */
	case 0x93:   /* TODO: STS */
	case 0x94:   /* TODO: CCH */
	case 0x95:   /* TODO: MW */
	case 0x96:   /* TODO: SPA */
	case 0x97:   /* TODO: EPA */
	case 0x98:   /* TODO: SOS */
	case 0x99:   /* TODO: SGCI */
		break;
	case 0x9a:   /* DECID -- Identify Terminal */
		write_da();
		break;
	case 0x9b:   /* TODO: CSI */
	case 0x9c:   /* TODO: ST */
		break;
	case 0x90:   /* DCS -- Device Control String */
	case 0x9d:   /* OSC -- Operating System Command */
	case 0x9e:   /* PM -- Privacy Message */
	case 0x9f:   /* APC -- Application Program Command */
		tstrsequence(ascii);
		return;
	}
	/* only CAN, SUB, \a and C1 chars interrupt a sequence */
	term.esc &= ~(ESC_STR_END|ESC_STR);
}

void
dcshandle(void)
{
	int n;
	char buf[16];

	/* DECSIXEL */
	if (IS_SET(MODE_SIXEL)) {
		createsixel();
		term.mode &= ~MODE_SIXEL;
		return;
	}

	strescseq.buf[strescseq.len] = '\0';

	/* DECRQSS */
	if (strescseq.buf[0] == '$' && strescseq.buf[1] == 'q') {
		if (strescseq.buf[2] == ' ' && strescseq.buf[3] == 'q') {
			/* DECSCUSR - cursor style */
			n = snprintf(buf, sizeof buf, "\033P1$r%d q\033\\", win.cursor);
			ttywrite(buf, n, 0);
			return;
		} else {
			/* invalid request */
			ttywrite("\033P0$r\033\\", 7, 0);
		}
	}

	/* Synchronized updates */
	/* https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec */
	if (strescseq.buf[0] == '=') {
		if (strescseq.buf[1] == '1' && strescseq.buf[2] == 's') {
			tsync_begin();  /* BSU */
			return;
		} else if (strescseq.buf[1] == '2' && strescseq.buf[2] == 's') {
			tsync_end();  /* ESU */
			return;
		}
	}

	fprintf(stderr, "erresc: unknown dcs ");
	strdump();
}

void
initsixel(void)
{
	int par, transparent;
	uint bgcolor;
	uchar r, g, b, a = 255;

	/* If we are already in sixel mode, it means that we have not yet
	 * finished the current sixel. So we need to do that first before we
	 * can start the new sixel. */
	if (IS_SET(MODE_SIXEL)) {
		createsixel();
		term.mode &= ~MODE_SIXEL;
	}

	par = csiescseq.narg >= 1 ? csiescseq.arg[0] : 0;
	transparent = (csiescseq.narg >= 2 && csiescseq.arg[1] == 1);
	if (IS_TRUECOL(term.c.attr.bg)) {
		r = term.c.attr.bg >> 16 & 255;
		g = term.c.attr.bg >> 8 & 255;
		b = term.c.attr.bg >> 0 & 255;
	} else {
		xgetcolor(term.c.attr.bg, &r, &g, &b);
		if (term.c.attr.bg == defaultbg)
			a = dc.col[defaultbg].pixel >> 24 & 255;
	}
	bgcolor = (uint)a << 24 | (uint)r << 16 | (uint)g << 8 | (uint)b;
	if (sixel_parser_init(&sixel_st, par, transparent, bgcolor,
	                      IS_SET(MODE_SIXEL_PRIVATE_PALETTE)) != 0)
		perror("sixel_parser_init() failed");
	term.mode |= MODE_SIXEL;
}

void
createsixel(void)
{
	int cx, cy;
	ImageList *im, *newimages, *next, *tail = NULL;
	int scr = IS_SET(MODE_ALTSCREEN) ? 0 : term.scr;
	int i, j, x1, y1, x2, y2, y, numimages;
	Line line;

	if (!sixel_st.image.data) {
		sixel_parser_deinit(&sixel_st);
		return;
	}

	cx = IS_SET(MODE_SIXEL_SDM) ? 0 : term.c.x;
	cy = IS_SET(MODE_SIXEL_SDM) ? 0 : term.c.y;
	if ((numimages = sixel_parser_finalize(&sixel_st, &newimages,
			cx, cy + scr, win.cw, win.ch)) <= 0) {
		sixel_parser_deinit(&sixel_st);
		perror("sixel_parser_finalize() failed");
		return;
	}
	sixel_parser_deinit(&sixel_st);

	x1 = newimages->x;
	y1 = newimages->y;
	x2 = x1 + newimages->cols;
	y2 = y1 + numimages;

	/* Delete the old images that are covered by the new image(s). We also need
	 * to check if they have already been deleted before adding the new ones. */
	if (term.images) {
		char transparent[numimages];
		for (i = 0, im = newimages; im; im = im->next, i++) {
			transparent[i] = im->transparent;
		}
		for (im = term.images; im; im = next) {
			next = im->next;
			if (im->y >= y1 && im->y < y2) {
				y = im->y - scr;
				if (y >= 0 && y < term.row && term.dirty[y]) {
					line = term.line[y];
					j = MIN(im->x + im->cols, term.col);
					for (i = im->x; i < j; i++) {
						if (line[i].extra & EXT_SIXEL)
							break;
					}
					if (i == j) {
						delete_image(im);
						continue;
					}
				}
				if (im->x >= x1 && im->x + im->cols <= x2 && !transparent[im->y - y1]) {
					delete_image(im);
					continue;
				}
			}
			tail = im;
		}
	}
	if (tail) {
		tail->next = newimages;
		newimages->prev = tail;
	} else {
		term.images = newimages;
	}

	x2 = MIN(x2, term.col) - 1;
	if (IS_SET(MODE_SIXEL_SDM)) {
		/* Sixel display mode: put the sixel in the upper left corner of
		 * the screen, disable scrolling (the sixel will be truncated if
		 * it is too long) and do not change the cursor position. */
		for (i = 0, im = newimages; im; im = next, i++) {
			next = im->next;
			if (i >= term.row) {
				delete_image(im);
				continue;
			}
			im->y = i + scr;
			tsetsixelattr(term.line[i], x1, x2);
			term.dirty[MIN(im->y, term.row-1)] = 1;
			term.dirtyimg[MIN(im->y, term.row-1)] = 1;
		}
	} else {
		for (i = 0, im = newimages; im; im = next, i++) {
			next = im->next;
			im->y = term.c.y + scr;
			tsetsixelattr(term.line[term.c.y], x1, x2);
			term.dirty[MIN(im->y, term.row-1)] = 1;
			term.dirtyimg[MIN(im->y, term.row-1)] = 1;
			if (i < numimages-1) {
				im->next = NULL;
				tnewline(0);
				im->next = next;
			}
		}
		/* if mode 8452 is set, sixel scrolling leaves cursor to right of graphic */
		if (IS_SET(MODE_SIXEL_CUR_RT))
			term.c.x = MIN(term.c.x + newimages->cols, term.col-1);
	}
}

/*
 * returns 1 when the sequence is finished and it hasn't to read
 * more characters for this sequence, otherwise 0
 */
int
eschandle(uchar ascii)
{
	switch (ascii) {
	case '[':
		term.esc |= ESC_CSI;
		return 0;
	case '#':
		term.esc |= ESC_TEST;
		return 0;
	case '%':
		term.esc |= ESC_UTF8;
		return 0;
	case 'P': /* DCS -- Device Control String */
		term.esc |= ESC_DCS;
	case '_': /* APC -- Application Program Command */
	case '^': /* PM -- Privacy Message */
	case ']': /* OSC -- Operating System Command */
	case 'k': /* old title set compatibility */
		tstrsequence(ascii);
		return 0;
	case 'n': /* LS2 -- Locking shift 2 */
	case 'o': /* LS3 -- Locking shift 3 */
		term.charset = 2 + (ascii - 'n');
		break;
	case '(': /* GZD4 -- set primary charset G0 */
	case ')': /* G1D4 -- set secondary charset G1 */
	case '*': /* G2D4 -- set tertiary charset G2 */
	case '+': /* G3D4 -- set quaternary charset G3 */
		term.icharset = ascii - '(';
		term.esc |= ESC_ALTCHARSET;
		return 0;
	case 'D': /* IND -- Linefeed */
		if (term.c.y == term.bot) {
			tscrollup(term.top, term.bot, 1, SCROLL_SAVEHIST);
		} else {
			tmoveto(term.c.x, term.c.y+1);
		}
		term.c.state &= ~CURSOR_WRAPNEXT;
		break;
	case 'E': /* NEL -- Next line */
		tnewline(1); /* always go to first col */
		break;
	case 'H': /* HTS -- Horizontal tab stop */
		term.tabs[term.c.x] = 1;
		break;
	case 'M': /* RI -- Reverse index */
		if (term.c.y == term.top) {
			tscrolldown(term.top, 1);
		} else {
			tmoveto(term.c.x, term.c.y-1);
		}
		term.c.state &= ~CURSOR_WRAPNEXT;
		break;
	case 'Z': /* DECID -- Identify Terminal */
		write_da();
		break;
	case 'c': /* RIS -- Reset to initial state */
		win.mode ^= kbds_keyboardhandler(XK_Escape, NULL, 0, 1);
		treset();
		xsetcursor(0); /* reset cursor style */
		xfreetitlestack();
		resettitle();
		xloadcols();
		xsetmode(0, MODE_APPCURSOR | MODE_APPKEYPAD | MODE_BRCKTPASTE);
		xsetmode(0, MODE_HIDE | MODE_KBDLOCK | MODE_REVERSE);
		break;
	case '=': /* DECPAM -- Application keypad */
		xsetmode(1, MODE_APPKEYPAD);
		break;
	case '>': /* DECPNM -- Normal keypad */
		xsetmode(0, MODE_APPKEYPAD);
		break;
	case '7': /* DECSC -- Save Cursor */
		tcursor(CURSOR_SAVE);
		break;
	case '8': /* DECRC -- Restore Cursor */
		tcursor(CURSOR_LOAD);
		break;
	case '\\': /* ST -- String Terminator */
		if (term.esc & ESC_STR_END) {
			strescseq.term = STR_TERM_ST;
			strhandle();
		}
		break;
	default:
		fprintf(stderr, "erresc: unknown sequence ESC 0x%02X '%c'\n",
			(uchar) ascii, isprint(ascii)? ascii:'.');
		break;
	}
	return 1;
}

void
tputc(Rune u)
{
	char c[UTF_SIZ];
	int control;
	int width, len;
	Glyph *gp;

	control = ISCONTROL(u);
	if (u < 127 || !IS_SET(MODE_UTF8))
	{
		c[0] = u;
		width = len = 1;
	} else {
		len = utf8encode(u, c);
		if (!control && (width = wcwidth(u)) == -1)
			width = 1;
	}

	if (IS_SET(MODE_PRINT))
		tprinter(c, len);

	/*
	 * STR sequence must be checked before anything else
	 * because it uses all following characters until it
	 * receives a ESC, a SUB, a ST or any other C1 control
	 * character.
	 */
	if (term.esc & ESC_STR) {
		if (u == '\a' || u == 030 || u == 032 || u == 033 ||
		   ISCONTROLC1(u)) {
			term.esc &= ~(ESC_START|ESC_STR|ESC_DCS);
			term.esc |= ESC_STR_END;
			goto check_control_code;
		}

		if (term.esc & ESC_DCS) {
			control = 0;
			goto check_control_code;
		}

		if (strescseq.len+UTF_SIZ >= strescseq.siz) {
			/*
			 * Here is a bug in terminals. If the user never sends
			 * some code to stop the str or esc command, then st
			 * will stop responding. But this is better than
			 * silently failing with unknown characters. At least
			 * then users will report back.
			 *
			 * In the case users ever get fixed, here is the code:
			 */
			/*
			 * term.esc = 0;
			 * strhandle();
			 */
			if (strescseq.siz > (SIZE_MAX - UTF_SIZ) / 2)
				return;
			strescseq.siz *= 2;
			strescseq.buf = xrealloc(strescseq.buf, strescseq.siz);
		}

		/* Prefer UTF_SIZ to len because the constant size lets the
		 * compiler optimize memcpy. */
		memcpy(&strescseq.buf[strescseq.len], c, UTF_SIZ);
		strescseq.len += len;
		return;
	}

check_control_code:
	/*
	 * Actions of control codes must be performed as soon they arrive
	 * because they can be embedded inside a control sequence, and
	 * they must not cause conflicts with sequences.
	 */
	if (control) {
		/* in UTF-8 mode ignore handling C1 control characters */
		if (IS_SET(MODE_UTF8) && ISCONTROLC1(u))
			return;
		tcontrolcode(u);
		/*
		 * control codes are not shown ever
		 */
		if (!term.esc)
			term.lastc = 0;
		return;
	} else if (term.esc & ESC_START) {
		if (term.esc & ESC_CSI) {
			csiescseq.buf[csiescseq.len++] = u;
			if (BETWEEN(u, 0x40, 0x7E)
					|| csiescseq.len >= \
					sizeof(csiescseq.buf)-1) {
				term.esc = 0;
				csiparse();
				csihandle();
			}
			return;
		} else if (term.esc & ESC_DCS) {
			if (strescseq.len < STR_BUF_SIZ-1 && strescseq.len < sizeof(csiescseq.buf)-1) {
				strescseq.buf[strescseq.len++] = u;
				csiescseq.buf[csiescseq.len++] = u;
				if (u == 'q') {
					/* DCS sequences are processed after the ST arrives, but the sixel
					 * mode must be turned on as soon as the sixel header is detected. */
					csiparse();
					if (csiescseq.mode[0] == 'q')
						initsixel();
				}
			}
			return;
		} else if (term.esc & ESC_UTF8) {
			tdefutf8(u);
		} else if (term.esc & ESC_ALTCHARSET) {
			tdeftran(u);
		} else if (term.esc & ESC_TEST) {
			tdectest(u);
		} else {
			if (!eschandle(u))
				return;
			/* sequence already finished */
		}
		term.esc = 0;
		/*
		 * All characters which form part of a sequence are not
		 * printed
		 */
		return;
	}
	/* selected() takes relative coordinates */
	if (selected(term.c.x, term.c.y + term.scr))
		selclear();

	gp = &term.line[term.c.y][term.c.x];
	if (IS_SET(MODE_WRAP) && (term.c.state & CURSOR_WRAPNEXT)) {
		term.line[term.c.y][term.col-1].mode |= ATTR_WRAP;
		tnewline(1);
		gp = &term.line[term.c.y][term.c.x];
	}

	if (IS_SET(MODE_INSERT) && term.c.x+width < term.col)
		tinsertblank(width);

	if (term.c.x+width > term.col) {
		if (IS_SET(MODE_WRAP)) {
			tclearglyph(&term.line[term.c.y][term.col-1], 0);
			term.line[term.c.y][term.col-2].mode |= ATTR_WRAP;
			tnewline(1);
		} else {
			tmoveto(term.col - width, term.c.y);
		}
		gp = &term.line[term.c.y][term.c.x];
	}

	tsetchar(u, &term.c.attr, term.c.x, term.c.y);
	term.lastc = u;

	if (width == 2) {
		gp->mode |= ATTR_WIDE;
		if (term.c.x+1 < term.col) {
			if ((gp[1].mode & ATTR_WIDE) && term.c.x+2 < term.col) {
				gp[2].u = ' ';
				gp[2].mode &= ~ATTR_WDUMMY;
			}
			gp[1].u = '\0';
			gp[1].mode = ATTR_WDUMMY | ATTR_SET;
		}
	}
	if (term.c.x+width < term.col) {
		tmoveto(term.c.x+width, term.c.y);
	} else {
		term.wrapcwidth[IS_SET(MODE_ALTSCREEN)] = width;
		term.c.state |= CURSOR_WRAPNEXT;
	}
}

int
twrite(const char *buf, int buflen, int show_ctrl)
{
	int charsize;
	Rune u;
	int n;

	int su0 = su;
	twrite_aborted = 0;

	for (n = 0; n < buflen; n += charsize) {
		if (IS_SET(MODE_SIXEL) && sixel_st.state != PS_ESC) {
			charsize = sixel_parser_parse(&sixel_st, (const unsigned char*)buf + n, buflen - n);
			continue;
		} else if (IS_SET(MODE_UTF8)) {
			/* process a complete utf8 char */
			charsize = utf8decode(buf + n, &u, buflen - n);
			if (charsize == 0)
				break;
		} else {
			u = buf[n] & 0xFF;
			charsize = 1;
		}
		if (su0 && !su) {
			twrite_aborted = 1;
			break;  // ESU - allow rendering before a new BSU
		}
		if (show_ctrl && ISCONTROL(u)) {
			if (u & 0x80) {
				u &= 0x7f;
				tputc('^');
				tputc('[');
			} else if (u != '\n' && u != '\r' && u != '\t') {
				u ^= 0x40;
				tputc('^');
			}
		}
		tputc(u);
	}
	return n;
}

void
treflow_moveimages(int oldy, int newy)
{
	ImageList *im;

	for (im = term.images; im; im = im->next) {
		if (im->y == oldy)
			im->reflow_y = newy;
	}
}

void
treflow(int col, int row)
{
	int i, j;
	int oce, nce, bot, scr;
	int ox = 0, oy = -term.histf, nx = 0, ny = -1, len;
	int cy = -1; /* proxy for new y coordinate of cursor */
	int buflen, nlines;
	Line *buf, bufline, line;
	ImageList *im, *next;

	/* unset reflow_y in images */
	for (im = term.images; im; im = im->next)
		im->reflow_y = INT_MIN;

	/* y coordinate of cursor line end */
	for (oce = term.c.y; oce < term.row - 1 &&
	                     tiswrapped(term.line[oce]); oce++);

	nlines = term.histlimit + row;
	buf = xmalloc(nlines * sizeof(Line));
	do {
		if (!nx && ++ny < nlines)
			buf[ny] = xmalloc(col * sizeof(Glyph));
		if (!ox) {
			line = TLINEABS(oy);
			len = tlinelen(line);
		}
		if (oy == term.c.y) {
			if (!ox)
				len = MAX(len, term.c.x + 1);
			/* update cursor */
			if (cy < 0 && term.c.x - ox < col - nx) {
				term.c.x = nx + term.c.x - ox, cy = ny;
				UPDATEWRAPNEXT(0, col);
			}
		}
		/* get reflowed lines in buf */
		bufline = buf[ny % nlines];
		if (col - nx > len - ox) {
			memcpy(&bufline[nx], &line[ox], (len-ox) * sizeof(Glyph));
			nx += len - ox;
			if (len == 0 || !(line[len - 1].mode & ATTR_WRAP)) {
				for (j = nx; j < col; j++)
					tclearglyph(&bufline[j], 0);
				treflow_moveimages(oy+term.scr, ny);
				nx = 0;
			} else if (nx > 0) {
				bufline[nx - 1].mode &= ~ATTR_WRAP;
			}
			ox = 0, oy++;
		} else if (col - nx == len - ox) {
			memcpy(&bufline[nx], &line[ox], (col-nx) * sizeof(Glyph));
			treflow_moveimages(oy+term.scr, ny);
			ox = 0, oy++, nx = 0;
		} else/* if (col - nx < len - ox) */ {
			memcpy(&bufline[nx], &line[ox], (col-nx) * sizeof(Glyph));
			if (bufline[col - 1].mode & ATTR_WIDE) {
				bufline[col - 2].mode |= ATTR_WRAP;
				tclearglyph(&bufline[col - 1], 0);
				ox--;
			} else {
				bufline[col - 1].mode |= ATTR_WRAP;
			}
			treflow_moveimages(oy+term.scr, ny);
			ox += col - nx;
			nx = 0;
		}
	} while (oy <= oce);
	if (nx)
		for (j = nx; j < col; j++)
			tclearglyph(&bufline[j], 0);

	/* free extra lines */
	for (i = row; i < term.row; i++)
		free(term.line[i]);
	/* resize to new height */
	term.line = xrealloc(term.line, row * sizeof(Line));

	/* free old history */
	if (term.histlimit > 0) {
		for (i = 0; i < term.histsize; i++)
			free(term.hist[i]);
		free(term.hist);
		term.hist = NULL;
		term.histf = 0;
		term.histi = -1;
		term.histsize = 0;
	}

	buflen = MIN(ny + 1, nlines);
	bot = MIN(ny, row - 1);
	scr = MAX(row - term.row, 0);
	/* update y coordinate of cursor line end */
	nce = MIN(oce + scr, bot);
	/* update cursor y coordinate */
	term.c.y = nce - (ny - cy);
	if (term.c.y < 0) {
		j = nce, nce = MIN(nce + -term.c.y, bot);
		term.c.y += nce - j;
		while (term.c.y < 0) {
			free(buf[ny-- % nlines]);
			buflen--;
			term.c.y++;
		}
	}
	/* allocate new rows */
	for (i = row - 1; i > nce; i--) {
		if (i >= term.row)
			term.line[i] = xmalloc(col * sizeof(Glyph));
		else
			term.line[i] = xrealloc(term.line[i], col * sizeof(Glyph));
		for (j = 0; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* fill visible area */
	for (/*i = nce */; i >= term.row; i--, ny--, buflen--)
		term.line[i] = buf[ny % nlines];
	for (/*i = term.row - 1 */; i >= 0; i--, ny--, buflen--) {
		free(term.line[i]);
		term.line[i] = buf[ny % nlines];
	}
	/* fill lines in history buffer */
	if (term.histlimit > 0) {
		if (buflen > 0) {
			term.histsize = MIN(buflen, term.histlimit);
			term.hist = xmalloc(term.histsize * sizeof(*term.hist));
			for (i = term.histsize-1; i >= 0; i--, ny--, buflen--)
				term.hist[i] = buf[ny % nlines];
			term.histf = term.histsize;
			term.histi = term.histsize-1;
		} else {
			increasehistorysize(MIN_HISTSIZE, col);
		}
		term.scr = MIN(term.scr, term.histf);
	}

	/* move images to the final position */
	for (im = term.images; im; im = next) {
		next = im->next;
		if (im->reflow_y == INT_MIN) {
			delete_image(im);
		} else {
			im->y = im->reflow_y - term.histf + term.scr - (ny + 1);
			if (im->y - term.scr < -term.histsize || im->y - term.scr >= row)
				delete_image(im);
		}
	}

	/* expand images into new text cells */
	for (im = term.images; im; im = im->next) {
		j = MIN(im->x + im->cols, col);
		line = TLINE(im->y);
		for (i = im->x; i < j; i++) {
			if (!(line[i].mode & ATTR_SET))
				line[i].extra |= EXT_SIXEL;
		}
	}

	/* free lines that didn't fit in history buffer */
	for (; buflen > 0; ny--, buflen--)
		free(buf[ny % nlines]);
	free(buf);
}

void
rscrolldown(int n)
{
	int i;
	Line temp;

	/* can never be true as of now
	if (IS_SET(MODE_ALTSCREEN))
		return; */

	if ((n = MIN(n, term.histf)) <= 0)
		return;

	for (i = term.c.y + n; i >= n; i--) {
		temp = term.line[i];
		term.line[i] = term.line[i-n];
		term.line[i-n] = temp;
	}
	for (/*i = n - 1 */; i >= 0; i--) {
		temp = term.line[i];
		term.line[i] = term.hist[term.histi];
		term.hist[term.histi] = temp;
		term.histi = (term.histi - 1 + term.histsize) % term.histsize;
	}
	term.c.y += n;
	term.histf -= n;
	if ((i = term.scr - n) >= 0) {
		term.scr = i;
	} else {
		scroll_images(n - term.scr);
		term.scr = 0;
		if (sel.ob.x != -1 && !sel.alt)
			selmove(-i);
	}
}

void
tresize(int col, int row)
{
	int *bp;

	restoremousecursor();

	/* col and row are always MAX(_, n)
	if (col < 2 || row < 1) {
		fprintf(stderr, "tresize: error resizing to %dx%d\n", col, row);
		return;
	} */

	if (row != term.row || col != term.col)
		win.mode ^= kbds_keyboardhandler(XK_Escape, NULL, 0, 1);

	term.dirty = xrealloc(term.dirty, row * sizeof(*term.dirty));
	term.dirtyimg = xrealloc(term.dirtyimg, row * sizeof(*term.dirtyimg));
	term.tabs = xrealloc(term.tabs, col * sizeof(*term.tabs));
	if (col > term.col) {
		bp = term.tabs + term.col;
		memset(bp, 0, sizeof(*term.tabs) * (col - term.col));
		while (--bp > term.tabs && !*bp)
			/* nothing */ ;
		for (bp += tabspaces; bp < term.tabs + col; bp += tabspaces)
			*bp = 1;
	}

	if (IS_SET(MODE_ALTSCREEN))
		tresizealt(col, row);
	else
		tresizedef(col, row);
}

void
tresizedef(int col, int row)
{
	int i, j;

	/* return if dimensions haven't changed */
	if (term.col == col && term.row == row) {
		tfulldirt();
		return;
	}
	if (col != term.col) {
		if (!sel.alt)
			selremove();
		treflow(col, row);
	} else {
		/* slide screen up if otherwise cursor would get out of the screen */
		if (term.c.y >= row) {
			tscrollup(0, term.row - 1, term.c.y - row + 1, SCROLL_RESIZE);
			term.c.y = row - 1;
		}
		for (i = row; i < term.row; i++)
			free(term.line[i]);

		/* resize to new height */
		term.line = xrealloc(term.line, row * sizeof(Line));
		/* allocate any new rows */
		for (i = term.row; i < row; i++) {
			term.line[i] = xmalloc(col * sizeof(Glyph));
			for (j = 0; j < col; j++)
				tclearglyph(&term.line[i][j], 0);
		}
		/* scroll down as much as height has increased */
		rscrolldown(row - term.row);
	}
	/* update terminal size */
	term.col = col, term.row = row;
	/* reset scrolling region */
	term.top = 0, term.bot = row - 1;
	/* dirty all lines */
	tfulldirt();
}

void
tresizealt(int col, int row)
{
	int i, j;
	ImageList *im, *next;

	/* return if dimensions haven't changed */
	if (term.col == col && term.row == row) {
		tfulldirt();
		return;
	}
	if (sel.alt)
		selremove();
	/* slide screen up if otherwise cursor would get out of the screen */
	for (i = 0; i <= term.c.y - row; i++)
		free(term.line[i]);
	if (i > 0) {
		/* ensure that both src and dst are not NULL */
		memmove(term.line, term.line + i, row * sizeof(Line));
		scroll_images(-i);
		term.c.y = row - 1;
	}
	for (i += row; i < term.row; i++)
		free(term.line[i]);
	/* resize to new height */
	term.line = xrealloc(term.line, row * sizeof(Line));
	/* resize to new width */
	for (i = 0; i < MIN(row, term.row); i++) {
		term.line[i] = xrealloc(term.line[i], col * sizeof(Glyph));
		for (j = term.col; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* allocate any new rows */
	for (/*i = MIN(row, term.row) */; i < row; i++) {
		term.line[i] = xmalloc(col * sizeof(Glyph));
		for (j = 0; j < col; j++)
			tclearglyph(&term.line[i][j], 0);
	}
	/* update cursor */
	if (term.c.x >= col) {
		term.c.state &= ~CURSOR_WRAPNEXT;
		term.c.x = col - 1;
	} else {
		UPDATEWRAPNEXT(1, col);
	}
	/* update terminal size */
	term.col = col, term.row = row;
	/* reset scrolling region */
	term.top = 0, term.bot = row - 1;

	/* delete or clip images if they are not inside the screen */
	for (im = term.images; im; im = next) {
		next = im->next;
		if (im->x >= term.col || im->y >= term.row || im->y < 0) {
			delete_image(im);
		} else {
			if ((im->cols = MIN(im->x + im->cols, term.col) - im->x) <= 0)
				delete_image(im);
		}
	}

	/* dirty all lines */
	tfulldirt();
}

void
resettitle(void)
{
	xsettitle(NULL, 0);
}

void
drawregion(int x1, int y1, int x2, int y2)
{
	int y;

	for (y = y1; y < y2; y++) {
		if (!term.dirty[y])
			continue;

		term.dirty[y] = 0;
		xdrawline(TLINE(y), x1, y, x2);
	}
}

#include "patch/st_include.c"

void
draw(void)
{
	int cx = term.c.x, cy;

	if (!xstartdraw())
		return;

	/* adjust cursor position */
	LIMIT(term.ocx, 0, term.col-1);
	LIMIT(term.ocy, 0, term.row-1);
	if (term.line[term.ocy][term.ocx].mode & ATTR_WDUMMY)
		term.ocx--;
	if (term.line[term.c.y][cx].mode & ATTR_WDUMMY)
		cx--;

	if (!kbds_drawcursor()) {
		xdrawcursor(cx, term.c.y, term.line[term.c.y][cx],
		            term.ocx, term.ocy, term.line[term.ocy]);
	}
	drawregion(0, 0, term.col, term.row);
	drawhyperlinkhint();

	term.ocx = cx;
	term.ocy = term.c.y;
	xfinishdraw();

	if (kbds_getcursor(&cx, &cy))
		xximspot(cx, cy);
	else
		xximspot(term.ocx, term.ocy);
}

void
redraw(void)
{
	tsync_end();
	tfulldirt();
	draw();
}
