#include <ncvideo.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#define ASSERT_MSG(_v, _ctx, ...) if(!(_v)){ char _tb[512]; int _el = snprintf(_tb, sizeof(_tb), __VA_ARGS__); \
	puts(_tb); if(_ctx){ ncv_report_error(_ctx, 1, _tb, _el); } exit(1); }

int main(int argc, char** argv)
{
	ncv_context* ctx = NULL;

	printf("hello\n");
	ASSERT_MSG(argc == 3, NULL, "usage: %s [message queue name] [frame data name]", argv[0]);

	printf("shm: %s\n", argv[1]);

	ctx = ncv_ctx_create();
	ASSERT_MSG(ctx, NULL, "could not create context");

	ncv_error err = ncv_connect(ctx, argv[1], argv[2]);
	ASSERT_MSG(err == NCV_ERR_SUCCESS, NULL, "could not connect: %d", err);

	const ncv_frame* frame = NULL;
	
	const char* const* const* args;
	int num_args = 0;

	ncv_get_args(ctx, &num_args, &args);
	int i = 0;

	while((err = ncv_wait_for_frame(ctx, NCV_INFINITE, &frame)) == NCV_ERR_SUCCESS){
		printf("frame, %d x %d at pos: %" PRId64 ", flags: %u!\n", ncv_frame_get_width(frame), 
			ncv_frame_get_height(frame), ncv_frame_get_byte_pos(frame), ncv_frame_get_flags(frame));
		i++;
	}

	char result[512];
	snprintf(result, sizeof(result), "got fed %d frames", i);
	err = ncv_report_result(ctx, NCV_INFINITE, result, strlen(result));

	if(err != NCV_ERR_SUCCESS){
		printf("failed to report result\n");
	}else{
		printf("sent result\n");
	}

	printf("num args: %d\n", num_args);
	for(int j = 0; j < num_args; j++){
		printf("argument: %s: %s\n", args[j][0], args[j][1]);
	}

	err = ncv_report_finished(ctx, NCV_INFINITE);
	ASSERT_MSG(err == NCV_ERR_SUCCESS, ctx, "error reporting finished: %d", err);

	ncv_ctx_destroy(&ctx);
	
	printf("quitting\n");

	return 0;
}
