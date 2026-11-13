    .text
    .align 2
    .globl gcd_u32_asm
    .type gcd_u32_asm, @function

# Compute the unsigned 32-bit GCD using the binary GCD algorithm.
# Inputs: a0 = a, a1 = b.
# Output: a0 = GCD; gcd(0, 0) = 0.
# Registers:
#   a0, a1: current operands
#   t0: number of shared factors of two
#   t1: temporary value for parity checks
#   t2: temporary value for swapping operands
# Leaf function: no stack frame or saved registers are needed.
gcd_u32_asm:
    # Handle zero before the shift loops, which would not terminate for zero.
    beq     a0, zero, .Lreturn_b
    beq     a1, zero, .Ldone

    li      t0, 0

.Lboth:
    # The low bit of a0 | a1 is zero only when both operands are even.
    or      t1, a0, a1
    andi    t1, t1, 1
    bne     t1, zero, .Lleft

    # Remove one shared factor of two and record it.
    srli    a0, a0, 1
    srli    a1, a1, 1
    addi    t0, t0, 1
    j       .Lboth

.Lleft:
    # At least one operand is odd; strip any remaining factors of two from a0.
    andi    t1, a0, 1
    bne     t1, zero, .Lright

    srli    a0, a0, 1
    j       .Lleft

.Lright:
    # a0 is odd and a1 is nonzero, so stripping factors of two preserves GCD.
    andi    t1, a1, 1
    bne     t1, zero, .Lorder

    srli    a1, a1, 1
    j       .Lright

.Lorder:
    # Use unsigned comparison to ensure a0 <= a1 before subtraction.
    bgeu    a1, a0, .Lsubtract

    mv      t2, a0
    mv      a0, a1
    mv      a1, t2

.Lsubtract:
    # Subtraction preserves GCD. Continue only for a nonzero difference.
    sub     a1, a1, a0
    bne     a1, zero, .Lright

    # a0 holds the reduced GCD; restore the shared factors of two.
    sll     a0, a0, t0
    ret

.Lreturn_b:
    # gcd(0, b) = b, including the case where both inputs are zero.
    mv      a0, a1

.Ldone:
    ret

    .size gcd_u32_asm, .-gcd_u32_asm
