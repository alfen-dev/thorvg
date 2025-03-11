/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgMath.h"
#include "tvgSwCommon.h"

/************************************************************************/
/* Gaussian Blur Implementation                                         */
/************************************************************************/

struct SwGaussianBlur
{
    static constexpr int MAX_LEVEL = 3;
    int level;
    int kernel[MAX_LEVEL];
    int extends;
};


static inline int _gaussianEdgeWrap(int end, int idx)
{
    auto r = idx % (end + 1);
    return (r < 0) ? (end + 1) + r : r;
}


static inline int _gaussianEdgeExtend(int end, int idx)
{
    if (idx < 0) return 0;
    else if (idx > end) return end;
    return idx;
}


template<int border>
static inline int _gaussianRemap(int end, int idx)
{
    if (border == 1) return _gaussianEdgeWrap(end, idx);
    return _gaussianEdgeExtend(end, idx);
}


//TODO: SIMD OPTIMIZATION?
template<int border = 0>
static void _gaussianFilter(uint8_t* dst, uint8_t* src, int32_t stride_pixels, int32_t w, int32_t h, const SwBBox& bbox, int32_t dimension, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride_pixels + bbox.min.y) << PIXEL_SIZE_SHIFT;
        dst += (bbox.min.x * stride_pixels + bbox.min.y) << PIXEL_SIZE_SHIFT;
    } else {
        src += (bbox.min.y * stride_pixels + bbox.min.x) << PIXEL_SIZE_SHIFT;
        dst += (bbox.min.y * stride_pixels + bbox.min.x) << PIXEL_SIZE_SHIFT;
    }

    auto iarr = 1.0f / (dimension + dimension + 1);
    auto end = w - 1;

    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        auto p = y * stride_pixels;
        auto i = p * 4;                 //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc[4] = {0, 0, 0, 0};      //sliding accumulator

        //initial accumulation
        for (int x = l; x < r; ++x) {
            auto id = (_gaussianRemap<border>(end, x) + p) * 4;
            acc[0] += src[id++];
            acc[1] += src[id++];
            acc[2] += src[id++];
            acc[3] += src[id];
        }
        //perform filtering
        for (int x = 0; x < w; ++x, ++r, ++l) {
            auto rid = (_gaussianRemap<border>(end, r) + p) * 4;
            auto lid = (_gaussianRemap<border>(end, l) + p) * 4;
            acc[0] += src[rid++] - src[lid++];
            acc[1] += src[rid++] - src[lid++];
            acc[2] += src[rid++] - src[lid++];
            acc[3] += src[rid] - src[lid];
            //ignored rounding for the performance. It should be originally: acc[idx] * iarr + 0.5f
            dst[i++] = static_cast<uint8_t>(acc[0] * iarr);
            dst[i++] = static_cast<uint8_t>(acc[1] * iarr);
            dst[i++] = static_cast<uint8_t>(acc[2] * iarr);
            dst[i++] = static_cast<uint8_t>(acc[3] * iarr);
        }
    }
}


static int _gaussianInit(SwGaussianBlur* data, float sigma, int quality)
{
    const auto MAX_LEVEL = SwGaussianBlur::MAX_LEVEL;

    if (tvg::zero(sigma)) return 0;

    data->level = int(SwGaussianBlur::MAX_LEVEL * ((quality - 1) * 0.01f)) + 1;

    //compute box kernel sizes
    auto wl = (int) sqrt((12 * sigma / MAX_LEVEL) + 1);
    if (wl % 2 == 0) --wl;
    auto wu = wl + 2;
    auto mi = (12 * sigma - MAX_LEVEL * wl * wl - 4 * MAX_LEVEL * wl - 3 * MAX_LEVEL) / (-4 * wl - 4);
    auto m = int(mi + 0.5f);
    auto extends = 0;

    for (int i = 0; i < data->level; i++) {
        data->kernel[i] = ((i < m ? wl : wu) - 1) / 2;
        extends += data->kernel[i];
    }

    return extends;
}


bool effectGaussianBlurRegion(RenderEffectGaussianBlur* params)
{
    //bbox region expansion for feathering
    auto& region = params->extend;
    auto extra = static_cast<SwGaussianBlur*>(params->rd)->extends;

    if (params->direction != 2) {
        region.x = -extra;
        region.w = extra * 2;
    }
    if (params->direction != 1) {
        region.y = -extra;
        region.h = extra * 2;
    }

    return true;
}


