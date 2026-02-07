#define UVZMQ_IMPLEMENTATION
#include <stdio.h>
#include <uv.h>

int main(void) {
    printf("[TEST] Step 1: Creating libuv loop...\n");
    uv_loop_t loop;
    int rc = uv_loop_init(&loop);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] uv_loop_init failed: %d\n", rc);
        return 1;
    }
    printf("[TEST] uv_loop_init succeeded\n");

    printf("[TEST] Step 2: Running loop...\n");
    for (int i = 0; i < 5; i++) {
        rc = uv_run(&loop, UV_RUN_ONCE);
        printf("[TEST] Iteration %d, uv_run returned: %d\n", i, rc);
    }
    printf("[TEST] Step 3: Closing loop...\n");
    uv_loop_close(&loop);

    printf("[TEST] All tests passed!\n");
    return 0;
}