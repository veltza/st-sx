/* See LICENSE for license details. */

#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <gd.h>

/* macros */
#define MIN(a, b)		((a) < (b) ? (a) : (b))
#define MAX(a, b)		((a) < (b) ? (b) : (a))
#define LEN(a)			(sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)	((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)		(((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)		(a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)		(x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define ATTRCMP(a, b)		(((a).mode & (~ATTR_WRAP)) != ((b).mode & (~ATTR_WRAP)) || \
				(a).fg != (b).fg || \
				(a).bg != (b).bg || \
				(((a).mode & ATTR_UNDERLINE) && (a).extra != (b).extra))
#define TIMEDIFF(t1, t2)	((t1.tv_sec-t2.tv_sec)*1000 + \
				(t1.tv_nsec-t2.tv_nsec)/1E6)
#define MODBIT(x, set, bit)	((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r,g,b)	(1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)		(1 << 24 & (x))

/* linear interpolation for integers */
#define ILERP(a, b, t, s) ((a) + ((b) - (a)) * (t) / (s))

#define UNDERLINE_COLOR_BITS     (2 + 24)
#define UNDERLINE_COLOR_MASK     ((1 << UNDERLINE_COLOR_BITS) - 1)
#define UNDERLINE_TYPE_BITS      3
#define UNDERLINE_TYPE_SHIFT     UNDERLINE_COLOR_BITS
#define UNDERLINE_TYPE_MASK      (((1 << UNDERLINE_TYPE_BITS) - 1) << UNDERLINE_TYPE_SHIFT)

enum underlinetype {
	UNDERLINE_SINGLE = 1,
	UNDERLINE_DOUBLE = 2,
	UNDERLINE_CURLY  = 3,
	UNDERLINE_DOTTED = 4,
	UNDERLINE_DASHED = 5
};

enum glyph_attribute {
	ATTR_NULL           = 0,
	ATTR_SET            = 1 << 0,
	ATTR_BOLD           = 1 << 1,
	ATTR_FAINT          = 1 << 2,
	ATTR_ITALIC         = 1 << 3,
	ATTR_UNDERLINE      = 1 << 4,
	ATTR_BLINK          = 1 << 5,
	ATTR_REVERSE        = 1 << 6,
	ATTR_INVISIBLE      = 1 << 7,
	ATTR_STRUCK         = 1 << 8,
	ATTR_WRAP           = 1 << 9,
	ATTR_WIDE           = 1 << 10,
	ATTR_WDUMMY         = 1 << 11,
	ATTR_BOXDRAW        = 1 << 12,
	ATTR_HIGHLIGHT      = 1 << 13,
	ATTR_HYPERLINK      = 1 << 14,
	ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
	ATTR_FLASH_LABEL    = 1 << 15,
};

enum extra_attribute {
	/* bits 0 to 23 are reserved for underline color */
	EXT_UNDERLINE_COLOR_RGB     = 1 << 24,
	EXT_UNDERLINE_COLOR_PALETTE = 1 << 25,
	/* bits 26 to 28 are reserved for underline type */
	EXT_FTCS_PROMPT1_START      = 1 << 29, /* OSC "133;A" - start of shell prompt PS1 */
	EXT_FTCS_PROMPT1_INPUT      = 1 << 30, /* OSC "133;B" - start of prompt input PS1 */
	EXT_SIXEL                   = 1 << 31
};

typedef struct _ImageList {
	struct _ImageList *next, *prev;
	unsigned char *pixels;
	void *pixmap;
	void *clipmask;
	int width;
	int height;
	int x;
	int y;
	int reflow_y;
	int cols;
	int cw;
	int ch;
	int transparent;
} ImageList;

enum drawing_mode {
	DRAW_NONE = 0,
	DRAW_BG   = 1 << 0,
	DRAW_FG   = 1 << 1,
};

