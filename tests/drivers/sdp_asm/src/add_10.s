	.file	"add_10.c"
	.option nopic
	.attribute arch, "rv32e1p9_m2p0_c2p0_zicsr2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 4
	.text
	.section	.text.hrt_add_10,"ax",@progbits
	.align	1
	.globl	hrt_add_10
	.type	hrt_add_10, @function
hrt_add_10:
	lui	a4,%hi(arg_uint8_t)
	lbu	a5,%lo(arg_uint8_t)(a4)
	addi	a5,a5,10
	andi	a5,a5,0xff
	sb	a5,%lo(arg_uint8_t)(a4)
	lui	a4,%hi(arg_uint16_t)
	lhu	a5,%lo(arg_uint16_t)(a4)
	addi	a5,a5,20
	slli	a5,a5,16
	srli	a5,a5,16
	sh	a5,%lo(arg_uint16_t)(a4)
	lui	a4,%hi(arg_uint32_t)
	lw	a5,%lo(arg_uint32_t)(a4)
	addi	a5,a5,30
	sw	a5,%lo(arg_uint32_t)(a4)
	ret
	.size	hrt_add_10, .-hrt_add_10
