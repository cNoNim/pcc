/*	$Id$	*/
/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
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

#include "pass2.h"

#include <string.h>
#include <stdlib.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

static int dfsnum;

void saveip(struct interpass *ip);
void deljumps(void);
void deltemp(NODE *p);
void optdump(struct interpass *ip);
void printip(struct interpass *pole);

static struct varinfo defsites;
static struct interpass ipole;
struct interpass *storesave;

int bblocks_build(struct labelinfo *labinfo, struct bblockinfo *bbinfo);
void cfg_build(struct labelinfo *labinfo);
void cfg_dfs(struct basicblock *bb, unsigned int parent, 
	     struct bblockinfo *bbinfo);
void dominators(struct bblockinfo *bbinfo);
struct basicblock *
ancestorwithlowestsemi(struct basicblock *bblock, struct bblockinfo *bbinfo);
void link(struct basicblock *parent, struct basicblock *child);
void computeDF(struct basicblock *bblock, struct bblockinfo *bbinfo);
void findTemps(struct interpass *ip);
void placePhiFunctions(struct bblockinfo *bbinfo);
void remunreach(void);

static struct basicblock bblocks;

void
saveip(struct interpass *ip)
{
	struct interpass_prolog *ipp, *epp;
	static int inftn;

#if 0
	int regs;
#endif
	struct labelinfo labinfo;
	struct bblockinfo bbinfo;

	if (ip->type == IP_PROLOG) {
		DLIST_INIT(&ipole, qelem);
		inftn = 1;
	} else if (inftn == 0)
		comperr("saveip");

	DLIST_INSERT_BEFORE(&ipole, ip, qelem);

	if (ip->type != IP_EPILOG)
		return;
	inftn = 0;
	epp = (struct interpass_prolog *)ip;

	if (xdeljumps)
		deljumps();	/* Delete redundant jumps and dead code */

	if (xssaflag) {
		DLIST_INIT(&bblocks, bbelem);
		if (bblocks_build(&labinfo, &bbinfo)) {
			cfg_build(&labinfo);
			dominators(&bbinfo);
			computeDF(DLIST_NEXT(&bblocks, bbelem), &bbinfo);
			remunreach();
#if 0
			if (xssaflag) {
				dfg = dfg_build(cfg);
				ssa = ssa_build(cfg, dfg);
			}
#endif
		}
 
	}

#ifdef PCC_DEBUG
	if (epp->ipp_regs != MAXRVAR)
		comperr("register error");
#endif

	ipp = (struct interpass_prolog *)DLIST_NEXT(&ipole, qelem);
	ipp->ipp_autos = epp->ipp_autos;
	ipp->ipp_regs = epp->ipp_regs; // = regs;

#ifdef MYOPTIM
	myoptim((struct interpass *)ipp);
#endif

if (xnewreg == 0) {
	int tmpautooff;
	NODE *p;

	p2autooff = p2maxautooff = AUTOINIT;
	/* Must verify stack usage first */
	DLIST_FOREACH(ip, &ipole, qelem) {
		if (ip->type == IP_STKOFF) {
			p2autooff = ip->ip_off;
			if (p2autooff > p2maxautooff)
				p2maxautooff = p2autooff;
		}
	}
	p2autooff = p2maxautooff; /* don't have live range analysis yet */

	DLIST_FOREACH(ip, &ipole, qelem) {
		if (ip->type == IPSTK) {
			struct interpass *ip3;
			p2autooff = ip->ip_off;
			ip3 = ip;
			ip = DLIST_NEXT(ip, qelem);
			DLIST_REMOVE(ip3, qelem);
		}
			
		if (ip->type != IP_NODE)
			continue;

		p = ip->ip_node;
		walkf(p, deltemp);

		tmpautooff = p2autooff;

		geninsn(p, FOREFF);
		if (sucomp(p) < 0) {
			/* Create STKOFF entry */
			struct interpass *ip3;
			DLIST_INSERT_BEFORE(ip, storesave, qelem);
			ip3 = ipnode(NULL);
			ip3->type = IPSTK;
			ip3->ip_off = tmpautooff;
			DLIST_INSERT_AFTER(ip, ip3, qelem);
			ip = DLIST_PREV(storesave, qelem);
			continue;
		}

		p2autooff = tmpautooff;

		genregs(p);
		mygenregs(p);
	}

} else {
	/*
	 * Loop over instruction assignment until the register assignment
	 * code is satisfied.
	 */
#if 0
	extern int tempmin, tempmax;

	tempmin = ip->ip_tmpnum;
	tempmin = ie->ip_tmpnum;
#endif
	do {
		/* do instruction assignment */
		DLIST_FOREACH(ip, &ipole, qelem) {
			if (ip->type != IP_NODE)
				continue;
			geninsn(ip->ip_node, FOREFF);
			nsucomp(ip->ip_node);
		}
	} while (ngenregs(&ipole));
}

	DLIST_FOREACH(ip, &ipole, qelem) {
		emit(ip);
	}
	DLIST_INIT(&ipole, qelem);
}

