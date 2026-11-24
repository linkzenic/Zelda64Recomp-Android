#ifndef __YAZ0_HPP__
#define __YAZ0_HPP__

#include <stdint.h>

void yaz0_decompress(uint32_t uncompressedLength, uint32_t compressedSize, const uint8_t *srcPtr, uint8_t *dstPtr);
uint32_t yaz0_compress(uint32_t input_size, const uint8_t *src, uint8_t **dst);

#endif
