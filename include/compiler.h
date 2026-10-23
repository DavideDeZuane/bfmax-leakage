#pragma once

/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* QUALIFIERS                                                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* Required for profiling, using -DSKIP_INLINE disable the inlining otherwhise in perf we won't see anything  */
#ifdef SKIP_INLINE
    #define INLINE
#else
    #define INLINE inline
#endif

/* Required for alignment for AVX registers */
#define ALIGNED alignas(32)
 
/* Documentation for parameters of functions */
#define IN                  
#define OUT                

/* For compatibility */
#define RESTRICT
#define CONST
 
/* For the endiness*/
#define BE 0      
#define LE 1     
 
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* DEBUG                                                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
#ifdef DEBUG
#include <stdio.h>
#define DEBUG_PRINT(...)                            \
    do {                                            \
        printf("\033[0;33m");                       \
        printf(__VA_ARGS__);                        \
        printf("\033[0m");                          \
    } while(0)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif
 
 