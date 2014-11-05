#ifndef NCVIDEO_H
#define NCVIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifdef _WIN32
	#ifdef DLL_EXPORT
		#define NCV_APIENTRY __cdecl __declspec( dllexport )
	#else
		#ifdef NCV_STATIC
			#define NCV_APIENTRY __cdecl
		#else
			#define NCV_APIENTRY __cdecl __declspec( dllimport )
		#endif
	#endif
#else
	#define NCV_APIENTRY
#endif

#define NCV_INFINITE -1

typedef enum {
	NCV_ERR_SUCCESS = 0,
	NCV_ERR_UNKNOWN = 1,
	NCV_ERR_ALLOC = 2,
	NCV_ERR_SHM = 3,
	NCV_ERR_TIMEOUT = 4,
	NCV_ERR_HOST_END_SESSION = 5,
	NCV_ERR_UNKNOWN_MSG = 6,
	NCV_ERR_RESULT_TOO_LONG = 7,
	NCV_ERR_PARSING_ARGS = 8,
	NCV_ERR_TARGET_NOT_WRITABLE = 9,
	NCV_ERR_COULD_NOT_OPEN_FILE = 10,
	NCV_ERR_COULD_NOT_WRITE_TO_FILE = 11,
} ncv_error;

typedef enum {
	NCV_FF_KEYFRAME = 1,
	VX_FF_BYTE_POS_GUESSED = 2,
	VX_FF_HAS_PTS = 4
} ncv_frame_flags;

// maps to libswscale
typedef enum {
	NCV_SA_RECOMMENDED = 0,

	NCV_SA_FAST_BILINEAR = 1,
	NCV_SA_BILINEAR = 2,
	NCV_SA_BICUBIC = 4,
	NCV_SA_X = 8,
	NCV_SA_POINT = 0x10,
	NCV_SA_AREA = 0x20,
	NCV_SA_BICUBLIN = 0x40,
	NCV_SA_GAUSS = 0x80,
	NCV_SA_SINC = 0x100,
	NCV_SA_LANCZOS = 0x200,
	NCV_SA_SPLINE = 0x400

} ncv_scaling_algorithm;

typedef struct ncv_context ncv_context;
typedef struct ncv_frame ncv_frame;

NCV_APIENTRY ncv_context* ncv_ctx_create();
NCV_APIENTRY ncv_error ncv_connect(ncv_context* ctx, const char* shm_queue_name, const char* shm_frame_name);
NCV_APIENTRY void ncv_ctx_destroy(ncv_context** ctx);
NCV_APIENTRY ncv_error ncv_wait_for_frame(ncv_context* ctx, int timeout, const ncv_frame** frame);
NCV_APIENTRY ncv_error ncv_report_result(ncv_context* ctx, int timeout, const void* data, size_t size);
NCV_APIENTRY ncv_error ncv_get_args(ncv_context* ctx, int* out_num_args, const char* const* const* * out_args);
NCV_APIENTRY ncv_error ncv_get_num_frames(ncv_context* ctx, int* out_num_frames);
NCV_APIENTRY ncv_error ncv_get_frame_rate(ncv_context* ctx, float* out_fps, int* out_guessed);
NCV_APIENTRY ncv_error ncv_report_error(ncv_context* ctx, int err_code, const char* err_str, size_t size);
NCV_APIENTRY const char* ncv_get_last_error_msg(ncv_context* ctx);

NCV_APIENTRY ncv_error ncv_report_finished(ncv_context* ctx, int timeout);

NCV_APIENTRY int ncv_frame_get_width(const ncv_frame* frame);
NCV_APIENTRY int ncv_frame_get_height(const ncv_frame* frame);
NCV_APIENTRY unsigned int ncv_frame_get_flags(const ncv_frame* frame);
NCV_APIENTRY long long ncv_frame_get_byte_pos(const ncv_frame* frame);
NCV_APIENTRY const void* ncv_frame_get_buffer(const ncv_frame* frame);
NCV_APIENTRY void* ncv_frame_get_buffer_rw(const ncv_frame* frame);
NCV_APIENTRY long long ncv_frame_get_dts(const ncv_frame* frame);
NCV_APIENTRY long long ncv_frame_get_pts(const ncv_frame* frame);

NCV_APIENTRY ncv_frame* ncv_frame_create(int width, int height);
NCV_APIENTRY void ncv_frame_destroy(ncv_frame* frame);

NCV_APIENTRY ncv_error ncv_frame_flip_rgb_order(ncv_frame* frame);

NCV_APIENTRY ncv_error ncv_frame_scale(const ncv_frame* source, ncv_frame* target, int x, int y, 
	int width, int height, ncv_scaling_algorithm algorithm);

NCV_APIENTRY int ncv_frame_save_tga_mem(const ncv_frame* frame, char** out_buffer);
NCV_APIENTRY ncv_error ncv_frame_save_tga_file(const ncv_frame* frame, const char* path);

#ifdef __cplusplus
}
#endif

#endif

