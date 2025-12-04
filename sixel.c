// sixel.c (part of mintty)
// originally written by kmiya@cluti (https://github.com/saitoha/sixel/blob/master/fromsixel.c)
// Licensed under the terms of the GNU General Public License v3 or later.

#include <stdlib.h>
#include <string.h>  /* memcpy */

#include "st.h"
#include "win.h"
#include "sixel.h"
#include "sixel_hls.h"

#define SIXEL_RGB(r, g, b) (((uint)255 << 24) + ((uint)(r) << 16) + ((uint)(g) << 8) + (uint)(b))
#define SIXEL_PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))
#define SIXEL_XRGB(r,g,b) SIXEL_RGB(SIXEL_PALVAL(r, 255, 100), SIXEL_PALVAL(g, 255, 100), SIXEL_PALVAL(b, 255, 100))

static sixel_color_t const sixel_default_color_table[] = {
	SIXEL_XRGB( 0,  0,  0),  /*  0 Black    */
	SIXEL_XRGB(20, 20, 80),  /*  1 Blue     */
	SIXEL_XRGB(80, 13, 13),  /*  2 Red      */
	SIXEL_XRGB(20, 80, 20),  /*  3 Green    */
	SIXEL_XRGB(80, 20, 80),  /*  4 Magenta  */
	SIXEL_XRGB(20, 80, 80),  /*  5 Cyan     */
	SIXEL_XRGB(80, 80, 20),  /*  6 Yellow   */
	SIXEL_XRGB(53, 53, 53),  /*  7 Gray 50% */
	SIXEL_XRGB(26, 26, 26),  /*  8 Gray 25% */
	SIXEL_XRGB(33, 33, 60),  /*  9 Blue*    */
	SIXEL_XRGB(60, 26, 26),  /* 10 Red*     */
	SIXEL_XRGB(33, 60, 33),  /* 11 Green*   */
	SIXEL_XRGB(60, 33, 60),  /* 12 Magenta* */
	SIXEL_XRGB(33, 60, 60),  /* 13 Cyan*    */
	SIXEL_XRGB(60, 60, 33),  /* 14 Yellow*  */
	SIXEL_XRGB(80, 80, 80),  /* 15 Gray 75% */
};

void
scroll_images(int n) {
	ImageList *im, *next;
	int top = tisaltscr() ? 0 : term.scr - term.histsize;

	for (im = term.images; im; im = next) {
		next = im->next;
		im->y += n;

		/* check if the current sixel has exceeded the maximum
		 * draw distance, and should therefore be deleted */
		if (im->y < top)
			delete_image(im);
	}
}

void
delete_image(ImageList *im)
{
	if (im->prev)
		im->prev->next = im->next;
	else
		term.images = im->next;
	if (im->next)
		im->next->prev = im->prev;
	if (im->pixmap)
		XFreePixmap(xw.dpy, (Drawable)im->pixmap);
	if (im->clipmask)
		XFreePixmap(xw.dpy, (Drawable)im->clipmask);
	free(im->pixels);
	free(im);
}

static void
set_default_colors(sixel_color_t *palette, int setall)
{
	int i, n, r, g, b;

	/* palette initialization */
	memcpy(&palette[1], sixel_default_color_table,
	       sizeof(sixel_default_color_table));

	/* colors 17-232 are a 6x6x6 color cube */
	for (n = 17, r = 0; r < 6; r++) {
		for (g = 0; g < 6; g++) {
			for (b = 0; b < 6; b++)
				palette[n++] = SIXEL_RGB(r * 51, g * 51, b * 51);
		}
	}

	/* colors 233-256 are a grayscale ramp, intentionally leaving out */
	for (i = 0; i < 24; i++)
		palette[n++] = SIXEL_RGB(i * 11, i * 11, i * 11);

	if (setall) {
		memset(&palette[257], 0xff,
		       (DECSIXEL_PALETTE_MAX - 256) * sizeof(sixel_color_t));
	}
}

