    .text
    .align 2
    .globl bf16_from_f32_asm
    .type bf16_from_f32_asm,@function

# Input: a0 = FP32 bit pattern. Output: a0 = BF16 bit pattern.
bf16_from_f32_asm:
    # For infinity and NaN, keep the upper 16 bits unchanged.
    srli t0,a0,23
    andi t0,t0,255
    li t1,255
    beq t0,t1,.Lf32_shift

    # Round to nearest; ties select an even retained low bit.
    srli t0,a0,16
    andi t0,t0,1
    li t1,0x7fff
    add a0,a0,t1
    add a0,a0,t0
.Lf32_shift:
    srli a0,a0,16
    ret
    .size bf16_from_f32_asm,.-bf16_from_f32_asm

    .globl bf16_mul_asm
    .type bf16_mul_asm,@function

# Inputs: a0, a1 = BF16 operands. Output: a0 = BF16 product.
# s0/s1: original operands; s2: result sign.
# s3/s4: effective exponents; s5/s6: integer significands.
bf16_mul_asm:
    # Preserve callee-saved registers and ra across __mulsi3.
    addi sp,sp,-32
    sw ra,28(sp)
    sw s0,24(sp)
    sw s1,20(sp)
    sw s2,16(sp)
    sw s3,12(sp)
    sw s4,8(sp)
    sw s5,4(sp)
    sw s6,0(sp)

    mv s0,a0
    mv s1,a1
    xor s2,a0,a1
    li t0,0x8000
    and s2,s2,t0
    srli s3,a0,7
    andi s3,s3,255
    srli s4,a1,7
    andi s4,s4,255
    andi s5,a0,127
    andi s6,a1,127

    # Check special operands before handling finite zeros.
    li t0,255
    beq s3,t0,.Lmul_special_a
    beq s4,t0,.Lmul_special_b
    or t0,s3,s5
    beqz t0,.Lmul_zero
    or t0,s4,s6
    beqz t0,.Lmul_zero

    # Normalize subnormals by shifting and reducing the exponent.
    # Normal operands only need their implicit leading one restored.
    bnez s3,.Lmul_normal_a
    li s3,1
.Lmul_normalize_a:
    andi t0,s5,128
    bnez t0,.Lmul_prepare_b
    slli s5,s5,1
    addi s3,s3,-1
    j .Lmul_normalize_a
.Lmul_normal_a:
    ori s5,s5,128

.Lmul_prepare_b:
    bnez s4,.Lmul_normal_b
    li s4,1
.Lmul_normalize_b:
    andi t0,s6,128
    bnez t0,.Lmul_product
    slli s6,s6,1
    addi s4,s4,-1
    j .Lmul_normalize_b
.Lmul_normal_b:
    ori s6,s6,128

.Lmul_product:
    # Combine exponents and multiply the eight-bit significands.
    add s3,s3,s4
    addi s3,s3,-127
    mv a0,s5
    mv a1,s6
    call __mulsi3

    # Truncate the product and remove the implicit leading one.
    # Bit 15 requires an extra shift and exponent increment.
    li t0,0x8000
    and t0,a0,t0
    beqz t0,.Lmul_shift7
    srli a0,a0,8
    andi a0,a0,127
    addi s3,s3,1
    j .Lmul_range
.Lmul_shift7:
    srli a0,a0,7
    andi a0,a0,127

.Lmul_range:
    li t0,255
    bge s3,t0,.Lmul_inf
    bgtz s3,.Lmul_pack

    # Preserve the contract's underflow behavior.
    # Shift the fraction after removing the implicit leading one.
    li t0,-6
    blt s3,t0,.Lmul_zero
    li t0,1
    sub t0,t0,s3
    srl a0,a0,t0
    li s3,0

.Lmul_pack:
    # Combine sign, exponent and fraction into the BF16 encoding.
    slli t0,s3,7
    or a0,a0,t0
    or a0,a0,s2
    j .Lmul_return

    # Preserve operand order: infinite a takes precedence over NaN b.
.Lmul_special_a:
    bnez s5,.Lmul_return_a
    or t0,s4,s6
    beqz t0,.Lmul_nan
    j .Lmul_inf
.Lmul_special_b:
    bnez s6,.Lmul_return_b
    or t0,s3,s5
    beqz t0,.Lmul_nan
    j .Lmul_inf
.Lmul_return_a:
    mv a0,s0
    j .Lmul_return
