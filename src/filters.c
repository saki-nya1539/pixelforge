#include "filters.h"
#include <stdlib.h>
#include <math.h>

static int clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

/* 端の座標を画像内にクランプする（ボックスブラー・Sobel共通で使う）。 */
static int clamp_coord(int v, int max_exclusive) {
    if (v < 0) return 0;
    if (v >= max_exclusive) return max_exclusive - 1;
    return v;
}

void filter_grayscale(Image *img) {
    if (img == NULL || img->data == NULL) {
        return;
    }
    long n = (long)img->width * (long)img->height;
    for (long i = 0; i < n; i++) {
        unsigned char *px = img->data + i * 3;
        /* ITU-R BT.601 の輝度係数（人間の目が緑に敏感なことを反映） */
        double gray = 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
        unsigned char g = (unsigned char)clamp255((int)(gray + 0.5));
        px[0] = g;
        px[1] = g;
        px[2] = g;
    }
}

void filter_invert(Image *img) {
    if (img == NULL || img->data == NULL) {
        return;
    }
    long n = (long)img->width * (long)img->height * 3;
    for (long i = 0; i < n; i++) {
        img->data[i] = (unsigned char)(255 - img->data[i]);
    }
}

void filter_brightness(Image *img, int delta) {
    if (img == NULL || img->data == NULL) {
        return;
    }
    long n = (long)img->width * (long)img->height * 3;
    for (long i = 0; i < n; i++) {
        img->data[i] = (unsigned char)clamp255((int)img->data[i] + delta);
    }
}

void filter_box_blur(Image *img, int radius) {
    if (img == NULL || img->data == NULL || radius <= 0) {
        return;
    }
    int w = img->width;
    int h = img->height;
    unsigned char *src_copy = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
    if (src_copy == NULL) {
        return; /* メモリ確保に失敗した場合は元画像のまま何もしない */
    }
    for (long i = 0; i < (long)w * h * 3; i++) {
        src_copy[i] = img->data[i];
    }

    int window = 2 * radius + 1;
    int window_area = window * window;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            long sum[3] = {0, 0, 0};
            for (int dy = -radius; dy <= radius; dy++) {
                int sy = clamp_coord(y + dy, h);
                for (int dx = -radius; dx <= radius; dx++) {
                    int sx = clamp_coord(x + dx, w);
                    const unsigned char *sp = src_copy + ((long)sy * w + sx) * 3;
                    sum[0] += sp[0];
                    sum[1] += sp[1];
                    sum[2] += sp[2];
                }
            }
            unsigned char *dp = img->data + ((long)y * w + x) * 3;
            dp[0] = (unsigned char)(sum[0] / window_area);
            dp[1] = (unsigned char)(sum[1] / window_area);
            dp[2] = (unsigned char)(sum[2] / window_area);
        }
    }

    free(src_copy);
}

Image filter_sobel_edge(const Image *img) {
    Image out = image_create(img->width, img->height);
    if (out.data == NULL || img->data == NULL) {
        return out;
    }
    int w = img->width;
    int h = img->height;

    /* まず輝度（グレースケール値）だけの一時バッファを作る。 */
    unsigned char *gray = (unsigned char *)malloc((size_t)w * (size_t)h);
    if (gray == NULL) {
        return out;
    }
    for (long i = 0; i < (long)w * h; i++) {
        const unsigned char *px = img->data + i * 3;
        double g = 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
        gray[i] = (unsigned char)clamp255((int)(g + 0.5));
    }

    static const int gx_kernel[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    static const int gy_kernel[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}
    };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int gx = 0;
            int gy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                int sy = clamp_coord(y + ky, h);
                for (int kx = -1; kx <= 1; kx++) {
                    int sx = clamp_coord(x + kx, w);
                    int v = gray[(long)sy * w + sx];
                    gx += gx_kernel[ky + 1][kx + 1] * v;
                    gy += gy_kernel[ky + 1][kx + 1] * v;
                }
            }
            double magnitude = sqrt((double)gx * gx + (double)gy * gy);
            unsigned char m = (unsigned char)clamp255((int)(magnitude + 0.5));
            unsigned char *dp = out.data + ((long)y * w + x) * 3;
            dp[0] = m;
            dp[1] = m;
            dp[2] = m;
        }
    }

    free(gray);
    return out;
}
