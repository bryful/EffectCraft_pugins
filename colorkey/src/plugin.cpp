namespace {

constexpr char manifest[] = R"json({
  "api": 1,
  "id": "org.effectcraft.community.fs-colorkey",
  "name": "Fs-ColorKey",
  "category": "Fs",
  "version": "1.0.0",
  "description": "Makes pixels transparent based on their RGB match to the target color.",
  "params": [
    {"id": "target_color", "name": "TargetColor", "type": "color", "default": [1.0, 1.0, 1.0, 1.0]},
    {"id": "level", "name": "level(%)", "type": "slider", "default": 0, "min": 0, "max": 50, "sliderMax": 20, "decimals": 1},
    {"id": "reverse", "name": "Reverse", "type": "checkbox", "default": false}
  ]
})json";

struct Rgba {
    float red, green, blue, alpha;
};

static_assert(sizeof(Rgba) == 16);
constexpr int param_count = 6; // Four color channels, level, reverse.
constexpr unsigned param_bytes = param_count * sizeof(double);
constexpr unsigned page_bytes = 65536;
constexpr unsigned memory_limit = 1u << 30;

extern "C" unsigned char __heap_base;
unsigned frame_bytes = 0;

unsigned heap_start()
{
    return (reinterpret_cast<unsigned>(&__heap_base) + 7) & ~7u;
}

double clamp(double value, double upper)
{
    if (!(value > 0.0)) {
        return 0.0;
    }
    return value < upper ? value : upper;
}

int quantize8(double value)
{
    return static_cast<int>(clamp(value, 1.0) * 255.0 + 0.5);
}

bool within_level(int value, int target, int tolerance)
{
    const int delta = value - target;
    return delta >= -tolerance && delta <= tolerance;
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
    const unsigned start = heap_start();
    if (bytes < static_cast<int>(param_bytes) ||
        static_cast<unsigned>(bytes) > memory_limit - start ||
        (bytes - param_bytes) % sizeof(Rgba) != 0) {
        return 0;
    }
    const unsigned needed = (start + bytes + page_bytes - 1) / page_bytes;
    const unsigned current = __builtin_wasm_memory_size(0);
    if (needed > current && __builtin_wasm_memory_grow(0, needed - current) == -1u) {
        return 0;
    }
    frame_bytes = bytes;
    return start;
}

extern "C" int ec_render(unsigned pixels, int width, int height, unsigned params,
                         int nparams, double, double)
{
    if (width < 0 || height < 0 || nparams != param_count || params != heap_start() ||
        pixels != params + param_bytes || frame_bytes < param_bytes) {
        return 1;
    }
    const auto count = static_cast<unsigned long long>(width) * height;
    if (count != (frame_bytes - param_bytes) / sizeof(Rgba)) {
        return 1;
    }

    const auto* settings = reinterpret_cast<const double*>(params);
    const int red = quantize8(settings[0]);
    const int green = quantize8(settings[1]);
    const int blue = quantize8(settings[2]);
    const int tolerance = static_cast<int>(clamp(settings[4], 50.0) * 255.0 / 100.0);
    const bool reverse = settings[5] != 0.0;

    auto* frame = reinterpret_cast<Rgba*>(pixels);
    for (unsigned i = 0; i < count; ++i) {
        auto& pixel = frame[i];
        if (pixel.alpha <= 0.0f) {
            continue;
        }
        // Compare straight RGB. Keep the original premultiplied values on surviving pixels.
        const double alpha = pixel.alpha;
        const bool match = within_level(quantize8(pixel.red / alpha), red, tolerance) &&
                           within_level(quantize8(pixel.green / alpha), green, tolerance) &&
                           within_level(quantize8(pixel.blue / alpha), blue, tolerance);
        if (match != reverse) {
            pixel = {};
        }
    }
    return 0;
}
