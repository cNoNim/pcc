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
# include <ctype.h>

# define putstr(s)	fputs((s), stdout)

void sconv(NODE *p, int);
void acon(NODE *p);
int argsize(NODE *p);
void genargs(NODE *p);

static int ftlab1, ftlab2;

void
lineid(int l, char *fn)
{
	/* identify line l and file fn */
	printf("#	line %d, file %s\n", l, fn);
}

void
defname(char *name, int visib)
{
	printf("	.align 4\n");
	if (visib)
		printf("	.globl %s\n", name);
	printf("%s:\n", name);
}

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

static int isoptim;

void
prologue(int regs, int autos)
{
	int addto;

	if (regs < 0 || autos < 0) {
		/*
		 * non-optimized code, jump to epilogue for code generation.
		 */
		ftlab1 = getlab();
		ftlab2 = getlab();
		printf("	jmp " LABFMT "\n", ftlab1);
		deflab(ftlab2);
	} else {
		/*
		 * We here know what register to save and how much to 
		 * add to the stack.
		 */
		addto = (maxautooff - AUTOINIT)/SZCHAR;
		printf("	pushl %%ebp\n");
		printf("	movl %%esp,%%ebp\n");
		if (addto)
			printf("	subl $%d,%%esp\n", addto);
		isoptim = 1;
	}
}

/*
 * End of block.
 */
void
eoftn(int regs, int autos, int retlab)
{
	register OFFSZ spoff;	/* offset from stack pointer */

	spoff = maxautooff;
	if (spoff >= AUTOINIT)
		spoff -= AUTOINIT;
	spoff /= SZCHAR;
	/* return from function code */
	deflab(retlab);
	printf("	leave\n");
	printf("	ret\n");

	/* Prolog code */
	if (isoptim == 0) {
		deflab(ftlab1);
		printf("	pushl %%ebp\n");
		printf("	movl %%esp,%%ebp\n");
		if (spoff)
			printf("	subl $%lld,%%esp\n", spoff);
		printf("	jmp " LABFMT "\n", ftlab2);
	}
	isoptim = 0;
}

static char *loctbl[] = { "text", "data", "data", "text", "text", "stab" };

void
setlocc(int locctr)
{
	static int lastloc;

	if (locctr == lastloc)
		return;

	lastloc = locctr;
	printf("	.%s\n", loctbl[locctr]);
}

/*
 * add/sub/...
 *
 * Param given:
 */
void
hopcode(int f, int o)
{
	char *str;

	if (asgop(o))
		o = NOASG o;

	switch (o) {
	case PLUS:
		str = "add";
		break;
	case MINUS:
		str = "sub";
		break;
	case AND:
		str = "and";
		break;
	case OR:
		str = "ior";
		break;
	case ER:
		str = "xor";
		break;
	default:
		cerror("hopcode2: %d", o);
	}
	printf("%s%c", str, f);
}

char *
rnames[] = {  /* keyed to register number tokens */
	"%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "%ebp", "%esp",
};

int rstatus[] = {
	SAREG|STAREG, SAREG|STAREG, SBREG|STBREG, SAREG|STAREG,
	SAREG|STAREG, SAREG|STAREG, 0, 0,
};

int
tlen(p) NODE *p;
{
	switch(p->n_type) {
		case CHAR:
		case UCHAR:
			return(1);

		case SHORT:
		case USHORT:
			return(SZSHORT/SZCHAR);

		case DOUBLE:
			return(SZDOUBLE/SZCHAR);

		case INT:
		case UNSIGNED:
		case LONG:
		case ULONG:
			return(SZINT/SZCHAR);

		case LONGLONG:
		case ULONGLONG:
			return SZLONGLONG/SZCHAR;

		default:
			if (!ISPTR(p->n_type))
				cerror("tlen type %d not pointer");
			return SZPOINT/SZCHAR;
		}
}

