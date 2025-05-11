#include <X11/Xft/Xft.h>
#include <hb.h>
#include <hb-ft.h>

#define FEATURE(c1,c2,c3,c4) { .tag = HB_TAG(c1,c2,c3,c4), .value = 1, .start = HB_FEATURE_GLOBAL_START, .end = HB_FEATURE_GLOBAL_END }

typedef struct {
	hb_buffer_t *buffer;
	hb_glyph_info_t *glyphs;
	hb_glyph_position_t *positions;
	unsigned int count;
} HbTransformData;

void hbcreatebuffer(void);
void hbdestroybuffer(void);
void hbunloadfonts(void);
void hbtransform(HbTransformData *, XftFont *, const Glyph *, int, int);
