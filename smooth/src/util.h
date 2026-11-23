// Adapted from loilo-inc/smooth (Apache-2.0) for the EffectCraft plug-in API.
#pragma once

struct Pixel8 {
    unsigned char alpha, red, green, blue;

    bool operator==(const Pixel8&) const = default;
};

struct Pixel16 {
    unsigned short alpha, red, green, blue;

    bool operator==(const Pixel16&) const = default;
};

struct ImageSize {
    int width, height;
};

template <typename PixelType>
struct PixelBuffer {
    PixelType* data;
    unsigned int size;
    PixelType* outside;

    PixelType& operator[](long index) const {
        return static_cast<unsigned long>(index) < size ? data[index] : *outside;
    }
};

#include "define.h"

#define GET_WIDTH(image) ((image)->width)
#define GET_HEIGHT(image) ((image)->height)
#define CEIL(value) static_cast<int>(__builtin_ceil(value))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ABS(a) ((a) < 0 ? -(a) : (a))
#define GET_SIGN(a) ((a) / ABS(a))
#define DEBUG_PIXEL(out_ptr, output, x, y) ((void)0)
#define DEBUG_TARGET_PIXEL(out_ptr, output, target) ((void)0)

template <typename PixelType> unsigned int getMaxValue();
template <> inline unsigned int getMaxValue<Pixel8>() { return 255; }
template <> inline unsigned int getMaxValue<Pixel16>() { return 32768; }

#define ComparePixel(a, b) RangeComparePixelNotEqual(&info->in_ptr[a], &info->in_ptr[b], info->range)
#define ComparePixelEqual(a, b) RangeComparePixelEqual(&info->in_ptr[a], &info->in_ptr[b], info->range)

template <typename PixelType>
static inline bool RangeComparePixelNotEqual( const PixelType *p0, const PixelType *p1, const unsigned int range )
{
    unsigned int delta;
    delta = ABS(p0->red   - p1->red   ) +
            ABS(p0->green - p1->green ) +
            ABS(p0->blue  - p1->blue  ) +
            ABS(p0->alpha - p1->alpha );

    return (delta > range);
}


template <typename PixelType>
static inline bool RangeComparePixelEqual( const PixelType *p0, const PixelType *p1, const unsigned int range )
{
    unsigned int delta;
    delta = ABS(p0->red   - p1->red   ) +
            ABS(p0->green - p1->green ) +
            ABS(p0->blue  - p1->blue  ) +
            ABS(p0->alpha - p1->alpha );

    return (delta <= range);
}
// グラデーションを検知
// 3つのピクセルの平均値とCenterのピクセルを比較して、range以下だったらtrue。
// ref1,とcenterが別の色なのが前提
template <typename PixelType>
static inline bool DetectGradation( const PixelType *center,
									const PixelType *ref1,
									const PixelType *ref2,
									const unsigned int range )
{
	// 平均ピクセルを作る
	PixelType	ave;
	ave.red		= (center->red + ref1->red + ref2->red)/3;
	ave.blue	= (center->blue + ref1->blue + ref2->blue)/3;
	ave.green	= (center->green + ref1->green + ref2->green)/3;
	ave.alpha	= (center->alpha + ref1->alpha + ref2->alpha)/3;

	// centerと平均が同じで、そのほか２つとは別の色
    return RangeComparePixelEqual( center, &ave, 1 * 255 * 4 );
}


//---------------------------------------------------------------------------------------//
//概要:     ピクセル同士をブレンドする
//引数:     input,output:   おなじみ入出力画像へのポインタ
//          blend_target:   ブレンドするターゲット。結果はここのピクセルに入る
//          ref_target:     ブレンドするターゲット２。ここと上のやつのブレンドになる
//          ratio:          ブレンドする割合。1.0fでblend_targetのまんまになる 0.0fでref
//---------------------------------------------------------------------------------------//
template <typename PixelType>
static inline void BlendingPixelf(  PixelType *target_pixel,
                                    PixelType *ref_pixel,
                                    PixelType *output_pixel,
                                    float   ratio )
{
    unsigned int  max_value = getMaxValue<PixelType>();
    ratio = MAX(0.0f, MIN(1.0f, ratio));
    unsigned int alpha = (unsigned int)(max_value * ratio), r_alpha;

    r_alpha = max_value - alpha;

    if(target_pixel->alpha == max_value && ref_pixel->alpha == max_value )
    {
		// どちらも不透明
        output_pixel->alpha     = max_value;

        output_pixel->red       = (((target_pixel->red * alpha)+
                                    (ref_pixel->red * r_alpha))/max_value);
        output_pixel->green     = (((target_pixel->green * alpha)+
                                    (ref_pixel->green * r_alpha))/max_value);
        output_pixel->blue      = (((target_pixel->blue * alpha)+
                                    (ref_pixel->blue * r_alpha))/max_value);
    }
    else if(target_pixel->alpha == 0 )
    {
		// ターゲットが抜き
        output_pixel->alpha = (((target_pixel->alpha * alpha)+
                                        (ref_pixel->alpha * r_alpha))/max_value);

        output_pixel->red       = ref_pixel->red;
        output_pixel->green     = ref_pixel->green;
        output_pixel->blue      = ref_pixel->blue;

    }
    else if(ref_pixel->alpha == 0 )
    {
		// refが抜き
        output_pixel->alpha = (((target_pixel->alpha * alpha)+
                                        (ref_pixel->alpha * r_alpha))/max_value);

        output_pixel->red       = target_pixel->red;
        output_pixel->green     = target_pixel->green;
        output_pixel->blue      = target_pixel->blue;
    }
	else
	{
		// 半透明
        output_pixel->alpha = (((target_pixel->alpha * alpha)+
                                        (ref_pixel->alpha * r_alpha))/max_value);
        output_pixel->red       = (((target_pixel->red * alpha)+
                                    (ref_pixel->red * r_alpha))/max_value);
        output_pixel->green     = (((target_pixel->green * alpha)+
                                    (ref_pixel->green * r_alpha))/max_value);
        output_pixel->blue      = (((target_pixel->blue * alpha)+
                                    (ref_pixel->blue * r_alpha))/max_value);
	}
}




// float版のブレンディング命令 //
template <typename PixelType>
static inline void Blendingf(   const PixelBuffer<PixelType>& in_ptr,
                                const PixelBuffer<PixelType>& out_ptr,
                                long        blend_target,
                                long        ref_target,
                                long        output_target,
                                float       ratio )
{
    BlendingPixelf<PixelType>(	&(in_ptr[blend_target]),
								&(in_ptr[ref_target]),
								&(out_ptr[output_target]),
								ratio );
}


template<typename PixelType>
void BlendLine(     BlendingInfo<PixelType>    *pinfo,                 //
                    double          length,                 // このパターンの長さ
                    long            blend_target,           // ブレンド元のターゲット(input)
                    long            out_target,             // ブレンド先のターゲット(output)
                    int             ref_offset,             // ブレンド参照先のターゲット(input)
                    int             next_pixel_step_in,     // 次のピクセルへ移動するときこの値を足す(input)
                    int             next_pixel_step_out,    // 次のピクセルへ移動するときこの値を足す(output)
                    bool            ratio_invert,
                    bool            no_line_weight);
