#ifndef PIXELFORGE_FILTERS_H
#define PIXELFORGE_FILTERS_H

#include "bmp.h"

/* imgを破壊的にグレースケール化する（R=G=B=輝度）。 */
void filter_grayscale(Image *img);

/* imgの各チャンネルを 255-値 に反転する。 */
void filter_invert(Image *img);

/* 各チャンネルに delta を加算し、0〜255にクランプする。
 * deltaは負の値も許容する（減光）。 */
void filter_brightness(Image *img, int delta);

/* (2*radius+1)四方のボックスブラーを適用する。radius<=0は何もしない。
 * 端はクランプ（同じ画素の繰り返し）で扱う。 */
void filter_box_blur(Image *img, int radius);

/* Sobelオペレータによるエッジ検出。入力を内部でグレースケール化してから
 * 勾配の大きさを計算し、新しい画像（白黒のエッジ画像）を返す。
 * 呼び出し側が返り値を image_free() する責任を持つ。 */
Image filter_sobel_edge(const Image *img);

#endif
