pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	add	arg01, #12
	rdlong	result1, arg01
_fetch_ret
	ret

_fetchv
	mov	fetchv_tmp002_, ptr__dat__
	mov	arg01, #20
	mov	arg02, #0
	call	#__system___gc_doalloc
	mov	arg02, result1 wz
 if_e	mov	result1, #0
 if_ne	mov	result1, arg02
	mov	arg01, result1
	mov	arg03, #20
	mov	arg02, arg01
	cmps	arg01, fetchv_tmp002_ wc
 if_ae	jmp	#LR__0002
	mov	__system____builtin_memmove__idx__0000, #20 wz
LR__0001
	rdbyte	result1, fetchv_tmp002_
	wrbyte	result1, arg01
	add	arg01, #1
	add	fetchv_tmp002_, #1
	djnz	__system____builtin_memmove__idx__0000, #LR__0001
	jmp	#LR__0005
LR__0002
	add	arg01, arg03
	add	fetchv_tmp002_, arg03
	mov	__system____builtin_memmove__idx__0001, arg03 wz
 if_e	jmp	#LR__0004
LR__0003
	sub	arg01, #1
	sub	fetchv_tmp002_, #1
	rdbyte	__system____builtin_memmove__idx__0000, fetchv_tmp002_
	wrbyte	__system____builtin_memmove__idx__0000, arg01
	djnz	__system____builtin_memmove__idx__0001, #LR__0003
LR__0004
LR__0005
	add	arg02, #12
	rdlong	result1, arg02
_fetchv_ret
	ret

__system____topofstack
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #12
	add	fp, #4
	wrlong	arg01, fp
	mov	result1, fp
	sub	fp, #4
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
__system____topofstack_ret
	ret

__system___gc_ptrs
	mov	_var01, __heap_ptr
	mov	_var02, _var01
	add	_var02, imm_1016_
	rdlong	result2, _var01 wz
 if_ne	jmp	#LR__0010
	mov	result2, _var02
	sub	result2, _var01
	mov	result1, #1
	wrword	result1, _var01
	mov	result1, _var01
	add	result1, #2
	mov	_var03, imm_27792_
	wrword	_var03, result1
	mov	result1, _var01
	add	result1, #4
	mov	_var03, #0
	wrword	_var03, result1
	mov	result1, _var01
	add	result1, #6
	mov	_var03, #1
	wrword	_var03, result1
	add	_var01, #16
	abs	_var03, result2 wc
	shr	_var03, #4
	negc	_var03, _var03
	wrword	_var03, _var01
	mov	result2, _var01
	add	result2, #2
	mov	_var03, imm_27791_
	wrword	_var03, result2
	mov	result2, _var01
	add	result2, #4
	mov	_var03, #0
	wrword	_var03, result2
	mov	result2, _var01
	add	result2, #6
	wrword	_var03, result2
	sub	_var01, #16
LR__0010
	mov	result2, _var02
	mov	result1, _var01
__system___gc_ptrs_ret
	ret

__system___gc_pageptr
	cmp	arg02, #0 wz
 if_e	mov	result1, #0
 if_ne	shl	arg02, #4
 if_ne	add	arg01, arg02
 if_ne	mov	result1, arg01
__system___gc_pageptr_ret
	ret

__system___gc_pageindex
	cmp	arg02, #0 wz
 if_e	mov	result1, #0
 if_ne	sub	arg02, arg01
 if_ne	shr	arg02, #4
 if_ne	mov	result1, arg02
__system___gc_pageindex_ret
	ret

__system___gc_isFree
	mov	result1, #0
	add	arg01, #2
	rdword	arg01, arg01
	cmp	arg01, imm_27791_ wz
 if_e	neg	result1, #1
__system___gc_isFree_ret
	ret

__system___gc_nextBlockPtr
	rdword	_var01, arg01 wz
 if_e	mov	result1, #0
 if_ne	shl	_var01, #4
 if_ne	add	arg01, _var01
 if_ne	mov	result1, arg01
__system___gc_nextBlockPtr_ret
	ret

__system___gc_tryalloc
	mov	__system___gc_tryalloc_size, arg01
	mov	__system___gc_tryalloc_reserveflag, arg02
	call	#__system___gc_ptrs
	mov	__system___gc_tryalloc_heap_base, result1
	mov	__system___gc_tryalloc_heap_end, result2
	mov	__system___gc_tryalloc_ptr, __system___gc_tryalloc_heap_base
	mov	__system___gc_tryalloc_availsize, #0
