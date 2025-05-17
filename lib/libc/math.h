#ifndef _MATH_H
#define _MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#define fpclassify(x)        (__builtin_fpclassify(FP_NAN, FP_INFINITE, \
                                                   FP_NORMAL, FP_SUBNORMAL, \
                                                   FP_ZERO, x))
#define isinf(x)             (__builtin_isinf(x))
#define isnan(x)             (__builtin_isnan(x))
#define isnormal(x)          (__builtin_isnormal(x))
#define isfinite(x)          (__builtin_isfinite(x))
#define signbit(x)           (__builtin_signbit(x))
#define isunordered(x, y)    (__builtin_isunordered(x, y))
#define isless(x, y)         (__builtin_isless(x, y))
#define islessequal(x, y)    (__builtin_islessequal(x, y))
#define islessgreater(x, y)  (__builtin_islessgreater(x, y))
#define isgreater(x, y)      (__builtin_isgreater(x, y))
#define isgreaterequal(x, y) (__builtin_isgreaterequal(x, y))

#define ceil(x) (__builtin_ceil(x))
#define ceilf(x) (__builtin_ceilf(x))
#define copysign(x, y) (__builtin_copysign(x, y))
#define copysignf(x, y) (__builtin_copysignf(x, y))
#define fabs(x) (__builtin_fabs(x))
#define floorf(x) (__builtin_fabsf(x))
#define floor(x) (__builtin_floor(x))
#define fabsf(x) (__builtin_floorf(x))
#define nearbyint(x) (__builtin_nearbyint(x))
#define nearbyintf(x) (__builtin_nearbyintf(x))
#define rint(x) (__builtin_rint(x))
#define rintf(x) (__builtin_rintf(x))
#define sqrt(x) (__builtin_sqrt(x))
#define sqrtf(x) (__builtin_sqrtf(x))
#define trunc(x) (__builtin_trunc(x))
#define truncf(x) (__builtin_truncf(x))

double      fmax(double, double);
float       fmaxf(float, float);

double      fmin(double, double);
float       fminf(float, float);

#ifdef __cplusplus
}
#endif

#endif
