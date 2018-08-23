# Fast Propeller Basic compiler

## Language Syntax

### Comments

Comments start with `rem` or a single quote character, and go to the end of line

### Keywords

```
as
continue
dim
do
else
end
exit
for
function
if
let
loop
mod
next
rem
return
step
sub
to
until
while
```

### Variables

Variable names consist of a letter, followed by any sequence of letters or digits. Underscores may be inserted anywhere in this sequence, but are ignored for purposes of comparing variables. Similarly, case is ignored. So the names `avar`, `a_var`, `AVar`, `A_VAR`, etc. all refer to the same variable.

Variables may have a type specifier appended to them. `$` indicates a string variable, `%` an integer variable, and `#` a floating point variable. The type specifier is part of the name, so `a$` and `a#` are different variables: the first is a string variable, and the second is a floating point variable. If no type specifier is appended, the variable is assumed to be an integer.

### Function declarations

Function names follow the same rules as variable names. Like variable names, function names may have a type specifier appended, and the type specifier gives the type that the function returns.

```
function Add(a, b)
  return a+b
end function
```
