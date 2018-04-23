CON
	FFT_SIZE = 1024
PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fillInput
	mov	_var_02, #0
	mov	_var_03, objptr
	mov	_var_04, #0
	mov	_var_06, imm_1023_
	mov	_tmp001_, objptr
	add	_tmp001_, imm_4096_
	add	_var_06, _tmp001_
	mov	_var_01, imm_1024_
L__0006
	wrlong	_var_02, _var_03
	wrbyte	_var_04, _var_06
	add	_var_02, #13
	add	_var_03, #4
	add	_var_04, #17
	sub	_var_06, #1
	djnz	_var_01, #L__0006
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
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[1280]
	org	COG_BSS_START
_tmp001_
	res	1
_var_01
	res	1
_var_02
	res	1
_var_03
	res	1
_var_04
	res	1
_var_06
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
