/**
 * Data for boxdraw_extra.
 */

void drawextrasymbol(int x, int y, int w, int h, XftColor *fg, ushort symbol, int bold);
void initextrasymbols(void);

/* Box draw category (see boxdraw.h): */
#define BDE (8<<10)  /* Box extra */

/* Lower byte of U+2500 */
#define BE_HDASH3        0x04
#define BE_HDASH3_HEAVY  0x05
#define BE_VDASH3        0x06
#define BE_VDASH3_HEAVY  0x07
#define BE_HDASH4        0x08
#define BE_HDASH4_HEAVY  0x09
#define BE_VDASH4        0x0A
#define BE_VDASH4_HEAVY  0x0B
#define BE_HDASH2        0x4C
#define BE_HDASH2_HEAVY  0x4D
#define BE_VDASH2        0x4E
#define BE_VDASH2_HEAVY  0x4F
#define BE_ARC_DR        0x6D
#define BE_ARC_DL        0x6E
#define BE_ARC_UL        0x6F
#define BE_ARC_UR        0x70
#define BE_DIAG_RL       0x71
#define BE_DIAG_LR       0x72
#define BE_DIAG_CROSS    0x73

/* Number of misc characters (dashes/diagonals/rounded corners) plus one */
#define BE_MISC_LEN     (19+1)

/* Number of sextant characters */
#define BE_SEXTANTS_LEN  60

/* Number of wedges (U+1FB3C..U+1FB6F and U+1FB9A..U+1FB9B) */
#define BE_WEDGES_LEN  54

/* Index and number of legacy characters */
#define BE_LEGACY_IDX  BE_MISC_LEN
#define BE_LEGACY_LEN  (BE_SEXTANTS_LEN + BE_WEDGES_LEN + 52)

/* Index and number of octant characters */
#define BE_OCTANTS_IDX  (BE_LEGACY_IDX + BE_LEGACY_LEN)
#define BE_OCTANTS_LEN  230

/* Total number of extra characters */
#define BE_EXTRA_LEN  (BE_MISC_LEN + BE_LEGACY_LEN + BE_OCTANTS_LEN)

/**
 * The order of misc characters in the mask picture. The first character starts
 * at position one, because in this table zero means that the character is not
 * implemented.
 */
static const uchar boxmisc[256] = {
	[BE_HDASH3]       = 1,
	[BE_HDASH3_HEAVY] = 2,
	[BE_VDASH3]       = 3,
	[BE_VDASH3_HEAVY] = 4,
	[BE_HDASH4]       = 5,
	[BE_HDASH4_HEAVY] = 6,
	[BE_VDASH4]       = 7,
	[BE_VDASH4_HEAVY] = 8,
	[BE_HDASH2]       = 9,
	[BE_HDASH2_HEAVY] = 10,
	[BE_VDASH2]       = 11,
	[BE_VDASH2_HEAVY] = 12,
	[BE_ARC_DR]       = 13,
	[BE_ARC_DL]       = 14,
	[BE_ARC_UL]       = 15,
	[BE_ARC_UR]       = 16,
	[BE_DIAG_RL]      = 17,
	[BE_DIAG_LR]      = 18,
	[BE_DIAG_CROSS]   = 19
};

/**
 * The order of legacy characters in the mask picture.
 * (BE_LEGACY_IDX-1 must be added to the values when reading the array)
 */
uchar boxlegacy[256];

#define BLK1  (1 << 0)
#define BLK2  (1 << 1)
#define BLK3  (1 << 2)
#define BLK4  (1 << 3)
#define BLK5  (1 << 4)
#define BLK6  (1 << 5)
#define BLK7  (1 << 6)
#define BLK8  (1 << 7)

/**
 * Sextants: U+1FB00 .. U+1FB3B
 * Blocks:   +---+---+
 *           | 1 | 2 |
 *           +---+---+
 *           | 3 | 4 |
 *           +---+---+
 *           | 5 | 6 |
 *           +---+---+
 */
