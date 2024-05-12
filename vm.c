#include <stdio.h>

#include "utils.h"
#include "ad.h"

Instr *addInstr(Instr **list, Opcode op)
{
	Instr *i = (Instr *)safeAlloc(sizeof(Instr));
	i->op = op;
	i->next = NULL;
	if (*list)
	{
		Instr *p = *list;
		while (p->next)
			p = p->next;
		p->next = i;
	}
	else
	{
		*list = i;
	}
	return i;
}

Instr *addInstrWithInt(Instr **list, Opcode op, int argVal)
{
	Instr *i = addInstr(list, op);
	i->arg.i = argVal;
	return i;
}

Instr *addInstrWithDouble(Instr **list, Opcode op, double argVal)
{
	Instr *i = addInstr(list, op);
	i->arg.f = argVal;
	return i;
}

Val stack[10000];	 // the stack
Val *SP = stack - 1; // Stack pointer - the stack's top - points to the value from the top of the stack
Val *FP = NULL;		 // the initial value doesn't matter

void pushv(Val v)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	*++SP = v;
}

Val popv()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	return *SP--;
}

void pushi(int i)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	(++SP)->i = i;
}

// nou
void pushf(double f)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	(++SP)->f = f;
}

int popi()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	return SP--->i;
}

// nou
double popf()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	return SP--->f;
}

void pushp(void *p)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	(++SP)->p = p;
}

void *popp()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	return SP--->p;
}

void put_i()
{
	printf("=> %d", popi());
}

// nou - trebuia numit put_d
void put_d()
{
	printf("=> %g", popf());
}

void vmInit()
{
	Symbol *fn = addExtFn("put_i", put_i, (Type){TB_VOID, NULL, -1});
	addFnParam(fn, "i", (Type){TB_INT, NULL, -1});

	// nou
	Symbol *fn_put_d = addExtFn("put_d", put_d, (Type){TB_VOID, NULL, -1});
	addFnParam(fn_put_d, "f", (Type){TB_DOUBLE, NULL, -1});
}

void run(Instr *IP)
{
	Val v;
	int iArg, iTop, iBefore;
	double fTop, fBefore; // nou
	void (*extFnPtr)();
	for (;;)
	{
		// shows the index of the current instruction and the number of values from stack
		printf("%p/%d\t", IP, (int)(SP - stack + 1));
		switch (IP->op)
		{
		case OP_HALT:
			printf("HALT");
			return;
		case OP_PUSH_I:
			printf("PUSH.i\t%d", IP->arg.i);
			pushi(IP->arg.i);
			IP = IP->next;
			break;
		// nou
		case OP_PUSH_F:
			printf("PUSH.f\t%g", IP->arg.f);
			pushf(IP->arg.f);
			IP = IP->next;
			break;
		case OP_CALL:
			pushp(IP->next);
			printf("CALL\t%p", IP->arg.instr);
			IP = IP->arg.instr;
			break;
		case OP_CALL_EXT:
			extFnPtr = IP->arg.extFnPtr;
			printf("CALL_EXT\t%p\n", extFnPtr);
			extFnPtr();
			IP = IP->next;
			break;
		case OP_ENTER:
			pushp(FP);
			FP = SP;
			SP += IP->arg.i;
			printf("ENTER\t%d", IP->arg.i);
			IP = IP->next;
			break;
		case OP_RET_VOID:
			iArg = IP->arg.i;
			printf("RET_VOID\t%d", iArg);
			IP = FP[-1].p;
			SP = FP - iArg - 2;
			FP = FP[0].p;
			break;
		case OP_JMP:
			printf("JMP\t%p", IP->arg.instr);
			IP = IP->arg.instr;
			break;
		case OP_JF:
			iTop = popi();
			printf("JF\t%p\t// %d", IP->arg.instr, iTop);
			IP = iTop ? IP->next : IP->arg.instr;
			break;
		// nou
		case OP_JF_F:
			fTop = popf();
			printf("JF\t%p\t// %g", IP->arg.instr, fTop);
			IP = (fTop != 0.0) ? IP->next : IP->arg.instr;
			break;
		case OP_FPLOAD:
			v = FP[IP->arg.i];
			pushv(v);
			printf("FPLOAD\t%d\t// i:%d, f:%g", IP->arg.i, v.i, v.f);
			IP = IP->next;
			break;
		// nou
		case OP_FPLOAD_F:
			v = FP[IP->arg.i];
			pushv(v);
			printf("FPLOAD\t%d\t// f:%g", IP->arg.i, v.f);
			IP = IP->next;
			break;
		case OP_FPSTORE:
			v = popv();
			FP[IP->arg.i] = v;
			printf("FPSTORE\t%d\t// i:%d, f:%g", IP->arg.i, v.i, v.f);
			IP = IP->next;
			break;
		// nou
		case OP_FPSTORE_F:
			v = popv();
			FP[IP->arg.i] = v;
			printf("FPSTORE\t%d\t// f:%g", IP->arg.i, v.f);
			IP = IP->next;
			break;
		case OP_ADD_I:
			iTop = popi();
			iBefore = popi();
			pushi(iBefore + iTop);
			printf("ADD.i\t// %d+%d -> %d", iBefore, iTop, iBefore + iTop);
			IP = IP->next;
			break;
		// nou
		case OP_ADD_F:
			fTop = popf();
			fBefore = popf();
			pushf(fBefore + fTop);
			printf("ADD.f\t// %g+%g -> %g", fBefore, fTop, fBefore + fTop);
			IP = IP->next;
			break;
		case OP_LESS_I:
			iTop = popi();
			iBefore = popi();
			pushi(iBefore < iTop);
			printf("LESS.i\t// %d<%d -> %d", iBefore, iTop, iBefore < iTop);
			IP = IP->next;
			break;
		// nou
		case OP_LESS_F:
			fTop = popf();
			fBefore = popf();
			pushf(fBefore < fTop);
			printf("LESS.f\t// %g<%g -> %d", fBefore, fTop, fBefore < fTop);
			IP = IP->next;
			break;
		default:
			err("run: instructiune neimplementata: %d", IP->op);
		}
		putchar('\n');
	}
}

