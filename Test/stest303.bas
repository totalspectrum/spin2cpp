 
dim ct as integer

sub for "noinline" uninlinable(lmao as ulong,str as string)
end sub

sub tryit2(linetype as ulong)
dim varname$ as string
dim suffix$ as string
ct=99
if linetype=0 then
  varname$="part0" : ct = 2
end if
if linetype=1 then
  varname$="part1" : ct = 3
end if
'print "called tryit with linetype",linetype
suffix$=varname$ ' right$(varname$,1)
uninlinable(ct,"aha")
uninlinable(ct,suffix$)
end sub

tryit2(0)
tryit2(1)
tryit2(2)
