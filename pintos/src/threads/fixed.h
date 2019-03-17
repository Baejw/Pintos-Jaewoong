#ifndef THREAD_FIXED
#define THREAD_FIXED

#define Q 14
#define SHIFT 1<<(Q)

#define ADD(x,n) (x)+(n) * (SHIFT)
#define SUB(x,n) (x)-(n) * (SHIFT)
#define CON_FIX(x) (x) * (SHIFT)
#define CON_INT(x) (x) / (SHIFT)
#define CON_INT_N(x) ((x)>0 ? ((x)+SHIFT/2)/(SHIFT) : ((x)-SHIFT/2)/(SHIFT))
#define MUL(x,n) ((int64_t)(x)) * (n) / (SHIFT)
#define DIV(x,n) ((int64_t)(x)) * (SHIFT) / (n)
#endif