/* The program implements the following AtomC source code:
f(2);
void f(int n){		// stack frame: n[-2] ret[-1] oldFP[0] i[1]
	int i=0;
	while(i<n){
		put_i(i);
		i=i+1;
		}
	}
*/
// Instr *genTestProgram()
// {
// 	Instr *code = NULL;
// 	addInstrWithInt(&code, OP_PUSH_I, 2);
// 	Instr *callPos = addInstr(&code, OP_CALL);
// 	addInstr(&code, OP_HALT);
// 	callPos->arg.instr = addInstrWithInt(&code, OP_ENTER, 1);
// 	// int i=0;
// 	addInstrWithInt(&code, OP_PUSH_I, 0);
// 	addInstrWithInt(&code, OP_FPSTORE, 1);
// 	// while(i<n){
// 	Instr *whilePos = addInstrWithInt(&code, OP_FPLOAD, 1);
// 	addInstrWithInt(&code, OP_FPLOAD, -2);
// 	addInstr(&code, OP_LESS_I);
// 	Instr *jfAfter = addInstr(&code, OP_JF);
// 	// put_i(i);
// 	addInstrWithInt(&code, OP_FPLOAD, 1);
// 	Symbol *s = findSymbol("put_i");
// 	if (!s)
// 		err("undefined: put_i");
// 	addInstr(&code, OP_CALL_EXT)->arg.extFnPtr = s->fn.extFnPtr;
// 	// i=i+1;
// 	addInstrWithInt(&code, OP_FPLOAD, 1);
// 	addInstrWithInt(&code, OP_PUSH_I, 1);
// 	addInstr(&code, OP_ADD_I);
// 	addInstrWithInt(&code, OP_FPSTORE, 1);
// 	// } ( the next iteration)
// 	addInstr(&code, OP_JMP)->arg.instr = whilePos;
// 	// returns from function
// 	jfAfter->arg.instr = addInstrWithInt(&code, OP_RET_VOID, 1);
// 	return code;
// }

// nou
Instr *genTestProgram()
{
	Instr *code = NULL;
	addInstrWithDouble(&code, OP_PUSH_F, 2.0); // adăugăm o valoare double pe stivă
	Instr *callPos = addInstr(&code, OP_CALL);
	addInstr(&code, OP_HALT);
	callPos->arg.instr = addInstrWithInt(&code, OP_ENTER, 1);
	// double i=0.0;
	addInstrWithDouble(&code, OP_PUSH_F, 0.0);
	addInstrWithInt(&code, OP_FPSTORE_F, 1);
	// while(i<n){
	Instr *whilePos = addInstrWithInt(&code, OP_FPLOAD_F, 1);
	addInstrWithDouble(&code, OP_FPLOAD_F, -2);
	addInstr(&code, OP_LESS_F);
	Instr *jfAfter = addInstr(&code, OP_JF_F);
	// put_d(i);
	addInstrWithDouble(&code, OP_FPLOAD_F, 1);
	Symbol *s = findSymbol("put_d");
	if (!s)
		err("undefined: put_d");
	addInstr(&code, OP_CALL_EXT)->arg.extFnPtr = s->fn.extFnPtr;
	// i=i+0.5;
	addInstrWithDouble(&code, OP_FPLOAD_F, 1);
	addInstrWithDouble(&code, OP_PUSH_F, 0.5);
	addInstr(&code, OP_ADD_F);
	addInstrWithInt(&code, OP_FPSTORE_F, 1);
	// } ( the next iteration)
	addInstr(&code, OP_JMP)->arg.instr = whilePos;
	// returns from function
	jfAfter->arg.instr = addInstrWithInt(&code, OP_RET_VOID, 1);
	return code;
}