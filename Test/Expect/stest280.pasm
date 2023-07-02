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
	add	fp, #4
	wrlong	arg01, fp
	add	fp, #4
	mov	arg01, fp
	sub	fp, #8
	mov	arg02, #0
	mov	arg03, #40
LR__0001
	wrbyte	arg02, arg01
	add	arg01, #1
	djnz	arg03, #LR__0001
	add	fp, #4
	rdlong	result1, fp
	shl	result1, #2
	add	fp, #4
	add	result1, fp
	rdlong	result1, result1
	sub	fp, #8
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
	add	fp, #4
	wrlong	arg01, fp
	add	fp, #4
	mov	arg01, fp
	sub	fp, #8
	mov	arg02, ptr__dat__
	mov	arg03, #8
	cmps	arg01, arg02 wc
 if_ae	jmp	#LR__0011
	mov	_var01, #8
LR__0010
	rdbyte	result1, arg02
	wrbyte	result1, arg01
	add	arg01, #1
	add	arg02, #1
	djnz	_var01, #LR__0010
	jmp	#LR__0014
LR__0011
	add	arg01, arg03
	add	arg02, arg03
	mov	_var02, arg03 wz
 if_e	jmp	#LR__0013
LR__0012
	sub	arg01, #1
	sub	arg02, #1
	rdbyte	_var01, arg02
	wrbyte	_var01, arg01
	djnz	_var02, #LR__0012
LR__0013
LR__0014
	add	fp, #4
	rdlong	result1, fp
	add	fp, #4
	add	result1, fp
	rdbyte	result1, result1
	sub	fp, #8
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
