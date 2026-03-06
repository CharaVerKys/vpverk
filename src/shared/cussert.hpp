#pragma once // may be included not from defines.h
#include <source_location>

[[noreturn]] void cuabort(const char* info = nullptr);
[[noreturn]] void cuabort_loc(const char* info = nullptr, std::source_location const& loc = std::source_location::current());
[[noreturn]] void ___cuassertImplementation(const char* x, std::source_location const& loc = std::source_location::current());

#if defined NCUSSERT
    #define cussert(x) (void)0
#else
#define cussert(x) \
        if(not static_cast<bool>((x))){ \
            ___cuassertImplementation(#x); \
        }
#endif

#if defined NDEBUG 
    #define cussert_d(x) (void)0
#else
#define cussert_d(x) cussert(x)
#endif