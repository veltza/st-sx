/**
 * Data for boxdraw_extra.
 */

void drawextrasymbol(int x, int y, int w, int h, XftColor *fg, ushort symbol, int bold);

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

/* Number of characters plus one */
#define BE_NUM_CHARS     (19+1)

/**
 * The order of characters in the mask picture. The first character starts at
 * position one, because in this table zero means that the character is not
 * implemented.
 */
static const unsigned char boxdataextra[256] = {
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