LR__0020
	mov	__system___gc_tryalloc_lastptr, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc_ptr, #6
	rdword	arg02, __system___gc_tryalloc_ptr
	mov	arg01, __system___gc_tryalloc_heap_base
	call	#__system___gc_pageptr
	mov	__system___gc_tryalloc_ptr, result1 wz
 if_ne	rdword	__system___gc_tryalloc_availsize, __system___gc_tryalloc_ptr
	cmp	__system___gc_tryalloc_ptr, #0 wz
 if_ne	cmps	__system___gc_tryalloc_ptr, __system___gc_tryalloc_heap_end wc
 if_a	jmp	#LR__0021
 if_ne	cmps	__system___gc_tryalloc_size, __system___gc_tryalloc_availsize wc,wz
 if_a	jmp	#LR__0020
LR__0021
	cmp	__system___gc_tryalloc_ptr, #0 wz
 if_e	mov	result1, __system___gc_tryalloc_ptr
 if_e	jmp	#__system___gc_tryalloc_ret
	mov	__system___gc_tryalloc__cse__0002, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0002, #6
	rdword	__system___gc_tryalloc_linkindex, __system___gc_tryalloc__cse__0002
	cmps	__system___gc_tryalloc_size, __system___gc_tryalloc_availsize wc
 if_ae	jmp	#LR__0023
	wrword	__system___gc_tryalloc_size, __system___gc_tryalloc_ptr
	mov	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_size
	shl	__system___gc_tryalloc_linkindex, #4
	mov	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_linkindex
	sub	__system___gc_tryalloc_availsize, __system___gc_tryalloc_size
	wrword	__system___gc_tryalloc_availsize, __system___gc_tryalloc_nextptr
	mov	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_nextptr
	add	__system___gc_tryalloc_linkindex, #2
	mov	__system___gc_tryalloc_availsize, imm_27791_
	wrword	__system___gc_tryalloc_availsize, __system___gc_tryalloc_linkindex
	mov	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_nextptr
	add	__system___gc_tryalloc_linkindex, #4
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc_linkindex
	mov	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_nextptr
	rdword	arg02, __system___gc_tryalloc__cse__0002
	add	__system___gc_tryalloc_linkindex, #6
	wrword	arg02, __system___gc_tryalloc_linkindex
	mov	__system___gc_tryalloc_saveptr, __system___gc_tryalloc_nextptr
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_saveptr
	call	#__system___gc_pageindex
	mov	__system___gc_tryalloc_linkindex, result1
	mov	arg01, __system___gc_tryalloc_nextptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_tryalloc_nextptr, result1 wz
 if_e	jmp	#LR__0022
	cmps	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_heap_end wc
 if_ae	jmp	#LR__0022
	add	__system___gc_tryalloc_nextptr, #4
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_saveptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc_nextptr
LR__0022
LR__0023
	add	__system___gc_tryalloc_lastptr, #6
	wrword	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_lastptr
	mov	__system___gc_tryalloc_saveptr, imm_27776_
	or	__system___gc_tryalloc_saveptr, __system___gc_tryalloc_reserveflag
	mov	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc_nextptr, #2
	cogid	result1
	or	__system___gc_tryalloc_saveptr, result1
	wrword	__system___gc_tryalloc_saveptr, __system___gc_tryalloc_nextptr
	mov	__system___gc_tryalloc_saveptr, __system___gc_tryalloc_heap_base
	add	__system___gc_tryalloc_saveptr, #8
	rdword	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_saveptr
	wrword	__system___gc_tryalloc_nextptr, __system___gc_tryalloc__cse__0002
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc_saveptr
	add	__system___gc_tryalloc_ptr, #8
	or	__system___gc_tryalloc_ptr, imm_1669332992_
	mov	result1, __system___gc_tryalloc_ptr
__system___gc_tryalloc_ret
	ret

