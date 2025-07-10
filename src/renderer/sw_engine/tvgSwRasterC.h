/*
 * Copyright (c) 2021 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_SW_RASTER_C_H_
#define _TVG_SW_RASTER_C_H_
 
template<typename PIXEL_T>
static void inline cRasterTranslucentPixels(PIXEL_T* dst, PIXEL_T* src, uint32_t len, uint8_t opacity)
{
    //TODO: 64bits faster?
    if (opacity == 255) {
        for (uint32_t x = 0; x < len; ++x, ++dst, ++src) {
            *dst = *src + ALPHA_BLEND(*dst, IA(*src));
        }
    } else {
        for (uint32_t x = 0; x < len; ++x, ++dst, ++src) {
            auto tmp = ALPHA_BLEND(*src, opacity);
            *dst = tmp + ALPHA_BLEND(*dst, IA(tmp));
        }
    }
}


template<typename PIXEL_T>
static void inline cRasterPixels(PIXEL_T* dst, PIXEL_T* src, uint32_t len, uint8_t opacity)
{
    //TODO: 64bits faster?
    if (opacity == 255) {
        for (uint32_t x = 0; x < len; ++x, ++dst, ++src) {
            *dst = *src;
        }
    } else {
        cRasterTranslucentPixels(dst, src, len, opacity);
    }
}


template<typename PIXEL_T>
static void inline cRasterPixels(PIXEL_T* dst, PIXEL_T val, uint32_t offset, int32_t len)
{
    dst += offset;

    //fix the misaligned memory
    auto alignOffset = (long long) dst % 8;
    if (alignOffset > 0) {
        if (sizeof(PIXEL_T) == 4) alignOffset /= 4;
        else if (sizeof(PIXEL_T) == 2) alignOffset /= 2;
        else if (sizeof(PIXEL_T) == 1) alignOffset = 8 - alignOffset;
        while (alignOffset > 0 && len > 0) {
            *dst++ = val;
            --len;
            --alignOffset;
        }
    }

    //64bits faster clear
    if ((sizeof(PIXEL_T) == 4)) {
        auto val64 = (uint64_t(val) << 32) | uint64_t(val);
        while (len > 1) {
            *reinterpret_cast<uint64_t*>(dst) = val64;
            len -= 2;
            dst += 2;
        }
    } else if (sizeof(PIXEL_T) == 2) {
        // 4 times the 16 bit value fits 64 bits, 
        // so first assemble a 32 bits value using twice a 16 bit, 
        // and duplicate the 32 bits for thw 64 bits
        auto val32 = (uint32_t(val) << 16) | uint32_t(val);
        auto val64 = (uint64_t(val32) << 32) | val32;
        while (len > 3) {
            *reinterpret_cast<uint64_t*>(dst) = val64;
            len -= 4;
            dst += 4;
        }
    } else if (sizeof(PIXEL_T) == 1) {
        auto val32 = (uint32_t(val) << 24) | (uint32_t(val) << 16) | (uint32_t(val) << 8) | uint32_t(val);
        auto val64 = (uint64_t(val32) << 32) | val32;
        while (len > 7) {
            *reinterpret_cast<uint64_t*>(dst) = val64;
            len -= 8;
            dst += 8;
        }
    }

    //leftovers
    while (len--) *dst++ = val;
}


template<typename PIXEL_T>
static bool inline cRasterTranslucentRle(SwSurface<PIXEL_T>* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
{
    const SwSpan* end;
    int32_t x, len;

    //16/32bits channels
    if (surface->channelSize != sizeof(uint8_t)) {
        PIXEL_T color = surface->join(c.r, c.g, c.b, c.a);
        PIXEL_T src;
        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            PIXEL_T* dst = &surface->pixelBuffer[span->y * surface->stride + x];
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;
            uint8_t ialpha = IA(src);
            for (auto x = 0; x < len; ++x, ++dst) {
                *dst = src + ALPHA_BLEND(*dst, ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        uint8_t src;
        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            auto dst = &surface->buf8[span->y * surface->stride + x];
            if (span->coverage < 255) src = MULTIPLY(span->coverage, c.a);
            else src = c.a;
            auto ialpha = ~c.a;
            for (auto x = 0; x < len; ++x, ++dst) {
                *dst = src + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


static bool inline cRasterTranslucentRect(SwSurface<PixelType>* surface, const RenderRegion& bbox, const RenderColor& c)
{
    //16/32bits channels
    if (surface->channelSize != sizeof(uint8_t)) {
        PixelType color = surface->join(c.r, c.g, c.b, c.a);
        PixelType* buffer = surface->pixelBuffer + (bbox.min.y * surface->stride) + bbox.min.x;
        uint8_t ialpha = 255 - c.a;
        for (uint32_t y = 0; y < bbox.h(); ++y) {
            PixelType* dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < bbox.w(); ++x, ++dst) {
                *dst = color + ALPHA_BLEND(*dst, ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        auto buffer = surface->buf8 + (bbox.min.y * surface->stride) + bbox.min.x;
        uint8_t ialpha = ~c.a;
        for (uint32_t y = 0; y < bbox.h(); ++y) {
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < bbox.w(); ++x, ++dst) {
                *dst = c.a + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


static bool inline cRasterABGRtoARGB(RenderSurface* surface)
{
    TVGLOG("SW_ENGINE", "Convert (32bit) ColorSpace ABGR - ARGB [Size: %d x %d]", surface->w, surface->h);

    //64bits faster converting
    if (surface->w % 2 == 0) {
        auto buffer = reinterpret_cast<uint64_t*>(surface->pixelBuffer);
        for (uint32_t y = 0; y < surface->h; ++y, buffer += surface->stride / 2) {
            auto dst = buffer;
            for (uint32_t x = 0; x < surface->w / 2; ++x, ++dst) {
                auto c = *dst;
                //flip Blue, Red channels
                *dst = (c & 0xff000000ff000000) + ((c & 0x00ff000000ff0000) >> 16) + (c & 0x0000ff000000ff00) + ((c & 0x000000ff000000ff) << 16);
            }
        }
    //default converting
    } else {
        auto buffer = surface->pixelBuffer;
        for (uint32_t y = 0; y < surface->h; ++y, buffer += surface->stride) {
            auto dst = buffer;
            for (uint32_t x = 0; x < surface->w; ++x, ++dst) {
                auto c = *dst;
                //flip Blue, Red channels
                *dst = (c & 0xff000000) + ((c & 0x00ff0000) >> 16) + (c & 0x0000ff00) + ((c & 0x000000ff) << 16);
            }
        }
    }
    return true;
}


static bool inline cRasterARGBtoABGR(RenderSurface* surface)
{
    //exactly same with ABGRtoARGB
    return cRasterABGRtoARGB(surface);
}

#endif