#include <assert.h>
#include <libswscale/swscale.h>

#include "ncvideo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <libshmipc.h>
#include <inttypes.h>

#define NCV_ERROR_MSG_SIZE 2048
#define NCV_HEADER_SIZE 4096
#define NCV_PADDING_SIZE 4096
#define NCV_BPP 4

#pragma pack(4)
typedef struct __attribute__ ((packed)) shm_vid_info {
	uint32_t reserved;
	uint32_t width;
	uint32_t height;
	uint32_t flags;
	int64_t byte_pos;
	int64_t pts;
	int64_t dts;
	
	uint32_t tot_frames;
	float fps;
	char fps_guessed;

	double pts_seconds;
	double dts_seconds;

	char has_audio;
	int orig_sample_rate;
	int channels;
	int num_samples;
	char sample_format_str[16];
} shm_vid_info;
#pragma pack() // restore packing

struct ncv_frame
{
	int width, height;
	uint32_t flags;
	const uint8_t* buffer;
	uint8_t* rw_buffer;

	int num_samples;
	const float* audio_buffer;
	float* audio_buffer_rw;

	int64_t byte_pos;
	int64_t pts;
	int64_t dts;

	double pts_seconds;
	double dts_seconds;
};

struct ncv_context
{
	shmipc* read_queue, *write_queue;
	shmhandle* frame_shm;
	const void* shm_area;
	char*** args;
	int num_args;

	char error_msg[NCV_ERROR_MSG_SIZE];

	volatile shm_vid_info* info;
	ncv_frame frame;
};

#define NCV_PRINT_ERROR(_ctx, ...) snprintf((_ctx)->error_msg, NCV_ERROR_MSG_SIZE, __VA_ARGS__);
#define NCV_ASSERT_CLEANUP(_v, _e, ...) if(!(_v)){ ret = _e; snprintf(ctx->error_msg, NCV_ERROR_MSG_SIZE, __VA_ARGS__); goto cleanup; }

bool parse_args(shmipc* read_queue, int* out_num_args, char**** out_args)
{
	char type[SHMIPC_MESSAGE_TYPE_LENGTH];
	char message[shmipc_get_message_max_length(read_queue)];
	size_t size;
	
	shmipc_error serr = shmipc_recv_message(read_queue, type, message, &size, NCV_INFINITE);

	if(serr != SHMIPC_ERR_SUCCESS || strcmp(type, "arguments") != 0){
		printf("expceted argument, but got: %s\n", type);
		return false;
	}
	
	int num_args = atoi(message);

	char*** args = calloc(1, sizeof(char**) * num_args);
	if(!args)
		return false;

	for(int i = 0; i < num_args; i++){
		shmipc_error serr = shmipc_recv_message(read_queue, type, message, &size, NCV_INFINITE);

		if(serr != SHMIPC_ERR_SUCCESS)
			goto cleanup;

		args[i] = calloc(1, sizeof(char*) * 2);
		if(!args[i])
			goto cleanup;

		args[i][0] = strdup(type);
		args[i][1] = strdup(message);
	}

	*out_num_args = num_args;
	*out_args = args;

	return true;

cleanup:
	// TODO
	return false;
}

NCV_APIENTRY ncv_context* ncv_ctx_create()
{
	ncv_context* ctx = calloc(1, sizeof(ncv_context));
	return ctx;
}

NCV_APIENTRY ncv_error ncv_connect(ncv_context* ctx, const char* shm_queue_name, const char* shm_frame_name)
{
	ncv_error ret = NCV_ERR_UNKNOWN;

	char name_host_writer[512];
	snprintf(name_host_writer, sizeof(name_host_writer), "%s_host_writer", shm_queue_name);

	shmipc_error serr = shmipc_open(shm_queue_name, SHMIPC_AM_WRITE, &ctx->write_queue);
	NCV_ASSERT_CLEANUP(serr == SHMIPC_ERR_SUCCESS, NCV_ERR_SHM, "could not open write_queue: %d", serr);
	
	serr = shmipc_open(name_host_writer, SHMIPC_AM_READ, &ctx->read_queue);
	NCV_ASSERT_CLEANUP(serr == SHMIPC_ERR_SUCCESS, NCV_ERR_SHM, "could not open read_queue: %d", serr);

	size_t shmsize;
	serr = shmipc_open_shm_ro(shm_frame_name, &shmsize, &ctx->shm_area, &ctx->frame_shm);
	NCV_ASSERT_CLEANUP(serr == SHMIPC_ERR_SUCCESS, NCV_ERR_SHM, "could not open shm_area: %s (%d, %s)", 
		shm_frame_name, serr, shmipc_get_last_error_msg());

	NCV_ASSERT_CLEANUP(parse_args(ctx->read_queue, &ctx->num_args, &ctx->args), NCV_ERR_PARSING_ARGS, "could not parse arguments");

	ctx->info = (shm_vid_info*)ctx->shm_area;

	return NCV_ERR_SUCCESS;

cleanup:
	// TODO
	return ret;
}

