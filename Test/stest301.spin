' Test for ApplyConditionAfter behaviors with certain special flag instrs

PUB rcl_nc_const(x) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        rcl x,#4
        mov r,x
  endasm

PUB rcl_c_const(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        rcl x,#4
        mov r,x
  endasm

PUB rcr_nc_const(x) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        rcr x,#4
        mov r,x
  endasm

PUB rcr_c_const(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        rcr x,#4
        mov r,x
  endasm

PUB rcl_nc_dyn(x,s) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        rcl x,s
        mov r,x
  endasm

PUB rcl_c_dyn(x,s) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        rcl x,s
        mov r,x
  endasm

PUB rcl_c_fold() : r
  asm
        mov r,#2
        cmp r,#100 wc
        rcl r,#4
  endasm

PUB rcl_nc_fold() : r
  asm
        mov r,#2
        cmp r,#0 wc
        rcl r,#4
  endasm

PUB rcl_c_wz(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        rcl x,#4 wz
        negz r,x
  endasm

PUB rcl_nc_wz(x) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        rcl x,#4 wz
        negz r,x
  endasm

PUB addx_nc_const(x) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        addx x,#1
        mov r,x
  endasm

PUB addx_c_const(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        addx x,#1
        mov r,x
  endasm

PUB subx_nc_const(x) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        subx x,#1
        mov r,x
  endasm

PUB subx_c_const(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        subx x,#1
        mov r,x
  endasm

PUB addx_c_const_noop(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        neg tmp,#1
        addx x,tmp
        mov r,x
  endasm

PUB addx_c_const_noop2(x) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        neg tmp,#1
        addx x,tmp wc
    if_c or x,#123
        mov r,x
  endasm

PUB addx_nc_dyn(x,b) : r | tmp
  asm
        mov tmp,#2
        cmp tmp,#1 wc
        addx x,b
        mov r,x
  endasm

PUB addx_c_dyn(x,b) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        addx x,b
        mov r,x
  endasm

PUB addx_c_full_fold : l,h  | tmp, a,b
  '' does not work yet
  asm
        neg l,#16
        mov h,#8
        mov a,#15
        mov b,#1
        mov tmp,#0
        cmp tmp,#1 wc
        add l,a wc
        addx h,b
  endasm

PUB bad_instr(x,b) : r | tmp
  asm
        mov tmp,#0
        cmp tmp,#1 wc
        addsx x,b
        subsx x,b
        rcl x,b wc
        mov r,x
  endasm