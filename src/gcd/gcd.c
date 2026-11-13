#include "gcd.h"
/*
 * Compute the unsigned 32-bit GCD using the binary GCD algorithm.
 * Return the other operand for a zero input, including gcd(0, 0) = 0.
 */
uint32_t gcd_u32(uint32_t a, uint32_t b)
{
    /* Handle zero before the shift loops*/
    if (a == 0)
        return b;
    /* Handle zero before the shift loops*/
    if (b == 0)
        return a;

    unsigned shift = 0;
    /* Remove same factors of two and count them for later restoration. */
    while (((a | b) & 1U) == 0) {
        a >>= 1;
        b >>= 1;
        ++shift;
    }

    /* At least one operand is odd, so removing factors of two from a is safe.
     */
    while ((a & 1U) == 0)
        a >>= 1;
    /* a is odd and b is nonzero; removing factors of two preserves GCD. */
    do {
        while ((b & 1U) == 0)
            b >>= 1;
        /*make bigger one the first  */
        if (a > b) {
            uint32_t temp = a;
            a = b;
            b = temp;
        }
        /* gcd(a, b) = gcd(a, b - a) */
        b -= a;
    } while (b != 0);
    /* multiple the result by the saved shift */
    return a << shift;
}
