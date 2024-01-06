#include <wctype.h>

static int kbds_in_use, kbds_quant;
static int kbds_type_old = SEL_REGULAR, kbds_type = SEL_REGULAR;
static int kbds_mode, kbds_searchlen, kbds_searchdir, kbds_searchcase;
static Glyph *kbds_searchstr;
static TCursor kbds_c, kbds_oc;
static const char kbds_ldelim[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~ \t";
static const char kbds_sdelim[] = " \t";

enum keyboardselect_mode {
	KBDS_MODE_SELECT  = 1<<0,
	KBDS_MODE_LSELECT = 1<<1,
	KBDS_MODE_SEARCH  = 1<<2
};

void
kbds_drawmode(int y)
{
	static char *mode[] = { " MOVE ", "  SEL " , " RSEL ", " LSEL " };
	static char quant[20] = { ' ' };
	static Glyph g;
	int i, n, m;

	if (!kbds_in_use || y != term.row-1)
		return;

	g.mode = ATTR_REVERSE;
	g.fg = defaultfg;
	g.bg = defaultbg;

	if (kbds_mode & KBDS_MODE_SEARCH) {
		for (g.u = ' ', i = 0; i < term.col; i++)
			xdrawglyph(g, i, y);
		g.u = (kbds_searchdir < 0) ? '/' : '?';
		xdrawglyph(g, 0, y);
		for (i = 0; i < kbds_searchlen; i++) {
			if (kbds_searchstr[i].mode & ATTR_WDUMMY)
				continue;
			g.u = kbds_searchstr[i].u;
			g.mode = kbds_searchstr[i].mode | ATTR_REVERSE;
			xdrawglyph(g, i + 1, y);
		}
		g.u = ' ';
		g.mode = ATTR_NULL;
		xdrawglyph(g, i + 1, y);
	} else {
		for (n = 6, i = term.col-1; i >= 0 && n > 0; i--) {
			m = (!(kbds_mode & KBDS_MODE_SELECT) || (kbds_mode & KBDS_MODE_LSELECT))
			     ? kbds_mode : kbds_mode + (kbds_type == SEL_REGULAR ? 0 : 1);
			g.u = mode[m][--n];
			xdrawglyph(g, i, y);
		}
		if (kbds_quant) {
			n = snprintf(quant+1, sizeof quant-1, "%i", kbds_quant) + 1;
			for (; i >= 0 && n > 0; i--) {
				g.u = quant[--n];
				xdrawglyph(g, i, y);
			}
		}
	}
}

void
kbds_setmode(int mode)
{
	kbds_mode = mode;
	term.dirty[term.row-1] = 1;
}

void
kbds_selecttext(void)
{
	if (kbds_in_use && kbds_mode & KBDS_MODE_SELECT) {
		selextend(kbds_c.x, kbds_c.y, kbds_type, 0);
		if (sel.mode == SEL_IDLE)
			kbds_setmode(kbds_mode ^ KBDS_MODE_SELECT);
	}
}

int
kbds_drawcursor(void)
{
	if (kbds_in_use && !(kbds_mode & KBDS_MODE_SEARCH && kbds_c.y == term.row-1)) {
		xdrawcursor(kbds_c.x, kbds_c.y, TLINE(kbds_c.y)[kbds_c.x],
					kbds_oc.x, kbds_oc.y, TLINE(kbds_oc.y)[kbds_oc.x],
					TLINE(kbds_oc.y), term.col);
		kbds_oc = kbds_c;
	}
	return term.scr != 0 || kbds_in_use;
}

int
kbds_moveto(int x, int y)
{
	kbds_c.x = x;
	kbds_c.y = y;
	LIMIT(kbds_c.x, 0, term.col-1);
	LIMIT(kbds_c.y, 0, term.row-1);
	if ((TLINE(kbds_c.y)[kbds_c.x].mode & ATTR_WDUMMY) && kbds_c.x > 0)
		kbds_c.x--;
}

int
kbds_ismatch(Line line, int x, int y, int bot, int len)
{
	int i, xo = x, yo = y, hlen = len;
	Line hline = line;

	if (len <= 0 || (x + kbds_searchlen > len &&
	    (!(line[len-1].mode & ATTR_WRAP) || y >= bot)))
		return 0;

	for (i = 0; i < kbds_searchlen; i++, x++) {
		if (x >= len) {
			if (!(line[len-1].mode & ATTR_WRAP) || ++y > bot)
				return 0;
			x = 0;
			line = TLINE(y);
			if ((len = tlinelen(line)) <= 0)
				return 0;
		}
		if ((kbds_searchcase && kbds_searchstr[i].u != line[x].u) ||
		    (!kbds_searchcase && kbds_searchstr[i].u != towlower(line[x].u)))
			return 0;
	}

	for (i = 0; i < kbds_searchlen; i++, xo++) {
		if (xo >= hlen) {
			xo = 0, yo++;
			hline = TLINE(yo);
			hlen = tlinelen(hline);
		}
		hline[xo].mode |= ATTR_HIGHLIGHT;
	}
	return 1;
}

void
kbds_searchall(void)
{
	int top = IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf + term.scr;
	int bot = IS_SET(MODE_ALTSCREEN) ? term.row-1 : term.row + term.scr - 1;
	int x, y, len;
	Line line;

	if (!kbds_searchlen)
		return;

	for (y = top; y <= bot; y++) {
		line = TLINE(y);
		len = tlinelen(line);
		for (x = 0; x < len; x++)
			kbds_ismatch(line, x, y, bot, len);
	}
	tfulldirt();
}

void
kbds_searchnext(int dir)
{
	int top = IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf + term.scr;
	int bot = IS_SET(MODE_ALTSCREEN) ? term.row-1 : term.row + term.scr - 1;
	int xo, yo, x = kbds_c.x, y = kbds_c.y;
	Line line = TLINE(y);
	int len = tlinelen(line);
	int wrapped = 0;

	if (!kbds_searchlen)
		return;

	if (dir < 0 && x > len)
		x = len;

	for (kbds_quant = MAX(kbds_quant, 1); kbds_quant > 0;) {
		for (xo = x, yo = y;;) {
			x += dir;
			if (x < 0 || x >= len) {
				y += dir;
				if (y < top)
					y = bot, wrapped = 1;
				else if (y > bot)
					y = top, wrapped = 1;
				line = TLINE(y);
				len = tlinelen(line);
				x = (dir > 0) ? 0 : MAX(len-1, 0);
			}
			if (kbds_ismatch(line, x, y, bot, len)) {
				if (y < 0)
					kscrollup(&((Arg){ .i = -y }));
				else if (y >= term.row)
					kscrolldown(&((Arg){ .i = y - term.row + 1 }));
				LIMIT(y, 0, term.row-1);
				if (!IS_SET(MODE_ALTSCREEN))
					top = -term.histf + term.scr, bot =  term.row + term.scr - 1;
				kbds_c.x = x, kbds_c.y = y;
				kbds_quant--;
				break;
			}
			if ((wrapped && dir > 0 && x >= xo && y >= yo) ||
			    (wrapped && dir < 0 && x <= xo && y <= yo))
				goto end;
		}
	}
end:
	kbds_quant = 0;
}

void
kbds_clearhighlights(void)
{
	int x, y;
	Line line;

	for (y = (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf); y < term.row; y++) {
		line = TLINEABS(y);
		for (x = 0; x < term.col; x++)
			line[x].mode &= ~ATTR_HIGHLIGHT;
	}
	tfulldirt();
}

int
kbds_isdelim(Line line, int x, int y, int xoff, int len, const char *delims)
{
	for (;;) {
		x += xoff;
		if (x >= len) {
			if (len <= 0 || !(line[len-1].mode & ATTR_WRAP) ||
			   ++y >= (IS_SET(MODE_ALTSCREEN) ? term.row : term.row + term.scr))
				return 1;
			line = TLINE(y);
			len = tlinelen(line);
			x = 0;
		} else if (x < 0) {
			if (--y < (IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf + term.scr))
				return 1;
			line = TLINE(y);
			len = tlinelen(line);
			x = len-1;
			if (x < 0 || !(line[x].mode & ATTR_WRAP))
				return 1;
		}
		if ((line[x].mode & ATTR_WDUMMY) && xoff != 0)
			continue;
		return !(line[x].mode & ATTR_SET) || line[x].u == 0 ||
		       (line[x].u < 128 && strchr(delims, (int)line[x].u) != NULL);
	}
}

void
kbds_nextword(int start, int dir, const char *delims)
{
	int top = IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf + term.scr;
	int bot = IS_SET(MODE_ALTSCREEN) ? term.row-1 : term.row + term.scr - 1;
	int x = kbds_c.x, y = kbds_c.y;
	Line line = TLINE(y);
	int len = tlinelen(line);
	int xoff = start ? -1 : 1;

	if (dir < 0 && x > len)
		x = len;

	for (kbds_quant = MAX(kbds_quant, 1); kbds_quant > 0;) {
		x += dir;
		if (x < 0 || x >= len) {
			y += dir;
			if (y < top || y > bot)
				break;
			line = TLINE(y);
			len = tlinelen(line);
			x = (dir > 0) ? 0 : MAX(len-1, 0);
		}
		if (line[x].mode & ATTR_WDUMMY)
			continue;
		if (!kbds_isdelim(line, x, y, 0, len, delims) &&
		    kbds_isdelim(line, x, y, xoff, len, delims))
		{
			if (y < 0)
				kscrollup(&((Arg){ .i = -y }));
			else if (y >= term.row)
				kscrolldown(&((Arg){ .i = y - term.row + 1 }));
			LIMIT(y, 0, term.row-1);
			if (!IS_SET(MODE_ALTSCREEN))
				top = -term.histf + term.scr, bot =  term.row + term.scr - 1;
			kbds_c.x = x, kbds_c.y = y;
			kbds_quant--;
		}
	}
	kbds_quant = 0;
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
			if (u > 0x1f && kbds_searchlen < term.col-2) {
				kbds_searchstr[kbds_searchlen].u = u;
				kbds_searchstr[kbds_searchlen++].mode = ATTR_NULL;
				if (wcwidth(u) > 1) {
					kbds_searchstr[kbds_searchlen-1].mode = ATTR_WIDE;
					if (kbds_searchlen < term.col-2) {
						kbds_searchstr[kbds_searchlen].u = '\0';
						kbds_searchstr[kbds_searchlen++].mode = ATTR_WDUMMY;
					}
				}
			}
		}
		buflen -= n;
		/* keep any incomplete UTF-8 byte sequence for the next call */
		if (buflen > 0)
			memmove(buf, buf + n, buflen);
	}
	term.dirty[term.row-1] = 1;
}

