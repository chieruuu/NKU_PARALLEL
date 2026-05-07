#include "md5.h"
#include <iomanip>
#include <assert.h>
#include <chrono>

using namespace std;
using namespace chrono;

Byte *StringProcess(string input, int *n_byte)
{
	Byte *blocks = (Byte *)input.c_str();
	int length = input.length();
	int bitLength = length * 8;
	int paddingBits = bitLength % 512;
	if (paddingBits > 448)
	{
		paddingBits += 512 - (paddingBits - 448);
	}
	else if (paddingBits < 448)
	{
		paddingBits = 448 - paddingBits;
	}
	else if (paddingBits == 448)
	{
		paddingBits = 512;
	}

	int paddingBytes = paddingBits / 8;
	int paddedLength = length + paddingBytes + 8;
	Byte *paddedMessage = new Byte[paddedLength];

	memcpy(paddedMessage, blocks, length);

	paddedMessage[length] = 0x80;							 
	memset(paddedMessage + length + 1, 0, paddingBytes - 1); 

	for (int i = 0; i < 8; ++i)
	{
		paddedMessage[length + paddingBytes + i] = ((uint64_t)length * 8 >> (i * 8)) & 0xFF;
	}

	int residual = 8 * paddedLength % 512;
	*n_byte = paddedLength;
	return paddedMessage;
}

