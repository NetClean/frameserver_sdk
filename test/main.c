#include <ncvideo.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define ASSERT_MSG(_v, ...) if(!(_v)){ printf(__VA_ARGS__); puts(""); exit(1); }

int main(int argc, char** argv)
{
	ncv_context* ctx;

	printf("hello\n");
	ASSERT_MSG(argc == 2, "expeced an shm name");

	printf("shm: %s\n", argv[1]);

	ncv_error err = ncv_ctx_create(argv[1], &ctx);
	ASSERT_MSG(err == NCV_ERR_SUCCESS, "could not create context: %d", err);

	int width = 0, height = 0;
	uint8_t* frame = NULL;
	
	while((err = ncv_wait_for_frame(ctx, -1, &width, &height, (void**)&frame)) == NCV_ERR_SUCCESS){
		printf("frame!\n");
	}
	
	ncv_ctx_destroy(&ctx);

	return 0;
}