/*
 * Return true if the constant can be bundled in an instruction (immediate).
 */
static int
oneinstr(NODE *p)
{
	if (p->n_name[0] != '\0')
		return 0;
	if ((p->n_lval & 0777777000000ULL) != 0)
		return 0;
	return 1;
}

/*
 * Handle xor of constants separate.
 * Emit two instructions instead of one extra memory reference.
 */
static void
emitxor(NODE *p)               
{                       
	CONSZ val;
	int reg;

	if (p->n_op != EREQ)
		cerror("emitxor");
	if (p->n_right->n_op != ICON)
		cerror("emitxor2");
	val = p->n_right->n_lval;
	reg = p->n_left->n_rval;
	if (val & 0777777)
		printf("	trc 0%o,0%llo\n", reg, val & 0777777);
	if (val & 0777777000000)
		printf("	tlc 0%o,0%llo\n", reg, (val >> 18) & 0777777);
}

/*
 * Print an instruction that takes care of a byte or short (less than 36 bits)
 */
static void
outvbyte(NODE *p)
{
	NODE *l = p->n_left;
	int lval, bsz, boff;

	lval = l->n_lval;
	l->n_lval &= 0777777;
	bsz = (lval >> 18) & 077;
	boff = (lval >> 24) & 077;

	if ((bsz == 18) && (boff == 0 || boff == 18)) {
		printf("hr%cm", boff ? 'r' : 'l');
		return;
	}
	cerror("outvbyte: bsz %d boff %d", bsz, boff);
}

/*
 * Add an int to a pointer. Both args are in registers, store value
 * in the right register.
 */
static void     
addtoptr(NODE *p) 
{                   
	NODE *l = p->n_left;
	int pp = l->n_type & TMASK1; /* pointer to pointer */
	int ty = l->n_type;
	int ischar = BTYPE(ty) == CHAR || BTYPE(ty) == UCHAR;

	if (!ischar && BTYPE(ty) != SHORT && BTYPE(ty) != USHORT)
		cerror("addtoptr != CHAR/SHORT");
	printf("	ad%s ", pp ? "d" : "jbp");
	adrput(getlr(p, 'R'));
	putchar(',');
	adrput(getlr(p, 'L'));
	putchar('\n');
}

/*
 * Add a constant to a pointer.
 */
static void     
addcontoptr(NODE *p) 
{                   
	NODE *l = p->n_left;
	int pp = l->n_type & TMASK1; /* pointer to pointer */
	int ty = l->n_type;
	int ischar = BTYPE(ty) == CHAR || BTYPE(ty) == UCHAR;

	if (!ischar && BTYPE(ty) != SHORT && BTYPE(ty) != USHORT)
		cerror("addtoptr != SHORT/CHAR");
	if (pp) {
		printf("	addi ");
		adrput(getlr(p, 'L'));
		putchar(',');
		adrput(getlr(p, 'R'));
		putchar('\n');
		if ((p->n_type & TMASK1) == 0) {
			/* Downgrading to pointer to short */
			/* Must make short pointer */
			printf("	tlo ");
			adrput(getlr(p, 'L'));
			if (ischar)
				printf(",0700000\n");
			else
				printf(",0750000\n");
		}
	} else {
		CONSZ off = p->n_right->n_lval;

		if (off == 0)
			return; /* Should be taken care of in clocal() */
		printf("	addi ");
		adrput(getlr(p, 'L'));
		printf(",0%llo\n", off >> 1);
		if (off & 1) {
			printf("	ibp ");
			adrput(getlr(p, 'L'));
			printf("\n");
		}
	}
}

/*
 * Add a constant to a char pointer and return it in a scratch reg.
 */
