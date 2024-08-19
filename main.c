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


void pixelbuf_get_img_info(char* fn,int* w,int* h,int req_comps) {
    uint8_t* ptr = stbi_load(fn,w,h,0,req_comps);
    assert(ptr && "Failed to load img file. wrong path?");
    stbi_image_free(ptr);
}

PixelBuf canny_edge_detector(Arena* arena,char* fn) {
    int w , h;
    const int comps = 1;
    pixelbuf_get_img_info(fn,&w,&h,comps);

    Arena local_arena;
    arena_init(&local_arena,w * h * comps * (sizeof(int)) * 5 + 1024);

    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };
    const Kernel sobel_filter_x =       (Kernel){ .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 1,0,-1,  2,0,-2,  1,0,-1 } };
    const Kernel sobel_filter_y =       (Kernel){ .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-2,-1, 0,0,0, +1,+2,+1} };

    PixelBuf buf = pixelbuf_load(arena, fn, 1);


    PixelBuf grey_buf = pixelbuf_init(&local_arena, w,h,comps);
    kernel_apply(grey_kernel_interface, gaussian_blur_filter, &buf, &grey_buf);

    PixelBuf Ix = pixelbuf_init(&local_arena, w,h,comps);
    kernel_apply(grey_kernel_interface, sobel_filter_x, &grey_buf, &Ix);

    PixelBuf Iy = pixelbuf_init(&local_arena, w,h,comps);
    kernel_apply(grey_kernel_interface, sobel_filter_y, &grey_buf, &Iy);

    PixelBuf mag = pixelbuf_init(&local_arena, w,h,comps);
    PixelBuf angles = pixelbuf_init(&local_arena, w,h,comps);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            mag.pixels[i] = sqrt(Ix.pixels[i] * Ix.pixels[i] + Iy.pixels[i] * Iy.pixels[i]);
            angles.pixels[i] = (atan2(Iy.pixels[i], Ix.pixels[i]) * 180 / 3.14);
        }
    }


    PixelBuf out_buf = pixelbuf_init(arena, w,h,comps);

    // apply non maximus supperssion
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            int angle = out_buf.pixels[i];
            int q , r;

            if (0 <= angle < 22.5 || 157.5 <= angle <= 180) {
                q = mag.pixels[y * mag.w + x + 1];
                r = mag.pixels[y * mag.w + x - 1];
            } else if (22.5 <= angle && angle < 67.5) {
                q = mag.pixels[(y + 1) * mag.w + x - 1];
                r = mag.pixels[(y - 1) * mag.w + x + 1];
            } else if (67.5 <= angle && angle < 112.5) {
                q = mag.pixels[(y + 1) * mag.w + x];
                r = mag.pixels[(y - 1) * mag.w + x];
            } else {
                q = mag.pixels[(y + 1) * mag.w + x + 1];
                r = mag.pixels[(y - 1) * mag.w + x - 1];
            }

            out_buf.pixels[i] = ((int)(mag.pixels[i] >= q && mag.pixels[i] >= r)) * mag.pixels[i]; 
        }
    }

    // apply thresholding
    float high_threshold = 0.3; 
    float low_threshold  = 0.2;
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if(out_buf.pixels[i] >= 255 * high_threshold) {
                out_buf.pixels[i] = 255;
            } else if(out_buf.pixels[i] >= 255 * low_threshold) {
                out_buf.pixels[i] = 25;
            } else {
                out_buf.pixels[i] = 0;
            }
        }
    }

    // apply heuristics 
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if(out_buf.pixels[i] == 25) {
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



int main() {
    Arena arena;
    arena_init(&arena, 1024 * 1024 * 32);
    core_init();


    // const Kernel laplacian_operator = (Kernel){ .w = 3, .h = 3,  .factor = 6, .matrix = (int[]) { 1,4,1, 4,-20,4,1,4,1} };
    const Kernel laplacian_operator = (Kernel){ .w = 3, .h = 3,  .factor = 1, .matrix = (int[]) { 0,1,0, 1,-4,1,0,1,0} };
    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };

    PixelBuf buf = pixelbuf_load(&arena,"assets/lenna.png",1);
    PixelBuf blured_buf = pixelbuf_init(&arena, buf.w,buf.h,1);
    PixelBuf out_buf = pixelbuf_init(&arena,buf.w,buf.h,1);
    kernel_apply(grey_kernel_interface,gaussian_blur_filter,&buf, &blured_buf);
    kernel_apply(grey_kernel_interface,laplacian_operator,&blured_buf,&out_buf);


    PixelBuf norm128_buf = pixelbuf_normalize_to_128(&arena,&out_buf);
    for (int y = 0; y < out_buf.h; y++) {
        for (int x = 0; x < out_buf.w; x++) {
            int i = y * out_buf.w + x;
            // printf("%d\n",norm128_buf.pixels[i]);
            if(norm128_buf.pixels[i] == 128 ) {
                out_buf.pixels[i] = 255;
            } else {
                out_buf.pixels[i] = 0;
            }
        }
    }

    pixelbuf_save(out_buf, "tmps/laplacian.png",false);
    pixelbuf_save(norm128_buf, "tmps/norm.png",false);

#if 0
    PixelBuf buf =  canny_edge_detector(&arena,"assets/2.jpg");
    pixelbuf_save(buf, "tmps/canny_out.png");
#endif

    core_dispose();
    arena_dispose(&arena);
    return 0;
}


