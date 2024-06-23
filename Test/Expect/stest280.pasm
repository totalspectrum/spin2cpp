pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah1
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #48
	mov	_var01, arg01
	mov	arg01, fp
	add	arg01, #8
	mov	arg02, #0
	mov	arg03, #40
LR__0001
	wrbyte	arg02, arg01
	add	arg01, #1
	djnz	arg03, #LR__0001
	shl	_var01, #2
	mov	result1, fp
	add	result1, #8
	add	_var01, result1
	rdlong	result1, _var01
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_blah1_ret
	ret

_blah2
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #20
	mov	_var01, arg01
	mov	arg01, fp
	add	arg01, #8
	mov	arg02, ptr__dat__
	mov	arg03, #8
	cmps	arg01, arg02 wc
 if_ae	jmp	#LR__0011
	mov	_var02, #8
LR__0010
	rdbyte	result1, arg02
	wrbyte	result1, arg01
	add	arg01, #1
	add	arg02, #1
	djnz	_var02, #LR__0010
	jmp	#LR__0014
LR__0011
	add	arg01, arg03
	add	arg02, arg03
	mov	_var03, arg03 wz
 if_e	jmp	#LR__0013
LR__0012
	sub	arg01, #1
	sub	arg02, #1
	rdbyte	_var02, arg02
	wrbyte	_var02, arg01
	djnz	_var03, #LR__0012
LR__0013
LR__0014
	mov	result1, fp
	add	result1, #8
	add	_var01, result1
	rdbyte	result1, _var01
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_blah2_ret
	ret

fp
	long	0
ptr__dat__
	long	@@@_dat_
result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
	long
_dat_
	byte	$01, $02, $03, $04, $05, $06, $07, $08
stackspace
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
