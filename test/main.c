#include <ncvideo.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define ASSERT_MSG(_v, ...) if(!(_v)){ printf(__VA_ARGS__); puts(""); exit(1); }

int main(int argc, char** argv)
{
	ncv_context* ctx;

	printf("hello\n");
	ASSERT_MSG(argc == 3, "usage: %s [message queue name] [frame data name]", argv[0]);

	printf("shm: %s\n", argv[1]);

	ncv_error err = ncv_ctx_create(argv[1], argv[2], &ctx);
	ASSERT_MSG(err == NCV_ERR_SUCCESS, "could not create context: %d", err);

	int width = 0, height = 0;
	uint8_t* frame = NULL;

	int i = 0;	

	while((err = ncv_wait_for_frame(ctx, -1, &width, &height, (void**)&frame)) == NCV_ERR_SUCCESS){
		printf("frame, %d x %d!\n", width, height);
		/*if(i++ == 100){
			FILE* f = fopen("image.raw", "wb");
			fwrite(frame, width * height * 3, 1, f);
			fclose(f);
		}*/
	}

	printf("quitting\n");
	
	ncv_ctx_destroy(&ctx);

	return 0;
}
