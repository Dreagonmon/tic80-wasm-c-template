#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#ifdef __cplusplus
}
#endif

#define assert(x) ((void)((x) || (abort(), 0)))