__system___gc_doalloc
	mov	__system___gc_doalloc_size, arg01 wz
	mov	__system___gc_doalloc_reserveflag, arg02
 if_e	mov	result1, #0
 if_e	jmp	#__system___gc_doalloc_ret
	add	__system___gc_doalloc_size, #23
	andn	__system___gc_doalloc_size, #15
	shr	__system___gc_doalloc_size, #4
	add	ptr___system__dat__, #44
	mov	__system___gc_doalloc__cse__0005, ptr___system__dat__
	cogid	result1
	add	result1, #256
	sub	ptr___system__dat__, #44
LR__0030
	rdlong	arg01, __system___gc_doalloc__cse__0005 wz
 if_e	wrlong	result1, __system___gc_doalloc__cse__0005
 if_e	rdlong	arg01, __system___gc_doalloc__cse__0005
 if_e	rdlong	arg01, __system___gc_doalloc__cse__0005
	cmp	arg01, result1 wz
 if_ne	jmp	#LR__0030
	mov	arg01, __system___gc_doalloc_size
	mov	arg02, __system___gc_doalloc_reserveflag
	call	#__system___gc_tryalloc
	mov	__system___gc_doalloc_ptr, result1 wz
 if_ne	jmp	#LR__0031
	call	#__system___gc_docollect
	mov	arg01, __system___gc_doalloc_size
	mov	arg02, __system___gc_doalloc_reserveflag
	call	#__system___gc_tryalloc
	mov	__system___gc_doalloc_ptr, result1
LR__0031
	mov	__system___gc_doalloc_reserveflag, #0
	wrlong	__system___gc_doalloc_reserveflag, __system___gc_doalloc__cse__0005
	cmp	__system___gc_doalloc_ptr, #0 wz
 if_e	jmp	#LR__0034
	shl	__system___gc_doalloc_size, #4
	sub	__system___gc_doalloc_size, #8
	abs	_system___gc_doalloc_tmp001_, __system___gc_doalloc_size wc
	shr	_system___gc_doalloc_tmp001_, #2
	negc	__system___gc_doalloc__idx__0000, _system___gc_doalloc_tmp001_ wz
	mov	__system___gc_doalloc_zptr, __system___gc_doalloc_ptr
 if_e	jmp	#LR__0033
LR__0032
	mov	_system___gc_doalloc_tmp001_, #0
	wrlong	_system___gc_doalloc_tmp001_, __system___gc_doalloc_zptr
	add	__system___gc_doalloc_zptr, #4
	djnz	__system___gc_doalloc__idx__0000, #LR__0032
LR__0033
LR__0034
	mov	result1, __system___gc_doalloc_ptr
__system___gc_doalloc_ret
	ret

__system___gc_isvalidptr
	mov	_var01, arg03
	and	_var01, imm_4293918720_
	cmp	_var01, imm_1669332992_ wz
 if_ne	mov	result1, #0
 if_ne	jmp	#__system___gc_isvalidptr_ret
	sub	arg03, #8
	andn	arg03, imm_4293918720_
	cmps	arg03, arg01 wc
 if_b	jmp	#LR__0040
	cmps	arg03, arg02 wc
 if_b	jmp	#LR__0041
LR__0040
	mov	result1, #0
	jmp	#__system___gc_isvalidptr_ret
LR__0041
	mov	_var01, arg03
	xor	_var01, arg01
	test	_var01, #15 wz
 if_ne	mov	result1, #0
 if_ne	jmp	#__system___gc_isvalidptr_ret
	mov	_var01, arg03
	add	_var01, #2
	rdword	_var01, _var01
	and	_var01, imm_65472_
	cmp	_var01, imm_27776_ wz
 if_ne	mov	result1, #0
 if_e	mov	result1, arg03
__system___gc_isvalidptr_ret
	ret

__system___gc_docollect
	call	#__system___gc_ptrs
	mov	__system___gc_docollect_endheap, result2
	mov	__system___gc_docollect_startheap, result1
	mov	arg01, __system___gc_docollect_startheap
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect_ptr, result1 wz
	cogid	result1
	mov	__system___gc_docollect_ourid, result1
 if_e	jmp	#LR__0051
LR__0050
	cmps	__system___gc_docollect_ptr, __system___gc_docollect_endheap wc
 if_ae	jmp	#LR__0051
	mov	arg01, __system___gc_docollect_ptr
	add	arg01, #2
	rdword	__system___gc_docollect__cse__0001, arg01
	andn	__system___gc_docollect__cse__0001, #32
	wrword	__system___gc_docollect__cse__0001, arg01
	mov	arg01, __system___gc_docollect_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect_ptr, result1 wz
 if_ne	jmp	#LR__0050
