#pragma once

#include "util.h"

struct Rect {
    int left, top, right, bottom;
};

void smooth_pixels(ImageSize size, PixelBuffer<Pixel16> input, PixelBuffer<Pixel16> output,
                   Rect bounds, unsigned int range, float line_weight);
