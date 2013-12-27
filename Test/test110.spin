{{{
   This is a test of comment parsing.
   The Spin compiler does not actually count parentheses if the comment is
   a 'doc' comment starting with '{{'; in that case it only searches for
   the closing comment symbol of two }'s in a row
}}

PUB main
  DIRA[0] := 1
  OUTA[0] := 1

{{
  The end of the program
}}