LR__0051
	mov	arg01, #0
	call	#__system____topofstack
	mov	arg02, result1
	mov	arg01, #0
	mov	__system___gc_markhub_startaddr, #0
	mov	__system___gc_markhub_endaddr, arg02
	call	#__system___gc_ptrs
	mov	__system___gc_markhub_heap_base, result1
	mov	__system___gc_markhub_heap_end, result2
LR__0052
	cmps	__system___gc_markhub_startaddr, __system___gc_markhub_endaddr wc
 if_ae	jmp	#LR__0053
	rdlong	arg03, __system___gc_markhub_startaddr
	add	__system___gc_markhub_startaddr, #4
	mov	arg02, __system___gc_markhub_heap_end
	mov	arg01, __system___gc_markhub_heap_base
	call	#__system___gc_isvalidptr
	mov	__system___gc_markhub_ptr, result1 wz
 if_e	jmp	#LR__0052
	mov	arg01, __system___gc_markhub_ptr
	call	#__system___gc_isFree
	cmp	result1, #0 wz
 if_e	add	__system___gc_markhub_ptr, #2
 if_e	rdword	__system___gc_markhub_flags, __system___gc_markhub_ptr
 if_e	andn	__system___gc_markhub_flags, #15
 if_e	or	__system___gc_markhub_flags, #46
 if_e	wrword	__system___gc_markhub_flags, __system___gc_markhub_ptr
	jmp	#LR__0052
LR__0053
	call	#__system___gc_ptrs
	mov	__system___gc_markcog_heap_base, result1
	mov	__system___gc_markcog_heap_end, result2
	mov	__system___gc_markcog_cogaddr, #495
LR__0054
	'.live	__system___gc_markcog_ptr
	movs	wrcog, __system___gc_markcog_cogaddr
	movd	wrcog, #__system___gc_markcog_ptr
	call	#wrcog
	mov	arg02, __system___gc_markcog_heap_end
	mov	arg01, __system___gc_markcog_heap_base
	mov	arg03, __system___gc_markcog_ptr
	call	#__system___gc_isvalidptr
	mov	__system___gc_markcog_ptr, result1 wz
 if_ne	mov	__system___gc_markcog__cse__0000, __system___gc_markcog_ptr
 if_ne	add	__system___gc_markcog__cse__0000, #2
 if_ne	rdword	__system___gc_markcog__cse__0001, __system___gc_markcog__cse__0000
 if_ne	or	__system___gc_markcog__cse__0001, #32
 if_ne	wrword	__system___gc_markcog__cse__0001, __system___gc_markcog__cse__0000
	sub	__system___gc_markcog_cogaddr, #1
	cmps	__system___gc_markcog_cogaddr, #0 wc
 if_ae	jmp	#LR__0054
	mov	arg01, __system___gc_docollect_startheap
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect__cse__0001, result1 wz
 if_e	jmp	#__system___gc_docollect_ret
LR__0055
	mov	__system___gc_docollect_ptr, __system___gc_docollect__cse__0001
	mov	arg01, __system___gc_docollect_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect__cse__0001, result1
	mov	__system___gc_docollect_startheap, __system___gc_docollect_ptr
	add	__system___gc_docollect_startheap, #2
	rdword	__system___gc_docollect_startheap, __system___gc_docollect_startheap
	test	__system___gc_docollect_startheap, #32 wz
 if_e	test	__system___gc_docollect_startheap, #16 wz
 if_ne	jmp	#LR__0064
	and	__system___gc_docollect_startheap, #15
	cmp	__system___gc_docollect_startheap, __system___gc_docollect_ourid wz
 if_ne	cmp	__system___gc_docollect_startheap, #14 wz
 if_ne	jmp	#LR__0063
	mov	arg01, __system___gc_docollect_ptr
	mov	__system___gc_dofree_ptr, arg01
	call	#__system___gc_ptrs
	mov	__system___gc_dofree_heapend, result2
	mov	__system___gc_dofree_heapbase, result1
	mov	__system___gc_dofree__cse__0000, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0000, #2
	mov	result1, imm_27791_
	wrword	result1, __system___gc_dofree__cse__0000
	mov	__system___gc_dofree_prevptr, __system___gc_dofree_ptr
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1
LR__0056
	add	__system___gc_dofree_prevptr, #4
	rdword	arg02, __system___gc_dofree_prevptr
	mov	arg01, __system___gc_dofree_heapbase
	call	#__system___gc_pageptr
	mov	__system___gc_dofree_prevptr, result1 wz
 if_e	jmp	#LR__0057
	mov	arg01, __system___gc_dofree_prevptr
	call	#__system___gc_isFree
	cmp	result1, #0 wz
 if_e	jmp	#LR__0056