static int
sixel_image_init(
    sixel_image_t    *image,
    int              width,
    int              height,
    int              bgcolor,
    int              use_private_palette,
    sixel_color_t    *palette)
{
	size_t size;

	if (width <= 0 || height <= 0)
		width = height = 0;

	image->width = width;
	image->height = height;
	image->data = NULL;

	size = (size_t)(width * height) * sizeof(sixel_color_no_t);
	if (size > 0) {
		if ((image->data = (sixel_color_no_t *)malloc(size)) == NULL)
			return -1;
		memset(image->data, 0, size);
	}

	image->use_private_palette = use_private_palette;
	image->palette = palette;
	image->palette[0] = bgcolor;
	if (use_private_palette)
		set_default_colors(image->palette, 0);
	return 0;
}


static int
image_buffer_resize(
    sixel_image_t   *image,
    int              width,
    int              height)
{
	size_t size;
	sixel_color_no_t *alt_buffer;
	int n, min_height;

	width = MIN(width, DECSIXEL_WIDTH_MAX);
	height = MIN(height, DECSIXEL_HEIGHT_MAX);
	min_height = MIN(height, image->height);

	if (image->width == width && image->height == height)
		return 0;

	size = (size_t)(width * height) * sizeof(sixel_color_no_t);
	alt_buffer = (sixel_color_no_t *)malloc(size);
	if (alt_buffer == NULL) {
		/* free source image */
		free(image->data);
		image->data = NULL;
		return -1;
	}

	if (image->data == NULL) {
		memset(alt_buffer, 0, size);
		goto end;
	}

	if (width > image->width) {  /* if width is extended */
		for (n = 0; n < min_height; ++n) {
			/* copy from source image */
			memcpy(alt_buffer + width * n,
			       image->data + image->width * n,
			       (size_t)image->width * sizeof(sixel_color_no_t));
			/* fill extended area with background color */
			memset(alt_buffer + width * n + image->width, 0,
			       (size_t)(width - image->width) * sizeof(sixel_color_no_t));
		}
	} else {
		for (n = 0; n < min_height; ++n) {
			/* copy from source image */
			memcpy(alt_buffer + width * n,
			       image->data + image->width * n,
			       (size_t)width * sizeof(sixel_color_no_t));
		}
	}

	if (height > image->height) {  /* if height is extended */
		/* fill extended area with background color */
		memset(alt_buffer + width * image->height, 0,
		       (size_t)(width * (height - image->height)) * sizeof(sixel_color_no_t));
	}

	/* free source image */
	free(image->data);
end:
	image->data = alt_buffer;
	image->width = width;
	image->height = height;
	return 0;
}

static void
sixel_image_deinit(sixel_image_t *image)
{
	free(image->data);
	image->data = NULL;
}

static inline void
parse_number(int *param, char c)
{
	*param = *param * 10 + c - '0';
	*param = MIN(*param, DECSIXEL_PARAMVALUE_MAX);
}

static inline void
save_param(sixel_state_t *st, int *param)
{
	if (st->nparams < DECSIXEL_PARAMS_MAX)
		st->params[st->nparams++] = *param;
	*param = 0;
}

static inline int
draw_unscaled_sixel(sixel_color_no_t *data,
                    int width,
                    int bits,
                    int count,
                    int color)
{
	int n, x;

	if (count == 1) {
		if (bits & 0x01) { *data = color; n = 0; }; data += width;
		if (bits & 0x02) { *data = color; n = 1; }; data += width;
		if (bits & 0x04) { *data = color; n = 2; }; data += width;
		if (bits & 0x08) { *data = color; n = 3; }; data += width;
		if (bits & 0x10) { *data = color; n = 4; }; data += width;
		if (bits & 0x20) { *data = color; n = 5; }
		return n;
	}

	for (n = -1; bits; bits >>= 1, n++, data += width) {
		if (bits & 1) {
			data[0] = color;
			data[1] = color;
			for (x = 2; x < count; x++)
				data[x] = color;
		}
	}
	return n;
}

static inline int
draw_scaled_sixel(sixel_color_no_t *data,
                  int width,
                  int bits,
                  int count,
                  int color,
                  int pan)
{
	int n, x, y;

	if (count == 1) {
		for (n = -1; bits; bits >>= 1, n++) {
			if (bits & 1) {
				*data = color;
				data += width;
				*data = color;
				data += width;
				for (y = 2; y < pan; y++, data += width)
					*data = color;
			} else {
				data += width * pan;
			}
		}
	} else {
		for (n = -1; bits; bits >>= 1, n++) {
			if (bits & 1) {
				y = 0;
				do {
					data[0] = color;
					data[1] = color;
					for (x = 2; x < count; x++)
						data[x] = color;
					data += width;
				} while (++y < pan);
			} else {
				data += width * pan;
			}
		}
	}
	return (n + 1) * pan - 1;
}

