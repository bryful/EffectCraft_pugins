#pragma once

#include "util.h"

struct Rect {
    int left, top, right, bottom;
};

void smooth_pixels(ImageSize size, PixelBuffer<Pixel8> input, PixelBuffer<Pixel8> output,
                   Rect bounds, unsigned int range, float line_weight);
