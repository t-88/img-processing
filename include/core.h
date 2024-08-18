#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>


#include "arena.h"


#ifndef CORE_H
#define CORE_H

typedef struct PixelBuf {
    int w;
    int h;
    int comps;
    uint8_t* pixels;
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


PixelBuf pixelbuf_load(Arena* arena, char* fn, int comps);
PixelBuf pixelbuf_save(PixelBuf buf, char* fn);

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





#endif //CORE_H

#ifdef CORE_IMPLEMENTATION_H

PixelBuf pixelbuf_load(Arena* arena, char* fn, int comps) {
    if (comps == 0) comps = 1;

    PixelBuf buf;
    buf.comps = comps;
    uint8_t* tmp_buf = stbi_load(fn, &buf.w, &buf.h, 0, buf.comps);
    buf.pixels = arena_alloc_of(arena, uint8_t, buf.w * buf.h * buf.comps);
    memcpy(buf.pixels, tmp_buf, buf.w * buf.h * buf.comps);
    stbi_image_free(tmp_buf);
    return buf;
}

PixelBuf pixelbuf_save(PixelBuf buf, char* fn) {
    stbi_write_png(fn, buf.w, buf.h, buf.comps, buf.pixels, 0);
}


void pixel_zero(void* out) {
    ((Pixel*)out)->r = 0;
    ((Pixel*)out)->g = 0;
    ((Pixel*)out)->b = 0;
}
void pixel_add(void* out, void* other) {
    ((Pixel*)out)->r += ((Pixel*)other)->r;
    ((Pixel*)out)->g += ((Pixel*)other)->g;
    ((Pixel*)out)->b += ((Pixel*)other)->b;
}
void pixel_mult(void* out, float factor) {
    ((Pixel*)out)->r *= factor;
    ((Pixel*)out)->g *= factor;
    ((Pixel*)out)->b *= factor;
}
void pixel_buf_mult(void* out, float factor, PixelBuf* buf, int x, int y) {
    ((Pixel*)out)->r = buf->pixels[3 * (y * buf->w + x) + 0] * factor;
    ((Pixel*)out)->g = buf->pixels[3 * (y * buf->w + x) + 1] * factor;
    ((Pixel*)out)->b = buf->pixels[3 * (y * buf->w + x) + 2] * factor;
}
void pixel_buf_assign(void* pixel, PixelBuf* buf, int x, int y) {
    buf->pixels[3 * (y * buf->w + x) + 0] = fmin(255, fmax(0, ((Pixel*)pixel)->r));
    buf->pixels[3 * (y * buf->w + x) + 1] = fmin(255, fmax(0, ((Pixel*)pixel)->g));
    buf->pixels[3 * (y * buf->w + x) + 2] = fmin(255, fmax(0, ((Pixel*)pixel)->b));
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
    buf->pixels[y * buf->w + x] = fmin(255, fmax(0, *casted));
}


void kernel_apply(KernelOpsInterace interface, Kernel kernel, PixelBuf* in_buf, PixelBuf* out_buf) {
    int kstart = kernel.w % 2 == 0 ? floorf(-kernel.w / 2.f) : floorf(-kernel.w / 2.f + 1);
    int kend = ceilf(kernel.w / 2.f - 1);

    for (int y = 0; y < in_buf->h; y++) {
        for (int x = 0; x < in_buf->w; x++) {
            interface.zero(interface.acc);

            for (int cy = kstart; cy <= kend; cy++) {
                for (int cx = kstart; cx <= kend; cx++) {
                    int xPos = x + cx;
                    int yPos = y + cy;
                    if (xPos < 0) xPos = x;
                    if (yPos < 0) yPos = y;
                    if (xPos >= in_buf->w) xPos -= in_buf->w;
                    if (yPos >= in_buf->h) yPos -= in_buf->h;

                    float kernel_factor = kernel.matrix[(cy - kstart) * kernel.w + (cx - kstart)];


                    interface.buf_mult(interface.tmp, kernel_factor, in_buf, xPos, yPos);
                    interface.add(interface.acc, interface.tmp);

                }
            }

            interface.mult(interface.acc, 1.f / kernel.factor);
            interface.buf_assign(interface.acc, out_buf, x, y);
        }
    }
}


#endif // CORE_IMPLEMENTATION_H
