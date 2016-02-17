stest027_hyp
	mov	mula_, stest027_hyp_x_
	mov	mulb_, stest027_hyp_x_
	call	#multiply_
	mov	stest027_hyp_tmp001_, mula_
	mov	mula_, stest027_hyp_y_
	mov	mulb_, stest027_hyp_y_
	call	#multiply_
	add	stest027_hyp_tmp001_, mula_
	mov	result_, stest027_hyp_tmp001_
stest027_hyp_ret
	ret

stest027_hyp_tmp001_
	long	0
stest027_hyp_x_
	long	0
stest027_hyp_y_
	long	0
result_
	long	0