static uchar boxdatasextants[BE_SEXTANTS_LEN] = {
	BLK1,
	BLK2,
	BLK1 | BLK2,
	BLK3,
	BLK1 | BLK3,
	BLK2 | BLK3,
	BLK1 | BLK2 | BLK3,
	BLK4,
	BLK1 | BLK4,
	BLK2 | BLK4,
	BLK1 | BLK2 | BLK4,
	BLK3 | BLK4,
	BLK1 | BLK3 | BLK4,
	BLK2 | BLK3 | BLK4,
	BLK1 | BLK2 | BLK3 | BLK4,
	BLK5,
	BLK1 | BLK5,
	BLK2 | BLK5,
	BLK1 | BLK2 | BLK5,
	BLK3 | BLK5,
	BLK2 | BLK3 | BLK5,
	BLK1 | BLK2 | BLK3 | BLK5,
	BLK4 | BLK5,
	BLK1 | BLK4 | BLK5,
	BLK2 | BLK4 | BLK5,
	BLK1 | BLK2 | BLK4 | BLK5,
	BLK3 | BLK4 | BLK5,
	BLK1 | BLK3 | BLK4 | BLK5,
	BLK2 | BLK3 | BLK4 | BLK5,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK5,
	BLK6,
	BLK1 | BLK6,
	BLK2 | BLK6,
	BLK1 | BLK2 | BLK6,
	BLK3 | BLK6,
	BLK1 | BLK3 | BLK6,
	BLK2 | BLK3 | BLK6,
	BLK1 | BLK2 | BLK3 | BLK6,
	BLK4 | BLK6,
	BLK1 | BLK4 | BLK6,
	BLK1 | BLK2 | BLK4 | BLK6,
	BLK3 | BLK4 | BLK6,
	BLK1 | BLK3 | BLK4 | BLK6,
	BLK2 | BLK3 | BLK4 | BLK6,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK6,
	BLK5 | BLK6,
	BLK1 | BLK5 | BLK6,
	BLK2 | BLK5 | BLK6,
	BLK1 | BLK2 | BLK5 | BLK6,
	BLK3 | BLK5 | BLK6,
	BLK1 | BLK3 | BLK5 | BLK6,
	BLK2 | BLK3 | BLK5 | BLK6,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK6,
	BLK4 | BLK5 | BLK6,
	BLK1 | BLK4 | BLK5 | BLK6,
	BLK2 | BLK4 | BLK5 | BLK6,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK6,
	BLK3 | BLK4 | BLK5 | BLK6,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK6,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK6
};

/**
 * Octants: U+1CD00 .. U+1CDE5
 * Blocks:  +---+---+
 *          | 1 | 2 |
 *          +---+---+
 *          | 3 | 4 |
 *          +---+---+
 *          | 5 | 6 |
 *          +---+---+
 *          | 7 | 8 |
 *          +---+---+
 */
