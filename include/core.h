#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <math.h>


#ifndef CORE_H
#define CORE_H

typedef struct PixelBuf {
    int w;
    int h;
    int comps;
    int* pixels;
} PixelBuf;

typedef struct Kernel {
    int w;
    int h;
    float factor;
    int* matrix;
} Kernel;

typedef struct Pixel {
    float r, g, b;
} Pixel;

typedef struct KernelOpsInterace {
    void (*zero)(void*);
    void (*add)(void*, void*);
    void (*mult)(void*, float);
    void (*buf_mult)(void*, float, PixelBuf*, int, int);
    void (*buf_assign)(void*, PixelBuf*, int, int);
    void* acc;
    void* tmp;
} KernelOpsInterace;


PixelBuf pixbuf_load(char* fn, int comps);
PixelBuf pixbuf_save(PixelBuf buf, char* fn, bool norm);
PixelBuf pixbuf_copy(PixelBuf buf);
void pixbuf_free(PixelBuf* buf);
PixelBuf pixbuf_normalize(PixelBuf* buf);
PixelBuf pixbuf_normalize_to_128(PixelBuf* buf);
void pixbuf_get_img_info(char* fn, int* w, int* h, int req_comps);

void pixel_zero(void* out);
void pixel_add(void* out, void* other);
void pixel_mult(void* out, float factor);
void pixel_buf_mult(void* out, float factor, PixelBuf* buf, int x, int y);
void pixel_buf_assign(void* pixel, PixelBuf* buf, int x, int y);
void number_zero(void* out);
void number_add(void* out, void* other);
void number_mult(void* out, float factor);
void number_buf_mult(void* out, float factor, PixelBuf* buf, int x, int y);
void number_buf_assign(void* pixel, PixelBuf* buf, int x, int y);



void kernel_apply(KernelOpsInterace interface, Kernel kernel, PixelBuf* in_buf, PixelBuf* out_buf);

void core_init();
void core_free();


KernelOpsInterace grey_kernel_interface;
KernelOpsInterace pixel_kernel_interface;

// algorthims
PixelBuf canny_edge_detector(char* fn);
PixelBuf laplacian_edge_detector(char* fn);
PixelBuf gradient_edge_detector(char* fn);


#endif //CORE_H

#ifdef CORE_IMPLEMENTATION_H

void core_init() {
    pixel_kernel_interface = (KernelOpsInterace){
        .zero = pixel_zero,
        .add = pixel_add,
        .mult = pixel_mult,
        .buf_assign = pixel_buf_assign,
        .buf_mult = pixel_buf_mult,
        .acc = malloc(sizeof(Pixel)),
        .tmp = malloc(sizeof(Pixel)),
    };
    grey_kernel_interface = (KernelOpsInterace){
        .zero = number_zero,
        .add = number_add,
        .mult = number_mult,
        .buf_assign = number_buf_assign,
        .buf_mult = number_buf_mult,
        .acc = malloc(sizeof(int)),
        .tmp = malloc(sizeof(int)),
    };
}

void core_free() {
    free(pixel_kernel_interface.acc);
    free(pixel_kernel_interface.tmp);
    free(grey_kernel_interface.tmp);
    free(grey_kernel_interface.acc);
}


PixelBuf pixbuf_load(char* fn, int comps) {
    if (comps == 0) comps = 1;

    PixelBuf buf;
    buf.comps = comps;
    uint8_t* tmp_buf = stbi_load(fn, &buf.w, &buf.h, 0, buf.comps);
    assert(tmp_buf && "Failed to load img file. wrong path?");

    buf.pixels = malloc(sizeof(int) * buf.w * buf.h * buf.comps);

    for (int y = 0; y < buf.h; y++) {
        for (int x = 0; x < buf.w; x++) {
            for (int i = 0; i < buf.comps; i++) {
                buf.pixels[buf.comps * (y * buf.w + x) + i] = tmp_buf[buf.comps * (y * buf.w + x) + i];
            }
        }
    }
    stbi_image_free(tmp_buf);
    return buf;
}

