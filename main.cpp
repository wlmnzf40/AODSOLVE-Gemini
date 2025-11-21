#include <immintrin.h>  // AVX, AVX2, SSE 等 SIMD 指令集
#include <stdint.h>     // uint8_t 等标准整数类型
#include <stddef.h>     // size_t 等类型
#include <stdio.h>      // printf
#include <string.h>     // strlen, memcpy
#include <time.h>       // 性能测试
#include <stdlib.h> 

#define __AVX2__
// AVX2优化的转小写函数
void lower_case_avx2(uint8_t* dst, const uint8_t* src, size_t len) {
#if defined(__AVX2__)
    const __m256i _A = _mm256_set1_epi8('A' - 1);
    const __m256i Z_ = _mm256_set1_epi8('Z' + 1);
    const __m256i delta = _mm256_set1_epi8('a' - 'A');
    uint8_t* q = dst;

    while (len >= 32) {
        __m256i op = _mm256_loadu_si256((__m256i*)src);
        __m256i gt = _mm256_cmpgt_epi8(op, _A);
        __m256i lt = _mm256_cmpgt_epi8(Z_, op);
        __m256i mingle = _mm256_and_si256(gt, lt);
        __m256i add = _mm256_and_si256(mingle, delta);
        __m256i lower = _mm256_add_epi8(op, add);
        _mm256_storeu_si256((__m256i*)q, lower);
        src += 32;
        q += 32;
        len -= 32;
    }

    // 处理剩余字节（不足32字节的部分）
    while (len > 0) {
        if (*src >= 'A' && *src <= 'Z') {
            *q = *src + ('a' - 'A');
        }
        else {
            *q = *src;
        }
        src++;
        q++;
        len--;
    }
#else
    // 如果不支持AVX2，使用普通实现
    for (size_t i = 0; i < len; i++) {
        if (src[i] >= 'A' && src[i] <= 'Z') {
            dst[i] = src[i] + ('a' - 'A');
        }
        else {
            dst[i] = src[i];
        }
    }
#endif
}

// 普通的转小写函数（用于对比）
void lower_case_scalar(uint8_t* dst, const uint8_t* src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (src[i] >= 'A' && src[i] <= 'Z') {
            dst[i] = src[i] + ('a' - 'A');
        }
        else {
            dst[i] = src[i];
        }
    }
}

// 辅助函数：打印字符串（限制长度）
void print_string(const char* label, const uint8_t* str, size_t len) {
    printf("%s: \"", label);
    size_t print_len = len < 80 ? len : 80;  // 最多打印80个字符
    for (size_t i = 0; i < print_len; i++) {
        printf("%c", str[i]);
    }
    if (len > 80) {
        printf("...");
    }
    printf("\"\n");
}

// 性能测试函数
void benchmark(const char* test_name,
    void (*func)(uint8_t*, const uint8_t*, size_t),
    const uint8_t* src, size_t len, int iterations) {
    uint8_t* dst = (uint8_t*)malloc(len);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        func(dst, src, len);
    }
    clock_t end = clock();

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    double throughput = (len * iterations) / (time_taken * 1024.0 * 1024.0);

    printf("%s:\n", test_name);
    printf("  时间: %.6f 秒\n", time_taken);
    printf("  吞吐量: %.2f MB/s\n", throughput);

    free(dst);
}

