#define TLINE(y) ( \
	(y) < term.scr ? term.hist[(term.histi + (y) - term.scr + 1 + term.histsize) % term.histsize] \
	               : term.line[(y) - term.scr] \
)

#define TLINEABS(y) ( \
	(y) < 0 ? term.hist[(term.histi + (y) + 1 + term.histsize) % term.histsize] : term.line[(y)] \
)

#define UPDATEWRAPNEXT(alt, col) do { \
	if ((term.c.state & CURSOR_WRAPNEXT) && term.c.x + term.wrapcwidth[alt] < col) { \
		term.c.x += term.wrapcwidth[alt]; \
		term.c.state &= ~CURSOR_WRAPNEXT; \
	} \
} while (0);

#define MIN_HISTSIZE 500
#define MAX_HISTSIZE 100000

void kscrolldown(const Arg *);
void kscrollup(const Arg *);
void increasehistorysize(int, int);
void sethistorylimit(int);

typedef struct {
	 uint b;
	 uint mask;
	 void (*func)(const Arg *);
	 const Arg arg;
} MouseKey;

extern MouseKey mkeys[];
