/* See LICENSE for license details. */
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <Imlib2.h>

char *argv0;
#include "arg.h"
#include "st.h"
#include "win.h"
#include "sixel.h"

/* X modifiers */
#define XK_ANY_MOD    UINT_MAX
#define XK_NO_MOD     0
#define XK_SWITCH_MOD (1<<13|1<<14)

/* max number of fallback fonts in .Xresources */
#define FONT2_XRESOURCES_SIZE 8

/* function definitions used in config.h */
static void clipcopy(const Arg *);
static void clippaste(const Arg *);
static void numlock(const Arg *);
static void selpaste(const Arg *);
static void ttysend(const Arg *);
static void zoom(const Arg *);
static void zoomabs(const Arg *);
static void zoomreset(const Arg *);
static void scrolltoprompt(const Arg *);
static void copylinktoclipboard(const Arg *arg) { /* proxy for copyUrlOnClick() */ }

char *font2_xresources[FONT2_XRESOURCES_SIZE];

#include "patch/st_include.h"
#include "patch/x_include.h"

/* config.h for applying patches and the configuration. */
#include "config.h"

#if !DISABLE_LIGATURES
uint hbfeaturecount = sizeof(hbfeatures) / sizeof(hb_feature_t);
#endif

/* size of title stack */
#define TITLESTACKSIZE 8

/* XEMBED messages */
#define XEMBED_FOCUS_IN  4
#define XEMBED_FOCUS_OUT 5

/* macros */
#define IS_SET(flag)		((win.mode & (flag)) != 0)
#define TRUERED(x)		(((x) & 0xff0000) >> 8)
#define TRUEGREEN(x)		(((x) & 0xff00))
#define TRUEBLUE(x)		(((x) & 0xff) << 8)

static inline ushort sixd_to_16bit(int);
#if !DISABLE_LIGATURES
static inline void xresetfontsettings(Mode mode, Font **font, int *frcflags);
static int xmakeglyphfontspecs_ligatures(XftGlyphFontSpec *, const Glyph *, int, int, int);
static inline void xdrawline_ligatures(Line, int, int, int);
#endif
static int xmakeglyphfontspecs_noligatures(GlyphFontSeq *, XftGlyphFontSpec *, const Glyph *, int, int, int);
static inline void xdrawline_noligatures(Line, int, int, int);
static void xdrawglyphfontspecs(const XftGlyphFontSpec *, GlyphFontSeq *, int, int);
static void xclear(int, int, int, int);
static int xgeommasktogravity(int);
static int ximopen(Display *);
static void ximinstantiate(Display *, XPointer, XPointer);
static void ximdestroy(XIM, XPointer, XPointer);
static int xicdestroy(XIC, XPointer, XPointer);
static void xinit(int, int);
static void cresize(int, int);
static void xresize(int, int);
static void xhints(void);
static int xloadcolor(int, const char *, Color *);
static FcPattern *xcreatefontpattern(const char *, double);
static int xloadfont(Font *, FcPattern *);
static void xloadfonts(const char *, double);
static void xunloadfont(Font *);
static void xunloadfonts(void);
static void xsetenv(void);
static void xseturgency(int);
static inline void lerpvisualbellcolor(Color *, Color *);
static void drawscrollbackindicator(void);
static int drawunderline(Glyph *, Color *, int, int, int, int, int);
static int evcol(XEvent *);
static int evrow(XEvent *);

static void expose(XEvent *);
static void visibility(XEvent *);
static void unmap(XEvent *);
static void kpress(XEvent *);
static void cmessage(XEvent *);
static void focus(XEvent *);
static uint buttonmask(uint);
static void brelease(XEvent *);
static void bpress(XEvent *);
static void bmotion(XEvent *);
static void propnotify(XEvent *);
static void selnotify(XEvent *);
static void selclear_(XEvent *);
static void selrequest(XEvent *);
static void setsel(char *, Time);
static void sigusr1_reload(int sig);
static int mouseaction(XEvent *, uint);
static void mousesel(XEvent *, int);
static void mousereport(XEvent *);
static char *kmap(KeySym, uint);
static int match(uint, uint);

static void run(void);
static void usage(void);

static void (*handler[LASTEvent])(XEvent *) = {
	[KeyPress] = kpress,
	[ClientMessage] = cmessage,
	[VisibilityNotify] = visibility,
	[UnmapNotify] = unmap,
	[Expose] = expose,
	[FocusIn] = focus,
	[FocusOut] = focus,
	[MotionNotify] = bmotion,
	[ButtonPress] = bpress,
	[ButtonRelease] = brelease,
/*
 * Uncomment if you want the selection to disappear when you select something
 * different in another window.
 */
/*	[SelectionClear] = selclear_, */
	[SelectionNotify] = selnotify,
/*
 * PropertyNotify is only turned on when there is some INCR transfer happening
 * for the selection retrieval.
 */
	[PropertyNotify] = propnotify,
	[SelectionRequest] = selrequest,
};

/* Globals */
Term term;
DC dc;
XWindow xw;
XSelection xsel;
TermWindow win;

static int tstki; /* title stack index */
static char *titlestack[TITLESTACKSIZE]; /* title stack */

/* Font Ring Cache */
enum {
	FRC_NORMAL,
	FRC_ITALIC,
	FRC_BOLD,
	FRC_ITALICBOLD
};

typedef struct {
	XftFont *font;
	int flags;
	Rune unicodep;
} Fontcache;

typedef enum {
	SCROLL_UP,
	SCROLL_DOWN,
} ScrollDirection;

typedef struct {
	int col;
	int row;
	int isscrolling;
	int isbutton1pressed;
	int seltype;
	double timeout;
	ScrollDirection dir;
} Autoscroller;

struct {
	int active;
	int reverse;
	int frame;
	int frames;
	double frametime;
	double timeout;
	struct timespec firstbell;
	struct timespec lastbell;
} visualbell;

/* Fontcache is an array now. A new font will be appended to the array. */
static Fontcache *frc = NULL;
static int frclen = 0;
static int frccap = 0;
static char *usedfont = NULL;
static double usedfontsize = 0;
static double defaultfontsize = 0;

static char *opt_alpha = NULL;
static char *opt_class = NULL;
static char **opt_cmd  = NULL;
static char *opt_embed = NULL;
static char *opt_font  = NULL;
static char *opt_io    = NULL;
static char *opt_line  = NULL;
static char *opt_name  = NULL;
static char *opt_title = NULL;
static char *opt_permanent_title = NULL;
static char *opt_dir   = NULL;
static int opt_geometry_cols;
static int opt_geometry_rows;
static int opt_fullscreen;
static int opt_borderpx = -1;
static int opt_borderperc = -1;

static int focused = 0;

static float alpha_def;
static float alphaUnfocused_def;

static uint buttons; /* bit field of pressed buttons */
static XColor xmousefg, xmousebg;
static int cursorblinks;
static Autoscroller asr;

extern int tinsync(uint);
extern int ttyread_pending(void);

#include "patch/x_include.c"

void
clipcopy(const Arg *dummy)
{
	Atom clipboard;

	free(xsel.clipboard);
	xsel.clipboard = NULL;

	if (xsel.primary != NULL) {
		xsel.clipboard = xstrdup(xsel.primary);
		clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
		XSetSelectionOwner(xw.dpy, clipboard, xw.win, CurrentTime);
	}
}

void
clippaste(const Arg *dummy)
{
	Atom clipboard;

	if (IS_SET(MODE_KBDSELECT) && !kbds_issearchmode())
		return;

	clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
	XConvertSelection(xw.dpy, clipboard, xsel.xtarget, clipboard,
			xw.win, CurrentTime);
}

void
numlock(const Arg *dummy)
{
	win.mode ^= MODE_NUMLOCK;
}

void
selpaste(const Arg *dummy)
{
	if (IS_SET(MODE_KBDSELECT) && !kbds_issearchmode())
		return;

	XConvertSelection(xw.dpy, XA_PRIMARY, xsel.xtarget, XA_PRIMARY,
			xw.win, CurrentTime);
}

void
ttysend(const Arg *arg)
{
	ttywrite(arg->s, strlen(arg->s), 1);
}

void
zoom(const Arg *arg)
{
	Arg larg;

	larg.f = usedfontsize + arg->f;
	if (larg.f >= 1.0)
		zoomabs(&larg);
}

void
zoomabs(const Arg *arg)
{
	int i;
	ImageList *im;

	xunloadfonts();
	xloadfonts(usedfont, arg->f);
	xloadsparefonts();

	/* delete old pixmaps so that xfinishdraw() can create new scaled ones */
	for (im = term.images, i = 0; i < 2; i++, im = term.images_alt) {
		for (; im; im = im->next) {
			if (im->pixmap)
				XFreePixmap(xw.dpy, (Drawable)im->pixmap);
			if (im->clipmask)
				XFreePixmap(xw.dpy, (Drawable)im->clipmask);
			im->pixmap = NULL;
			im->clipmask = NULL;
		}
	}

	cresize(0, 0);
	redraw();
	xhints();
}

void
zoomreset(const Arg *arg)
{
	Arg larg;

	if (defaultfontsize > 0) {
		larg.f = defaultfontsize;
		zoomabs(&larg);
	}
}

void
scrolltoprompt(const Arg *arg)
{
	int x, y;
	int top = term.scr - term.histf;
	int bot = term.scr + term.row-1;
	int dy = arg->i;
	Line line;

	if (!dy || tisaltscr())
		return;

	for (y = dy; y >= top && y <= bot; y += dy) {
		for (line = TLINE(y), x = 0; x < term.col; x++) {
			if (line[x].extra & EXT_FTCS_PROMPT1_START)
				goto scroll;
		}
	}

scroll:
	if (dy < 0)
		kscrollup(&((Arg){ .i = -y }));
	else
		kscrolldown(&((Arg){ .i = y }));
}

void
lerpvisualbellcolor(Color *col, Color *result)
{
	XRenderColor tmp;
	Color *bell = &dc.col[visualbellcolor];
	int frame = visualbell.frame, frames = visualbell.frames;

	tmp.red =   ILERP(bell->color.red,   col->color.red,   frame, frames);
	tmp.green = ILERP(bell->color.green, col->color.green, frame, frames);
	tmp.blue =  ILERP(bell->color.blue,  col->color.blue,  frame, frames);
	tmp.alpha = ILERP(bell->color.alpha, col->color.alpha, frame, frames);
	XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &tmp, result);
}

int
evcol(XEvent *e)
{
	int x = e->xbutton.x - borderpx;
	LIMIT(x, 0, win.tw - 1);
	return x / win.cw;
}

int
evrow(XEvent *e)
{
	int y = e->xbutton.y - borderpx;
	LIMIT(y, 0, win.th - 1);
	return y / win.ch;
}

uint
buttonmask(uint button)
{
	return button == Button1 ? Button1Mask
		: button == Button2 ? Button2Mask
		: button == Button3 ? Button3Mask
		: button == Button4 ? Button4Mask
		: button == Button5 ? Button5Mask
		: 0;
}

int
mouseaction(XEvent *e, uint release)
{
	MouseShortcut *ms;
	int screen = tisaltscr() ? S_ALT : S_PRI;

	/* ignore Button<N>mask for Button<N> - it's set on release */
	uint state = e->xbutton.state & ~buttonmask(e->xbutton.button);

	for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
		if (ms->release == release &&
				ms->button == e->xbutton.button &&
				(!ms->screen || (ms->screen == screen)) &&
				(match(ms->mod, state) ||  /* exact or forced */
				 match(ms->mod, state & ~forcemousemod))) {
			if (ms->func == copylinktoclipboard)
				copyUrlOnClick(evcol(e), evrow(e));
			else
				ms->func(&(ms->arg));
			return 1;
		}
	}

	return 0;
}

void
mousesel(XEvent *e, int done)
{
	int dy, bot = term.row * win.ch + borderpx - 1;
	int type, seltype = SEL_REGULAR;
	uint state = e->xbutton.state & ~(Button1Mask | forcemousemod);

	if (kbds_isselectmode())
		return;

	for (type = 1; type < LEN(selmasks); ++type) {
		if (match(selmasks[type], state)) {
			seltype = type;
			break;
		}
	}

	if (asr.isbutton1pressed && (e->xbutton.y < borderpx || e->xbutton.y > bot)) {
		dy = e->xbutton.y < borderpx ? borderpx - e->xbutton.y : e->xbutton.y - bot;
		asr.timeout = MAX((double)autoscrolltimeout -
				autoscrollacceleration * (dy * dy - dy * 2 + 1),
				1.0);
		asr.dir = e->xbutton.y < borderpx ? SCROLL_UP : SCROLL_DOWN;
		asr.col = evcol(e);
		asr.row = evrow(e);
		asr.seltype = seltype;
		asr.isscrolling = 1;
	} else {
		asr.isscrolling = 0;
	}

	selextend(evcol(e), evrow(e), seltype, done);
	if (done)
		setsel(getsel(), e->xbutton.time);
}

