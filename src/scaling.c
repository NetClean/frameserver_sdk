#include "videoint.h"

bool ncv_frame_scale_nn(const ncv_frame* source, ncv_frame* target, int tx, int ty, int tw, int th)
{
	float sx = (float)source->width / (float)tw;
	float sy = (float)source->height / (float)th;
	
	for(int y = 0; y < th; y++){
		for(int x = 0; x < tw; x++){
			if(y + ty >= 0 && x + tx >= 0 && x + tx < target->width && y + ty < target->height){
				const uint8_t* sd = source->buffer + (((int)(x * sx)) + ((int)(y * sy) * source->width)) * 3;
				uint8_t* td = target->rw_buffer + (x + tx + (y + ty) * target->width) * 3;

				*(td++) = *(sd++);
				*(td++) = *(sd++);
				*(td++) = *(sd);
			}
		}
	}

	return true;
}

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a > b ? a : b; }

static float* frame_to_float(const ncv_frame* f)
{
	float* data = calloc(1, f->width * f->height * 4 * sizeof(float));
	if(!data)
		return NULL;
	
	float* fpx = data;
	const uint8_t* bpx = f->buffer;

	for(int i = 0; i < f->width * f->height; i++){
		*(fpx++) = *(bpx++) / 255.0f;
		*(fpx++) = *(bpx++) / 255.0f;
		*(fpx++) = *(bpx++) / 255.0f;
		*(fpx++) = 1.0f;
	}

	return data;
}

// bicubic scaling algorithm from libpipi

