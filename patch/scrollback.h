#define TLINE(y) ( \
	(y) < term.scr ? term.hist[(term.histi + (y) - term.scr + 1 + HISTSIZE) % HISTSIZE] \
	               : term.line[(y) - term.scr] \
)

#define TLINEABS(y) ( \
	(y) < 0 ? term.hist[(term.histi + (y) + 1 + HISTSIZE) % HISTSIZE] : term.line[(y)] \
)

#define UPDATEWRAPNEXT(alt, col) do { \
	if ((term.c.state & CURSOR_WRAPNEXT) && term.c.x + term.wrapcwidth[alt] < col) { \
		term.c.x += term.wrapcwidth[alt]; \
		term.c.state &= ~CURSOR_WRAPNEXT; \
	} \
} while (0);

void kscrolldown(const Arg *);
void kscrollup(const Arg *);

typedef struct {
	 uint b;
	 uint mask;
	 void (*func)(const Arg *);
	 const Arg arg;
} MouseKey;

extern MouseKey mkeys[];
