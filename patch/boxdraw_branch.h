/**
 * Data for branch drawing symbols U+F5D0..U+F60D
 */

void drawbranchsymbol(int, int, int, int, XftColor *, ushort);

/* Box draw category (see boxdraw.h): */
#define BRS (7<<10)  /* Box Branch symbols */

/* Data */
#define BSFR  (1<<0)   /* fade line to right */
#define BSFL  (1<<1)   /* fade line to left */
#define BSFD  (1<<2)   /* fade line to down */
#define BSFU  (1<<3)   /* fade line to up */

#define BSABR (1<<4)   /* arc from bottom to right */
#define BSABL (1<<5)   /* arc from bottom to left */
#define BSATR (1<<6)   /* arc from top to right */
#define BSATL (1<<7)   /* arc from top to left */

#define BSCN  (1<<8)   /* commit non-merged */
#define BSCM  (1<<9)   /* commit merged */

#define BSLR  (1<<10)  /* line right */
#define BSLL  (1<<11)  /* line left */
#define BSLD  (1<<12)  /* line down */
#define BSLU  (1<<13)  /* line up */

#define BSLH  (BSLR + BSLL)  /* line horizontal */
#define BSLV  (BSLD + BSLU)  /* line vertical */

#define BSLH_IDX    0
#define BSLV_IDX    1
#define BSABR_INDX  6
#define BSABL_INDX  7
#define BSATR_INDX  8
#define BSATL_INDX  9
#define BSCM_INDX   30

static const unsigned int branchsymbols[] = {
	BSLH,                             /* U+F5D0 */
	BSLV,                             /* U+F5D1 */
	BSFR,                             /* U+F5D2 */
	BSFL,                             /* U+F5D3 */
	BSFD,                             /* U+F5D4 */
	BSFU,                             /* U+F5D5 */

	BSABR,                            /* U+F5D6 */
	BSABL,                            /* U+F5D7 */
	BSATR,                            /* U+F5D8 */
	BSATL,                            /* U+F5D9 */

	BSATR + BSLV,                     /* U+F5DA */
	BSABR + BSLV,                     /* U+F5DB */
	BSATR + BSABR,                    /* U+F5DC */
	BSATL + BSLV,                     /* U+F5DD */
	BSABL + BSLV,                     /* U+F5DE */
	BSATL + BSABL,                    /* U+F5DF */

	BSABL + BSLH,                     /* U+F5E0 */
	BSABR + BSLH,                     /* U+F5E1 */
	BSABL + BSABR,                    /* U+F5E2 */
	BSATL + BSLH,                     /* U+F5E3 */
	BSATR + BSLH,                     /* U+F5E4 */
	BSATL + BSATR,                    /* U+F5E5 */

	BSATL + BSATR + BSLV,             /* U+F5E6 */
	BSABL + BSABR + BSLV,             /* U+F5E7 */
	BSATL + BSABL + BSLH,             /* U+F5E8 */
	BSATR + BSABR + BSLH,             /* U+F5E9 */

	BSATL + BSABR + BSLV,             /* U+F5EA */
	BSATR + BSABL + BSLV,             /* U+F5EB */
	BSATL + BSABR + BSLH,             /* U+F5EC */
	BSATR + BSABL + BSLH,             /* U+F5ED */

	BSCM,                             /* U+F5EE */
	BSCN,                             /* U+F5EF */

	BSCM + BSLR,                      /* U+F5F0 */
	BSCN + BSLR,                      /* U+F5F1 */
	BSCM + BSLL,                      /* U+F5F2 */
	BSCN + BSLL,                      /* U+F5F3 */

	BSCM + BSLH,                      /* U+F5F4 */
	BSCN + BSLL + BSLR,               /* U+F5F5 */

	BSCM + BSLD,                      /* U+F5F6 */
	BSCN + BSLD,                      /* U+F5F7 */
	BSCM + BSLU,                      /* U+F5F8 */
	BSCN + BSLU,                      /* U+F5F9 */

	BSCM + BSLV,                      /* U+F5FA */
	BSCN + BSLU + BSLD,               /* U+F5FB */

	BSCM + BSLR + BSLD,               /* U+F5FC */
	BSCN + BSLR + BSLD,               /* U+F5FD */
	BSCM + BSLL + BSLD,               /* U+F5FE */
	BSCN + BSLL + BSLD,               /* U+F5FF */
	BSCM + BSLR + BSLU,               /* U+F600 */
	BSCN + BSLR + BSLU,               /* U+F601 */
	BSCM + BSLL + BSLU,               /* U+F602 */
	BSCN + BSLL + BSLU,               /* U+F603 */

	BSCM + BSLR + BSLV,               /* U+F604 */
	BSCN + BSLR + BSLU + BSLD,        /* U+F605 */
	BSCM + BSLL + BSLV,               /* U+F606 */
	BSCN + BSLL + BSLU + BSLD,        /* U+F607 */

	BSCM + BSLD + BSLH,               /* U+F608 */
	BSCN + BSLD + BSLL + BSLR,        /* U+F609 */
	BSCM + BSLU + BSLH,               /* U+F60A */
	BSCN + BSLU + BSLL + BSLR,        /* U+F60B */

	BSCM + BSLV + BSLH,               /* U+F60C */
	BSCN + BSLL + BSLR + BSLU + BSLD, /* U+F60D */
};
