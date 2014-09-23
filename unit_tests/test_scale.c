#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "tests.h"
#include "ncvideo.h"

bool test_scale()
{
	ncv_frame* f = ncv_frame_create(320, 240);
	ASSERT_RET(f);

	int width = ncv_frame_get_width(f);
	ASSERT_RET(width == 320);
	
	int height = ncv_frame_get_height(f);
	ASSERT_RET(height == 240);

	uint8_t* buffer = ncv_frame_get_buffer_rw(f);

	bool px = false;
	for(int y = 0; y < 240; y++){
		for(int x = 0; x < 320; x++){
			*(buffer++) = px ? 255 : 0;
			*(buffer++) = px ? 255 : 0;
			*(buffer++) = px ? 255 : 0;
			px = !px;				
		}
		px = !px;
	}

	ncv_frame* f2 = ncv_frame_create(640, 480);
	ASSERT_RET(f);

	width = ncv_frame_get_width(f2);
	ASSERT_RET(width == 640);

	height = ncv_frame_get_height(f2);
	ASSERT_RET(height == 480);

	ncv_error s = ncv_frame_scale(f, f2, 32, 32, 320 * 1.5, 240 * 1.5, NCV_SA_NEAREST_NEIGHBOR);
	ASSERT_RET(s == NCV_ERR_SUCCESS);
	
	s = ncv_frame_scale(f, f2, 64, 64, 320 * 2, 240 * 2, NCV_SA_NEAREST_NEIGHBOR);
	ASSERT_RET(s == NCV_ERR_SUCCESS);
	
	s = ncv_frame_scale(f, f2, -120, -30, 320 * 2, 240 * 2, NCV_SA_NEAREST_NEIGHBOR);
	ASSERT_RET(s == NCV_ERR_SUCCESS);

	s = ncv_frame_save_tga_file(f2, "image_scale.tga");
	ASSERT_RET(s == NCV_ERR_SUCCESS);

	ncv_frame_destroy(f);
	ncv_frame_destroy(f2);
	
	return true;
}
