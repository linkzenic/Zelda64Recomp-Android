// Originally written by Mr-Wiseguy and modified for this library
// https://gist.github.com/Mr-Wiseguy/6cca110d74b32b5bb19b76cfa2d7ab4f

// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "yaz0.hpp"

static void search(int a1, int a2, int *a3, int *a4, const uint8_t *data_in);
static int mischarsearch(const uint8_t *a1, int a2, const uint8_t *a3, int a4);
static void initskip(const uint8_t *a1, int a2, unsigned short *skip);

static inline uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}

#define DIVIDE_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#define MAX(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })
#define MIN(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })

typedef struct {
    uint32_t magic;
    uint32_t uncompressed_size;
    uint32_t padding[2];
} Yaz0Header;

// dst is caller-freed memory!
uint32_t yaz0_compress(uint32_t input_size, const uint8_t *src, uint8_t **dst) {
    // Worst-case size for output is zero compression on the input, meaning the input size plus the number of layout bytes.
    // There would be one layout byte for every 8 input bytes, so the worst-case size is:
    //   input_size + ROUND_UP_DIVIDE(input_size, 8)
    uint8_t *output = static_cast<uint8_t *>(calloc(input_size + DIVIDE_ROUND_UP(input_size, 8), sizeof(unsigned char)));
    uint8_t *cur_layout_byte = &output[0];
    uint8_t *out_ptr = cur_layout_byte + 1;
    unsigned int input_pos = 0;
    unsigned int cur_layout_bit = 0x80;

    while (input_pos < input_size) {
        int group_pos;
        int group_size;

        search(input_pos, input_size, &group_pos, &group_size, src);

        // If the group isn't larger than 2 bytes, copying the input without compression is smaller
        if (group_size <= 2) {
            // Set the current layout bit to indicate that this is an uncompressed byte
            *cur_layout_byte |= cur_layout_bit;
            *out_ptr++ = src[input_pos++];
        } else {
            int new_size;
            int new_position;

            // Search for a new group after one position after the current one
            search(input_pos + 1, input_size, &new_position, &new_size, src);

            // If the new group is better than the current group by at least 2 bytes, use it one instead
            if (new_size >= group_size + 2) {
                // Mark the current layout bit to skip compressing this byte, as the next input position yielded better compression
                *cur_layout_byte |= cur_layout_bit;
                // Copy the input byte to the output
                *out_ptr++ = src[input_pos++];

                // Advance to the next layout bit
                cur_layout_bit >>= 1;

                if (!cur_layout_bit) {
                    cur_layout_bit = 0x80;
                    cur_layout_byte = out_ptr++;
                    *cur_layout_byte = 0;
                }

                group_size = new_size;
                group_pos = new_position;
            }

            // Calculate the offset for the current group
            int group_offset = input_pos - group_pos - 1;

            // Determine which encoding to use for the current group
            if (group_size >= 0x12) {
                // Three bytes, 0RRRNN
                *out_ptr++ = (group_offset >> 8);
                *out_ptr++ = (group_offset & 0xFF);
                *out_ptr++ = group_size - 0x12;
            } else {
                // Two bytes, NRRR
                *out_ptr++ = (group_offset >> 8) | ((group_size - 2) << 4);
                *out_ptr++ = (group_offset & 0xFF);
            }

            // Move forward in the input by the size of the group
            input_pos += group_size;
        }

        // Advance to the next layout bit
        cur_layout_bit >>= 1;

        if (!cur_layout_bit) {
            cur_layout_bit = 0x80;
            cur_layout_byte = out_ptr++;
            *cur_layout_byte = 0;
        }
    }

    if (cur_layout_bit != 0x80) {
        out_ptr++;
    }

    *dst = output;
    return out_ptr - output;
}

#define MAX_OFFSET 0x1000
#define MAX_SIZE 0x111

