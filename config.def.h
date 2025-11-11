/* See LICENSE file for copyright and license details. */

/*
 * appearance
 *
 * Normal font. See http://freedesktop.org/software/fontconfig/fontconfig-user.html
 */
static char *font = "Liberation Mono:pixelsize=12:antialias=true:autohint=true";

/* Bold font. Optional. Note: The font size is set automatically for this font. */
static char *font_bold;

/* Italic font. Optional. Note: The font size is set automatically for this font. */
static char *font_italic;

/* Bold italic font. Optional. Note: The font size is set automatically for this font. */
static char *font_bolditalic;

/* Spare fonts */
static char *font2[] = {
/*	"Inconsolata for Powerline:pixelsize=12:antialias=true:autohint=true", */
/*	"Hack Nerd Font Mono:pixelsize=11:antialias=true:autohint=true", */
};

/* pattern for regex mode,
 * use () sub-patterns to define the range to be copied*/
char *pattern_list[] = {
	// Markdown URL
	"\\[[^]]*\\]\\(([^)]+)\\)",
	// URL
	"((?:https?://|git@|git://|ssh://|ftp://|file://)\\S+)",
	// Diff a
	"(--- a/(\\S+))",
	// Diff b
	"(\\+\\+\\+ b/(\\S+))",
	// Docker
	"(sha256:([0-9a-f]{64}))",
	// File path
	"((?:[.\\pL\\pN_\\-@~]+)?(?:/+[.\\pL\\pN_\\-@]+)+)",
	// Color hex code
	"(#[0-9a-fA-F]{6})",
	// UUID
	"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})",
	// IPFS
	"(Qm[0-9a-zA-Z]{44})",
	// SHA hash
	"([0-9a-f]{7,40})",
	// IPv4 address
	"(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})",
	// IPv6 address
	"([A-f0-9:]+:+[A-f0-9:]+[%\\w\\d]+)",
	// Hexadecimal address
	"(0x[0-9a-fA-F]+)",
	// Number (at least 4 digits)
	"\\b([0-9]{4,})\\b",

	// The NULL at the end is required, please do not remove it
	NULL
};

/* use for url search when url is a same value*/
unsigned int enable_url_same_label = 0;

/* use for regex search when regex is a same value*/
unsigned int enable_regex_same_label = 1;

/* Borders in pixels */
static int borderpx = 2;

/* Borders as a percentage of the cell width. If greater than zero, overrides
 * the borderpx setting. */
static int borderperc = 0;

/* Command that is executed when a URL or hyperlink is clicked */
char *url_opener = "xdg-open";

/* List of URL protocols to search for when detecting a URL under the mouse */
char *url_protocols = "https://,http://,file:/,ftp://,mailto:,vscode://";

/* Specifies the modifier that is required to be pressed when you are clicking
 * on links. The options are ControlMask, ShiftMask and XK_ANY_MOD. */
static uint url_opener_modkey = XK_ANY_MOD;

/*
 * What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: scroll and/or utmp
 * 3: SHELL environment variable
 * 4: value of shell in /etc/passwd
 * 5: value of shell in config.h
 */
static char *shell = "/bin/sh";
char *utmp = NULL;
/* scroll program: to enable use a string like "scroll" */
char *scroll = NULL;
char *stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
char *vtiden = "\033[?62;4c"; /* VT200 family (62) with sixel (4) */

/* sixel rgb byte order: LSBFirst or MSBFirst */
int const sixelbyteorder = LSBFirst;

/* Kerning / character bounding-box multipliers */
static float cwscale = 1.0;
static float chscale = 1.0;

/* vertically center lines in the space available if you have set a larger
 * chscale in config.h or .Xresources: 0 = off, 1 = on */
int vertcenter = 0;

/*
 * word delimiter string
 *
 * More advanced example: L" `'\"()[]{}"
 */
wchar_t *worddelimiters = L" ";

/* Word delimiters for short and long jumps in the keyboard select patch */
wchar_t *kbds_sdelim = L"!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~ ";
wchar_t *kbds_ldelim = L" ";

/* selection timeouts (in milliseconds) */
static unsigned int doubleclicktimeout = 300;
static unsigned int tripleclicktimeout = 600;

/* alt screens */
int allowaltscreen = 1;

/* allow certain non-interactive (insecure) window operations such as:
   setting the clipboard text using the OSC 52 sequence */
int allowwindowops = 0;

/*
 * draw latency range in ms - from new content/keypress/etc until drawing.
 * within this range, st draws when content stops arriving (idle). mostly it's
 * near minlatency, but it waits longer for slow updates to avoid partial draw.
 * low minlatency will tear/flicker more, as it can "detect" idle too early.
 */
static float minlatency = 2;
static float maxlatency = 33;

/*
 * Synchronized-Update timeout in ms
 * https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec
 */
static unsigned int su_timeout = 200;

/*
 * Specifies how fast the screen scrolls when you select text and drag the
 * mouse to the top or bottom of the screen.
 */
static unsigned int autoscrolltimeout = 200;

/*
 * Specifies how fast the autoscroll accelerates. Set to 0 to disable the
 * acceleration.
 */
static float autoscrollacceleration = 1.0;

/*
 * blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute.
 */
static unsigned int blinktimeout = 800;

/*
 * thickness of underline and bar cursors
 */
static unsigned int cursorthickness = 2;

/* Hide the X cursor whenever a key is pressed. 0: off, 1: on */
int hidecursor = 0;

/* Ligatures. 0: off, 1: on */
int ligatures = 0;

/*
 * Permanently disable ligatures so that they won't be compiled in. You also
 * need to comment out the ligature lines in config.mk to avoid compiling and
 * linking the HarfBuzz code.
 */
#define DISABLE_LIGATURES 0