static inline void
draw_sixel(sixel_state_t *st, int bits)
{
	sixel_image_t *image = &st->image;
	sixel_color_no_t *data;
	int n, sx, sy;
	int max_x = st->pos_x + st->repeat_count;
	int max_y = st->pos_y + st->sixel_height;

	if (image->width < max_x || image->height < max_y) {
		sx = MAX(image->width, 64);
		sy = MAX(image->height, 60);
		while (sx < max_x)
			sx *= 2;
		while (sy < max_y)
			sy *= 2;
		if (image_buffer_resize(image, sx, sy) < 0) {
			perror("sixel: draw_sixel() failed");
			st->state = PS_ERROR;
			return;
		}
		if (image->height < max_y) {
			st->pos_x += st->repeat_count;
			st->pos_x = MIN(st->pos_x, image->width);
			return;
		}
	}

	if (st->pos_x + st->repeat_count > image->width)
		st->repeat_count = image->width - st->pos_x;

	if (bits && st->repeat_count > 0) {
		data = image->data + image->width * st->pos_y + st->pos_x;
		if (st->pan == 1) {
			n = draw_unscaled_sixel(data, image->width, bits,
			                        st->repeat_count, st->color_index);
		} else {
			n = draw_scaled_sixel(data, image->width, bits,
			                      st->repeat_count, st->color_index, st->pan);
		}
		if (st->max_x < (st->pos_x + st->repeat_count - 1))
			st->max_x = st->pos_x + st->repeat_count - 1;
		if (st->max_y < (st->pos_y + n))
			st->max_y = st->pos_y + n;
	}
	st->pos_x += st->repeat_count;
	st->repeat_count = st->pad;
}

static inline void
decgra(sixel_state_t *st,
       const unsigned char **p,
       const unsigned char *p2)
{
	sixel_image_t *image = &st->image;
	int sx, sy, max_height;

	do {
		switch (**p) {
		case '\x1b':
			st->state = PS_ESC;
			return;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			parse_number(&st->param, **p);
			break;
		case ';':
			save_param(st, &st->param);
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			/* ignore whitespace */
			break;
		default:
			save_param(st, &st->param);
			if (st->nparams > 0 && st->params[0] > 0)
				st->pan = st->params[0];
			if (st->nparams > 1 && st->params[1] > 0)
				st->pad = st->params[1];
			if (st->nparams > 2 && st->params[2] > 0)
				st->ph = MAX(st->ph, st->params[2]);
			if (st->nparams > 3 && st->params[3] > 0)
				st->pv = MAX(st->pv, st->pos_y + st->params[3]);

			/* round pixel aspect ratio up to nearest integer */
			if (st->pan > st->pad && st->pad > 0) {
				st->pan = (st->pan + st->pad - 1) / st->pad;
				st->pan = MAX(st->pan, 1);
				st->pad = 1;
			} else if (st->pad > st->pan && st->pan > 0) {
				st->pad = (st->pad + st->pan - 1) / st->pan;
				st->pad = MAX(st->pad, 1);
				st->pan = 1;
			} else {
				st->pan = st->pad = 1;
			}

			st->repeat_count = st->pad;
			st->sixel_height = st->pan * 6;
			max_height = MAX(st->pv, st->sixel_height);

			if (image->width < st->ph || image->height < max_height) {
				sx = MAX(image->width, st->ph);
				sy = MAX(image->height, max_height);

				/* the height of the image buffer must be divisible by
				 * sixel height to avoid unnecessary resizing of the
				 * buffer when rendering the last sixel line */
				sy = sy + st->sixel_height - 1;
				sy = sy / st->sixel_height * st->sixel_height;

				if (image_buffer_resize(image, sx, sy) < 0) {
					perror("sixel: decgra() failed");
					st->state = PS_ERROR;
					return;
				}
			}
			st->nparams = 0;
			st->state = PS_DECSIXEL;
			return;
		}
	} while (++(*p) < p2);
}

