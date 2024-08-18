#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define CORE_IMPLEMENTATION_H
#include "core.h"
#define ARENA_IMPLEMENTATION_H
#include "arena.h"


int main() {
    Arena arena;
    arena_init(&arena, 1024 * 1024 * 32);
    core_init();



    PixelBuf img_buf = pixelbuf_load(&arena, "assets/wiki.png", 1);
    PixelBuf out_buf = { .w = img_buf.w, .h = img_buf.h, .comps = 1};
    out_buf.pixels = arena_alloc_of(&arena, uint8_t, img_buf.w * img_buf.h * out_buf.comps);

    Kernel kernel = {
        // .w = 2, .h = 2, .factor = 1 , .matrix = (int[]) { -1,-1,1,1}
        // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-2,-1, 0,0,0, 1,2,1}
        // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 0,-1,0,-1,4,-1,0,-1,0}
        // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-1,-1,-1,8,-1,-1,-1,-1}
        // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 0,0,0,0,1,0, 0,0,0}
        // .w = 3, .h = 3, .factor = 9, .matrix = (int[]) { 1,1,1, 1,1,1, 1,1,1}
        .w = 5, .h = 5, .factor = 256, .matrix = (int[])    {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1}
    };


    kernel_apply(number_kernel_interface, kernel, &img_buf, &out_buf);
    pixelbuf_save(out_buf, "out.png");
    pixelbuf_save(img_buf, "img.png");

    core_dispose();
    arena_dispose(&arena);
    return 0;
}


