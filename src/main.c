#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmp.h"
#include "filters.h"

static void print_usage(const char *prog) {
    fprintf(stderr,
        "PixelForge - BMP画像フィルタ・エッジ検出CLIツール\n\n"
        "使い方:\n"
        "  %s <入力.bmp> <出力.bmp> --filter <種類> [オプション]\n\n"
        "フィルタ種類:\n"
        "  grayscale   グレースケール変換\n"
        "  invert      色反転（ネガポジ反転）\n"
        "  brightness  明るさ調整（--amount が必須。負の値も可）\n"
        "  blur        ボックスブラー（--radius 省略時は1）\n"
        "  edge        Sobelオペレータによるエッジ検出\n\n"
        "例:\n"
        "  %s photo.bmp gray.bmp --filter grayscale\n"
        "  %s photo.bmp bright.bmp --filter brightness --amount 30\n"
        "  %s photo.bmp blur.bmp --filter blur --radius 3\n"
        "  %s photo.bmp edge.bmp --filter edge\n\n"
        "対応形式: 24bit非圧縮BMP（BI_RGB）のみ\n",
        prog, prog, prog, prog, prog);
}

static const char *bmp_err_message(int code) {
    switch (code) {
        case BMP_ERR_OPEN:   return "ファイルを開けませんでした";
        case BMP_ERR_FORMAT: return "対応していないBMP形式です（24bit非圧縮BMPのみ対応）";
        case BMP_ERR_ALLOC:  return "メモリの確保に失敗しました";
        case BMP_ERR_WRITE:  return "ファイルへの書き込みに失敗しました";
        default:              return "不明なエラーです";
    }
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return (argc < 2) ? 1 : 0;
    }

    if (argc < 4) {
        fprintf(stderr, "エラー: 引数が不足しています。\n\n");
        print_usage(argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    const char *filter_name = NULL;
    int has_amount = 0;
    int amount = 0;
    int radius = 1;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter_name = argv[++i];
        } else if (strcmp(argv[i], "--amount") == 0 && i + 1 < argc) {
            amount = atoi(argv[++i]);
            has_amount = 1;
        } else if (strcmp(argv[i], "--radius") == 0 && i + 1 < argc) {
            radius = atoi(argv[++i]);
        } else {
            fprintf(stderr, "エラー: 不明なオプション '%s' です。\n\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (filter_name == NULL) {
        fprintf(stderr, "エラー: --filter は必須です。\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(filter_name, "brightness") == 0 && !has_amount) {
        fprintf(stderr, "エラー: brightnessフィルタには --amount が必須です。\n");
        return 1;
    }

    Image img;
    int read_result = bmp_read(input_path, &img);
    if (read_result != BMP_OK) {
        fprintf(stderr, "エラー: '%s' の読み込みに失敗しました（%s）。\n",
                input_path, bmp_err_message(read_result));
        return 1;
    }

    printf("読み込み: %s (%dx%d)\n", input_path, img.width, img.height);

    Image *result = &img;
    Image edge_result;
    int free_result_separately = 0;

    if (strcmp(filter_name, "grayscale") == 0) {
        filter_grayscale(&img);
        printf("フィルタ適用: grayscale\n");
    } else if (strcmp(filter_name, "invert") == 0) {
        filter_invert(&img);
        printf("フィルタ適用: invert\n");
    } else if (strcmp(filter_name, "brightness") == 0) {
        filter_brightness(&img, amount);
        printf("フィルタ適用: brightness (amount=%d)\n", amount);
    } else if (strcmp(filter_name, "blur") == 0) {
        if (radius <= 0) {
            fprintf(stderr, "エラー: --radius は1以上を指定してください。\n");
            image_free(&img);
            return 1;
        }
        filter_box_blur(&img, radius);
        printf("フィルタ適用: blur (radius=%d)\n", radius);
    } else if (strcmp(filter_name, "edge") == 0) {
        edge_result = filter_sobel_edge(&img);
        if (edge_result.data == NULL) {
            fprintf(stderr, "エラー: エッジ検出処理に失敗しました（メモリ不足の可能性）。\n");
            image_free(&img);
            return 1;
        }
        result = &edge_result;
        free_result_separately = 1;
        printf("フィルタ適用: edge (Sobel)\n");
    } else {
        fprintf(stderr, "エラー: 不明なフィルタ '%s' です。\n\n", filter_name);
        print_usage(argv[0]);
        image_free(&img);
        return 1;
    }

    int write_result = bmp_write(output_path, result);
    if (write_result != BMP_OK) {
        fprintf(stderr, "エラー: '%s' への書き込みに失敗しました（%s）。\n",
                output_path, bmp_err_message(write_result));
        image_free(&img);
        if (free_result_separately) {
            image_free(&edge_result);
        }
        return 1;
    }

    printf("書き込み完了: %s\n", output_path);

    image_free(&img);
    if (free_result_separately) {
        image_free(&edge_result);
    }
    return 0;
}
