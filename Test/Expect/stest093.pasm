con
	FFT_SIZE = 1024
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fillInput
	mov	_var01, #0
	mov	_var02, objptr
	mov	_var03, #0
	mov	_var04, imm_1023_
	mov	_var05, objptr
	add	_var05, imm_4096_
	add	_var04, _var05
	mov	_var05, imm_1024_
LR__0001
	wrlong	_var01, _var02
	wrbyte	_var03, _var04
	add	_var01, #13
	add	_var02, #4
	add	_var03, #17
	sub	_var04, #1
	djnz	_var05, #LR__0001
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
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
	fit	496