static void     
addconandcharptr(NODE *p) 
{                   
	NODE *l = p->n_left;
	int ty = l->n_type;
	CONSZ off = p->n_right->n_lval;

	if (BTYPE(ty) != CHAR && BTYPE(ty) != UCHAR)
		cerror("addconandcharptr != CHAR");
	if (l->n_rval == FPREG) {
		printf("	xmovei ");
		adrput(getlr(p, '1'));
		printf(",0%llo(0%o)\n", off >> 2, l->n_rval);
		printf("	tlo ");
		adrput(getlr(p, '1'));
		printf(",0%o0000\n", (int)(off & 3) + 070);
	} else {
		if (off >= 0 && off <= 0777777) {
			printf("	movei ");
			adrput(getlr(p, '1'));
			printf(",0%llo\n", off);
		} else {
			printf("	move ");
			adrput(getlr(p, '1'));
			printf(",[ .long 0%llo ]\n", off & 0777777777777);
		}
		printf("	adjbp ");
		adrput(getlr(p, '1'));
		printf(",0%o\n", l->n_rval);
	}
}

/*
 * Divide a register with a constant.
 */
static void     
idivi(NODE *p)
{
	NODE *r = p->n_right;

	if (r->n_lval >= 0 && r->n_lval <= 0777777) {
		printf("	idivi ");
		adrput(getlr(p, '1'));
		printf(",0%llo\n", r->n_lval);
	} else {
		printf("	idiv ");
		adrput(getlr(p, '1'));
		printf(",[ .long 0%llo ]\n", r->n_lval & 0777777777777);
	}
}

static void
putcond(NODE *p)
{               
	char *c;

	switch (p->n_op) {
	case EQ: c = "e"; break;
	case NE: c = "n"; break;
	case LE: c = "le"; break;
	case LT: c = "l"; break;
	case GT: c = "g"; break;
	case GE: c = "ge"; break;
	default:
		cerror("putcond");
	}
	printf("%s", c);
}

/*
 * XOR a longlong with a constant.
 * XXX - if constant is 0400000000000 only deal with high word.
 * This is correct because bit 0 on lower word is useless anyway.
 */
static void
xorllcon(NODE *p)
{                       
	CONSZ c = p->n_right->n_lval;
	int r = p->n_left->n_rval;
	int n;

	if (c == 0400000000000LL) {
		printf("	tlc %s,0400000\n", rnames[r]);
		return;
	}
	if ((n = ((c >> 54) & 0777777)))
		printf("	tlc %s,0%06o\n", rnames[r], n);
	if ((n = ((c >> 36) & 0777777)))
		printf("	trc %s,0%06o\n", rnames[r], n);
	if ((n = ((c >> 18) & 0777777)))
		printf("	tlc %s,0%06o\n", rnames[r+1], n);
	if ((n = (c & 0777777)))
		printf("	trc %s,0%06o\n", rnames[r+1], n);
}