/* Used to control which screen(s) keybindings and mouse shortcuts apply to. */
enum screen {
	S_PRI = -1, /* primary screen */
	S_ALL = 0,  /* both primary and alt screen */
	S_ALT = 1   /* alternate screen */
};

enum selection_mode {
	SEL_IDLE = 0,
	SEL_EMPTY = 1,
	SEL_READY = 2
};

enum selection_type {
	SEL_REGULAR = 1,
	SEL_RECTANGULAR = 2
};

enum selection_snap {
	SNAP_WORD = 1,
	SNAP_LINE = 2
};

enum visualbell_style {
	VISUALBELL_NONE = 0,
	VISUALBELL_COLOR = 1,
	VISUALBELL_INVERT = 2
};

enum hold_state {
	TTYREAD  = 1 << 0,
	TTYWRITE = 1 << 1
};

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef uint_least32_t Rune;
typedef ushort Mode;

typedef XftDraw *Draw;
typedef XftColor Color;
typedef XftGlyphFontSpec GlyphFontSpec;

#define Glyph Glyph_
typedef struct {
	Rune u;           /* character code */
	Rune ubk;         /* character code for flash label */
	Mode mode;        /* attribute flags */
	ushort hlink;     /* hyperlink index */
	uint32_t fg;      /* foreground  */
	uint32_t bg;      /* background  */
	uint32_t extra;   /* underline style and color, semantic prompts, sixel */
} Glyph;

typedef Glyph *Line;

typedef struct {
	int x;
	int charlen;
	int numspecs;
	Glyph base;
} GlyphFontSeq;

typedef struct {
	Glyph attr; /* current char attributes */
	int x;
	int y;
	char state;
} TCursor;

typedef struct {
	ushort *buckets;
	int capacity;
} HyperlinkHT;

typedef struct {
	char *id;
	char *url;
	ushort next;
} HyperlinkItem;

typedef struct {
	HyperlinkHT hashtable;
	HyperlinkItem *items;
	int head;
	int count;
	int capacity;
} Hyperlinks;

/* Internal representation of the screen */
typedef struct {
	int row;      /* nb row */
	int col;      /* nb col */
	Line *line;   /* screen */
	Line *hist;          /* history buffer */
	int histlimit;       /* max history size */
	int histsize;        /* current history size */
	int histf;           /* nb history available */
	int histi;           /* history index */
	int scr;             /* scroll back */
	int wrapcwidth[2];   /* used in updating WRAPNEXT when resizing */
	int *dirty;     /* dirtyness of lines */
	char *dirtyimg; /* dirtyness of image lines */
	TCursor c;    /* cursor */
	int ocx;      /* old cursor col */
	int ocy;      /* old cursor row */
	int top;      /* top    scroll limit */
	int bot;      /* bottom scroll limit */
	int mode;     /* terminal mode flags */
	int esc;      /* escape state flags */
	char trantbl[4]; /* charset table translation */
	int charset;  /* current charset */
	int icharset; /* selected charset for sequence */
	int *tabs;
	ImageList *images;     /* sixel images */
	ImageList *images_alt; /* sixel images for alternate screen */
	Hyperlinks *hyperlinks;
	Hyperlinks *hyperlinks_alt;
	Rune lastc;   /* last printed char outside of sequence, 0 if control */
	char *cwd;    /* current working directory */
	int hold_at_exit; /* remain open after child process exits */
	int hold;         /* hold state */
} Term;

typedef union {
	int i;
	uint ui;
	float f;
	const void *v;
	const char *s;
} Arg;

/* Purely graphic info */
typedef struct {
	int tw, th; /* tty width and height */
	int w, h; /* window width and height */
	int ch; /* char height */
	int cw; /* char width  */
	int cyo; /* char y offset */
	int mode; /* window state/mode flags */
	int cursor; /* cursor style */
} TermWindow;

