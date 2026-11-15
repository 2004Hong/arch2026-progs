#include <stdint.h>

/*
 * Provide the low 64 bits of a 64-by-64-bit product without multiplication.
 * Explicit 32-bit parts prevent GCC -Os from replacing a compact 64-bit
 * loop with a recursive call to this same helper.
 */
uint64_t __muldi3(uint64_t a, uint64_t b)
{
    /* Split each operand into low and high 32-bit parts. */
    uint32_t al = (uint32_t) a, ah = (uint32_t) (a >> 32);
    uint32_t bl = (uint32_t) b, bh = (uint32_t) (b >> 32);
    uint32_t low = 0, high = 0;

    while ((bl | bh) != 0) {
        if (bl & 1U) {
            /* Add the current multiplicand when the multiplier bit is set. */
            uint32_t old = low;
            low += al;
            high += ah;

            /* Unsigned wrap in the low part carries one into the high part. */
            high += (low < old);
        }

        /* Shift the entire multiplicand left, passing bit 31 into ah. */
        ah = (ah << 1) | (al >> 31);
        al <<= 1;

        /* Shift the entire multiplier right, passing bit 0 of bh into bl. */
        bl = (bl >> 1) | (bh << 31);
        bh >>= 1;
    }

    /* Combine the two result parts; any bits above bit 63 are discarded. */
    return ((uint64_t) high << 32) | low;
}
