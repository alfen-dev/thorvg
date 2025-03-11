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

#include <fstream>
#include <string.h>
#include "tvgLoader.h"
#include "tvgRawLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

RawLoader::RawLoader() : ImageLoader(FileType::Raw)
{
}


RawLoader::~RawLoader()
{
    if (copy) free(surface.pixel_buffer);
}


bool RawLoader::open(const PIXEL_TYPE* data, uint32_t w, uint32_t h, ColorSpace cs, bool copy)
{
    if (!LoadModule::read()) return true;

    if (!data || w == 0 || h == 0) return false;

    this->w = (float)w;
    this->h = (float)h;
    this->copy = copy;

    if (copy) {
        surface.pixel_buffer = (PIXEL_TYPE*)malloc(sizeof(PIXEL_TYPE) * w * h);
        if (!surface.pixel_buffer) return false;
        memcpy((void*)surface.pixel_buffer, data, sizeof(PIXEL_TYPE) * w * h);
    }
    else surface.pixel_buffer = const_cast<PIXEL_TYPE*>(data);

    //setup the surface
    surface.stride_pixels = w;
    surface.w = w;
    surface.h = h;
    surface.cs = cs;
    surface.channelSize = sizeof(PIXEL_TYPE);
    surface.premultiplied = true;

    return true;
}


bool RawLoader::read()
{
    LoadModule::read();

    return true;
}
