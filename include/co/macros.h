/**
 * @file macros.h
 * @author wotsen (astralrovers@outlook.com)
 * @brief 
 * @date 2020-11-14
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef __co_MACROS_H__
#define __co_MACROS_H__

#include <limits.h>

// 结构体指定对齐
#define st_packed __attribute__ ((packed))
// 标识符转字符串
#define sym_str(expr) #expr
// 数字宏定义转字符串
#define number_macro_str(num) sym_str(num)

///< 向上对齐
#define num_round_up(v, align) (((v) + (align) - 1) & ~((align) - 1))

///< bit位操作
#define setbit(a, n) (a[(n) >> 3] |= (1 << (n & (CHAR_BIT - 1))))
#define clrbit(a, n) (a[(n) >> 3] &= ~(1 << (n & (CHAR_BIT - 1))))
#define getbit(a, n) (a[(n) >> 3] & (1 << (n & (CHAR_BIT - 1))))

#endif // !__co_MACROS_H__