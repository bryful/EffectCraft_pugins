namespace
{

constexpr int MAX_PAIRS = 16;
constexpr int CHANNEL_LEVELS = 256;
constexpr int COLOR_COMPONENTS = 4;
constexpr int PAIR_COMPONENTS = 1 + 2 * COLOR_COMPONENTS;
constexpr int HEADER_COMPONENTS = 1;
constexpr int PARAM_COUNT = HEADER_COMPONENTS + MAX_PAIRS * PAIR_COMPONENTS;
constexpr unsigned PARAM_BYTES = PARAM_COUNT * sizeof(double);
constexpr unsigned PAGE_BYTES = 65536;
constexpr unsigned MEMORY_LIMIT = 1u << 30;

#define COLOR_PAIR(N, ENABLED) \
    ",{\"id\":\"enabled_" #N "\",\"name\":\"Enable Color Pair " #N "\",\"type\":\"checkbox\",\"default\":" #ENABLED "}" \
    ",{\"id\":\"source_" #N "\",\"name\":\"Source Color " #N "\",\"type\":\"color\",\"default\":[0,0,0,1]}" \
    ",{\"id\":\"replacement_" #N "\",\"name\":\"Replacement Color " #N "\",\"type\":\"color\",\"default\":[0,0,0,1]}"

constexpr char MANIFEST[] = R"json({
    "api": 1,
    "id": "org.effectcraft.community.fs-colorchange",
    "name": "Fs-ColorChange",
    "category": "Fs",
    "version": "2.0.0",
    "description": "Replaces up to 16 independently enabled RGB colors.",
    "params": [
        {"id": "tolerance", "name": "Tolerance (%)", "type": "slider", "default": 0, "min": 0, "max": 50, "sliderMax": 20, "decimals": 1}
)json"
        COLOR_PAIR(1, true)   COLOR_PAIR(2, false)  COLOR_PAIR(3, false)  COLOR_PAIR(4, false)
        COLOR_PAIR(5, false)  COLOR_PAIR(6, false)  COLOR_PAIR(7, false)  COLOR_PAIR(8, false)
        COLOR_PAIR(9, false)  COLOR_PAIR(10, false) COLOR_PAIR(11, false) COLOR_PAIR(12, false)
        COLOR_PAIR(13, false) COLOR_PAIR(14, false) COLOR_PAIR(15, false) COLOR_PAIR(16, false)
    "]}";

#undef COLOR_PAIR

struct Rgba
{
    float red, green, blue, alpha;
};

struct Rgb
{
    float red, green, blue;
};

static_assert(sizeof(Rgba) == 16 && sizeof(unsigned) == 4);
extern "C" unsigned char __heap_base;
unsigned frameBytes = 0;

unsigned heapStart()
{
    return (reinterpret_cast<unsigned>(&__heap_base) + 7) & ~7u;
}

double clamp(double value, double upper)
{
    if (!(value > 0.0))
    {
        return 0.0;
    }
    return value < upper ? value : upper;
}

int quantize8(double value)
{
    return static_cast<int>(clamp(value, 1.0) * 255.0 + 0.5);
}

void addRange(unsigned (&table)[CHANNEL_LEVELS + 1], int center, int tolerance, unsigned bit)
{
    const int lower = center - tolerance;
    const int upper = center + tolerance + 1;
    table[lower > 0 ? lower : 0] ^= bit;
    table[upper < CHANNEL_LEVELS ? upper : CHANNEL_LEVELS] ^= bit;
}

} // namespace

extern "C" int ec_api_version()
{
    return 1;
}

extern "C" const char* ec_manifest_ptr()
{
    return MANIFEST;
}

extern "C" int ec_manifest_len()
{
    return sizeof(MANIFEST) - 1;
}

extern "C" int ec_alloc(int bytes)
{
    frameBytes = 0;
    const unsigned start = heapStart();
    if (bytes < static_cast<int>(PARAM_BYTES) ||
        static_cast<unsigned>(bytes) > MEMORY_LIMIT - start ||
        (bytes - PARAM_BYTES) % sizeof(Rgba) != 0)
    {
        return 0;
    }

    const unsigned needed = (start + bytes + PAGE_BYTES - 1) / PAGE_BYTES;
    const unsigned current = __builtin_wasm_memory_size(0);
    if (needed > current && __builtin_wasm_memory_grow(0, needed - current) == -1u)
    {
        return 0;
    }
    frameBytes = bytes;
    return start;
}

extern "C" int ec_render(unsigned pixels, int width, int height, unsigned params,
                         int nparams, double, double)
{
    if (width < 0 || height < 0 || nparams != PARAM_COUNT || params != heapStart() ||
        pixels != params + PARAM_BYTES || frameBytes < PARAM_BYTES)
    {
        return 1;
    }
    const auto count = static_cast<unsigned long long>(width) * height;
    if (count != (frameBytes - PARAM_BYTES) / sizeof(Rgba))
    {
        return 1;
    }
    if (count == 0)
    {
        return 0;
    }

    const auto* settings = reinterpret_cast<const double*>(params);
    const int tolerance = static_cast<int>(clamp(settings[0], 50.0) * 255.0 / 100.0);
    unsigned red[CHANNEL_LEVELS + 1]{};
    unsigned green[CHANNEL_LEVELS + 1]{};
    unsigned blue[CHANNEL_LEVELS + 1]{};
    Rgb replacements[MAX_PAIRS];
    bool anyEnabled = false;
    for (int i = 0; i < MAX_PAIRS; ++i)
    {
        const double* pair = settings + HEADER_COMPONENTS + i * PAIR_COMPONENTS;
        if (pair[0] == 0.0)
        {
            continue;
        }

        anyEnabled = true;
        const double* source = pair + 1;
        const double* replacement = source + COLOR_COMPONENTS;
        const unsigned bit = 1u << i;
        addRange(red, quantize8(source[0]), tolerance, bit);
        addRange(green, quantize8(source[1]), tolerance, bit);
        addRange(blue, quantize8(source[2]), tolerance, bit);
        replacements[i] = {
            static_cast<float>(clamp(replacement[0], 1.0)),
            static_cast<float>(clamp(replacement[1], 1.0)),
            static_cast<float>(clamp(replacement[2], 1.0)),
        };
    }
    if (!anyEnabled)
    {
        return 0;
    }

    // 区間の始点と終点でビットを反転し、各チャンネルの一致表を作る。
    for (int i = 1; i < CHANNEL_LEVELS; ++i)
    {
        red[i] ^= red[i - 1];
        green[i] ^= green[i - 1];
        blue[i] ^= blue[i - 1];
    }

    auto* frame = reinterpret_cast<Rgba*>(pixels);
    for (unsigned i = 0; i < count; ++i)
    {
        auto& pixel = frame[i];
        if (!(pixel.alpha > 0.0f))
        {
            continue;
        }

        const double inverseAlpha = 1.0 / pixel.alpha;
        const unsigned matches = red[quantize8(pixel.red * inverseAlpha)] &
                                 green[quantize8(pixel.green * inverseAlpha)] &
                                 blue[quantize8(pixel.blue * inverseAlpha)];
        if (matches != 0)
        {
            // 複数の色に一致する場合は、先に指定された組を採用する。
            const auto& replacement = replacements[__builtin_ctz(matches)];
            pixel.red = replacement.red * pixel.alpha;
            pixel.green = replacement.green * pixel.alpha;
            pixel.blue = replacement.blue * pixel.alpha;
        }
    }
    return 0;
}