void effectGaussianBlurUpdate(RenderEffectGaussianBlur* params, const Matrix& transform)
{
    if (!params->rd) params->rd = (SwGaussianBlur*)malloc(sizeof(SwGaussianBlur));
    auto rd = static_cast<SwGaussianBlur*>(params->rd);

    //compute box kernel sizes
    auto scale = sqrt(transform.e11 * transform.e11 + transform.e12 * transform.e12);
    rd->extends = _gaussianInit(rd, std::pow(params->sigma * scale, 2), params->quality);

    //invalid
    if (rd->extends == 0) {
        params->valid = false;
        return;
    }

    params->valid = true;
}


bool effectGaussianBlur(SwCompositor* cmp, SwSurface<PIXEL_TYPE>* surface, const RenderEffectGaussianBlur* params)
{
    auto& buffer = surface->compositor->image;
    auto data = static_cast<SwGaussianBlur*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);
    auto stride_pixels = cmp->image.stride_pixels;
    auto front = cmp->image.pixel_buffer;
    auto back = buffer.pixel_buffer;
    auto swapped = false;

    TVGLOG("SW_ENGINE", "GaussianFilter region(%ld, %ld, %ld, %ld) params(%f %d %d), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->sigma, params->direction, params->border, data->level);

    /* It is best to take advantage of the Gaussian blur’s separable property
       by dividing the process into two passes. horizontal and vertical.
       We can expect fewer calculations. */

    //horizontal
    if (params->direction != 2) {
        for (int i = 0; i < data->level; ++i) {
            _gaussianFilter(reinterpret_cast<uint8_t*>(back), reinterpret_cast<uint8_t*>(front), stride_pixels, w, h, bbox, data->kernel[i], false);
            std::swap(front, back);
            swapped = !swapped;
        }
    }

    //vertical. x/y flipping and horionztal access is pretty compatible with the memory architecture.
    if (params->direction != 1) {
        rasterXYFlip<PIXEL_TYPE>(front, back, stride_pixels, w, h, bbox, false);
        std::swap(front, back);

        for (int i = 0; i < data->level; ++i) {
            _gaussianFilter(reinterpret_cast<uint8_t*>(back), reinterpret_cast<uint8_t*>(front), stride_pixels, h, w, bbox, data->kernel[i], true);
            std::swap(front, back);
            swapped = !swapped;
        }

        rasterXYFlip<PIXEL_TYPE>(front, back, stride_pixels, h, w, bbox, true);
        std::swap(front, back);
    }

    if (swapped) std::swap(cmp->image.pixel_buffer, buffer.pixel_buffer);

    return true;
}

/************************************************************************/
/* Drop Shadow Implementation                                           */
/************************************************************************/

struct SwDropShadow : SwGaussianBlur
{
    SwPoint offset;
};


//TODO: SIMD OPTIMIZATION?
template<typename T>
static void _dropShadowFilter(T* dst, T* src, int stride_pixels, int w, int h, const SwBBox& bbox, int32_t dimension, T color, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride_pixels + bbox.min.y);
        dst += (bbox.min.x * stride_pixels + bbox.min.y);
    } else {
        src += (bbox.min.y * stride_pixels + bbox.min.x);
        dst += (bbox.min.y * stride_pixels + bbox.min.x);
    }
    auto iarr = 1.0f / (dimension + dimension + 1);
    auto end = w - 1;

    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        auto p = y * stride_pixels;
        auto i = p;                     //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc = 0;                    //sliding accumulator

        //initial accumulation
        for (int x = l; x < r; ++x) {
            auto id = _gaussianEdgeExtend(end, x) + p;
            acc += A(src[id]);
        }
        //perform filtering
        for (int x = 0; x < w; ++x, ++r, ++l) {
            auto rid = _gaussianEdgeExtend(end, r) + p;
            auto lid = _gaussianEdgeExtend(end, l) + p;
            acc += A(src[rid]) - A(src[lid]);
            //ignored rounding for the performance. It should be originally: acc * iarr
            dst[i++] = ALPHA_BLEND(color, static_cast<uint8_t>(acc * iarr));
        }
    }
}