void
zzzcode(NODE *p, int c)
{
	NODE *r;
	int m;

	switch (c) {
	case 'A':
		/*
		 * Shift operations. Either the right node is a constant
		 * or a register, in the latter case it must be %cl.
		 */
		p = p->n_right;
		if (p->n_op == ICON)
			printf("$" CONFMT, p->n_lval);
		else if (p->n_op != REG || p->n_rval != 2) /* CX */
			cerror("bad shift reg");
		else
			printf("%%cl"); 
		break;

	case 'B':
		/*
		 * Print conversion chars for loading into register.
		 */
		p = getlr(p, 'R');
		switch (p->n_type) {
		case SHORT: printf("swl"); break;
		case USHORT: printf("zwl"); break;
		case CHAR: printf("sbl"); break;
		case UCHAR: printf("zbl"); break;
		default: cerror("ZB: %d", p->n_type);
		}
		break;

	case 'L':
	case 'R':
	case '1':
		/*
		 * Prints out a register of small type, like %al.
		 * Type is determined by op.
		 */
		r = getlr(p, c);
		if (r->n_op != REG)
			adrput(r);
		else if (p->n_type == SHORT || p->n_type == USHORT)
			printf("%%%cx", rnames[r->n_rval][2]);
		else
			printf("%%%cl", rnames[r->n_rval][2]);
		break;

	case 'E': /* Print correct constant expression */
		if (p->n_name[0] == '\0') {
			if ((p->n_lval <= 0777777) && (p->n_lval > 0)){
				printf("0%llo", p->n_lval);
			} else if ((p->n_lval & 0777777) == 0) {
				printf("0%llo", p->n_lval >> 18);
			} else {
				if (p->n_lval < 0)
					printf("[ .long -0%llo]", -p->n_lval);
				else
					printf("[ .long 0%llo]", p->n_lval);
			}
		} else {
			if (p->n_lval == 0)
				printf("[ .long %s]", p->n_name);
			else
				printf("[ .long %s+0%llo]",
				    p->n_name, p->n_lval);
		}
		break;

	case 'G': /* Print a constant expression based on its const type */
		p = p->n_right;
		if (oneinstr(p)) {
			printf("0%llo", p->n_lval);
		} else {
			if (p->n_name[0] == '\0') {
				printf("[ .long 0%llo ]",
				    p->n_lval & 0777777777777ULL);
			} else {
				if (p->n_lval == 0)
					printf("[ .long %s ]", p->n_name);
				else
					printf("[ .long %s+0%llo]", p->n_name,
					    p->n_lval, 0777777777777ULL);
			}
		}
		break;

	case 'I':
		p = p->n_left;
		/* FALLTHROUGH */
	case 'K':
		if (p->n_name[0] != '\0')
			putstr(p->n_name);
		if (p->n_lval != 0) {
			putchar('+');
			printf("0%llo", p->n_lval & 0777777777777);
		}
		break;

	case 'J':
		outvbyte(p);
		break;

	case 'T':
		if (p->n_op != OREG)
			cerror("ZT");
		p->n_op = REG;
		adrput(p);
		p->n_op = OREG;
		break;

	case 'M':
		sconv( p, c == 'M' );
		break;

	case 'N':  /* logical ops, turned into 0-1 */
		/* use register given by register 1 */
		cbgen(0, m = getlab());
		deflab(p->n_label);
		printf("	setz %s,\n", rnames[getlr(p, '1')->n_rval]);
		deflab(m);
		break;

	case 'S':
		emitxor(p);
		break;

	case 'W':
		addtoptr(p);
		break;

	case 'X':
		addcontoptr(p);
		break;

	case 'Y':
		addconandcharptr(p);
		break;

	case 'b':
		idivi(p);
		break;

	case 'e':
		putcond(p);
		break;

	case 'f':
		xorllcon(p);
		break;

	default:
		cerror("zzzcode %c", c);
	}
}

/*
 * Convert between two data types.
 */
void
sconv(NODE *p, int forarg)
{
}

/*
 * Output code to move a value between two registers.
 * XXX - longlong?
 */
void
rmove(int rt, int rs, TWORD t)
{
	printf("\t%s %s,%s\n", (t == DOUBLE ? "dmove" : "move"),
	    rnames[rt], rnames[rs]);
}

struct respref respref[] = {
	{ INTAREG|INTBREG,	INTAREG|INTBREG, },
	{ INAREG|INBREG,	INAREG|INBREG|SOREG|STARREG|STARNM|SNAME|SCON,},
	{ INTEMP,	INTEMP, },
	{ FORARG,	FORARG, },
	{ INTEMP,	INTAREG|INAREG|INTBREG|INBREG|SOREG|STARREG|STARNM, },
	{ 0,	0 },
};

/* set up temporary registers */
void
setregs()
{
	fregs = 6;	/* 6 free regs on x86 (0-5) */
}

/*ARGSUSED*/
int
rewfld(NODE *p)
{
	return(1);
}

/*ARGSUSED*/
int
callreg(NODE *p)
{
	return(0);
}

