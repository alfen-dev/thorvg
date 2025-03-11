/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include "thorvg.h"

#include "tvgSwCommon.h"


template<>
inline uint32_t JOIN<uint32_t>(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3)
{
    LV_ASSERT(false);
    return (c0 << 24 | c1 << 16 | c2 << 8 | c3);
}

template<>
inline uint16_t JOIN<uint16_t>(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    //return (a << 24 | r << 16 | g << 8 | b);
    return (((uint16_t)(r & 0b11111000)) << 8) | (((uint16_t)(g & 0b11111100)) << 3) | (((uint16_t)b) >> 3);
}


//TODO: BlendMethod could remove the alpha parameter.
template<>
inline uint32_t opBlendDifference<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    //if (s > d) => s - d
    //else => d - s
    auto c1 = (C1(s) > C1(d)) ? (C1(s) - C1(d)) : (C1(d) - C1(s));
    auto c2 = (C2(s) > C2(d)) ? (C2(s) - C2(d)) : (C2(d) - C2(s));
    auto c3 = (C3(s) > C3(d)) ? (C3(s) - C3(d)) : (C3(d) - C3(s));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendDifference<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    //if (s > d) => s - d
    //else => d - s
    uint8_t r_s = (s & 0xf800) >> 8;
    uint8_t g_s = (s & 0x07e0) >> 3;
    uint8_t b_s = (s & 0x001f) << 3;
    uint8_t r_d = (d & 0xf800) >> 8;
    uint8_t g_d = (d & 0x07e0) >> 3;
    uint8_t b_d = (d & 0x001f) << 3;


    auto r = (r_s > r_d) ? (r_s - r_d) : (r_d - r_s);
    auto g = (g_s > g_d) ? (g_s - g_d) : (g_d - g_s);
    auto b = (b_s > b_d) ? (b_s - b_d) : (b_d - b_s);
    return JOIN<uint16_t>(255, r, g, b);
}


template<>
inline uint32_t opBlendExclusion<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // (s + d) - (2 * s * d)
    auto c1 = C1(s) + C1(d) - 2 * MULTIPLY(C1(s), C1(d));
    tvg::clamp(c1, 0, 255);
    auto c2 = C2(s) + C2(d) - 2 * MULTIPLY(C2(s), C2(d));
    tvg::clamp(c2, 0, 255);
    auto c3 = C3(s) + C3(d) - 2 * MULTIPLY(C3(s), C3(d));
    tvg::clamp(c3, 0, 255);
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendExclusion<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    //A + B - 2AB
    uint8_t r_s = (s & 0xf800) >> 8;
    uint8_t g_s = (s & 0x07e0) >> 3;
    uint8_t b_s = (s & 0x001f) << 3;
    uint8_t r_d = (d & 0xf800) >> 8;
    uint8_t g_d = (d & 0x07e0) >> 3;
    uint8_t b_d = (d & 0x001f) << 3;

    auto r = r_s + r_d - 2 * MULTIPLY(r_s, r_d);
    auto g = g_s + g_d - 2 * MULTIPLY(g_s, g_d);
    auto b = b_s + b_d - 2 * MULTIPLY(b_s, b_d);
    tvg::clamp(r, 0, 255);
    tvg::clamp(g, 0, 255);
    tvg::clamp(b, 0, 255);
    return JOIN<uint16_t>(255, r, g, b);
}


template<>
inline uint32_t opBlendAdd<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // s + d
    auto c1 = std::min(C1(s) + C1(d), 255);
    auto c2 = std::min(C2(s) + C2(d), 255);
    auto c3 = std::min(C3(s) + C3(d), 255);
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendAdd<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // s + d
    uint8_t r_s = (s & 0xf800) >> 8;
    uint8_t g_s = (s & 0x07e0) >> 3;
    uint8_t b_s = (s & 0x001f) << 3;
    uint8_t r_d = (d & 0xf800) >> 8;
    uint8_t g_d = (d & 0x07e0) >> 3;
    uint8_t b_d = (d & 0x001f) << 3;

    auto r = std::min(r_s + r_d, 255);
    auto g = std::min(g_s + g_d, 255);
    auto b = std::min(b_s + b_d, 255);
    return JOIN<uint16_t>(255, r, g, b);
}