void
mousereport(XEvent *e)
{
	int len, btn, code;
	int x = evcol(e), y = evrow(e);
	int state = e->xbutton.state;
	char buf[40];
	static int ox, oy;

	if (e->type == MotionNotify) {
		if (x == ox && y == oy)
			return;
		if (!IS_SET(MODE_MOUSEMOTION) && !IS_SET(MODE_MOUSEMANY))
			return;
		/* MODE_MOUSEMOTION: no reporting if no button is pressed */
		if (IS_SET(MODE_MOUSEMOTION) && buttons == 0)
			return;

		/* Set btn to lowest-numbered pressed button, or 12 if no
		 * buttons are pressed. */
		for (btn = 1; btn <= 11 && !(buttons & (1<<(btn-1))); btn++)
			;
		code = 32;
	} else {
		btn = e->xbutton.button;
		/* Only buttons 1 through 11 can be encoded */
		if (btn < 1 || btn > 11)
			return;
		if (e->type == ButtonRelease) {
			/* MODE_MOUSEX10: no button release reporting */
			if (IS_SET(MODE_MOUSEX10))
				return;
			/* Don't send release events for the scroll wheel */
			if (btn == 4 || btn == 5)
				return;
		}
		code = 0;
	}

	ox = x;
	oy = y;

	/* Encode btn into code. If no button is pressed for a motion event in
	 * MODE_MOUSEMANY, then encode it as a release. */
	if ((!IS_SET(MODE_MOUSESGR) && e->type == ButtonRelease) || btn == 12)
		code += 3;
	else if (btn >= 8)
		code += 128 + btn - 8;
	else if (btn >= 4)
		code += 64 + btn - 4;
	else
		code += btn - 1;

	if (!IS_SET(MODE_MOUSEX10)) {
		code += ((state & ShiftMask  ) ?  4 : 0)
		      + ((state & Mod1Mask   ) ?  8 : 0) /* meta key: alt */
		      + ((state & ControlMask) ? 16 : 0);
	}

	if (IS_SET(MODE_MOUSESGR)) {
		len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c",
				code, x+1, y+1,
				e->type == ButtonRelease ? 'm' : 'M');
	} else if (x < 223 && y < 223) {
		len = snprintf(buf, sizeof(buf), "\033[M%c%c%c",
				32+code, 32+x+1, 32+y+1);
	} else {
		return;
	}

	ttywrite(buf, len, 0);
}

void
bpress(XEvent *e)
{
	int btn = e->xbutton.button;
	struct timespec now;
	int snap;

	if (1 <= btn && btn <= 11)
		buttons |= 1 << (btn-1);

	if (btn == Button1) {
		asr.isbutton1pressed = 1;
		asr.isscrolling = 0;
		activeurl.click = 1;
		clearurl(0);
		if (url_opener_modkey != XK_ANY_MOD &&
			(e->xkey.state & url_opener_modkey) &&
			detecturl(evcol(e), evrow(e), 0)) {
			return;
		}
	}

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	if (mouseaction(e, 0))
		return;

	if (btn == Button1) {
		/*
		 * If the user clicks below predefined timeouts specific
		 * snapping behaviour is exposed.
		 */
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (TIMEDIFF(now, xsel.tclick2) <= tripleclicktimeout) {
			snap = SNAP_LINE;
		} else if (TIMEDIFF(now, xsel.tclick1) <= doubleclicktimeout) {
			snap = SNAP_WORD;
		} else {
			snap = 0;
		}
		xsel.tclick2 = xsel.tclick1;
		xsel.tclick1 = now;

		if (kbds_isselectmode())
			return;

		selstart(evcol(e), evrow(e), snap);
	}
}

void
propnotify(XEvent *e)
{
	XPropertyEvent *xpev;
	Atom clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);

	xpev = &e->xproperty;
	if (xpev->state == PropertyNewValue &&
			(xpev->atom == XA_PRIMARY ||
			 xpev->atom == clipboard)) {
		selnotify(e);
	}

}

void
selnotify(XEvent *e)
{
	ulong nitems, ofs, rem;
	int format;
	uchar *data, *last, *repl;
	Atom type, incratom, property = None;
	int append = 0;

	incratom = XInternAtom(xw.dpy, "INCR", 0);

	ofs = 0;
	if (e->type == SelectionNotify)
		property = e->xselection.property;
	else if (e->type == PropertyNotify)
		property = e->xproperty.atom;

	if (property == None)
		return;

	do {
		if (XGetWindowProperty(xw.dpy, xw.win, property, ofs,
					BUFSIZ/4, False, AnyPropertyType,
					&type, &format, &nitems, &rem,
					&data)) {
			fprintf(stderr, "Clipboard allocation failed\n");
			return;
		}

		if (e->type == PropertyNotify && nitems == 0 && rem == 0)
		{
			/*
			 * If there is some PropertyNotify with no data, then
			 * this is the signal of the selection owner that all
			 * data has been transferred. We won't need to receive
			 * PropertyNotify events anymore.
			 */
			MODBIT(xw.attrs.event_mask, 0, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
					&xw.attrs);
		}

		if (type == incratom) {
			/*
			 * Activate the PropertyNotify events so we receive
			 * when the selection owner does send us the next
			 * chunk of data.
			 */
			MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
					&xw.attrs);

			/*
			 * Deleting the property is the transfer start signal.
			 */
			XDeleteProperty(xw.dpy, xw.win, (int)property);
			continue;
		}

		if (IS_SET(MODE_KBDSELECT) && kbds_issearchmode()) {
			kbds_pasteintosearch((const char *)data, nitems * format / 8, append++);
		} else {
			/*
			 * As seen in getsel:
			 * Line endings are inconsistent in the terminal and GUI world
			 * copy and pasting. When receiving some selection data,
			 * replace all '\n' with '\r'.
			 * FIXME: Fix the computer world.
			 */
			repl = data;
			last = data + nitems * format / 8;
			while ((repl = memchr(repl, '\n', last - repl))) {
				*repl++ = '\r';
			}

			if (IS_SET(MODE_BRCKTPASTE) && ofs == 0)
				ttywrite("\033[200~", 6, 0);
			ttywrite((char *)data, nitems * format / 8, 1);
			if (IS_SET(MODE_BRCKTPASTE) && rem == 0)
				ttywrite("\033[201~", 6, 0);
		}

		XFree(data);
		/* number of 32-bit chunks returned */
		ofs += nitems * format / 32;
	} while (rem > 0);

	/*
	 * Deleting the property again tells the selection owner to send the
	 * next data chunk in the property.
	 */
	XDeleteProperty(xw.dpy, xw.win, (int)property);
}

void
xclipcopy(void)
{
	clipcopy(NULL);
}

void
selclear_(XEvent *e)
{
	selclear();
}

void
selrequest(XEvent *e)
{
	XSelectionRequestEvent *xsre;
	XSelectionEvent xev;
	Atom xa_targets, string, clipboard;
	char *seltext;

	xsre = (XSelectionRequestEvent *) e;
	xev.type = SelectionNotify;
	xev.requestor = xsre->requestor;
	xev.selection = xsre->selection;
	xev.target = xsre->target;
	xev.time = xsre->time;
	if (xsre->property == None)
		xsre->property = xsre->target;

	/* reject */
	xev.property = None;

	xa_targets = XInternAtom(xw.dpy, "TARGETS", 0);
	if (xsre->target == xa_targets) {
		/* respond with the supported type */
		string = xsel.xtarget;
		XChangeProperty(xsre->display, xsre->requestor, xsre->property,
				XA_ATOM, 32, PropModeReplace,
				(uchar *) &string, 1);
		xev.property = xsre->property;
	} else if (xsre->target == xsel.xtarget || xsre->target == XA_STRING) {
		/*
		 * xith XA_STRING non ascii characters may be incorrect in the
		 * requestor. It is not our problem, use utf8.
		 */
		clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
		if (xsre->selection == XA_PRIMARY) {
			seltext = xsel.primary;
		} else if (xsre->selection == clipboard) {
			seltext = xsel.clipboard;
		} else {
			fprintf(stderr,
				"Unhandled clipboard selection 0x%lx\n",
				xsre->selection);
			return;
		}
		if (seltext != NULL) {
			XChangeProperty(xsre->display, xsre->requestor,
					xsre->property, xsre->target,
					8, PropModeReplace,
					(uchar *)seltext, strlen(seltext));
			xev.property = xsre->property;
		}
	}

	/* all done, send a notification to the listener */
	if (!XSendEvent(xsre->display, xsre->requestor, 1, 0, (XEvent *) &xev))
		fprintf(stderr, "Error sending SelectionNotify event\n");
}

void
setsel(char *str, Time t)
{
	if (!str)
		return;

	free(xsel.primary);
	xsel.primary = str;

	XSetSelectionOwner(xw.dpy, XA_PRIMARY, xw.win, t);
	if (XGetSelectionOwner(xw.dpy, XA_PRIMARY) != xw.win)
		selclear();

	clipcopy(NULL);
}

void
sigusr1_reload(int sig)
{
	reload_config(sig);
	signal(SIGUSR1, sigusr1_reload);
}

void
xsetsel(char *str)
{
	setsel(str, CurrentTime);
}

void
brelease(XEvent *e)
{
	int btn = e->xbutton.button;

	if (1 <= btn && btn <= 11)
		buttons &= ~(1 << (btn-1));

	if (btn == Button1) {
		asr.isbutton1pressed = 0;
		asr.isscrolling = 0;
	}

	if (IS_SET(MODE_MOUSE)) {
		if (btn == Button1 && activeurl.click &&
			url_opener_modkey != XK_ANY_MOD &&
			(e->xkey.state & url_opener_modkey) &&
			detecturl(evcol(e), evrow(e), 0)) {
			openUrlOnClick(evcol(e), evrow(e), url_opener);
			return;
		} else if (!(e->xbutton.state & forcemousemod)) {
			mousereport(e);
			return;
		}
	}

	if (mouseaction(e, 1))
		return;

	if (btn == Button1) {
		mousesel(e, 1);
		if (activeurl.click && e->xkey.state & url_opener_modkey)
			openUrlOnClick(evcol(e), evrow(e), url_opener);
	}
}

void
bmotion(XEvent *e)
{
	if (!xw.pointerisvisible) {
		if (win.mode & MODE_MOUSE)
			XUndefineCursor(xw.dpy, xw.win);
		else
			XDefineCursor(xw.dpy, xw.win, xw.vpointer);
		xw.pointerisvisible = 1;
		if (!IS_SET(MODE_MOUSEMANY))
			xsetpointermotion(0);
	}
	if (!(e->xbutton.state & Button1Mask) && detecturl(evcol(e), evrow(e), 1))
		XDefineCursor(xw.dpy, xw.win, xw.upointer);
	else if (win.mode & MODE_MOUSE)
		XUndefineCursor(xw.dpy, xw.win);
	else
		XDefineCursor(xw.dpy, xw.win, xw.vpointer);
	activeurl.click = 0;

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
		mousereport(e);
		return;
	}

	mousesel(e, 0);
}

void
cresize(int width, int height)
{
	int col, row;

	if (width != 0)
		win.w = width;
	if (height != 0)
		win.h = height;

	col = (win.w - 2 * borderpx) / win.cw;
	row = (win.h - 2 * borderpx) / win.ch;
	col = MAX(2, col);
	row = MAX(1, row);

	tresize(col, row);
	xresize(col, row);
	ttyresize(win.tw, win.th);
}

void
xresize(int col, int row)
{
	win.tw = col * win.cw;
	win.th = row * win.ch;

	XFreePixmap(xw.dpy, xw.buf);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			xw.depth
	);
	XftDrawChange(xw.draw, xw.buf);
	xclear(0, 0, win.w, win.h);

	/* resize to new width */
	#if !DISABLE_LIGATURES
	xw.specbuf = xrealloc(xw.specbuf, col * sizeof(GlyphFontSpec) * 4);
	#else
	xw.specbuf = xrealloc(xw.specbuf, col * sizeof(GlyphFontSpec));
	#endif
	xw.specseq = xrealloc(xw.specseq, col * sizeof(GlyphFontSeq));
}