int canaddr(NODE *);
int
canaddr(NODE *p)
{
	int o = p->n_op;

	if (o==NAME || o==REG || o==ICON || o==OREG ||
	    (o==UNARY MUL && shumul(p->n_left)))
		return(1);
	return(0);
}

int
flshape(NODE *p)
{
	register int o = p->n_op;

	return (o == REG || o == NAME || o == ICON ||
		(o == OREG && (!R2TEST(p->n_rval) || tlen(p) == 1)));
}

/* INTEMP shapes must not contain any temporary registers */
int
shtemp(NODE *p)
{
	int r;

	if (p->n_op == STARG )
		p = p->n_left;

	switch (p->n_op) {
	case REG:
		return (!istreg(p->n_rval));

	case OREG:
		r = p->n_rval;
		if (R2TEST(r)) {
			if (istreg(R2UPK1(r)))
				return(0);
			r = R2UPK2(r);
		}
		return (!istreg(r));

	case UNARY MUL:
		p = p->n_left;
		return (p->n_op != UNARY MUL && shtemp(p));
	}

	if (optype(p->n_op) != LTYPE)
		return(0);
	return(1);
}

int
shumul(NODE *p)
{
	register int o;

	if (x2debug) {
		int val;
		printf("shumul(%p)\n", p);
		eprint(p, 0, &val, &val);
	}

	o = p->n_op;
#if 0
	if (o == NAME || (o == OREG && !R2TEST(p->n_rval)) || o == ICON)
		return(STARNM);
#endif

	if ((o == INCR || o == ASG MINUS) &&
	    (p->n_left->n_op == REG && p->n_right->n_op == ICON) &&
	    p->n_right->n_name[0] == '\0') {
		switch (p->n_type) {
			case CHAR|PTR:
			case UCHAR|PTR:
				o = 1;
				break;

			case SHORT|PTR:
			case USHORT|PTR:
				o = 2;
				break;

			case INT|PTR:
			case UNSIGNED|PTR:
			case LONG|PTR:
			case ULONG|PTR:
			case FLOAT|PTR:
				o = 4;
				break;

			case DOUBLE|PTR:
			case LONGLONG|PTR:
			case ULONGLONG|PTR:
				o = 8;
				break;

			default:
				if (ISPTR(p->n_type) &&
				     ISPTR(DECREF(p->n_type))) {
					o = 4;
					break;
				} else
					return(0);
		}
		return( p->n_right->n_lval == o ? STARREG : 0);
	}

	return( 0 );
}

void
adrcon(CONSZ val)
{
	printf("$" CONFMT, val);
}

void
conput(NODE *p)
{
	switch (p->n_op) {
	case ICON:
		if (p->n_lval != 0) {
			acon(p);
			if (p->n_name[0] != '\0')
				putchar('+');
		}
		if (p->n_name[0] != '\0')
			printf("%s", p->n_name);
		if (p->n_name[0] == '\0' && p->n_lval == 0)
			putchar('0');
		return;

	case REG:
		putstr(rnames[p->n_rval]);
		return;

	default:
		cerror("illegal conput");
	}
}

/*ARGSUSED*/
void
insput(NODE *p)
{
	cerror("insput");
}

/*
 * Write out the upper address, like the upper register of a 2-register
 * reference, or the next memory location.
 */
void
upput(NODE *p, int size)
{

	size /= SZLONG;
	switch (p->n_op) {
	case REG:
		putstr(rnames[p->n_rval + size]);
		break;

	case NAME:
	case OREG:
		p->n_lval += size;
		adrput(p);
		p->n_lval -= size;
		break;
	case ICON:
		printf(CONFMT, p->n_lval >> (36 * size));
		break;
	default:
		cerror("upput bad op %d size %d", p->n_op, size);
	}
}

