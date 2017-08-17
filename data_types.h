/**
  * @file data_types.h
  * @brief Contains the basic data type definitions used commonly in all modules.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (dgeromichalos)
  * @date August, 2017
  */

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#ifdef  __cplusplus
extern "C" {
#endif

/***************************** Type Definitions ******************************/

typedef unsigned char      u8_t;
typedef signed char        s8_t;
typedef unsigned short     u16_t;
typedef signed short       s16_t;
typedef unsigned int       u32_t;
typedef signed int         s32_t;
typedef unsigned long long u64_t;
typedef signed long long   s64_t;
typedef float              f32_t;
typedef double             f64_t;

#define U8_MAX  (255u)
#define U8_MIN  (0u)
#define S8_MAX  (127)
#define S8_MIN  (-128)
#define U16_MAX (65535u)
#define U16_MIN (0u)
#define S16_MAX (32767)
#define S16_MIN (-32768)
#define U32_MAX (4294967295u)
#define U32_MIN (0u)
#define S32_MAX (2147483647)
#define S32_MIN (-2147483648)
#define U64_MAX (18446744073709551615u)
#define U64_MIN (0u)
#define S64_MAX (9223372036854775807)
#define S64_MIN (-9223372036854775808)
#define F32_MIN (1.175494351e-38F)
#define F32_MAX (3.402823466e+38F)
#define F64_MIN (2.2250738585072014e-308)
#define F64_MAX (1.7976931348623158e+308)

#ifndef NULL
#define NULL ((void*)0u)
#endif

#ifndef TRUE
#define TRUE (1u)
#endif

#ifndef FALSE
#define FALSE (0u)
#endif

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif  /* DATA_TYPES_H */
