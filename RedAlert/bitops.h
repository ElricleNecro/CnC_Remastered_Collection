#pragma once

#include <cstdint>

// Returns the low word of a long
// #define LOW_WORD(a) ((unsigned short)((long)(a) & 0x0000FFFFL))
constexpr inline uint16_t low_word(uint32_t a) {
	return static_cast<uint16_t>(a & 0x0000FFFFU);
}

// Returns the high word of a long
// #define HIGH_WORD(a) ((unsigned long)(a) >> 16)
constexpr inline uint16_t high_word(uint32_t a) {
	return static_cast<uint16_t>(a >> 16);
}

// Merges to shorts to become a long
// #define MAKE_LONG(a, b) (((long)(a) << 16) | (long)((b) & 0x0000FFFFL))
constexpr inline uint32_t make_long(uint16_t a, uint16_t b) {
	return static_cast<uint32_t>((a << 16) | (b & 0x0000FFFFU));
}

template <class T>
constexpr inline void BitFlagsOn(T &a, T b) {
	a |= b;
}

template <class T>
constexpr inline void BitFlagsOff(T &a, T b) {
	a &= ~b;
}

template <class T>
constexpr inline T BitFlagsValue(T a, T b) {
	return a & b;
}

template <class T>
constexpr inline void BitFlagsFlip(T &a, T b) {
	a ^= b;
}