/*
 * Delete unused labels, excess of labels, gotos to gotos.
 * This routine can be made much more efficient.
 */
void
deljumps()
{
	struct interpass_prolog *ipp, *epp;
	struct interpass *ip, *n, *ip2;
	int gotone,low, high;
	int *lblary, sz, o, i;

	ipp = (struct interpass_prolog *)DLIST_NEXT(&ipole, qelem);
	epp = (struct interpass_prolog *)DLIST_PREV(&ipole, qelem);

	low = ipp->ip_lblnum;
	high = epp->ip_lblnum;

#ifdef notyet
	mark = tmpmark(); /* temporary used memory */
#endif

	sz = (high-low) * sizeof(int);
	lblary = tmpalloc(sz);

again:	gotone = 0;
	memset(lblary, 0, sz);

	/* refcount and coalesce  all labels */
	DLIST_FOREACH(ip, &ipole, qelem) {
		if (ip->type == IP_DEFLAB) {
			n = DLIST_NEXT(ip, qelem);
			while (n->type == IP_DEFLAB || n->type == IP_STKOFF) {
				if (n->type == IP_DEFLAB &&
				    lblary[n->ip_lbl-low] >= 0)
					lblary[n->ip_lbl-low] = -ip->ip_lbl;
				n = DLIST_NEXT(n, qelem);
			}
		}
		if (ip->type != IP_NODE)
			continue;
		o = ip->ip_node->n_op;
		if (o == GOTO)
			i = ip->ip_node->n_left->n_lval;
		else if (o == CBRANCH)
			i = ip->ip_node->n_right->n_lval;
		else
			continue;
		lblary[i-low] |= 1;
	}

	/* delete coalesced/unused labels and rename gotos */
	DLIST_FOREACH(ip, &ipole, qelem) {
		n = DLIST_NEXT(ip, qelem);
		if (n->type == IP_DEFLAB) {
			if (lblary[n->ip_lbl-low] <= 0) {
				DLIST_REMOVE(n, qelem);
				gotone = 1;
			}
			continue;
		}
		if (n->type != IP_NODE)
			continue;
		o = n->ip_node->n_op;
		if (o == GOTO)
			i = n->ip_node->n_left->n_lval;
		else if (o == CBRANCH)
			i = n->ip_node->n_right->n_lval;
		else
			continue;
		if (lblary[i-low] < 0) {
			if (o == GOTO)
				n->ip_node->n_left->n_lval = -lblary[i-low];
			else
				n->ip_node->n_right->n_lval = -lblary[i-low];
		}
	}

	/* Delete gotos to the next statement */
	DLIST_FOREACH(ip, &ipole, qelem) {
		n = DLIST_NEXT(ip, qelem);
		if (n->type != IP_NODE)
			continue;
		o = n->ip_node->n_op;
		if (o == GOTO)
			i = n->ip_node->n_left->n_lval;
		else if (o == CBRANCH)
			i = n->ip_node->n_right->n_lval;
		else
			continue;
		ip2 = DLIST_NEXT(n, qelem);
		if (ip2->type != IP_DEFLAB)
			continue;
		if (ip2->ip_lbl == i) {
			tfree(n->ip_node);
			DLIST_REMOVE(n, qelem);
			gotone = 1;
		}
	}

	if (gotone)
		goto again;

#ifdef notyet
	tmpfree(mark);
#endif
}

