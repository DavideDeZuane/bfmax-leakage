/*
 * params.h - parametri del codice.
 */
#pragma once

/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* PARAMETERS                                                                                                                                       */
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */

#if !defined(TOY)
    #define P               (12323)
    #define N0              (2) 
    #define N               (N0 * P)
    #define V               (71)
    #define NUM_ERRORS_T    (134)
#else
    #define P               (4801)
    #define N0              (2)
    #define N               (N0 * P)
    #define V               (45)
    #define NUM_ERRORS_T    (84)
#endif

#define MAX_ITER        ((int)(1.5 * NUM_ERRORS_T))