#include "fixed.h"

/*
 * Add modulo 2^32 using bitwise operations.
 * Store the number of loop passes through n.
 */
uint32_t op1(uint32_t a, uint32_t b, int *n)
{
    *n = 0;
    while (b) {
        /* Separate the carry bits from the sum without carries. */
        uint32_t carry = (a & b) << 1;
        a ^= b;
        b = carry;
        ++*n;
    }
    return a;
}

/* Unsigned subtraction wraps modulo 2^32. */
uint32_t op2(uint32_t a, uint32_t b)
{
    return a - b;
}

/* Compute the floor of the average without overflowing the sum. */
uint32_t op3(uint32_t a, uint32_t b)
{
    /* Two odd inputs each lose 0.5 when shifted, so restore one. */
    return (a >> 1) + (b >> 1) + (a & b & 1U);
}

/* Multiply two Q11.20 values and retain the low 32 result bits. */
int32_t q_mul(int32_t a, int32_t b)
{
    /* Extend before multiplication to preserve the full signed product. */
    int64_t product = (int64_t) a * b;

    /* Restore the Q20 scale; exact halves round toward positive infinity. */
    return (int32_t) ((product + (INT64_C(1) << (Q - 1))) >> Q);
}
