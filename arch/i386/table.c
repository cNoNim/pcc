/*	$Id$	*/
/*
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


# include "pass2.h"

# define TLL TLONGLONG|TULONGLONG
# define ANYSIGNED TINT|TLONG|TSHORT|TCHAR
# define ANYUSIGNED TUNSIGNED|TULONG|TUSHORT|TUCHAR
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define TUWORD TUNSIGNED|TULONG
# define TSWORD TINT|TLONG
# define TWORD TUWORD|TSWORD

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/*
 * A bunch conversions of integral<->integral types
 */

/* convert pointers to int. */
{ SCONV,	INTAREG,
	STAREG,	TPOINT|TWORD,
	SANY,	TWORD,
		0,	RLEFT,
		"", },

/* convert int to short/char. This is done when register is loaded */
{ SCONV,	INTAREG,
	STAREG,	TWORD,
	SANY,	TSHORT|TUSHORT|TCHAR|TUCHAR|TWORD,
		0,	RLEFT,
		"", },

/* convert signed char to int. */
{ SCONV,	INTAREG,
	SAREG|STAREG|SOREG|SNAME,	TCHAR,
	SAREG|STAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	movsbl ZL,A1\n", },

/* convert unsigned char to (u)int. */
{ SCONV,	INTAREG,
	SAREG|STAREG|SOREG|SNAME,	TUCHAR,
	SAREG|STAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	movzbl ZL,A1\n", },

/* convert short to (u)int. */
{ SCONV,	INTAREG,
	SAREG|STAREG|SOREG|SNAME,	TSHORT,
	SAREG|STAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	movswl ZL,A1\n", },

/* convert unsigned char to (u)int. */
{ SCONV,	INTAREG,
	SAREG|STAREG|SOREG|SNAME,	TUSHORT,
	SAREG|STAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	movzwl ZL,A1\n", },

/* convert int to long long */
{ SCONV,	INTAREG,
	SAREG|STAREG,	TWORD,
	SAREG|STAREG,	TLONGLONG,
		NSPECIAL|NAREG|NASL,	RESC1,
		"	cltd\n", },

/* convert int to unsigned long long */
{ SCONV,	INTAREG,
	SAREG|STAREG|SOREG|SNAME,	TWORD,
	SAREG|STAREG,	TULONGLONG,
		NASL|NAREG,	RESC1,
		"	movl AL,A1\n	xorl U1,U1\n", },

/*
 * Store constant initializers.
 */
{ INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TWORD|TPOINT,
		0,	RNOP,
		"	.long CL\n", },

{ INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TLL,
		0,	RNOP,
		"	.long UL\n	.long CL\n", },

/*
 * Subroutine calls.
 */

{ UCALL,	INTAREG,
	SCON,	TANY,
	SANY,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT|TFLOAT|TDOUBLE|TLL|TPOINT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	call CL\nZC", },

{ UCALL,	INTAREG,
	SAREG|STAREG,	TANY,
	SANY,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT|TFLOAT|TDOUBLE|TLL|TPOINT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	call *AL\nZC", },

/*
 * The next rules handle all binop-style operators.
 */
/* Special treatment for long long */
{ PLUS,		INAREG|FOREFF,
	STAREG,		TLL,
	SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	addl AR,AL\n	adcl UR,UL\n", },

{ MINUS,		INAREG|FOREFF,
	STAREG,		TLL,
	SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	subl AR,AL\n	sbbl UR,UL\n", },

{ OPSIMP,	INAREG|FOREFF,
	SAREG|STAREG,		TWORD|TPOINT,
	SAREG|STAREG|SNAME|SOREG,	TWORD|TPOINT,
		0,	RLEFT,
		"	Ol AR,AL\n", },

{ OPSIMP,	INAREG|FOREFF,
	SAREG|STAREG,		TWORD|TPOINT,
	SCON,	TWORD|TPOINT,
		0,	RLEFT,
		"	Ol AR,AL\n", },

{ OPSIMP,	INAREG|FOREFF,
	SAREG|STAREG,	TCHAR|TUCHAR,
	SCON,	TANY,
		0,	RLEFT,
		"	Ob ZR,ZL\n", },

/*
 * The next rules handle all shift operators.
 */
{ LS,	INTAREG|INAREG,
	STAREG|SAREG,	TWORD,
	STAREG|SCON,	TWORD,
		0,	RLEFT,
		"	sall ZA,AL\n", },

{ LS,	FOREFF,
	STAREG|SAREG|SNAME|SOREG,	TWORD,
	STAREG|SCON,	TWORD,
		0,	RLEFT,
		"	sall ZA,AL\n", },

{ RS,	INTAREG|INAREG|FOREFF,
	STAREG|SAREG|SNAME|SOREG,	TSWORD,
	STAREG|SCON,	TWORD,
		0,	RLEFT,
		"	sarl ZA,AL\n", },

{ RS,	INTAREG|INAREG|FOREFF,
	STAREG|SAREG|SNAME|SOREG,	TUWORD,
	STAREG|SCON,	TWORD,
		0,	RLEFT,
		"	shrl ZA,AL\n", },

/*
 * The next rules takes care of assignments. "=".
 */
{ ASSIGN,	FOREFF|INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TLL,
	SCON,		TANY,
		0,	0,
		"	movl AR,AL\n	movl UR,UL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TWORD|TPOINT,
	SCON,		TANY,
		0,	0,
		"	movl AR,AL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TSHORT|TUSHORT,
	SCON,		TANY,
		0,	0,
		"	movw ZR,ZL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TCHAR|TUCHAR,
	SCON,		TANY,
		0,	0,
		"	movb ZR,ZL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	STAREG|SAREG|SNAME|SOREG,	TLL,
	STAREG|SAREG,			TLL,
		0,	RRIGHT,
		"	movl AR,AL\n	movl UR,UL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	STAREG|SNAME|SOREG,	TWORD|TPOINT,
	STAREG,		TWORD|TPOINT,
		0,	RRIGHT,
		"	movl AR,AL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	STAREG|SNAME|SOREG,	TSHORT|TUSHORT,
	STAREG,		TSHORT|TUSHORT,
		0,	RRIGHT,
		"	movw ZR,ZL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	STAREG|SNAME|SOREG,	TCHAR|TUCHAR,
	STAREG,			TCHAR|TUCHAR|TWORD,
		0,	RRIGHT,
		"	movb ZR,ZL\n", },

/*
 * DIV/MOD/MUL 
 */
{ DIV,	INTAREG,
	STAREG,				TWORD|TPOINT,
	STAREG|SAREG|SNAME|SOREG,	TWORD|TPOINT,
		NSPECIAL,		RLEFT,
		"	cltd\n	idivl AR\n", },

{ MOD,	INTAREG,
	STAREG,				TWORD|TPOINT,
	STAREG|SAREG|SNAME|SOREG,	TWORD|TPOINT,
		NSPECIAL|NAREG,		RESC1,
		"	cltd\n	idivl AR\n", },

{ MUL,	INTAREG,
	STAREG,		TWORD|TPOINT,
	STAREG|SNAME|SOREG|SCON,	TWORD|TPOINT,
		0,	RLEFT,
		"	imull AR,AL\n", },

/*
 * Indirection operators.
 */
{ UMUL,	INTAREG,
	SAREG|STAREG,	TPTRTO|TLL,
	SANY,		TLL,
		NAREG|NASL,	RESC1,
		"	movl (AL),U1\n	movl (AL),A1\n", },

{ UMUL,	INTAREG,
	SAREG|STAREG,	TPOINT|TWORD,
	SANY,		TPOINT|TWORD,
		NAREG|NASL,	RESC1,
		"	movl (AL),A1\n", },

{ UMUL,	INTAREG,
	SAREG|STAREG,	TCHAR|TUCHAR|TPTRTO,
	SANY,		TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	movb (AL),Z1\n", },

{ UMUL,	INTAREG,
	SAREG|STAREG,	TSHORT|TUSHORT|TPTRTO,
	SANY,		TSHORT|TUSHORT,
		NAREG|NASL,	RESC1,
		"	movw (AL),Z1\n", },

/*
 * INCR/DECR operators (post-increment)
 */
{ INCR,	INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TCHAR|TUCHAR|TPTRTO,
	SANY,	TANY,
		NAREG,	RESC1,
		"	movl ZL,Z1\n	incl ZL\n", },

{ INCR,	INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TWORD,
	SANY,	TANY,
		NAREG,	RESC1,
		"	movl ZL,Z1\n	incl ZL\n", },

{ INCR,	INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TSHORT|TUSHORT,
	SANY,	TANY,
		NAREG,	RESC1,
		"	movw ZL,Z1\n	incw ZL\n", },

{ INCR,	INTAREG,
	SAREG|STAREG|SNAME|SOREG,	TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG,	RESC1,
		"	movb ZL,Z1\n	incb ZL\n", },

/*
 * Logical/branching operators
 */

/* Comparisions, take care of everything */
{ OPLOG,	FORCC,
	SAREG|STAREG|SOREG|SNAME,	TLL,
	SAREG|STAREG,			TLL,
		0,	0,
		"ZD", },

{ OPLOG,	FORCC,
	SAREG|STAREG|SOREG|SNAME,	TWORD|TPOINT,
	SCON|SAREG|STAREG,	TWORD|TPOINT,
		0, 	RESCC,
		"	cmpl AR,AL\n", },

{ OPLOG,	FORCC,
	SAREG|STAREG|SOREG|SNAME,	TSHORT|TUSHORT,
	SCON|SAREG|STAREG,	TANY,
		0, 	RESCC,
		"	cmpw ZR,ZL\n", },

{ OPLOG,	FORCC,
	SAREG|STAREG|SOREG|SNAME,	TCHAR|TUCHAR,
	SCON|SAREG|STAREG,	TANY,
		0, 	RESCC,
		"	cmpb ZR,ZL\n", },

{ OPLOG,	FORCC,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"diediedie!", },

/* AND/OR/ER/NOT */
{ AND,	INTAREG|FOREFF,
	SAREG|STAREG|SOREG|SNAME,	TWORD,
	SCON|SAREG|STAREG,		TWORD,
		0,	RLEFT,
		"	andl AR,AL\n", },

{ AND,	INTAREG|FOREFF,
	SAREG|STAREG,			TWORD,
	SAREG|STAREG|SOREG|SNAME,	TWORD,
		0,	RLEFT,
		"	andl AR,AL\n", },

{ AND,	INTAREG|FOREFF,  
	SAREG|STAREG|SOREG|SNAME,	TSHORT|TUSHORT,
	SCON|SAREG|STAREG,		TSHORT|TUSHORT,
		0,	RLEFT,
		"	andw ZR,ZL\n", },

{ AND,	INTAREG|FOREFF,  
	SAREG|STAREG,			TSHORT|TUSHORT,
	SAREG|STAREG|SOREG|SNAME,	TSHORT|TUSHORT,
		0,	RLEFT,
		"	andw ZR,ZL\n", },

{ AND,	INTAREG|FOREFF,
	SAREG|STAREG|SOREG|SNAME,	TCHAR|TUCHAR,
	SCON|SAREG|STAREG,		TCHAR|TUCHAR,
		0,	RLEFT,
		"	andb ZR,ZL\n", },

{ AND,	INTAREG|FOREFF,
	SAREG|STAREG,			TCHAR|TUCHAR,
	SAREG|STAREG|SOREG|SNAME,	TCHAR|TUCHAR,
		0,	RLEFT,
		"	andb ZR,ZL\n", },

/* AND/OR/ER/NOT */

/*
 * Jumps.
 */
{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp LL\n", },

/*
 * Convert LTYPE to reg.
 */
{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SCON|SOREG|SNAME,	TLL,
		NAREG,	RESC1,
		"	movl UL,U1\n	movl AL,A1\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SAREG|SCON|SOREG|SNAME,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	movl AL,A1\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SAREG|SOREG|SNAME|SCON,	TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	movb ZL,Z1\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SAREG|SOREG|SNAME|SCON,	TSHORT|TUSHORT,
		NAREG,	RESC1,
		"	movw ZL,Z1\n", },

/*
 * Negate a word.
 */
{ UMINUS,	INAREG|INTAREG|FOREFF,
	SAREG|STAREG|SNAME|SOREG,	TWORD,
	SANY,	TWORD,
		NAREG|NASR,	RESC1,
		"	movn A1,AL\n", },

{ UMINUS,	INAREG|INTAREG|FOREFF,
	SAREG|STAREG|SNAME|SOREG,	TLL,
	SANY,	TLL,
		NAREG|NASR,	RESC1,
		"	dmovn A1,AL\n", },

{ COMPL,	INTAREG,
	SAREG|STAREG,	TWORD,
	SANY,	TANY,
		NASL,	RLEFT,
		"	notl AL\n", },

/*
 * Arguments to functions.
 */
{ FUNARG,	FOREFF,
	SCON|SAREG|SNAME|SOREG,	TLL,
	SANY,	TANY,
		0,	RNULL,
		"	pushl UL\n	pushl AL\n", },

{ FUNARG,	FOREFF,
	SCON|SAREG|SNAME|SOREG,	TWORD|TPOINT|TFLOAT,
	SANY,	TANY,
		0,	RNULL,
		"	pushl AL\n", },

# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

{ UMUL, DF( UMUL ), },

{ INCR, DF(INCR), },

{ DECR, DF(INCR), },

{ ASSIGN, DF(ASSIGN), },

{ STASG, DF(STASG), },

{ FLD, DF(FLD), },

{ OPLEAF, DF(NAME), },

{ INIT, DF(INIT), },

{ OPUNARY, DF(UMINUS), },

{ OPANY, DF(BITYPE), },

{ FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
