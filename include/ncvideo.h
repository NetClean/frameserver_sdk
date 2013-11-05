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
	NCV_ERR_HOST_QUIT = 5,
	NCV_ERR_UNKNOWN_MSG = 6,
	NCV_ERR_RESULT_TOO_LONG = 7,
	NCV_ERR_PARSING_ARGS = 8,
} ncv_error;

typedef struct ncv_context ncv_context;

NCV_APIENTRY ncv_error ncv_ctx_create(const char* shm_queue_name, const char* shm_frame_name, ncv_context** out_context);
NCV_APIENTRY void ncv_ctx_destroy(ncv_context** ctx);
NCV_APIENTRY ncv_error ncv_wait_for_frame(ncv_context* ctx, int timeout, int* out_width, int* out_height, void** out_frame);
NCV_APIENTRY ncv_error ncv_report_result(ncv_context* ctx, int timeout, void* data, size_t size);
NCV_APIENTRY ncv_error ncv_get_args(ncv_context* ctx, int* out_num_args, const char* const* const* * out_args);
NCV_APIENTRY ncv_error ncv_get_num_frames(ncv_context* ctx, int* out_num_frames);

#ifdef __cplusplus
}
#endif

#endif