void
adrput(NODE *p)
{
	int r;
	/* output an address, with offsets, from p */

	if (p->n_op == FLD)
		p = p->n_left;

	switch (p->n_op) {

	case NAME:
		zzzcode(p, 'K');
		return;

	case OREG:
		r = p->n_rval;
		acon(p);
		if (p->n_name[0] != '\0')
			printf("+%s", p->n_name);
		printf("(%s)", rnames[p->n_rval]);
		return;
	case ICON:
		/* addressable value of the constant */
		if (p->n_lval != 0) {
			adrcon(p->n_lval);
			if (p->n_name[0] != '\0')
				putchar('+');
		}
		if (p->n_name[0] != '\0')
			printf("%s", p->n_name);
		if (p->n_name[0] == '\0' && p->n_lval == 0)
			adrcon(0);
		return;

	case REG:
		putstr(rnames[p->n_rval]);
		return;

	default:
		cerror("illegal address, op %d", p->n_op);
		return;

	}
}

/*
 * print out a constant
*/
void
acon(NODE *p)
{
	printf(CONFMT, p->n_lval);
}

int
genscall(NODE *p, int cookie)
{
	/* structure valued call */
	return(gencall(p, cookie));
}

/*
 * generate the call given by p
 */
/*ARGSUSED*/
int
gencall(NODE *p, int cookie)
{

	NODE *p1;
	int temp1, m;
	OFFSZ temp;

	temp = p->n_right ? argsize(p->n_right) : 0;

	if (p->n_op == STCALL || p->n_op == UNARY STCALL) {
		/* set aside room for structure return */

		temp1 = p->n_stsize > temp ? p->n_stsize : temp;
	}

	SETOFF(temp1,4);

	 /* make temp node, put offset in, and generate args */
	if (p->n_right)
		genargs(p->n_right);

	/*
	 * Verify that pushj can be emitted.
	 */
	p1 = p->n_left;
	switch (p1->n_op) {
	case ICON:
	case REG:
	case OREG:
	case NAME:
		break;
	default:
		order(p1, INAREG);
	}

	p->n_op = UNARY CALL;
	m = match(p, INTAREG|INTBREG);

	/* Remove args (if any) from stack */
	if (temp)
		printf("	addl $" CONFMT ",%%esp\n", temp);

	return(m != MDONE);
}

static char *
ccbranches[] = {
	"je",		/* jumpe */
	"jne",		/* jumpn */
	"jle",		/* jumple */
	"jl",		/* jumpl */
	"jge",		/* jumpge */
	"jg",		/* jumpg */
	"jbe",		/* jumple (jlequ) */
	"jb",		/* jumpl (jlssu) */
	"jae",		/* jumpge (jgequ) */
	"ja",		/* jumpg (jgtru) */
};


/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		cerror("bad conditional branch: %s", opst[o]);
	printf("	%s " LABFMT "\n", ccbranches[o-EQ], lab);
}

/* we have failed to match p with cookie; try another */
int
nextcook(NODE *p, int cookie)
{
	if (cookie == FORREW)
		return(0);  /* hopeless! */
	if (!(cookie&(INTAREG|INTBREG)))
		return(INTAREG|INTBREG);
	if (!(cookie&INTEMP) && asgop(p->n_op))
		return(INTEMP|INAREG|INTAREG|INTBREG|INBREG);
	return(FORREW);
}

int
lastchance(NODE *p, int cook)
{
	/* forget it! */
	return(0);
}

#if 0
static void
unaryops(NODE *p)
{
	NODE *q;
	char *fn;

	switch (p->n_op) {
	case UNARY MINUS:
		fn = "__negdi2";
		break;

	case COMPL:
		fn = "__one_cmpldi2";
		break;
	default:
		return;
	}

	p->n_op = CALL;
	p->n_right = p->n_left;
	p->n_left = q = talloc();
	q->n_op = ICON;
	q->n_rall = NOPREF;
	q->n_type = INCREF(FTN + p->n_type);
	q->n_name = fn;
	q->n_lval = 0;
	q->n_rval = 0;
}