ushort
sixd_to_16bit(int x)
{
	return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

int
xloadcolor(int i, const char *name, Color *ncolor)
{
	XRenderColor color = { .alpha = 0xffff };

	if (!name) {
		if (BETWEEN(i, 16, 255)) { /* 256 color */
			if (i < 6*6*6+16) { /* same colors as xterm */
				color.red   = sixd_to_16bit( ((i-16)/36)%6 );
				color.green = sixd_to_16bit( ((i-16)/6) %6 );
				color.blue  = sixd_to_16bit( ((i-16)/1) %6 );
			} else { /* greyscale */
				color.red = 0x0808 + 0x0a0a * (i - (6*6*6+16));
				color.green = color.blue = color.red;
			}
			return XftColorAllocValue(xw.dpy, xw.vis,
			                          xw.cmap, &color, ncolor);
		} else if (i == defaultbg) {
			name = colorname[bg];
		} else {
			name = colorname[i];
		}
	}

	return XftColorAllocName(xw.dpy, xw.vis, xw.cmap, name, ncolor);
}

void
xloadalpha(void)
{
	float usedAlpha = (opt_alpha) ? strtof(opt_alpha, NULL)
	                              : focused || alphaUnfocused == -1 ? alpha : alphaUnfocused;
	LIMIT(usedAlpha, 0.0, 1.0);
	dc.col[defaultbg] = focused ? dc.col[bg] : dc.col[bgUnfocused];
	dc.col[defaultbg].color.alpha = (unsigned short)(0xffff * usedAlpha);
	dc.col[defaultbg].pixel &= 0x00FFFFFF;
	dc.col[defaultbg].pixel |= (unsigned int)(0xff * usedAlpha) << 24;
	dc.col[defaultbg].color.red   *= usedAlpha;
	dc.col[defaultbg].color.green *= usedAlpha;
	dc.col[defaultbg].color.blue  *= usedAlpha;
}

void
xloadcols(void)
{
	static int loaded;
	Color *cp;

	if (loaded) {
		for (cp = dc.col; cp < &dc.col[dc.collen-1]; ++cp)
			XftColorFree(xw.dpy, xw.vis, xw.cmap, cp);
	} else {
		dc.collen = 1 + defaultbg;
		dc.col = xmalloc((dc.collen) * sizeof(Color));
	}

	for (int i = 0; i+1 < dc.collen; ++i)
		if (!xloadcolor(i, NULL, &dc.col[i])) {
			if (colorname[i])
				die("could not allocate color '%s'\n", colorname[i]);
			else
				die("could not allocate color %d\n", i);
		}

	xloadalpha();
	loaded = 1;
}

int
xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b)
{
	if (!BETWEEN(x, 0, dc.collen - 1))
		return 1;

	*r = dc.col[x].color.red >> 8;
	*g = dc.col[x].color.green >> 8;
	*b = dc.col[x].color.blue >> 8;

	return 0;
}

int
xsetcolorname(int x, const char *name)
{
	Color ncolor;

	if (!BETWEEN(x, 0, dc.collen - 1))
		return 1;

	if (!xloadcolor(x, name, &ncolor))
		return 1;

	if (x == defaultbg) {
		XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[bg]);
		XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[bgUnfocused]);
		dc.col[bg] = ncolor;
		xloadcolor(bgUnfocused, name, &dc.col[bgUnfocused]);
		xloadalpha();
	} else {
		XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[x]);
		dc.col[x] = ncolor;
	}
	return 0;
}

/*
 * Absolute coordinates.
 */
void
xclear(int x1, int y1, int x2, int y2)
{
	Color bell, *bg = &dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg];

	if (visualbell.active && visualbellstyle == VISUALBELL_COLOR) {
		lerpvisualbellcolor(bg, &bell);
		bg = &bell;
	}

	XftDrawRect(xw.draw, bg, x1, y1, x2-x1, y2-y1);
}

void
xhints(void)
{
	XClassHint class = {opt_name ? opt_name : "st",
	                    opt_class ? opt_class : "St"};
	XWMHints wm = {.flags = InputHint, .input = 1};
	XSizeHints *sizeh;

	sizeh = XAllocSizeHints();

	sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
	sizeh->height = win.h;
	sizeh->width = win.w;
	sizeh->height_inc = anysize ? 1 : win.ch;
	sizeh->width_inc = anysize ? 1 : win.cw;
	sizeh->base_height = 2 * borderpx;
	sizeh->base_width = 2 * borderpx;
	sizeh->min_height = win.ch + 2 * borderpx;
	sizeh->min_width = win.cw + 2 * borderpx;
	if (xw.isfixed) {
		sizeh->flags |= PMaxSize;
		sizeh->min_width = sizeh->max_width = win.w;
		sizeh->min_height = sizeh->max_height = win.h;
	}
	if (xw.gm & (XValue|YValue)) {
		sizeh->flags |= USPosition | PWinGravity;
		sizeh->x = xw.l;
		sizeh->y = xw.t;
		sizeh->win_gravity = xgeommasktogravity(xw.gm);
	}

	XSetWMProperties(xw.dpy, xw.win, NULL, NULL, NULL, 0, sizeh, &wm,
			&class);
	XFree(sizeh);
}

int
xgeommasktogravity(int mask)
{
	switch (mask & (XNegative|YNegative)) {
	case 0:
		return NorthWestGravity;
	case XNegative:
		return NorthEastGravity;
	case YNegative:
		return SouthWestGravity;
	}

	return SouthEastGravity;
}

int
ximopen(Display *dpy)
{
	XIMCallback imdestroy = { .client_data = NULL, .callback = ximdestroy };
	XICCallback icdestroy = { .client_data = NULL, .callback = xicdestroy };

	xw.ime.xim = XOpenIM(xw.dpy, NULL, NULL, NULL);
	if (xw.ime.xim == NULL)
		return 0;

	if (XSetIMValues(xw.ime.xim, XNDestroyCallback, &imdestroy, NULL))
		fprintf(stderr, "XSetIMValues: "
		                "Could not set XNDestroyCallback.\n");

	xw.ime.spotlist = XVaCreateNestedList(0, XNSpotLocation, &xw.ime.spot,
	                                      NULL);

	if (xw.ime.xic == NULL) {
		xw.ime.xic = XCreateIC(xw.ime.xim, XNInputStyle,
		                       XIMPreeditNothing | XIMStatusNothing,
		                       XNClientWindow, xw.win,
		                       XNDestroyCallback, &icdestroy,
		                       NULL);
	}
	if (xw.ime.xic == NULL)
		fprintf(stderr, "XCreateIC: Could not create input context.\n");

	return 1;
}

void
ximinstantiate(Display *dpy, XPointer client, XPointer call)
{
	if (ximopen(dpy))
		XUnregisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
		                                 ximinstantiate, NULL);
}

void
ximdestroy(XIM xim, XPointer client, XPointer call)
{
	xw.ime.xim = NULL;
	XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
	                               ximinstantiate, NULL);
	XFree(xw.ime.spotlist);
}

int
xicdestroy(XIC xim, XPointer client, XPointer call)
{
	xw.ime.xic = NULL;
	return 1;
}

FcPattern *
xcreatefontpattern(const char *fontstr, double fontsize)
{
	FcPattern *pattern;

	if (fontstr[0] == '-')
		pattern = XftXlfdParse(fontstr, False, False);
	else
		pattern = FcNameParse((const FcChar8 *)fontstr);

	if (!pattern)
		die("can't open font: %s\n", fontstr);

	if (fontsize > 1) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternDel(pattern, FC_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, (double)fontsize);
	}
	return pattern;
}

int
xloadfont(Font *f, FcPattern *pattern)
{
	FcPattern *configured;
	FcPattern *match;
	FcResult result;
	XGlyphInfo extents;
	int wantattr, haveattr;

	/*
	 * Manually configure instead of calling XftMatchFont
	 * so that we can use the configured pattern for
	 * "missing glyph" lookups.
	 */
	configured = FcPatternDuplicate(pattern);
	if (!configured)
		return 1;

	FcConfigSubstitute(NULL, configured, FcMatchPattern);
	XftDefaultSubstitute(xw.dpy, xw.scr, configured);

	match = FcFontMatch(NULL, configured, &result);
	if (!match) {
		FcPatternDestroy(configured);
		return 1;
	}

	if (!(f->match = XftFontOpenPattern(xw.dpy, match))) {
		FcPatternDestroy(configured);
		FcPatternDestroy(match);
		return 1;
	}

	if ((XftPatternGetInteger(pattern, "slant", 0, &wantattr) ==
	    XftResultMatch)) {
		/*
		 * Check if xft was unable to find a font with the appropriate
		 * slant but gave us one anyway. Try to mitigate.
		 */
		if ((XftPatternGetInteger(f->match->pattern, "slant", 0,
		    &haveattr) != XftResultMatch) || haveattr < wantattr) {
			f->badslant = 1;
			fputs("font slant does not match\n", stderr);
		}
	}

	if ((XftPatternGetInteger(pattern, "weight", 0, &wantattr) ==
	    XftResultMatch)) {
		if ((XftPatternGetInteger(f->match->pattern, "weight", 0,
		    &haveattr) != XftResultMatch) || haveattr != wantattr) {
			f->badweight = 1;
			fputs("font weight does not match\n", stderr);
		}
	}

	XftTextExtentsUtf8(xw.dpy, f->match,
		(const FcChar8 *) ascii_printable,
		strlen(ascii_printable), &extents);

	f->set = NULL;
	f->pattern = configured;

	f->ascent = f->match->ascent;
	f->descent = f->match->descent;
	f->lbearing = 0;
	f->rbearing = f->match->max_advance_width;

	f->height = f->ascent + f->descent;
	f->width = DIVCEIL(extents.xOff, strlen(ascii_printable));

	return 0;
}

