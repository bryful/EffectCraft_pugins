// Adapted from loilo-inc/smooth (Apache-2.0) for the EffectCraft plug-in API.
#include "smoothing.h"
#include "upMode.h"
#include "downMode.h"
#include "8link.h"
#include "Lack.h"

void smooth_pixels(ImageSize size, PixelBuffer<Pixel8> in_ptr, PixelBuffer<Pixel8> out_ptr,
                   Rect extent_hint, unsigned int range, float line_weight)
{
    using PixelType = Pixel8;
    auto* input = &size;
    auto* output = &size;
    int     in_width,in_height, out_width, out_height, i,j;
    long    in_target, out_target;
    float weight;
    bool        lack_flg;

    in_width    = GET_WIDTH(input);
    in_height   = GET_HEIGHT(input);
    out_width   = GET_WIDTH(output);
    out_height  = GET_HEIGHT(output);


    BlendingInfo<PixelType>    blend_info, *info;

    info = &blend_info;

    // 共通部分を初期化
    blend_info.input        = input;
    blend_info.output       = output;
    blend_info.in_ptr       = in_ptr;
    blend_info.out_ptr      = out_ptr;
    blend_info.range        = range;
    blend_info.LineWeight   = line_weight;

	// 領域情報を加工
	if( extent_hint.top == 0 )			extent_hint.top = 1;
	if( extent_hint.left == 0 )			extent_hint.left = 1;
	if( extent_hint.right == in_width )	extent_hint.right -= 1;
	if( extent_hint.bottom == in_height )	extent_hint.bottom -= 1;


    ///////////////// アンチ処理 //////////////////////////////////////////////////
    for( j=extent_hint.top; j<extent_hint.bottom; j++)
    {
        lack_flg = false;

        in_target   = j*in_width+extent_hint.left;
        out_target  = j*out_width+extent_hint.left;

        for( i=extent_hint.left; i<extent_hint.right; i++, in_target++, out_target++)
        {
            // 欠けである可能性あり
            if( lack_flg )
            {
                // フラグ落とす
                lack_flg = false;

                // 設定
                blend_info.i            = i;
                blend_info.j            = j;
                blend_info.in_target    = in_target;
                blend_info.out_target   = out_target;
                blend_info.flag         = 0;

                // 処理
                LackMode0304Execute( &blend_info );
            }

            // 可能性のある角のみに限定する //
			if( in_ptr[in_target] != in_ptr[in_target + 1] )
            {
                unsigned char   mode_flg = 0;

                // 初期化 //

                blend_info.i            = i;
                blend_info.j            = j;
                blend_info.in_target    = in_target;
                blend_info.out_target   = out_target;
                blend_info.flag         = 0;

                for (auto& direction : blend_info.core) {
                    direction = {};
                }

                // ここでアンチを処理 //
                if( ComparePixel(in_target, in_target+1))           (mode_flg |= 1<<0);
                if( ComparePixel(in_target, in_target-in_width))    (mode_flg |= 1<<1);
                if( ComparePixel(in_target, in_target+in_width))    (mode_flg |= 1<<2);
                if( ComparePixel(in_target, in_target-1))           (mode_flg |= 1<<3);


                if( mode_flg != 0 )
                {
                    // 次のピクセルがlackである可能性あり
                    if( i < input->width-2 && (mode_flg & 1<<0))
					{
                        lack_flg = true;
					}

                    switch(mode_flg)
                    {

                        case 3: ////// 上向きの角 /////////////////////////////


                            // 8連結系モードとのバッティングを避ける
                            if( ComparePixelEqual(in_target-in_width,   in_target+1) &&     // 対角が同じで
                                ComparePixel     (in_target-in_width+1, in_target-in_width) &&  // 対角の角がそれぞれ違ういろ
                                ComparePixel     (in_target-in_width+1, in_target+1))
                            {
                                // 処理しない
                                break;
                            }


                            // カウント //
                            upMode_LeftCountLength<PixelType>( &blend_info);

                            upMode_RightCountLength<PixelType>( &blend_info);

                            upMode_TopCountLength<PixelType>( &blend_info);

                            upMode_BottomCountLength<PixelType>( &blend_info);


                            ///// 補正処理 //////////

                            /////// 水平方向 /////////////////////////
                            // start座標 (leftが長い場合しか行わない) //
                            if(blend_info.core[0].length - blend_info.core[1].length == 1)
                            {
                                blend_info.core[0].start -= 0.5f;
                                blend_info.core[1].start -= 0.5f;
                            }

                            if( (blend_info.core[0].flg & CR_FLG_FILL ) ||
                                (blend_info.core[1].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }

                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[0].end = blend_info.core[0].start - (blend_info.core[0].start - blend_info.core[0].end) * weight;
                            blend_info.core[1].end = blend_info.core[1].start + (blend_info.core[1].end   - blend_info.core[1].start) * weight;


                            /////// 垂直方向 /////////////////////////
                            // start座標 (bottomが長い場合しか行わない) //
                            if(blend_info.core[3].length - blend_info.core[2].length == 1)
                            {
                                blend_info.core[2].start += 0.5f;
                                blend_info.core[3].start += 0.5f;
                            }

                            if( (blend_info.core[2].flg & CR_FLG_FILL ) ||
                                (blend_info.core[3].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }

                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[2].end = blend_info.core[2].start - (blend_info.core[2].start - blend_info.core[2].end) * weight;
                            blend_info.core[3].end = blend_info.core[3].start + (blend_info.core[3].end   - blend_info.core[3].start) * weight;


                            if( blend_info.core[0].length >= 2 && // left >= 2 && bottom >= 2
                                blend_info.core[3].length >= 2)
                            {
                                // 欠けモード2
                                LackMode02Execute( &blend_info );
                            }
                            else if(blend_info.core[1].length > 0)  // Left > 0 && Right > 0
                            {   ////////// 水平方向のブレンド /////////////

                                blend_info.mode = BLEND_MODE_UP_H;  // モード設定

                                // ブレンド //
                                upMode_LeftBlending<PixelType>( &blend_info);

                                upMode_RightBlending<PixelType>( &blend_info);

                                if(blend_info.core[2].length > 1)
                                {   //// 同時に垂直方向も存在 //////
                                    upMode_TopBlending<PixelType>( &blend_info);

                                    upMode_BottomBlending<PixelType>( &blend_info);
                                }

                            }
                            else if(blend_info.core[2].length > 0)  // bottom > 0 && top > 0
                            {   //////////// 垂直方向のブレンド /////////////

                                blend_info.mode = BLEND_MODE_UP_V;  // モード設定


                                upMode_TopBlending<PixelType>( &blend_info);

                                upMode_BottomBlending<PixelType>( &blend_info);
                            }

                            break;




                        case 5: //// 下向きの角 ///////////////////////////////////
                            // 8連結系モードとのバッティングを避ける
                            if( ComparePixelEqual(in_target+in_width,   in_target+1) &&     // 対角が同じで
                                ComparePixel     (in_target+in_width+1, in_target+in_width) &&  // 対角の角がそれぞれ違ういろ
                                ComparePixel     (in_target+in_width+1, in_target+1))
                            {
                                // 処理しない
                                break;
                            }


                            ////// カウント //////////
                            downMode_LeftCountLength<PixelType>( &blend_info);

                            downMode_RightCountLength<PixelType>( &blend_info);

                            downMode_TopCountLength<PixelType>( &blend_info);

                            downMode_BottomCountLength<PixelType>( &blend_info);



                            ///// 補正処理 //////////
                            /////// 水平方向 /////////////////////////
                            // start座標 (leftが長い場合しか行わない) //
                            if(blend_info.core[0].length - blend_info.core[1].length == 1)
                            {
                                blend_info.core[0].start -= 0.5f;
                                blend_info.core[1].start -= 0.5f;
                            }

                            if( (blend_info.core[0].flg & CR_FLG_FILL ) ||
                                (blend_info.core[1].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }

                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[0].end = blend_info.core[0].start - (blend_info.core[0].start - blend_info.core[0].end)  * weight;
                            blend_info.core[1].end = blend_info.core[1].start + (blend_info.core[1].end   - blend_info.core[1].start)  * weight;


                            /////// 垂直方向 /////////////////////////
                            // start座標 (bottomが長い場合しか行わない) //
                            if(blend_info.core[3].length - blend_info.core[2].length == 1)
                            {
                                blend_info.core[2].start += 0.5f;
                                blend_info.core[3].start += 0.5f;
                            }

                            if( (blend_info.core[2].flg & CR_FLG_FILL ) ||
                                (blend_info.core[3].flg & CR_FLG_FILL ) )
                            {
                                weight = 0.5f;
                            }
                            else
                            {
                                weight = blend_info.LineWeight;
                            }

                            // end を計算しなおす(Countが出力するend値はLenと同じで0.5がかけられていない)(補正時の再計算で沸けわからなくなるため) //
                            blend_info.core[2].end = blend_info.core[2].start - (blend_info.core[2].start - blend_info.core[2].end)  * weight;
                            blend_info.core[3].end = blend_info.core[3].start + (blend_info.core[3].end   - blend_info.core[3].start)  * weight;


                            ////// ブレンド ///////
                            if( blend_info.core[0].length >= 2 && // left >= 2 && top >= 2
                                blend_info.core[2].length >= 2)
                            {
                                // 欠けモード1
                                LackMode01Execute( &blend_info );
                            }
                            else if(blend_info.core[1].length > 0)  // Left > 0 && Right > 0
                            {   ////////// 水平方向のブレンド /////////////

                                blend_info.mode = BLEND_MODE_UP_H;  // モード設定

                                // ブレンド //
                                downMode_LeftBlending<PixelType>( &blend_info);

                                downMode_RightBlending<PixelType>( &blend_info);

                                if(blend_info.core[3].length > 1)
                                {   //// 同時に垂直方向も存在 //////
                                    downMode_TopBlending<PixelType>( &blend_info);

                                    downMode_BottomBlending<PixelType>( &blend_info);
                                }

                            }
                            else if(blend_info.core[3].length > 0)  // bottom > 0 && top > 0
                            {   //////////// 垂直方向のブレンド /////////////

                                blend_info.mode = BLEND_MODE_UP_V;  // モード設定


                                downMode_TopBlending<PixelType>( &blend_info);

                                downMode_BottomBlending<PixelType>( &blend_info);
                            }

                            break;
                        //// 突起1 コ  2 п  3 ヒ  4 ш
                        case 7: ////// 突起1
                            Link8Mode01Execute(&blend_info);
                            break;

                        case 11: ////// 突起2
                            Link8Mode02Execute(&blend_info);
                            break;

                        case 13: ////// 突起4
                            Link8Mode04Execute(&blend_info);
                            break;



                        case 15: ////// 四角のピクセル
                            Link8SquareExecute(&blend_info);
                            break;

                        default:
                            break;
                    }

					// 突起mode3
                    if(i < input->width-2)
                    {
                        // 初期化 //
                        blend_info.i            = i+1;
                        blend_info.j            = j;
                        blend_info.in_target    = in_target+1;
                        blend_info.out_target   = out_target+1;
                        blend_info.flag         = 0;

                        mode_flg = 0;
                        if( ComparePixel(blend_info.in_target, blend_info.in_target-in_width))  (mode_flg |= 1<<0);
                        if( ComparePixel(blend_info.in_target, blend_info.in_target+in_width))  (mode_flg |= 1<<1);
                        if( ComparePixel(blend_info.in_target, blend_info.in_target+1))         (mode_flg |= 1<<2);

                        if(3==mode_flg)
                        {
                            // 突起 3 ヒ
                            Link8Mode03Execute(&blend_info);
                        }
                    }
                }

            }

        }
    }

}
