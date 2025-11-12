#pragma once

#include <vendor/skCrypter.h>
#include <ntdef.h>

/// Returns the minimum of two values, uses `const T&` for large types to avoid copies.
template<typename T>
__forceinline T MinByRef(const T& a, const T& b) {
    return (b < a) ? b : a;
}

/// Returns the minimum of two values, uses pass-by-value for small types.
template<typename T>
__forceinline T MinByVal(T a, T b) {
    return (b < a) ? b : a;
}

/// Returns the minimum of two values, choosing the optimal passing method based on size.
template<typename T>
__forceinline T Min(const T& a, const T& b) {
    if constexpr (sizeof(T) > 8) {
        return MinByRef(a, b);
    } else {
        return MinByVal(a, b);
    }
}

/// Helper function to initialize UNICODE_STRING with encrypted string
template <size_t N, char K1, char K2>
__forceinline UNICODE_STRING initString(skc::skCrypter<N, K1, K2, wchar_t> str, void* pRtlInitUnicodeString) {
	UNICODE_STRING uStr;
	using RtlInitUnicodeString_t = void(__stdcall*)(PUNICODE_STRING, PCWSTR);
	reinterpret_cast<RtlInitUnicodeString_t>(pRtlInitUnicodeString)(&uStr, str.decrypt());
	return uStr;
}

// Helper macro to initialize UNICODE_STRING with encrypted string
#define INIT_USTRING(str) initString(skCrypt(str), KFNs::pRtlInitUnicodeString)
