/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_SW_COMMON_H_
#define _TVG_SW_COMMON_H_

#include <algorithm>
#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgRender.h"

#define SW_CURVE_TYPE_POINT 0
#define SW_CURVE_TYPE_CUBIC 1
#define SW_ANGLE_PI (180L << 16)
#define SW_ANGLE_2PI (SW_ANGLE_PI << 1)
#define SW_ANGLE_PI2 (SW_ANGLE_PI >> 1)


static inline float TO_FLOAT(int32_t val)
{
    return static_cast<float>(val) / 64.0f;
}

struct SwPoint
{
    int32_t x, y;

    SwPoint& operator-=(const SwPoint& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    SwPoint& operator+=(const SwPoint& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    SwPoint operator+(const SwPoint& rhs) const
    {
        return {x + rhs.x, y + rhs.y};
    }

    SwPoint operator-(const SwPoint& rhs) const
    {
        return {x - rhs.x, y - rhs.y};
    }

    bool operator==(const SwPoint& rhs) const
    {
        return (x == rhs.x && y == rhs.y);
    }

    bool operator!=(const SwPoint& rhs) const
    {
        return (x != rhs.x || y != rhs.y);
    }

    bool zero() const
    {
        if (x == 0 && y == 0) return true;
        else return false;
    }

    bool small() const
    {
        //2 is epsilon...
        if (abs(x) < 2 && abs(y) < 2) return true;
        else return false;
    }

    Point toPoint() const
    {
        return {TO_FLOAT(x),  TO_FLOAT(y)};
    }
};

struct SwSize
{
    int32_t w, h;
};

struct SwOutline
{
    Array<SwPoint> pts;             //the outline's points
    Array<uint32_t> cntrs;          //the contour end points
    Array<uint8_t> types;           //curve type
    Array<bool> closed;             //opened or closed path?
    FillRule fillRule;
};

struct SwSpan
{
    uint16_t x, y;
    uint16_t len;
    uint8_t coverage;

    bool fetch(const RenderRegion& bbox, int32_t& x, int32_t& len) const
    {
        x = std::max((int32_t)this->x, bbox.min.x);
        len = std::min((int32_t)(this->x + this->len), bbox.max.x) - x;
        return (len > 0) ? true : false;
    }
};


struct SwRle
{
    Array<SwSpan> spans;

    const SwSpan* fetch(const RenderRegion& bbox, const SwSpan** end) const
    {
        return fetch(bbox.min.y, bbox.max.y - 1, end);
    }

    const SwSpan* fetch(int32_t min, uint32_t max, const SwSpan** end) const
    {
        const SwSpan* begin;

        if (min <= spans.first().y) {
            begin = spans.begin();
        } else {
            auto comp = [](const SwSpan& span, int y) { return span.y < y; };
            begin = lower_bound(spans.begin(), spans.end(), min, comp);
        }
        if (end) {
            if (max >= spans.last().y) {
                *end = spans.end();
            } else {
                auto comp = [](int y, const SwSpan& span) { return y < span.y; };
                *end = upper_bound(spans.begin(), spans.end(), max, comp);
            }
        }
        return begin;
    }

    bool invalid() const { return spans.empty(); }
    bool valid() const { return !invalid(); }
    uint32_t size() const { return spans.count; }
    SwSpan* data() const { return spans.data; }
};

struct SwFill
{
    struct SwLinear {
        float dx, dy;
        float offset;
    };

    struct SwRadial {
        float a11, a12, a13;
        float a21, a22, a23;
        float fx, fy, fr;
        float dx, dy, dr;
        float invA, a;
    };

    union {
        SwLinear linear;
        SwRadial radial;
    };

    uint32_t* ctable;
    FillSpread spread;

    bool solid = false; //solid color fill with the last color from colorStops
    bool translucent;
};

struct SwStrokeBorder
{
    uint32_t ptsCnt;
    uint32_t maxPts;
    SwPoint* pts;
    uint8_t* tags;
    int32_t start;     //index of current sub-path start point
    bool movable;      //true: for ends of lineto borders
};

struct SwStroke
{
    int64_t angleIn;
    int64_t angleOut;
    SwPoint center;
    int64_t lineLength;
    int64_t subPathAngle;
    SwPoint ptStartSubPath;
    int64_t subPathLineLength;
    int64_t width;
    int64_t miterlimit;
    SwFill* fill = nullptr;
    SwStrokeBorder borders[2];
    float sx, sy;
    StrokeCap cap;
    StrokeJoin join;
    StrokeJoin joinSaved;
    bool firstPt;
    bool closedSubPath;
    bool handleWideStrokes;
};

struct SwDashStroke
{
    SwOutline* outline = nullptr;
    float curLen = 0;
    int32_t curIdx = 0;
    Point ptStart = {0, 0};
    Point ptCur = {0, 0};
    float* pattern = nullptr;
    uint32_t cnt = 0;
    bool curOpGap = false;
    bool move = true;
};

struct SwShape
{
    SwOutline* outline = nullptr;
    SwStroke* stroke = nullptr;
    SwFill* fill = nullptr;
    SwRle* rle = nullptr;
    SwRle* strokeRle = nullptr;
    RenderRegion bbox;           //Keep it boundary without stroke region. Using for optimal filling.

    bool fastTrack = false;   //Fast Track: axis-aligned rectangle without any clips?
};

struct SwImage
{
    SwOutline*   outline = nullptr;
    SwRle*   rle = nullptr;
    union {
        pixel_t*  data;      //system based data pointer
        uint32_t* buf32;     //for explicit 32bits channels
        uint8_t*  buf8;      //for explicit 8bits grayscale
    };
    uint32_t     w, h, stride;
    int32_t      ox = 0;         //offset x
    int32_t      oy = 0;         //offset y
    float        scale;
    uint8_t      channelSize;

    bool         direct = false;  //draw image directly (with offset)
    bool         scaled = false;  //draw scaled image
};

typedef uint8_t(*SwMask)(uint8_t s, uint8_t d, uint8_t a);                  //src, dst, alpha
typedef uint32_t(*SwBlender)(uint32_t s, uint32_t d, uint8_t a);            //src, dst, alpha
typedef uint32_t(*SwJoin)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);      //color channel join
typedef uint8_t(*SwAlpha)(uint8_t*);                                        //blending alpha

struct SwCompositor;

struct SwSurface : RenderSurface
{
    SwJoin  join;
    SwAlpha alphas[4];                    //Alpha:2, InvAlpha:3, Luma:4, InvLuma:5
    SwBlender blender = nullptr;          //blender (optional)
    SwCompositor* compositor = nullptr;   //compositor (optional)
    BlendMethod blendMethod = BlendMethod::Normal;