void
optdump(struct interpass *ip)
{
	static char *nm[] = { "node", "prolog", "newblk", "epilog", "locctr",
		"deflab", "defnam", "asm" };
	printf("type %s\n", nm[ip->type-1]);
	switch (ip->type) {
	case IP_NODE:
		fwalk(ip->ip_node, e2print, 0);
		break;
	case IP_DEFLAB:
		printf("label " LABFMT "\n", ip->ip_lbl);
		break;
	case IP_ASM:
		printf(": %s\n", ip->ip_asm);
		break;
	}
}

/*
 * Build the basic blocks, algorithm 9.1, pp 529 in Compilers.
 *
 * Also fills the labelinfo struct with information about which bblocks
 * that contain which label.
 */

int
bblocks_build(struct labelinfo *labinfo, struct bblockinfo *bbinfo)
{
	struct interpass *ip;
	struct basicblock *bb = NULL;
	int leader = 1;
	unsigned int low, high = 0;
	int count = 0;
	int i;

	low = sizeof(int) == 2 ? 65000 : 1000000000;
	/* 
	 * First statement is a leader.
	 * Any statement that is target of a jump is a leader.
	 * Any statement that immediately follows a jump is a leader.
	 */

	DLIST_FOREACH(ip, &ipole, qelem) {
		/* Garbage, skip it */
		if (leader) {
			bb = tmpalloc(sizeof(struct basicblock));
			bb->first = bb->last = ip;
			SLIST_INIT(&bb->children);
			SLIST_INIT(&bb->parents);
			bb->dfnum = 0;
			bb->dfparent = 0;
			bb->semi = 0;
			bb->ancestor = 0;
			bb->idom = 0;
			bb->samedom = 0;
			bb->bucket = NULL;
			bb->df = NULL;
			bb->dfchildren = NULL;
			bb->Aorig = NULL;
			bb->Aphi = NULL;
			DLIST_INSERT_BEFORE(&bblocks, bb, bbelem);
			leader = 0;
			count++;
		} 
		
		/* Prologue and epilogue in their own bblock */
		if ((ip->type == IP_PROLOG) || (ip->type == IP_EPILOG)) {
			bb->last = ip;
			if (ip->type == IP_EPILOG)
				high = MAX(ip->ip_lbl, high);
			leader = 1;
			continue;
		}
		
		/* Keep track of highest and lowest label number */
		if (ip->type == IP_DEFLAB) {
			low = MIN(ip->ip_lbl, low);
			high = MAX(ip->ip_lbl, high);
		}

		/* Make sure each label is in a unique bblock */
		if (((ip->type == IP_DEFLAB) || (ip->type == IP_DEFNAM)) && 
		    bb->first != ip) {
			bb = tmpalloc(sizeof(struct basicblock));
			bb->first = bb->last = ip;
			SLIST_INIT(&bb->children);
			SLIST_INIT(&bb->parents);
			bb->dfnum = 0;
			bb->dfparent = 0;
			bb->semi = 0;
			bb->ancestor = 0;
			bb->idom = 0;
			bb->samedom = 0;
			bb->bucket = NULL;
			bb->df = NULL;
			bb->dfchildren = NULL;
			bb->Aorig = NULL;
			bb->Aphi = NULL;
			DLIST_INSERT_BEFORE(&bblocks, bb, bbelem);
			count++;
			continue;
		}

		if (ip->type == IP_ASM)
			return 0;

		if (ip->type == IP_NODE) {
			switch(ip->ip_node->n_op) {
			case CBRANCH:
			case GOTO:
			case RETURN:
				/* Jumps, last in bblock. */
				leader = 1;
				break;

			case NAME:
			case ICON:
			case FCON:
			case REG:
			case OREG:
			case MOVE:
			case PLUS:
			case MINUS:
			case DIV:
			case MOD:
			case MUL:
			case AND:
			case OR:
			case ER:
			case LS:
			case COMPL:
			case INCR:
			case DECR:
			case UMUL:
			case UMINUS:
			case EQ:
			case NE:
			case LE:
			case GE:
			case GT:
			case ULE:
			case ULT:
			case UGE:
			case UGT:
			case ASSIGN:
			case FORCE:
			case FUNARG:
			case CALL:
			case UCALL:
			case FORTCALL:
			case UFORTCALL:
			case STCALL:
			case USTCALL:
				/* Not jumps, continue with bblock. */
				break;

			default:
				comperr("optim2:bblocks_build() %d",ip->ip_node->n_op ); 
				break;
			}
		}

		bb->last = ip;
	}
#if 0
	printf("Basic blocks in func: %d\n", count);
	printf("Label range in func: %d\n", high - low + 1);
#endif

	labinfo->low = low;
	labinfo->size = high - low + 1;
	labinfo->arr = tmpalloc(labinfo->size * sizeof(struct basicblock *));
	for (i = 0; i <= labinfo->size; i++) {
		labinfo->arr[i] = NULL;
	}
	
	bbinfo->size = count + 1;
	bbinfo->arr = tmpalloc(bbinfo->size * sizeof(struct basicblock *));
	for (i = 0; i <= bbinfo->size; i++) {
		bbinfo->arr[i] = NULL;
	}

	DLIST_FOREACH(bb, &bblocks, bbelem) {
		/* Build the label table */
		if (bb->first->type == IP_DEFLAB) {
			labinfo->arr[bb->first->ip_lbl - low] = bb;
		}
		if (bb->first->type == IP_EPILOG) {
			labinfo->arr[bb->first->ip_lbl - low] = bb;
		}
	}

	return 1;
}