PixelBuf pixbuf_init(int w, int h, int comps) {
    return (PixelBuf) {
        .w = w,
            .h = h,
            .comps = comps,
            .pixels = malloc(sizeof(int) * w * h * comps),
    };
}

void pixbuf_free(PixelBuf* buf) { 
    free(buf->pixels);
}

PixelBuf pixbuf_normalize_to_128(PixelBuf* buf) {
    float maxs[3] = { FLT_MIN, FLT_MIN, FLT_MIN };
    float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };

    for (int y = 0; y < buf->h; y++) {
        for (int x = 0; x < buf->w; x++) {
            for (int i = 0; i < buf->comps; i++) {
                const int idx = buf->comps * (y * buf->w + x) + i;
                if (buf->pixels[idx] > maxs[i])  maxs[i] = buf->pixels[idx];
                if (buf->pixels[idx] < mins[i])  mins[i] = buf->pixels[idx];
            }
        }
    }

    PixelBuf out_buf = pixbuf_init(buf->w, buf->h, buf->comps);
    for (int y = 0; y < buf->h; y++) {
        for (int x = 0; x < buf->w; x++) {
            for (int i = 0; i < buf->comps; i++) {
                const int idx = buf->comps * (y * buf->w + x) + i;
                const float pixel = buf->pixels[idx];
                if (pixel > 0) {
                    out_buf.pixels[idx] = (pixel - 0) / (float)(maxs[i] - 0) * 128.f + 128.f;
                }
                else if (pixel < 0) {
                    out_buf.pixels[idx] = (pixel - mins[i]) / (float)(0 - mins[i]) * 128.f;
                }
            }
        }
    }

    return out_buf;
}


PixelBuf pixbuf_normalize(PixelBuf* buf) {
    float maxs[3] = { FLT_MIN, FLT_MIN, FLT_MIN };
    float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };

    PixelBuf out_buf = pixbuf_init(buf->w, buf->h, buf->comps);
    for (int y = 0; y < buf->h; y++) {
        for (int x = 0; x < buf->w; x++) {
            for (int i = 0; i < buf->comps; i++) {
                const int idx = buf->comps * (y * buf->w + x) + i;
                if (buf->pixels[idx] > maxs[i])  maxs[i] = buf->pixels[idx];
                if (buf->pixels[idx] < mins[i])  mins[i] = buf->pixels[idx];
            }
        }
    }
    for (int y = 0; y < buf->h; y++) {
        for (int x = 0; x < buf->w; x++) {
            for (int i = 0; i < buf->comps; i++) {
                const int idx = buf->comps * (y * buf->w + x) + i;
                const float pixel = buf->pixels[idx];
                out_buf.pixels[idx] = ((float)(pixel - mins[i]) / (maxs[i] - mins[i])) * 255;
            }
        }
    }

    return out_buf;
}
PixelBuf pixbuf_save(PixelBuf buf, char* fn, bool normalize) {
    uint8_t* data = malloc(sizeof(uint8_t) * buf.w * buf.h * buf.comps);

    if (normalize) {
        PixelBuf normalized = pixbuf_normalize(&buf);
        for (int y = 0; y < normalized.h; y++) {
            for (int x = 0; x < normalized.w; x++) {
                for (int i = 0; i < normalized.comps; i++) {
                    const int idx = normalized.comps * (y * normalized.w + x) + i;
                    data[idx] = normalized.pixels[idx];
                }
            }
        }
    }
    else {
        for (int y = 0; y < buf.h; y++) {
            for (int x = 0; x < buf.w; x++) {
                for (int i = 0; i < buf.comps; i++) {
                    const int idx = buf.comps * (y * buf.w + x) + i;
                    data[idx] = fmax(0, fmin(buf.pixels[idx], 255));
                }
            }
        }
    }

    stbi_write_png(fn, buf.w, buf.h, buf.comps, data, buf.w * buf.comps);
    free(data);
}
PixelBuf pixbuf_copy(PixelBuf buf) {
    PixelBuf out = { .w = buf.w, .h = buf.h, .comps = buf.comps };
    out.pixels = malloc(sizeof(int) * buf.w * buf.h * buf.comps);
    memcpy(out.pixels, buf.pixels, buf.w * buf.h * buf.comps);
    return out;
}

