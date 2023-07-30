pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_uninlinable
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #12
	wrlong	arg01, fp
	add	fp, #4
	wrlong	arg02, fp
	mov	outa, arg01
	mov	outa, fp
	sub	fp, #4
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_uninlinable_ret
	ret

_tryit2
	mov	arg02, arg01 wz
	mov	tryit2_tmp001_, #99
	wrlong	tryit2_tmp001_, objptr
 if_e	mov	_tryit2_varname_S, ptr_L__0003_
 if_e	mov	tryit2_tmp001_, #2
 if_e	wrlong	tryit2_tmp001_, objptr
	cmp	arg02, #1 wz
 if_e	mov	_tryit2_varname_S, ptr_L__0005_
 if_e	mov	tryit2_tmp001_, #3
 if_e	wrlong	tryit2_tmp001_, objptr
	rdlong	arg01, objptr
	mov	arg02, ptr_L__0006_
	call	#_uninlinable
	rdlong	arg01, objptr
	mov	arg02, _tryit2_varname_S
	call	#_uninlinable
_tryit2_ret
	ret

_program
	mov	arg01, #0
	call	#_tryit2
	mov	arg01, #1
	call	#_tryit2
	mov	arg01, #2
	call	#_tryit2
_program_ret
	ret

fp
	long	0
objptr
	long	@@@objmem
ptr_L__0003_
	long	@@@LR__0001
ptr_L__0005_
	long	@@@LR__0002
ptr_L__0006_
	long	@@@LR__0003
result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496

LR__0001
	byte	"part0"
	byte	0
LR__0002
	byte	"part1"
	byte	0
LR__0003
	byte	"aha"
	byte	0
objmem
	long	0[1]
stackspace
	long	0[1]
	org	COG_BSS_START
_tryit2_varname_S
	res	1
arg01
	res	1
arg02
	res	1
tryit2_tmp001_
	res	1
	fit	496
