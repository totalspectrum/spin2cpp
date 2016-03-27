PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_blinky
	mov	arg2, ptr__dat__
	mov	arg1, #8
	mov	arg3, #240
	call	#__system___coginit
_blinky_ret
	ret

__system___coginit
	and	arg3, imm_65532_
	shl	arg3, #16
	and	arg2, imm_65532_
	shl	arg2, #2
	or	arg3, arg2
	and	arg1, #15
	or	arg3, arg1
	coginit	arg3 wr
	mov	result1, arg3
__system___coginit_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
imm_65532_
	long	65532
ptr__dat__
	long	@@@_dat_
result1
	long	0
	fit	496
	long
_dat_
	long	$a0bc19f0
	long	$28fc1804
	long	$a0fc1a01
	long	$2cbc1a0c
	long	$a0bfec0d
	long	$a0bfe80d
	long	$08fc1c00
	long	$a0bc1e0e
	long	$80bc1ff1
	long	$f8bc1e0e
	long	$6cbfe80d
	long	$5c7c0009
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
