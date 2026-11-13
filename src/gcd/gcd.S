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
#   t1: temporary value for parity and trailing-zero checks
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
    # a0 is odd and a1 is nonzero. An odd a1 needs no shift.
    andi    t1, a1, 1
    bnez    t1, .Lorder

    # a1 is even here. Low bits 10 mean exactly one trailing zero;
    # low bits 00 mean at least two, so use the grouped checks below.
    andi    t1, a1, 3
    beqz    t1, .Lright_many
    srli    a1, a1, 1
    j       .Lorder

.Lright_many:
    # Remove trailing zeros in groups of 16, 8, 4, 2, and 1 bits.
    # Shifting a copy left by 16 tests whether the low 16 bits are zero.
    slli    t1, a1, 16
    bnez    t1, .Lright8
    srli    a1, a1, 16

.Lright8:
    # Test the low 8 bits of the current value.
    andi    t1, a1, 255
    bnez    t1, .Lright4
    srli    a1, a1, 8

.Lright4:
    # Test the low 4 bits after any earlier shifts.
    andi    t1, a1, 15
    bnez    t1, .Lright2
    srli    a1, a1, 4

.Lright2:
    # Remove two more factors of two if both low bits are zero.
    andi    t1, a1, 3
    bnez    t1, .Lright1
    srli    a1, a1, 2

.Lright1:
    # Remove the final factor of two, if present.
    andi    t1, a1, 1
    bnez    t1, .Lorder
    srli    a1, a1, 1

.Lorder:
    # Both operands are now odd. Ensure a0 <= a1 before subtraction.
    bgeu    a1, a0, .Lsubtract

    mv      t2, a0
    mv      a0, a1
    mv      a1, t2

.Lsubtract:
    # Subtraction preserves GCD. Continue only for a nonzero difference.
    sub     a1, a1, a0
    bne     a1, zero, .Lright

    # Restore the shared factors of two removed at the beginning.
    sll     a0, a0, t0
    ret

.Lreturn_b:
    # gcd(0, b) = b, including the case where both inputs are zero.
    mv      a0, a1

.Ldone:
    ret

    .size gcd_u32_asm, .-gcd_u32_asm
