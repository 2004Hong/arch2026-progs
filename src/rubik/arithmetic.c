/* Software division and remainder helpers for the RV32I C build. */
#include <stdint.h>
/* Divide bit by bit, returning the quotient and storing the remainder. */
static uint32_t udivide(uint32_t a, uint32_t b, uint32_t *remainder)
{
    uint32_t q = 0, r = 0;
    /* For a zero divisor, return all quotient bits set and retain a. */
    if (!b) {
        *remainder = a;
        return UINT32_MAX;
    }
    /* Bring down each dividend bit and subtract when the remainder permits. */
    for (unsigned i = 32; i--;) {
        r = (r << 1) | ((a >> i) & 1U);
        if (r >= b) {
            r -= b;
            q |= 1U << i;
        }
    }
    *remainder = r;
    return q;
}
uint32_t __udivsi3(uint32_t a, uint32_t b)
{
    uint32_t r;
    return udivide(a, b, &r);
}
uint32_t __umodsi3(uint32_t a, uint32_t b)
{
    uint32_t r;
    udivide(a, b, &r);
    return r;
}

/* Use unsigned magnitudes to handle INT32_MIN without signed negation.
 * The remainder has the sign of the dividend.
 */
int32_t __modsi3(int32_t a, int32_t b)
{
    uint32_t ua = (uint32_t) a, ub = (uint32_t) b;
    if (a < 0)
        ua = 0U - ua;
    if (b < 0)
        ub = 0U - ub;
    uint32_t rem = __umodsi3(ua, ub);
    return (int32_t) (a < 0 ? 0U - rem : rem);
}
