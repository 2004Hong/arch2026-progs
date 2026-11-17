#include "bf16.h"
typedef struct {
    uint16_t bits;
} bf16_t;

#define BF16_EXP_BIAS 127
#define BF16_NAN() ((bf16_t) {.bits = 0x7FC0U})
#define BF16_ZERO() ((bf16_t) {.bits = 0x0000U})

/* Receive the FP32 bit pattern without floating-point arithmetic. */
uint16_t bf16_from_f32(uint32_t f32bits)
{
    /* For infinity and NaN, keep the upper 16 bits unchanged. */
    if (((f32bits >> 23) & 0xFFU) == 0xFFU)
        return (uint16_t) (f32bits >> 16);

    /* Round to nearest, with ties to an even retained low bit. */
    f32bits += ((f32bits >> 16) & 1U) + 0x7FFFU;
    return (uint16_t) (f32bits >> 16);
}

static bf16_t multiply_value(bf16_t a, bf16_t b)
{
    uint16_t sign_a = (a.bits >> 15) & 1;
    uint16_t sign_b = (b.bits >> 15) & 1;
    int16_t exp_a = ((a.bits >> 7) & 0xFF);
    int16_t exp_b = ((b.bits >> 7) & 0xFF);
    uint16_t mant_a = a.bits & 0x7F;
    uint16_t mant_b = b.bits & 0x7F;

    uint16_t result_sign = sign_a ^ sign_b;

    /* Preserve operand order: infinite a takes precedence over NaN b. */
    if (exp_a == 0xFF) {
        if (mant_a)
            return a;
        if (!exp_b && !mant_b)
            return BF16_NAN();
        return (bf16_t) {.bits = (result_sign << 15) | 0x7F80};
    }
    if (exp_b == 0xFF) {
        if (mant_b)
            return b;
        if (!exp_a && !mant_a)
            return BF16_NAN();
        return (bf16_t) {.bits = (result_sign << 15) | 0x7F80};
    }
    if ((!exp_a && !mant_a) || (!exp_b && !mant_b))
        return (bf16_t) {.bits = result_sign << 15};

    /* Restore the leading one for normal operands.
     * Normalize subnormals and record their exponent adjustment.
     */
    int16_t exp_adjust = 0;
    if (!exp_a) {
        while (!(mant_a & 0x80)) {
            mant_a <<= 1;
            exp_adjust--;
        }
        exp_a = 1;
    } else
        mant_a |= 0x80;
    if (!exp_b) {
        while (!(mant_b & 0x80)) {
            mant_b <<= 1;
            exp_adjust--;
        }
        exp_b = 1;
    } else
        mant_b |= 0x80;

    uint32_t result_mant = (uint32_t) mant_a * mant_b;
    int32_t result_exp = (int32_t) exp_a + exp_b - BF16_EXP_BIAS + exp_adjust;

    /* Truncate the product and remove the implicit leading one.
     * Bit 15 requires an extra shift and exponent increment.
     */
    if (result_mant & 0x8000) {
        result_mant = (result_mant >> 8) & 0x7F;
        result_exp++;
    } else
        result_mant = (result_mant >> 7) & 0x7F;

    if (result_exp >= 0xFF)
        return (bf16_t) {.bits = (result_sign << 15) | 0x7F80};

    /* Preserve the contract's underflow behavior:
     * shift the fraction after removing the implicit leading one.
     */
    if (result_exp <= 0) {
        if (result_exp < -6)
            return (bf16_t) {.bits = result_sign << 15};
        result_mant >>= (1 - result_exp);
        result_exp = 0;
    }

    return (bf16_t) {.bits = (result_sign << 15) | ((result_exp & 0xFF) << 7) |
                             (result_mant & 0x7F)};
}

static bf16_t square_root_value(bf16_t a)
{
    uint16_t sign = (a.bits >> 15) & 1;
    int16_t exp = (a.bits >> 7) & 0xFF;
    uint16_t mant = a.bits & 0x7F;

    /* Preserve NaNs and positive infinity; negative infinity becomes NaN. */
    if (exp == 0xFF) {
        if (mant)
            return a;
        if (sign)
            return BF16_NAN();
        return a;
    }

    /* Both signed zeros become positive zero. */
    if (!exp && !mant)
        return BF16_ZERO();

    if (sign)
        return BF16_NAN();

    /* All positive subnormal inputs become positive zero. */
    if (!exp)
        return BF16_ZERO();

    int32_t e = exp - BF16_EXP_BIAS;
    int32_t new_exp;
    uint32_t m = 0x80U | mant;

    /* Make the exponent even while preserving the input value,
     * then halve it and restore the bias.
     */
    if (e & 1) {
        m <<= 1;
        new_exp = ((e - 1) >> 1) + BF16_EXP_BIAS;
    } else {
        new_exp = (e >> 1) + BF16_EXP_BIAS;
    }

    /* Preserve floor(q*q / 128) <= m from the original search.
     * Its largest solution is floor(sqrt(128*m + 127)).
     */
    uint32_t remainder = (m << 7) + 127U;
    uint32_t result = 0;
    uint32_t bit = 0x4000U; /* Start with 128 squared. */

    /* Build the root from high bits to low bits without multiplication.
     * result holds a scaled intermediate value until the final step.
     */
    while (bit != 0) {
        uint32_t trial = result + bit;
        result >>= 1;

        /* Accept this root bit if the remainder covers the increase. */
        if (remainder >= trial) {
            remainder -= trial;
            result |= bit;
        }

        /* Halving the candidate root bit divides its square by four. */
        bit >>= 2;
    }

    /* Retain the original normalization and exponent-range checks. */
    if (result >= 256) {
        result >>= 1;
        new_exp++;
    } else if (result < 128) {
        while (result < 128 && new_exp > 1) {
            result <<= 1;
            new_exp--;
        }
    }

    /* Store only the fraction; the leading one is implicit. */
    uint16_t new_mant = result & 0x7F;

    if (new_exp >= 0xFF)
        return (bf16_t) {.bits = 0x7F80};

    if (new_exp <= 0)
        return BF16_ZERO();

    return (bf16_t) {.bits = ((new_exp & 0xFF) << 7) | new_mant};
}

/* Adapt the public bit-pattern interfaces to the internal representation. */
uint16_t bf16_mul(uint16_t a, uint16_t b)
{
    bf16_t operand_a = {.bits = a};
    bf16_t operand_b = {.bits = b};

    return multiply_value(operand_a, operand_b).bits;
}

uint16_t bf16_sqrt(uint16_t a)
{
    return square_root_value((bf16_t) {.bits = a}).bits;
}