template<typename T>
static void _dropShadowShift(T* dst, T* src, int stride_pixels, SwBBox& region, SwPoint& offset, uint8_t opacity, bool direct)
{
    src += (region.min.y * stride_pixels + region.min.x);
    dst += (region.min.y * stride_pixels + region.min.x);

    auto w = region.max.x - region.min.x;
    auto h = region.max.y - region.min.y;
    auto translucent = (direct || opacity < 255);

    //shift offset
    if (region.min.x + offset.x < 0) src -= offset.x;
    else dst += offset.x;

    if (region.min.y + offset.y < 0) src -= (offset.y * stride_pixels);
    else dst += (offset.y * stride_pixels);

    for (auto y = 0; y < h; ++y) {
        if (translucent) rasterTranslucentPixel(dst, src, w, opacity);
        else rasterPixel(dst, src, w, opacity);
        src += stride_pixels;
        dst += stride_pixels;
    }
}


bool effectDropShadowRegion(RenderEffectDropShadow* params)
{
    //bbox region expansion for feathering
    auto& region = params->extend;
    auto& offset = static_cast<SwDropShadow*>(params->rd)->offset;
    auto extra = static_cast<SwDropShadow*>(params->rd)->extends;

    region.x = -extra;
    region.w = extra * 2;
    region.y = -extra;
    region.h = extra * 2;

    region.x = std::min(region.x + (int32_t)offset.x, region.x);
    region.y = std::min(region.y + (int32_t)offset.y, region.y);
    region.w += abs(offset.x);
    region.h += abs(offset.y);

    return true;
}


void effectDropShadowUpdate(RenderEffectDropShadow* params, const Matrix& transform)
{
    if (!params->rd) params->rd = (SwDropShadow*)malloc(sizeof(SwDropShadow));
    auto rd = static_cast<SwDropShadow*>(params->rd);

    //compute box kernel sizes
    auto scale = sqrt(transform.e11 * transform.e11 + transform.e12 * transform.e12);
    rd->extends = _gaussianInit(rd, std::pow(params->sigma * scale, 2), params->quality);

    //invalid
    if (rd->extends == 0 || params->color[3] == 0) {
        params->valid = false;
        return;
    }

    //offset
    if (params->distance > 0.0f) {
        auto radian = tvg::deg2rad(90.0f - params->angle);
        rd->offset = {(SwCoord)(params->distance * cosf(radian)), (SwCoord)(-1.0f * params->distance * sinf(radian))};
    } else {
        rd->offset = {0, 0};
    }

    params->valid = true;
}

template<typename PIXEL_T> void foo()
{
    TVGERR("SW_ENGINE", "Not supported grayscale Gaussian Blur!");
}

//A quite same integration with effectGaussianBlur(). See it for detailed comments.
//surface[0]: the original image, to overlay it into the filtered image.
//surface[1]: temporary buffer for generating the filtered image.
bool effectDropShadow(SwCompositor* cmp, SwSurface<PIXEL_TYPE>* surface[2], const RenderEffectDropShadow* params, bool direct)
{
    //FIXME: if the body is partially visible due to clipping, the shadow also becomes partially visible.

    auto data = static_cast<SwDropShadow*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);

    //outside the screen
    if (abs(data->offset.x) >= w || abs(data->offset.y) >= h) return true;

    SwImage* buffer[] = {&surface[0]->compositor->image, &surface[1]->compositor->image};
    auto color = cmp->recoverSfc->join(params->color[0], params->color[1], params->color[2], 255);
    auto stride_pixels = cmp->image.stride_pixels;
    auto front = cmp->image.pixel_buffer;
    auto back = buffer[1]->pixel_buffer;

    auto opacity = direct ? MULTIPLY(params->color[3], cmp->opacity) : params->color[3];

    TVGLOG("SW_ENGINE", "DropShadow region(%ld, %ld, %ld, %ld) params(%f %f %f), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->angle, params->distance, params->sigma, data->level);

    //saving the original image in order to overlay it into the filtered image.
    _dropShadowFilter<PIXEL_TYPE>(back, front, stride_pixels, w, h, bbox, data->kernel[0], color, false);
    std::swap(front, buffer[0]->pixel_buffer);
    std::swap(front, back);

    //horizontal
    for (int i = 1; i < data->level; ++i) {
        _dropShadowFilter<PIXEL_TYPE>(back, front, stride_pixels, w, h, bbox, data->kernel[i], color, false);
        std::swap(front, back);
    }

    //vertical
    rasterXYFlip(front, back, stride_pixels, w, h, bbox, false);
    std::swap(front, back);

    for (int i = 0; i < data->level; ++i) {
        _dropShadowFilter<PIXEL_TYPE>(back, front, stride_pixels, h, w, bbox, data->kernel[i], color, true);
        std::swap(front, back);
    }

    rasterXYFlip(front, back, stride_pixels, h, w, bbox, true);
    std::swap(cmp->image.pixel_buffer, back);

    //draw to the main surface directly
    if (direct) {
        _dropShadowShift(cmp->recoverSfc->pixel_buffer, cmp->image.pixel_buffer, stride_pixels, bbox, data->offset, opacity, direct);
        std::swap(cmp->image.pixel_buffer, buffer[0]->pixel_buffer);
        return true;
    }

    //draw to the intermediate surface
    rasterClear(surface[1], bbox.min.x, bbox.min.y, w, h);
    _dropShadowShift(buffer[1]->pixel_buffer, cmp->image.pixel_buffer, stride_pixels, bbox, data->offset, opacity, direct);
    std::swap(cmp->image.pixel_buffer, buffer[1]->pixel_buffer);

    //compositing shadow and body
    auto s = buffer[0]->pixel_buffer + (bbox.min.y * buffer[0]->stride_pixels + bbox.min.x);
    auto d = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);

    for (auto y = 0; y < h; ++y) {
        rasterTranslucentPixel<PIXEL_TYPE>(d, s, w, 255);
        s += buffer[0]->stride_pixels;
        d += cmp->image.stride_pixels;
    }

    return true;
}


