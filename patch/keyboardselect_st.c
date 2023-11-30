#include <wctype.h>

static int kbds_in_use, kbds_quant;
static int kbds_type_old = SEL_REGULAR, kbds_type = SEL_REGULAR;
static int kbds_mode, kbds_searchlen, kbds_searchdir, kbds_searchcase;
static Rune *kbds_searchstr;
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
		g.u = kbds_searchdir < 0 ? '/' : '?';
		xdrawglyph(g, 0, y);
		for (i = 0; i < term.col-1; i++) {
			g.u = i < kbds_searchlen ? kbds_searchstr[i] : ' ';
			g.mode = (i == kbds_searchlen) ? ATTR_NULL : ATTR_REVERSE;
			xdrawglyph(g, i + 1, y);
		}
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

void
kbds_search(int dir)
{
	Glyph *line;
	int i, s, x, y;
	int yoff = (term.histf - term.scr), bot = term.col*(term.row + term.histf) - 1;

	if (!dir || !kbds_searchlen)
		return;

	for (i = term.col*(kbds_c.y + yoff) + kbds_c.x + dir; i >= 0 && i <= bot; i += dir) {
		for (s = 0; s < kbds_searchlen && i + s <= bot; s++) {
			x = (i + s) % term.col;
			y = (i + s) / term.col - yoff;
			if ((kbds_searchcase && kbds_searchstr[s] != TLINE(y)[x].u) ||
			    (!kbds_searchcase && towlower(kbds_searchstr[s]) != towlower(TLINE(y)[x].u)))
				break;
		}
		if (s == kbds_searchlen) {
			kbds_c.x = i % term.col;
			kbds_c.y = i / term.col - yoff;
			if (kbds_c.y < 0)
				kscrollup(&((Arg){ .i = -kbds_c.y }));
			else if (kbds_c.y >= term.row)
				kscrolldown(&((Arg){ .i = kbds_c.y - term.row + 1 }));
			LIMIT(kbds_c.y, 0, term.row-1);
			if (kbds_mode & KBDS_MODE_LSELECT) {
				kbds_type = SEL_REGULAR;
				kbds_setmode(kbds_mode & ~(KBDS_MODE_LSELECT));
			}
			kbds_selecttext();
			if (--kbds_quant > 0)
				return kbds_search(dir);
			break;
		}
	}
	kbds_quant = 0;
}

int
kbds_isdelim(Line line, int x, int y, int len, const char *delims)
{
	int top = -term.histf + term.scr, bot = term.row + term.scr - 1;

	if (x >= len) {
		if (len <= 0 || !(line[len-1].mode & ATTR_WRAP) || ++y > bot)
			return 1;
		line = TLINE(y);
		x = 0;
	} else if (x < 0) {
		if (--y < top)
			return 1;
		line = TLINE(y);
		x = tlinelen(line)-1;
		if (x < 0 || !(line[x].mode & ATTR_WRAP))
			return 1;
	}
	return !(line[x].mode & ATTR_SET) || line[x].u == 0 ||
	       (line[x].u < 128 && strchr(delims, (int)line[x].u) != NULL);
}

void
kbds_nextword(int start, int dir, const char *delims)
{
	int top = -term.histf + term.scr, bot = term.row + term.scr - 1;
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
		if (!kbds_isdelim(line, x, y, len, delims) &&
		    kbds_isdelim(line, x + xoff, y, len, delims))
		{
			if (y < 0)
				kscrollup(&((Arg){ .i = -y }));
			if (y >= term.row)
				kscrolldown(&((Arg){ .i = y - term.row + 1 }));
			LIMIT(y, 0, term.row-1);
			kbds_c.x = x, kbds_c.y = y;
			top = -term.histf + term.scr, bot = term.row + term.scr - 1;
			kbds_quant--;
		}
	}
	kbds_quant = 0;
}

