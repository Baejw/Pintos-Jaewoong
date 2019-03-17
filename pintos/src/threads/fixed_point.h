#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>

#define SHIFT 1<<(14)

#define ADD(x,n) (x)+(n) * (SHIFT)
#define SUB(x,n) (x)-(n) * (SHIFT)
#define CON_FIX(x) (x) * (SHIFT)
#define CON_INT(x) (x) / (SHIFT)
#define CON_INT_N(x) ((x)>0 ? ((x)+SHIFT/2)/(SHIFT) : ((x)-SHIFT/2)/(SHIFT))
#define MUL(x,n) ((int64_t)(x)) * (n) / (SHIFT)
#define DIV(x,n) ((int64_t)(x)) * (SHIFT) / (n)

int conver_to_fixed (int);
int covert_to_int (int);
int round_convert_to_int(int);

int add_fixed_fixed(int, int);
int add_int_fixed(int, int);

int sub_fixed_fixed(int, int);
int sub_int_fixed(int, int);
int sub_fixed_int(int, int);

int mul_fixed_fixed(int, int);
int mul_int_fixed(int, int);

int div_fixed_fixed(int, int);
int div_fixed_int(int, int);

#endif
