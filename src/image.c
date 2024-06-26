#define WOB_FILE "buffer.c"

#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "image.h"
#include "log.h"

struct wob_image *
wob_image_create_argb8888(size_t width, size_t height)
{
	int shmid = -1;
	char shm_name[NAME_MAX];
	for (int i = 0; i < UCHAR_MAX; ++i) {
		if (snprintf(shm_name, NAME_MAX, "/wob-%d", i) >= NAME_MAX) {
			break;
		}
		shmid = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (shmid > 0 || errno != EEXIST) {
			break;
		}
	}

	if (shmid < 0) {
		wob_log_error("shm_open() failed: %s", strerror(errno));
		return NULL;
	}

	if (shm_unlink(shm_name) != 0) {
		wob_log_error("shm_unlink() failed: %s", strerror(errno));
		return NULL;
	}

	size_t size = width * height * 8;
	if (ftruncate(shmid, size) != 0) {
		wob_log_error("ftruncate() failed: %s", strerror(errno));
		return NULL;
	}

	void *buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
	if (buffer == MAP_FAILED) {
		wob_log_error("mmap() failed: %s", strerror(errno));
		return NULL;
	}

	struct wob_image *image = malloc(sizeof(struct wob_image));

	image->shmid = shmid;
	image->width = width;
	image->height = height;
	image->size_in_bytes = width * height * 4;
	image->data = buffer;

	return image;
}

void
wob_image_destroy(struct wob_image *image)
{
	free(image);
}

bool
corners(float x,float y,float r)
{
	return ( (x < 0) && (y < 0) && ((x*x)+(y*y) > (r*r)) );
}

bool
outside_corners(float x,float y,float l,float t,float h,float w,float r)
{
	float tl_x = ((x-l)-r)-x;
	float tl_y = (y-t)-r;
	bool tl = corners(tl_x,tl_y,r);
	float bl_x = (x-l)-r;
	float bl_y = ((t+h)-r)-y;
	bool bl = corners(bl_x,bl_y,r);
	float br_x = ((l+w)-r)-x;
	float br_y = ((t+h)-r)-y;
	bool br = corners(br_x,br_y,r);
	float tr_x = ((l+w)-r)-x;
	float tr_y = (y-t)-r;
	bool tr = corners(tr_x,tr_y,r);
	bool outside = ( (x < l) ||
			(x > (l+w)) ||
			(y < t) ||
			(y > (t+h)) ||
			tl ||
			bl ||
			br ||
			tr );
	return outside;
}

void
fill_rectangle(uint32_t *pixels, size_t width, size_t height, size_t stride, uint32_t color)
{
	float fheight;
	fheight = height;
	float fwidth;
	fwidth = width;
	float fleft = 0;
	float ftop = 0;

	uint32_t cornercolor = 0;
	float radius = 15;
	printf("\n");
	for (size_t y = 0; y < height; ++y) {
		float fy;
		fy = y;
		for (size_t x = 0; x < width; ++x) {
			float fx;
			fx = x;
			bool outside = outside_corners(fx,fy,fleft,ftop,fheight,fwidth,radius);
			if (outside) {
				pixels[x] = cornercolor;
				printf(" ");
			} else {
				pixels[x] = color;
				printf("0");
			}
		}
	printf("\n");
	pixels += stride;
	}
}

void
wob_image_draw(struct wob_image *image, struct wob_colors colors, struct wob_dimensions dimensions, unsigned long percentage, unsigned long maximum)
{
	uint32_t bar_color = wob_color_to_argb(wob_color_premultiply_alpha(colors.value));
	uint32_t background_color = wob_color_to_argb(wob_color_premultiply_alpha(colors.background));
	uint32_t border_color = wob_color_to_argb(wob_color_premultiply_alpha(colors.border));

	uint32_t *data;
	uint32_t height;
	uint32_t width;
	uint32_t offset;
	uint32_t stride = image->width;

	height = dimensions.height;
	width = dimensions.width;
	data = image->data;
	fill_rectangle(data, width, height, stride, background_color);

	offset = dimensions.border_offset;
	height = image->height - (2 * offset);
	width = image->width - (2 * offset);
	data = image->data + (offset * (dimensions.width + 1));
	fill_rectangle(data, width, height, stride, border_color);

	offset = dimensions.border_offset + dimensions.border_size;
	height = image->height - (2 * offset);
	width = image->width - (2 * offset);
	data = image->data + (offset * (dimensions.width + 1));
	fill_rectangle(data, width, height, stride, background_color);

	offset = dimensions.border_offset + dimensions.border_size + dimensions.bar_padding;
	size_t bar_width = dimensions.width - 2 * offset;
	size_t bar_height = dimensions.height - 2 * offset;
	switch (dimensions.orientation) {
		case WOB_ORIENTATION_HORIZONTAL:
			height = bar_height;
			width = bar_width * percentage / maximum;
			data = image->data + (offset * (dimensions.width + 1));
			fill_rectangle(data, width, height, stride, bar_color);
			break;
		case WOB_ORIENTATION_VERTICAL:
			height = bar_height * percentage / maximum;
			width = bar_width;
			data = image->data + (offset * (dimensions.width + 1)) + (bar_height - height) * dimensions.width;
			fill_rectangle(data, width, height, stride, bar_color);
			break;
	}
}