static inline void
decgri(sixel_state_t *st,
       const unsigned char **p,
       const unsigned char *p2)
{
	do {
		switch (**p) {
		case '\x1b':
			st->state = PS_ESC;
			return;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			parse_number(&st->param, **p);
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			/* ignore whitespace */
			break;
		default:
			st->repeat_count = st->pad * MAX(st->param, 1);
			st->param = 0;
			st->nparams = 0;
			st->state = PS_DECSIXEL;
			return;
		}
	} while (++(*p) < p2);
}

static inline void
decgci(sixel_state_t *st,
       const unsigned char **p,
       const unsigned char *p2)
{
	sixel_image_t *image = &st->image;

	do {
		switch (**p) {
		case '\x1b':
			st->state = PS_ESC;
			return;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			parse_number(&st->param, **p);
			break;
		case ';':
			save_param(st, &st->param);
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			/* ignore whitespace */
			break;
		default:
			save_param(st, &st->param);
			if (st->nparams > 0) {
				st->color_index = st->params[0] + 1; /* offset by 1 (background) */
				LIMIT(st->color_index, 1, DECSIXEL_PALETTE_MAX);
			}
			if (st->nparams > 4) {
				if (st->params[1] == 1) {
					/* HLS */
					st->params[2] = MIN(st->params[2], 360);
					st->params[3] = MIN(st->params[3], 100);
					st->params[4] = MIN(st->params[4], 100);
					image->palette[st->color_index]
					    = hls_to_rgb(st->params[2], st->params[3], st->params[4]);
				} else if (st->params[1] == 2) {
					/* RGB */
					st->params[2] = MIN(st->params[2], 100);
					st->params[3] = MIN(st->params[3], 100);
					st->params[4] = MIN(st->params[4], 100);
					image->palette[st->color_index]
					    = SIXEL_XRGB(st->params[2], st->params[3], st->params[4]);
				}
			}
			st->state = PS_DECSIXEL;
			return;
		}
	} while (++(*p) < p2);
}

int
sixel_parser_init(sixel_state_t *st,
                  int par,
                  int transparent,
                  sixel_color_t bgcolor,
                  unsigned char use_private_palette)
{
	int status;

	st->pos_x = 0;
	st->pos_y = 0;
	st->max_x = 0;
	st->max_y = 0;
	st->ph = 0;
	st->pv = 0;
	st->transparent = transparent;
	st->repeat_count = 1;
	st->color_index = 16;
	st->nparams = 0;
	st->param = 0;
	st->use_private_palette = use_private_palette;

	/* pixel aspect ratio */
	switch (par) {
	case 0:
	case 1:
	case 5:
	case 6:
		st->pan = 2;
		break;
	case 2:
		st->pan = 5;
		break;
	case 3:
	case 4:
		st->pan = 3;
		break;
	case 7:
	case 8:
	case 9:
	default:
		st->pan = 1;
		break;
	}
	st->pad = 1;
	st->sixel_height = st->pan * 6;

	/* buffer initialization */
	status = sixel_image_init(&st->image, 0, 0, transparent ? 0 : bgcolor,
	                          st->use_private_palette,
	                          st->use_private_palette ? st->private_palette : st->shared_palette);
	st->state = (status < 0) ? PS_ERROR : PS_DECSIXEL;
	return status;
}

void
sixel_parser_set_default_colors(sixel_state_t *st)
{
	set_default_colors(st->shared_palette, 1);
	set_default_colors(st->private_palette, 1);
}