/*
 *  libpipi       Pathetic image processing interface library
 *  Copyright (c) 2004-2010 Sam Hocevar <sam@hocevar.net>
 *                All Rights Reserved
 *
 *  $Id: bicubic.c 4696 2010-10-17 00:57:27Z sam $
 *
 *  This library is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What The Fuck You Want
 *  To Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

bool ncv_frame_scale_bicubic(const ncv_frame* source, ncv_frame* target, int tx, int ty, int tw, int th)
{
	bool ret = false;
	float *p0, *p1, *p2, *p3;

	float* srcdata = frame_to_float(source);
	if(!srcdata)
		goto cleanup;

	int x, y, i, sw, sh, i0, i1, i2, i3;
	float scalex, scaley;

	sw = source->width;
	sh = source->height;

	scalex = tw > 1 ? (float)(sw - 1) / (tw - 1) : 1.0f;
	scaley = th > 1 ? (float)(sh - 1) / (th - 1) : 1.0f;

	for(y = 0; y < th; y++)
	{
		float yfloat = scaley * y;
		int yint = (int)yfloat;
		float y1 = yfloat - yint;

		p0 = srcdata + 4 * sw * min_int(max_int(0, yint - 1), sh - 1);
		p1 = srcdata + 4 * sw * min_int(max_int(0, yint    ), sh - 1);
		p2 = srcdata + 4 * sw * min_int(max_int(0, yint + 1), sh - 1);
		p3 = srcdata + 4 * sw * min_int(max_int(0, yint + 2), sh - 1);

		for (x = 0; x < tw; x++)
		{
			float xfloat = scalex * x;
			int xint = (int)xfloat;
			float x1 = xfloat - xint;

			i0 = 4 * min_int(max_int(0, xint - 1), sw - 1);
			i1 = 4 * min_int(max_int(0, xint    ), sw - 1);
			i2 = 4 * min_int(max_int(0, xint + 1), sw - 1);
			i3 = 4 * min_int(max_int(0, xint + 2), sw - 1);

			for (i = 0; i < 4; i++, i0++, i1++, i2++, i3++)
			{
				float a00 = p1[i1];
				float a01 = .5f * (p2[i1] - p0[i1]);
				float a02 = p0[i1] - 2.5f * p1[i1]
					+ 2.f * p2[i1] - .5f * p3[i1];
				float a03 = .5f * (p3[i1] - p0[i1]) + 1.5f * (p1[i1] - p2[i1]);

				float a10 = .5f * (p1[i2] - p1[i0]);
				float a11 = .25f * (p0[i0] - p2[i0] - p0[i2] + p2[i2]);
				float a12 = .5f * (p0[i2] - p0[i0]) + 1.25f * (p1[i0] - p1[i2])
					+ .25f * (p3[i0] - p3[i2]) + p2[i2] - p2[i0];
				float a13 = .25f * (p0[i0] - p3[i0] - p0[i2] + p3[i2])
					+ .75f * (p2[i0] - p1[i0] + p1[i2] - p2[i2]);

				float a20 = p1[i0] - 2.5f * p1[i1]
					+ 2.f * p1[i2] - .5f * p1[i3];
				float a21 = .5f * (p2[i0] - p0[i0]) + 1.25f * (p0[i1] - p2[i1])
					+ .25f * (p0[i3] - p2[i3]) - p0[i2] + p2[i2];
				float a22 = p0[i0] - p3[i2] - 2.5f * (p1[i0] + p0[i1])
					+ 2.f * (p2[i0] + p0[i2]) - .5f * (p3[i0] + p0[i3])
					+ 6.25f * p1[i1] - 5.f * (p2[i1] + p1[i2])
					+ 1.25f * (p3[i1] + p1[i3])
					+ 4.f * p2[i2] - p2[i3] + .25f * p3[i3];
				float a23 = 1.5f * (p1[i0] - p2[i0]) + .5f * (p3[i0] - p0[i0])
					+ 1.25f * (p0[i1] - p3[i1])
					+ 3.75f * (p2[i1] - p1[i1]) + p3[i2] - p0[i2]
					+ 3.f * (p1[i2] - p2[i2]) + .25f * (p0[i3] - p3[i3])
					+ .75f * (p2[i3] - p1[i3]);

				float a30 = .5f * (p1[i3] - p1[i0]) + 1.5f * (p1[i1] - p1[i2]);
				float a31 = .25f * (p0[i0] - p2[i0]) + .25f * (p2[i3] - p0[i3])
					+ .75f * (p2[i1] - p0[i1] + p0[i2] - p2[i2]);
				float a32 = -.5f * p0[i0] + 1.25f * p1[i0] - p2[i0]
					+ .25f * p3[i0] + 1.5f * p0[i1] - 3.75f * p1[i1]
					+ 3.f * p2[i1] - .75f * p3[i1] - 1.5f * p0[i2]
					+ 3.75f * p1[i2] - 3.f * p2[i2] + .75f * p3[i2]
					+ .5f * p0[i3] - 1.25f * p1[i3] + p2[i3]
					- .25f * p3[i3];
				float a33 = .25f * p0[i0] - .75f * p1[i0] + .75f * p2[i0]
					- .25f * p3[i0] - .75f * p0[i1] + 2.25f * p1[i1]
					- 2.25f * p2[i1] + .75f * p3[i1] + .75f * p0[i2]
					- 2.25f * p1[i2] + 2.25f * p2[i2] - .75f * p3[i2]
					- .25f * p0[i3] + .75f * p1[i3] - .75f * p2[i3]
					+ .25f * p3[i3];

				float y2 = y1 * y1; float y3 = y2 * y1;
				float x2 = x1 * x1; float x3 = x2 * x1;

				float p = a00 + a01 * y1 + a02 * y2 + a03 * y3 +
					+ a10 * x1 + a11 * x1 * y1 + a12 * x1 * y2 + a13 * x1 * y3
					+ a20 * x2 + a21 * x2 * y1 + a22 * x2 * y2 + a23 * x2 * y3
					+ a30 * x3 + a31 * x3 * y1 + a32 * x3 * y2 + a33 * x3 * y3;

				if (p < 0.0f)
					p = 0.0f;

				if (p > 1.0f)
					p = 1.0f;

				if(x + tx >= 0 && x + tx < target->width && y + ty >= 0 && y + ty < target->height && i < 3)
					target->rw_buffer[((y + ty) * target->width + x + tx) * 3 + i] = p * 255.0f;
			}
		}
	}

	ret = true;

cleanup:
	if(srcdata)
		free(srcdata);

	return ret;
}
