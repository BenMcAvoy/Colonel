#pragma once

template<typename T>
__forceinline T MinByRef(const T& a, const T& b) {
    return (b < a) ? b : a;
}

template<typename T>
__forceinline T MinByVal(T a, T b) {
    return (b < a) ? b : a;
}

template<typename T>
__forceinline T Min(const T& a, const T& b) {
    if constexpr (sizeof(T) > 8) {
        return MinByRef(a, b);
    } else {
        return MinByVal(a, b);
    }
}