LR__0057
	cmp	__system___gc_dofree_prevptr, #0 wz
 if_e	mov	__system___gc_dofree_prevptr, __system___gc_dofree_heapbase
	mov	__system___gc_dofree__cse__0002, __system___gc_dofree_prevptr
	add	__system___gc_dofree__cse__0002, #6
	mov	__system___gc_dofree__cse__0003, __system___gc_dofree_ptr
	rdword	arg02, __system___gc_dofree__cse__0002
	add	__system___gc_dofree__cse__0003, #6
	wrword	arg02, __system___gc_dofree__cse__0003
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_dofree__cse__0002
	cmp	__system___gc_dofree_prevptr, __system___gc_dofree_heapbase wz
 if_e	jmp	#LR__0060
	mov	arg01, __system___gc_dofree_prevptr
	call	#__system___gc_nextBlockPtr
	cmp	result1, __system___gc_dofree_ptr wz
 if_ne	jmp	#LR__0059
	rdword	arg01, __system___gc_dofree_prevptr
	rdword	result1, __system___gc_dofree_ptr
	add	arg01, result1
	wrword	arg01, __system___gc_dofree_prevptr
	mov	__system___gc_dofree_nextptr, #0
	wrword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0000
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1
	cmps	__system___gc_dofree_nextptr, __system___gc_dofree_heapend wc
 if_ae	jmp	#LR__0058
	mov	__system___gc_dofree__cse__0000, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0000, #4
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_prevptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_dofree__cse__0000
LR__0058
	rdword	__system___gc_dofree__cse__0000, __system___gc_dofree__cse__0003
	wrword	__system___gc_dofree__cse__0000, __system___gc_dofree__cse__0002
	mov	__system___gc_dofree__cse__0002, #0
	wrword	__system___gc_dofree__cse__0002, __system___gc_dofree__cse__0003
	mov	__system___gc_dofree_ptr, __system___gc_dofree_prevptr
LR__0059
LR__0060
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree__cse__0003, result1 wz
 if_e	jmp	#LR__0062
	cmps	__system___gc_dofree__cse__0003, __system___gc_dofree_heapend wc
 if_ae	jmp	#LR__0062
	mov	arg01, __system___gc_dofree__cse__0003
	call	#__system___gc_isFree
	cmp	result1, #0 wz
 if_e	jmp	#LR__0062
	mov	__system___gc_dofree_prevptr, __system___gc_dofree_ptr
	rdword	__system___gc_dofree__cse__0002, __system___gc_dofree_prevptr
	mov	arg01, __system___gc_dofree__cse__0003
	rdword	__system___gc_dofree__cse__0003, arg01
	add	__system___gc_dofree__cse__0002, __system___gc_dofree__cse__0003
	wrword	__system___gc_dofree__cse__0002, __system___gc_dofree_prevptr
	mov	__system___gc_dofree_nextptr, arg01
	add	__system___gc_dofree_nextptr, #6
	mov	__system___gc_dofree__cse__0003, __system___gc_dofree_prevptr
	rdword	__system___gc_dofree__cse__0002, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0003, #6
	wrword	__system___gc_dofree__cse__0002, __system___gc_dofree__cse__0003
	mov	__system___gc_dofree__cse__0002, arg01
	add	__system___gc_dofree__cse__0002, #2
	mov	__system___gc_dofree__cse__0003, #170
	wrword	__system___gc_dofree__cse__0003, __system___gc_dofree__cse__0002
	mov	__system___gc_dofree__cse__0003, #0
	wrword	__system___gc_dofree__cse__0003, __system___gc_dofree_nextptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1 wz
 if_e	jmp	#LR__0061
	cmps	__system___gc_dofree_nextptr, __system___gc_dofree_heapend wc
 if_ae	jmp	#LR__0061
	mov	__system___gc_dofree__cse__0003, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0003, #4
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_prevptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_dofree__cse__0003
LR__0061
LR__0062
	mov	__system___gc_docollect__cse__0001, __system___gc_dofree_nextptr