void
xloadfonts(const char *fontstr, double fontsize)
{
	FcPattern *normalpattern, *boldpattern, *italicpattern, *bolditalicpattern;
	double fontval;

	normalpattern = xcreatefontpattern(fontstr, fontsize);

	if (fontsize > 1) {
		usedfontsize = fontsize;
	} else {
		if (FcPatternGetDouble(normalpattern, FC_PIXEL_SIZE, 0, &fontval) ==
				FcResultMatch) {
			usedfontsize = fontval;
		} else if (FcPatternGetDouble(normalpattern, FC_SIZE, 0, &fontval) ==
				FcResultMatch) {
			usedfontsize = -1;
		} else {
			/*
			 * Default font size is 12, if none given. This is to
			 * have a known usedfontsize value.
			 */
			FcPatternAddDouble(normalpattern, FC_PIXEL_SIZE, 12);
			usedfontsize = 12;
		}
		defaultfontsize = usedfontsize;
	}

	if (xloadfont(&dc.font, normalpattern))
		die("can't open normal font: %s\n", fontstr);

	if (usedfontsize < 0) {
		FcPatternGetDouble(dc.font.match->pattern,
		                   FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
		if (fontsize == 0)
			defaultfontsize = fontval;
	}

	/* Setting character width and height. */
	win.cw = ceilf(dc.font.width * cwscale);
	win.ch = ceilf(dc.font.height * chscale);
	win.cyo = vertcenter ? ceilf(dc.font.height * (chscale - 1.0) / 2) : 0;

	borderpx = (borderperc > 0) ? ceilf(win.cw * borderperc / 100.0) : borderpx;
	borderpx = MAX(borderpx, 0);

	/* Bold font */
	if (font_bold && font_bold[0] && !opt_font) {
		boldpattern = xcreatefontpattern(font_bold, usedfontsize);
	} else {
		boldpattern = FcPatternDuplicate(normalpattern);
		FcPatternDel(boldpattern, FC_WEIGHT);
		FcPatternAddInteger(boldpattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	}
	if (xloadfont(&dc.bfont, boldpattern))
		die("can't open bold font: %s\n",
		    font_bold && font_bold[0] ? font_bold : fontstr);

	/* Italic font */
	if (font_italic && font_italic[0] && !opt_font) {
		italicpattern = xcreatefontpattern(font_italic, usedfontsize);
	} else {
		italicpattern = FcPatternDuplicate(normalpattern);
		FcPatternDel(italicpattern, FC_SLANT);
		FcPatternAddInteger(italicpattern, FC_SLANT, FC_SLANT_ITALIC);
	}
	if (xloadfont(&dc.ifont, italicpattern))
		die("can't open italic font: %s\n",
		    font_italic && font_italic[0] ? font_italic : fontstr);

	/* Bold italic font */
	if (font_bolditalic && font_bolditalic[0] && !opt_font) {
		bolditalicpattern = xcreatefontpattern(font_bolditalic, usedfontsize);
	} else if (font_italic && font_italic[0] && !opt_font) {
		bolditalicpattern = FcPatternDuplicate(italicpattern);
		FcPatternDel(bolditalicpattern, FC_WEIGHT);
		FcPatternAddInteger(bolditalicpattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	} else {
		bolditalicpattern = FcPatternDuplicate(boldpattern);
		FcPatternDel(bolditalicpattern, FC_SLANT);
		FcPatternAddInteger(bolditalicpattern, FC_SLANT, FC_SLANT_ITALIC);
	}
	if (xloadfont(&dc.ibfont, bolditalicpattern))
		die("can't open bold italic font: %s\n",
		    font_bolditalic && font_bolditalic[0] ? font_bolditalic : fontstr);

	FcPatternDestroy(normalpattern);
	FcPatternDestroy(boldpattern);
	FcPatternDestroy(italicpattern);
	FcPatternDestroy(bolditalicpattern);
}

void
xunloadfont(Font *f)
{
	XftFontClose(xw.dpy, f->match);
	FcPatternDestroy(f->pattern);
	if (f->set)
		FcFontSetDestroy(f->set);
}

void
xunloadfonts(void)
{
	#if !DISABLE_LIGATURES
	/* Clear Harfbuzz font cache. */
	hbunloadfonts();
	#endif

	/* Free the loaded fonts in the font cache.  */
	while (frclen > 0)
		XftFontClose(xw.dpy, frc[--frclen].font);

	xunloadfont(&dc.font);
	xunloadfont(&dc.bfont);
	xunloadfont(&dc.ifont);
	xunloadfont(&dc.ibfont);
}

void
xinit(int cols, int rows)
{
	XGCValues gcvalues;
	Pixmap blankpm;
	Window parent, root;
	pid_t thispid = getpid();
	XWindowAttributes attr;
	XVisualInfo vis;

	xw.scr = XDefaultScreen(xw.dpy);

	root = XRootWindow(xw.dpy, xw.scr);
	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0)))) {
		parent = root;
		xw.depth = 32;
	} else {
		XGetWindowAttributes(xw.dpy, parent, &attr);
		xw.depth = attr.depth;
	}

	XMatchVisualInfo(xw.dpy, xw.scr, xw.depth, TrueColor, &vis);
	xw.vis = vis.visual;

	/* font */
	if (!FcInit())
		die("could not init fontconfig.\n");

	usedfont = (opt_font == NULL)? font : opt_font;
	xloadfonts(usedfont, 0);

	/* spare fonts */
	xloadsparefonts();

	/* save alpha defaults */
	alpha_def = alpha;
	alphaUnfocused_def = alphaUnfocused;

	/* colors */
	xw.cmap = XCreateColormap(xw.dpy, parent, xw.vis, None);
	xloadcols();

	/* adjust fixed window geometry */
	win.w = 2 * borderpx + cols * win.cw;
	win.h = 2 * borderpx + rows * win.ch;
	if (xw.gm & XNegative)
		xw.l += DisplayWidth(xw.dpy, xw.scr) - win.w - 2;
	if (xw.gm & YNegative)
		xw.t += DisplayHeight(xw.dpy, xw.scr) - win.h - 2;

	/* Events */
	xw.attrs.background_pixel = dc.col[defaultbg].pixel;
	xw.attrs.border_pixel = dc.col[defaultbg].pixel;
	xw.attrs.bit_gravity = NorthWestGravity;
	xw.attrs.event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask
		| ExposureMask | VisibilityChangeMask | StructureNotifyMask
		| ButtonMotionMask | ButtonPressMask | ButtonReleaseMask
		;
	xw.attrs.colormap = xw.cmap;
	xw.attrs.event_mask |= PointerMotionMask;

	xw.win = XCreateWindow(xw.dpy, root, xw.l, xw.t,
			win.w, win.h, 0, xw.depth, InputOutput,
			xw.vis, CWBackPixel | CWBorderPixel | CWBitGravity
			| CWEventMask | CWColormap, &xw.attrs);
	if (parent != root)
		XReparentWindow(xw.dpy, xw.win, parent, xw.l, xw.t);

	memset(&gcvalues, 0, sizeof(gcvalues));
	gcvalues.graphics_exposures = False;

	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h, xw.depth);
	dc.gc = XCreateGC(xw.dpy, xw.buf, GCGraphicsExposures, &gcvalues);
	XSetForeground(xw.dpy, dc.gc, dc.col[defaultbg].pixel);
	XFillRectangle(xw.dpy, xw.buf, dc.gc, 0, 0, win.w, win.h);

	/* font spec buffer */
	#if !DISABLE_LIGATURES
	xw.specbuf = xmalloc(cols * sizeof(GlyphFontSpec) * 4);
	#else
	xw.specbuf = xmalloc(cols * sizeof(GlyphFontSpec));
	#endif
	xw.specseq = xmalloc(cols * sizeof(GlyphFontSeq));

	/* Xft rendering context */
	xw.draw = XftDrawCreate(xw.dpy, xw.buf, xw.vis, xw.cmap);

	/* input methods */
	if (!ximopen(xw.dpy)) {
		XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
		                               ximinstantiate, NULL);
	}

	/* white cursor, black outline */
	xw.pointerisvisible = 1;
	xw.vpointer = XCreateFontCursor(xw.dpy, mouseshape);
	XDefineCursor(xw.dpy, xw.win, xw.vpointer);

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousefg], &xmousefg) == 0) {
		xmousefg.red   = 0xffff;
		xmousefg.green = 0xffff;
		xmousefg.blue  = 0xffff;
	}

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousebg], &xmousebg) == 0) {
		xmousebg.red   = 0x0000;
		xmousebg.green = 0x0000;
		xmousebg.blue  = 0x0000;
	}

	XRecolorCursor(xw.dpy, xw.vpointer, &xmousefg, &xmousebg);
	blankpm = XCreateBitmapFromData(xw.dpy, xw.win, &(char){0}, 1, 1);
	xw.bpointer = XCreatePixmapCursor(xw.dpy, blankpm, blankpm,
					  &xmousefg, &xmousebg, 0, 0);

	xw.upointer = XCreateFontCursor(xw.dpy, XC_hand2);

	xw.xembed = XInternAtom(xw.dpy, "_XEMBED", False);
	xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
	xw.netwmname = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
	xw.netwmiconname = XInternAtom(xw.dpy, "_NET_WM_ICON_NAME", False);
	XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);

	/* use a png-image to set _NET_WM_ICON */
	FILE* file = fopen(ICON, "r");
	if (file) {
		/* load image in rgba-format */
		const gdImagePtr icon_rgba = gdImageCreateFromPng(file);
		fclose(file);
		/* declare icon-variable which will store the image in argb-format */
		const int width  = gdImageSX(icon_rgba);
		const int height = gdImageSY(icon_rgba);
		const int icon_n = width * height + 2;
		long icon_argb[icon_n];
		/* set width and height of the icon */
		int i = 0;
		icon_argb[i++] = width;
		icon_argb[i++] = height;
		/* rgba -> argb */
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				const int pixel_rgba = gdImageGetPixel(icon_rgba, x, y);
				unsigned char *pixel_argb = (unsigned char *) &icon_argb[i++];
				pixel_argb[0] = gdImageBlue(icon_rgba, pixel_rgba);
				pixel_argb[1] = gdImageGreen(icon_rgba, pixel_rgba);
				pixel_argb[2] = gdImageRed(icon_rgba, pixel_rgba);
				/* scale alpha from 0-127 to 0-255 */
				const unsigned char alpha = 127 - gdImageAlpha(icon_rgba, pixel_rgba);
				pixel_argb[3] = alpha == 127 ? 255 : alpha * 2;
			}
		}
		gdImageDestroy(icon_rgba);
		/* set _NET_WM_ICON */
		xw.netwmicon = XInternAtom(xw.dpy, "_NET_WM_ICON", False);
		XChangeProperty(xw.dpy, xw.win, xw.netwmicon, XA_CARDINAL, 32,
				PropModeReplace, (uchar *) icon_argb, icon_n);
	}

	xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmpid, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)&thispid, 1);

	xw.netwmstate = XInternAtom(xw.dpy, "_NET_WM_STATE", False);
	xw.netwmfullscreen = XInternAtom(xw.dpy, "_NET_WM_STATE_FULLSCREEN", False);

	win.mode = MODE_NUMLOCK;
	resettitle();
	xhints();
	XMapWindow(xw.dpy, xw.win);
	XSync(xw.dpy, False);

	clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
	clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
	xsel.primary = NULL;
	xsel.clipboard = NULL;
	xsel.xtarget = XInternAtom(xw.dpy, "UTF8_STRING", 0);
	if (xsel.xtarget == None)
		xsel.xtarget = XA_STRING;

	boxdraw_xinit(xw.dpy, xw.cmap, xw.draw, xw.vis);
}

#if !DISABLE_LIGATURES
void
xresetfontsettings(Mode mode, Font **font, int *frcflags)
{
	*font = &dc.font;
	if ((mode & ATTR_ITALIC) && (mode & ATTR_BOLD)) {
		*font = &dc.ibfont;
		*frcflags = FRC_ITALICBOLD;
	} else if (mode & ATTR_ITALIC) {
		*font = &dc.ifont;
		*frcflags = FRC_ITALIC;
	} else if (mode & ATTR_BOLD) {
		*font = &dc.bfont;
		*frcflags = FRC_BOLD;
	}
}

int
xmakeglyphfontspecs_ligatures(XftGlyphFontSpec *specs, const Glyph *glyphs, int len, int x, int y)
{
	float winx = borderpx + x * win.cw, winy = borderpx + y * win.ch, xp, yp;
	Font *font = &dc.font;
	int frcflags = FRC_NORMAL;
	float runewidth = win.cw * ((glyphs[0].mode & ATTR_WIDE) ? 2.0f : 1.0f);
	Rune rune;
	FT_UInt glyphidx;
	FcResult fcres;
	FcPattern *fcpattern, *fontpattern;
	FcFontSet *fcsets[] = { NULL };
	FcCharSet *fccharset;
	int f, numspecs = 0;
	float cluster_xp, cluster_yp;
	HbTransformData shaped;

	/* Initial values. */
	xresetfontsettings(glyphs[0].mode, &font, &frcflags);
	xp = winx, yp = winy + font->ascent + win.cyo;

	/* Handle box-drawing characters */
	if (glyphs[0].mode & ATTR_BOXDRAW) {
		for (numspecs = 0; numspecs < len; numspecs++, xp += runewidth) {
			/* minor shoehorning: boxdraw uses only this ushort */
			specs[numspecs].font = font->match;
			specs[numspecs].glyph = boxdrawindex(&glyphs[numspecs]);
			specs[numspecs].x = xp;
			specs[numspecs].y = yp;
		}
		return numspecs;
	}

	/* Shape the segment. */
	hbtransform(&shaped, font->match, glyphs, 0, len);
	cluster_xp = xp, cluster_yp = yp;
	for (int code_idx = 0; code_idx < shaped.count; code_idx++) {
		int idx = shaped.glyphs[code_idx].cluster;

		if (glyphs[idx].mode & ATTR_WDUMMY)
			continue;

		/* Advance the drawing cursor if we've moved to a new cluster */
		if (code_idx > 0 && idx != shaped.glyphs[code_idx - 1].cluster) {
			xp += runewidth * (idx - shaped.glyphs[code_idx - 1].cluster);
			cluster_xp = xp;
			cluster_yp = yp;
		}

		if (shaped.glyphs[code_idx].codepoint != 0) {
			/* If symbol is found, put it into the specs. */
			specs[numspecs].font = font->match;
			specs[numspecs].glyph = shaped.glyphs[code_idx].codepoint;
			specs[numspecs].x = cluster_xp + (short)(shaped.positions[code_idx].x_offset / 64.);
			specs[numspecs].y = cluster_yp - (short)(shaped.positions[code_idx].y_offset / 64.);
			cluster_xp += shaped.positions[code_idx].x_advance / 64.;
			cluster_yp += shaped.positions[code_idx].y_advance / 64.;
			numspecs++;
		} else {
			/* If it's not found, try to fetch it through the font cache. */
			rune = glyphs[idx].u;
			for (f = 0; f < frclen; f++) {
				glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
				/* Everything correct. */
				if (glyphidx && frc[f].flags == frcflags)
					break;
				/* We got a default font for a not found glyph. */
				if (!glyphidx && frc[f].flags == frcflags
						&& frc[f].unicodep == rune) {
					break;
				}
			}

			/* Nothing was found. Use fontconfig to find matching font. */
			if (f >= frclen) {
				if (!font->set)
					font->set = FcFontSort(0, font->pattern, 1, 0, &fcres);
				fcsets[0] = font->set;

				/*
				 * Nothing was found in the cache. Now use
				 * some dozen of Fontconfig calls to get the
				 * font for one single character.
				 *
				 * Xft and fontconfig are design failures.
				 */
				fcpattern = FcPatternDuplicate(font->pattern);
				fccharset = FcCharSetCreate();

				FcCharSetAddChar(fccharset, rune);
				FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
				FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

				FcConfigSubstitute(0, fcpattern, FcMatchPattern);
				FcDefaultSubstitute(fcpattern);

				fontpattern = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);

				/* Allocate memory for the new cache entry. */
				if (frclen >= frccap) {
					frccap += 16;
					frc = xrealloc(frc, frccap * sizeof(Fontcache));
				}

				frc[frclen].font = XftFontOpenPattern(xw.dpy, fontpattern);
				if (!frc[frclen].font)
					die("XftFontOpenPattern failed seeking fallback font: %s\n",
						strerror(errno));
				frc[frclen].flags = frcflags;
				frc[frclen].unicodep = rune;

				glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

				f = frclen;
				frclen++;

				FcPatternDestroy(fcpattern);
				FcCharSetDestroy(fccharset);
			}

			specs[numspecs].font = frc[f].font;
			specs[numspecs].glyph = glyphidx;
			specs[numspecs].x = (short)xp;
			specs[numspecs].y = (short)yp;
			numspecs++;
		}
	}

	return numspecs;
}
#endif