void MD5Hash(string input, bit32 *state)
{
	Byte *paddedMessage;
	int *messageLength = new int[1];
	for (int i = 0; i < 1; i += 1)
	{
		paddedMessage = StringProcess(input, &messageLength[i]);
		assert(messageLength[i] == messageLength[0]);
	}
	int n_blocks = messageLength[0] / 64;

	state[0] = 0x67452301;
	state[1] = 0xefcdab89;
	state[2] = 0x98badcfe;
	state[3] = 0x10325476;

	for (int i = 0; i < n_blocks; i += 1)
	{
		bit32 x[16];
		for (int i1 = 0; i1 < 16; ++i1)
		{
			x[i1] = (paddedMessage[4 * i1 + i * 64]) |
					(paddedMessage[4 * i1 + 1 + i * 64] << 8) |
					(paddedMessage[4 * i1 + 2 + i * 64] << 16) |
					(paddedMessage[4 * i1 + 3 + i * 64] << 24);
		}

		bit32 a = state[0], b = state[1], c = state[2], d = state[3];

		/* Round 1 */
		FF(a, b, c, d, x[0], s11, 0xd76aa478);
		FF(d, a, b, c, x[1], s12, 0xe8c7b756);
		FF(c, d, a, b, x[2], s13, 0x242070db);
		FF(b, c, d, a, x[3], s14, 0xc1bdceee);
		FF(a, b, c, d, x[4], s11, 0xf57c0faf);
		FF(d, a, b, c, x[5], s12, 0x4787c62a);
		FF(c, d, a, b, x[6], s13, 0xa8304613);
		FF(b, c, d, a, x[7], s14, 0xfd469501);
		FF(a, b, c, d, x[8], s11, 0x698098d8);
		FF(d, a, b, c, x[9], s12, 0x8b44f7af);
		FF(c, d, a, b, x[10], s13, 0xffff5bb1);
		FF(b, c, d, a, x[11], s14, 0x895cd7be);
		FF(a, b, c, d, x[12], s11, 0x6b901122);
		FF(d, a, b, c, x[13], s12, 0xfd987193);
		FF(c, d, a, b, x[14], s13, 0xa679438e);
		FF(b, c, d, a, x[15], s14, 0x49b40821);

		/* Round 2 */
		GG(a, b, c, d, x[1], s21, 0xf61e2562);
		GG(d, a, b, c, x[6], s22, 0xc040b340);
		GG(c, d, a, b, x[11], s23, 0x265e5a51);
		GG(b, c, d, a, x[0], s24, 0xe9b6c7aa);
		GG(a, b, c, d, x[5], s21, 0xd62f105d);
		GG(d, a, b, c, x[10], s22, 0x02441453);
		GG(c, d, a, b, x[15], s23, 0xd8a1e681);
		GG(b, c, d, a, x[4], s24, 0xe7d3fbc8);
		GG(a, b, c, d, x[9], s21, 0x21e1cde6);
		GG(d, a, b, c, x[14], s22, 0xc33707d6);
		GG(c, d, a, b, x[3], s23, 0xf4d50d87);
		GG(b, c, d, a, x[8], s24, 0x455a14ed);
		GG(a, b, c, d, x[13], s21, 0xa9e3e905);
		GG(d, a, b, c, x[2], s22, 0xfcefa3f8);
		GG(c, d, a, b, x[7], s23, 0x676f02d9);
		GG(b, c, d, a, x[12], s24, 0x8d2a4c8a);

		/* Round 3 */
		HH(a, b, c, d, x[5], s31, 0xfffa3942);
		HH(d, a, b, c, x[8], s32, 0x8771f681);
		HH(c, d, a, b, x[11], s33, 0x6d9d6122);
		HH(b, c, d, a, x[14], s34, 0xfde5380c);
		HH(a, b, c, d, x[1], s31, 0xa4beea44);
		HH(d, a, b, c, x[4], s32, 0x4bdecfa9);
		HH(c, d, a, b, x[7], s33, 0xf6bb4b60);
		HH(b, c, d, a, x[10], s34, 0xbebfbc70);
		HH(a, b, c, d, x[13], s31, 0x289b7ec6);
		HH(d, a, b, c, x[0], s32, 0xeaa127fa);
		HH(c, d, a, b, x[3], s33, 0xd4ef3085);
		HH(b, c, d, a, x[6], s34, 0x04881d05);
		HH(a, b, c, d, x[9], s31, 0xd9d4d039);
		HH(d, a, b, c, x[12], s32, 0xe6db99e5);
		HH(c, d, a, b, x[15], s33, 0x1fa27cf8);
		HH(b, c, d, a, x[2], s34, 0xc4ac5665);

		/* Round 4 */
		II(a, b, c, d, x[0], s41, 0xf4292244);
		II(d, a, b, c, x[7], s42, 0x432aff97);
		II(c, d, a, b, x[14], s43, 0xab9423a7);
		II(b, c, d, a, x[5], s44, 0xfc93a039);
		II(a, b, c, d, x[12], s41, 0x655b59c3);
		II(d, a, b, c, x[3], s42, 0x8f0ccc92);
		II(c, d, a, b, x[10], s43, 0xffeff47d);
		II(b, c, d, a, x[1], s44, 0x85845dd1);
		II(a, b, c, d, x[8], s41, 0x6fa87e4f);
		II(d, a, b, c, x[15], s42, 0xfe2ce6e0);
		II(c, d, a, b, x[6], s43, 0xa3014314);
		II(b, c, d, a, x[13], s44, 0x4e0811a1);
		II(a, b, c, d, x[4], s41, 0xf7537e82);
		II(d, a, b, c, x[11], s42, 0xbd3af235);
		II(c, d, a, b, x[2], s43, 0x2ad7d2bb);
		II(b, c, d, a, x[9], s44, 0xeb86d391);

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
	}

	for (int i = 0; i < 4; i++)
	{
		uint32_t value = state[i];
		state[i] = ((value & 0xff) << 24) |		 
				   ((value & 0xff00) << 8) |	 
				   ((value & 0xff0000) >> 8) |	 
				   ((value & 0xff000000) >> 24); 
	}

	delete[] paddedMessage;
	delete[] messageLength;
}

