#define TLINE_HIST(y)           ((y) <= HISTSIZE-term.row+2 ? term.hist[(y)] : term.line[(y-HISTSIZE+term.row-3)])

void externalpipe(const Arg *);
void externalpipein(const Arg *);
int tlinehistlen(int);