typedef struct {
	Display *dpy;
	Colormap cmap;
	Window win;
	Drawable buf;
	GlyphFontSpec *specbuf; /* font spec buffer used for rendering */
	GlyphFontSeq *specseq;
	Atom xembed, wmdeletewin, netwmname, netwmiconname, netwmpid;
	Atom netwmstate, netwmfullscreen;
	Atom netwmicon;
	struct {
		XIM xim;
		XIC xic;
		XPoint spot;
		XVaNestedList spotlist;
	} ime;
	Draw draw;
	Visual *vis;
	XSetWindowAttributes attrs;
	/* Here, we use the term *pointer* to differentiate the cursor
	 * one sees when hovering the mouse over the terminal from, e.g.,
	 * a green rectangle where text would be entered. */
	Cursor vpointer, bpointer; /* visible and hidden pointers */
	int pointerisvisible;
	Cursor upointer;
	int scr;
	int isfixed; /* is fixed geometry? */
	int depth; /* bit depth */
	int l, t; /* left and top offset */
	int gm; /* geometry mask */
} XWindow;

typedef struct {
	Atom xtarget;
	char *primary, *clipboard;
	struct timespec tclick1;
	struct timespec tclick2;
} XSelection;

/* types used in config.h */
typedef struct {
	uint mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
	int screen;
} Shortcut;

typedef struct {
	uint mod;
	uint button;
	void (*func)(const Arg *);
	const Arg arg;
	uint release;
	int screen;
} MouseShortcut;

typedef struct {
	KeySym k;
	uint mask;
	char *s;
	/* three-valued logic variables: 0 indifferent, 1 on, -1 off */
	signed char appkey;    /* application keypad */
	signed char appcursor; /* application cursor */
} Key;

/* Font structure */
#define Font Font_
typedef struct {
	int height;
	int width;
	int ascent;
	int descent;
	int badslant;
	int badweight;
	short lbearing;
	short rbearing;
	XftFont *match;
	FcFontSet *set;
	FcPattern *pattern;
} Font;

/* Drawing Context */
typedef struct {
	Color *col;
	size_t collen;
	Font font, bfont, ifont, ibfont;
	GC gc;
} DC;

void die(const char *, ...);
void redraw(void);
void draw(void);
void drawregion(int, int, int, int);
void tfulldirt(void);

void printscreen(const Arg *);
void printsel(const Arg *);
void sendbreak(const Arg *);
void toggleprinter(const Arg *);

int tattrset(int);
int tisaltscr(void);
void tsethold(int);
void tnew(int, int);
void tresize(int, int);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);

void resettitle(void);

void selclear(void);
void selinit(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
void xyselextend(int, int, int);
int selected(int, int);
char *getsel(void);

size_t utf8decode(const char *, Rune *, size_t);
size_t utf8encode(Rune, char *);

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b);

int isboxdraw(Rune);
ushort boxdrawindex(const Glyph *);
#ifdef XFT_VERSION
/* only exposed to x.c, otherwise we'll need Xft.h for the types */
void boxdraw_xinit(Display *, Colormap, XftDraw *, Visual *);
void drawboxes(int, int, int, int, XftColor *, XftColor *, const XftGlyphFontSpec *, int);
#endif // XFT_VERSION

/* config.h globals */
extern char *utmp;
extern char *scroll;
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern wchar_t *kbds_sdelim;
extern wchar_t *kbds_ldelim;
extern int allowaltscreen;
extern int allowwindowops;
extern char *termname;
extern char *url_opener;
extern char *pattern_list[];
extern unsigned int enable_url_same_label;
extern unsigned int enable_regex_same_label;
extern unsigned int tabspaces;
extern unsigned int defaultfg;
extern unsigned int defaultbg;
extern unsigned int defaultcs;
extern unsigned int kbselectfg;
extern unsigned int kbselectbg;

extern int boxdraw, boxdraw_bold, boxdraw_braille;
extern float alpha;
extern float alphaUnfocused;

extern DC dc;
extern XWindow xw;
extern XSelection xsel;
extern TermWindow win;
extern Term term;
extern unsigned int disablehyperlinks;
extern int undercurl_style;
