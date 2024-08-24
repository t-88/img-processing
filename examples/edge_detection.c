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




int main() {
    core_init();
    PixelBuf buf;

    buf = gradient_edge_detector("assets/lenna.png");
    pixelbuf_save(buf, "tmps/gradients.png", false);
    pixbuf_free(&buf);

    buf = laplacian_edge_detector("assets/lenna.png");
    pixelbuf_save(buf, "tmps/laplacian.png", false);
    pixbuf_free(&buf);

    buf = canny_edge_detector("assets/lenna.png");
    pixelbuf_save(buf, "tmps/canny.png", false);
    pixbuf_free(&buf);

    core_free();
    return 0;
}