    SwAlpha alpha(MaskMethod method)
    {
        auto idx = (int)(method) - 1;       //-1 for None
        return alphas[idx > 3 ? 0 : idx];   //CompositeMethod has only four Matting methods.
    }

    SwSurface()
    {
    }

    SwSurface(const SwSurface* rhs) : RenderSurface(rhs)
    {
        join = rhs->join;
        memcpy(alphas, rhs->alphas, sizeof(alphas));
        blender = rhs->blender;
        compositor = rhs->compositor;
        blendMethod = rhs->blendMethod;
     }
};

struct SwCompositor : RenderCompositor
{
    SwSurface* recoverSfc;                  //Recover surface when composition is started
    SwCompositor* recoverCmp;               //Recover compositor when composition is done
    SwImage image;
    RenderRegion bbox;
    bool valid;
};

struct SwMpool
{
    SwOutline* outline;
    SwOutline* strokeOutline;
    SwOutline* dashOutline;
    unsigned allocSize;
};

static inline int32_t TO_SWCOORD(float val)
{
    return int32_t(val * 64.0f);
}

static inline uint32_t JOIN(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3)
{
    return (c0 << 24 | c1 << 16 | c2 << 8 | c3);
}

static inline uint32_t ALPHA_BLEND(uint32_t c, uint32_t a)
{
    ++a;
    return (((((c >> 8) & 0x00ff00ff) * a) & 0xff00ff00) + ((((c & 0x00ff00ff) * a) >> 8) & 0x00ff00ff));
}

static inline uint32_t INTERPOLATE(uint32_t s, uint32_t d, uint8_t a)
{
    return (((((((s >> 8) & 0xff00ff) - ((d >> 8) & 0xff00ff)) * a) + (d & 0xff00ff00)) & 0xff00ff00) + ((((((s & 0xff00ff) - (d & 0xff00ff)) * a) >> 8) + (d & 0xff00ff)) & 0xff00ff));
}

static inline uint8_t INTERPOLATE8(uint8_t s, uint8_t d, uint8_t a)
{
    return (((s) * (a) + 0xff) >> 8) + (((d) * ~(a) + 0xff) >> 8);
}

static inline int32_t HALF_STROKE(float width)
{
    return TO_SWCOORD(width * 0.5f);
}

static inline uint8_t A(uint32_t c)
{
    return ((c) >> 24);
}

static inline uint8_t IA(uint32_t c)
{
    return (~(c) >> 24);
}

static inline uint8_t C1(uint32_t c)
{
    return ((c) >> 16);
}

static inline uint8_t C2(uint32_t c)
{
    return ((c) >> 8);
}

static inline uint8_t C3(uint32_t c)
{
    return (c);
}

static inline bool UNPREMULTIPLY(uint32_t color, RenderColor& out)
{
    out.a = A(color);
    if (out.a == 0) return false;

    out.r = C1(color) * 255 / out.a;
    out.g = C2(color) * 255 / out.a;
    out.b = C3(color) * 255 / out.a;

    return true;
}

static inline uint32_t opBlendInterp(uint32_t s, uint32_t d, uint8_t a)
{
    return INTERPOLATE(s, d, a);
}

static inline uint32_t opBlendNormal(uint32_t s, uint32_t d, uint8_t a)
{
    auto t = ALPHA_BLEND(s, a);
    return t + ALPHA_BLEND(d, IA(t));
}

static inline uint32_t opBlendPreNormal(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    return s + ALPHA_BLEND(d, IA(s));
}

static inline uint32_t opBlendSrcOver(uint32_t s, TVG_UNUSED uint32_t d, TVG_UNUSED uint8_t a)
{
    return s;
}

//TODO: BlendMethod could remove the alpha parameter.
static inline uint32_t opBlendDifference(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    //if (s > d) => s - d
    //else => d - s
    auto c1 = (C1(s) > C1(d)) ? (C1(s) - C1(d)) : (C1(d) - C1(s));
    auto c2 = (C2(s) > C2(d)) ? (C2(s) - C2(d)) : (C2(d) - C2(s));
    auto c3 = (C3(s) > C3(d)) ? (C3(s) - C3(d)) : (C3(d) - C3(s));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendExclusion(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    // (s + d) - (2 * s * d)
    auto c1 = tvg::clamp(C1(s) + C1(d) - 2 * MULTIPLY(C1(s), C1(d)), 0, 255);
    auto c2 = tvg::clamp(C2(s) + C2(d) - 2 * MULTIPLY(C2(s), C2(d)), 0, 255);
    auto c3 = tvg::clamp(C3(s) + C3(d) - 2 * MULTIPLY(C3(s), C3(d)), 0, 255);
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendAdd(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    // s + d
    auto c1 = std::min(C1(s) + C1(d), 255);
    auto c2 = std::min(C2(s) + C2(d), 255);
    auto c3 = std::min(C3(s) + C3(d), 255);
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendScreen(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    // s + d - s * d
    auto c1 = C1(s) + C1(d) - MULTIPLY(C1(s), C1(d));
    auto c2 = C2(s) + C2(d) - MULTIPLY(C2(s), C2(d));
    auto c3 = C3(s) + C3(d) - MULTIPLY(C3(s), C3(d));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendMultiply(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    RenderColor o;
    if (!UNPREMULTIPLY(d, o)) return 0;

    // s * d
    auto c1 = MULTIPLY(C1(s), o.r);
    auto c2 = MULTIPLY(C2(s), o.g);
    auto c3 = MULTIPLY(C3(s), o.b);
    return JOIN(255, c1, c2, c3);
}


static inline uint32_t opBlendOverlay(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    RenderColor o;
    if (!UNPREMULTIPLY(d, o)) return 0;

    // if (2 * d < da) => 2 * s * d,
    // else => 1 - 2 * (1 - s) * (1 - d)
    auto c1 = (o.r < 128) ? std::min(255, 2 * MULTIPLY(C1(s), o.r)) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - o.r)));
    auto c2 = (o.g < 128) ? std::min(255, 2 * MULTIPLY(C2(s), o.g)) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - o.g)));
    auto c3 = (o.b < 128) ? std::min(255, 2 * MULTIPLY(C3(s), o.b)) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - o.b)));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendDarken(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    RenderColor o;
    if (!UNPREMULTIPLY(d, o)) return 0;

    // min(s, d)
    auto c1 = std::min(C1(s), o.r);
    auto c2 = std::min(C2(s), o.g);
    auto c3 = std::min(C3(s), o.b);
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendLighten(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    // max(s, d)
    auto c1 = std::max(C1(s), C1(d));
    auto c2 = std::max(C2(s), C2(d));
    auto c3 = std::max(C3(s), C3(d));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendColorDodge(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    // d / (1 - s)
    s = 0xffffffff - s;
    auto c1 = C1(d) == 0 ? 0 : (C1(s) == 0 ? 255 : std::min(C1(d) * 255 / C1(s), 255));
    auto c2 = C2(d) == 0 ? 0 : (C2(s) == 0 ? 255 : std::min(C2(d) * 255 / C2(s), 255));
    auto c3 = C3(d) == 0 ? 0 : (C3(s) == 0 ? 255 : std::min(C3(d) * 255 / C3(s), 255));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendColorBurn(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    RenderColor o;
    if (!UNPREMULTIPLY(d, o)) o.r = o.g = o.b = 0;

    // 1 - (1 - d) / s
    auto ir = 255 - o.r;
    auto ig = 255 - o.g;
    auto ib = 255 - o.b;

    auto c1 = o.r == 255 ? 255 : (C1(s) == 0 ? 0 : 255 - std::min(ir * 255 / C1(s), 255));
    auto c2 = o.g == 255 ? 255 : (C2(s) == 0 ? 0 : 255 - std::min(ig * 255 / C2(s), 255));
    auto c3 = o.b == 255 ? 255 : (C3(s) == 0 ? 0 : 255 - std::min(ib * 255 / C3(s), 255));

    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendHardLight(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    RenderColor o;
    if (!UNPREMULTIPLY(d, o)) o.r = o.g = o.b = 0;

    // if (s < sa), (2 * s * d)
    // else (sa * da) - 2 * (da - s) * (sa - d)
    auto c1 = (C1(s) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), o.r)) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - o.r)));
    auto c2 = (C2(s) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), o.g)) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - o.g)));
    auto c3 = (C3(s) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), o.b)) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - o.b)));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendSoftLight(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    if (d == 0) return s;

    RenderColor o;
    if (!UNPREMULTIPLY(d, o)) o.r = o.g = o.b = 0;

    //(255 - 2 * s) * (d * d) + (2 * s * b)
    auto c1 = MULTIPLY(255 - std::min(255, 2 * C1(s)), MULTIPLY(o.r, o.r)) + MULTIPLY(std::min(255, 2 * C1(s)), o.r);
    auto c2 = MULTIPLY(255 - std::min(255, 2 * C2(s)), MULTIPLY(o.g, o.g)) + MULTIPLY(std::min(255, 2 * C2(s)), o.g);
    auto c3 = MULTIPLY(255 - std::min(255, 2 * C3(s)), MULTIPLY(o.b, o.b)) + MULTIPLY(std::min(255, 2 * C3(s)), o.b);
    return JOIN(255, c1, c2, c3);
}


