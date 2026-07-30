#include "test_harness.h"
#include "bmp.h"
#include "filters.h"
#include <stdlib.h>
#include <string.h>

static const char *TMP_BMP_PATH = "pf_test_roundtrip_tmp.bmp";

/* ---------- bmp.c のテスト ---------- */

static void test_bmp_write_read_roundtrip(void) {
    Image img = image_create(4, 3);
    PF_ASSERT(img.data != NULL, "image_create should allocate data");

    /* 各ピクセルに異なるRGB値を入れ、BGR変換・上下反転が正しく
     * 元に戻るかを検証する。 */
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            unsigned char *px = img.data + ((long)y * 4 + x) * 3;
            px[0] = (unsigned char)(x * 10 + 1);       /* R */
            px[1] = (unsigned char)(y * 20 + 2);       /* G */
            px[2] = (unsigned char)((x + y) * 5 + 3);  /* B */
        }
    }

    int write_result = bmp_write(TMP_BMP_PATH, &img);
    PF_ASSERT(write_result == BMP_OK, "bmp_write should succeed");

    Image read_back;
    int read_result = bmp_read(TMP_BMP_PATH, &read_back);
    PF_ASSERT(read_result == BMP_OK, "bmp_read should succeed on file we just wrote");
    PF_ASSERT(read_back.width == 4, "width should round-trip");
    PF_ASSERT(read_back.height == 3, "height should round-trip");

    int all_match = 1;
    for (long i = 0; i < 4L * 3L * 3L; i++) {
        if (img.data[i] != read_back.data[i]) {
            all_match = 0;
            break;
        }
    }
    PF_ASSERT(all_match, "pixel data should be identical after write+read round-trip");

    image_free(&img);
    image_free(&read_back);
    remove(TMP_BMP_PATH);
}

static void test_bmp_read_nonexistent_file(void) {
    Image img;
    int result = bmp_read("this_file_should_not_exist_12345.bmp", &img);
    PF_ASSERT(result == BMP_ERR_OPEN, "reading a missing file should return BMP_ERR_OPEN");
}

static void test_bmp_read_invalid_signature(void) {
    FILE *fp = fopen(TMP_BMP_PATH, "wb");
    PF_ASSERT(fp != NULL, "should be able to create a temp file for the bad-signature test");
    if (fp != NULL) {
        const char *garbage = "NOT A BMP FILE AT ALL, JUST TEXT";
        fwrite(garbage, 1, strlen(garbage), fp);
        fclose(fp);
    }

    Image img;
    int result = bmp_read(TMP_BMP_PATH, &img);
    PF_ASSERT(result == BMP_ERR_FORMAT, "a file without 'BM' signature should return BMP_ERR_FORMAT");

    remove(TMP_BMP_PATH);
}

/* ---------- filters.c のテスト ---------- */

static Image make_solid_image(int w, int h, unsigned char r, unsigned char g, unsigned char b) {
    Image img = image_create(w, h);
    for (long i = 0; i < (long)w * h; i++) {
        unsigned char *px = img.data + i * 3;
        px[0] = r;
        px[1] = g;
        px[2] = b;
    }
    return img;
}

static void test_filter_grayscale_known_value(void) {
    /* 純赤(255,0,0)の輝度は 0.299*255 = 76.245 -> 四捨五入で76になるはず */
    Image img = make_solid_image(2, 2, 255, 0, 0);
    filter_grayscale(&img);
    unsigned char *px = img.data;
    PF_ASSERT(px[0] == 76, "grayscale of pure red should be 76");
    PF_ASSERT(px[0] == px[1] && px[1] == px[2], "grayscale should set R=G=B");
    image_free(&img);
}

static void test_filter_invert(void) {
    Image img = make_solid_image(1, 1, 10, 20, 30);
    filter_invert(&img);
    PF_ASSERT(img.data[0] == 245, "invert(10) should be 245");
    PF_ASSERT(img.data[1] == 235, "invert(20) should be 235");
    PF_ASSERT(img.data[2] == 225, "invert(30) should be 225");
    image_free(&img);
}

