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


# include "pass1.h"

/*
 * cause the alignment to become a multiple of n
 */
void
defalign(int n)
{
	char *s;

	n /= SZCHAR;
	if (lastloc == PROG || n == 1)
		return;
	s = (isinlining ? permalloc(40) : tmpalloc(40));
	sprintf(s, "	.align %d\n", n);
	send_passt(IP_ASM, s);
}

/*
 * code for the end of a function
 * deals with struct return here
 */
void
efcode()
{
	NODE *p, *q;
	int sz;

	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
	/* address of return struct is in eax */
	/* create a call to memcpy() */
	/* will get the result in eax */
	p = block(REG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_rval = EAX;
	q = block(OREG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	q->n_rval = EBP;
	q->n_lval = 8; /* return buffer offset */
	p = block(CM, q, p, INT, 0, MKSUE(INT));
	sz = (tsize(STRTY, cftnsp->sdf, cftnsp->ssue)+SZCHAR-1)/SZCHAR;
	p = block(CM, p, bcon(sz), INT, 0, MKSUE(INT));
	p->n_right->n_name = "";
	p = block(CALL, bcon(0), p, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_left->n_name = "memcpy";
	send_passt(IP_NODE, p);
}

/*
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **a, int n)
{
	int i;

	send_passt(IP_LOCCTR, PROG);
	defnam(cftnsp);
	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
	/* Function returns struct, adjust arg offset */
	for (i = 0; i < n; i++)
		a[i]->soffset += SZPOINT;
}


/*
 * by now, the automatics and register variables are allocated
 */
void
bccode()
{
	SETOFF(autooff, SZINT);
}

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int flag )
{
}

/*
 * Print character t at position i in one string, until t == -1.
 * Locctr & label is already defined.
 */
void
bycode(int t, int i)
{
	static	int	lastoctal = 0;

	/* put byte i+1 in a string */

	if (t < 0) {
		if (i != 0)
			puts("\"");
	} else {
		if (i == 0)
			printf("\t.ascii \"");
		if (t == '\\' || t == '"') {
			lastoctal = 0;
			putchar('\\');
			putchar(t);
		} else if (t < 040 || t >= 0177) {
			lastoctal++;
			printf("\\%o",t);
		} else if (lastoctal && '0' <= t && t <= '9') {
			lastoctal = 0;
			printf("\"\n\t.ascii \"%c", t);
		} else {	
			lastoctal = 0;
			putchar(t);
		}
	}
}

/*
 * n integer words of zeros
 */
void
zecode(int n)
{
	printf("	.zero %d\n", n * (SZINT/SZCHAR));
	inoff += n * SZINT;
}

/*
 * return the alignment of field of type t
 */
int
fldal(unsigned int t)
{
	uerror("illegal field type");
	return(ALINT);
}

/* fix up type of field p */
void
fldty(struct symtab *p)
{
}

/* p points to an array of structures, each consisting
 * of a constant value and a label.
 * The first is >=0 if there is a default label;
 * its value is the label number
 * The entries p[1] to p[n] are the nontrivial cases
 * XXX - fix genswitch.
 */
void
genswitch(struct swents **p, int n)
{
	int i;
	char *s;

	/* simple switch code */
	for (i = 1; i <= n; ++i) {
		/* already in 1 */
		s = (isinlining ? permalloc(40) : tmpalloc(40));
		sprintf(s, "	cmpl $%lld,%%eax", p[i]->sval);
		send_passt(IP_ASM, s);
		s = (isinlining ? permalloc(40) : tmpalloc(40));
		sprintf(s, "	je " LABFMT, p[i]->slab);
		send_passt(IP_ASM, s);
	}
	if (p[0]->slab > 0)
		branch(p[0]->slab);
}