int64_t mathMultiply(int64_t a, int64_t b);
int64_t mathDivide(int64_t a, int64_t b);
int64_t mathMulDiv(int64_t a, int64_t b, int64_t c);
void mathRotate(SwPoint& pt, int64_t angle);
int64_t mathTan(int64_t angle);
int64_t mathAtan(const SwPoint& pt);
int64_t mathCos(int64_t angle);
int64_t mathSin(int64_t angle);
void mathSplitCubic(SwPoint* base);
void mathSplitLine(SwPoint* base);
int64_t mathDiff(int64_t angle1, int64_t angle2);
int64_t mathLength(const SwPoint& pt);
int mathCubicAngle(const SwPoint* base, int64_t& angleIn, int64_t& angleMid, int64_t& angleOut);
int64_t mathMean(int64_t angle1, int64_t angle2);
SwPoint mathTransform(const Point* to, const Matrix& transform);
bool mathUpdateOutlineBBox(const SwOutline* outline, const RenderRegion& clipBox, RenderRegion& renderBox, bool fastTrack);

void shapeReset(SwShape* shape);
bool shapePrepare(SwShape* shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid, bool hasComposite);
bool shapePrepared(const SwShape* shape);
bool shapeGenRle(SwShape* shape, const RenderShape* rshape, bool antiAlias);
void shapeDelOutline(SwShape* shape, SwMpool* mpool, uint32_t tid);
void shapeResetStroke(SwShape* shape, const RenderShape* rshape, const Matrix& transform);
bool shapeGenStrokeRle(SwShape* shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid);
void shapeFree(SwShape* shape);
void shapeDelStroke(SwShape* shape);
bool shapeGenFillColors(SwShape* shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
bool shapeGenStrokeFillColors(SwShape* shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
void shapeResetFill(SwShape* shape);
void shapeResetStrokeFill(SwShape* shape);
void shapeDelFill(SwShape* shape);
void shapeDelStrokeFill(SwShape* shape);

void strokeReset(SwStroke* stroke, const RenderShape* shape, const Matrix& transform);
bool strokeParseOutline(SwStroke* stroke, const SwOutline& outline);
SwOutline* strokeExportOutline(SwStroke* stroke, SwMpool* mpool, unsigned tid);
void strokeFree(SwStroke* stroke);

bool imagePrepare(SwImage* image, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid);
bool imageGenRle(SwImage* image, const RenderRegion& bbox, bool antiAlias);
void imageDelOutline(SwImage* image, SwMpool* mpool, uint32_t tid);
void imageReset(SwImage* image);
void imageFree(SwImage* image);

bool fillGenColorTable(SwFill* fill, const Fill* fdata, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
const Fill::ColorStop* fillFetchSolid(const SwFill* fill, const Fill* fdata);
void fillReset(SwFill* fill);
void fillFree(SwFill* fill);

//OPTIMIZE_ME: Skip the function pointer access
void fillLinear(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, SwMask maskOp, uint8_t opacity);                                   //composite masking ver.
void fillLinear(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwMask maskOp, uint8_t opacity);                     //direct masking ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, uint8_t a);                                         //blending ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, SwBlender op2, uint8_t a);                          //blending + BlendingMethod(op2) ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwAlpha alpha, uint8_t csize, uint8_t opacity);     //matting ver.

void fillRadial(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, SwMask op, uint8_t a);                                             //composite masking ver.
void fillRadial(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwMask op, uint8_t a) ;                              //direct masking ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, uint8_t a);                                         //blending ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, SwBlender op2, uint8_t a);                          //blending + BlendingMethod(op2) ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwAlpha alpha, uint8_t csize, uint8_t opacity);     //matting ver.

SwRle* rleRender(SwRle* rle, const SwOutline* outline, const RenderRegion& bbox, bool antiAlias);
SwRle* rleRender(const RenderRegion* bbox);
void rleFree(SwRle* rle);
void rleReset(SwRle* rle);
void rleMerge(SwRle* rle, SwRle* clip1, SwRle* clip2);
bool rleClip(SwRle* rle, const SwRle* clip);
bool rleClip(SwRle* rle, const RenderRegion* clip);

SwMpool* mpoolInit(uint32_t threads);
bool mpoolTerm(SwMpool* mpool);
bool mpoolClear(SwMpool* mpool);
SwOutline* mpoolReqOutline(SwMpool* mpool, unsigned idx);
void mpoolRetOutline(SwMpool* mpool, unsigned idx);
SwOutline* mpoolReqStrokeOutline(SwMpool* mpool, unsigned idx);
void mpoolRetStrokeOutline(SwMpool* mpool, unsigned idx);
SwOutline* mpoolReqDashOutline(SwMpool* mpool, unsigned idx);
void mpoolRetDashOutline(SwMpool* mpool, unsigned idx);

bool rasterCompositor(SwSurface* surface);
bool rasterShape(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, RenderColor& c);
bool rasterTexmapPolygon(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity);
bool rasterScaledImage(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity);
bool rasterDirectImage(SwSurface* surface, const SwImage& image, const RenderRegion& bbox, uint8_t opacity);
bool rasterScaledRleImage(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity);
bool rasterDirectRleImage(SwSurface* surface, const SwImage& image, const RenderRegion& bbox, uint8_t opacity);
bool rasterStroke(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, RenderColor& c);
bool rasterGradientShape(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, const Fill* fdata, uint8_t opacity);
bool rasterGradientStroke(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, const Fill* fdata, uint8_t opacity);
bool rasterClear(SwSurface* surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, pixel_t val = 0);
void rasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len);
void rasterTranslucentPixel32(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity);
void rasterPixel32(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity);
void rasterGrayscale8(uint8_t *dst, uint8_t val, uint32_t offset, int32_t len);
void rasterXYFlip(uint32_t* src, uint32_t* dst, int32_t stride, int32_t w, int32_t h, const RenderRegion& bbox, bool flipped);
void rasterUnpremultiply(RenderSurface* surface);
void rasterPremultiply(RenderSurface* surface);
bool rasterConvertCS(RenderSurface* surface, ColorSpace to);
uint32_t rasterUnpremultiply(uint32_t data);

bool effectGaussianBlur(SwCompositor* cmp, SwSurface* surface, const RenderEffectGaussianBlur* params);
bool effectGaussianBlurRegion(RenderEffectGaussianBlur* effect);
void effectGaussianBlurUpdate(RenderEffectGaussianBlur* effect, const Matrix& transform);
bool effectDropShadow(SwCompositor* cmp, SwSurface* surfaces[2], const RenderEffectDropShadow* params);
bool effectDropShadowRegion(RenderEffectDropShadow* effect);
void effectDropShadowUpdate(RenderEffectDropShadow* effect, const Matrix& transform);
void effectFillUpdate(RenderEffectFill* effect);
bool effectFill(SwCompositor* cmp, const RenderEffectFill* params, bool direct);
void effectTintUpdate(RenderEffectTint* effect);
bool effectTint(SwCompositor* cmp, const RenderEffectTint* params, bool direct);
void effectTritoneUpdate(RenderEffectTritone* effect);
bool effectTritone(SwCompositor* cmp, const RenderEffectTritone* params, bool direct);

#endif /* _TVG_SW_COMMON_H_ */
