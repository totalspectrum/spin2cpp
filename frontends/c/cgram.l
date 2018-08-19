D			[0-9]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			[Ee][+-]?{D}+
FS			(f|F|l|L)
IS			(u|U|l|L)*

%{
#include <stdio.h>
#include "cgram.tab.h"

void count();
%}

%%
"/*"			{ comment(); }

"auto"			{ count(); return(C_AUTO); }
"break"			{ count(); return(C_BREAK); }
"case"			{ count(); return(C_CASE); }
"char"			{ count(); return(C_CHAR); }
"const"			{ count(); return(C_CONST); }
"continue"		{ count(); return(C_CONTINUE); }
"default"		{ count(); return(C_DEFAULT); }
"do"			{ count(); return(C_DO); }
"double"		{ count(); return(C_DOUBLE); }
"else"			{ count(); return(C_ELSE); }
"enum"			{ count(); return(C_ENUM); }
"extern"		{ count(); return(C_EXTERN); }
"float"			{ count(); return(C_FLOAT); }
"for"			{ count(); return(C_FOR); }
"goto"			{ count(); return(C_GOTO); }
"if"			{ count(); return(C_IF); }
"int"			{ count(); return(C_INT); }
"long"			{ count(); return(C_LONG); }
"register"		{ count(); return(C_REGISTER); }
"return"		{ count(); return(C_RETURN); }
"short"			{ count(); return(C_SHORT); }
"signed"		{ count(); return(C_SIGNED); }
"sizeof"		{ count(); return(C_SIZEOF); }
"static"		{ count(); return(C_STATIC); }
"struct"		{ count(); return(C_STRUCT); }
"switch"		{ count(); return(C_SWITCH); }
"typedef"		{ count(); return(C_TYPEDEF); }
"union"			{ count(); return(C_UNION); }
"unsigned"		{ count(); return(C_UNSIGNED); }
"void"			{ count(); return(C_VOID); }
"volatile"		{ count(); return(C_VOLATILE); }
"while"			{ count(); return(C_WHILE); }

{L}({L}|{D})*		{ count(); return(check_type()); }

0[xX]{H}+{IS}?		{ count(); return(C_CONSTANT); }
0{D}+{IS}?		{ count(); return(C_CONSTANT); }
{D}+{IS}?		{ count(); return(C_CONSTANT); }
L?'(\\.|[^\\'])+'	{ count(); return(C_CONSTANT); }

{D}+{E}{FS}?		{ count(); return(C_CONSTANT); }
{D}*"."{D}+({E})?{FS}?	{ count(); return(C_CONSTANT); }
{D}+"."{D}*({E})?{FS}?	{ count(); return(C_CONSTANT); }

L?\"(\\.|[^\\"])*\"	{ count(); return(C_STRING_LITERAL); }

"..."			{ count(); return(C_ELLIPSIS); }
">>="			{ count(); return(C_RIGHT_ASSIGN); }
"<<="			{ count(); return(C_LEFT_ASSIGN); }
"+="			{ count(); return(C_ADD_ASSIGN); }
"-="			{ count(); return(C_SUB_ASSIGN); }
"*="			{ count(); return(C_MUL_ASSIGN); }
"/="			{ count(); return(C_DIV_ASSIGN); }
"%="			{ count(); return(C_MOD_ASSIGN); }
"&="			{ count(); return(C_AND_ASSIGN); }
"^="			{ count(); return(C_XOR_ASSIGN); }
"|="			{ count(); return(C_OR_ASSIGN); }
">>"			{ count(); return(C_RIGHT_OP); }
"<<"			{ count(); return(C_LEFT_OP); }
"++"			{ count(); return(C_INC_OP); }
"--"			{ count(); return(C_DEC_OP); }
"->"			{ count(); return(C_PTR_OP); }
"&&"			{ count(); return(C_AND_OP); }
"||"			{ count(); return(C_OR_OP); }
"<="			{ count(); return(C_LE_OP); }
">="			{ count(); return(C_GE_OP); }
"=="			{ count(); return(C_EQ_OP); }
"!="			{ count(); return(C_NE_OP); }
";"			{ count(); return(';'); }
("{"|"<%")		{ count(); return('{'); }
("}"|"%>")		{ count(); return('}'); }
","			{ count(); return(','); }
":"			{ count(); return(':'); }
"="			{ count(); return('='); }
"("			{ count(); return('('); }
")"			{ count(); return(')'); }
("["|"<:")		{ count(); return('['); }
("]"|":>")		{ count(); return(']'); }
"."			{ count(); return('.'); }
"&"			{ count(); return('&'); }
"!"			{ count(); return('!'); }
"~"			{ count(); return('~'); }
"-"			{ count(); return('-'); }
"+"			{ count(); return('+'); }
"*"			{ count(); return('*'); }
"/"			{ count(); return('/'); }
"%"			{ count(); return('%'); }
"<"			{ count(); return('<'); }
">"			{ count(); return('>'); }
"^"			{ count(); return('^'); }
"|"			{ count(); return('|'); }
"?"			{ count(); return('?'); }

[ \t\v\n\f]		{ count(); }
.			{ /* ignore bad characters */ }

%%

cgram_yywrap()
{
	return(1);
}


comment()
{
	char c, c1;

loop:
	while ((c = input()) != '*' && c != 0)
		putchar(c);

	if ((c1 = input()) != '/' && c != 0)
	{
		unput(c1);
		goto loop;
	}

	if (c != 0)
		putchar(c1);
}


int column = 0;

void count()
{
	int i;

	for (i = 0; yytext[i] != '\0'; i++)
		if (yytext[i] == '\n')
			column = 0;
		else if (yytext[i] == '\t')
			column += 8 - (column % 8);
		else
			column++;

	ECHO;
}


int check_type()
{
/*
* pseudo code --- this is what it should check
*
*	if (yytext == type_name)
*		return(TYPE_NAME);
*
*	return(IDENTIFIER);
*/

/*
*	it actually will only return IDENTIFIER
*/

	return(IDENTIFIER);
}