int
kbds_keyboardhandler(KeySym ksym, char *buf, int len, int forcequit)
{
	int i, q, *xy, dy, prevscr;
	Line line;
	Rune u;

	if (kbds_mode & KBDS_MODE_SEARCH && !forcequit) {
		switch (ksym) {
			case XK_Escape:
				kbds_searchlen = 0;
				/* FALLTHROUGH */
			case XK_Return:
				for (kbds_searchcase = 0, i = 0; i < kbds_searchlen; i++) {
					if (kbds_searchstr[i].u != towlower(kbds_searchstr[i].u)) {
						kbds_searchcase = 1;
						break;
					}
				}
				kbds_searchall();
				kbds_searchnext(kbds_searchdir);
				kbds_setmode(kbds_mode ^ KBDS_MODE_SEARCH);
				break;
			case XK_BackSpace:
				if (kbds_searchlen) {
					kbds_searchlen--;
					if (kbds_searchlen && (kbds_searchstr[kbds_searchlen].mode & ATTR_WDUMMY))
						kbds_searchlen--;
				}
				break;
			default:
				if (len < 1 || kbds_searchlen >= term.col-2)
					return 0;
				utf8decode(buf, &u, len);
				kbds_searchstr[kbds_searchlen].u = u;
				kbds_searchstr[kbds_searchlen++].mode = ATTR_NULL;
				if (wcwidth(u) > 1) {
					kbds_searchstr[kbds_searchlen-1].mode = ATTR_WIDE;
					if (kbds_searchlen < term.col-2) {
						kbds_searchstr[kbds_searchlen].u = '\0';
						kbds_searchstr[kbds_searchlen++].mode = ATTR_WDUMMY;
					}
				}
				break;
		}
		term.dirty[term.row-1] = 1;
		return 0;
	}

	switch (ksym) {
	case -1:
		kbds_searchstr = xmalloc(term.col * sizeof(Glyph));
		kbds_in_use = 1;
		kbds_c = kbds_oc = term.c;
		kbds_setmode(0);
		return MODE_KBDSELECT;
	case XK_V:
		if (kbds_mode & KBDS_MODE_LSELECT) {
			selclear();
			kbds_c.x = 0;
			kbds_type = kbds_type_old;
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		} else if (kbds_mode & KBDS_MODE_SELECT) {
			kbds_c.x = term.col-1;
			kbds_type_old = kbds_type;
			kbds_type = SEL_RECTANGULAR;
			selextend(kbds_c.x, kbds_c.y, kbds_type, 0);
			sel.ob.x = 0;
			tfulldirt();
			kbds_setmode(KBDS_MODE_SELECT | KBDS_MODE_LSELECT);
		} else {
			kbds_c.x = 0;
			selstart(kbds_c.x, kbds_c.y, 0);
			kbds_c.x = term.col-1;
			kbds_type_old = kbds_type;
			kbds_type = SEL_RECTANGULAR;
			selextend(kbds_c.x, kbds_c.y, kbds_type, 0);
			kbds_setmode(KBDS_MODE_SELECT | KBDS_MODE_LSELECT);
		}
		break;
	case XK_v:
		if (kbds_mode & KBDS_MODE_LSELECT) {
			kbds_type = kbds_type_old;
			selextend(kbds_c.x, kbds_c.y, kbds_type, 0);
			kbds_setmode(KBDS_MODE_SELECT & ~(KBDS_MODE_LSELECT));
		} else if (kbds_mode & KBDS_MODE_SELECT) {
			selclear();
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT));
		} else {
			selstart(kbds_c.x, kbds_c.y, 0);
			kbds_setmode(KBDS_MODE_SELECT);
		}
		break;
	case XK_t:
		if (!(kbds_mode & KBDS_MODE_LSELECT)) {
			selextend(kbds_c.x, kbds_c.y, kbds_type ^= (SEL_REGULAR | SEL_RECTANGULAR), 0);
			selextend(kbds_c.x, kbds_c.y, kbds_type, 0);
		}
		break;
	case XK_y:
	case XK_Y:
		if (kbds_mode & KBDS_MODE_SELECT) {
			selextend(kbds_c.x, kbds_c.y, kbds_type, 1);
			xsetsel(getsel());
			xclipcopy();
			selclear();
			if (kbds_mode & KBDS_MODE_LSELECT) {
				kbds_c.x = 0;
				kbds_type = kbds_type_old;
			}
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
		}
		break;
	case XK_slash:
	case XK_KP_Divide:
	case XK_question:
		kbds_searchdir = (ksym == XK_question) ? 1 : -1;
		kbds_searchlen = 0;
		kbds_setmode(kbds_mode ^ KBDS_MODE_SEARCH);
		kbds_clearhighlights();
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
		if (kbds_mode & KBDS_MODE_SELECT && !forcequit) {
			if (kbds_mode & KBDS_MODE_LSELECT) {
				kbds_c.x = 0;
				kbds_type = kbds_type_old;
			}
			kbds_setmode(kbds_mode & ~(KBDS_MODE_SELECT | KBDS_MODE_LSELECT));
			break;
		}
		kbds_mode = 0;
		/* FALLTHROUGH */
	case XK_Return:
		if (kbds_mode & KBDS_MODE_SELECT) {
			selextend(kbds_c.x, kbds_c.y, kbds_type, 1);
			xsetsel(getsel());
			xclipcopy();
			if (kbds_mode & KBDS_MODE_LSELECT) {
				kbds_c.x = 0;
				kbds_type = kbds_type_old;
			}
		}
		kbds_in_use = kbds_quant = 0;
		free(kbds_searchstr);
		kscrolldown(&((Arg){ .i = term.histf }));
		kbds_clearhighlights();
		return MODE_KBDSELECT;
	case XK_n:
	case XK_N:
		kbds_searchnext(ksym == XK_n ? kbds_searchdir : -kbds_searchdir);
		break;
	case XK_BackSpace:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_moveto(0, kbds_c.y);
		break;
	case XK_exclam:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_moveto(term.col/2, kbds_c.y);
		break;
	case XK_underscore:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_moveto(term.col-1, kbds_c.y);
		break;
	case XK_dollar:
	case XK_A:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_moveto(tlinelen(TLINE(kbds_c.y))-1, kbds_c.y);
		break;
	case XK_asciicircum:
	case XK_I:
		if (!(kbds_mode & KBDS_MODE_LSELECT)) {
			line = TLINE(kbds_c.y);
			len = tlinelen(line);
			for (i = 0; i < len && line[i].u == ' '; i++)
				;
			kbds_moveto((i < len) ? i : 0, kbds_c.y);
		}
		break;
	case XK_End:
	case XK_KP_End:
		kbds_c.y = term.row-1;
		break;
	case XK_Home:
	case XK_KP_Home:
	case XK_H:
		kbds_c.y = 0;
		break;
	case XK_M:
		kbds_c.y = IS_SET(MODE_ALTSCREEN)
			? (term.row-1) / 2
			: MIN(term.c.y + term.scr, term.row-1) / 2;
		break;
	case XK_L:
		kbds_c.y = IS_SET(MODE_ALTSCREEN)
			? term.row-1
			: MIN(term.c.y + term.scr, term.row-1);
		break;
	case XK_Page_Up:
	case XK_KP_Page_Up:
	case XK_K:
		prevscr = term.scr;
		kscrollup(&((Arg){ .i = term.row }));
		kbds_c.y = IS_SET(MODE_ALTSCREEN)
			? 0
			: MAX(0, kbds_c.y - term.row + term.scr - prevscr);
		break;
	case XK_Page_Down:
	case XK_KP_Page_Down:
	case XK_J:
		prevscr = term.scr;
		kscrolldown(&((Arg){ .i = term.row }));
		kbds_c.y = IS_SET(MODE_ALTSCREEN)
			? term.row-1
			: MIN(MIN(term.c.y + term.scr, term.row-1), kbds_c.y + term.row + term.scr - prevscr);
		break;
	case XK_asterisk:
	case XK_KP_Multiply:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_moveto(term.col/2, (term.row-1) / 2);
		break;
	case XK_g:
		kscrollup(&((Arg){ .i = term.histf }));
		kbds_c.y = 0;
		break;
	case XK_G:
		kscrolldown(&((Arg){ .i = term.histf }));
		kbds_c.y = term.c.y;
		break;
	case XK_b:
	case XK_B:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_nextword(1, -1, (ksym == XK_b) ? kbds_ldelim : kbds_sdelim);
		break;
	case XK_w:
	case XK_W:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_nextword(1, +1, (ksym == XK_w) ? kbds_ldelim : kbds_sdelim);
		break;
	case XK_e:
	case XK_E:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_nextword(0, +1, (ksym == XK_e) ? kbds_ldelim : kbds_sdelim);
		break;
	case XK_z:
		prevscr = term.scr;
		dy = kbds_c.y - (term.row-1) / 2;
		if (dy <= 0)
			kscrollup(&((Arg){ .i = -dy }));
		else
			kscrolldown(&((Arg){ .i = dy }));
		kbds_c.y += term.scr - prevscr;
		break;
	case XK_0:
	case XK_KP_0:
		if (!kbds_quant && !(kbds_mode & KBDS_MODE_LSELECT)) {
			kbds_c.x = 0;
			break;
		}
		/* FALLTHROUGH */
	default:
		if (ksym >= XK_0 && ksym <= XK_9) {                 /* 0-9 keyboard */
			q = (kbds_quant * 10) + (ksym ^ XK_0);
			kbds_quant = q <= 99999999 ? q : kbds_quant;
			term.dirty[term.row-1] = 1;
			return 0;
		} else if (ksym >= XK_KP_0 && ksym <= XK_KP_9) {    /* 0-9 numpad */
			q = (kbds_quant * 10) + (ksym ^ XK_KP_0);
			kbds_quant = q <= 99999999 ? q : kbds_quant;
			term.dirty[term.row-1] = 1;
			return 0;
		} else if (ksym == XK_k || ksym == XK_h)
			i = ksym & 1;
		else if (ksym == XK_l || ksym == XK_j)
			i = ((ksym & 6) | 4) >> 1;
		else if (ksym >= XK_KP_Left && ksym <= XK_KP_Down)
			i = ksym - XK_KP_Left;
		else if ((XK_Home & ksym) != XK_Home || (i = (ksym ^ XK_Home) - 1) > 3)
			return 0;

		if (kbds_mode & KBDS_MODE_LSELECT && !(i & 1))
			return 0;

		kbds_quant = (kbds_quant ? kbds_quant : 1);

		if (i & 1) {
			kbds_c.y += kbds_quant * (i & 2 ? 1 : -1);
			if (kbds_c.y < 0)
				kscrollup(&((Arg){ .i = -kbds_c.y }));
			else if (kbds_c.y >= term.row)
				kscrolldown(&((Arg){ .i = kbds_c.y - term.row + 1 }));
		} else {
			for (line = TLINE(kbds_c.y); kbds_quant > 0;) {
				kbds_c.x += (i & 2 ? 1 : -1);
				if (kbds_c.x < 0 || kbds_c.x >= term.col)
					break;
				if (line[kbds_c.x].mode & ATTR_WDUMMY)
					continue;
				kbds_quant--;
			}
		}
		kbds_moveto(kbds_c.x, kbds_c.y);
	}
	kbds_selecttext();
	kbds_quant = 0;
	term.dirty[term.row-1] = 1;
	return 0;
}

int
kbds_isselectmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_SELECT);
}

int
kbds_issearchmode(void)
{
	return kbds_in_use && (kbds_mode & KBDS_MODE_SEARCH);
}
