#include "ncvideo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <libshmipc.h>

#define NCV_ASSERT_CLEANUP(_v, _e, ...) if(!(_v)){ ret = _e; printf(__VA_ARGS__); puts(""); goto cleanup; }

#pragma pack(push,4)
typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t flags;
	int64_t byte_pos;
	int64_t pts;
	int64_t dts;

	uint32_t tot_frames;
	float fps;
} shm_vid_info;
#pragma pack(pop)


struct ncv_frame
{
	int width, height;
	uint32_t flags;
	const uint8_t* buffer;
	uint8_t* rw_buffer;

	int64_t byte_pos;
	int64_t pts;
	int64_t dts;
};

struct ncv_context
{
	shmipc* read_queue, *write_queue;
	shmhandle* frame_shm;
	const void* shm_area;
	char*** args;
	int num_args;

	volatile shm_vid_info* info;
	ncv_frame frame;
};

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

ncv_error ncv_ctx_create(const char* shm_queue_name, const char* shm_frame_name, ncv_context** out_context)
{
	ncv_error ret = NCV_ERR_UNKNOWN;

	ncv_context* ctx = calloc(1, sizeof(ncv_context));
	NCV_ASSERT_CLEANUP(ctx, NCV_ERR_ALLOC, "context allocation failed");

	char name_host_writer[512];
	snprintf(name_host_writer, sizeof(name_host_writer), "%s_host_writer", shm_queue_name);

	shmipc_error serr = shmipc_open(shm_queue_name, SHMIPC_AM_WRITE, &ctx->write_queue);
	NCV_ASSERT_CLEANUP(serr == SHMIPC_ERR_SUCCESS, NCV_ERR_SHM, "could not open write_queue: %d", serr);
	
	serr = shmipc_open(name_host_writer, SHMIPC_AM_READ, &ctx->read_queue);
	NCV_ASSERT_CLEANUP(serr == SHMIPC_ERR_SUCCESS, NCV_ERR_SHM, "could not open read_queue: %d", serr);

	size_t shmsize;
	serr = shmipc_open_shm_ro(shm_frame_name, &shmsize, &ctx->shm_area, &ctx->frame_shm);
	NCV_ASSERT_CLEANUP(serr == SHMIPC_ERR_SUCCESS, NCV_ERR_SHM, "could not open shm_area: %d", serr);

	NCV_ASSERT_CLEANUP(parse_args(ctx->read_queue, &ctx->num_args, &ctx->args), NCV_ERR_PARSING_ARGS, "could not parse arguments");

	ctx->info = (shm_vid_info*)ctx->shm_area;

	*out_context = ctx;

	return NCV_ERR_SUCCESS;

cleanup:
	// TODO
	return ret;
}

void ncv_ctx_destroy(ncv_context** ctx)
{
	shmipc_destroy(&(*ctx)->read_queue);
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

ncv_error ncv_get_frame_rate(ncv_context* ctx, float* out_fps)
{
	*out_fps = ctx->info->fps;
	return NCV_ERR_SUCCESS;
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

	if(strcmp(type, "cmd") != 0)
		return NCV_ERR_UNKNOWN_MSG;
	
	if(!strcmp(message, "quit"))
		return NCV_ERR_HOST_QUIT;
	
	if(!strcmp(message, "newframe")){
		ctx->frame.width = ctx->info->width;
		ctx->frame.height = ctx->info->height;
		ctx->frame.flags = ctx->info->flags;
		ctx->frame.byte_pos = ctx->info->byte_pos;
		ctx->frame.dts = ctx->info->dts;
		ctx->frame.pts = ctx->info->pts;
		
		ctx->frame.buffer = ((uint8_t*)ctx->shm_area + 4096);

		*frame = &ctx->frame;

		return NCV_ERR_SUCCESS;
	}

	return NCV_ERR_UNKNOWN_MSG;
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

ncv_frame* ncv_frame_create(int width, int height)
{
	ncv_frame* ret = calloc(1, sizeof(ncv_frame));

	if(!ret)
		return NULL;

	ret->rw_buffer = calloc(1, width * height * 3); 

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
		free(frame->rw_buffer);
	free(frame);
}

ncv_error ncv_frame_scale(const ncv_frame* source, ncv_frame* target, int tx, int ty, int tw, int th, ncv_scaling_algorithm algorithm)
{
	if(!target->rw_buffer)
		return NCV_ERR_TARGET_NOT_WRITABLE;

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

  // width
  buffer[14] = frame->height & 0xff;
  buffer[15] = (frame->height >> 8) & 0xff;

  // bits per pixel
  buffer[16] = 24;

  *out_buffer = buffer;

  buffer += TGA_HEADER_SIZE;

  for(int y = frame->height - 1; y >= 0; y--){
    for(int x = 0; x < frame->width; x++){
      int i = (x + y * frame->width) * 3;

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
	
	ncv_error ret = w ? NCV_ERR_SUCCESS : NCV_ERR_COULD_NOT_OPEN_FILE;

	fclose(f);

	return ret;
}
