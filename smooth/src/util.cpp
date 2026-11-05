// Adapted from loilo-inc/smooth (Apache-2.0) for the EffectCraft plug-in API.
#include "util.h"

template<typename PixelType>
void BlendLine(     BlendingInfo<PixelType>    *pinfo,                 //
                    double          length,                 // このパターンの長さ
                    long            blend_target,           // ブレンド元のターゲット(input)
                    long            out_target,             // ブレンド先のターゲット(output)
                    int             ref_offset,             // ブレンド参照先のターゲット(input)
                    int             next_pixel_step_in,     // 次のピクセルへ移動するときこの値を足す(input)
                    int             next_pixel_step_out,    // 次のピクセルへ移動するときこの値を足す(output)
                    bool            ratio_invert,
                    bool            no_line_weight)
{
    double  len;
    int     blend_count;            // ブレンドするピクセルの数 切り上げ
    double  pre_ratio       = 0.0;

    if( no_line_weight )
        len = (length * 0.5 ); // 全体の底辺
    else
        len = (length * (double)pinfo->LineWeight ); // 全体の底辺

    blend_count = CEIL(len);


    // 境界の逆方向からブレンドを行うので、その分移動 //
    blend_target    += (blend_count-1) * next_pixel_step_in;
    out_target      += (blend_count-1) * next_pixel_step_out;


    // 全体の～ : ブレンド全体の三角形、 なし :そのピクセルの三角形
    // 底辺×高さ÷２ = 底辺×((底辺/全体の底辺)×全体の高さ)÷２
    //                          ↑相似な三角形なので
    // = l(底辺) * l(底辺) * 0.5(全体の高さ) * 0.5(÷2) / len(全体の底辺)
    int t;
    for(t=0;t<blend_count;t++)
    {
        double  l           = len - (float)(((int)CEIL(len)-1) - t);    // このピクセルでの底辺 CEIL(len)-1 は (1.000...1 ～ 2.0) -> 1.0としたいため
        double  ratio       = (l * l * 0.5 * 0.5) / len;
        double  r;

        r = ratio_invert ? 1.0 - (ratio - pre_ratio) : (ratio - pre_ratio);

        // ブレンド
        Blendingf(pinfo->in_ptr, pinfo->out_ptr, blend_target, blend_target+ref_offset, out_target, (float)r);

        pre_ratio = ratio;

        // 次のピクセルへ
        blend_target    -= next_pixel_step_in;
        out_target      -= next_pixel_step_out;
    }
}




// 明示的インスタンス化宣言
template void BlendLine<Pixel8>(BlendingInfo<Pixel8>    *pinfo,
								   double          length,
								   long            blend_target,
								   long            out_target,
								   int             ref_offset,
								   int             next_pixel_step_in,
								   int             next_pixel_step_out,
								   bool            ratio_invert,
								   bool            no_line_weight);

template void BlendLine<Pixel16>(BlendingInfo<Pixel16>    *pinfo,
								    double          length,
								    long            blend_target,
								    long            out_target,
								    int             ref_offset,
								    int             next_pixel_step_in,
								    int             next_pixel_step_out,
								    bool            ratio_invert,
								    bool            no_line_weight);