/*
 * Build the control flow graph.
 */

void
cfg_build(struct labelinfo *labinfo)
{
	/* Child and parent nodes */
	struct cfgnode *cnode; 
	struct cfgnode *pnode;
	struct basicblock *bb;
	
	DLIST_FOREACH(bb, &bblocks, bbelem) {

		if (bb->first->type == IP_EPILOG) {
			break;
		}

		cnode = tmpalloc(sizeof(struct cfgnode));
		pnode = tmpalloc(sizeof(struct cfgnode));
		pnode->bblock = bb;

		if ((bb->last->type == IP_NODE) && 
		    (bb->last->ip_node->n_op == GOTO)) {
			if (bb->last->ip_node->n_left->n_lval - labinfo->low > 
			    labinfo->size) {
				comperr("Label out of range: %d, base %d", 
					bb->last->ip_node->n_left->n_lval, 
					labinfo->low);
			}
			cnode->bblock = labinfo->arr[bb->last->ip_node->n_left->n_lval - labinfo->low];
			SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
			SLIST_INSERT_LAST(&bb->children, cnode, cfgelem);
			continue;
		}
		if ((bb->last->type == IP_NODE) && 
		    (bb->last->ip_node->n_op == CBRANCH)) {
			if (bb->last->ip_node->n_right->n_lval - labinfo->low > 
			    labinfo->size) 
				comperr("Label out of range: %d", 
					bb->last->ip_node->n_left->n_lval);
			
			cnode->bblock = labinfo->arr[bb->last->ip_node->n_right->n_lval - labinfo->low];
			SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
			SLIST_INSERT_LAST(&bb->children, cnode, cfgelem);
			cnode = tmpalloc(sizeof(struct cfgnode));
			pnode = tmpalloc(sizeof(struct cfgnode));
			pnode->bblock = bb;
		}

		cnode->bblock = DLIST_NEXT(bb, bbelem);
		SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
		SLIST_INSERT_LAST(&bb->children, cnode, cfgelem);
	}
}