int
xmakeglyphfontspecs_noligatures(GlyphFontSeq *seq, XftGlyphFontSpec *specs, const Glyph *glyphs, int len, int x, int y)
{
	float winx = borderpx + x * win.cw, winy = borderpx + y * win.ch, xp, yp;
	Font *font = &dc.font;
	int frcflags = FRC_NORMAL;
	float runewidth = win.cw;
	Rune rune;
	FT_UInt glyphidx;
	FcResult fcres;
	FcPattern *fcpattern, *fontpattern;
	FcFontSet *fcsets[] = { NULL };
	FcCharSet *fccharset;
	Glyph new;
	int i, f, oi = -1, numspecs = 0, numseqs = 0;

	for (i = 0, xp = winx, yp = winy + font->ascent + win.cyo; i < len; ++i, ++x) {
		/* Skip dummy wide-character spacing. */
		if (glyphs[i].mode & ATTR_WDUMMY)
			continue;

		/* Fetch current glyph. */
		new = glyphs[i];
		new.mode ^= (selected(x, y) ? ATTR_REVERSE : 0);
		rune = new.u;

		/* Determine font for glyph if different from previous glyph. */
		if (oi < 0 || ATTRCMP(seq[numseqs].base, new)) {
			if (oi >= 0) {
				seq[numseqs].charlen = i - oi;
				seq[numseqs++].numspecs = numspecs;
				specs += numspecs;
				numspecs = 0;
			}
			seq[numseqs].x = x;
			seq[numseqs].base = new;
			oi = i;
			font = &dc.font;
			frcflags = FRC_NORMAL;
			runewidth = win.cw * ((new.mode & ATTR_WIDE) ? 2.0f : 1.0f);
			if ((new.mode & ATTR_ITALIC) && (new.mode & ATTR_BOLD)) {
				font = &dc.ibfont;
				frcflags = FRC_ITALICBOLD;
			} else if (new.mode & ATTR_ITALIC) {
				font = &dc.ifont;
				frcflags = FRC_ITALIC;
			} else if (new.mode & ATTR_BOLD) {
				font = &dc.bfont;
				frcflags = FRC_BOLD;
			}
			yp = winy + font->ascent + win.cyo;
		}

		if (rune == ' ') {
			/* Improve rendering speed by not drawing spaces. */
			xp += runewidth;
			continue;
		} else if (new.mode & ATTR_BOXDRAW) {
			/* minor shoehorning: boxdraw uses only this ushort */
			glyphidx = boxdrawindex(&new);
		} else {
			/* Lookup character index with default font. */
			glyphidx = XftCharIndex(xw.dpy, font->match, rune);
		}
		if (glyphidx) {
			specs[numspecs].font = font->match;
			specs[numspecs].glyph = glyphidx;
			specs[numspecs].x = (short)xp;
			specs[numspecs].y = (short)yp;
			xp += runewidth;
			numspecs++;
			continue;
		}

		/* Fallback on font cache, search the font cache for match. */
		for (f = 0; f < frclen; f++) {
			glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
			/* Everything correct. */
			if (glyphidx && frc[f].flags == frcflags)
				break;
			/* We got a default font for a not found glyph. */
			if (!glyphidx && frc[f].flags == frcflags
					&& frc[f].unicodep == rune) {
				break;
			}
		}

		/* Nothing was found. Use fontconfig to find matching font. */
		if (f >= frclen) {
			if (!font->set)
				font->set = FcFontSort(0, font->pattern,
				                       1, 0, &fcres);
			fcsets[0] = font->set;

			/*
			 * Nothing was found in the cache. Now use
			 * some dozen of Fontconfig calls to get the
			 * font for one single character.
			 *
			 * Xft and fontconfig are design failures.
			 */
			fcpattern = FcPatternDuplicate(font->pattern);
			fccharset = FcCharSetCreate();

			FcCharSetAddChar(fccharset, rune);
			FcPatternAddCharSet(fcpattern, FC_CHARSET,
					fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

			FcConfigSubstitute(0, fcpattern,
					FcMatchPattern);
			FcDefaultSubstitute(fcpattern);

			fontpattern = FcFontSetMatch(0, fcsets, 1,
					fcpattern, &fcres);

			/* Allocate memory for the new cache entry. */
			if (frclen >= frccap) {
				frccap += 16;
				frc = xrealloc(frc, frccap * sizeof(Fontcache));
			}

			frc[frclen].font = XftFontOpenPattern(xw.dpy,
					fontpattern);
			if (!frc[frclen].font)
				die("XftFontOpenPattern failed seeking fallback font: %s\n",
					strerror(errno));
			frc[frclen].flags = frcflags;
			frc[frclen].unicodep = rune;

			glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

			f = frclen;
			frclen++;

			FcPatternDestroy(fcpattern);
			FcCharSetDestroy(fccharset);
		}

		specs[numspecs].font = frc[f].font;
		specs[numspecs].glyph = glyphidx;
		specs[numspecs].x = (short)xp;
		specs[numspecs].y = (short)yp;
		xp += runewidth;
		numspecs++;
	}

	if (oi >= 0) {
		seq[numseqs].charlen = i - oi;
		seq[numseqs++].numspecs = numspecs;
	} else if (len > 0) {
		seq[0].x = x - len;
		seq[0].base = glyphs[0];
		seq[0].base.mode ^= (selected(x - len, y) ? ATTR_REVERSE : 0);
		seq[0].charlen = len;
		seq[0].numspecs = 0;
		numseqs = 1;
	}

	return numseqs;
}

void
xdrawglyphfontspecs(const XftGlyphFontSpec *specs, GlyphFontSeq *seq, int y, int dmode)
{
	int x = seq->x;
	int winx = borderpx + x * win.cw, winy = borderpx + y * win.ch;
	int width = seq->charlen * win.cw;
	Color *fg, *bg, *temp, revfg, revbg, truefg, truebg, bellfg, bellbg;
	XRenderColor colfg, colbg;
	Mode mode = seq->base.mode;
	Glyph *base = &seq->base;

	/* Fallback on color display for attributes not supported by the font */
	if (mode & ATTR_ITALIC && mode & ATTR_BOLD) {
		if (dc.ibfont.badslant || dc.ibfont.badweight)
			base->fg = defaultattr;
	} else if ((mode & ATTR_ITALIC && dc.ifont.badslant) ||
	    (mode & ATTR_BOLD && dc.bfont.badweight)) {
		base->fg = defaultattr;
	}

	if (IS_TRUECOL(base->fg)) {
		colfg.alpha = 0xffff;
		colfg.red = TRUERED(base->fg);
		colfg.green = TRUEGREEN(base->fg);
		colfg.blue = TRUEBLUE(base->fg);
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &truefg);
		fg = &truefg;
	} else {
		fg = &dc.col[base->fg];
	}

	if (IS_TRUECOL(base->bg)) {
		colbg.alpha = 0xffff;
		colbg.green = TRUEGREEN(base->bg);
		colbg.red = TRUERED(base->bg);
		colbg.blue = TRUEBLUE(base->bg);
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &truebg);
		bg = &truebg;
	} else {
		bg = &dc.col[base->bg];
	}

	/* Change basic system colors [0-7] to bright system colors [8-15] */
	if (!bold_is_not_bright &&
	    (mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base->fg, 0, 7))
		fg = &dc.col[base->fg + 8];

	if (IS_SET(MODE_REVERSE)) {
		if (fg == &dc.col[defaultfg]) {
			fg = &dc.col[defaultbg];
		} else {
			colfg.red = ~fg->color.red;
			colfg.green = ~fg->color.green;
			colfg.blue = ~fg->color.blue;
			colfg.alpha = fg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg,
					&revfg);
			fg = &revfg;
		}

		if (bg == &dc.col[defaultbg]) {
			bg = &dc.col[defaultfg];
		} else {
			colbg.red = ~bg->color.red;
			colbg.green = ~bg->color.green;
			colbg.blue = ~bg->color.blue;
			colbg.alpha = bg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg,
					&revbg);
			bg = &revbg;
		}
	}

	if ((mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		colfg.red = fg->color.red / 2;
		colfg.green = fg->color.green / 2;
		colfg.blue = fg->color.blue / 2;
		colfg.alpha = fg->color.alpha;
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
		fg = &revfg;
	}

	if (mode & ATTR_REVERSE) {
		temp = fg;
		fg = bg;
		bg = temp;
	}

	if (mode & ATTR_BLINK && win.mode & MODE_BLINK)
		fg = bg;

	if (mode & ATTR_INVISIBLE)
		fg = bg;

	if (mode & ATTR_HIGHLIGHT) {
		fg = &dc.col[(mode & ATTR_REVERSE) ? highlightbg : highlightfg];
		bg = &dc.col[(mode & ATTR_REVERSE) ? highlightfg : highlightbg];
	}

	if (visualbell.active && visualbellstyle == VISUALBELL_COLOR) {
		lerpvisualbellcolor(fg, &bellfg);
		lerpvisualbellcolor(bg, &bellbg);
		fg = &bellfg;
		bg = &bellbg;
	}

	if (mode & ATTR_FLASH_LABEL) {
		fg = &dc.col[(mode & ATTR_REVERSE) ? flashlabelbg : flashlabelfg];
		bg = &dc.col[(mode & ATTR_REVERSE) ? flashlabelfg : flashlabelbg];
	}
	
	if (!(mode & (ATTR_HIGHLIGHT | ATTR_REVERSE | ATTR_WDUMMY | ATTR_FLASH_LABEL)) && kbds_isflashmode()) {
		fg = &dc.col[flashtextfg];
		if (base->bg != defaultbg)
			bg = &dc.col[flashtextbg];
	}

	if (dmode & DRAW_BG) {
		/* Intelligent cleaning up of the borders. */
		if (x == 0) {
			xclear(0, (y == 0)? 0 : winy, borderpx,
				winy + win.ch +
				((winy + win.ch >= borderpx + win.th)? win.h : 0));
		}
		if (winx + width >= borderpx + win.tw) {
			xclear(winx + width, (y == 0)? 0 : winy, win.w,
				((winy + win.ch >= borderpx + win.th)? win.h : (winy + win.ch)));
		}
		if (y == 0)
			xclear(winx, 0, winx + width, borderpx);
		if (winy + win.ch >= borderpx + win.th)
			xclear(winx, winy + win.ch, winx + width, win.h);

		/* Clean up the region we want to draw to. */
		XftDrawRect(xw.draw, bg, winx, winy, width, win.ch);
	}

	if (dmode & DRAW_FG) {
		if (mode & ATTR_BOXDRAW) {
			drawboxes(winx, winy, win.cw, win.ch, fg, bg, specs, seq->numspecs);
		} else if (seq->numspecs) {
			/* Render the glyphs. */
			XftDrawGlyphFontSpec(xw.draw, fg, specs, seq->numspecs);
		}

		/* Render underline */
		int url_yoffset = 2;
		int underline_thickness = (dc.font.height / undercurl_thickness_threshold) + 1;
		if (mode & ATTR_UNDERLINE || mode & ATTR_HYPERLINK)
			url_yoffset = drawunderline(base, fg, winx, winy, seq->charlen, underline_thickness, url_yoffset);

		/* Render strikethrough */
		if (mode & ATTR_STRUCK) {
			XftDrawRect(xw.draw, fg, winx, winy + win.cyo + 2 * dc.font.ascent / 3,
					width, underline_thickness);
		}

		/* underline url (openurlonclick patch) */
		if (activeurl.draw && y >= activeurl.y1 && y <= activeurl.y2)
			drawurl(fg, mode, x, y, seq->charlen, url_yoffset, underline_thickness);
	}
}

