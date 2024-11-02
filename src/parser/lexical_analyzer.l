%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"

int lines = 1;
int pos_start = 1;
int pos_end = 1;

void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

%}


%%
 /* TODO: for students */
 /* two cases for you, pass_node will send flex's token to bison */
\+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;}
\- 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return SUB;}
\* 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return MUL;}
\/ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return DIV;}
\< 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LES;}
\<= 	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return LSEQ;}
\>	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LAR;}
\>= 	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return LREQ;}
\==	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return EQL;}
\!=	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return NEQ;}
\=	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ASN;}
\; 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return SEMCLN;}
\, 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return COM;}
\( 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LPAREN;}
\) 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return RPAREN;}
\[ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LBRACK;}
\]	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return RBRACK;}
\{ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LCURL;}
\} 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return RCURL;}
"/*"[^*]*[*]+([^*/][^*]*[*]+)*[/]	{pos_start = pos_end; pos_end += strlen(yytext);}
[0-9]+  {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return INTEGER;}
[0-9]+\.|[0-9]*\.[0-9]+  {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return FLOAT;}
if   {pos_start = pos_end; pos_end += 2; pass_node(yytext); return KEYWORDIF;}
else {pos_start = pos_end; pos_end += 4; pass_node(yytext); return KEYWORDELSE;}
int  {pos_start = pos_end; pos_end += 3; pass_node(yytext); return KEYWORDINT;}
return    {pos_start = pos_end; pos_end += 6; pass_node(yytext); return KEYWORDRETURN;}
void  {pos_start = pos_end; pos_end += 4; pass_node(yytext); return KEYWORDVOID;}
while  {pos_start = pos_end; pos_end += 5; pass_node(yytext); return KEYWORDWHILE;}
float  {pos_start = pos_end; pos_end += 5; pass_node(yytext); return KEYWORDFLOAT;}
 /*[a-zA-Z] {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LETTER;}*/
[a-zA-Z]+  {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return ID;}
" "|\t   {pos_start = pos_end; pos_end += 1;}
\r\n|\n|\r {pos_start = 1; lines +=1; pos_end = 1; }
. { pos_start = pos_end; pos_end++; return ERROR; }

 /****请在此补全所有flex的模式与动作  end******/
%%
