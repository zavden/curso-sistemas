/* calc_v2.h -- Public header for libcalc ABI v2 (INCOMPATIBLE with v1) */
#ifndef CALC_V2_H
#define CALC_V2_H

/* Changed signature: now takes 3 arguments (ABI break) */
int calc_add(int a, int b, int c);
int calc_mul(int a, int b);

#endif /* CALC_V2_H */
