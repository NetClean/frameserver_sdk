#ifndef VIDEOINT_H
#define VIDEOINT_H

#include "ncvideo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <libshmipc.h>

#define NCV_ERROR_MSG_SIZE 2048

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
	bool fps_guessed;
} shm_vid_info;


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

	char error_msg[NCV_ERROR_MSG_SIZE];

	volatile shm_vid_info* info;
	ncv_frame frame;
};

bool ncv_frame_scale_nn(const ncv_frame* source, ncv_frame* target, int tx, int ty, int tw, int th);
bool ncv_frame_scale_bicubic(const ncv_frame* source, ncv_frame* target, int tx, int ty, int tw, int th);

#endif
