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

#include "tvgCanvas.h"
#include "tvgTaskScheduler.h"
#include "tvgLoadModule.h"

#ifdef THORVG_SW_RASTER_SUPPORT
    #include "sw_engine/tvgSwRenderer.h"
#endif


SwCanvas::SwCanvas() = default;

SwCanvas::~SwCanvas()
{
#ifdef THORVG_SW_RASTER_SUPPORT
    SwRenderer::term();
#endif
}


SwCanvas* SwCanvas::gen() noexcept
{
#ifdef THORVG_SW_RASTER_SUPPORT
    if (engineInit > 0) {
        auto renderer = SwRenderer::gen(TaskScheduler::threads());
        renderer->ref();
        auto ret = new SwCanvas;
        ret->pImpl->renderer = renderer;
        return ret;
    }
#endif
    return nullptr;
}