static void test_filter_brightness_clamps(void) {
    Image img = make_solid_image(1, 1, 250, 5, 128);
    filter_brightness(&img, 20);
    PF_ASSERT(img.data[0] == 255, "250+20 should clamp to 255");
    PF_ASSERT(img.data[2] == 148, "128+20 should be 148 (no clamping needed)");
    image_free(&img);

    Image img2 = make_solid_image(1, 1, 250, 5, 128);
    filter_brightness(&img2, -20);
    PF_ASSERT(img2.data[1] == 0, "5-20 should clamp to 0");
    image_free(&img2);
}

static void test_filter_box_blur_uniform_image_unchanged(void) {
    /* 一様な色の画像をぼかしても、値は変化しないはず（平均=元の値）。 */
    Image img = make_solid_image(6, 6, 100, 150, 200);
    filter_box_blur(&img, 2);
    int all_same = 1;
    for (long i = 0; i < 6L * 6L * 3L; i++) {
        unsigned char expected = (i % 3 == 0) ? 100 : (i % 3 == 1) ? 150 : 200;
        if (img.data[i] != expected) {
            all_same = 0;
            break;
        }
    }
    PF_ASSERT(all_same, "box blur on a uniform-color image should not change any pixel");
    image_free(&img);
}

static void test_filter_box_blur_radius_zero_is_noop(void) {
    Image img = make_solid_image(3, 3, 1, 2, 3);
    img.data[0] = 99; /* 1ピクセルだけ変える */
    filter_box_blur(&img, 0);
    PF_ASSERT(img.data[0] == 99, "radius=0 should leave the image untouched");
    image_free(&img);
}

static void test_filter_sobel_edge_flat_image_is_zero(void) {
    Image img = make_solid_image(5, 5, 80, 80, 80);
    Image edges = filter_sobel_edge(&img);
    PF_ASSERT(edges.data != NULL, "sobel edge should allocate output image");
    int all_zero = 1;
    for (long i = 0; i < 5L * 5L * 3L; i++) {
        if (edges.data[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    PF_ASSERT(all_zero, "a flat-color image should have zero edge magnitude everywhere");
    image_free(&img);
    image_free(&edges);
}

static void test_filter_sobel_edge_detects_vertical_boundary(void) {
    /* 左半分が黒、右半分が白の画像。境界付近のエッジ強度は高く、
     * 境界から離れた場所（左端・右端）は低いはず。 */
    int w = 10, h = 10;
    Image img = image_create(w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned char v = (x < w / 2) ? 0 : 255;
            unsigned char *px = img.data + ((long)y * w + x) * 3;
            px[0] = v;
            px[1] = v;
            px[2] = v;
        }
    }

    Image edges = filter_sobel_edge(&img);
    int mid_y = h / 2;
    unsigned char boundary_val = edges.data[((long)mid_y * w + (w / 2)) * 3];
    unsigned char far_left_val = edges.data[((long)mid_y * w + 0) * 3];

    PF_ASSERT(boundary_val > far_left_val,
        "edge magnitude at the black/white boundary should exceed a flat region far from it");
    PF_ASSERT(boundary_val > 100, "edge magnitude at a sharp boundary should be strongly nonzero");

    image_free(&img);
    image_free(&edges);
}

int main(void) {
    PF_RUN_TEST(test_bmp_write_read_roundtrip);
    PF_RUN_TEST(test_bmp_read_nonexistent_file);
    PF_RUN_TEST(test_bmp_read_invalid_signature);
    PF_RUN_TEST(test_filter_grayscale_known_value);
    PF_RUN_TEST(test_filter_invert);
    PF_RUN_TEST(test_filter_brightness_clamps);
    PF_RUN_TEST(test_filter_box_blur_uniform_image_unchanged);
    PF_RUN_TEST(test_filter_box_blur_radius_zero_is_noop);
    PF_RUN_TEST(test_filter_sobel_edge_flat_image_is_zero);
    PF_RUN_TEST(test_filter_sobel_edge_detects_vertical_boundary);
    PF_TEST_SUMMARY();
    return g_tests_failed > 0 ? 1 : 0;
}