int main() {
    printf("==============================================\n");
    printf("AVX2 字符串转小写性能测试\n");
    printf("==============================================\n\n");

    // 测试1: 简单示例
    printf("【测试1】简单示例\n");
    printf("--------------------\n");
    const char* test1 = "ZZZZ Hello World! THIS IS A TEST STRING 123.";
    size_t len1 = strlen(test1);
    uint8_t* result1 = (uint8_t*)malloc(len1 + 1);

    lower_case_avx2(result1, (const uint8_t*)test1, len1);
    result1[len1] = '\0';  // 添加字符串结束符

    printf("原始字符串: \"%s\"\n", test1);
    printf("转换结果:   \"%s\"\n", (char*)result1);
    printf("\n");

    free(result1);

    // 测试2: 长字符串示例
    printf("【测试2】长字符串（包含各种字符）\n");
    printf("--------------------\n");
    const char* test2 = "The Quick BROWN Fox Jumps OVER The Lazy DOG! 12345 @#$% "
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz "
        "THIS IS A VERY LONG STRING TO TEST AVX2 PERFORMANCE!!!";
    size_t len2 = strlen(test2);
    uint8_t* result2 = (uint8_t*)malloc(len2 + 1);

    lower_case_avx2(result2, (const uint8_t*)test2, len2);
    result2[len2] = '\0';

    print_string("原始字符串", (const uint8_t*)test2, len2);
    print_string("转换结果  ", result2, len2);
    printf("\n");

    free(result2);

    // 测试3: 边界情况
    printf("【测试3】边界情况测试\n");
    printf("--------------------\n");

    // 3.1: 空字符串
    uint8_t empty_result[1] = { 0 };
    lower_case_avx2(empty_result, (const uint8_t*)"", 0);
    printf("空字符串: 通过\n");

    // 3.2: 只有小写
    const char* test3_2 = "already lowercase 123";
    uint8_t* result3_2 = (uint8_t*)malloc(strlen(test3_2) + 1);
    lower_case_avx2(result3_2, (const uint8_t*)test3_2, strlen(test3_2));
    result3_2[strlen(test3_2)] = '\0';
    printf("只有小写: \"%s\" -> \"%s\"\n", test3_2, (char*)result3_2);
    free(result3_2);

    // 3.3: 只有大写
    const char* test3_3 = "ALL UPPERCASE";
    uint8_t* result3_3 = (uint8_t*)malloc(strlen(test3_3) + 1);
    lower_case_avx2(result3_3, (const uint8_t*)test3_3, strlen(test3_3));
    result3_3[strlen(test3_3)] = '\0';
    printf("只有大写: \"%s\" -> \"%s\"\n", test3_3, (char*)result3_3);
    free(result3_3);

    // 3.4: 长度不是32的倍数
    const char* test3_4 = "SHORT";  // 5字节
    uint8_t* result3_4 = (uint8_t*)malloc(strlen(test3_4) + 1);
    lower_case_avx2(result3_4, (const uint8_t*)test3_4, strlen(test3_4));
    result3_4[strlen(test3_4)] = '\0';
    printf("短字符串: \"%s\" -> \"%s\"\n", test3_4, (char*)result3_4);
    free(result3_4);

    printf("\n");

    // 测试4: 性能对比
    printf("【测试4】性能对比测试\n");
    printf("--------------------\n");

    // 创建一个大的测试字符串（1MB）
    size_t large_size = 1024 * 1024;  // 1MB
    uint8_t* large_test = (uint8_t*)malloc(large_size);

    // 填充测试数据（50%大写，50%小写和其他字符）
    for (size_t i = 0; i < large_size; i++) {
        if (i % 4 == 0) {
            large_test[i] = 'A' + (i % 26);  // 大写字母
        }
        else if (i % 4 == 1) {
            large_test[i] = 'a' + (i % 26);  // 小写字母
        }
        else if (i % 4 == 2) {
            large_test[i] = '0' + (i % 10);  // 数字
        }
        else {
            large_test[i] = ' ';  // 空格
        }
    }

    printf("测试数据大小: %zu MB\n", large_size / (1024 * 1024));
    printf("迭代次数: 100\n\n");

#ifdef __AVX2__
    printf("AVX2支持: 是\n\n");
    benchmark("AVX2优化版本", lower_case_avx2, large_test, large_size, 100);
#else
    printf("AVX2支持: 否（将使用标量实现）\n\n");
#endif

    benchmark("标量版本", lower_case_scalar, large_test, large_size, 100);

    free(large_test);

    // 测试5: 正确性验证
    printf("\n【测试5】正确性验证\n");
    printf("--------------------\n");

    size_t verify_size = 1000;
    uint8_t* verify_src = (uint8_t*)malloc(verify_size);
    uint8_t* verify_dst1 = (uint8_t*)malloc(verify_size);
    uint8_t* verify_dst2 = (uint8_t*)malloc(verify_size);

    // 生成随机测试数据
    for (size_t i = 0; i < verify_size; i++) {
        int r = rand() % 100;
        if (r < 26) {
            verify_src[i] = 'A' + (rand() % 26);  // 大写字母
        }
        else if (r < 52) {
            verify_src[i] = 'a' + (rand() % 26);  // 小写字母
        }
        else if (r < 62) {
            verify_src[i] = '0' + (rand() % 10);  // 数字
        }
        else {
            verify_src[i] = ' ' + (rand() % 32);  // 其他字符
        }
    }

    // 使用两种方法转换
    lower_case_avx2(verify_dst1, verify_src, verify_size);
    lower_case_scalar(verify_dst2, verify_src, verify_size);

    // 比较结果
    int mismatch = 0;
    for (size_t i = 0; i < verify_size; i++) {
        if (verify_dst1[i] != verify_dst2[i]) {
            mismatch++;
            if (mismatch <= 5) {  // 只打印前5个不匹配
                printf("位置 %zu: AVX2=%c(%d) vs 标量=%c(%d) [原始=%c(%d)]\n",
                    i, verify_dst1[i], verify_dst1[i],
                    verify_dst2[i], verify_dst2[i],
                    verify_src[i], verify_src[i]);
            }
        }
    }

    if (mismatch == 0) {
        printf("✓ 正确性验证通过！AVX2结果与标量版本完全一致。\n");
    }
    else {
        printf("✗ 发现 %d 处不匹配（共测试 %zu 字节）\n", mismatch, verify_size);
    }

    free(verify_src);
    free(verify_dst1);
    free(verify_dst2);

    printf("\n==============================================\n");
    printf("测试完成！\n");
    printf("==============================================\n");

    return 0;
}