/*
 * Font features for ligatures.
 * Populate the array with a list of font features, wrapped in FEATURE macro,
 * e. g.
 * hb_feature_t hbfeatures[] = { FEATURE('c', 'a', 'l', 't'), FEATURE('d', 'l', 'i', 'g') };
 */
#if !DISABLE_LIGATURES
#include "hb.h"
hb_feature_t hbfeatures[] = { };
#endif

/*
 * 1: render most of the lines/blocks characters without using the font for
 *    perfect alignment between cells (U2500 - U259F except dashes/diagonals).
 *    Bold affects lines thickness if boxdraw_bold is not 0. Italic is ignored.
 * 0: disable (render all U25XX glyphs normally from the font).
 */
int boxdraw = 0;
int boxdraw_bold = 0;

/* braille (U28XX):  1: render as adjacent "pixels",  0: use font */
int boxdraw_braille = 0;

/* extra: if enabled, render dashes, diagonals, sextants, octants and proper
 *        rounded corners. (boxdraw has to be enabled as well) */
int boxdraw_extra = 1;

/* branch symbols (UF5D0..U+F60D). Ref. https://github.com/kovidgoyal/kitty/pull/7681 */
int boxdraw_branch = 1;

/* line thickness of the branch symbols in pixels (0 = auto) */
int boxdraw_branch_thickness = 0;

/*
 * bell volume. It must be a value between -100 and 100. Use 0 for disabling
 * it
 */
static int bellvolume = 0;

/*
 * Visual bell style: 0 = none, 1 = custom bell color, 2 = inverted colors
 */
static unsigned int visualbellstyle = 0;

/*
 * Duration of the visual bell flash in milliseconds
 */
static unsigned int visualbellduration = 100;

/*
 * Specifies how often the visual bell animation is updated. Set to 0 to disable
 * the animation. Note that:
 * - The animation only works with the custom bell color
 * - This is just a target speed as render time may vary
 * - Don't set this too high as it will increase CPU usage
 */
static unsigned int visualbellanimfps = 60;

/* default TERM value */
char *termname = "st-256color";

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
unsigned int tabspaces = 8;

/* bg opacity
 *
 * To disable the unfocused bg opacity, set alphaUnfocused=-1. This will
 * cause alpha to be used for both the focused and unfocused bg opacity.
 */
float alpha = 0.93;
float alphaUnfocused = 0.6;

/* Terminal colors (16 first used in escape sequence) */
static const char *colorname[] = {
	/* 8 normal colors */
	"black",
	"red3",
	"green3",
	"yellow3",
	"blue2",
	"magenta3",
	"cyan3",
	"gray90",

	/* 8 bright colors */
	"gray50",
	"red",
	"green",
	"yellow",
	"#5c5cff",
	"magenta",
	"cyan",
	"white",

	[255] = 0,

	/* more colors can be added after 255 to use with DefaultXX */
	"#cccccc", /* 256 -> cursor */
	"#555555", /* 257 -> rev cursor */
	"gray90",  /* 258 -> foreground */
	"black",   /* 259 -> background */
	"black",   /* 260 -> background unfocused */
	"gray90",  /* 261 -> visual bell */
};

/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor, visual bell
 */
unsigned int defaultfg = 258;
unsigned int defaultbg = 259;
unsigned int defaultcs = 256;
static unsigned int defaultrcs = 257;
unsigned int bg = 259;
unsigned int bgUnfocused = 260;
unsigned int visualbellcolor = 261;

/* Foreground and background color of search results */
unsigned int highlightfg = 15;
unsigned int highlightbg = 160;

/* Foreground and background color of flash label */
unsigned int flashlabelfg = 15;
unsigned int flashlabelbg = 4;
unsigned int flashtextfg = 8;
unsigned int flashtextbg = 0;

/* Foreground and background color of the hyperlink hint */
unsigned int hyperlinkhintfg = 0;
unsigned int hyperlinkhintbg = 258;

/* Foreground and background colors for keyboard selection mode bars */
unsigned int kbselectfg = 0;
unsigned int kbselectbg = 258;

/* Foreground color for the scrollback indicator */
unsigned int scrollbackindicatorfg = 258;

/* Bold text is not rendered in bright color. 0: off, 1: on */
unsigned int bold_is_not_bright = 1;

/*
 * Dynamic cursor color
 * 0: the cursor color is fixed (default st behavior)
 * 1: the cursor uses reverse colors based on the colors of the text cell
*/
unsigned int dynamic_cursor_color = 1;

/*
 * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps-SP-q.1D81
 * Default style of cursor
 * 1: blinking block
 * 2: steady block ("█")
 * 3: blinking underline
 * 4: steady underline ("_")
 * 5: blinking bar
 * 6: steady bar ("|")
 * 7: blinking st cursor
 * 8: steady st cursor
 */
static unsigned int cursorstyle = 2;
static Rune stcursor = 0x2603; /* snowman ("☃") */

/*
 * Controls the blinking of the cursor
 * 0: prevent the cursor from ever blinking
 * 1: allow blinking
 * 2: force to always blink
 */
static unsigned int cursorblinking = 1;

/* Specifies how fast the blinking cursor blinks */
static unsigned int cursorblinktimeout = 800;

/*
 * Default columns and rows numbers
 */
static unsigned int cols = 80;
static unsigned int rows = 24;

/* Allow resizing the window to any pixel size: 0 = off, 1 = on */
unsigned int anysize = 1;

/* Disable hyperlinks (OSC 8) */
unsigned int disablehyperlinks = 0;

/* Hyperlink style: 0 = none, 1 = dotted underline */
unsigned int hyperlinkstyle = 1;

/* Show a hyperlink hint at the bottom of the screen */
unsigned int showhyperlinkhint = 1;

/* Specifies how many hyperlinks can be cached on the primary screen and the
 * scrollback. When the cache is full, the oldest hyperlinks will be thrown
 * away. Default value is 8192, maximum value is 65535. */