void
cfg_dfs(struct basicblock *bb, unsigned int parent, struct bblockinfo *bbinfo)
{
	struct cfgnode *cnode;
	
	if (bb->dfnum != 0)
		return;

	bb->dfnum = ++dfsnum;
	bb->dfparent = parent;
	bbinfo->arr[bb->dfnum] = bb;
	SLIST_FOREACH(cnode, &bb->children, cfgelem) {
		cfg_dfs(cnode->bblock, bb->dfnum, bbinfo);
	}
	/* Don't bring in unreachable nodes in the future */
	bbinfo->size = dfsnum + 1;
}

static bittype *
setalloc(int nelem)
{
	bittype *b;
	int sz = (nelem+NUMBITS-1)/NUMBITS;

	b = tmpalloc(sz * sizeof(bittype));
	memset(b, 0, sz * sizeof(bittype));
	return b;
}

/*
 * Algorithm 19.9, pp 414 from Appel.
 */

void
dominators(struct bblockinfo *bbinfo)
{
	struct cfgnode *cnode;
	struct basicblock *bb, *y, *v;
	struct basicblock *s, *sprime, *p;
	int h, i;

	DLIST_FOREACH(bb, &bblocks, bbelem) {
		bb->bucket = setalloc(bbinfo->size);
		bb->df = setalloc(bbinfo->size);
		bb->dfchildren = setalloc(bbinfo->size);
	}

	dfsnum = 0;
	cfg_dfs(DLIST_NEXT(&bblocks, bbelem), 0, bbinfo);

#if 0
	{ struct basicblock *bbb; struct cfgnode *ccnode;
	DLIST_FOREACH(bbb, &bblocks, bbelem) {
		printf("Basic block %d, parents: ", bbb->dfnum);
		SLIST_FOREACH(ccnode, &bbb->parents, cfgelem) {
			printf("%d, ", ccnode->bblock->dfnum);
		}
		printf("\nChildren: ");
		SLIST_FOREACH(ccnode, &bbb->children, cfgelem) {
			printf("%d, ", ccnode->bblock->dfnum);
		}
		printf("\n");
	}
	}
#endif

	for(h = bbinfo->size - 1; h > 1; h--) {
		bb = bbinfo->arr[h];
		p = s = bbinfo->arr[bb->dfparent];
		SLIST_FOREACH(cnode, &bb->parents, cfgelem) {
			if (cnode->bblock->dfnum <= bb->dfnum) 
				sprime = cnode->bblock;
			else 
				sprime = bbinfo->arr[ancestorwithlowestsemi
					      (cnode->bblock, bbinfo)->semi];
			if (sprime->dfnum < s->dfnum)
				s = sprime;
		}
		bb->semi = s->dfnum;
		BITSET(s->bucket, bb->dfnum);
		link(p, bb);
		for (i = 1; i < bbinfo->size; i++) {
			if(TESTBIT(p->bucket, i)) {
				v = bbinfo->arr[i];
				y = ancestorwithlowestsemi(v, bbinfo);
				if (y->semi == v->semi) 
					v->idom = p->dfnum;
				else
					v->samedom = y->dfnum;
			}
		}
		memset(p->bucket, 0, (bbinfo->size + 7)/8);
	}
#if 0
	printf("Num\tSemi\tAncest\tidom\n");
	CIRCLEQ_FOREACH(bb, &bblocks, bbelem) {
		printf("%d\t%d\t%d\t%d\n", bb->dfnum, bb->semi, bb->ancestor, bb->idom);
	}
#endif
	for(h = 2; h < bbinfo->size; h++) {
		bb = bbinfo->arr[h];
		if (bb->samedom != 0) {
			bb->idom = bbinfo->arr[bb->samedom]->idom;
		}
	}
	DLIST_FOREACH(bb, &bblocks, bbelem) {
		if (bb->idom != 0 && bb->idom != bb->dfnum) {
#if 0

			printf("Setting child %d of %d\n", bb->dfnum, bbinfo->arr[bb->idom]->dfnum);
#endif

			BITSET(bbinfo->arr[bb->idom]->dfchildren, bb->dfnum);
		}
	}
}


