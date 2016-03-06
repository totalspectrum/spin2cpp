DAT
	org	0

foo
	mov	foo__idx__0000_, #5
L_001_
	add	OUTA, #1
	djnz	foo__idx__0000_, #L_001_
foo_ret
	ret

foo__idx__0000_
	long	0