unsigned int hyperlinkcache_pri = 8192;

/* Specifies how many hyperlinks can be cached on the alternate screen.
 * Default value is 1024, maximum value is 65535. */
unsigned int hyperlinkcache_alt = 1024;

/* Show a scrollback indicator on the right edge of the screen to indicate how far
 * you have scrolled. 0: off, 1: on, 2: only when keyboard selection mode is on */
unsigned int scrollbackindicator = 1;

/* Maximum number of lines in the scrollback buffer. Limited to 100000. */
unsigned int scrollbacklines = 2000;

/*
 * Default colour and shape of the mouse cursor
 */
static unsigned int mouseshape = XC_xterm;
static unsigned int mousefg = 7;
static unsigned int mousebg = 0;

/*
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
static unsigned int defaultattr = 11;

/*
 * Force mouse select/shortcuts while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use ShiftMask with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static uint forcemousemod = ShiftMask;

/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
static MouseShortcut mshortcuts[] = {
	/* mask                 button   function        argument       release  screen */
	{ XK_ANY_MOD,           Button2, clippaste,      {.i = 0},      1 },
	{ ShiftMask,            Button4, kscrollup,      {.i = 1},      0, S_PRI},
	{ ShiftMask,            Button5, kscrolldown,    {.i = 1},      0, S_PRI},
	{ XK_NO_MOD,            Button4, kscrollup,      {.i = 1},      0, S_PRI },
	{ XK_NO_MOD,            Button5, kscrolldown,    {.i = 1},      0, S_PRI },
	{ XK_ANY_MOD,           Button4, ttysend,        {.s = "\031"}, 0, S_ALT },
	{ XK_ANY_MOD,           Button5, ttysend,        {.s = "\005"}, 0, S_ALT },
	{ XK_ANY_MOD,           Button3, copylinktoclipboard, {0},      1 },
};

/* Internal keyboard shortcuts. */
#define MODKEY Mod1Mask
#define TERMMOD (ControlMask|ShiftMask)

/* External pipe script */
static char *copyoutput[]    = { "/bin/sh", "-c", "st-copyout", "externalpipe", NULL };

/* Example of externalpipein */
// static char *setbgcolorcmd[] = { "/bin/sh", "-c", "printf '\033]11;#008000\007'", "externalpipein", NULL };

static Shortcut shortcuts[] = {
	/* mask                 keysym          function         argument   screen */
	{ XK_ANY_MOD,           XK_Break,       sendbreak,       {.i =  0} },
	{ ControlMask,          XK_Print,       toggleprinter,   {.i =  0} },
	{ ShiftMask,            XK_Print,       printscreen,     {.i =  0} },
	{ XK_ANY_MOD,           XK_Print,       printsel,        {.i =  0} },
	{ TERMMOD,              XK_Prior,       zoom,            {.f = +1} },
	{ TERMMOD,              XK_Next,        zoom,            {.f = -1} },
	{ TERMMOD,              XK_Home,        zoomreset,       {.f =  0} },
	{ TERMMOD,              XK_C,           clipcopy,        {.i =  0} },
	{ TERMMOD,              XK_V,           clippaste,       {.i =  0} },
	{ TERMMOD,              XK_O,           changealpha,     {.f = +0.05} },
	{ TERMMOD,              XK_P,           changealpha,     {.f = -0.05} },
	{ TERMMOD,              XK_L,           changealpha,     {.f = 0} },
	//{ TERMMOD,              XK_,           changealphaunfocused, {.f = +0.05} },
	//{ TERMMOD,              XK_,           changealphaunfocused, {.f = -0.05} },
	//{ TERMMOD,              XK_,           changealphaunfocused, {.f = 0} },
	{ ShiftMask,            XK_Page_Up,     kscrollup,       {.i = -1}, S_PRI },
	{ ShiftMask,            XK_Page_Down,   kscrolldown,     {.i = -1}, S_PRI },
	{ TERMMOD,              XK_Y,           clippaste,       {.i =  0} },
	{ ShiftMask,            XK_Insert,      clippaste,       {.i =  0} },
	{ TERMMOD,              XK_Num_Lock,    numlock,         {.i =  0} },
	{ TERMMOD,              XK_Return,      newterm,         {.i =  NEWTERM_FG_CWD} },
	{ TERMMOD,              XK_space,       keyboard_select, { 0 } },
	{ TERMMOD,              XK_F,           searchforward,   { 0 } },
	{ TERMMOD,              XK_B,           searchbackward,  { 0 } },
	{ TERMMOD,              XK_I,           keyboard_flash,  { 0 } },
	{ TERMMOD,              XK_N,           keyboard_regex,  { 0 } },
	{ TERMMOD,              XK_M,           keyboard_url,    { 0 } },
	{ TERMMOD,              XK_Z,           scrolltoprompt,  {.i = -1}, S_PRI },
	{ TERMMOD,              XK_X,           scrolltoprompt,  {.i =  1}, S_PRI },
	{ XK_NO_MOD,            XK_F11,         fullscreen,      {.i =  0} },
	{ MODKEY,               XK_Return,      fullscreen,      {.i =  0} },
	{ MODKEY,               XK_o,           externalpipe,    {.v = copyoutput }, S_PRI },
};

/*
 * Special keys (change & recompile st.info accordingly)
 *
 * Mask value:
 * * Use XK_ANY_MOD to match the key no matter modifiers state
 * * Use XK_NO_MOD to match the key alone (no modifiers)
 * appkey value:
 * * 0: no value
 * * > 0: keypad application mode enabled
 * *   = 2: term.numlock = 1
 * * < 0: keypad application mode disabled
 * appcursor value:
 * * 0: no value
 * * > 0: cursor application mode enabled
 * * < 0: cursor application mode disabled
 *
 * Be careful with the order of the definitions because st searches in
 * this table sequentially, so any XK_ANY_MOD must be in the last
 * position for a key.
 */