struct basicblock *
ancestorwithlowestsemi(struct basicblock *bblock, struct bblockinfo *bbinfo)
{
	struct basicblock *u = bblock;
	struct basicblock *v = bblock;

	while (v->ancestor != 0) {
		if (bbinfo->arr[v->semi]->dfnum < 
		    bbinfo->arr[u->semi]->dfnum) 
			u = v;
		v = bbinfo->arr[v->ancestor];
	}
	return u;
}

void
link(struct basicblock *parent, struct basicblock *child)
{
	child->ancestor = parent->dfnum;
}

void
computeDF(struct basicblock *bblock, struct bblockinfo *bbinfo)
{
	struct cfgnode *cnode;
	int h, i;
	
	SLIST_FOREACH(cnode, &bblock->children, cfgelem) {
		if (cnode->bblock->idom != bblock->dfnum)
			BITSET(bblock->df, cnode->bblock->dfnum);
	}
	for (h = 1; h < bbinfo->size; h++) {
		if (!TESTBIT(bblock->dfchildren, h))
			continue;
		computeDF(bbinfo->arr[h], bbinfo);
		for (i = 1; i < bbinfo->size; i++) {
			if (TESTBIT(bbinfo->arr[h]->df, i) && 
			    (bbinfo->arr[h] == bblock ||
			     (bblock->idom != bbinfo->arr[h]->dfnum))) 
			    BITSET(bblock->df, i);
		}
	}
}

static struct basicblock *currbb;
static struct interpass *currip;

/* Helper function for findTemps, Find assignment nodes. */
static void
findasg(NODE *p)
{
	struct pvarinfo *pv;

	if (p->n_op != ASSIGN)
		return;

	if (p->n_left->n_op != TEMP)
		return;

	pv = tmpcalloc(sizeof(struct pvarinfo));
	pv->next = defsites.arr[p->n_left->n_lval];
	pv->bb = currbb;
	pv->top = currip->ip_node;
	pv->n = p->n_left;
	BITSET(currbb->Aorig, p->n_left->n_lval);

	defsites.arr[p->n_left->n_lval] = pv;
}

/* Walk the interpass looking for assignment nodes. */
void findTemps(struct interpass *ip)
{
	if (ip->type != IP_NODE)
		return;

	currip = ip;

	walkf(ip->ip_node, findasg);
}

/*
 * Algorithm 19.6 from Appel.
 */

