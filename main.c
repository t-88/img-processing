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


#define FRAME_COUNT 312
#define DIR_PATH "assets/obj-tracking/"

PixelBuf load_frame(int idx) {
    static char fn[64];
    sprintf(fn, DIR_PATH "%03d.png", idx);
    PixelBuf buf = pixbuf_load(fn, 1);
    return buf;
}

int main() {
    core_init();

    char fn[64];

    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };

    PixelBuf diff = pixbuf_init(450, 300, 1);
    PixelBuf blur_frame1 = pixbuf_init(450, 300, 1);
    PixelBuf blur_frame2 = pixbuf_init(450, 300, 1);

    for (int i = 1; i < FRAME_COUNT - 1; i++) {
        PixelBuf frame1 = load_frame(i);
        PixelBuf frame2 = load_frame(i+1);
        kernel_apply(grey_kernel_interface,gaussian_blur_filter,&frame1,&blur_frame1);
        kernel_apply(grey_kernel_interface,gaussian_blur_filter,&frame2,&blur_frame2);


        for (int y = 0; y < frame1.h; y++) {
            for (int x = 0; x < frame1.w; x++) {
                for (int i = 0; i < frame1.comps; i++) {
                    int idx = frame1.comps * (y * frame1.w + x) + i;
                    diff.pixels[idx] = abs(blur_frame1.pixels[idx] - blur_frame2.pixels[idx]);
                    if (diff.pixels[idx] > 4) {
                        diff.pixels[idx] = 255;
                    }
                    else {
                        diff.pixels[idx] = 0;
                    }
                }
            }
        }

        printf("%d\n",i);
        sprintf(fn, "tmps/%03d_diff.png", i);
        pixbuf_save(diff, fn, false);

        pixbuf_free(&frame1);
        pixbuf_free(&frame2);
    }
    pixbuf_free(&diff);
    pixbuf_free(&blur_frame1);
    pixbuf_free(&blur_frame2);


    core_free();
    return 0;
}


