#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "tests.h"
#include "ncvideo.h"

bool test_write_tga()
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
			*(buffer++) = 0;
			*(buffer++) = px ? 255 : 0;
			*(buffer++) = 0;
			*(buffer++) = 0;
			px = !px;				
		}
		px = !px;
	}

	ncv_error s = ncv_frame_save_tga_file(f, "image.tga");
	ASSERT_RET(s == NCV_ERR_SUCCESS);

	ncv_frame_destroy(f);
	
	return true;
}