void
placePhiFunctions(struct bblockinfo *bbinfo)
{
	struct basicblock *bb;
	struct interpass *ip;
	int maxtmp, i, j, k, l;
	struct pvarinfo *n;
	struct cfgnode *cnode;
	TWORD ntype;
	NODE *p;
	struct pvarinfo *pv;

	bb = DLIST_NEXT(&bblocks, bbelem);
	defsites.low = ((struct interpass_prolog *)bb->first)->ip_tmpnum;
	bb = DLIST_PREV(&bblocks, bbelem);
	maxtmp = ((struct interpass_prolog *)bb->first)->ip_tmpnum;
	defsites.size = maxtmp - defsites.low + 1;
	defsites.arr = tmpcalloc(defsites.size*sizeof(struct pvarinfo *));

	/* Find all defsites */
	DLIST_FOREACH(bb, &bblocks, bbelem) {
		currbb = bb;
		ip = bb->first;
		bb->Aorig = setalloc(defsites.size);
		bb->Aphi = setalloc(defsites.size);
		

		while (ip != bb->last) {
			findTemps(ip);
			ip = DLIST_NEXT(ip, qelem);
		}
		/* Make sure we get the last statement in the bblock */
		findTemps(ip);
	}
	/* For each variable */
	for (i = defsites.low; i < defsites.size; i++) {
		/* While W not empty */
		while (defsites.arr[i] != NULL) {
			/* Remove some node n from W */
			n = defsites.arr[i];
			defsites.arr[i] = n->next;
			/* For each y in n->bb->df */
			for (j = 0; j < bbinfo->size; j++) {
				if (!TESTBIT(n->bb->df, j))
					continue;
				
				if (TESTBIT(bbinfo->arr[j]->Aphi, i))
					continue;

				ntype = n->n->n_type;
				k = 0;
				/* Amount of predecessors for y */
				SLIST_FOREACH(cnode, &n->bb->parents, cfgelem) 
					k++;
				/* Construct phi(...) */
				p = mklnode(TEMP, i, 0, ntype);
				for (l = 0; l < k-1; l++)
					p = mkbinode(PHI, p,
					    mklnode(TEMP, i, 0, ntype), ntype);
				ip = ipnode(mkbinode(ASSIGN,
				    mklnode(TEMP, i, 0, ntype), p, ntype));
				/* Insert phi at top of basic block */
				DLIST_INSERT_BEFORE(((struct interpass*)&n->bb->first), ip, qelem);
				n->bb->first = ip;
				BITSET(bbinfo->arr[j]->Aphi, i);
				if (!TESTBIT(bbinfo->arr[j]->Aorig, i)) {
					pv = tmpalloc(sizeof(struct pvarinfo));
					// XXXpj Ej fullständig information.
					pv->bb = bbinfo->arr[j];
					pv->next = defsites.arr[i]->next;
					defsites.arr[i] = pv;
				}
					

			}
		}
	}
}

/*
 * Remove unreachable nodes in the CFG.
 */ 

void
remunreach(void)
{
	struct basicblock *bb, *nbb;
	struct interpass *next, *ctree;

	bb = DLIST_NEXT(&bblocks, bbelem);
	while (bb != &bblocks) {
		nbb = DLIST_NEXT(bb, bbelem);

		/* Code with dfnum 0 is unreachable */
		if (bb->dfnum != 0) {
			bb = nbb;
			continue;
		}

		/* Need the epilogue node for other parts of the
		   compiler, set its label to 0 and backend will
		   handle it. */ 
		if (bb->first->type == IP_EPILOG) {
			bb->first->ip_lbl = 0;
			bb = nbb;
			continue;
		}

		next = bb->first;
		do {
			ctree = next;
			next = DLIST_NEXT(ctree, qelem);
			
			if (ctree->type == IP_NODE)
				tfree(ctree->ip_node);
			DLIST_REMOVE(ctree, qelem);
		} while (ctree != bb->last);
			
		DLIST_REMOVE(bb, bbelem);
		bb = nbb;
	}
}

void
printip(struct interpass *pole)
{
	static char *foo[] = {
	   0, "NODE", "PROLOG", "STKOFF", "EPILOG", "DEFLAB", "DEFNAM", "ASM" };
	struct interpass *ip;

	DLIST_FOREACH(ip, pole, qelem) {
		if (ip->type > MAXIP)
			printf("IP(%d) (%p): ", ip->type, ip);
		else
			printf("%s (%p): ", foo[ip->type], ip);
		switch (ip->type) {
		case IP_NODE: printf("\n");
			fwalk(ip->ip_node, e2print, 0); break;
		case IP_PROLOG:
			printf("%s\n",
			    ((struct interpass_prolog *)ip)->ipp_name); break;
		case IP_STKOFF: printf("%d\n", ip->ip_off); break;
		case IP_EPILOG: printf("\n"); break;
		case IP_DEFLAB: printf(LABFMT "\n", ip->ip_lbl); break;
		}
	}
}
