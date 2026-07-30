#ifndef PIXELFORGE_BMP_H
#define PIXELFORGE_BMP_H

/*
 * 24bit非圧縮BMP（Windows Bitmap, BI_RGB）専用の最小限リーダー/ライター。
 * 外部ライブラリ（libpng等）はサンドボックス/ユーザー環境の双方で
 * 確実に使える保証がないため、あえて依存ゼロで自前実装している。
 *
 * Image.data は常に「上から下、1ピクセルRGBの順で3バイト」の
 * 行優先バイト列として保持する（BMPファイル上のBGR・下から上とは
 * 読み書き時に変換する）。
 */

typedef struct {
    int width;
    int height;
    unsigned char *data; /* width*height*3 bytes, RGB, top-to-bottom */
} Image;

/* 成功時0、失敗時は負の値を返す。 */
#define BMP_OK 0
#define BMP_ERR_OPEN -1
#define BMP_ERR_FORMAT -2
#define BMP_ERR_ALLOC -3
#define BMP_ERR_WRITE -4

int bmp_read(const char *path, Image *out_img);
int bmp_write(const char *path, const Image *img);

Image image_create(int width, int height);
void image_free(Image *img);

#endif
