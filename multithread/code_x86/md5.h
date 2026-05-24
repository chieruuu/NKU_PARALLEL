#include <iostream>
#include <string>
#include <cstring>
// 引入 Intel 的 SIMD 头文件（包含了 SSE 和 AVX）
#include <immintrin.h>

using namespace std;

// 定义了Byte，便于使用
typedef unsigned char Byte;
// 定义了32比特
typedef unsigned int bit32;

// MD5的一系列参数
#define s11 7
#define s12 12
#define s13 17
#define s14 22
#define s21 5
#define s22 9
#define s23 14
#define s24 20
#define s31 4
#define s32 11
#define s33 16
#define s34 23
#define s41 6
#define s42 10
#define s43 15
#define s44 21

// 原始标量运算宏
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/*========================修改后===============================*/
// Intel SSE 没有专用的按位取反指令，需要通过与全 1 (0xFFFFFFFF) 异或来实现取反
#define NOT_SIMD(x) _mm_xor_si128((x), _mm_set1_epi32(0xFFFFFFFF))

// SIMD 版本的逻辑运算 (替换为 Intel SSE 指令)
#define F_SIMD(x, y, z) _mm_or_si128(_mm_and_si128((x), (y)), _mm_and_si128(NOT_SIMD(x), (z)))
#define G_SIMD(x, y, z) _mm_or_si128(_mm_and_si128((x), (z)), _mm_and_si128((y), NOT_SIMD(z)))
#define H_SIMD(x, y, z) _mm_xor_si128(_mm_xor_si128((x), (y)), (z))
#define I_SIMD(x, y, z) _mm_xor_si128((y), _mm_or_si128((x), NOT_SIMD(z)))

#define ROTATELEFT(num, n) (((num) << (n)) | ((num) >> (32-(n))))

#define FF(a, b, c, d, x, s, ac) { \
  (a) += F ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
  (a) += G ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
  (a) += H ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
  (a) += I ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}

// SIMD 循环左移：Intel 同样用普通的左移和右移拼接起来
#define ROTATELEFT_SIMD(num, n) _mm_or_si128(_mm_slli_epi32((num), (n)), _mm_srli_epi32((num), 32-(n)))

// SIMD 轮函数：_mm_add_epi32 是向量加法；_mm_set1_epi32 是常量广播
#define FF_SIMD(a, b, c, d, x, s, ac) { \
  (a) = _mm_add_epi32((a), _mm_add_epi32(F_SIMD((b), (c), (d)), _mm_add_epi32((x), _mm_set1_epi32(ac)))); \
  (a) = ROTATELEFT_SIMD((a), (s)); \
  (a) = _mm_add_epi32((a), (b)); \
}
#define GG_SIMD(a, b, c, d, x, s, ac) { \
  (a) = _mm_add_epi32((a), _mm_add_epi32(G_SIMD((b), (c), (d)), _mm_add_epi32((x), _mm_set1_epi32(ac)))); \
  (a) = ROTATELEFT_SIMD((a), (s)); \
  (a) = _mm_add_epi32((a), (b)); \
}
#define HH_SIMD(a, b, c, d, x, s, ac) { \
  (a) = _mm_add_epi32((a), _mm_add_epi32(H_SIMD((b), (c), (d)), _mm_add_epi32((x), _mm_set1_epi32(ac)))); \
  (a) = ROTATELEFT_SIMD((a), (s)); \
  (a) = _mm_add_epi32((a), (b)); \
}
#define II_SIMD(a, b, c, d, x, s, ac) { \
  (a) = _mm_add_epi32((a), _mm_add_epi32(I_SIMD((b), (c), (d)), _mm_add_epi32((x), _mm_set1_epi32(ac)))); \
  (a) = ROTATELEFT_SIMD((a), (s)); \
  (a) = _mm_add_epi32((a), (b)); \
}
/*========================修改后===============================*/

void MD5Hash(string input, bit32 *state);
void MD5Hash_SIMD(string input[4], bit32 state[4][4]);