#include <stdio.h>
#include <uv.h>

int main(void) {
    printf("Before loop init\n");
    fflush(stdout);

    uv_loop_t loop;
    uv_loop_init(&loop);

    printf("After loop init\n");
    fflush(stdout);

    uv_run(&loop, UV_RUN_NOWAIT);

    printf("After uv_run\n");
    fflush(stdout);

    uv_loop_close(&loop);

    printf("Done!\n");
    return 0;
}