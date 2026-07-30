#ifndef PIXELFORGE_TEST_HARNESS_H
#define PIXELFORGE_TEST_HARNESS_H

#include <stdio.h>

/*
 * 依存ゼロの最小テストハーネス。
 * サンドボックス/ユーザー環境どちらでも追加のテストフレームワーク
 * （CUnit等）のインストールを前提にしたくないため自前で用意している。
 */

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define PF_ASSERT(cond, msg) \
    do { \
        g_tests_run++; \
        if (!(cond)) { \
            g_tests_failed++; \
            fprintf(stderr, "  [FAIL] %s (%s:%d) %s\n", #cond, __FILE__, __LINE__, msg); \
        } \
    } while (0)

#define PF_RUN_TEST(fn) \
    do { \
        printf("RUN  %s\n", #fn); \
        fn(); \
    } while (0)

#define PF_TEST_SUMMARY() \
    do { \
        printf("\n%d assertions checked, %d failed.\n", g_tests_run, g_tests_failed); \
        if (g_tests_failed > 0) { \
            printf("RESULT: FAILED\n"); \
        } else { \
            printf("RESULT: OK\n"); \
        } \
    } while (0)

#endif
