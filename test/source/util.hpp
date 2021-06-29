#pragma once


template <typename T> static inline T round_up(T num, unsigned int align) {
    return (num + align - 1) & -align;
}

template <typename T> static inline T round_down(T num, unsigned int align) {
    return num & -align;
}