int
kbds_keyboardhandler(KeySym ksym, char *buf, int len, int forcequit)
{
	int i, q, *xy;

	if (kbds_mode & KBDS_MODE_SEARCH && !forcequit) {
		switch (ksym) {
			case XK_Escape:
				kbds_searchlen = 0;
				/* FALLTHROUGH */
			case XK_Return:
				for (kbds_searchcase = 0, i = 0; i < kbds_searchlen; i++) {
					if (kbds_searchstr[i] != towlower(kbds_searchstr[i])) {
						kbds_searchcase = 1;
						break;
					}
				}
				kbds_search(kbds_searchdir);
				kbds_setmode(kbds_mode ^ KBDS_MODE_SEARCH);
				break;
			case XK_BackSpace:
				if (!kbds_searchlen)
					return 0;
				kbds_searchstr[--kbds_searchlen] = ' ';
				break;
			default:
				if (len < 1 || kbds_searchlen >= term.col-1)
					return 0;
				utf8decode(buf, &kbds_searchstr[kbds_searchlen], len);
				kbds_searchlen++;
				break;
		}
		term.dirty[term.row-1] = 1;
		return 0;
	}

	switch (ksym) {
	case -1:
		kbds_searchstr = xmalloc(term.col * sizeof(Rune));
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
		if (kbds_mode & KBDS_MODE_LSELECT)
			break;
		selextend(kbds_c.x, kbds_c.y, kbds_type ^= (SEL_REGULAR | SEL_RECTANGULAR), 0);
		selextend(kbds_c.x, kbds_c.y, kbds_type, 0);
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
		tfulldirt();
		return MODE_KBDSELECT;
	case XK_n:
	case XK_N:
		kbds_search(ksym == XK_n ? kbds_searchdir : -kbds_searchdir);
		break;
	case XK_BackSpace:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_c.x = 0;
		break;
	case XK_exclam:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_c.x = term.col / 2;
		break;
	case XK_underscore:
		if (!(kbds_mode & KBDS_MODE_LSELECT))
			kbds_c.x = term.col-1;
		break;
	case XK_dollar:
		if (!(kbds_mode & KBDS_MODE_LSELECT)) {
			kbds_c.x = tlinelen(TLINE(kbds_c.y))-1;
			kbds_c.x = MAX(kbds_c.x, 0);
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
		kbds_c.y = MIN(term.c.y + term.scr, term.row-1) / 2;
		break;
	case XK_L:
		kbds_c.y = MIN(term.c.y + term.scr, term.row-1);
		break;
	case XK_Page_Up:
	case XK_KP_Page_Up:
	case XK_K:
		i = term.scr;
		kscrollup(&((Arg){ .i = term.row }));
		kbds_c.y = MAX(0, kbds_c.y - term.row + term.scr - i);
		break;
	case XK_Page_Down:
	case XK_KP_Page_Down:
	case XK_J:
		i = term.scr;
		kscrolldown(&((Arg){ .i = term.row }));
		kbds_c.y = MIN(MIN(term.c.y + term.scr, term.row-1), kbds_c.y + term.row + term.scr - i);
		break;
	case XK_asterisk:
	case XK_KP_Multiply:
		if (!(kbds_mode & KBDS_MODE_LSELECT)) {
			kbds_c.x = term.col / 2;
			kbds_c.y = (term.row-1) / 2;
		}
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

		xy = i & 1 ? &kbds_c.y : &kbds_c.x;
		*xy += (kbds_quant ? kbds_quant : 1) * (i & 2 ? 1 : -1);

		if (i & 1 && *xy < 0)
			kscrollup(&((Arg){ .i = -*xy }));
		else if (i & 1 && *xy >= term.row)
			kscrolldown(&((Arg){ .i = *xy - term.row + 1 }));

		LIMIT(kbds_c.x, 0, term.col-1);
		LIMIT(kbds_c.y, 0, term.row-1);
	}
	kbds_selecttext();
	kbds_quant = 0;
	term.dirty[term.row-1] = 1;
	return 0;
}

int
kbds_isselectmode(void)
{
	return kbds_in_use && kbds_mode & KBDS_MODE_SELECT;
}