static void search(int input_pos, int input_size, int *pos_out, int *size_out, const uint8_t *data_in) {
    // Current group size
    int cur_size = 3;
    // Current position being searched
    int search_pos = MAX(input_pos - MAX_OFFSET, 0);
    // Number of bytes to search for
    int search_size = MIN(input_size - input_pos, MAX_SIZE);
    // Position of the current group
    int found_pos = 0;
    // Offset from the search pos that the group starts at
    int found_offset;

    if (search_size >= 3) {
        while (input_pos > search_pos) {
            found_offset = mischarsearch(&data_in[input_pos], cur_size, &data_in[search_pos], cur_size + input_pos - search_pos);

            if (found_offset >= input_pos - search_pos) {
                break;
            }

            while (search_size > cur_size) {
                if (data_in[cur_size + search_pos + found_offset] != data_in[cur_size + input_pos]) {
                    break;
                }

                cur_size++;
            }

            if (search_size == cur_size) {
                *pos_out = found_offset + search_pos;
                *size_out = cur_size;
                return;
            }

            found_pos = search_pos + found_offset;
            search_pos = found_pos + 1;
            ++cur_size;
        }

        *pos_out = found_pos;

        if (cur_size > 3) {
            *size_out = cur_size - 1;
        } else {
            *size_out = 0;
        }
    } else {
        // Not enough room to find a group
        *size_out = 0;
        *pos_out = 0;
    }
}

static int mischarsearch(const uint8_t *pattern, int patternlen, const uint8_t *data, int datalen) {
    static unsigned short skip[256]; // idb
    int result;                      // eax
    int i;                           // ebx
    int v6;                          // eax
    int j;                           // ecx

    result = datalen;
    if (patternlen <= datalen) {
        initskip(pattern, patternlen, skip);
        i = patternlen - 1;

        while (true) {
            if (pattern[patternlen - 1] == data[i]) {
                --i;
                j = patternlen - 2;

                if (patternlen - 2 < 0) {
                    return i + 1;
                }

                while (pattern[j] == data[i]) {
                    j--;
                    i--;
                    if (j < 0) {
                        return i + 1;
                    }
                }
                v6 = patternlen - j;

                if (skip[data[i]] > patternlen - j) {
                    v6 = skip[data[i]];
                }
            } else {
                v6 = skip[data[i]];
            }

            i += v6;
        }
    }
    return result;
}

static void initskip(const uint8_t *pattern, int len, unsigned short *skip) {
    for (int i = 0; i < 256; i++) {
        skip[i] = len;
    }

    for (int i = 0; i < len; i++) {
        skip[pattern[i]] = len - i - 1;
    }
}

void naive_copy(void *dst, void *src, int size) {
    uint8_t *cur_out = static_cast<uint8_t *>(dst);
    uint8_t *cur_in = static_cast<uint8_t *>(src);

    while (size--) {
        *cur_out++ = *cur_in++;
    }
}

void yaz0_decompress(uint32_t uncompressedLength, uint32_t compressedSize, const uint8_t *srcPtr, uint8_t *dstPtr) {
    int32_t layoutBitIndex;
    const uint8_t *srcEnd;
    uint8_t *dstEnd;
    uint8_t layoutBits;

    srcEnd = srcPtr + compressedSize;
    dstEnd = dstPtr + uncompressedLength;

    while (srcPtr < srcEnd) {
        layoutBitIndex = 0;
        layoutBits = *srcPtr++;

        while (layoutBitIndex < 8 && srcPtr < srcEnd && dstPtr < dstEnd) {
            if (layoutBits & 0x80) {
                *dstPtr++ = *srcPtr++;
            } else {
                int32_t firstByte = *srcPtr++;
                int32_t secondByte = *srcPtr++;
                uint32_t bytes = firstByte << 8 | secondByte;
                uint32_t offset = (bytes & 0x0FFF) + 1;
                uint32_t length;

                // Check how the group length is encoded
                if ((firstByte & 0xF0) == 0) {
                    // 3 byte encoding, 0RRRNN
                    int32_t thirdByte = *srcPtr++;
                    length = thirdByte + 0x12;
                } else {
                    // 2 byte encoding, NRRR
                    length = ((bytes & 0xF000) >> 12) + 2;
                }

                naive_copy(dstPtr, dstPtr - offset, length);
                dstPtr += length;
            }

            layoutBitIndex++;
            layoutBits <<= 1;
        }
    }
}