void ncv_ctx_destroy(ncv_context** ctx)
{
	if((*ctx)->read_queue)
		shmipc_destroy(&(*ctx)->read_queue);
	
	if((*ctx)->write_queue)
		shmipc_destroy(&(*ctx)->write_queue);

	free(*ctx);
	*ctx = NULL;
}

ncv_error ncv_get_args(ncv_context* ctx, int* out_num_args, const char* const* const* * out_args)
{
	*out_args = (const char* const* const*)ctx->args;
	*out_num_args = ctx->num_args;
	return NCV_ERR_SUCCESS;
}

ncv_error ncv_get_num_frames(ncv_context* ctx, int* out_num_frames)
{
	*out_num_frames = ctx->info->tot_frames;
	return NCV_ERR_SUCCESS;
}

ncv_error ncv_get_frame_rate(ncv_context* ctx, float* out_fps, int* out_guessed)
{
	*out_fps = ctx->info->fps;
	*out_guessed = (int)ctx->info->fps_guessed ? true : false;
	return NCV_ERR_SUCCESS;
}

int ncv_get_width(ncv_context* ctx)
{
	return ctx->info->width;
}

int ncv_get_height(ncv_context* ctx)
{
	return ctx->info->height;
}

int ncv_get_audio_present(ncv_context* ctx)
{
	return ctx->info->has_audio;
}

int ncv_get_audio_channels(ncv_context* ctx)
{
	return ctx->info->channels;
}