static uchar boxdataoctants[BE_OCTANTS_LEN] = {
	/* 00 .. 1F */
	BLK3,
	BLK2 | BLK3,
	BLK1 | BLK2 | BLK3,
	BLK4,
	BLK1 | BLK4,
	BLK1 | BLK2 | BLK4,
	BLK3 | BLK4,
	BLK1 | BLK3 | BLK4,
	BLK2 | BLK3 | BLK4,
	BLK5,
	BLK1 | BLK5,
	BLK2 | BLK5,
	BLK1 | BLK2 | BLK5,
	BLK1 | BLK3 | BLK5,
	BLK2 | BLK3 | BLK5,
	BLK1 | BLK2 | BLK3 | BLK5,
	BLK4 | BLK5,
	BLK1 | BLK4 | BLK5,
	BLK2 | BLK4 | BLK5,
	BLK1 | BLK2 | BLK4 | BLK5,
	BLK3 | BLK4 | BLK5,
	BLK1 | BLK3 | BLK4 | BLK5,
	BLK2 | BLK3 | BLK4 | BLK5,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK5,
	BLK6,
	BLK1 | BLK6,
	BLK2 | BLK6,
	BLK1 | BLK2 | BLK6,
	BLK3 | BLK6,
	BLK1 | BLK3 | BLK6,
	BLK2 | BLK3 | BLK6,
	BLK1 | BLK2 | BLK3 | BLK6,

	/* 20 .. 3F */
	BLK1 | BLK4 | BLK6,
	BLK2 | BLK4 | BLK6,
	BLK1 | BLK2 | BLK4 | BLK6,
	BLK3 | BLK4 | BLK6,
	BLK1 | BLK3 | BLK4 | BLK6,
	BLK2 | BLK3 | BLK4 | BLK6,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK6,
	BLK5 | BLK6,
	BLK1 | BLK5 | BLK6,
	BLK2 | BLK5 | BLK6,
	BLK1 | BLK2 | BLK5 | BLK6,
	BLK3 | BLK5 | BLK6,
	BLK1 | BLK3 | BLK5 | BLK6,
	BLK2 | BLK3 | BLK5 | BLK6,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK6,
	BLK4 | BLK5 | BLK6,
	BLK1 | BLK4 | BLK5 | BLK6,
	BLK2 | BLK4 | BLK5 | BLK6,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK6,
	BLK3 | BLK4 | BLK5 | BLK6,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK6,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK6,
	BLK1 | BLK7,
	BLK2 | BLK7,
	BLK1 | BLK2 | BLK7,
	BLK3 | BLK7,
	BLK1 | BLK3 | BLK7,
	BLK2 | BLK3 | BLK7,
	BLK1 | BLK2 | BLK3 | BLK7,
	BLK4 | BLK7,
	BLK1 | BLK4 | BLK7,
	BLK2 | BLK4 | BLK7,

	/* 40 .. 5F */
	BLK1 | BLK2 | BLK4 | BLK7,
	BLK3 | BLK4 | BLK7,
	BLK1 | BLK3 | BLK4 | BLK7,
	BLK2 | BLK3 | BLK4 | BLK7,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK7,
	BLK1 | BLK5 | BLK7,
	BLK2 | BLK5 | BLK7,
	BLK1 | BLK2 | BLK5 | BLK7,
	BLK3 | BLK5 | BLK7,
	BLK2 | BLK3 | BLK5 | BLK7,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK7,
	BLK4 | BLK5 | BLK7,
	BLK1 | BLK4 | BLK5 | BLK7,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK7,
	BLK3 | BLK4 | BLK5 | BLK7,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK7,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK7,
	BLK6 | BLK7,
	BLK1 | BLK6 | BLK7,
	BLK2 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK6 | BLK7,
	BLK3 | BLK6 | BLK7,
	BLK1 | BLK3 | BLK6 | BLK7,
	BLK2 | BLK3 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK3 | BLK6 | BLK7,
	BLK4 | BLK6 | BLK7,
	BLK1 | BLK4 | BLK6 | BLK7,
	BLK2 | BLK4 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK4 | BLK6 | BLK7,
	BLK3 | BLK4 | BLK6 | BLK7,
	BLK1 | BLK3 | BLK4 | BLK6 | BLK7,
	BLK2 | BLK3 | BLK4 | BLK6 | BLK7,

	/* 60 .. 7F */
	BLK1 | BLK2 | BLK3 | BLK4 | BLK6 | BLK7,
	BLK5 | BLK6 | BLK7,
	BLK1 | BLK5 | BLK6 | BLK7,
	BLK2 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK5 | BLK6 | BLK7,
	BLK3 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK3 | BLK5 | BLK6 | BLK7,
	BLK2 | BLK3 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK6 | BLK7,
	BLK4 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK2 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK3 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK5 | BLK6 | BLK7,
	BLK1 | BLK8,
	BLK2 | BLK8,
	BLK1 | BLK2 | BLK8,
	BLK3 | BLK8,
	BLK1 | BLK3 | BLK8,
	BLK2 | BLK3 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK8,
	BLK4 | BLK8,
	BLK1 | BLK4 | BLK8,
	BLK2 | BLK4 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK8,
	BLK3 | BLK4 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK8,

	/* 80 .. 9F */
	BLK5 | BLK8,
	BLK1 | BLK5 | BLK8,
	BLK2 | BLK5 | BLK8,
	BLK1 | BLK2 | BLK5 | BLK8,
	BLK3 | BLK5 | BLK8,
	BLK1 | BLK3 | BLK5 | BLK8,
	BLK2 | BLK3 | BLK5 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK8,
	BLK4 | BLK5 | BLK8,
	BLK1 | BLK4 | BLK5 | BLK8,
	BLK2 | BLK4 | BLK5 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK8,
	BLK3 | BLK4 | BLK5 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK5 | BLK8,
	BLK1 | BLK6 | BLK8,
	BLK2 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK6 | BLK8,
	BLK3 | BLK6 | BLK8,
	BLK2 | BLK3 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK6 | BLK8,
	BLK4 | BLK6 | BLK8,
	BLK1 | BLK4 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK6 | BLK8,
	BLK3 | BLK4 | BLK6 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK6 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK6 | BLK8,
	BLK5 | BLK6 | BLK8,
	BLK1 | BLK5 | BLK6 | BLK8,
	BLK2 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK5 | BLK6 | BLK8,

	/* A0 .. BF */
	BLK3 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK3 | BLK5 | BLK6 | BLK8,
	BLK2 | BLK3 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK6 | BLK8,
	BLK4 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK2 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK3 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK5 | BLK6 | BLK8,
	BLK1 | BLK7 | BLK8,
	BLK2 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK7 | BLK8,
	BLK3 | BLK7 | BLK8,
	BLK1 | BLK3 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK7 | BLK8,
	BLK4 | BLK7 | BLK8,
	BLK1 | BLK4 | BLK7 | BLK8,
	BLK2 | BLK4 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK7 | BLK8,
	BLK3 | BLK4 | BLK7 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK7 | BLK8,
	BLK5 | BLK7 | BLK8,
	BLK1 | BLK5 | BLK7 | BLK8,
	BLK2 | BLK5 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK5 | BLK7 | BLK8,
	BLK3 | BLK5 | BLK7 | BLK8,

	/* C0 .. E5 */
	BLK1 | BLK3 | BLK5 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK5 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK7 | BLK8,
	BLK4 | BLK5 | BLK7 | BLK8,
	BLK1 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK2 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK3 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK5 | BLK7 | BLK8,
	BLK6 | BLK7 | BLK8,
	BLK1 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK6 | BLK7 | BLK8,
	BLK3 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK3 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK6 | BLK7 | BLK8,
	BLK4 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK3 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK4 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK3 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK3 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK4 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK4 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK2 | BLK4 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK1 | BLK3 | BLK4 | BLK5 | BLK6 | BLK7 | BLK8,
	BLK2 | BLK3 | BLK4 | BLK5 | BLK6 | BLK7 | BLK8
};
