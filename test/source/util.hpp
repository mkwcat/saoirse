#pragma once

#define LIBOGC_SUCKS_BEGIN \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wpedantic\"")

#define LIBOGC_SUCKS_END \
	_Pragma("GCC diagnostic pop")

template <typename T> static inline T round_up(T num, unsigned int align) {
    return (num + align - 1) & -align;
}

template <typename T> static inline T round_down(T num, unsigned int align) {
    return num & -align;
}

template<class T>
inline bool aligned(T addr, unsigned int align) {
	return !(reinterpret_cast<unsigned int>(addr) & (align - 1));
}