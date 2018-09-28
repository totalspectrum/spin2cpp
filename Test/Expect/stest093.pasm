CON
	FFT_SIZE = 1024
PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fillInput
	mov	_var_01, #0
	mov	_var_02, objptr
	mov	_var_03, #0
	mov	_var_05, imm_1023_
	mov	_tmp002_, objptr
	add	_tmp002_, imm_4096_
	add	_var_05, _tmp002_
	mov	_var_00, imm_1024_
LR__0001
	wrlong	_var_01, _var_02
	wrbyte	_var_03, _var_05
	add	_var_01, #13
	add	_var_02, #4
	add	_var_03, #17
	sub	_var_05, #1
	djnz	_var_00, #LR__0001
_fillInput_ret
	ret

imm_1023_
	long	1023
imm_1024_
	long	1024
imm_4096_
	long	4096
objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[1280]
	org	COG_BSS_START
_tmp002_
	res	1
_var_00
	res	1
_var_01
	res	1
_var_02
	res	1
_var_03
	res	1
_var_05
	res	1
	fit	496