int
drawunderline(Glyph *base, Color *fg, int winx, int winy, int charlen, int lw, int url_yoffset)
{
	static GC ugc;
	static XGCValues ugcv;
	static int ugc_clip;
	Color linecolor;
	XRenderColor lcol;
	Mode mode = base->mode;
	int ascent = MIN(win.cyo + dc.font.ascent + undercurl_yoffset, win.ch - 1);
	int maxwh = MAX(win.ch - ascent, 1);
	int wh = MIN((int)(dc.font.descent * undercurl_height_scale + 0.5), maxwh);
	int wy = winy + ascent;
	int width = charlen * win.cw;
	int utype, hyperlink;

	/* Underline Color */
	if ((base->extra & (EXT_UNDERLINE_COLOR_PALETTE | EXT_UNDERLINE_COLOR_RGB)) &&
		!(mode & ATTR_BLINK && win.mode & MODE_BLINK) &&
		!(mode & ATTR_INVISIBLE)
	) {
		/* Special color for underline */
		if (base->extra & EXT_UNDERLINE_COLOR_PALETTE) {
			/* Index */
			linecolor = dc.col[base->extra & 255];
		} else {
			/* RGB */
			lcol.alpha = 0xffff;
			lcol.red = TRUERED(base->extra);
			lcol.green = TRUEGREEN(base->extra);
			lcol.blue = TRUEBLUE(base->extra);
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &lcol, &linecolor);
		}
	} else {
		/* Foreground color for underline */
		linecolor = *fg;
	}
	linecolor.pixel |= 0xff000000;

	/* Underline type */
	utype = (base->extra & UNDERLINE_TYPE_MASK) >> UNDERLINE_TYPE_SHIFT;
	hyperlink = 0;
	if (mode & ATTR_HYPERLINK) {
		if (!(mode & ATTR_UNDERLINE) || (utype == UNDERLINE_DOTTED)) {
			if (utype != UNDERLINE_DOTTED) {
				linecolor = *fg;
				linecolor.pixel |= 0xff000000;
			}
			utype = UNDERLINE_DOTTED;
			hyperlink = 1;
			wy = winy + win.cyo + dc.font.ascent + url_yoffset;
		}
	}

	if (utype != UNDERLINE_CURLY || undercurl_style != UNDERCURL_CURLY) {
		if (ugcv.foreground != linecolor.pixel || ugcv.line_width != lw) {
			ugcv.foreground = linecolor.pixel;
			ugcv.line_width = lw;
			if (ugc)
				XChangeGC(xw.dpy, ugc, GCForeground | GCLineWidth, &ugcv);
			else
				ugc = XCreateGC(xw.dpy, XftDrawDrawable(xw.draw),
					GCForeground | GCLineWidth, &ugcv);
		}
		if (utype != UNDERLINE_CURLY && ugc_clip) {
			XSetClipMask(xw.dpy, ugc, None);
			ugc_clip = 0;
		}
	}

	switch (utype) {
	case UNDERLINE_CURLY:
		switch (undercurl_style) {
		case UNDERCURL_NONE:
			break;
		case UNDERCURL_CURLY:
			undercurlcurly(&linecolor, winx, wy, wh, charlen, lw);
			break;
		case UNDERCURL_SPIKY:
			undercurlspiky(ugc, winx, wy, wh, winy, width);
			ugc_clip = 1;
			break;
		case UNDERCURL_CAPPED:
		default:
			undercurlcapped(ugc, winx, wy, wh, winy, width);
			ugc_clip = 1;
			break;
		}
		break;
	case UNDERLINE_DOTTED:
		if (!hyperlink || hyperlinkstyle)
			undercurldotted(ugc, winx, wy, lw, charlen, hyperlink);
		break;
	case UNDERLINE_DASHED:
		undercurldashed(ugc, winx, wy, lw, charlen);
		break;
	case UNDERLINE_SINGLE:
	case UNDERLINE_DOUBLE:
	default:
		XFillRectangle(xw.dpy, XftDrawDrawable(xw.draw), ugc, winx,
			winy + win.cyo + dc.font.ascent + 1, width, lw);
		if (utype == UNDERLINE_DOUBLE) {
			XFillRectangle(xw.dpy, XftDrawDrawable(xw.draw), ugc, winx,
				winy + win.cyo + dc.font.ascent + 1 + lw*2, width, lw);
		}
		url_yoffset = lw*2 + 1;
		break;
	}

	return url_yoffset;
}


void
xdrawglyph(Glyph *g, int x, int y)
{
	int charlen = (g->mode & ATTR_WIDE) ? 2 : 1;
	XRectangle r = {
		.x = borderpx + x * win.cw,
		.y = borderpx + y * win.ch,
		.width = win.cw * charlen,
		.height = win.ch
	};
	GlyphFontSeq *seq = xw.specseq;
	XftGlyphFontSpec *specs = xw.specbuf;

	#if !DISABLE_LIGATURES
	if (ligatures)
		seq->numspecs = xmakeglyphfontspecs_ligatures(specs, g, 1, x, y);
	else
	#endif
		xmakeglyphfontspecs_noligatures(seq, specs, g, 1, x, y);

	seq->x = x;
	seq->base = *g;
	seq->charlen = charlen;

	XftDrawSetClipRectangles(xw.draw, 0, 0, &r, 1);
	xdrawglyphfontspecs(specs, seq, y, DRAW_BG | DRAW_FG);
	XftDrawSetClip(xw.draw, 0);
	term.dirtyimg[y] = 1;
}

void
xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Line oline)
{
	Color drawcol;
	XRenderColor colbg;
	uint32_t tmpcol;
	static int oldcursor;
	int blink = IS_SET(MODE_CURSORBLINK);
	int hidden = IS_SET(MODE_HIDE) && !IS_SET(MODE_KBDSELECT);
	int cursor = hidden ? -1 : !IS_SET(MODE_FOCUSED) ? -2 : win.cursor;
	int cwidth = win.cw * (g.mode & ATTR_WIDE ? 2 : 1);

	/* Redraw the line where cursor was previously.
	 * It will restore the ligatures broken by the cursor. */
	if (oline && (term.dirty[oy] || oy != cy || ox != cx || oldcursor != cursor || blink)) {
		xdrawline(oline, 0, oy, term.col);
		term.dirty[oy] = 0;
	}
	oldcursor = cursor;
	activeurl.cursory = !hidden ? cy : -1;

	if (hidden)
		return;

	/* Redraw the current cursor line, if it is dirty */
	if (term.dirty[cy]) {
		xdrawline(TLINE(cy), 0, cy, term.col);
		term.dirty[cy] = 0;
	}

	/*
	 * Select the right color for the right mode.
	 */
	g.mode &= ATTR_BOLD|ATTR_ITALIC|ATTR_UNDERLINE|ATTR_STRUCK|ATTR_WIDE|ATTR_BOXDRAW|ATTR_HIGHLIGHT|ATTR_REVERSE;

	if (IS_SET(MODE_REVERSE)) {
		g.mode |= ATTR_REVERSE;
		g.mode &= ~ATTR_HIGHLIGHT;
		g.bg = defaultfg;
		if (selected(cx, cy)) {
			drawcol = dc.col[defaultcs];
			g.fg = defaultrcs;
		} else {
			drawcol = dc.col[defaultrcs];
			g.fg = defaultcs;
		}
	} else {
		if (dynamic_cursor_color) {
			if (selected(cx, cy)) {
				g.mode &= ~(ATTR_REVERSE | ATTR_HIGHLIGHT);
				g.fg = defaultfg;
				g.bg = defaultrcs;
				drawcol = dc.col[g.bg];
			} else {
				g.mode ^= (g.mode & ATTR_HIGHLIGHT) ? ATTR_REVERSE : 0;
				tmpcol = g.bg;
				g.bg = g.fg;
				g.fg = tmpcol;
				if (g.mode & ATTR_HIGHLIGHT)
					tmpcol = (g.mode & ATTR_REVERSE) ? highlightfg : highlightbg;
				else
					tmpcol = (g.mode & ATTR_REVERSE) ? g.fg : g.bg;
				if (IS_TRUECOL(tmpcol)) {
					colbg.alpha = 0xffff;
					colbg.red = TRUERED(tmpcol);
					colbg.green = TRUEGREEN(tmpcol);
					colbg.blue = TRUEBLUE(tmpcol);
					XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &drawcol);
				} else
					drawcol = dc.col[tmpcol];
			}
		} else {
			g.mode &= ~(ATTR_REVERSE | ATTR_HIGHLIGHT);
			if (selected(cx, cy)) {
				g.fg = defaultfg;
				g.bg = defaultrcs;
			} else {
				g.fg = defaultbg;
				g.bg = defaultcs;
			}
			drawcol = dc.col[g.bg];
		}
	}

	/* draw the new one */
	if (!IS_SET(MODE_FOCUSED)) {
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw,
				borderpx + cy * win.ch,
				cwidth - 1, 1);
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw,
				borderpx + cy * win.ch,
				1, win.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw + cwidth - 1,
				borderpx + cy * win.ch,
				1, win.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw,
				borderpx + (cy + 1) * win.ch - 1,
				cwidth, 1);
		term.dirtyimg[cy] = 1;
	} else if (!blink) {
		switch (win.cursor) {
		default:
		case 1: /* blinking block */
		case 2: /* steady block */
			xdrawglyph(&g, cx, cy);
			break;
		case 3: /* blinking underline */
		case 4: /* steady underline */
			XftDrawRect(xw.draw, &drawcol,
					borderpx + cx * win.cw,
					borderpx + (cy + 1) * win.ch - \
						cursorthickness,
					cwidth, cursorthickness);
			break;
		case 5: /* blinking bar */
		case 6: /* steady bar */
			XftDrawRect(xw.draw, &drawcol,
					borderpx + cx * win.cw,
					borderpx + cy * win.ch,
					cursorthickness, win.ch);
			break;
		case 7: /* blinking st cursor */
		case 8: /* steady st cursor */
			g.u = stcursor;
			xdrawglyph(&g, cx, cy);
			break;
		}
		term.dirtyimg[cy] = 1;
	}
}

void
xsetenv(void)
{
	char buf[sizeof(long) * 8 + 1];

	snprintf(buf, sizeof(buf), "%lu", xw.win);
	setenv("WINDOWID", buf, 1);
}

void
xseticontitle(char *p)
{
	XTextProperty prop;
	DEFAULT(p, opt_title);

	if (p[0] == '\0')
		p = opt_title;

	if (Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
			&prop) != Success)
		return;
	XSetWMIconName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmiconname);
	XFree(prop.value);
}

void
xsettitle(char *p, int pop)
{
	XTextProperty prop;

	free(titlestack[tstki]);
	if(opt_permanent_title) {
		titlestack[tstki] = NULL;
		p = opt_permanent_title;
	} else if (pop) {
		titlestack[tstki] = NULL;
		tstki = (tstki - 1 + TITLESTACKSIZE) % TITLESTACKSIZE;
		p = titlestack[tstki] ? titlestack[tstki] : opt_title;
	} else if (p && p[0] != '\0') {
		titlestack[tstki] = xstrdup(p);
	} else {
		titlestack[tstki] = NULL;
		p = opt_title;
	}

	if (Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
			&prop) != Success)
		return;
	XSetWMName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
	XFree(prop.value);
}

void
xpushtitle(void)
{
	int tstkin = (tstki + 1) % TITLESTACKSIZE;

	free(titlestack[tstkin]);
	titlestack[tstkin] = titlestack[tstki] ? xstrdup(titlestack[tstki]) : NULL;
	tstki = tstkin;
}

void
xfreetitlestack(void)
{
	for (int i = 0; i < LEN(titlestack); i++) {
		free(titlestack[i]);
		titlestack[i] = NULL;
	}
}

int
xstartdraw(void)
{
	return IS_SET(MODE_VISIBLE);
}

#if !DISABLE_LIGATURES
void
xdrawline_ligatures(Line line, int x1, int y1, int x2)
{
	int i, j, x, ox, numspecs, begin, end;
	Glyph new;
	GlyphFontSeq *seq = xw.specseq;
	XftGlyphFontSpec *specs = xw.specbuf;
	XRectangle r = {
		.x = borderpx,
		.y = borderpx + y1 * win.ch,
		.width = win.cw * term.col,
		.height = win.ch
	};

	/* Draw line in 2 passes: background and foreground. This way wide glyphs
	   won't get truncated (#223) */

	/* background */
	i = j = ox = 0;
	for (x = x1; x < x2; x++) {
		new = line[x];
		if (new.mode & ATTR_WDUMMY)
			continue;
		if (selected(x, y1))
			new.mode ^= ATTR_REVERSE;
		if ((i > 0) && ATTRCMP(seq[j].base, new)) {
			numspecs = 0;
			if (begin >= 0)
				numspecs = xmakeglyphfontspecs_ligatures(specs, &line[begin], end-begin+1, begin, y1);
			seq[j].charlen = x - ox;
			seq[j].numspecs = numspecs;
			xdrawglyphfontspecs(specs, &seq[j], y1, DRAW_BG);
			specs += numspecs;
			i = 0;
			j++;
		}
		if (i == 0) {
			ox = x;
			seq[j].x = ox;
			seq[j].base = new;
			begin = -1;
		}
		end = (new.u != ' ') ? x : end;
		begin = (new.u != ' ' && begin < 0) ? x : begin;
		i++;
	}
	if (i > 0) {
		numspecs = 0;
		if (begin >= 0)
			numspecs = xmakeglyphfontspecs_ligatures(specs, &line[begin], end-begin+1, begin, y1);
		seq[j].charlen = x2 - ox;
		seq[j].numspecs = numspecs;
		xdrawglyphfontspecs(specs, &seq[j], y1, DRAW_BG);
		j++;
	}

	/* Set the clipping region for text */
	XftDrawSetClipRectangles(xw.draw, 0, 0, &r, 1);

	/* foreground */
	specs = xw.specbuf;
	for (i = 0; i < j; i++) {
		if (seq[i].numspecs || (seq[i].base.mode & (ATTR_UNDERLINE|ATTR_STRUCK|ATTR_HYPERLINK)))
			xdrawglyphfontspecs(specs, &seq[i], y1, DRAW_FG);
		specs += seq[i].numspecs;
	}

	/* Reset the clipping region */
	XftDrawSetClip(xw.draw, 0);
}
#endif

