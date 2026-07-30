#include "bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- リトルエンディアンでの読み書きヘルパー ----
 * BMPは常にリトルエンディアンでフィールドを格納するため、
 * 構造体に直接キャストする方式（パディング/エンディアン依存で
 * 環境によって壊れる）は避け、バイト単位で明示的に組み立てる。
 */

static unsigned int read_u16le(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned long read_u32le(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static long read_i32le(const unsigned char *p) {
    unsigned long v = read_u32le(p);
    /* 2の補数を仮定して符号付きに変換する */
    if (v & 0x80000000UL) {
        return -(long)(0xFFFFFFFFUL - v) - 1;
    }
    return (long)v;
}

static void write_u16le(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void write_u32le(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

Image image_create(int width, int height) {
    Image img;
    img.width = width;
    img.height = height;
    if (width <= 0 || height <= 0) {
        img.data = NULL;
        return img;
    }
    img.data = (unsigned char *)calloc((size_t)width * (size_t)height * 3, 1);
    return img;
}

void image_free(Image *img) {
    if (img == NULL) {
        return;
    }
    free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
}

int bmp_read(const char *path, Image *out_img) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return BMP_ERR_OPEN;
    }

    unsigned char file_header[14];
    if (fread(file_header, 1, 14, fp) != 14) {
        fclose(fp);
        return BMP_ERR_FORMAT;
    }
    if (file_header[0] != 'B' || file_header[1] != 'M') {
        fclose(fp);
        return BMP_ERR_FORMAT;
    }
    unsigned long pixel_offset = read_u32le(&file_header[10]);

    unsigned char dib_header[40];
    if (fread(dib_header, 1, 40, fp) != 40) {
        fclose(fp);
        return BMP_ERR_FORMAT;
    }
    unsigned long dib_size = read_u32le(&dib_header[0]);
    if (dib_size < 40) {
        /* BITMAPCOREHEADER等の旧形式は非対応 */
        fclose(fp);
        return BMP_ERR_FORMAT;
    }
    long width = read_i32le(&dib_header[4]);
    long height_raw = read_i32le(&dib_header[8]);
    unsigned int bpp = read_u16le(&dib_header[14]);
    unsigned long compression = read_u32le(&dib_header[16]);

    if (width <= 0 || height_raw == 0 || bpp != 24 || compression != 0) {
        /* 24bit非圧縮のみサポート */
        fclose(fp);
        return BMP_ERR_FORMAT;
    }

    int bottom_up = 1;
    long height = height_raw;
    if (height_raw < 0) {
        bottom_up = 0;
        height = -height_raw;
    }

    Image img = image_create((int)width, (int)height);
    if (img.data == NULL) {
        fclose(fp);
        return BMP_ERR_ALLOC;
    }

    if (fseek(fp, (long)pixel_offset, SEEK_SET) != 0) {
        image_free(&img);
        fclose(fp);
        return BMP_ERR_FORMAT;
    }

    int row_bytes = (int)width * 3;
    int padded_row_bytes = ((row_bytes + 3) / 4) * 4;
    unsigned char *row_buf = (unsigned char *)malloc((size_t)padded_row_bytes);
    if (row_buf == NULL) {
        image_free(&img);
        fclose(fp);
        return BMP_ERR_ALLOC;
    }

    for (long r = 0; r < height; r++) {
        if (fread(row_buf, 1, (size_t)padded_row_bytes, fp) != (size_t)padded_row_bytes) {
            free(row_buf);
            image_free(&img);
            fclose(fp);
            return BMP_ERR_FORMAT;
        }
        /* BMPの行はBGR順。bottom_upの場合はファイル上の最初の行が
         * 画像の一番下の行にあたるため、格納先の行を反転させる。 */
        long dest_row = bottom_up ? (height - 1 - r) : r;
        unsigned char *dest = img.data + (size_t)dest_row * (size_t)width * 3;
        for (long c = 0; c < width; c++) {
            unsigned char b = row_buf[c * 3 + 0];
            unsigned char g = row_buf[c * 3 + 1];
            unsigned char rr = row_buf[c * 3 + 2];
            dest[c * 3 + 0] = rr;
            dest[c * 3 + 1] = g;
            dest[c * 3 + 2] = b;
        }
    }

    free(row_buf);
    fclose(fp);
    *out_img = img;
    return BMP_OK;
}

int bmp_write(const char *path, const Image *img) {
    if (img == NULL || img->data == NULL || img->width <= 0 || img->height <= 0) {
        return BMP_ERR_FORMAT;
    }

    int row_bytes = img->width * 3;
    int padded_row_bytes = ((row_bytes + 3) / 4) * 4;
    unsigned long pixel_data_size = (unsigned long)padded_row_bytes * (unsigned long)img->height;
    unsigned long file_size = 14 + 40 + pixel_data_size;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return BMP_ERR_OPEN;
    }

    unsigned char file_header[14];
    file_header[0] = 'B';
    file_header[1] = 'M';
    write_u32le(&file_header[2], file_size);
    write_u32le(&file_header[6], 0); /* reserved */
    write_u32le(&file_header[10], 14 + 40);
    if (fwrite(file_header, 1, 14, fp) != 14) {
        fclose(fp);
        return BMP_ERR_WRITE;
    }

    unsigned char dib_header[40];
    memset(dib_header, 0, sizeof(dib_header));
    write_u32le(&dib_header[0], 40);
    write_u32le(&dib_header[4], (unsigned long)img->width);
    write_u32le(&dib_header[8], (unsigned long)img->height); /* 正の値=bottom-upで出力 */
    write_u16le(&dib_header[12], 1);  /* planes */
    write_u16le(&dib_header[14], 24); /* bpp */
    write_u32le(&dib_header[16], 0);  /* BI_RGB */
    write_u32le(&dib_header[20], pixel_data_size);
    write_u32le(&dib_header[24], 2835); /* ~72dpi */
    write_u32le(&dib_header[28], 2835);
    write_u32le(&dib_header[32], 0);
    write_u32le(&dib_header[36], 0);
    if (fwrite(dib_header, 1, 40, fp) != 40) {
        fclose(fp);
        return BMP_ERR_WRITE;
    }

    unsigned char *row_buf = (unsigned char *)calloc((size_t)padded_row_bytes, 1);
    if (row_buf == NULL) {
        fclose(fp);
        return BMP_ERR_ALLOC;
    }

    for (int r = img->height - 1; r >= 0; r--) {
        const unsigned char *src = img->data + (size_t)r * (size_t)img->width * 3;
        for (int c = 0; c < img->width; c++) {
            unsigned char rr = src[c * 3 + 0];
            unsigned char g = src[c * 3 + 1];
            unsigned char b = src[c * 3 + 2];
            row_buf[c * 3 + 0] = b;
            row_buf[c * 3 + 1] = g;
            row_buf[c * 3 + 2] = rr;
        }
        if (fwrite(row_buf, 1, (size_t)padded_row_bytes, fp) != (size_t)padded_row_bytes) {
            free(row_buf);
            fclose(fp);
            return BMP_ERR_WRITE;
        }
    }

    free(row_buf);
    fclose(fp);
    return BMP_OK;
}