LR__0063
LR__0064
	cmp	__system___gc_docollect__cse__0001, #0 wz
 if_ne	cmps	__system___gc_docollect__cse__0001, __system___gc_docollect_endheap wc
 if_c_and_nz	jmp	#LR__0055
__system___gc_docollect_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

__heap_ptr
	long	@@@__heap_base
fp
	long	0
imm_1016_
	long	1016
imm_1669332992_
	long	1669332992
imm_27776_
	long	27776
imm_27791_
	long	27791
imm_27792_
	long	27792
imm_4293918720_
	long	-1048576
imm_65472_
	long	65472
ptr_L__0023_
	long	@@@LR__0070
ptr_L__0034_
	long	@@@LR__0071
ptr_L__0060_
	long	@@@LR__0072
ptr___system__dat__
	long	@@@__system__dat_
ptr__dat__
	long	@@@_dat_
result1
	long	0
result2
	long	1
sp
	long	@@@stackspace
COG_BSS_START
	fit	496

LR__0070
	byte	" !!! corrupted heap??? !!! "
	byte	0
LR__0071
	byte	" !!! out of heap memory !!! "
	byte	0
LR__0072
	byte	" !!! corrupted heap !!! "
	byte	0
	long
_dat_
	byte	$00[20]
	long
__system__dat_
	byte	$00, $00, $00, $00, $f0, $09, $bc, $0a, $00, $00, $68, $5c, $01, $08, $fc, $0c
	byte	$03, $08, $7c, $0c, $00, $00, $00, $00, $03, $00, $00, $00, $00, $00, $00, $00
	byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
__heap_base
	long	0[258]
stackspace
	long	0[1]
	org	COG_BSS_START
__system____builtin_memmove__idx__0000
	res	1
__system____builtin_memmove__idx__0001
	res	1
__system___gc_doalloc__cse__0005
	res	1
__system___gc_doalloc__idx__0000
	res	1
__system___gc_doalloc_ptr
	res	1
__system___gc_doalloc_reserveflag
	res	1
__system___gc_doalloc_size
	res	1
__system___gc_doalloc_zptr
	res	1
__system___gc_docollect__cse__0001
	res	1
__system___gc_docollect_endheap
	res	1
__system___gc_docollect_ourid
	res	1
__system___gc_docollect_ptr
	res	1
__system___gc_docollect_startheap
	res	1
__system___gc_dofree__cse__0000
	res	1
__system___gc_dofree__cse__0002
	res	1
__system___gc_dofree__cse__0003
	res	1
__system___gc_dofree_heapbase
	res	1
__system___gc_dofree_heapend
	res	1
__system___gc_dofree_nextptr
	res	1
__system___gc_dofree_prevptr
	res	1
__system___gc_dofree_ptr
	res	1
__system___gc_markcog__cse__0000
	res	1
__system___gc_markcog__cse__0001
	res	1
__system___gc_markcog_cogaddr
	res	1
__system___gc_markcog_heap_base
	res	1
__system___gc_markcog_heap_end
	res	1
__system___gc_markcog_ptr
	res	1
__system___gc_markhub_endaddr
	res	1
__system___gc_markhub_flags
	res	1
__system___gc_markhub_heap_base
	res	1
__system___gc_markhub_heap_end
	res	1
__system___gc_markhub_ptr
	res	1
__system___gc_markhub_startaddr
	res	1
__system___gc_tryalloc__cse__0002
	res	1
__system___gc_tryalloc_availsize
	res	1
__system___gc_tryalloc_heap_base
	res	1
__system___gc_tryalloc_heap_end
	res	1
__system___gc_tryalloc_lastptr
	res	1
__system___gc_tryalloc_linkindex
	res	1
__system___gc_tryalloc_nextptr
	res	1
__system___gc_tryalloc_ptr
	res	1
__system___gc_tryalloc_reserveflag
	res	1
__system___gc_tryalloc_saveptr
	res	1
__system___gc_tryalloc_size
	res	1
_system___gc_doalloc_tmp001_
	res	1
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
fetchv_tmp002_
	res	1
	fit	496