/************************************************************************/
/* Fill Implementation                                                  */
/************************************************************************/

void effectFillUpdate(RenderEffectFill* params)
{
    params->valid = true;
}


bool effectFill(SwCompositor* cmp, const RenderEffectFill* params, bool direct)
{
    auto opacity = direct ? MULTIPLY(params->color[3], cmp->opacity) : params->color[3];

    auto& bbox = cmp->bbox;
    auto w = size_t(bbox.max.x - bbox.min.x);
    auto h = size_t(bbox.max.y - bbox.min.y);
    auto color = cmp->recoverSfc->join(params->color[0], params->color[1], params->color[2], 255);

    TVGLOG("SW_ENGINE", "Fill region(%ld, %ld, %ld, %ld), param(%d %d %d %d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->color[0], params->color[1], params->color[2], params->color[3]);

    if (direct) {
        auto dbuffer = cmp->recoverSfc->pixel_buffer + (bbox.min.y * cmp->recoverSfc->stride_pixels + bbox.min.x);
        auto sbuffer = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            auto src = sbuffer;
            for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                auto a = MULTIPLY(opacity, A(*src));
                auto tmp = ALPHA_BLEND(color, a);
                *dst = tmp + ALPHA_BLEND(*dst, 255 - a);
            }
            dbuffer += cmp->image.stride_pixels;
            sbuffer += cmp->recoverSfc->stride_pixels;
        }
        cmp->valid = true;  //no need the subsequent composition
    } else {
        auto dbuffer = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            for (size_t x = 0; x < w; ++x, ++dst) {
                *dst = ALPHA_BLEND(color, MULTIPLY(opacity, A(*dst)));
            }
            dbuffer += cmp->image.stride_pixels;
        }
    }
    return true;
}


/************************************************************************/
/* Tint Implementation                                                  */
/************************************************************************/

void effectTintUpdate(RenderEffectTint* params)
{
    params->valid = true;
}