NCV_APIENTRY ncv_error ncv_wait_for_frame(ncv_context* ctx, int timeout, const ncv_frame** frame)
{
	const char msg[] = "ready";
	shmipc_error serr = shmipc_send_message(ctx->write_queue, "status", msg, sizeof(msg), timeout); 

	if(serr != SHMIPC_ERR_TIMEOUT && serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	if(serr == SHMIPC_ERR_TIMEOUT)
		return NCV_ERR_TIMEOUT;

	char type[SHMIPC_MESSAGE_TYPE_LENGTH];
	char message[shmipc_get_message_max_length(ctx->read_queue)];
	size_t size;
	
	serr = shmipc_recv_message(ctx->read_queue, type, message, &size, timeout);

	if(serr != SHMIPC_ERR_TIMEOUT && serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	if(serr == SHMIPC_ERR_TIMEOUT)
		return NCV_ERR_TIMEOUT;

	if(strcmp(type, "cmd") != 0){
		NCV_PRINT_ERROR(ctx, "unexpected message type from frameserver: '%s'", type);
		return NCV_ERR_UNKNOWN_MSG;
	}
	
	if(!strcmp(message, "quit")){
		NCV_PRINT_ERROR(ctx, "unexpected message type from frameserver: '%s' (unexpected at this time)", message);
		return NCV_ERR_UNKNOWN_MSG;
	}
	
	if(!strcmp(message, "endsession"))
		return NCV_ERR_HOST_END_SESSION;
	
	if(!strcmp(message, "newframe")){
		memset(&ctx->frame, 0, sizeof(ncv_frame));

		ctx->frame.width = ctx->info->width;
		ctx->frame.height = ctx->info->height;
		ctx->frame.flags = ctx->info->flags;
		ctx->frame.byte_pos = ctx->info->byte_pos;
		ctx->frame.dts = ctx->info->dts;
		ctx->frame.pts = ctx->info->pts;
		ctx->frame.dts_seconds = ctx->info->dts_seconds;
		ctx->frame.pts_seconds = ctx->info->pts_seconds;

		int frame_buffer_size = ctx->frame.width * ctx->frame.height * NCV_BPP;

		ctx->frame.buffer = ((uint8_t*)ctx->shm_area + NCV_HEADER_SIZE);
		ctx->frame.audio_buffer = (const float*)(((uint8_t*)ctx->shm_area) + NCV_HEADER_SIZE + frame_buffer_size + NCV_PADDING_SIZE);
		ctx->frame.num_samples = ctx->info->num_samples;

		*frame = &ctx->frame;

		return NCV_ERR_SUCCESS;
	}

	NCV_PRINT_ERROR(ctx, "unexpected message from frameserver: type: '%s', msg: '%s'", type, message);
	return NCV_ERR_UNKNOWN_MSG;
}

const float* ncv_frame_get_audio_buffer(const ncv_frame* frame)
{
	return frame->audio_buffer;
}

float* ncv_frame_get_audio_buffer_rw(const ncv_frame* frame)
{
	return frame->audio_buffer_rw;
}

int ncv_frame_get_num_samples(const ncv_frame* frame)
{
	return frame->num_samples;
}

ncv_error ncv_report_error(ncv_context* ctx, int err_code, const char* err_str, size_t size)
{
	char buffer[SHMIPC_MESSAGE_TYPE_LENGTH] = {0};
	snprintf(buffer, sizeof(buffer), "error %d", err_code);
	shmipc_error serr = shmipc_send_message(ctx->write_queue, buffer, err_str, size, SHMIPC_INFINITE);

	if(serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	return NCV_ERR_SUCCESS;
}

ncv_error ncv_report_finished(ncv_context* ctx, int timeout)
{
	const char msg[] = "finished";
	shmipc_error serr = shmipc_send_message(ctx->write_queue, "status", msg, sizeof(msg), timeout); 

	if(serr != SHMIPC_ERR_TIMEOUT && serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	if(serr == SHMIPC_ERR_TIMEOUT)
		return NCV_ERR_TIMEOUT;

	char type[SHMIPC_MESSAGE_TYPE_LENGTH];
	char message[shmipc_get_message_max_length(ctx->read_queue)];
	size_t size;
	
	serr = shmipc_recv_message(ctx->read_queue, type, message, &size, timeout);

	if(serr != SHMIPC_ERR_TIMEOUT && serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	if(serr == SHMIPC_ERR_TIMEOUT)
		return NCV_ERR_TIMEOUT;

	if(strcmp(type, "cmd") != 0){
		NCV_PRINT_ERROR(ctx, "unexpected message type from frameserver: '%s'", type);
		return NCV_ERR_UNKNOWN_MSG;
	}

	if(serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;
	
	if(!strcmp(message, "quit")){
		return NCV_ERR_SUCCESS;
	}
		
	NCV_PRINT_ERROR(ctx, "unexpected message type from frameserver: '%s' (unexpected at this time)", message);
	return NCV_ERR_UNKNOWN_MSG;
}

ncv_error ncv_report_result(ncv_context* ctx, int timeout, const void* data, size_t size)
{
	if(shmipc_get_message_max_length(ctx->write_queue) < size)
		return NCV_ERR_RESULT_TOO_LONG;

	char* buffer;
	shmipc_error serr = shmipc_acquire_buffer_w(ctx->write_queue, &buffer, timeout);

	if(serr == SHMIPC_ERR_TIMEOUT)
		return NCV_ERR_TIMEOUT;

	if(serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	memcpy(buffer, data, size);
	
	serr = shmipc_return_buffer_w(ctx->write_queue, &buffer, size, "results");

	if(serr != SHMIPC_ERR_SUCCESS)
		return NCV_ERR_SHM;

	return NCV_ERR_SUCCESS;
}

const char* ncv_get_last_error_msg(ncv_context* ctx)
{
	return ctx->error_msg;
}

int ncv_frame_get_width(const ncv_frame* frame)
{
	return frame->width;
}

int ncv_frame_get_height(const ncv_frame* frame)
{
	return frame->height;
}

unsigned int ncv_frame_get_flags(const ncv_frame* frame)
{
	return frame->flags;
}

long long ncv_frame_get_byte_pos(const ncv_frame* frame)
{
	return frame->byte_pos;
}

const void* ncv_frame_get_buffer(const ncv_frame* frame)
{
	return frame->buffer;
}

void* ncv_frame_get_buffer_rw(const ncv_frame* frame)
{
	return frame->rw_buffer;
}

long long ncv_frame_get_dts(const ncv_frame* frame)
{
	return frame->dts;
}

long long ncv_frame_get_pts(const ncv_frame* frame)
{
	return frame->pts;
}

double ncv_frame_get_dts_seconds(const ncv_frame* frame)
{
	return frame->dts_seconds;
}

double ncv_frame_get_pts_seconds(const ncv_frame* frame)
{
	return frame->pts_seconds;
}

ncv_frame* ncv_frame_create(int width, int height)
{
	ncv_frame* ret = calloc(1, sizeof(ncv_frame));

	if(!ret)
		return NULL;

	ret->rw_buffer = av_mallocz(width * height * NCV_BPP); 

	if(!ret->rw_buffer){
		free(ret);
		return NULL;
	}

	ret->buffer = ret->rw_buffer;

	ret->width = width;
	ret->height = height;	

	return ret;
}

void ncv_frame_destroy(ncv_frame* frame)
{
	if(frame->rw_buffer)
		av_free(frame->rw_buffer);

	free(frame);
}

ncv_error ncv_frame_scale(const ncv_frame* source, ncv_frame* target, int tx, int ty, int tw, int th, ncv_scaling_algorithm algorithm)
{
	if(!target->rw_buffer)
		return NCV_ERR_TARGET_NOT_WRITABLE;

	struct SwsContext* sws = sws_getContext(
		// source
		source->width, source->height, AV_PIX_FMT_BGRA,

		// dest
		tw, th, AV_PIX_FMT_BGRA,

		// flags/alg
		algorithm == 0 ? SWS_FAST_BILINEAR : algorithm,

		NULL, NULL, 0
	);

	assert(sws);

	uint8_t* dst = av_malloc(tw * NCV_BPP * th);
	assert(dst);

	uint8_t* dstSlice[3] = {dst, NULL, NULL};
	int dstStride[3] = {tw * NCV_BPP, 0, 0};

	const uint8_t* srcSlice[3] = {(const uint8_t*)source->buffer, 0, 0};
	int srcStride[3] = {source->width * NCV_BPP, 0, 0};

	sws_scale(sws, (const uint8_t * const*)&srcSlice, 
		srcStride, 0, source->height, (uint8_t * const*)&dstSlice, dstStride);

#if 0
	// blit is completely within target, use faster copy
	if(tx >= 0 && ty >= 0 && tx + tw < target->width &&
		ty >= 0 && ty >= 0 && ty + th < target->height){
		uint8_t* px = target->rw_buffer + tx * NCV_BPP;
		uint8_t* dpx = dst;

		for(int y = 0; y < th; y++){
			memcpy(px, dpx, tw * NCV_BPP);
			px += target->width * NCV_BPP;
			dpx += tw * NCV_BPP;
		}
	}
#endif

	if(0){}

	// blit is only partly within target (or entirely outside), test every pixel
	else{
		for(int y = 0; y < th; y++){
			for(int x = 0; x < tw; x++){
				if(
					x + tx < target->width && x + tx >= 0 && 
					y + ty < target->height && y + ty >= 0
				){
					uint8_t* d = target->rw_buffer + (x + tx + (y + ty) * target->width) * NCV_BPP;
					uint8_t* s = dst + (x + y * tw) * NCV_BPP;
					
					*(d++) = *(s++);
					*(d++) = *(s++);
					*(d++) = *(s++);
					*(d++) = *(s++);
				}
			}
		}
	}

	av_free(dst);

	sws_freeContext(sws);

	return NCV_ERR_SUCCESS;
}

#define TGA_HEADER_SIZE 18

int ncv_frame_save_tga_mem(const ncv_frame* frame, char** out_buffer)
{
  char* buffer = calloc(1, frame->width * frame->height * 3 + TGA_HEADER_SIZE);
  if(!buffer)
    return 0;

  // uncompressed RGB
  buffer[2] = 2;

  // width
  buffer[12] = frame->width & 0xff;
  buffer[13] = (frame->width >> 8) & 0xff;

  // height
  buffer[14] = frame->height & 0xff;
  buffer[15] = (frame->height >> 8) & 0xff;

  // bits per pixel
  buffer[16] = 24;

  *out_buffer = buffer;

  buffer += TGA_HEADER_SIZE;

  for(int y = frame->height - 1; y >= 0; y--){
    for(int x = 0; x < frame->width; x++){
      int i = (x + y * frame->width) * NCV_BPP;

      *(buffer++) = ((const char*)frame->buffer)[i+0];
      *(buffer++) = ((const char*)frame->buffer)[i+1];
      *(buffer++) = ((const char*)frame->buffer)[i+2];
    }
  }

  return frame->width * frame->height * 3 + TGA_HEADER_SIZE;
}

ncv_error ncv_frame_save_tga_file(const ncv_frame* frame, const char* path)
{
	char* buffer;
	int size = ncv_frame_save_tga_mem(frame, &buffer);

	if(size == 0)
		return NCV_ERR_ALLOC;

	FILE* f = fopen(path, "wb");

	if(!f)
		return NCV_ERR_COULD_NOT_OPEN_FILE;
	
	int w = fwrite(buffer, size, 1, f);
	
	ncv_error ret = w ? NCV_ERR_SUCCESS : NCV_ERR_COULD_NOT_WRITE_TO_FILE;

	fclose(f);

	return ret;
}

ncv_error ncv_frame_flip_rgb_order(ncv_frame* frame)
{
#if 0
	if(!frame->rw_buffer)
		return NCV_ERR_TARGET_NOT_WRITABLE;

	for(int i = 0; i < frame->width * frame->height * NCV_BPP; i += NCV_BPP)
	{
		uint8_t tmp = frame->rw_buffer[i+1];
		frame->rw_buffer[i] = frame->rw_buffer[i+3];
		frame->rw_buffer[i+3] = tmp;
	}
#endif
	
	return NCV_ERR_SUCCESS;
}
