#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include<unistd.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define CORE_IMPLEMENTATION_H
#include "core.h"
#define ARENA_IMPLEMENTATION_H
#include "arena.h"


Kernel kernel = {
    // .w = 2, .h = 2, .factor = 1 , .matrix = (int[]) { -1,-1,1,1}
    // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-2,-1, 0,0,0, 1,2,1}
    // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 0,-1,0,-1,4,-1,0,-1,0}
    // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-1,-1,-1,8,-1,-1,-1,-1}
    // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 0,0,0,0,1,0, 0,0,0}
    // .w = 3, .h = 3, .factor = 9, .matrix = (int[]) { 1,1,1, 1,1,1, 1,1,1}
    // .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-1,-1, -1,9,-1, -1,-1,-1}
    // .w = 5, .h = 5, .factor = 256, .matrix = (int[])    {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1}
};



PixelBuf canny_edge_detector(Arena* arena, char* fn) {
    int w, h;
    const int comps = 1;
    pixelbuf_get_img_info(fn, &w, &h, comps);

    Arena local_arena;
    arena_init(&local_arena, w * h * comps * (sizeof(int)) * 5 + 1024);

    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };
    const Kernel sobel_filter_x = (Kernel){ .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 1,0,-1,  2,0,-2,  1,0,-1 } };
    const Kernel sobel_filter_y = (Kernel){ .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-2,-1, 0,0,0, +1,+2,+1} };

    PixelBuf buf = pixelbuf_load(arena, fn, 1);


    PixelBuf grey_buf = pixelbuf_init(&local_arena, w, h, comps);
    kernel_apply(grey_kernel_interface, gaussian_blur_filter, &buf, &grey_buf);

    PixelBuf Ix = pixelbuf_init(&local_arena, w, h, comps);
    kernel_apply(grey_kernel_interface, sobel_filter_x, &grey_buf, &Ix);

    PixelBuf Iy = pixelbuf_init(&local_arena, w, h, comps);
    kernel_apply(grey_kernel_interface, sobel_filter_y, &grey_buf, &Iy);

    PixelBuf mag = pixelbuf_init(&local_arena, w, h, comps);
    PixelBuf angles = pixelbuf_init(&local_arena, w, h, comps);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            mag.pixels[i] = sqrt(Ix.pixels[i] * Ix.pixels[i] + Iy.pixels[i] * Iy.pixels[i]);
            angles.pixels[i] = (atan2(Iy.pixels[i], Ix.pixels[i]) * 180 / 3.14);
        }
    }


    PixelBuf out_buf = pixelbuf_init(arena, w, h, comps);

    // apply non maximus supperssion
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            int angle = out_buf.pixels[i];
            int q, r;

            if (0 <= angle < 22.5 || 157.5 <= angle <= 180) {
                q = mag.pixels[y * mag.w + x + 1];
                r = mag.pixels[y * mag.w + x - 1];
            }
            else if (22.5 <= angle && angle < 67.5) {
                q = mag.pixels[(y + 1) * mag.w + x - 1];
                r = mag.pixels[(y - 1) * mag.w + x + 1];
            }
            else if (67.5 <= angle && angle < 112.5) {
                q = mag.pixels[(y + 1) * mag.w + x];
                r = mag.pixels[(y - 1) * mag.w + x];
            }
            else {
                q = mag.pixels[(y + 1) * mag.w + x + 1];
                r = mag.pixels[(y - 1) * mag.w + x - 1];
            }

            out_buf.pixels[i] = ((int)(mag.pixels[i] >= q && mag.pixels[i] >= r)) * mag.pixels[i];
        }
    }

    // apply thresholding
    float high_threshold = 0.3;
    float low_threshold = 0.1;
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if (out_buf.pixels[i] >= 255 * high_threshold) {
                out_buf.pixels[i] = 255;
            }
            else if (out_buf.pixels[i] >= 255 * low_threshold) {
                out_buf.pixels[i] = 25;
            }
            else {
                out_buf.pixels[i] = 0;
            }
        }
    }

    // apply heuristics 
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if (out_buf.pixels[i] == 25) {
                bool close_strong = out_buf.pixels[(y + 0) * w + (x + 1)] == 255 ||
                    out_buf.pixels[(y + 0) * w + (x - 1)] == 255 ||
                    out_buf.pixels[(y - 1) * w + (x + 1)] == 255 ||
                    out_buf.pixels[(y - 1) * w + (x + 0)] == 255 ||
                    out_buf.pixels[(y - 1) * w + (x - 1)] == 255 ||
                    out_buf.pixels[(y + 1) * w + (x + 1)] == 255 ||
                    out_buf.pixels[(y + 1) * w + (x + 0)] == 255 ||
                    out_buf.pixels[(y + 1) * w + (x - 1)] == 255;
                out_buf.pixels[i] = ((int)close_strong) * 255;
            }
        }
    }

    arena_dispose(&local_arena);

    return out_buf;
}


