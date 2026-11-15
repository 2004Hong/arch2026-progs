    .text
    .align 2
    .globl op1_asm
    .type op1_asm, @function

# Add modulo 2^32 and record the number of loop passes.
# Inputs: a0 = a, a1 = b, a2 = address of the pass counter.
# Output: a0 = sum; the pass count is stored through a2.
# Temporaries: t0 = carry, t1 = pass count.
op1_asm:
    li t1,0
    sw t1,0(a2)

.Lop1_loop:
    # Stop when no addition or carry remains.
    beqz a1,.Lop1_done

    # Separate the carry bits from the sum without carries.
    and t0,a0,a1
    slli t0,t0,1
    xor a0,a0,a1
    mv a1,t0

    # Update the pass count in memory after each iteration.
    addi t1,t1,1
    sw t1,0(a2)
    j .Lop1_loop

.Lop1_done:
    ret
    .size op1_asm,.-op1_asm

    .globl op2_asm
    .type op2_asm, @function

# Inputs: a0 = a, a1 = b.
# Output: a0 = a - b modulo 2^32. No temporary registers are used.
op2_asm:
    sub a0,a0,a1
    ret
    .size op2_asm,.-op2_asm

    .globl op3_asm
    .type op3_asm, @function

# Inputs: a0 = a, a1 = b.
# Output: a0 = floor of the exact average, without sum overflow.
# Temporary: t1 = sum of the individually shifted inputs.
# a1 is overwritten.
op3_asm:
    srli t1,a0,1
    and a0,a0,a1
    srli a1,a1,1
    add t1,t1,a1

    # Two odd inputs each lose 0.5 when shifted, so restore one.
    andi a0,a0,1
    add a0,t1,a0
    ret
    .size op3_asm,.-op3_asm

    .globl q_mul_asm
    .type q_mul_asm,@function

# Multiply two signed Q11.20 values and retain the low 32 result bits.
# Inputs: a0 = a, a1 = b. Output: a0 = rounded Q11.20 product.
# Registers:
#   a0, a1: input magnitudes after sign handling
#   a2: sign information from the original inputs
#   t1:t0: high and low parts of the current multiplicand
#   t2: remaining unsigned multiplier
#   t4:t3: high and low parts of the accumulated product
#   t5, t6: carry and shift temporaries
# All four functions are leaf functions and need no stack frames.
q_mul_asm:
    # The sign bit of the XOR indicates whether the product is negative.
    xor a2,a0,a1

    # Convert inputs to unsigned magnitudes, including INT32_MIN.
    bgez a0,.Lq_a_positive
    sub a0,zero,a0

.Lq_a_positive:
    bgez a1,.Lq_b_positive
    sub a1,zero,a1

.Lq_b_positive:
    mv t0,a0
    li t1,0
    mv t2,a1
    li t3,0
    li t4,0

.Lq_multiply:
    beqz t2,.Lq_apply_sign
    andi t5,t2,1
    beqz t5,.Lq_shift

    # Accumulate the multiplicand and propagate the low-part carry.
    add t3,t3,t0
    sltu t5,t3,t0
    add t4,t4,t1
    add t4,t4,t5

.Lq_shift:
    # Shift the 64-bit multiplicand left, preserving the crossing bit.
    srli t6,t0,31
    slli t1,t1,1
    or t1,t1,t6
    slli t0,t0,1

    # Advance to the next multiplier bit.
    srli t2,t2,1
    j .Lq_multiply

.Lq_apply_sign:
    bgez a2,.Lq_round

    # Negate the full 64-bit product using two's complement.
    not t3,t3
    not t4,t4
    addi t3,t3,1
    sltiu t5,t3,1
    add t4,t4,t5

.Lq_round:
    # Add 2^19 before scaling; exact halves round toward positive infinity.
    li t0,0x80000
    add t3,t3,t0
    sltu t5,t3,t0
    add t4,t4,t5

    # Extract bits 20 through 51 as the low 32 bits of the scaled result.
    srli a0,t3,20
    slli t4,t4,12
    or a0,a0,t4
    ret

    .size q_mul_asm,.-q_mul_asm
    .section .note.GNU-stack,"",@progbits
