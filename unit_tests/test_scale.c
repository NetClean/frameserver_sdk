#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "tests.h"
#include "ncvideo.h"

#define TGA_HEADER_SIZE 18

ncv_frame* load_tga(const char* path)
{
	FILE* f = fopen(path, "rb");
	assert(f);

	uint8_t header[TGA_HEADER_SIZE];

	fread(header, 1, TGA_HEADER_SIZE, f);

	assert(header[2] == 2);
	assert(header[16] == 24);
	
	int width = header[12] | ((int)header[13] << 8);
	int height = header[14] | ((int)header[15] << 8);

	printf("%s %dx%d\n", path, width, height);

	ncv_frame* frame = ncv_frame_create(width, height);
	uint8_t* frame_buffer = ncv_frame_get_buffer_rw(frame); 

  for(int y = height - 1; y >= 0; y--){
    for(int x = 0; x < width; x++){
			char buffer[3];
			fread(buffer, 1, 3, f);

			uint8_t* px = frame_buffer + (x + y * width) * 3;
			
      *(px++) = buffer[2];
      *(px++) = buffer[1];
      *(px)   = buffer[0];
    }
  }

  return frame;
}

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

bool scole()
{
	ncv_frame* frame = load_tga("frame.tga");
	ncv_frame* target = ncv_frame_create(1280, 960);
	ncv_frame_scale(frame, target, 0, 0, 320, 240, NCV_SA_RECOMMENDED);
	return true;
}

bool do_scale(ncv_frame_flags alg, const char* path)
{
	ncv_frame* f = create_test_frame(320, 240);
	//ncv_frame* f = load_tga("frame.tga");
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
	return scole();

	if(!do_scale(NCV_SA_POINT, "image_scale_nn.tga"))
		return false;
	
	if(!do_scale(NCV_SA_FAST_BILINEAR, "image_scale_fast_bilinear.tga"))
		return false;
	
	if(!do_scale(NCV_SA_BICUBIC, "image_scale_bicubic.tga"))
		return false;
	
	return true;
}
