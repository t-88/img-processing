#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <math.h>


#include "arena.h"


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


PixelBuf pixelbuf_load(Arena* arena, char* fn, int comps);
PixelBuf pixelbuf_save(PixelBuf buf, char* fn, bool norm);
PixelBuf pixelbuf_copy(Arena* arena, PixelBuf buf);
PixelBuf pixelbuf_normalize(Arena* arena, PixelBuf* buf);
PixelBuf pixelbuf_normalize_to_128(Arena* arena, PixelBuf* buf);
void pixelbuf_get_img_info(char* fn, int* w, int* h, int req_comps);

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
void core_dispose();



KernelOpsInterace grey_kernel_interface;
KernelOpsInterace pixel_kernel_interface;
static Arena _core_arena;
static Arena _save_arena;
static Arena _tmp_arena;


#endif //CORE_H

#ifdef CORE_IMPLEMENTATION_H


void core_init() {
    arena_init(&_core_arena, 1024);
    // 2 Megs imgs
    arena_init(&_save_arena, 1024 * 1024 * 32);


    pixel_kernel_interface = (KernelOpsInterace){
        .zero = pixel_zero,
        .add = pixel_add,
        .mult = pixel_mult,
        .buf_assign = pixel_buf_assign,
        .buf_mult = pixel_buf_mult,
        .acc = arena_alloc_of(&_core_arena,Pixel,1),
        .tmp = arena_alloc_of(&_core_arena,Pixel,1),
    };
    grey_kernel_interface = (KernelOpsInterace){
        .zero = number_zero,
        .add = number_add,
        .mult = number_mult,
        .buf_assign = number_buf_assign,
        .buf_mult = number_buf_mult,
        .acc = arena_alloc_of(&_core_arena,int,1),
        .tmp = arena_alloc_of(&_core_arena,int,1),
    };
}

void core_dispose() {
    arena_dispose(&_core_arena);
}


PixelBuf pixelbuf_load(Arena* arena, char* fn, int comps) {
    if (comps == 0) comps = 1;

    PixelBuf buf;
    buf.comps = comps;
    uint8_t* tmp_buf = stbi_load(fn, &buf.w, &buf.h, 0, buf.comps);
    assert(tmp_buf && "Failed to load img file. wrong path?");

    buf.pixels = arena_alloc_of(arena, int, buf.w * buf.h * buf.comps);

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


PixelBuf pixelbuf_init(Arena* arena, int w, int h, int comps) {
    return (PixelBuf) {
        .w = w,
            .h = h,
            .comps = comps,
            .pixels = arena_alloc_of(arena, int, w * h * comps),
    };
}

PixelBuf pixelbuf_normalize_to_128(Arena* arena, PixelBuf* buf) {
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

    PixelBuf out_buf = pixelbuf_init(arena, buf->w, buf->h, buf->comps);
    for (int y = 0; y < buf->h; y++) {
        for (int x = 0; x < buf->w; x++) {
            for (int i = 0; i < buf->comps; i++) {
                const int idx = buf->comps * (y * buf->w + x) + i;
                const float pixel = buf->pixels[idx];
                if (pixel > 0) {
                    out_buf.pixels[idx] = (pixel - 0) / (float)(maxs[i] - 0) * 128.f + 128.f;
                }
                else if(pixel < 0) {
                    out_buf.pixels[idx] = (pixel - mins[i]) / (float)(0 - mins[i]) * 128.f;
                } 
            }
        }
    }

    return out_buf;
}


PixelBuf pixelbuf_normalize(Arena* arena, PixelBuf* buf) {
    float maxs[3] = { FLT_MIN, FLT_MIN, FLT_MIN };
    float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };

    PixelBuf out_buf = pixelbuf_init(arena, buf->w, buf->h, buf->comps);
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

PixelBuf pixelbuf_save(PixelBuf buf, char* fn, bool normalize) {
    uint8_t* data = arena_alloc_of(&_save_arena, uint8_t, buf.w * buf.h * buf.comps);

    if (normalize) {
        PixelBuf normalized = pixelbuf_normalize(&_save_arena, &buf);
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
    arena_reset(&_save_arena);
}
PixelBuf pixelbuf_copy(Arena* arena, PixelBuf buf) {
    PixelBuf out = { .w = buf.w, .h = buf.h, .comps = buf.comps };
    out.pixels = arena_alloc_of(arena, int, buf.w * buf.h * buf.comps);
    memcpy(out.pixels, buf.pixels, buf.w * buf.h * buf.comps);
    return out;
}

void pixelbuf_get_img_info(char* fn, int* w, int* h, int req_comps) {
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


#endif // CORE_IMPLEMENTATION_H
