#include <debug.h>
#include <random.h>
#include <stdio.h>
#include <string.h>

#define SHIFT (1<<14)

int											
convert_to_fixed (int n)  // convert integer to fixed point number
{
	return n * SHIFT;
}

int					
convert_to_int (int f)  // convert fixed point number to integer(round down)
{
	return f / SHIFT;
}

int
round_convert_to_int (int f) // covert fixed to integer(rounding)
{
	
	if(f>=0)
	{
		return (f + SHIFT/2) / SHIFT;
	}
	else
	{
		return (f - SHIFT/2) / SHIFT;
	}
}

int
add_fixed_fixed(int f1, int f2) // float + fixed
{
	return f1 + f2;
}

int
sub_fixed_fixed(int f1, int f2) // float - fixed
{
	return f1 - f2;
}

add_int_fixed(int n, int f)  // int + fixed
{
	return f + n * SHIFT;
}

sub_int_fixed(int n, int f)  // int - fixed
{
	return n * SHIFT - f;
}

sub_fixed_int(int f, int n) // fixed - int
{
	return f -  n * SHIFT;
}

mul_fixed_fixed(int f1, int f2)
{
	int64_t result = f1 * f2;
	return result / SHIFT;
}

mul_int_fixed(int n, int f)
{
	return n * f;
}

div_fixed_fixed(int f1, int f2)
{
	int64_t f = (int64_t) f1 ;
	return f / f2;
}

div_fixed_int(int f, int n)
{
	return f/n;
}