void pixbuf_get_img_info(char* fn, int* w, int* h, int req_comps) {
    uint8_t* ptr = stbi_load(fn, w, h, 0, req_comps);
    assert(ptr && "Failed to load img file. wrong path?");
    stbi_image_free(ptr);
}


void pixel_zero(void* out) {
    Pixel* casted = (Pixel*)out;
    casted->r = 0;
    casted->g = 0;
    casted->b = 0;
}
void pixel_add(void* out, void* other) {
    Pixel* casted = (Pixel*)out;
    Pixel* casted_other = (Pixel*)out;
    casted->r += casted_other->r;
    casted->g += casted_other->g;
    casted->b += casted_other->b;
}
void pixel_mult(void* out, float factor) {
    Pixel* casted = (Pixel*)out;
    casted->r *= factor;
    casted->g *= factor;
    casted->b *= factor;
}
void pixel_buf_mult(void* out, float factor, PixelBuf* buf, int x, int y) {
    Pixel* casted = (Pixel*)out;
    casted->r = buf->pixels[3 * (y * buf->w + x) + 0] * factor;
    casted->g = buf->pixels[3 * (y * buf->w + x) + 1] * factor;
    casted->b = buf->pixels[3 * (y * buf->w + x) + 2] * factor;
}
void pixel_buf_assign(void* pixel, PixelBuf* buf, int x, int y) {
    Pixel* casted = (Pixel*)pixel;

    buf->pixels[3 * (y * buf->w + x) + 0] = fmin(255, fmax(0, casted->r));
    buf->pixels[3 * (y * buf->w + x) + 1] = fmin(255, fmax(0, casted->g));
    buf->pixels[3 * (y * buf->w + x) + 2] = fmin(255, fmax(0, casted->b));
}



void number_zero(void* out) {
    int* casted = (int*)out;
    *casted = 0;

}
void number_add(void* out, void* other) {
    int* casted = (int*)out;
    int* casted_other = (int*)other;
    *casted += *casted_other;
}
void number_mult(void* out, float factor) {
    int* casted = (int*)out;
    *casted *= factor;
}
void number_buf_mult(void* out, float factor, PixelBuf* buf, int x, int y) {
    int* casted = (int*)out;
    *casted = buf->pixels[y * buf->w + x] * factor;
}
void number_buf_assign(void* pixel, PixelBuf* buf, int x, int y) {
    int* casted = (int*)pixel;
    buf->pixels[y * buf->w + x] = *casted;
}



void kernel_apply(KernelOpsInterace interface, Kernel kernel, PixelBuf* in_buf, PixelBuf* out_buf) {
    int ox = kernel.h / 2;
    int oy = kernel.w / 2;

    for (int y = 0; y < in_buf->h; y++) {
        for (int x = 0; x < in_buf->w; x++) {
            interface.zero(interface.acc);

            for (int cy = 0; cy < kernel.h; cy++) {
                for (int cx = 0; cx < kernel.w; cx++) {
                    int xPos = x + cx - ox;
                    int yPos = y + cy - oy;
                    if (xPos < 0) xPos = x;
                    if (yPos < 0) yPos = y;
                    if (xPos >= in_buf->w) xPos = x;
                    if (yPos >= in_buf->h) yPos = y;

                    float kernel_factor = kernel.matrix[cy * kernel.w + cx];


                    interface.buf_mult(interface.tmp, kernel_factor, in_buf, xPos, yPos);
                    interface.add(interface.acc, interface.tmp);

                }
            }

            interface.mult(interface.acc, 1.f / kernel.factor);
            interface.buf_assign(interface.acc, out_buf, x, y);
        }
    }
}