/* added by jwf */
struct functbl {
	int fop;
	TWORD ftype;
	char *func;
} opfunc[] = {
//	{ DIV,		TANY,	"__divdi3", },
//	{ MOD,		TANY,	"__moddi3", },
//	{ MUL,		TANY,	"__muldi3", },
//	{ PLUS,		TANY,	"__adddi3", },
//	{ MINUS,	TANY,	"__subdi3", },
//	{ RS,		TANY,	"__ashrdi3", },
//	{ ASG DIV,	TANY,	"__divdi3", },
//	{ ASG MOD,	TANY,	"__moddi3", },
//	{ ASG MUL,	TANY,	"__muldi3", },
//	{ ASG PLUS,	TANY,	"__adddi3", },
//	{ ASG MINUS,	TANY,	"__subdi3", },
//	{ ASG RS,	TANY,	"__ashrdi3", },
	{ 0,	0,	0 },
};

int e2print(NODE *p, int down, int *a, int *b);
void hardops(NODE *p);
void
hardops(NODE *p)
{
	/* change hard to do operators into function calls.  */
	NODE *q;
	TWORD t;
	struct functbl *f;
	int o;
	NODE *old,*temp;

	o = p->n_op;
	t = p->n_type;


	if (!ISLONGLONG(t))
		return;

	if (optype(o) != BITYPE)
		return unaryops(p);

	for (f = opfunc; f->fop; f++) {
		if (o == f->fop)
			goto convert;
	}
	return;

	convert:
	/*
	 * If it's a "a += b" style operator, rewrite it to "a = a + b".
	 */
	if (asgop(o)) {
		old = NIL;
		switch (p->n_left->n_op) {

		case UNARY MUL:
			q = p->n_left;
			/*
			 * rewrite (lval /= rval); as
			 *  ((*temp) = udiv((*(temp = &lval)), rval));
			 * else the compiler will evaluate lval twice.
			 */

			/* first allocate a temp storage */
			temp = talloc();
			temp->n_op = OREG;
			temp->n_rval = TMPREG;
			temp->n_lval = BITOOR(freetemp(1));
			temp->n_type = INCREF(p->n_type);
			temp->n_name = "";
			old = q->n_left;
			q->n_left = temp;

			/* FALLTHROUGH */
		case REG:
		case NAME:
		case OREG:
			/* change ASG OP to a simple OP */
			q = talloc();
			q->n_op = NOASG p->n_op;
			q->n_rall = NOPREF;
			q->n_type = p->n_type;
			q->n_left = tcopy(p->n_left);
			q->n_right = p->n_right;
			p->n_op = ASSIGN;
			p->n_right = q;
			p = q;

			/* on the right side only - replace *temp with
			 *(temp = &lval), build the assignment node */
			if (old) {
				temp = q->n_left; /* the "*" node */
				q = talloc();
				q->n_op = ASSIGN;
				q->n_left = temp->n_left;
				q->n_right = old;
				q->n_type = old->n_type;
				q->n_name = "";
				temp->n_left = q;
			}
			break;

		default:
			cerror( "hardops: can't compute & LHS" );
			}
		}

	/* build comma op for args to function */
	q = talloc();
	q->n_op = CM;
	q->n_rall = NOPREF;
	q->n_type = INT;
	q->n_left = p->n_left;
	q->n_right = p->n_right;
	p->n_op = CALL;
	p->n_right = q;

	/* put function name in left node of call */
	p->n_left = q = talloc();
	q->n_op = ICON;
	q->n_rall = NOPREF;
	q->n_type = INCREF(FTN + p->n_type);
	q->n_name = f->func;
	if (ISUNSIGNED(t)) {
		switch (o) {
		case DIV:
			q->n_name = "__udivdi3";
			break;
		case MOD:
			q->n_name = "__umoddi3";
			break;
		}
	}

	q->n_lval = 0;
	q->n_rval = 0;
}
#endif