/*
 * If you want keys other than the X11 function keys (0xFD00 - 0xFFFF)
 * to be mapped below, add them to this array.
 */
static KeySym mappedkeys[] = { -1 };

/*
 * State bits to ignore when matching key or button events.  By default,
 * numlock (Mod2Mask) and keyboard layout (XK_SWITCH_MOD) are ignored.
 */
static uint ignoremod = Mod2Mask|XK_SWITCH_MOD;

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
static Key key[] = {
	/* keysym           mask            string      appkey appcursor */
	{ XK_KP_Home,       ControlMask,    "\033[1;5H",     0,    0},
	{ XK_KP_Home, ControlMask|ShiftMask,"\033[1;6H",     0,    0},
	{ XK_KP_Home,       Mod1Mask,       "\033[1;3H",     0,    0},
	{ XK_KP_Home,       ShiftMask,      "\033[1;2H",     0,   -1}, /* original st sequence: "\033[2J" */
	{ XK_KP_Home,       ShiftMask,      "\033[1;2H",     0,   +1},
	{ XK_KP_Home,       XK_ANY_MOD,     "\033[H",        0,   -1},
	{ XK_KP_Home,       XK_ANY_MOD,     "\033[1~",       0,   +1},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033Ox",       +1,    0},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033[A",        0,   -1},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033OA",        0,   +1},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033Or",       +1,    0},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033[B",        0,   -1},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033OB",        0,   +1},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033Ot",       +1,    0},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033[D",        0,   -1},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033OD",        0,   +1},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033Ov",       +1,    0},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033[C",        0,   -1},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033OC",        0,   +1},
	{ XK_KP_Prior,      ControlMask,    "\033[5;5~",     0,    0},
	{ XK_KP_Prior,      Mod1Mask,       "\033[5;3~",     0,    0},
	{ XK_KP_Prior,      ShiftMask,      "\033[5;2~",     0,    0},
	{ XK_KP_Prior,      XK_ANY_MOD,     "\033[5~",       0,    0},
	{ XK_KP_Begin,      XK_ANY_MOD,     "\033[E",        0,    0},
	{ XK_KP_End,        ControlMask,    "\033[1;5F",    -1,    0}, /* original st sequence: "\033[J" */
	{ XK_KP_End,        ControlMask,    "\033[1;5F",    +1,    0},
	{ XK_KP_End,  ControlMask|ShiftMask,"\033[1;6F",     0,    0},
	{ XK_KP_End,        Mod1Mask,       "\033[1;3F",     0,    0},
	{ XK_KP_End,        ShiftMask,      "\033[1;2F",    -1,    0}, /* original st sequence: "\033[K" */
	{ XK_KP_End,        ShiftMask,      "\033[1;2F",    +1,    0},
	{ XK_KP_End,        XK_ANY_MOD,     "\033[4~",       0,    0},
	{ XK_KP_Next,       ControlMask,    "\033[6;5~",     0,    0},
	{ XK_KP_Next,       Mod1Mask,       "\033[6;3~",     0,    0},
	{ XK_KP_Next,       ShiftMask,      "\033[6;2~",     0,    0},
	{ XK_KP_Next,       XK_ANY_MOD,     "\033[6~",       0,    0},
	{ XK_KP_Insert,     Mod1Mask,       "\033[2;3~",     0,    0},
	{ XK_KP_Insert,     ShiftMask,      "\033[2;2~",    +1,    0},
	{ XK_KP_Insert,     ShiftMask,      "\033[4l",      -1,    0},
	{ XK_KP_Insert,     ControlMask,    "\033[L",       -1,    0},
	{ XK_KP_Insert,     ControlMask,    "\033[2;5~",    +1,    0},
	{ XK_KP_Insert,     XK_ANY_MOD,     "\033[2~",      -1,    0}, /* original st sequence: "\033[4h" */
	{ XK_KP_Insert,     XK_ANY_MOD,     "\033[2~",      +1,    0},
	{ XK_KP_Delete,     ControlMask,    "\033[3;5~",    -1,    0}, /* original st sequence: "\033[M" */
	{ XK_KP_Delete,     ControlMask,    "\033[3;5~",    +1,    0},
	{ XK_KP_Delete,     Mod1Mask,       "\033[3;3~",     0,    0},
	{ XK_KP_Delete,     ShiftMask,      "\033[2K",      -1,    0},
	{ XK_KP_Delete,     ShiftMask,      "\033[3;2~",    +1,    0},
	{ XK_KP_Delete,     XK_ANY_MOD,     "\033[3~",      -1,    0}, /* original st sequence: "\033[P" */
	{ XK_KP_Delete,     XK_ANY_MOD,     "\033[3~",      +1,    0},
	{ XK_KP_Multiply,   XK_ANY_MOD,     "\033Oj",       +2,    0},
	{ XK_KP_Add,        XK_ANY_MOD,     "\033Ok",       +2,    0},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\033OM",       +2,    0},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\r",           -1,    0},
	{ XK_KP_Subtract,   XK_ANY_MOD,     "\033Om",       +2,    0},
	{ XK_KP_Decimal,    XK_ANY_MOD,     "\033On",       +2,    0},
	{ XK_KP_Divide,     XK_ANY_MOD,     "\033Oo",       +2,    0},
	{ XK_KP_0,          XK_ANY_MOD,     "\033Op",       +2,    0},
	{ XK_KP_1,          XK_ANY_MOD,     "\033Oq",       +2,    0},
	{ XK_KP_2,          XK_ANY_MOD,     "\033Or",       +2,    0},
	{ XK_KP_3,          XK_ANY_MOD,     "\033Os",       +2,    0},
	{ XK_KP_4,          XK_ANY_MOD,     "\033Ot",       +2,    0},
	{ XK_KP_5,          XK_ANY_MOD,     "\033Ou",       +2,    0},
	{ XK_KP_6,          XK_ANY_MOD,     "\033Ov",       +2,    0},
	{ XK_KP_7,          XK_ANY_MOD,     "\033Ow",       +2,    0},
	{ XK_KP_8,          XK_ANY_MOD,     "\033Ox",       +2,    0},
	{ XK_KP_9,          XK_ANY_MOD,     "\033Oy",       +2,    0},
	{ XK_Up,            ShiftMask,      "\033[1;2A",     0,    0},
	{ XK_Up,            Mod1Mask,       "\033[1;3A",     0,    0},
	{ XK_Up,         ShiftMask|Mod1Mask,"\033[1;4A",     0,    0},
	{ XK_Up,            ControlMask,    "\033[1;5A",     0,    0},
	{ XK_Up,      ShiftMask|ControlMask,"\033[1;6A",     0,    0},
	{ XK_Up,       ControlMask|Mod1Mask,"\033[1;7A",     0,    0},
	{ XK_Up,ShiftMask|ControlMask|Mod1Mask,"\033[1;8A",  0,    0},
	{ XK_Up,            XK_ANY_MOD,     "\033[A",        0,   -1},
	{ XK_Up,            XK_ANY_MOD,     "\033OA",        0,   +1},
	{ XK_Down,          ShiftMask,      "\033[1;2B",     0,    0},
	{ XK_Down,          Mod1Mask,       "\033[1;3B",     0,    0},
	{ XK_Down,       ShiftMask|Mod1Mask,"\033[1;4B",     0,    0},
	{ XK_Down,          ControlMask,    "\033[1;5B",     0,    0},
	{ XK_Down,    ShiftMask|ControlMask,"\033[1;6B",     0,    0},
	{ XK_Down,     ControlMask|Mod1Mask,"\033[1;7B",     0,    0},
	{ XK_Down,ShiftMask|ControlMask|Mod1Mask,"\033[1;8B",0,    0},
	{ XK_Down,          XK_ANY_MOD,     "\033[B",        0,   -1},
	{ XK_Down,          XK_ANY_MOD,     "\033OB",        0,   +1},
	{ XK_Left,          ShiftMask,      "\033[1;2D",     0,    0},
	{ XK_Left,          Mod1Mask,       "\033[1;3D",     0,    0},
	{ XK_Left,       ShiftMask|Mod1Mask,"\033[1;4D",     0,    0},
	{ XK_Left,          ControlMask,    "\033[1;5D",     0,    0},
	{ XK_Left,    ShiftMask|ControlMask,"\033[1;6D",     0,    0},
	{ XK_Left,     ControlMask|Mod1Mask,"\033[1;7D",     0,    0},
	{ XK_Left,ShiftMask|ControlMask|Mod1Mask,"\033[1;8D",0,    0},
	{ XK_Left,          XK_ANY_MOD,     "\033[D",        0,   -1},
	{ XK_Left,          XK_ANY_MOD,     "\033OD",        0,   +1},
	{ XK_Right,         ShiftMask,      "\033[1;2C",     0,    0},
	{ XK_Right,         Mod1Mask,       "\033[1;3C",     0,    0},
	{ XK_Right,      ShiftMask|Mod1Mask,"\033[1;4C",     0,    0},
	{ XK_Right,         ControlMask,    "\033[1;5C",     0,    0},
	{ XK_Right,   ShiftMask|ControlMask,"\033[1;6C",     0,    0},
	{ XK_Right,    ControlMask|Mod1Mask,"\033[1;7C",     0,    0},
	{ XK_Right,ShiftMask|ControlMask|Mod1Mask,"\033[1;8C",0,   0},
	{ XK_Right,         XK_ANY_MOD,     "\033[C",        0,   -1},
	{ XK_Right,         XK_ANY_MOD,     "\033OC",        0,   +1},
	{ XK_ISO_Left_Tab,  ShiftMask,      "\033[Z",        0,    0},
	{ XK_Return,        Mod1Mask,       "\033\r",        0,    0},
	{ XK_Return,        XK_ANY_MOD,     "\r",            0,    0},
	{ XK_Insert,        ShiftMask,      "\033[4l",      -1,    0},
	{ XK_Insert,        ShiftMask,      "\033[2;2~",    +1,    0},
	{ XK_Insert,        Mod1Mask,       "\033[2;3~",     0,    0},
	{ XK_Insert,        ControlMask,    "\033[L",       -1,    0},
	{ XK_Insert,        ControlMask,    "\033[2;5~",    +1,    0},
	{ XK_Insert,        XK_ANY_MOD,     "\033[2~",      -1,    0}, /* original st sequence: "\033[4h" */
	{ XK_Insert,        XK_ANY_MOD,     "\033[2~",      +1,    0},
	{ XK_Delete,        ControlMask,    "\033[3;5~",    -1,    0}, /* original st sequence: "\033[M" */
	{ XK_Delete,        ControlMask,    "\033[3;5~",    +1,    0},
	{ XK_Delete,        Mod1Mask,       "\033[3;3~",     0,    0},
	{ XK_Delete,        ShiftMask,      "\033[2K",      -1,    0},
	{ XK_Delete,        ShiftMask,      "\033[3;2~",    +1,    0},
	{ XK_Delete,        XK_ANY_MOD,     "\033[3~",      -1,    0}, /* original st sequence: "\033[P" */
	{ XK_Delete,        XK_ANY_MOD,     "\033[3~",      +1,    0},
	{ XK_BackSpace,     XK_NO_MOD,      "\177",          0,    0},
	{ XK_BackSpace,     Mod1Mask,       "\033\177",      0,    0},
	{ XK_Home,          ControlMask,    "\033[1;5H",     0,    0},
	{ XK_Home,    ControlMask|ShiftMask,"\033[1;6H",     0,    0},
	{ XK_Home,          Mod1Mask,       "\033[1;3H",     0,    0},
	{ XK_Home,          ShiftMask,      "\033[1;2H",     0,   -1}, /* original st sequence: "\033[2J" */
	{ XK_Home,          ShiftMask,      "\033[1;2H",     0,   +1},
	{ XK_Home,          XK_ANY_MOD,     "\033[H",        0,   -1},
	{ XK_Home,          XK_ANY_MOD,     "\033[1~",       0,   +1},
	{ XK_End,           ControlMask,    "\033[1;5F",    -1,    0}, /* original st sequence: "\033[J" */
	{ XK_End,           ControlMask,    "\033[1;5F",    +1,    0},
	{ XK_End,     ControlMask|ShiftMask,"\033[1;6F",     0,    0},
	{ XK_End,           Mod1Mask,       "\033[1;3F",     0,    0},
	{ XK_End,           ShiftMask,      "\033[1;2F",    -1,    0}, /* original st sequence: "\033[K" */
	{ XK_End,           ShiftMask,      "\033[1;2F",    +1,    0},
	{ XK_End,           XK_ANY_MOD,     "\033[4~",       0,    0},
	{ XK_Prior,         ControlMask,    "\033[5;5~",     0,    0},
	{ XK_Prior,         Mod1Mask,       "\033[5;3~",     0,    0},
	{ XK_Prior,         ShiftMask,      "\033[5;2~",     0,    0},
	{ XK_Prior,         XK_ANY_MOD,     "\033[5~",       0,    0},
	{ XK_Next,          ControlMask,    "\033[6;5~",     0,    0},
	{ XK_Next,          Mod1Mask,       "\033[6;3~",     0,    0},
	{ XK_Next,          ShiftMask,      "\033[6;2~",     0,    0},
	{ XK_Next,          XK_ANY_MOD,     "\033[6~",       0,    0},
	{ XK_F1,            XK_NO_MOD,      "\033OP" ,       0,    0},
	{ XK_F1, /* F13 */  ShiftMask,      "\033[1;2P",     0,    0},
	{ XK_F1, /* F25 */  ControlMask,    "\033[1;5P",     0,    0},
	{ XK_F1, /* F37 */  Mod4Mask,       "\033[1;6P",     0,    0},
	{ XK_F1, /* F49 */  Mod1Mask,       "\033[1;3P",     0,    0},
	{ XK_F1, /* F61 */  Mod3Mask,       "\033[1;4P",     0,    0},
	{ XK_F2,            XK_NO_MOD,      "\033OQ" ,       0,    0},
	{ XK_F2, /* F14 */  ShiftMask,      "\033[1;2Q",     0,    0},
	{ XK_F2, /* F26 */  ControlMask,    "\033[1;5Q",     0,    0},
	{ XK_F2, /* F38 */  Mod4Mask,       "\033[1;6Q",     0,    0},
	{ XK_F2, /* F50 */  Mod1Mask,       "\033[1;3Q",     0,    0},
	{ XK_F2, /* F62 */  Mod3Mask,       "\033[1;4Q",     0,    0},
	{ XK_F3,            XK_NO_MOD,      "\033OR" ,       0,    0},
	{ XK_F3, /* F15 */  ShiftMask,      "\033[1;2R",     0,    0},
	{ XK_F3, /* F27 */  ControlMask,    "\033[1;5R",     0,    0},
	{ XK_F3, /* F39 */  Mod4Mask,       "\033[1;6R",     0,    0},
	{ XK_F3, /* F51 */  Mod1Mask,       "\033[1;3R",     0,    0},
	{ XK_F3, /* F63 */  Mod3Mask,       "\033[1;4R",     0,    0},
	{ XK_F4,            XK_NO_MOD,      "\033OS" ,       0,    0},
	{ XK_F4, /* F16 */  ShiftMask,      "\033[1;2S",     0,    0},
	{ XK_F4, /* F28 */  ControlMask,    "\033[1;5S",     0,    0},
	{ XK_F4, /* F40 */  Mod4Mask,       "\033[1;6S",     0,    0},
	{ XK_F4, /* F52 */  Mod1Mask,       "\033[1;3S",     0,    0},
	{ XK_F5,            XK_NO_MOD,      "\033[15~",      0,    0},
	{ XK_F5, /* F17 */  ShiftMask,      "\033[15;2~",    0,    0},
	{ XK_F5, /* F29 */  ControlMask,    "\033[15;5~",    0,    0},
	{ XK_F5, /* F41 */  Mod4Mask,       "\033[15;6~",    0,    0},
	{ XK_F5, /* F53 */  Mod1Mask,       "\033[15;3~",    0,    0},
	{ XK_F6,            XK_NO_MOD,      "\033[17~",      0,    0},
	{ XK_F6, /* F18 */  ShiftMask,      "\033[17;2~",    0,    0},
	{ XK_F6, /* F30 */  ControlMask,    "\033[17;5~",    0,    0},
	{ XK_F6, /* F42 */  Mod4Mask,       "\033[17;6~",    0,    0},
	{ XK_F6, /* F54 */  Mod1Mask,       "\033[17;3~",    0,    0},
	{ XK_F7,            XK_NO_MOD,      "\033[18~",      0,    0},
	{ XK_F7, /* F19 */  ShiftMask,      "\033[18;2~",    0,    0},
	{ XK_F7, /* F31 */  ControlMask,    "\033[18;5~",    0,    0},
	{ XK_F7, /* F43 */  Mod4Mask,       "\033[18;6~",    0,    0},
	{ XK_F7, /* F55 */  Mod1Mask,       "\033[18;3~",    0,    0},
	{ XK_F8,            XK_NO_MOD,      "\033[19~",      0,    0},
	{ XK_F8, /* F20 */  ShiftMask,      "\033[19;2~",    0,    0},
	{ XK_F8, /* F32 */  ControlMask,    "\033[19;5~",    0,    0},
	{ XK_F8, /* F44 */  Mod4Mask,       "\033[19;6~",    0,    0},
	{ XK_F8, /* F56 */  Mod1Mask,       "\033[19;3~",    0,    0},
	{ XK_F9,            XK_NO_MOD,      "\033[20~",      0,    0},
	{ XK_F9, /* F21 */  ShiftMask,      "\033[20;2~",    0,    0},
	{ XK_F9, /* F33 */  ControlMask,    "\033[20;5~",    0,    0},
	{ XK_F9, /* F45 */  Mod4Mask,       "\033[20;6~",    0,    0},
	{ XK_F9, /* F57 */  Mod1Mask,       "\033[20;3~",    0,    0},
	{ XK_F10,           XK_NO_MOD,      "\033[21~",      0,    0},
	{ XK_F10, /* F22 */ ShiftMask,      "\033[21;2~",    0,    0},
	{ XK_F10, /* F34 */ ControlMask,    "\033[21;5~",    0,    0},
	{ XK_F10, /* F46 */ Mod4Mask,       "\033[21;6~",    0,    0},
	{ XK_F10, /* F58 */ Mod1Mask,       "\033[21;3~",    0,    0},
	{ XK_F11,           XK_NO_MOD,      "\033[23~",      0,    0},
	{ XK_F11, /* F23 */ ShiftMask,      "\033[23;2~",    0,    0},
	{ XK_F11, /* F35 */ ControlMask,    "\033[23;5~",    0,    0},
	{ XK_F11, /* F47 */ Mod4Mask,       "\033[23;6~",    0,    0},
	{ XK_F11, /* F59 */ Mod1Mask,       "\033[23;3~",    0,    0},
	{ XK_F12,           XK_NO_MOD,      "\033[24~",      0,    0},
	{ XK_F12, /* F24 */ ShiftMask,      "\033[24;2~",    0,    0},
	{ XK_F12, /* F36 */ ControlMask,    "\033[24;5~",    0,    0},
	{ XK_F12, /* F48 */ Mod4Mask,       "\033[24;6~",    0,    0},
	{ XK_F12, /* F60 */ Mod1Mask,       "\033[24;3~",    0,    0},
	{ XK_F13,           XK_NO_MOD,      "\033[1;2P",     0,    0},
	{ XK_F14,           XK_NO_MOD,      "\033[1;2Q",     0,    0},
	{ XK_F15,           XK_NO_MOD,      "\033[1;2R",     0,    0},
	{ XK_F16,           XK_NO_MOD,      "\033[1;2S",     0,    0},
	{ XK_F17,           XK_NO_MOD,      "\033[15;2~",    0,    0},
	{ XK_F18,           XK_NO_MOD,      "\033[17;2~",    0,    0},
	{ XK_F19,           XK_NO_MOD,      "\033[18;2~",    0,    0},
	{ XK_F20,           XK_NO_MOD,      "\033[19;2~",    0,    0},
	{ XK_F21,           XK_NO_MOD,      "\033[20;2~",    0,    0},
	{ XK_F22,           XK_NO_MOD,      "\033[21;2~",    0,    0},
	{ XK_F23,           XK_NO_MOD,      "\033[23;2~",    0,    0},
	{ XK_F24,           XK_NO_MOD,      "\033[24;2~",    0,    0},
	{ XK_F25,           XK_NO_MOD,      "\033[1;5P",     0,    0},
	{ XK_F26,           XK_NO_MOD,      "\033[1;5Q",     0,    0},
	{ XK_F27,           XK_NO_MOD,      "\033[1;5R",     0,    0},
	{ XK_F28,           XK_NO_MOD,      "\033[1;5S",     0,    0},
	{ XK_F29,           XK_NO_MOD,      "\033[15;5~",    0,    0},
	{ XK_F30,           XK_NO_MOD,      "\033[17;5~",    0,    0},
	{ XK_F31,           XK_NO_MOD,      "\033[18;5~",    0,    0},
	{ XK_F32,           XK_NO_MOD,      "\033[19;5~",    0,    0},
	{ XK_F33,           XK_NO_MOD,      "\033[20;5~",    0,    0},
	{ XK_F34,           XK_NO_MOD,      "\033[21;5~",    0,    0},
	{ XK_F35,           XK_NO_MOD,      "\033[23;5~",    0,    0},
};