/*========================修改后：Intel SSE 核心===============================*/
void MD5Hash_SIMD(string input[4], bit32 state[4][4])
{
    Byte* paddedMessage[4];
    int messageLength[4];

    for(int i = 0; i < 4; i++){
        paddedMessage[i] = StringProcess(input[i], &messageLength[i]);
    }

    // 2. 初始化槽位 (__m128i 是 Intel 的 128位 向量类型)
    __m128i a = _mm_set1_epi32(0x67452301);
    __m128i b = _mm_set1_epi32(0xefcdab89);
    __m128i c = _mm_set1_epi32(0x98badcfe);
    __m128i d = _mm_set1_epi32(0x10325476);

    __m128i state0 = a, state1 = b, state2 = c, state3 = d;

    // 3. 数据重组 (AoS -> SoA)
    __m128i x[16];
    
    uint32_t* ptr0 = (uint32_t*)paddedMessage[0];
    uint32_t* ptr1 = (uint32_t*)paddedMessage[1];
    uint32_t* ptr2 = (uint32_t*)paddedMessage[2];
    uint32_t* ptr3 = (uint32_t*)paddedMessage[3];

    for (int i = 0; i < 16; ++i) {
        // 重要提示：Intel 的 _mm_set_epi32 指令参数顺序是从高位到低位！
        // 传入顺序必须反向 (ptr3, ptr2, ptr1, ptr0)，这样在内存分布上才是 0在最前面
        x[i] = _mm_set_epi32(ptr3[i], ptr2[i], ptr1[i], ptr0[i]); 
    }

    // 4. 开始 64 轮疯狂搅拌 (宏展开后会直接调用 Intel _mm_ 指令)
    /* Round 1 */
    FF_SIMD(a, b, c, d, x[0], s11, 0xd76aa478);
    FF_SIMD(d, a, b, c, x[1], s12, 0xe8c7b756);
    FF_SIMD(c, d, a, b, x[2], s13, 0x242070db);
    FF_SIMD(b, c, d, a, x[3], s14, 0xc1bdceee);
    FF_SIMD(a, b, c, d, x[4], s11, 0xf57c0faf);
    FF_SIMD(d, a, b, c, x[5], s12, 0x4787c62a);
    FF_SIMD(c, d, a, b, x[6], s13, 0xa8304613);
    FF_SIMD(b, c, d, a, x[7], s14, 0xfd469501);
    FF_SIMD(a, b, c, d, x[8], s11, 0x698098d8);
    FF_SIMD(d, a, b, c, x[9], s12, 0x8b44f7af);
    FF_SIMD(c, d, a, b, x[10], s13, 0xffff5bb1);
    FF_SIMD(b, c, d, a, x[11], s14, 0x895cd7be);
    FF_SIMD(a, b, c, d, x[12], s11, 0x6b901122);
    FF_SIMD(d, a, b, c, x[13], s12, 0xfd987193);
    FF_SIMD(c, d, a, b, x[14], s13, 0xa679438e);
    FF_SIMD(b, c, d, a, x[15], s14, 0x49b40821);

    /* Round 2 */
    GG_SIMD(a, b, c, d, x[1], s21, 0xf61e2562);
    GG_SIMD(d, a, b, c, x[6], s22, 0xc040b340);
    GG_SIMD(c, d, a, b, x[11], s23, 0x265e5a51);
    GG_SIMD(b, c, d, a, x[0], s24, 0xe9b6c7aa);
    GG_SIMD(a, b, c, d, x[5], s21, 0xd62f105d);
    GG_SIMD(d, a, b, c, x[10], s22, 0x02441453);
    GG_SIMD(c, d, a, b, x[15], s23, 0xd8a1e681);
    GG_SIMD(b, c, d, a, x[4], s24, 0xe7d3fbc8);
    GG_SIMD(a, b, c, d, x[9], s21, 0x21e1cde6);
    GG_SIMD(d, a, b, c, x[14], s22, 0xc33707d6);
    GG_SIMD(c, d, a, b, x[3], s23, 0xf4d50d87);
    GG_SIMD(b, c, d, a, x[8], s24, 0x455a14ed);
    GG_SIMD(a, b, c, d, x[13], s21, 0xa9e3e905);
    GG_SIMD(d, a, b, c, x[2], s22, 0xfcefa3f8);
    GG_SIMD(c, d, a, b, x[7], s23, 0x676f02d9);
    GG_SIMD(b, c, d, a, x[12], s24, 0x8d2a4c8a);

    /* Round 3 */
    HH_SIMD(a, b, c, d, x[5], s31, 0xfffa3942);
    HH_SIMD(d, a, b, c, x[8], s32, 0x8771f681);
    HH_SIMD(c, d, a, b, x[11], s33, 0x6d9d6122);
    HH_SIMD(b, c, d, a, x[14], s34, 0xfde5380c);
    HH_SIMD(a, b, c, d, x[1], s31, 0xa4beea44);
    HH_SIMD(d, a, b, c, x[4], s32, 0x4bdecfa9);
    HH_SIMD(c, d, a, b, x[7], s33, 0xf6bb4b60);
    HH_SIMD(b, c, d, a, x[10], s34, 0xbebfbc70);
    HH_SIMD(a, b, c, d, x[13], s31, 0x289b7ec6);
    HH_SIMD(d, a, b, c, x[0], s32, 0xeaa127fa);
    HH_SIMD(c, d, a, b, x[3], s33, 0xd4ef3085);
    HH_SIMD(b, c, d, a, x[6], s34, 0x04881d05);
    HH_SIMD(a, b, c, d, x[9], s31, 0xd9d4d039);
    HH_SIMD(d, a, b, c, x[12], s32, 0xe6db99e5);
    HH_SIMD(c, d, a, b, x[15], s33, 0x1fa27cf8);
    HH_SIMD(b, c, d, a, x[2], s34, 0xc4ac5665);

    /* Round 4 */
    II_SIMD(a, b, c, d, x[0], s41, 0xf4292244);
    II_SIMD(d, a, b, c, x[7], s42, 0x432aff97);
    II_SIMD(c, d, a, b, x[14], s43, 0xab9423a7);
    II_SIMD(b, c, d, a, x[5], s44, 0xfc93a039);
    II_SIMD(a, b, c, d, x[12], s41, 0x655b59c3);
    II_SIMD(d, a, b, c, x[3], s42, 0x8f0ccc92);
    II_SIMD(c, d, a, b, x[10], s43, 0xffeff47d);
    II_SIMD(b, c, d, a, x[1], s44, 0x85845dd1);
    II_SIMD(a, b, c, d, x[8], s41, 0x6fa87e4f);
    II_SIMD(d, a, b, c, x[15], s42, 0xfe2ce6e0);
    II_SIMD(c, d, a, b, x[6], s43, 0xa3014314);
    II_SIMD(b, c, d, a, x[13], s44, 0x4e0811a1);
    II_SIMD(a, b, c, d, x[4], s41, 0xf7537e82);
    II_SIMD(d, a, b, c, x[11], s42, 0xbd3af235);
    II_SIMD(c, d, a, b, x[2], s43, 0x2ad7d2bb);
    II_SIMD(b, c, d, a, x[9], s44, 0xeb86d391);

    // 5. 加上初始值
    a = _mm_add_epi32(a, state0);
    b = _mm_add_epi32(b, state1);
    c = _mm_add_epi32(c, state2);
    d = _mm_add_epi32(d, state3);

    // 6. 将结果写回普通内存数组
    // _mm_storeu_si128 用于非对齐的内存存入
    uint32_t out_a[4], out_b[4], out_c[4], out_d[4];
    _mm_storeu_si128((__m128i*)out_a, a);
    _mm_storeu_si128((__m128i*)out_b, b);
    _mm_storeu_si128((__m128i*)out_c, c);
    _mm_storeu_si128((__m128i*)out_d, d);

    for (int i = 0; i < 4; i++) {
        uint32_t vals[4] = {out_a[i], out_b[i], out_c[i], out_d[i]};
        for(int j = 0; j < 4; j++) {
            uint32_t value = vals[j];
            state[i][j] = ((value & 0xff) << 24) |      
                       ((value & 0xff00) << 8) |      
                       ((value & 0xff0000) >> 8) |    
                       ((value & 0xff000000) >> 24);  
        }
    }

    for(int i = 0; i < 4; i++){
        delete[] paddedMessage[i];
    }
}
/*========================修改后结束===============================*/