/*
 * Do some local optimizations that must be done after optim is called.
 */
static void
optim2(NODE *p)
{
	int op = p->n_op;
	int m, ml;
	NODE *l;

	/* Remove redundant PCONV's */
	if (op == PCONV) {
		l = p->n_left;
		m = BTYPE(p->n_type);
		ml = BTYPE(l->n_type);
		if ((m == INT || m == LONG || m == LONGLONG || m == FLOAT ||
		    m == DOUBLE || m == STRTY || m == UNIONTY || m == ENUMTY ||
		    m == UNSIGNED || m == ULONG || m == ULONGLONG) &&
		    (ml == INT || ml == LONG || ml == LONGLONG || ml == FLOAT ||
		    ml == DOUBLE || ml == STRTY || ml == UNIONTY || 
		    ml == ENUMTY || ml == UNSIGNED || ml == ULONG ||
		    ml == ULONGLONG) && ISPTR(l->n_type)) {
			*p = *l;
			nfree(l);
			op = p->n_op;
		} else
		if (ISPTR(DECREF(p->n_type)) &&
		    (l->n_type == INCREF(STRTY))) {
			*p = *l;
			nfree(l);
			op = p->n_op;
		} else
		if (ISPTR(DECREF(l->n_type)) &&
		    (p->n_type == INCREF(INT) ||
		    p->n_type == INCREF(STRTY) ||
		    p->n_type == INCREF(UNSIGNED))) {
			*p = *l;
			nfree(l);
			op = p->n_op;
		}

	}
	/* Add constands, similar to the one in optim() */
	if (op == PLUS && p->n_right->n_op == ICON) {
		l = p->n_left;
		if (l->n_op == PLUS && l->n_right->n_op == ICON &&
		    (p->n_right->n_name[0] == '\0' ||
		     l->n_right->n_name[0] == '\0')) {
			l->n_right->n_lval += p->n_right->n_lval;
			if (l->n_right->n_name[0] == '\0')
				l->n_right->n_name = p->n_right->n_name;
			nfree(p->n_right);
			*p = *l;
			nfree(l);
		}
	}

	/* Convert "PTR undef" (void *) to "PTR uchar" */
	/* XXX - should be done in MI code */
	if (BTYPE(p->n_type) == VOID)
		p->n_type = (p->n_type & ~BTMASK) | UCHAR;
}

void
myreader(NODE *p)
{
	int e2print(NODE *p, int down, int *a, int *b);
//	walkf(p, hardops);	/* convert ops to function calls */
	walkf(p, optim2);
	if (x2debug) {
		printf("myreader final tree:\n");
		fwalk(p, e2print, 0);
	}
}

/*
 * Remove some PCONVs after OREGs are created.
 */
static void
pconv2(NODE *p)
{
	NODE *q;

	if (p->n_op == PLUS) {
		if (p->n_type == (PTR|SHORT) || p->n_type == (PTR|USHORT)) {
			if (p->n_right->n_op != ICON)
				return;
			if (p->n_left->n_op != PCONV)
				return;
			if (p->n_left->n_left->n_op != OREG)
				return;
			q = p->n_left->n_left;
			nfree(p->n_left);
			p->n_left = q;
			/*
			 * This will be converted to another OREG later.
			 */
		}
	}
}

void
mycanon(NODE *p)
{
	walkf(p, pconv2);
}

/*
 * Remove last goto.
 */
void
myoptim(struct interpass *ip)
{
	while (ip->sqelem.sqe_next->type != IP_EPILOG)
		ip = ip->sqelem.sqe_next;
	if (ip->type != IP_NODE || ip->ip_node->n_op != GOTO)
		cerror("myoptim");
	tfree(ip->ip_node);
	*ip = *ip->sqelem.sqe_next;
}