bool effectTint(SwCompositor* cmp, const RenderEffectTint* params, bool direct)
{
    auto& bbox = cmp->bbox;
    auto w = size_t(bbox.max.x - bbox.min.x);
    auto h = size_t(bbox.max.y - bbox.min.y);
    PIXEL_TYPE black = cmp->recoverSfc->join(params->black[0], params->black[1], params->black[2], 255);
    PIXEL_TYPE white = cmp->recoverSfc->join(params->white[0], params->white[1], params->white[2], 255);
    auto opacity = cmp->opacity;
    auto luma = cmp->recoverSfc->alphas[2];  //luma function

    TVGLOG("SW_ENGINE", "Tint region(%ld, %ld, %ld, %ld), param(%d %d %d, %d %d %d, %d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->black[0], params->black[1], params->black[2], params->white[0], params->white[1], params->white[2], params->intensity);

    /* Tint Formula: (1 - L) * Black + L * White, where the L is Luminance. */

    if (direct) {
        auto dbuffer = cmp->recoverSfc->pixel_buffer + (bbox.min.y * cmp->recoverSfc->stride_pixels + bbox.min.x);
        auto sbuffer = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            auto src = sbuffer;
            for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                PIXEL_TYPE tmp = rasterUnpremultiply(*src);
                auto val = INTERPOLATE(INTERPOLATE(black, white, luma((uint8_t*)&tmp)), tmp, params->intensity);
                *dst = INTERPOLATE(val, *dst, MULTIPLY(opacity, A(tmp)));
            }
            dbuffer += cmp->image.stride_pixels;
            sbuffer += cmp->recoverSfc->stride_pixels;
        }
        cmp->valid = true;  //no need the subsequent composition
    } else {
        auto dbuffer = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            for (size_t x = 0; x < w; ++x, ++dst) {
                PIXEL_TYPE tmp = rasterUnpremultiply(*dst);
                auto val = INTERPOLATE(INTERPOLATE(black, white, luma((uint8_t*)&tmp)), tmp, params->intensity);
                *dst = ALPHA_BLEND(val, A(tmp));
            }
            dbuffer += cmp->image.stride_pixels;
        }
    }

    return true;
}


/************************************************************************/
/* Tritone Implementation                                              */
/************************************************************************/

template<typename PIXEL_T>
static PIXEL_T _trintone(PIXEL_T s, PIXEL_T m, PIXEL_T h, int l)
{
    /* Tritone Formula:
       if (L < 0.5) { (1 - 2L) * Shadow + 2L * Midtone }
       else { (1 - 2(L - 0.5)) * Midtone + (2(L - 0.5)) * Highlight }
       Where the L is Luminance. */

    if (l < 128) {
        uint8_t a = std::min(l * 2, 255);
        return ALPHA_BLEND(s, 255 - a) + ALPHA_BLEND(m, a);
    } else {
        uint8_t a = 2 * std::max(0, l - 128);
        return ALPHA_BLEND(m, 255 - a) + ALPHA_BLEND(h, a);
    }
}


void effectTritoneUpdate(RenderEffectTritone* params)
{
    params->valid = true;
}


bool effectTritone(SwCompositor* cmp, const RenderEffectTritone* params, bool direct)
{
    auto& bbox = cmp->bbox;
    auto w = size_t(bbox.max.x - bbox.min.x);
    auto h = size_t(bbox.max.y - bbox.min.y);
    auto shadow = cmp->recoverSfc->join(params->shadow[0], params->shadow[1], params->shadow[2], 255);
    auto midtone = cmp->recoverSfc->join(params->midtone[0], params->midtone[1], params->midtone[2], 255);
    auto highlight = cmp->recoverSfc->join(params->highlight[0], params->highlight[1], params->highlight[2], 255);
    auto opacity = cmp->opacity;
    auto luma = cmp->recoverSfc->alphas[2];  //luma function

    TVGLOG("SW_ENGINE", "Tritone region(%ld, %ld, %ld, %ld), param(%d %d %d, %d %d %d, %d %d %d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->shadow[0], params->shadow[1], params->shadow[2], params->midtone[0], params->midtone[1], params->midtone[2], params->highlight[0], params->highlight[1], params->highlight[2]);

    if (direct) {
        auto dbuffer = cmp->recoverSfc->pixel_buffer + (bbox.min.y * cmp->recoverSfc->stride_pixels + bbox.min.x);
        auto sbuffer = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            auto src = sbuffer;
            for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                auto tmp = rasterUnpremultiply(*src);
                *dst = INTERPOLATE(_trintone(shadow, midtone, highlight, luma((uint8_t*)&tmp)), *dst, MULTIPLY(opacity, A(tmp)));
            }
            dbuffer += cmp->image.stride_pixels;
            sbuffer += cmp->recoverSfc->stride_pixels;
        }
        cmp->valid = true;  //no need the subsequent composition
    } else {
        auto dbuffer = cmp->image.pixel_buffer + (bbox.min.y * cmp->image.stride_pixels + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            for (size_t x = 0; x < w; ++x, ++dst) {
                auto tmp = rasterUnpremultiply(*dst);
                *dst = ALPHA_BLEND(_trintone(shadow, midtone, highlight, luma((uint8_t*)&tmp)), A(tmp));
            }
            dbuffer += cmp->image.stride_pixels;
        }
    }

    return true;
}