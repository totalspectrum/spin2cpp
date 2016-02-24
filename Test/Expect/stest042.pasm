DAT
	org	0
select
	cmps	select_x_, #0 wz
 if_ne	mov	select_tmp001_, select_y_
 if_eq	add	select_z_, #2
 if_eq	mov	select_tmp001_, select_z_
	mov	result_, select_tmp001_
select_ret
	ret

result_
	long	0
select_tmp001_
	long	0
select_x_
	long	0
select_y_
	long	0
select_z_
	long	0