/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static uint selmasks[] = {
	[SEL_RECTANGULAR] = Mod1Mask,
};

/*
 * Printable characters in ASCII, used to estimate the advance width
 * of single wide characters.
 */
static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";

/*
 * Undercurl styles. Set undercurl_style to one of the available styles.
 */
#define UNDERCURL_NONE 0
#define UNDERCURL_CURLY 1
#define UNDERCURL_SPIKY 2
#define UNDERCURL_CAPPED 3
// Active style
int undercurl_style = UNDERCURL_CURLY;
// Adds 1 pixel of thickness for every undercurl_thickness_threshold pixels of font size
int undercurl_thickness_threshold  = 28;
// Extra thickness for spiky and capped waves: 0 = off, 1 = on
int undercurl_extra_thickness  = 0;
// Y-offset of undercurl
int undercurl_yoffset = 0;
// Scaling factor for undercurl height
float undercurl_height_scale = 1.0;

/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
		{ "font",                STRING,  &font },
		{ "font_bold",           STRING,  &font_bold },
		{ "font_italic",         STRING,  &font_italic },
		{ "font_bolditalic",     STRING,  &font_bolditalic },
		{ "font_fallback1",      STRING,  &font2_xresources[0] },
		{ "font_fallback2",      STRING,  &font2_xresources[1] },
		{ "font_fallback3",      STRING,  &font2_xresources[2] },
		{ "font_fallback4",      STRING,  &font2_xresources[3] },
		{ "font_fallback5",      STRING,  &font2_xresources[4] },
		{ "font_fallback6",      STRING,  &font2_xresources[5] },
		{ "font_fallback7",      STRING,  &font2_xresources[6] },
		{ "font_fallback8",      STRING,  &font2_xresources[7] },
		{ "color0",              STRING,  &colorname[0] },
		{ "color1",              STRING,  &colorname[1] },
		{ "color2",              STRING,  &colorname[2] },
		{ "color3",              STRING,  &colorname[3] },
		{ "color4",              STRING,  &colorname[4] },
		{ "color5",              STRING,  &colorname[5] },
		{ "color6",              STRING,  &colorname[6] },
		{ "color7",              STRING,  &colorname[7] },
		{ "color8",              STRING,  &colorname[8] },
		{ "color9",              STRING,  &colorname[9] },
		{ "color10",             STRING,  &colorname[10] },
		{ "color11",             STRING,  &colorname[11] },
		{ "color12",             STRING,  &colorname[12] },
		{ "color13",             STRING,  &colorname[13] },
		{ "color14",             STRING,  &colorname[14] },
		{ "color15",             STRING,  &colorname[15] },
		{ "foreground",          STRING,  &colorname[258] },
		{ "background",          STRING,  &colorname[259] },
		{ "bgUnfocused",         STRING,  &colorname[260] },
		{ "cursorColor",         STRING,  &colorname[256] },
		{ "reverseCursor",       STRING,  &colorname[257] },
		{ "visualbellcolor",     STRING,  &colorname[261] },
		{ "highlightfg",         INTEGER, &highlightfg },
		{ "highlightbg",         INTEGER, &highlightbg },
		{ "flashlabelfg",        INTEGER, &flashlabelfg },
		{ "flashlabelbg",        INTEGER, &flashlabelbg },
		{ "flashtextfg",         INTEGER, &flashtextfg },
		{ "flashtextbg",         INTEGER, &flashtextbg },
		{ "hyperlinkhintfg",     INTEGER, &hyperlinkhintfg },
		{ "hyperlinkhintbg",     INTEGER, &hyperlinkhintbg },
		{ "kbselectfg",          INTEGER, &kbselectfg },
		{ "kbselectbg",          INTEGER, &kbselectbg },
		{ "scrollbackindicatorfg", INTEGER, &scrollbackindicatorfg },
		{ "bold_is_not_bright",  INTEGER, &bold_is_not_bright },
		{ "dynamic_cursor_color",INTEGER, &dynamic_cursor_color },
		{ "cursorstyle",         INTEGER, &cursorstyle },
		{ "cursorblinking",      INTEGER, &cursorblinking },
		{ "cursorblinktimeout",  INTEGER, &cursorblinktimeout },
		{ "alpha",               FLOAT,   &alpha },
		{ "alphaUnfocused",      FLOAT,   &alphaUnfocused },
		{ "termname",            STRING,  &termname },
		{ "shell",               STRING,  &shell },
		{ "minlatency",          FLOAT,   &minlatency },
		{ "maxlatency",          FLOAT,   &maxlatency },
		{ "su_timeout",          INTEGER, &su_timeout },
		{ "blinktimeout",        INTEGER, &blinktimeout },
		{ "doubleclicktimeout",  INTEGER, &doubleclicktimeout },
		{ "tripleclicktimeout",  INTEGER, &tripleclicktimeout },
		{ "visualbellstyle",     INTEGER, &visualbellstyle },
		{ "visualbellduration",  INTEGER, &visualbellduration },
		{ "visualbellanimfps",   INTEGER, &visualbellanimfps },
		{ "bellvolume",          INTEGER, &bellvolume },
		{ "tabspaces",           INTEGER, &tabspaces },
		{ "cursorthickness",     INTEGER, &cursorthickness },
		{ "borderpx",            INTEGER, &borderpx },
		{ "borderperc",          INTEGER, &borderperc },
		{ "cwscale",             FLOAT,   &cwscale },
		{ "chscale",             FLOAT,   &chscale },
		{ "boxdraw",             INTEGER, &boxdraw },
		{ "boxdraw_bold",        INTEGER, &boxdraw_bold },
		{ "boxdraw_braille",     INTEGER, &boxdraw_braille },
		{ "boxdraw_extra",       INTEGER, &boxdraw_extra },
		{ "boxdraw_branch",      INTEGER, &boxdraw_branch },
		{ "boxdraw_branch_thickness", INTEGER, &boxdraw_branch_thickness },
		{ "hidecursor",          INTEGER, &hidecursor },
		{ "ligatures",           INTEGER, &ligatures },
		{ "vertcenter",          INTEGER, &vertcenter },
		{ "cols",                INTEGER, &cols },
		{ "rows",                INTEGER, &rows },
		{ "anysize",             INTEGER, &anysize },
		{ "disablehyperlinks",   INTEGER, &disablehyperlinks },
		{ "hyperlinkstyle",      INTEGER, &hyperlinkstyle },
		{ "showhyperlinkhint",   INTEGER, &showhyperlinkhint },
		{ "url_opener",          STRING,  &url_opener },
		{ "url_protocols",       STRING,  &url_protocols },
		{ "undercurl_style",               INTEGER, &undercurl_style },
		{ "undercurl_thickness_threshold", INTEGER, &undercurl_thickness_threshold },
		{ "undercurl_extra_thickness",     INTEGER, &undercurl_extra_thickness },
		{ "undercurl_yoffset",             INTEGER, &undercurl_yoffset },
		{ "undercurl_height_scale",        FLOAT,   &undercurl_height_scale },
		{ "autoscrolltimeout",             INTEGER, &autoscrolltimeout },
		{ "autoscrollacceleration",        FLOAT,   &autoscrollacceleration },
		{ "scrollbackindicator",           INTEGER, &scrollbackindicator },
		{ "scrollbacklines",               INTEGER, &scrollbacklines },
};
