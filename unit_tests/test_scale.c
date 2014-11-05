#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "tests.h"
#include "ncvideo.h"

ncv_frame* create_test_frame(int w, int h)
{
	ncv_frame* f = ncv_frame_create(w, h);
	ASSERT_RET(f);

	int width = ncv_frame_get_width(f);
	ASSERT_RET(width == w);
	
	int height = ncv_frame_get_height(f);
	ASSERT_RET(height = h);

	uint8_t* buffer = ncv_frame_get_buffer_rw(f);

	bool px = false;
	for(int y = 0; y < h; y++){
		for(int x = 0; x < w; x++){
			*(buffer++) = px ? 255 : 0;
			*(buffer++) = px ? 255 : 0;
			*(buffer++) = px ? 255 : 0;
			px = !px;				
		}
		px = !px;
	}

	return f;
}

bool do_scale(ncv_frame_flags alg, const char* path)
{
	ncv_frame* f = create_test_frame(320, 240);
	ncv_frame* same_size = create_test_frame(640, 480);

	ncv_frame* f2 = ncv_frame_create(640, 480);
	ASSERT_RET(f);

	int width = ncv_frame_get_width(f2);
	ASSERT_RET(width == 640);

	int height = ncv_frame_get_height(f2);
	ASSERT_RET(height == 480);

	ncv_error s = ncv_frame_scale(same_size, f2, 0, 0, 640, 480, alg);
	ASSERT_RET(s == NCV_ERR_SUCCESS);

	s = ncv_frame_scale(f, f2, 32, 32, 320 * 1.5, 240 * 1.5, alg);
	ASSERT_RET(s == NCV_ERR_SUCCESS);
	
	s = ncv_frame_scale(f, f2, 64, 64, 320 * 2, 240 * 2, alg);
	ASSERT_RET(s == NCV_ERR_SUCCESS);
	
	s = ncv_frame_scale(f, f2, -120, -30, 320 * 2, 240 * 2, alg);
	ASSERT_RET(s == NCV_ERR_SUCCESS);

	s = ncv_frame_save_tga_file(f2, path);
	ASSERT_RET(s == NCV_ERR_SUCCESS);

	ncv_frame_destroy(f);
	ncv_frame_destroy(f2);
	
	return true;
}

bool test_scale()
{
	if(!do_scale(NCV_SA_POINT, "image_scale_nn.tga"))
		return false;
	
	if(!do_scale(NCV_SA_BICUBIC, "image_scale_bicubic.tga"))
		return false;
	
	return true;
}