//  algorthims
PixelBuf canny_edge_detector(char* fn) {
    int w, h;
    const int comps = 1;
    pixbuf_get_img_info(fn, &w, &h, comps);

    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };
    const Kernel sobel_filter_x = (Kernel){ .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { 1,0,-1,  2,0,-2,  1,0,-1 } };
    const Kernel sobel_filter_y = (Kernel){ .w = 3, .h = 3, .factor = 1, .matrix = (int[]) { -1,-2,-1, 0,0,0, +1,+2,+1} };

    PixelBuf buf = pixbuf_load(fn, 1);
    PixelBuf grey_buf = pixbuf_init(w, h, comps);
    PixelBuf Ix = pixbuf_init(w, h, comps);
    PixelBuf Iy = pixbuf_init(w, h, comps);
    PixelBuf mag = pixbuf_init(w, h, comps);
    PixelBuf angles = pixbuf_init(w, h, comps);

    kernel_apply(grey_kernel_interface, gaussian_blur_filter, &buf, &grey_buf);
    kernel_apply(grey_kernel_interface, sobel_filter_x, &grey_buf, &Ix);
    kernel_apply(grey_kernel_interface, sobel_filter_y, &grey_buf, &Iy);



    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            mag.pixels[i] = sqrt(Ix.pixels[i] * Ix.pixels[i] + Iy.pixels[i] * Iy.pixels[i]);
            angles.pixels[i] = (atan2(Iy.pixels[i], Ix.pixels[i]) * 180 / 3.14);
        }
    }


    PixelBuf out_buf = pixbuf_init(w, h, comps);

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

    pixbuf_free(&buf);
    pixbuf_free(&grey_buf);
    pixbuf_free(&Ix);
    pixbuf_free(&Iy);
    pixbuf_free(&mag);
    pixbuf_free(&angles);

    return out_buf;
}

PixelBuf laplacian_edge_detector(char* fn) {
    // i have problems with it
    const Kernel laplacian_operator = (Kernel){ .w = 3, .h = 3,  .factor = 1, .matrix = (int[]) { 1,1,1, 1,-8,1,1,1,1} };
    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };

    PixelBuf buf = pixbuf_load(fn, 1);
    PixelBuf blured_buf = pixbuf_init(buf.w, buf.h, 1);
    PixelBuf out_buf = pixbuf_init(buf.w, buf.h, 1);

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


    pixbuf_free(&buf);
    pixbuf_free(&blured_buf);

    return out_buf;
}

PixelBuf gradient_edge_detector(char* fn) {
    const Kernel gradients_x_operator = (Kernel){ .w = 2, .h = 2,  .factor = 1, .matrix = (int[]) { 0,1,-1,0} };
    const Kernel gradients_y_operator = (Kernel){ .w = 2, .h = 2,  .factor = 1, .matrix = (int[]) { 1,0,0, -1 } };
    const Kernel gaussian_blur_filter = (Kernel){ .w = 5, .h = 5, .factor = 256, .matrix = (int[]) {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4,  1} };

    PixelBuf buf = pixbuf_load(fn, 1);
    PixelBuf blured_buf = pixbuf_init(buf.w, buf.h, 1);
    PixelBuf Ix = pixbuf_init(buf.w, buf.h, 1);
    PixelBuf Iy = pixbuf_init(buf.w, buf.h, 1);

    kernel_apply(grey_kernel_interface, gaussian_blur_filter, &buf, &blured_buf);
    kernel_apply(grey_kernel_interface, gradients_x_operator, &blured_buf, &Ix);
    kernel_apply(grey_kernel_interface, gradients_y_operator, &blured_buf, &Iy);



    PixelBuf mag = pixbuf_init(buf.w, buf.h, 1);

    for (int y = 0; y < buf.h; y++) {
        for (int x = 0; x < buf.w; x++) {
            int i = y * buf.w + x;
            mag.pixels[i] = sqrt(Ix.pixels[i] * Ix.pixels[i] + Iy.pixels[i] * Iy.pixels[i]);
            if (mag.pixels[i] > 15) {
                mag.pixels[i] = 255;
            }
            else {
                mag.pixels[i] = 0;
            }
        }
    }


    pixbuf_free(&buf);
    pixbuf_free(&blured_buf);
    pixbuf_free(&Ix);
    pixbuf_free(&Iy);

    return mag;
}




#endif // CORE_IMPLEMENTATION_H
