obj
  mod: "module.inc"

dat
    org 0
entry
    mov $1f0, #mod.val
    mov mod.reg1, #mod.val
    mov $1f0, #mod#val
    mov mod#reg1, #5
    mov $1f0, #5

pub demo
  cognew(@entry, 0)