template<>
inline uint32_t opBlendScreen<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // s + d - s * d
    auto c1 = C1(s) + C1(d) - MULTIPLY(C1(s), C1(d));
    auto c2 = C2(s) + C2(d) - MULTIPLY(C2(s), C2(d));
    auto c3 = C3(s) + C3(d) - MULTIPLY(C3(s), C3(d));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendScreen<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // s + d - s * d
    uint8_t r_s = (s & 0xf800) >> 8;
    uint8_t g_s = (s & 0x07e0) >> 3;
    uint8_t b_s = (s & 0x001f) << 3;
    uint8_t r_d = (d & 0xf800) >> 8;
    uint8_t g_d = (d & 0x07e0) >> 3;
    uint8_t b_d = (d & 0x001f) << 3;

    auto r = r_s + r_d - MULTIPLY(r_s, r_d);
    auto g = g_s + g_d - MULTIPLY(g_s, g_d);
    auto b = b_s + b_d - MULTIPLY(b_s, b_d);
    return JOIN<uint16_t>(255, r, g, b);
}


template<>
inline uint32_t opBlendMultiply<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // s * d
    auto c1 = MULTIPLY(C1(s), C1(d));
    auto c2 = MULTIPLY(C2(s), C2(d));
    auto c3 = MULTIPLY(C3(s), C3(d));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendMultiply<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // s * d
    uint8_t r_s = (s & 0xf800) >> 8;
    uint8_t g_s = (s & 0x07e0) >> 3;
    uint8_t b_s = (s & 0x001f) << 3;
    uint8_t r_d = (d & 0xf800) >> 8;
    uint8_t g_d = (d & 0x07e0) >> 3;
    uint8_t b_d = (d & 0x001f) << 3;

    auto r = MULTIPLY(r_s, r_d);
    auto g = MULTIPLY(g_s, g_d);
    auto b = MULTIPLY(b_s, b_d);
    return JOIN<uint16_t>(255, r, g, b);
}


template<>
inline uint32_t opBlendOverlay<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // if (2 * d < da) => 2 * s * d,
    // else => 1 - 2 * (1 - s) * (1 - d)
    auto c1 = (C1(d) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), C1(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - C1(d))));
    auto c2 = (C2(d) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), C2(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - C2(d))));
    auto c3 = (C3(d) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), C3(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - C3(d))));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendOverlay<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // if (2 * d < da) => 2 * s * d,
    // else => 1 - 2 * (1 - s) * (1 - d)
    auto c1 = (C1(d) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), C1(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - C1(d))));
    auto c2 = (C2(d) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), C2(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - C2(d))));
    auto c3 = (C3(d) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), C3(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - C3(d))));
    return JOIN<uint16_t>(255, c1, c2, c3);
}


template<>
inline uint32_t opBlendDarken<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // min(s, d)
    auto c1 = std::min(C1(s), C1(d));
    auto c2 = std::min(C2(s), C2(d));
    auto c3 = std::min(C3(s), C3(d));
    return JOIN<uint32_t>(255, c1, c2, c3);
}
template<>
inline uint16_t opBlendDarken<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // min(s, d)
    auto c1 = std::min(C1(s), C1(d));
    auto c2 = std::min(C2(s), C2(d));
    auto c3 = std::min(C3(s), C3(d));
    return JOIN<uint16_t>(255, c1, c2, c3);
}


template<>
inline uint32_t opBlendLighten<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // max(s, d)
    auto c1 = std::max(C1(s), C1(d));
    auto c2 = std::max(C2(s), C2(d));
    auto c3 = std::max(C3(s), C3(d));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendLighten<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // max(s, d)
    auto c1 = std::max(C1(s), C1(d));
    auto c2 = std::max(C2(s), C2(d));
    auto c3 = std::max(C3(s), C3(d));
    return JOIN<uint16_t>(255, c1, c2, c3);
}


