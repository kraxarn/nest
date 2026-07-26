package main

/*
#include <stdint.h>
uint32_t murmur3_32(const void *data, const size_t len, const uint32_t seed);
*/
import "C"

func murmur3(data []byte, seed uint32) uint32 {
	return uint32(C.murmur3_32(C.CBytes(data), C.size_t(len(data)), C.uint32_t(seed)))
}
