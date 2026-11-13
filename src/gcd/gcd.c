#include "gcd.h"

/*
 * Compute the unsigned 32-bit GCD using the binary GCD algorithm.
 * Return the other operand for a zero input, including gcd(0, 0) = 0.
 */
uint32_t gcd_u32(uint32_t a, uint32_t b)
{
    /* Handle zero before the shift loops, which never make zero odd. */
    if (a == 0)
        return b;

    if (b == 0)
        return a;

    unsigned shift = 0;

    /* Remove shared factors of two and count them for later restoration. */
    while (((a | b) & 1U) == 0) {
        a >>= 1;
        b >>= 1;
        ++shift;
    }

    /* At least one operand is odd; remove remaining factors of two from a. */
    while ((a & 1U) == 0)
        a >>= 1;

    do {
        /* a is odd and b is nonzero. An odd b needs no shift. */
        if ((b & 1U) == 0) {
            if ((b & 3U) != 0) {
                /* Low bits 10 mean exactly one trailing zero. */
                b >>= 1;
            } else {
                /*
                 * At least two trailing zeros remain.
                 * Remove them in groups of 16, 8, 4, 2, and 1 bits.
                 * Each check uses the value after any preceding shifts.
                 */
                if ((b & 0xFFFFU) == 0)
                    b >>= 16;
                if ((b & 0xFFU) == 0)
                    b >>= 8;
                if ((b & 0xFU) == 0)
                    b >>= 4;
                if ((b & 3U) == 0)
                    b >>= 2;
                if ((b & 1U) == 0)
                    b >>= 1;
            }
        }

        /* Both operands are odd. Ensure a <= b before subtraction. */
        if (a > b) {
            uint32_t temp = a;
            a = b;
            b = temp;
        }

        /* Subtraction preserves GCD. A zero difference ends the loop. */
        b -= a;
    } while (b != 0);

    /* Restore the shared factors of two removed at the beginning. */
    return a << shift;
}
