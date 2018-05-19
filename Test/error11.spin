''
'' check for instructions that should set flags not doing so
''
DAT

entry
    cmp par, #0
 if_z jmp #entry
    cmp cnt, #1 wz
 if_z jmp #entry

