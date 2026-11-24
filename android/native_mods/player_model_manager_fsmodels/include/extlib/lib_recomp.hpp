#pragma once

extern "C" {
    #include "mod_recomp.h"
    #define RECOMP_API_VERSION 1
    #define TO_PTR(type, var) ((type*)(&rdram[(uint64_t)var - 0xFFFFFFFF80000000]))
    #define PTR(x) int32_t

    // Type Defs:
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;

    typedef int8_t s8;
    typedef int16_t s16;
    typedef int32_t s32;
    typedef int64_t s64;
}



#include <string>
#include <stdint.h>

#define RDRAM_TO_PTR(rdram, type, var) ((type*)(&rdram[(uint64_t)var - 0xFFFFFFFF80000000]))
#define TO_PTR(type, var) ((type*)(&rdram[(uint64_t)var - 0xFFFFFFFF80000000]))
#define PTR(x) int32_t

#if defined(_WIN32)
    #define DLLEXPORT __declspec(dllexport)
    #define DLLIMPORT __declspec(dllimport)
#else
    #define DLLEXPORT __attribute__((visibility("default")))
    #define DLLIMPORT
#endif

inline std::string ptr_to_string(uint8_t* rdram, PTR(char) str) {
    size_t len = 0;
    while (MEM_B(str, len) != 0x00) {
        len++;
    }

    std::string ret{};
    ret.reserve(len + 1);

    for (size_t i = 0; i < len; i++) {
        ret += (char)MEM_B(str, i);
    }

    return ret;
}

inline std::u8string ptr_to_u8string(uint8_t* rdram, PTR(char) str) {
    size_t len = 0;
    while (MEM_B(str, len) != 0x00) {
        len++;
    }

    std::u8string ret{};
    ret.reserve(len + 1);

    for (size_t i = 0; i < len; i++) {
        ret += (char)MEM_B(str, i);
    }

    return ret;
}

template<int index, typename T>
T _arg(uint8_t* rdram, recomp_context* ctx) {
    static_assert(index < 4, "Only args 0 through 3 supported");
    gpr raw_arg = (&ctx->r4)[index];
    if constexpr (std::is_same_v<T, float>) {
        if constexpr (index < 2) {
            static_assert(index != 1, "Floats in arg 1 not supported");
            return ctx->f12.fl;
        }
        else {
            // static_assert in else workaround
            [] <bool flag = false>() {
                static_assert(flag, "Floats in a2/a3 not supported");
            }();
        }
    }
    else if constexpr (std::is_pointer_v<T>) {
        static_assert (!std::is_pointer_v<std::remove_pointer_t<T>>, "Double pointers not supported");
        return TO_PTR(std::remove_pointer_t<T>, raw_arg);
    }
    else if constexpr (std::is_integral_v<T>) {
        static_assert(sizeof(T) <= 4, "64-bit args not supported");
        return static_cast<T>(raw_arg);
    }
    else {
        // static_assert in else workaround
        [] <bool flag = false>() {
            static_assert(flag, "Unsupported type");
        }();
    }
}

template <int arg_index>
std::string _arg_string(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) str = _arg<arg_index, PTR(char)>(rdram, ctx);

    // Get the length of the byteswapped string.
    return ptr_to_string(rdram, str);
}

template <int arg_index>
std::u8string _arg_u8string(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) str = _arg<arg_index, PTR(char)>(rdram, ctx);

    // Get the length of the byteswapped string.
    return ptr_to_u8string(rdram, str);
}

template <typename T>
void _return(recomp_context* ctx, T val) {
    static_assert(sizeof(T) <= 4 && "Only 32-bit value returns supported currently");
    if constexpr (std::is_same_v<T, float>) {
        ctx->f0.fl = val;
    }
    else if constexpr (std::is_integral_v<T> && sizeof(T) <= 4) {
        ctx->r2 = int32_t(val);
    }
    else {
        // static_assert in else workaround
        [] <bool flag = false>() {
            static_assert(flag, "Unsupported type");
        }();
    }
}

#define NO_EXTERN_RECOMP_DLL_FUNC(_f_name) RECOMP_EXPORT void _f_name(uint8_t* rdram, recomp_context* ctx)
#define RECOMP_DLL_FUNC(_f_name) extern "C" NO_EXTERN_RECOMP_DLL_FUNC(_f_name)
#define RECOMP_ARG(_type, _pos) _arg<_pos, _type>(rdram, ctx)
#define RECOMP_ARG_STR(_pos) _arg_string<_pos>(rdram, ctx)
#define RECOMP_ARG_U8STR(_pos) _arg_u8string<_pos>(rdram, ctx)
#define RECOMP_RETURN(_type, _value) _return(ctx, (_type) _value); return
