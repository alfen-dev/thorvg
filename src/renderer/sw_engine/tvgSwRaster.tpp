


#include "thorvg.h"

#include "tvgSwCommon.h"


#include "tvgSwRasterC.h"

template<typename PIXEL_T>
void rasterTranslucentPixel(PIXEL_T* dst, PIXEL_T* src, uint32_t len, uint8_t opacity)
{
    //TODO: Support SIMD accelerations
    cRasterTranslucentPixels(dst, src, len, opacity);
}



template<typename PIXEL_T>
void rasterXYFlip(PIXEL_T* src, PIXEL_T* dst, int32_t stride_pixels, int32_t w, int32_t h, const SwBBox& bbox, bool flipped)
{
    constexpr int BLOCK = 8;  //experimental decision

    if (flipped) {
        src += ((bbox.min.x * stride_pixels) + bbox.min.y);
        dst += ((bbox.min.y * stride_pixels) + bbox.min.x);
    } else {
        src += ((bbox.min.y * stride_pixels) + bbox.min.x);
        dst += ((bbox.min.x * stride_pixels) + bbox.min.y);
    }

    #pragma omp parallel for
    for (int32_t x = 0; x < w; x += BLOCK) {
        auto bx = std::min(w, x + BLOCK) - x;
        PIXEL_T* in = &src[x];
        PIXEL_T* out = &dst[x * stride_pixels];
        for (int32_t y = 0; y < h; y += BLOCK) {
            PIXEL_T* p = &in[y * stride_pixels];
            PIXEL_T* q = &out[y];
            auto by = std::min(h, y + BLOCK) - y;
            for (int32_t xx = 0; xx < bx; ++xx) {
                for (int32_t yy = 0; yy < by; ++yy) {
                    *q = *p;
                    p += stride_pixels;
                    ++q;
                }
                p += 1 - by * stride_pixels;
                q += stride_pixels - by;
            }
        }
    }
}


