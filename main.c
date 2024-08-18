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

    KernelOpsInterace pixel_kernel_interface = {
        .zero = pixel_zero,
        .add = pixel_add,
        .mult = pixel_mult,
        .buf_assign = pixel_buf_assign,
        .buf_mult = pixel_buf_mult,
        .acc = arena_alloc_of(&arena,Pixel,1),
        .tmp = arena_alloc_of(&arena,Pixel,1),
    };
    KernelOpsInterace number_kernel_interface = {
        .zero = number_zero,
        .add = number_add,
        .mult = number_mult,
        .buf_assign = number_buf_assign,
        .buf_mult = number_buf_mult,
        .acc = arena_alloc_of(&arena,int,1),
        .tmp = arena_alloc_of(&arena,int,1),
    };


    PixelBuf img_buf = pixelbuf_load(&arena, "assets/wiki.png", 3);
    PixelBuf out_buf = { .w = img_buf.w, .h = img_buf.h, .comps = 3};
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


    kernel_apply(pixel_kernel_interface, kernel, &img_buf, &out_buf);
    pixelbuf_save(out_buf, "out.png");
    pixelbuf_save(img_buf, "img.png");

    arena_dispose(&arena);
    return 0;
}