int
sixel_parser_finalize(sixel_state_t *st, ImageList **newimages, int cx, int cy, int cw, int ch)
{
	sixel_image_t *image = &st->image;
	int x, y, w, h;
	int i, j, cols, numimages;
	sixel_color_no_t *src;
	sixel_color_t *dst, color;
	char trans;
	ImageList *im, *next, *tail;

	if (st->state == PS_ERROR || !image->data)
		return -1;

	if (++st->max_x < st->ph)
		st->max_x = st->ph;

	if (++st->max_y < st->pv)
		st->max_y = st->pv;

	w = MIN(st->max_x, image->width);
	h = MIN(st->max_y, image->height);

	if ((numimages = (h + ch-1) / ch) <= 0)
		return -1;

	cols = (w + cw-1) / cw;

	*newimages = NULL, tail = NULL;
	for (y = 0, i = 0; i < numimages; i++) {
		if ((im = malloc(sizeof(ImageList)))) {
			if (!tail) {
				*newimages = tail = im;
				im->prev = im->next = NULL;
			} else {
				tail->next = im;
				im->prev = tail;
				im->next = NULL;
				tail = im;
			}
			im->x = cx;
			im->y = cy + i;
			im->cols = cols;
			im->width = w;
			im->height = MIN(h - ch * i, ch);
			im->pixels = malloc(im->width * im->height * 4);
			im->pixmap = NULL;
			im->clipmask = NULL;
			im->cw = cw;
			im->ch = ch;
		}
		if (!im || !im->pixels) {
			for (im = *newimages; im; im = next) {
				next = im->next;
				if (im->pixels)
					free(im->pixels);
				free(im);
			}
			*newimages = NULL;
			return -1;
		}
		dst = (sixel_color_t *)im->pixels;
		for (trans = 0, j = 0; j < im->height && y < h; j++, y++) {
			src = st->image.data + image->width * y;
			for (x = 0; x < w; x++) {
				color = st->image.palette[*src++];
				trans |= (color == 0);
				*dst++ = color;
			}
		}
		im->transparent = (st->transparent && trans);
	}

	return numimages;
}

/* convert sixel data into indexed pixel bytes and palette data */
int
sixel_parser_parse(sixel_state_t *st, const unsigned char *p, size_t len)
{
	const unsigned char *p0 = p, *p2 = p + len;

	while (p < p2) {
		switch (st->state) {
		case PS_ESC:
			goto end;

		case PS_DECSIXEL:
			switch (*p) {
			case '\x1b':
				st->state = PS_ESC;
				break;
			case '"':
				st->param = 0;
				st->nparams = 0;
				st->state = PS_DECGRA;
				p++;
				break;
			case '!':
				st->param = 0;
				st->nparams = 0;
				st->state = PS_DECGRI;
				p++;
				break;
			case '#':
				st->param = 0;
				st->nparams = 0;
				st->state = PS_DECGCI;
				p++;
				break;
			case '$':
				/* DECGCR Graphics Carriage Return */
				st->pos_x = 0;
				p++;
				break;
			case '-':
				/* DECGNL Graphics Next Line */
				st->pos_x = 0;
				st->pos_y = MIN(st->pos_y + st->sixel_height, DECSIXEL_HEIGHT_MAX);
				p++;
				break;
			default:
				if (*p >= '?' && *p <= '~')
					draw_sixel(st, *p - '?');
				p++;
				break;
			}
			break;

		case PS_DECGRA:
			/* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
			decgra(st, &p, p2);
			break;

		case PS_DECGRI:
			/* DECGRI Graphics Repeat Introducer ! Pn Ch */
			decgri(st, &p, p2);
			break;

		case PS_DECGCI:
			/* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
			decgci(st, &p, p2);
			break;

		case PS_ERROR:
			if (*p == '\x1b') {
				st->state = PS_ESC;
				goto end;
			}
			p++;
			break;
		default:
			st->state = PS_ERROR;
			break;
		}
	}

end:
	return p - p0;
}

void
sixel_parser_deinit(sixel_state_t *st)
{
	if (st)
		sixel_image_deinit(&st->image);
}

Pixmap
sixel_create_clipmask(char *pixels, int width, int height)
{
	char c, *clipdata, *dst;
	uint b;
	int i, n, y, w;
	int msb = (XBitmapBitOrder(xw.dpy) == MSBFirst);
	sixel_color_t *src = (sixel_color_t *)pixels;
	Pixmap clipmask;

	clipdata = dst = malloc((width+7)/8 * height);
	if (!clipdata)
		return (Pixmap)None;

	for (y = 0; y < height; y++) {
		for (w = width; w > 0; w -= n) {
			n = MIN(w, 8);
			if (msb) {
				for (b = 0x80, c = 0, i = 0; i < n; i++, b >>= 1)
					c |= (*src++) ? b : 0;
			} else {
				for (b = 0x01, c = 0, i = 0; i < n; i++, b <<= 1)
					c |= (*src++) ? b : 0;
			}
			*dst++ = c;
		}
	}

	clipmask = XCreateBitmapFromData(xw.dpy, xw.win, clipdata, width, height);
	free(clipdata);
	return clipmask;
}
