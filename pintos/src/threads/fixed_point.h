#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>

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
