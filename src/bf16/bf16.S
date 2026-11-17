# Third optimization of the BF16 assembly.
# Remove the square-root stack frame and simplify its first step.
    .text
    .align 2
    .globl bf16_from_f32_asm
    .type bf16_from_f32_asm,@function

# Input: a0 = FP32 bit pattern. Output: a0 = BF16 bit pattern.
bf16_from_f32_asm:
    # Mask FP32 exponent bits 23-30 with 0x7F800000.
    lui t1,0x7f800
    and t0,a0,t1
    beq t0,t1,.Lf32_shift

    # Round to nearest, with ties to an even retained low bit.
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
    # Preserve used saved registers with 16-byte stack alignment.
    # No saved ra is needed because this function makes no calls.
    addi sp,sp,-32
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

    # Handle special operands before finite zeros.
    li t0,255
    beq s3,t0,.Lmul_special_a
    beq s4,t0,.Lmul_special_b
    or t0,s3,s5
    beqz t0,.Lmul_zero
    or t0,s4,s6
    beqz t0,.Lmul_zero

    # Normalize subnormals and adjust their effective exponents.
    # Normal operands only need the implicit leading one restored.
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
    add s3,s3,s4
    addi s3,s3,-127

    # Multiply the nonzero significands using a bottom-tested loop.
    mv t1,s5
    mv a1,s6
    li a0,0
.Lmul_accumulate:
    andi t0,a1,1
    beqz t0,.Lmul_advance
    add a0,a0,t1
.Lmul_advance:
    slli t1,t1,1
    srli a1,a1,1
    bnez a1,.Lmul_accumulate

    # The product fits in 16 bits; bit 15 selects the scaling shift.
    srli t0,a0,15
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

    # Preserve the contract's fraction-only underflow adjustment.
    li t0,-6
    blt s3,t0,.Lmul_zero
    li t0,1
    sub t0,t0,s3
    srl a0,a0,t0
    li s3,0

.Lmul_pack:
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
    addi sp,sp,32
    ret
    .size bf16_mul_asm,.-bf16_mul_asm

    .globl bf16_sqrt_asm
    .type bf16_sqrt_asm,@function

# Input: a0 = BF16 operand. Output: a0 = BF16 square root.
# a1: adjusted significand; a2: result exponent field; a3: root.
# Only caller-saved registers are used, so no stack frame is needed.
bf16_sqrt_asm:
    srli t0,a0,15
    srli t1,a0,7
    andi t1,t1,255
    andi t2,a0,127

    # Preserve special values, negative-input handling, and zero flushing.
    li t3,255
    beq t1,t3,.Lsqrt_special
    or t3,t1,t2
    beqz t3,.Lsqrt_zero
    bnez t0,.Lsqrt_nan
    beqz t1,.Lsqrt_zero

    # Restore the leading one and make the actual exponent even.
    addi t1,t1,-127
    ori a1,t2,128
    andi t3,t1,1
    beqz t3,.Lsqrt_halve
    slli a1,a1,1
    addi t1,t1,-1

.Lsqrt_halve:
    srai a2,t1,1
    addi a2,a2,127

    # Preserve the original search condition using target 128*m + 127.
    slli t0,a1,7
    addi t0,t0,127
    li t1,0

    # The target is at least 16511, so accepting 128 always succeeds.
    # Set the accumulator and subtract 128 squared without a comparison.
    li t1,16384
    sub t0,t0,t1

    # Remaining steps are unrolled.
    # t0: remainder; t1: scaled root accumulator; t3: trial increase.
.Lsqrt_step64:
    # Try adding 64: candidate squared = 4096.
    li t2,4096
    add t3,t1,t2
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_step32
    sub t0,t0,t3
    or t1,t1,t2

.Lsqrt_step32:
    # Smaller squared candidates fit in immediate instructions.
    addi t3,t1,1024
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_step16
    sub t0,t0,t3
    ori t1,t1,1024

.Lsqrt_step16:
    addi t3,t1,256
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_step8
    sub t0,t0,t3
    ori t1,t1,256

.Lsqrt_step8:
    addi t3,t1,64
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_step4
    sub t0,t0,t3
    ori t1,t1,64

.Lsqrt_step4:
    addi t3,t1,16
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_step2
    sub t0,t0,t3
    ori t1,t1,16

.Lsqrt_step2:
    addi t3,t1,4
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_step1
    sub t0,t0,t3
    ori t1,t1,4

.Lsqrt_step1:
    addi t3,t1,1
    srli t1,t1,1
    bltu t0,t3,.Lsqrt_digits_done
    sub t0,t0,t3
    ori t1,t1,1

.Lsqrt_digits_done:
    # The root is in [128,255] and the exponent field is in [64,190].
    # Pack the result directly without normalization or range checks.
    mv a3,t1
    slli a0,a2,7
    andi t0,a3,127
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
    li a0,0

.Lsqrt_return:
    ret
    .size bf16_sqrt_asm,.-bf16_sqrt_asm

    .section .note.GNU-stack,"",@progbits