void
xdrawline_noligatures(Line line, int x1, int y1, int x2)
{
	int i, numseqs;
	GlyphFontSeq *seq = xw.specseq;
	XftGlyphFontSpec *specs = xw.specbuf;
	XRectangle r = {
		.x = borderpx,
		.y = borderpx + y1 * win.ch,
		.width = win.cw * term.col,
		.height = win.ch
	};

	numseqs = xmakeglyphfontspecs_noligatures(seq, specs, &line[x1], x2 - x1, x1, y1);

	/* Draw line in 2 passes: background and foreground. This way wide glyphs
	   won't get truncated (#223) */

	/* background */
	for (i = 0; i < numseqs; i++) {
		xdrawglyphfontspecs(specs, &seq[i], y1, DRAW_BG);
		specs += seq[i].numspecs;
	}

	/* Set the clipping region for text */
	XftDrawSetClipRectangles(xw.draw, 0, 0, &r, 1);

	/* foreground */
	specs = xw.specbuf;
	for (i = 0; i < numseqs; i++) {
		if (seq[i].numspecs || (seq[i].base.mode & (ATTR_UNDERLINE|ATTR_STRUCK|ATTR_HYPERLINK)))
			xdrawglyphfontspecs(specs, &seq[i], y1, DRAW_FG);
		specs += seq[i].numspecs;
	}

	/* Reset the clipping region */
	XftDrawSetClip(xw.draw, 0);
}

void
xdrawline(Line line, int x1, int y1, int x2)
{
	#if !DISABLE_LIGATURES
	if (ligatures)
		xdrawline_ligatures(line, x1, y1, x2);
	else
	#endif
		xdrawline_noligatures(line, x1, y1, x2);

	term.dirtyimg[y1] = 1;
	kbds_drawstatusbar(y1);
}

void
xfinishdraw(void)
{
	ImageList *im, *next;
	Imlib_Image origin, scaled;
	XGCValues gcvalues;
	GC gc = NULL;
	int width, height;
	int cx, cy, del, desty, mode, x1, x2, xend;
	int bw = borderpx, bh = borderpx;
	Line line;
	Glyph g;

	if (term.images) {
		if (IS_SET(MODE_FOCUSED) && IS_SET(MODE_CURSORBLINK))
			cy = -1;
		else if (IS_SET(MODE_KBDSELECT))
			kbds_getcursor(&cx, &cy);
		else
			cx = term.c.x, cy = (!IS_SET(MODE_HIDE) && term.scr == 0) ? term.c.y : -1;
	}

	for (im = term.images; im; im = next) {
		next = im->next;

		/* do not draw or process the image, if it is not visible or
		 * the image line is not dirty */
		if (im->x >= term.col || im->y >= term.row || im->y < 0 || !term.dirtyimg[im->y])
			continue;

		/* do not draw the image on the search bar */
		if (im->y == term.row-1 && IS_SET(MODE_KBDSELECT) && kbds_issearchmode())
			continue;

		/* scale the image */
		width = MAX(im->width * win.cw / im->cw, 1);
		height = MAX(im->height * win.ch / im->ch, 1);
		if (!im->pixmap) {
			if (!(im->pixmap = (void *)XCreatePixmap(xw.dpy, xw.win, width, height, xw.depth)))
				continue;
			if (win.cw == im->cw && win.ch == im->ch) {
				XImage ximage = {
					.format = ZPixmap,
					.data = (char *)im->pixels,
					.width = im->width,
					.height = im->height,
					.xoffset = 0,
					.byte_order = sixelbyteorder,
					.bitmap_bit_order = MSBFirst,
					.bits_per_pixel = 32,
					.bytes_per_line = im->width * 4,
					.bitmap_unit = 32,
					.bitmap_pad = 32,
					.depth = xw.depth
				};
				XPutImage(xw.dpy, (Drawable)im->pixmap, dc.gc, &ximage, 0, 0, 0, 0, width, height);
				if (im->transparent)
					im->clipmask = (void *)sixel_create_clipmask((char *)im->pixels, width, height);
			} else {
				origin = imlib_create_image_using_data(im->width, im->height, (DATA32 *)im->pixels);
				if (!origin)
					continue;
				imlib_context_set_image(origin);
				imlib_image_set_has_alpha(1);
				imlib_context_set_anti_alias(im->transparent ? 0 : 1); /* anti-aliasing messes up the clip mask */
				scaled = imlib_create_cropped_scaled_image(0, 0, im->width, im->height, width, height);
				imlib_free_image_and_decache();
				if (!scaled)
					continue;
				imlib_context_set_image(scaled);
				imlib_image_set_has_alpha(1);
				XImage ximage = {
					.format = ZPixmap,
					.data = (char *)imlib_image_get_data_for_reading_only(),
					.width = width,
					.height = height,
					.xoffset = 0,
					.byte_order = sixelbyteorder,
					.bitmap_bit_order = MSBFirst,
					.bits_per_pixel = 32,
					.bytes_per_line = width * 4,
					.bitmap_unit = 32,
					.bitmap_pad = 32,
					.depth = xw.depth
				};
				XPutImage(xw.dpy, (Drawable)im->pixmap, dc.gc, &ximage, 0, 0, 0, 0, width, height);
				if (im->transparent)
					im->clipmask = (void *)sixel_create_clipmask((char *)imlib_image_get_data_for_reading_only(), width, height);
				imlib_free_image_and_decache();
			}
		}

		/* create GC */
		if (!gc) {
			memset(&gcvalues, 0, sizeof(gcvalues));
			gcvalues.graphics_exposures = False;
			gc = XCreateGC(xw.dpy, xw.win, GCGraphicsExposures, &gcvalues);
		}

		/* set the clip mask */
		desty = bh + im->y * win.ch;
		if (im->clipmask) {
			XSetClipMask(xw.dpy, gc, (Drawable)im->clipmask);
			XSetClipOrigin(xw.dpy, gc, bw + im->x * win.cw, desty);
		}

		/* draw only the parts of the image that are not erased */
		line = TLINE(im->y) + im->x;
		xend = MIN(im->x + im->cols, term.col);
		for (del = 1, x1 = im->x; x1 < xend; x1 = x2) {
			mode = line->extra & EXT_SIXEL;
			for (x2 = x1 + 1; x2 < xend; x2++) {
				if (((++line)->extra & EXT_SIXEL) != mode)
					break;
			}
			if (mode) {
				XCopyArea(xw.dpy, (Drawable)im->pixmap, xw.buf, gc,
				    (x1 - im->x) * win.cw, 0,
				    MIN((x2 - x1) * win.cw, width - (x1 - im->x) * win.cw), height,
				    bw + x1 * win.cw, desty);
				del = 0;
			}
		}
		if (im->clipmask)
			XSetClipMask(xw.dpy, gc, None);

		/* if all the parts are erased, we can delete the entire image */
		if (del && im->x + im->cols <= term.col)
			delete_image(im);

		/* Redraw the cursor if it is behind the image */
		if (cy == im->y && (line[cx-xend+1].extra & EXT_SIXEL)) {
			g = (Glyph){ .u = ' ', mode = 0, .fg = defaultfg, .bg = defaultbg, .extra = 0 };
			xdrawcursor(cx, cy, g, cx, cy, NULL);
		}
	}
	if (gc) {
		XFreeGC(xw.dpy, gc);
		memset(term.dirtyimg, 0, term.row * sizeof(*term.dirtyimg));
	}

	drawscrollbackindicator();

	XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, win.w, win.h, 0, 0);
	XSetForeground(xw.dpy, dc.gc, dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg].pixel);
}

void
drawscrollbackindicator(void)
{
	int barw, barh, barx, bary;
	Color *barcol = &dc.col[scrollbackindicatorfg];
	Color *bordercol = &dc.col[defaultbg];

	if (!scrollbackindicator || !term.scr || !term.histf || tisaltscr() ||
	    (scrollbackindicator > 1 && !IS_SET(MODE_KBDSELECT)))
		return;

	barw = win.cw / 2;
	barh = win.ch;
	barx = win.w - barw - borderpx;
	bary = win.ch * term.row - barh;
	bary -= bary * term.scr / term.histf - borderpx;
	XftDrawRect(xw.draw, bordercol, barx-1, bary-1, barw+2, barh+2);
	XftDrawRect(xw.draw, barcol, barx, bary, barw, barh);
}

void
xximspot(int x, int y)
{
	static int ox = -1, oy = -1;

	if (xw.ime.xic == NULL || (x == ox && y == oy))
		return;

	ox = x, oy = y;
	xw.ime.spot.x = borderpx + x * win.cw;
	xw.ime.spot.y = borderpx + (y + 1) * win.ch;

	XSetICValues(xw.ime.xic, XNPreeditAttributes, xw.ime.spotlist, NULL);
}

void
expose(XEvent *ev)
{
	redraw();
}

void
visibility(XEvent *ev)
{
	XVisibilityEvent *e = &ev->xvisibility;

	MODBIT(win.mode, e->state != VisibilityFullyObscured, MODE_VISIBLE);
}

void
unmap(XEvent *ev)
{
	win.mode &= ~MODE_VISIBLE;
}

void
xsetpointermotion(int set)
{
	if (!set && !xw.pointerisvisible)
		return;
	set = 1; /* keep MotionNotify event enabled */
	MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
	XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
}

void
xsetmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	if ((flags & MODE_MOUSE) && xw.pointerisvisible) {
		if (win.mode & MODE_MOUSE)
			XUndefineCursor(xw.dpy, xw.win);
		else
			XDefineCursor(xw.dpy, xw.win, xw.vpointer);
	}
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
		redraw();
}

int
xsetcursor(int cursor)
{
	if (!BETWEEN(cursor, 0, 8)) /* 7-8: st extensions */
		return 1;
	cursor = cursor ? cursor : cursorstyle;
	switch (cursorblinktimeout ? cursorblinking : 0) {
	case 0:
		win.cursor = (cursor + 1) & ~1;
		cursorblinks = 0;
		break;
	case 2:
		win.cursor = (cursor - 1) | 1;
		cursorblinks = 1;
		break;
	default:
		win.cursor = cursor;
		cursorblinks = win.cursor & 1;
		break;
	}
	return 0;
}

void
xseturgency(int add)
{
	XWMHints *h = XGetWMHints(xw.dpy, xw.win);

	MODBIT(h->flags, add, XUrgencyHint);
	XSetWMHints(xw.dpy, xw.win, h);
	XFree(h);
}

void
xbell(void)
{
	if (!(IS_SET(MODE_FOCUSED)))
		xseturgency(1);
	if (bellvolume)
		XkbBell(xw.dpy, xw.win, bellvolume, (Atom)NULL);
	if (visualbellstyle && visualbellduration) {
		clock_gettime(CLOCK_MONOTONIC, &visualbell.firstbell);
		visualbell.lastbell = visualbell.firstbell;
		visualbell.timeout = 0;
		if (visualbellstyle == VISUALBELL_COLOR && visualbellanimfps) {
			visualbell.frames = MAX((visualbellduration * visualbellanimfps + 500) / 1000, 1);
			visualbell.frametime = 1000.0 / visualbellanimfps;
			visualbell.active = 1;
		} else {
			visualbell.frames = 1;
			visualbell.frametime = visualbellduration;
			if (!visualbell.active) {
				visualbell.reverse = IS_SET(MODE_REVERSE);
				visualbell.active = 1;
			} else if (visualbellstyle == VISUALBELL_COLOR) {
				visualbell.active = 0;
				tfulldirt();
			}
		}
	}
}

void
focus(XEvent *ev)
{
	XFocusChangeEvent *e = &ev->xfocus;

	if (e->mode == NotifyGrab)
		return;

	if (ev->type == FocusIn) {
		if (xw.ime.xic)
			XSetICFocus(xw.ime.xic);
		win.mode |= MODE_FOCUSED;
		xseturgency(0);
		if (IS_SET(MODE_FOCUS))
			ttywrite("\033[I", 3, 0);
		if (!focused) {
			focused = 1;
			xloadalpha();
			tfulldirt();
		}
	} else {
		if (xw.ime.xic)
			XUnsetICFocus(xw.ime.xic);
		win.mode &= ~MODE_FOCUSED;
		if (IS_SET(MODE_FOCUS))
			ttywrite("\033[O", 3, 0);
		if (focused) {
			focused = 0;
			xloadalpha();
			tfulldirt();
		}
	}
}

int
match(uint mask, uint state)
{
	return mask == XK_ANY_MOD || mask == (state & ~ignoremod);
}

char*
kmap(KeySym k, uint state)
{
	Key *kp;
	int i;

	/* Check for mapped keys out of X11 function keys. */
	for (i = 0; i < LEN(mappedkeys); i++) {
		if (mappedkeys[i] == k)
			break;
	}
	if (i == LEN(mappedkeys)) {
		if ((k & 0xFFFF) < 0xFD00)
			return NULL;
	}

	for (kp = key; kp < key + LEN(key); kp++) {
		if (kp->k != k)
			continue;

		if (!match(kp->mask, state))
			continue;

		if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0)
			continue;
		if (IS_SET(MODE_NUMLOCK) && kp->appkey == 2)
			continue;

		if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < 0 : kp->appcursor > 0)
			continue;

		return kp->s;
	}

	return NULL;
}

