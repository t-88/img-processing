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
#define DIR_PATH "assets/dene_road/"

PixelBuf load_frame(int idx) {
    static char fn[64];
    sprintf(fn,DIR_PATH "%03d.png",idx);
    PixelBuf buf = pixbuf_load(fn,1);
    return buf;
}

int main() {
    core_init();


    PixelBuf frame = load_frame(1); 
    pixbuf_save(frame,"tmps/1.png",false);
    pixbuf_free(&frame);

    core_free();
    return 0;
}