PixelBuf laplacian_edge_detector(Arena* arena, char* fn) {
    // i have problems with it
    const Kernel laplacian_operator = (Kernel){ .w = 3, .h = 3,  .factor = 1, .matrix = (int[]) { 1,1,1, 1,-8,1,1,1,1} };
    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };

    PixelBuf buf = pixelbuf_load(arena, fn, 1);
    PixelBuf blured_buf = pixelbuf_init(arena, buf.w, buf.h, 1);
    PixelBuf out_buf = pixelbuf_init(arena, buf.w, buf.h, 1);

    kernel_apply(grey_kernel_interface, gaussian_blur_filter, &buf, &blured_buf);
    kernel_apply(grey_kernel_interface, laplacian_operator, &blured_buf, &out_buf);

    // using threshold    
    for (int y = 0; y < out_buf.h; y++) {
        for (int x = 0; x < out_buf.w; x++) {
            int i = y * out_buf.w + x;
            if (out_buf.pixels[i] > 15) {
                out_buf.pixels[i] = 255;
            }
            else {
                out_buf.pixels[i] = 0;
            }
        }
    }
    return out_buf;
}


PixelBuf gradient_edge_detector(Arena* arena, char* fn) {
    const Kernel gradients_x_operator = (Kernel){ .w = 2, .h = 2,  .factor = 1, .matrix = (int[]) { 0,1,-1,0} };
    const Kernel gradients_y_operator = (Kernel){ .w = 2, .h = 2,  .factor = 1, .matrix = (int[]) { 1,0,0, -1 } };
    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };

    PixelBuf buf = pixelbuf_load(arena, fn, 1);
    PixelBuf blured_buf = pixelbuf_init(arena, buf.w, buf.h, 1);
    PixelBuf Ix = pixelbuf_init(arena, buf.w, buf.h, 1);
    PixelBuf Iy = pixelbuf_init(arena, buf.w, buf.h, 1);

    kernel_apply(grey_kernel_interface, gaussian_blur_filter, &buf, &blured_buf);
    kernel_apply(grey_kernel_interface, gradients_x_operator, &blured_buf, &Ix);
    kernel_apply(grey_kernel_interface, gradients_y_operator, &blured_buf, &Iy);



    PixelBuf mag = pixelbuf_init(arena, buf.w, buf.h, 1);

    for (int y = 0; y < buf.h; y++) {
        for (int x = 0; x < buf.w; x++) {
            int i = y * buf.w + x;
            mag.pixels[i] = sqrt(Ix.pixels[i] * Ix.pixels[i] + Iy.pixels[i] * Iy.pixels[i]);
            if(mag.pixels[i] > 15) {
                mag.pixels[i] = 255;
            } else {
                mag.pixels[i] = 0;
            }
        }
    }

    return mag;


}

int main() {
    Arena arena;
    arena_init(&arena, 1024 * 1024 * 32);
    core_init();


    arena_reset(&arena);
    pixelbuf_save(gradient_edge_detector(&arena,"assets/lenna.png"), "tmps/gradients.png", false);
    arena_reset(&arena);
    pixelbuf_save(laplacian_edge_detector(&arena, "assets/lenna.png"), "tmps/laplacian.png", false);
    arena_reset(&arena);
    pixelbuf_save(canny_edge_detector(&arena,"assets/lenna.png"), "tmps/canny.png",false);



    core_dispose();
    arena_dispose(&arena);
    return 0;
}