void
kpress(XEvent *ev)
{
	XKeyEvent *e = &ev->xkey;
	KeySym ksym = NoSymbol;
	char *customkey;
	int len, screen;
	Rune c;
	Status status;
	Shortcut *bp;
	static char *buf;
	static int bufsize = 128; /* initial buffer size */

	if (!buf)
		buf = xmalloc(bufsize);

	if (xw.pointerisvisible && hidecursor) {
		int x = e->x - borderpx;
		int y = e->y - borderpx;
		LIMIT(x, 0, win.tw - 1);
		LIMIT(y, 0, win.th - 1);
		if (!detecturl(x / win.cw, y / win.ch, 0)) {
			XDefineCursor(xw.dpy, xw.win, xw.bpointer);
			xsetpointermotion(1);
			xw.pointerisvisible = 0;
		}
	}

	if (IS_SET(MODE_KBDLOCK))
		return;

	if (xw.ime.xic) {
		len = XmbLookupString(xw.ime.xic, e, buf, bufsize, &ksym, &status);
		if (status == XBufferOverflow) {
			bufsize = len;
			buf = xrealloc(buf, bufsize);
			len = XmbLookupString(xw.ime.xic, e, buf, bufsize, &ksym, &status);
		}
		if (status == XLookupNone)
			return;
	} else {
		len = XLookupString(e, buf, bufsize, &ksym, NULL);
	}

	screen = tisaltscr() ? S_ALT : S_PRI;

	if (IS_SET(MODE_KBDSELECT)) {
		if (kbds_issearchmode()) {
			for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
				if (ksym == bp->keysym && match(bp->mod, e->state) &&
						(!bp->screen || bp->screen == screen) &&
						(bp->func == clippaste || bp->func == selpaste)) {
					bp->func(&(bp->arg));
					return;
				}
			}
		}
		if (match(XK_NO_MOD, e->state) ||
			(XK_Shift_L | XK_Shift_R) & e->state )
			win.mode ^= kbds_keyboardhandler(ksym, buf, len, 0);
		return;
	}

	/* 1. shortcuts */
	for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
		if (ksym == bp->keysym && match(bp->mod, e->state) &&
				(!bp->screen || bp->screen == screen)) {
			bp->func(&(bp->arg));
			return;
		}
	}

	/* 2. custom keys from config.h */
	if ((customkey = kmap(ksym, e->state))) {
		ttywrite(customkey, strlen(customkey), 1);
		return;
	}

	/* 3. composed string from input method */
	if (len == 0)
		return;
	if (len == 1 && e->state & Mod1Mask) {
		if (IS_SET(MODE_8BIT)) {
			if (*buf < 0177) {
				c = *buf | 0x80;
				len = utf8encode(c, buf);
			}
		} else {
			buf[1] = buf[0];
			buf[0] = '\033';
			len = 2;
		}
	}
	ttywrite(buf, len, 1);
}

void
cmessage(XEvent *e)
{
	/*
	 * See xembed specs
	 *  http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
	 */
	if (e->xclient.message_type == xw.xembed && e->xclient.format == 32) {
		if (e->xclient.data.l[1] == XEMBED_FOCUS_IN) {
			win.mode |= MODE_FOCUSED;
			xseturgency(0);
		} else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
			win.mode &= ~MODE_FOCUSED;
		}
	} else if (e->xclient.data.l[0] == xw.wmdeletewin) {
		ttyhangup();
		exit(0);
	}
}

void
run(void)
{
	XEvent ev;
	int rev, w = win.w, h = win.h;
	fd_set rfd;
	int xfd = XConnectionNumber(xw.dpy), ttyfd, maxfd, xev, drawing;
	struct timespec seltv, *tv, now, trigger;
	struct timespec lastscroll, lastblink, cursorlastblink;
	double timeout, cursortimeout, scrolltimeout, vbelltimeout;

	/* Waiting for window mapping */
	do {
		XNextEvent(xw.dpy, &ev);
		/*
		 * This XFilterEvent call is required because of XOpenIM. It
		 * does filter out the key event and some client message for
		 * the input method too.
		 */
		if (XFilterEvent(&ev, None))
			continue;
		if (ev.type == ConfigureNotify) {
			w = ev.xconfigure.width;
			h = ev.xconfigure.height;
		}
	} while (ev.type != MapNotify);

	ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);
	cresize(w, h);

	lastscroll = (struct timespec){0};
	lastblink = (struct timespec){0};
	cursorlastblink = (struct timespec){0};

	for (timeout = -1, drawing = 0;;) {
		FD_ZERO(&rfd);
		FD_SET(xfd, &rfd);
		maxfd = xfd;

		if (!(term.hold & TTYREAD)) {
			FD_SET(ttyfd, &rfd);
			maxfd = MAX(xfd, ttyfd);
		}

		if (XPending(xw.dpy) || ttyread_pending())
			timeout = 0;  /* existing events might not set xfd */

		seltv.tv_sec = timeout / 1E3;
		seltv.tv_nsec = 1E6 * (timeout - 1E3 * seltv.tv_sec);
		tv = timeout >= 0 ? &seltv : NULL;

		if (pselect(maxfd+1, &rfd, NULL, NULL, tv, NULL) < 0) {
			if (errno == EINTR)
				continue;
			if (term.hold_at_exit && !(term.hold & TTYREAD)) {
				tsethold(TTYWRITE);
				continue;
			}
			die("select failed: %s\n", strerror(errno));
		}
		clock_gettime(CLOCK_MONOTONIC, &now);

		int ttyin = FD_ISSET(ttyfd, &rfd) || ttyread_pending();
		if (ttyin)
			ttyread();

		xev = 0, w = win.w, h = win.h;
		while (XPending(xw.dpy)) {
			XNextEvent(xw.dpy, &ev);
			if (!xev || xev == SelectionRequest)
				xev = ev.type;
			if (XFilterEvent(&ev, None))
				continue;
			if (ev.type == ConfigureNotify) {
				w = ev.xconfigure.width;
				h = ev.xconfigure.height;
			} else if (handler[ev.type]) {
				(handler[ev.type])(&ev);
			}
		}
		if (w != win.w || h != win.h)
			cresize(w, h);

		/*
		 * To reduce flicker and tearing, when new content or event
		 * triggers drawing, we first wait a bit to ensure we got
		 * everything, and if nothing new arrives - we draw.
		 * We start with trying to wait minlatency ms. If more content
		 * arrives sooner, we retry with shorter and shorter periods,
		 * and eventually draw even without idle after maxlatency ms.
		 * Typically this results in low latency while interacting,
		 * maximum latency intervals during `cat huge.txt`, and perfect
		 * sync with periodic updates from animations/key-repeats/etc.
		 */
		if (ttyin || xev) {
			if (!drawing) {
				trigger = now;
				if (xev != SelectionRequest) {
					win.mode &= ~MODE_CURSORBLINK;
					cursorlastblink = now;
				}
				drawing = 1;
			}
			timeout = (maxlatency - TIMEDIFF(now, trigger)) \
			          / maxlatency * minlatency;
			if (timeout > 0)
				continue;  /* we have time, try to find idle */
		}

		if (tinsync(su_timeout)) {
			/*
			 * on synchronized-update draw-suspension: don't reset
			 * drawing so that we draw ASAP once we can (just after
			 * ESU). it won't be too soon because we already can
			 * draw now but we skip. we set timeout > 0 to draw on
			 * SU-timeout even without new content.
			 */
			timeout = minlatency;
			continue;
		}

		/* idle detected or maxlatency exhausted -> draw */
		timeout = -1;
		if (blinktimeout && tattrset(ATTR_BLINK)) {
			timeout = blinktimeout - TIMEDIFF(now, lastblink);
			if (timeout <= 0) {
				if (-timeout > blinktimeout) /* start visible */
					win.mode |= MODE_BLINK;
				win.mode ^= MODE_BLINK;
				tsetdirtattr(ATTR_BLINK);
				lastblink = now;
				timeout = blinktimeout;
			}
		}
		if (cursorblinktimeout && cursorblinks) {
			cursortimeout = cursorblinktimeout - TIMEDIFF(now, cursorlastblink);
			if (cursortimeout <= 0) {
				win.mode ^= MODE_CURSORBLINK;
				cursorlastblink = now;
				cursortimeout = cursorblinktimeout;
			}
			timeout = (timeout >= 0) ? MIN(timeout, cursortimeout) : cursortimeout;
		}
		if (asr.isscrolling) {
			scrolltimeout = asr.timeout - TIMEDIFF(now, lastscroll);
			if (scrolltimeout <= 0) {
				if (asr.dir == SCROLL_UP)
					kscrollup(&(Arg){.i = 1});
				else
					kscrolldown(&(Arg){.i = 1});
				selextend(asr.col, asr.row, asr.seltype, 0);
				lastscroll = now;
				scrolltimeout = asr.timeout;
			}
			timeout = (timeout >= 0) ? MIN(timeout, scrolltimeout) : scrolltimeout;
		}

		if (visualbell.active && visualbell.timeout <= TIMEDIFF(now, visualbell.lastbell)) {
			visualbell.frame = TIMEDIFF(now, visualbell.firstbell) / visualbell.frametime;
			if (visualbell.frame >= visualbell.frames)
				visualbell.active = 0;
			if (visualbellstyle == VISUALBELL_INVERT) {
				rev = visualbell.active ? !IS_SET(MODE_REVERSE) : visualbell.reverse;
				MODBIT(win.mode, rev, MODE_REVERSE);
			}
			tfulldirt();
		}

		draw();
		XFlush(xw.dpy);
		drawing = 0;
		activeurl.draw = 0;

		if (visualbell.active) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			vbelltimeout = visualbell.timeout - TIMEDIFF(now, visualbell.lastbell);
			if (vbelltimeout <= 0) {
				vbelltimeout = visualbell.frametime + fmod(vbelltimeout, visualbell.frametime);
				visualbell.timeout = vbelltimeout;
				visualbell.lastbell = now;
			}
			timeout = (timeout >= 0) ? MIN(timeout, vbelltimeout) : vbelltimeout;
		}
	}
}

void
usage(void)
{
	die("usage: %s [-aivFH] [-A alpha] [-b border] [-c class]"
		" [-d path]"
		" [-f font] [-g geometry]"
	    " [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid]"
	    " [[-e] command [args ...]]\n"
	    "       %s [-aivFH] [-A alpha] [-b border] [-c class]"
		" [-d path]"
		" [-f font] [-g geometry]"
	    " [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid] -l line"
	    " [stty_args ...]\n", argv0, argv0);
}

int
main(int argc, char *argv[])
{
	char *value;
	xw.l = xw.t = 0;
	xw.isfixed = False;

	ARGBEGIN {
	case 'a':
		allowaltscreen = 0;
		break;
	case 'A':
		opt_alpha = EARGF(usage());
		break;
	case 'b':
		value = EARGF(usage());
		opt_borderpx = atoi(value);
		opt_borderperc = strchr(value, '%') ? opt_borderpx : -1;
		break;
	case 'c':
		opt_class = EARGF(usage());
		break;
	case 'd':
		opt_dir = EARGF(usage());
		break;
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		goto run;
	case 'f':
		opt_font = EARGF(usage());
		break;
	case 'F':
		opt_fullscreen = 1;
		break;
	case 'g':
		xw.gm = XParseGeometry(EARGF(usage()),
				&xw.l, &xw.t, &cols, &rows);
		opt_geometry_cols = cols;
		opt_geometry_rows = rows;
		break;
	case 'H':
		term.hold_at_exit = 1;
		break;
	case 'i':
		xw.isfixed = 1;
		break;
	case 'o':
		opt_io = EARGF(usage());
		break;
	case 'l':
		opt_line = EARGF(usage());
		break;
	case 'n':
		opt_name = EARGF(usage());
		break;
	case 't':
		opt_title = EARGF(usage());
		break;
	case 'T':
		opt_permanent_title = EARGF(usage());
		break;
	case 'w':
		opt_embed = EARGF(usage());
		break;
	case 'v':
		die("%s " VERSION "\n", argv0);
		break;
	default:
		usage();
	} ARGEND;

run:
	if (argc > 0) /* eat all remaining arguments */
		opt_cmd = argv;

	if (!opt_title)
		opt_title = (opt_line || !opt_cmd) ? "st" : opt_cmd[0];

	setlocale(LC_CTYPE, "");
	XSetLocaleModifiers("");
	signal(SIGUSR1, sigusr1_reload);
	if (!(xw.dpy = XOpenDisplay(NULL)))
		die("Can't open display\n");

	#if !DISABLE_LIGATURES
	hbcreatebuffer();
	#endif
	config_init(xw.dpy);
	sethistorylimit(scrollbacklines);
	cols = MAX(cols, 1);
	rows = MAX(rows, 1);
	defaultbg = MAX(LEN(colorname), 256);
	tnew(cols, rows);
	xinit(cols, rows);
	xsetenv();
	selinit();
	if (opt_dir && chdir(opt_dir))
		fprintf(stderr, "Can't change to working directory %s\n", opt_dir);
	if (opt_fullscreen)
		fullscreen(&((Arg) { .i = 0 }));
	inithyperlinks();
	run();

	return 0;
}
