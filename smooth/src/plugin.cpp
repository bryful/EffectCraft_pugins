#include "smoothing.h"

namespace {

constexpr char manifest[] = R"json({
  "api": 1,
  "id": "org.effectcraft.community.smooth",
  "name": "Smooth",
  "category": "Stylize",
  "version": "0.1.0",
  "description": "Smooths raster line art and flat-color edges.",
  "params": [
    {"id": "white_option", "name": "White Transparent", "type": "checkbox", "default": false},
    {"id": "range", "name": "Range", "type": "slider", "default": 1, "min": 0, "max": 100, "sliderMin": 0, "sliderMax": 10, "decimals": 1},
    {"id": "line_weight", "name": "Line Weight", "type": "slider", "default": 0, "min": 0, "max": 1, "decimals": 2}
  ]
})json";

struct Rgba {
    float red, green, blue, alpha;
};

static_assert(sizeof(Pixel8) == 4 && sizeof(Rgba) == 16);

constexpr int param_count = 3;
constexpr unsigned int param_bytes = param_count * sizeof(double);
constexpr unsigned int frame_pixel_bytes = sizeof(Rgba) + 2 * sizeof(Pixel8);
constexpr unsigned int page_bytes = 65536;
constexpr unsigned int memory_limit = 1u << 30;

extern "C" unsigned char __heap_base;
unsigned int frame_bytes = 0;

unsigned int heap_start()
{
    return (reinterpret_cast<unsigned int>(&__heap_base) + 7) & ~7u;
}

bool reserve(unsigned int bytes)
{
    const unsigned int needed = (bytes + page_bytes - 1) / page_bytes;
    const unsigned int current = __builtin_wasm_memory_size(0);
    return needed <= current || __builtin_wasm_memory_grow(0, needed - current) != -1u;
}

double clamp(double value, double upper)
{
    return value > 0.0 ? MIN(value, upper) : 0.0;
}

unsigned char quantize(float value)
{
    return static_cast<unsigned char>(clamp(value, 1.0) * 255.0 + 0.5);
}

bool is_white(Rgba pixel)
{
    return pixel.alpha > 0.0f && pixel.red == pixel.alpha &&
           pixel.green == pixel.alpha && pixel.blue == pixel.alpha;
}

Pixel8 to_pixel8(Rgba pixel)
{
    const auto alpha = quantize(pixel.alpha);
    if (alpha == 0) {
        return {};
    }
    return {alpha, quantize(pixel.red / pixel.alpha),
            quantize(pixel.green / pixel.alpha), quantize(pixel.blue / pixel.alpha)};
}

Rgba to_rgba(Pixel8 pixel)
{
    const float alpha = pixel.alpha / 255.0f;
    const float factor = alpha / 255.0f;
    return {pixel.red * factor, pixel.green * factor, pixel.blue * factor, alpha};
}

} // namespace

extern "C" int ec_api_version()
{
    return 1;
}

extern "C" const char* ec_manifest_ptr()
{
    return manifest;
}

extern "C" int ec_manifest_len()
{
    return sizeof(manifest) - 1;
}

extern "C" int ec_alloc(int bytes)
{
    frame_bytes = 0;
    if (bytes < static_cast<int>(param_bytes)) {
        return 0;
    }
    const auto count = (bytes - param_bytes) / sizeof(Rgba);
    const auto start = heap_start();
    if (count > (memory_limit - start - param_bytes) / frame_pixel_bytes ||
        bytes != static_cast<int>(param_bytes + count * sizeof(Rgba)) ||
        !reserve(start + param_bytes + count * frame_pixel_bytes)) {
        return 0;
    }
    frame_bytes = bytes;
    return start;
}

extern "C" int ec_render(unsigned int pixels, int width, int height, unsigned int params,
                         int nparams, double, double)
{
    if (width < 0 || height < 0 || nparams != param_count ||
        params != heap_start() || pixels != params + param_bytes || frame_bytes < param_bytes) {
        return 1;
    }
    const auto count = static_cast<unsigned long long>(width) * height;
    if (count != (frame_bytes - param_bytes) / sizeof(Rgba)) {
        return 1;
    }
    if (count == 0) {
        return 0;
    }

    const auto* settings = reinterpret_cast<const double*>(params);
    const bool white_transparent = settings[0] != 0.0;
    const auto range = static_cast<unsigned int>(clamp(settings[1], 100.0) * 1020) / 100;
    const auto line_weight = static_cast<float>(clamp(settings[2], 1.0) / 2.0 + 0.5);
    auto* frame = reinterpret_cast<Rgba*>(pixels);
    auto* input = reinterpret_cast<Pixel8*>(params + frame_bytes);
    auto* output = input + count;

    Rect bounds{width, height, 0, 0};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            const auto pixel = frame[index];
            const bool white = is_white(pixel);
            input[index] = white_transparent && white ? Pixel8{} : to_pixel8(pixel);
            output[index] = input[index];
            if (!white && input[index].alpha != 0) {
                bounds.left = MIN(bounds.left, x);
                bounds.top = MIN(bounds.top, y);
                bounds.right = MAX(bounds.right, x + 1);
                bounds.bottom = y + 1;
            }
        }
    }
    if (bounds.right > bounds.left && bounds.bottom > bounds.top && width >= 3 && height >= 3) {
        Pixel8 transparent{};
        Pixel8 sink{};
        const auto size = static_cast<unsigned int>(count);
        smooth_pixels({width, height}, {input, size, &transparent}, {output, size, &sink},
                      bounds, range, line_weight);
    }
    for (unsigned int i = 0; i < count; ++i) {
        if (output[i] != input[i] || (white_transparent && is_white(frame[i]))) {
            frame[i] = to_rgba(output[i]);
        }
    }
    return 0;
}