template<>
inline uint32_t opBlendColorDodge<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // d / (1 - s)
    s = 0xffffffff - s;
    auto c1 = (C1(s) == 0) ? C1(d) : std::min(C1(d) * 255 / C1(s), 255);
    auto c2 = (C2(s) == 0) ? C2(d) : std::min(C2(d) * 255 / C2(s), 255);
    auto c3 = (C3(s) == 0) ? C3(d) : std::min(C3(d) * 255 / C3(s), 255);
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendColorDodge<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // d / (1 - s)
    s = 0xffff - s;
    auto c1 = (C1(s) > 0) ? (C1(d) / C1(s)) : C1(d);
    auto c2 = (C2(s) > 0) ? (C2(d) / C2(s)) : C2(d);
    auto c3 = (C3(s) > 0) ? (C3(d) / C3(s)) : C3(d);
    return JOIN<uint16_t>(255, c1, c2, c3);
}


template<>
inline uint32_t opBlendColorBurn<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // 1 - (1 - d) / s
    auto id = 0xffffffff - d;
    auto c1 = (C1(s) == 0) ? C1(d) : 255 - std::min(C1(id) * 255 / C1(s), 255);
    auto c2 = (C2(s) == 0) ? C2(d) : 255 - std::min(C2(id) * 255 / C2(s), 255);
    auto c3 = (C3(s) == 0) ? C3(d) : 255 - std::min(C3(id) * 255 / C3(s), 255);

    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendColorBurn<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    // 1 - (1 - d) / s
    uint16_t id = 0xffff - d;
    auto c1 = 255 - ((C1(s) > 0) ? (C1(id) / C1(s)) : C1(id));
    auto c2 = 255 - ((C2(s) > 0) ? (C2(id) / C2(s)) : C2(id));
    auto c3 = 255 - ((C3(s) > 0) ? (C3(id) / C3(s)) : C3(id));
    return JOIN<uint16_t>(255, c1, c2, c3);
}


template<>
inline uint32_t opBlendHardLight<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // if (s < sa), (2 * s * d)
    // else (sa * da) - 2 * (da - s) * (sa - d)
    auto c1 = (C1(s) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), C1(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - C1(d))));
    auto c2 = (C2(s) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), C2(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - C2(d))));
    auto c3 = (C3(s) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), C3(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - C3(d))));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendHardLight<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    auto c1 = (C1(s) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), C1(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - C1(d))));
    auto c2 = (C2(s) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), C2(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - C2(d))));
    auto c3 = (C3(s) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), C3(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - C3(d))));
    return JOIN<uint16_t>(255, c1, c2, c3);
}


template<>
inline uint32_t opBlendSoftLight<uint32_t>(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    //(255 - 2 * s) * (d * d) + (2 * s * b)
    auto c1 = MULTIPLY(255 - std::min(255, 2 * C1(s)), MULTIPLY(C1(d), C1(d))) + MULTIPLY(std::min(255, 2 * C1(s)), C1(d));
    auto c2 = MULTIPLY(255 - std::min(255, 2 * C2(s)), MULTIPLY(C2(d), C2(d))) + MULTIPLY(std::min(255, 2 * C2(s)), C2(d));
    auto c3 = MULTIPLY(255 - std::min(255, 2 * C3(s)), MULTIPLY(C3(d), C3(d))) + MULTIPLY(std::min(255, 2 * C3(s)), C3(d));
    return JOIN<uint32_t>(255, c1, c2, c3);
}

template<>
inline uint16_t opBlendSoftLight<uint16_t>(uint16_t s, uint16_t d, TVG_UNUSED uint8_t a)
{
    //(255 - 2 * s) * (d * d) + (2 * s * b)
    auto c1 = std::min(255, MULTIPLY(255 - std::min(255, 2 * C1(s)), MULTIPLY(C1(d), C1(d))) + 2 * MULTIPLY(C1(s), C1(d)));
    auto c2 = std::min(255, MULTIPLY(255 - std::min(255, 2 * C2(s)), MULTIPLY(C2(d), C2(d))) + 2 * MULTIPLY(C2(s), C2(d)));
    auto c3 = std::min(255, MULTIPLY(255 - std::min(255, 2 * C3(s)), MULTIPLY(C3(d), C3(d))) + 2 * MULTIPLY(C3(s), C3(d)));
    return JOIN<uint16_t>(255, c1, c2, c3);
}