.Lmul_return_b:
    mv a0,s1
    j .Lmul_return
.Lmul_nan:
    li a0,0x7fc0
    j .Lmul_return
.Lmul_zero:
    mv a0,s2
    j .Lmul_return
.Lmul_inf:
    li a0,0x7f80
    or a0,a0,s2

.Lmul_return:
    lw s6,0(sp)
    lw s5,4(sp)
    lw s4,8(sp)
    lw s3,12(sp)
    lw s2,16(sp)
    lw s1,20(sp)
    lw s0,24(sp)
    lw ra,28(sp)
    addi sp,sp,32
    ret
    .size bf16_mul_asm,.-bf16_mul_asm

    .globl bf16_sqrt_asm
    .type bf16_sqrt_asm,@function

# Input: a0 = BF16 operand. Output: a0 = BF16 square root.
# s0: adjusted significand; s1: result exponent field.
# s2/s3: search bounds; s4: accepted result; s5: midpoint.
bf16_sqrt_asm:
    # Keep search values across calls to __mulsi3.
    addi sp,sp,-32
    sw ra,28(sp)
    sw s0,24(sp)
    sw s1,20(sp)
    sw s2,16(sp)
    sw s3,12(sp)
    sw s4,8(sp)
    sw s5,4(sp)

    srli t0,a0,15
    srli t1,a0,7
    andi t1,t1,255
    andi t2,a0,127

    # Handle NaNs and infinities before inspecting the sign.
    li t3,255
    beq t1,t3,.Lsqrt_special
    or t3,t1,t2
    beqz t3,.Lsqrt_zero
    bnez t0,.Lsqrt_nan

    # All positive subnormal inputs become positive zero.
    beqz t1,.Lsqrt_zero

    # Remove the bias and restore the implicit leading one.
    addi t1,t1,-127
    ori s0,t2,128

    # Make the exponent even without changing the input value.
    andi t3,t1,1
    beqz t3,.Lsqrt_halve
    slli s0,s0,1
    addi t1,t1,-1

.Lsqrt_halve:
    srai s1,t1,1
    addi s1,s1,127
    li s2,90
    li s3,256
    li s4,128

.Lsqrt_search:
    # Find the largest q satisfying floor(q*q / 128) <= s0.
    bltu s3,s2,.Lsqrt_normalize
    add s5,s2,s3
    srli s5,s5,1
    mv a0,s5
    mv a1,s5
    call __mulsi3
    srli a0,a0,7

    bltu s0,a0,.Lsqrt_high
    mv s4,s5
    addi s2,s5,1
    j .Lsqrt_search
.Lsqrt_high:
    addi s3,s5,-1
    j .Lsqrt_search

.Lsqrt_normalize:
    # Retain the original normalization and exponent-range checks.
    li t0,256
    bltu s4,t0,.Lsqrt_check_low
    srli s4,s4,1
    addi s1,s1,1
    j .Lsqrt_range
.Lsqrt_check_low:
    li t0,128
    bgeu s4,t0,.Lsqrt_range
    li t1,1
    bge t1,s1,.Lsqrt_range
    slli s4,s4,1
    addi s1,s1,-1
    j .Lsqrt_check_low

.Lsqrt_range:
    li t0,255
    bge s1,t0,.Lsqrt_inf
    blez s1,.Lsqrt_zero

    # Store the exponent and seven fraction bits.
    slli a0,s1,7
    andi t0,s4,127
    or a0,a0,t0
    j .Lsqrt_return

.Lsqrt_special:
    # Preserve NaNs and positive infinity; negative infinity becomes NaN.
    bnez t2,.Lsqrt_return
    bnez t0,.Lsqrt_nan
    j .Lsqrt_return
.Lsqrt_inf:
    li a0,0x7f80
    j .Lsqrt_return
.Lsqrt_nan:
    li a0,0x7fc0
    j .Lsqrt_return
.Lsqrt_zero:
    # Return positive zero for both signed zeros and positive subnormals.
    li a0,0

.Lsqrt_return:
    lw s5,4(sp)
    lw s4,8(sp)
    lw s3,12(sp)
    lw s2,16(sp)
    lw s1,20(sp)
    lw s0,24(sp)
    lw ra,28(sp)
    addi sp,sp,32
    ret
    .size bf16_sqrt_asm,.-bf16_sqrt_asm

    .section .note.GNU-stack,"",@progbits
