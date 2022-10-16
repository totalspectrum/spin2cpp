'' Compilation of various basic operators
VAR
byte b
word w
long l

PUB assign_value
' That narrow-assign-as-expresion thing
l := w := b := -1

PUB normal_binary | x,y,z
' Normal binary operators
x := y+z
x := y-z
x := y*z
x := y**z
x := y/z
x := y//z
x := y&z
x := y|z
x := y^z
x := y AND z
x := y OR z
x := y << z
x := y >> z
x := y -> z
x := y <- z
x := y ~> z
x := y >< z
x := y == z
x := y <> z
x := y < z
x := y =< z
x := y > z
x := y => z
x := y <# z
x := y #> z

PUB normal_unary |x,y
x := -y
x := NOT y
x := ||y
x := ^^y
x := |<y
x := >|y

PUB binary_assign_fuse | x,y,z
' Check for assign optimization
z := z - 1
y := 1 - y
x := 25 * x
x := 4 * x ' should convert to shift

PUB binary_emulated | x,y,z
' Emulated binary operators (can't do unsigned in Spin1 mode...)
x := y XOR z
x := y +| z
x := y -| z
x := y __andthen__ ina++
x := y __orelse__ ina++

PUB selfmodify | x,y,z

~z
~~z
~b
~~b
~w
~~w
~l
~~l
~l.word[1]

z++
z--
b++
b--
w++
w--
l++
l--
w.byte[0]++
w.byte[1]--

l?
?l

'' Arbitrary signx
z +|= 15
z -|= 15
z +|= 7
z -|= 7
z +|= 3
z -|= 3 ' should emulate!
x := z -| 7 ' should emulate!

'' Inside expressions
z := ~w
y := (b++) - 1
x := (?l) & 15

return ~~l


