#ifndef SIXEL_H
#define SIXEL_H

#define DECSIXEL_PARAMS_MAX 16
#define DECSIXEL_PALETTE_MAX 1024
#define DECSIXEL_PARAMVALUE_MAX 65535
#define DECSIXEL_WIDTH_MAX 4096
#define DECSIXEL_HEIGHT_MAX 4096

typedef unsigned short sixel_color_no_t;
typedef unsigned int sixel_color_t;

typedef struct sixel_image_buffer {
	sixel_color_no_t *data;
	int width;
	int height;
	sixel_color_t *palette;
	int use_private_palette;
} sixel_image_t;

typedef enum parse_state {
	PS_ESC        = 1,  /* ESC */
	PS_DECSIXEL   = 2,  /* DECSIXEL body part ", $, -, ? ... ~ */
	PS_DECGRA     = 3,  /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
	PS_DECGRI     = 4,  /* DECGRI Graphics Repeat Introducer ! Pn Ch */
	PS_DECGCI     = 5,  /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
	PS_ERROR      = 6,
} parse_state_t;

typedef struct parser_context {
	parse_state_t state;
	int pos_x;
	int pos_y;
	int max_x;
	int max_y;
	int pan;
	int pad;
	int ph;
	int pv;
	int sixel_height;
	int transparent;
	int repeat_count;
	int color_index;
	int param;
	int nparams;
	int params[DECSIXEL_PARAMS_MAX];
	int use_private_palette;
	sixel_color_t shared_palette[DECSIXEL_PALETTE_MAX + 1];
	sixel_color_t private_palette[DECSIXEL_PALETTE_MAX + 1];
	sixel_image_t image;
} sixel_state_t;

void scroll_images(int n);
void delete_image(ImageList *im);
int sixel_parser_init(sixel_state_t *st, int par, int transparent, sixel_color_t bgcolor, unsigned char use_private_palette);
int sixel_parser_parse(sixel_state_t *st, const unsigned char *p, size_t len);
void sixel_parser_set_default_colors(sixel_state_t *st);
int sixel_parser_finalize(sixel_state_t *st, ImageList **newimages, int cx, int cy, int cw, int ch);
void sixel_parser_deinit(sixel_state_t *st);
Pixmap sixel_create_clipmask(char *pixels, int width, int height);

#endif
