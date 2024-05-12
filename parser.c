#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "utils.h"
#include "ad.h"
#include "at.h"
#include "vm.h"
#include "parser.h"

Token *iTk;		   // the iterator in the tokens list
Token *consumedTk; // the last consumed token
Symbol *owner = NULL;
void tkerr(const char *fmt, ...);
bool consume(int code);
bool typeBase(Type *t);
bool unit();
bool structDef();
bool varDef();
bool arrayDecl(Type *t);
bool fnDef();
bool fnParam();
bool stm();
bool stmCompound(bool newDomain);
bool expr(Ret *r);
bool exprAssign(Ret *r);
bool exprOr(Ret *r);
bool exprOrPrim(Ret *r);
bool exprAnd(Ret *r);
bool exprAndPrim(Ret *r);
bool exprEq(Ret *r);
bool exprEqPrim(Ret *r);
bool exprRel(Ret *r);
bool exprRelPrim(Ret *r);
bool exprAdd(Ret *r);
bool exprAddPrim(Ret *r);
bool exprMul(Ret *r);
bool exprMulPrim(Ret *r);
bool exprCast(Ret *r);
bool exprUnary(Ret *r);
bool exprPostfix(Ret *r);
bool exprPostfixPrim(Ret *r);
bool exprPrimary(Ret *r);
void parse(Token *tokens);

void tkerr(const char *fmt, ...)
{
	fprintf(stderr, "error in line %d: ", iTk->line);
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

bool consume(int code)
{
	if (iTk->code == code)
	{
		consumedTk = iTk;
		iTk = iTk->next;
		return true;
	}
	return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t)
{
	t->n = -1;
	Token *start = iTk;
	if (consume(TYPE_INT))
	{
		t->tb = TB_INT;
		return true;
	}
	if (consume(TYPE_DOUBLE))
	{
		t->tb = TB_DOUBLE;
		return true;
	}
	if (consume(TYPE_CHAR))
	{
		t->tb = TB_CHAR;
		return true;
	}
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			Token *name = consumedTk;
			t->tb = TB_STRUCT;
			t->s = findSymbol(name->text);
			if (!t->s)
				tkerr("undefined structure: %s", name->text);
			return true;
		}
	}
	iTk = start;
	return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit()
{
	for (;;)
	{
		if (structDef())
		{
		}
		else if (fnDef())
		{
		}
		else if (varDef())
		{
		}
		else
			break;
	}
	if (consume(END))
	{
		return true;
	}
	return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
//* inseamna while
bool structDef()
{
	Token *t = iTk; // stocam pozitia curenta a iTk in t pt a putea reveni la ea in caz ca nu e valid ce se intampla
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			Token *name = consumedTk;
			if (consume(LACC))
			{
				Symbol *s = findSymbolInDomain(symTable, name->text);
				if (s)
				{
					tkerr("symbol redefinition: %s", name->text);
				}
				s = addSymbolToDomain(symTable, newSymbol(name->text, SK_STRUCT));
				s->type.tb = TB_STRUCT;
				s->type.s = s;
				s->type.n = -1;
				pushDomain();
				owner = s;
				for (;;)
				{
					if (varDef())
					{
					}
					else
						break;
				}
				if (consume(RACC))
				{
					if (consume(SEMICOLON))
					{
						owner = NULL;
						dropDomain();
						return true;
					}
					else
						tkerr("Missing ; at structure declaration");
				}
				else
					tkerr("Missing } from structure declaration");
			}
		}
		else
			tkerr("Missing identificator");
	}
	iTk = t;	  // revine la pozitia initiala a iTk
	return false; // nu e valid ce s-a intamplat
}

// varDef: typeBase ID arrayDecl? SEMICOLON
// ? inseamna ca nu vom avea nimic si codul continua
bool varDef()
{
	Token *start = iTk;
	Type t;
	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *name = consumedTk;
			if (arrayDecl(&t))
				if (t.n == 0)
					tkerr("a vector variable must have a specified dimension");

			if (consume(SEMICOLON))
			{
				Symbol *var = findSymbolInDomain(symTable, name->text);

				if (var)
					tkerr("symbol redefinition: %s", name->text);
				var = newSymbol(name->text, SK_VAR);

				var->type = t;
				var->owner = owner;
				addSymbolToDomain(symTable, var);
				if (owner)
				{
					switch (owner->kind)
					{
					case SK_FN:
						var->varIdx = symbolsLen(owner->fn.locals);
						addSymbolToList(&owner->fn.locals, dupSymbol(var));
						break;
					case SK_STRUCT:
						var->varIdx = typeSize(&owner->type);
						addSymbolToList(&owner->structMembers, dupSymbol(var));
						break;
					default:
						break;
					}
				}
				else
				{
					var->varMem = safeAlloc(typeSize(&t));
				}
				return true;
			}
			else
				tkerr("Missing ; at the variable declaration");
		}
		else
			tkerr("Missing variable name");
	}
	iTk = start;
	return false;
}

bool arrayDecl(Type *t)
{
	Token *start = iTk;
	if (consume(LBRACKET))
	{
		if (consume(INT))
		{
			Token *tkSize = consumedTk;
			t->n = tkSize->i; // stocheaza nr de elemente din array
		}
		else
		{
			t->n = 0;
		}
		if (consume(RBRACKET))
			return true;
		else
			tkerr("Missing ] at array declaration");
	}
	iTk = start;
	return false;
}

// declarare functie cu parametri si bloc de instructiuni in interior
bool fnDef()
{
	Type t;
	Token *start = iTk;
	if (typeBase(&t) || consume(VOID))
	{
		if (consumedTk->code == VOID)
		{
			t.tb = TB_VOID;
		}

		if (consume(ID))
		{
			Token *name = consumedTk;
			if (consume(LPAR))
			{
				Symbol *fn = findSymbolInDomain(symTable, name->text);
				if (fn)
					tkerr("symbol redefinition: %s", name->text);
				fn = newSymbol(name->text, SK_FN);

				fn->type = t;
				addSymbolToDomain(symTable, fn);
				owner = fn;
				pushDomain();
				if (fnParam())
				{
					for (;;)
					{
						if (consume(COMMA))
						{
							if (fnParam())
							{
							}
							else
							{
								iTk = start;
								tkerr("Error at function parameters declaration");
								return false;
							}
						}
						else if (fnParam())
						{
							tkerr("missing ,");
						}
						else
							break;
					}
				}
				if (consume(RPAR))
				{
					if (stmCompound(false))
					{
						dropDomain();
						owner = NULL;
						return true;
					}
				}
				else
					tkerr("Missing ) from function definition");
			}
		}
	}

	iTk = start;
	return false;
}

// fnParam: typeBase ID arrayDecl?
// declarare functie cu parametri
bool fnParam()
{
	Token *start = iTk;
	Type t;
	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *name = consumedTk;
			if (arrayDecl(&t))
			{
				t.n = 0;
			}

			Symbol *param = findSymbolInDomain(symTable, name->text);
			if (param)
				tkerr("symbol redefinition: %s", name->text);
			param = newSymbol(name->text, SK_PARAM);

			param->type = t;
			param->owner = owner;
			param->paramIdx = symbolsLen(owner->fn.params);
			// parametrul este adaugat atat la domeniul curent, cat si la parametrii fn
			addSymbolToDomain(symTable, param);
			addSymbolToList(&owner->fn.params, dupSymbol(param));
			return true;
		}
		else
			tkerr("Missing ID from function parameters");
	}
	iTk = start;
	return false;
}

/*stm: stmCompound
| IF LPAR expr RPAR stm ( ELSE stm )?
| WHILE LPAR expr RPAR stm
| RETURN expr? SEMICOLON
| expr? SEMICOLON
*/

// interiorul unei functii
bool stm()
{
	Ret rCond, rExpr;
	Token *start = iTk;

	if (stmCompound(true))
	{
		return true;
	}
	if (consume(IF))
	{
		if (consume(LPAR))
		{
			if (expr(&rCond))
			{
				if (!canBeScalar(&rCond))
				{
					tkerr("the if condition must be a scalar value");
				}

				if (consume(RPAR))
				{
					if (stm())
					{
						if (consume(ELSE))
						{
							if (stm())
							{
							}
						}
						return true;
					}
				}
				else
				{
					tkerr("Missing ) at if loop");
				}
			}
			else
			{
				tkerr("Invalid expression at if loop");
			}
		}
		else
		{
			tkerr("Mission ( at if loop");
		}
	}

	if (consume(WHILE))
	{
		if (consume(LPAR))
		{
			if (expr(&rCond))
			{
				if (!canBeScalar(&rCond))
				{
					tkerr("the while condition must be a scalar value");
				}

				if (consume(RPAR))
				{
					if (stm())
						return true;
				}
				else
				{
					tkerr("Missing ) at while loop");
				}
			}
			else
			{
				tkerr("Invalid expression at while loop");
			}
		}
		else
		{
			tkerr("Missing ( at while loop");
		}
	}

	if (consume(RETURN))
	{
		if (expr(&rExpr))
		{
			if (owner->type.tb == TB_VOID)
			{
				tkerr("a void function cannot return a value");
			}
			if (!canBeScalar(&rExpr))
			{
				tkerr("the return value must be a scalar value");
			}
			if (!convTo(&rExpr.type, &owner->type))
			{
				tkerr("cannot convert the return expression type to the function return type");
			}
		}
		else
		{
			if (owner->type.tb != TB_VOID)
			{
				tkerr("a non-void function must return a value");
			}
		}

		if (consume(SEMICOLON))
			return true;
		else
			tkerr("Missing ; from the return statement");
	}

	if (expr(&rExpr))
	{
		if (consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("Lipseste ';'");
		}
		// iTk=start;
	}

	if (consume(SEMICOLON))
		return true;

	iTk = start;
	return false;
}

// acoladele {interior functie}
bool stmCompound(bool newDomain)
{
	Token *start = iTk;

	if (consume(LACC))
	{
		if (newDomain)
		{
			pushDomain();
		}

		for (;;)
		{
			if (varDef())
			{
			}
			else if (stm())
			{
			}
			else
				break;
		}
		if (consume(RACC))
		{
			if (newDomain)
			{
				dropDomain();
			}
			return true;
		}
		else
			tkerr("Missing } at the end of the function expression");
	}
	iTk = start;
	return false;
}

// expr: exprAssign deci atribuire
bool expr(Ret *r)
{
	Token *start = iTk;

	if (exprAssign(r))
		return true;

	iTk = start;
	return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
// x = y || z
bool exprAssign(Ret *r)
{
	Ret rDst;
	Token *start = iTk;

	if (exprUnary(&rDst))
	{
		if (consume(ASSIGN))
		{
			if (exprAssign(r))
			{
				if (!rDst.lval)
				{
					tkerr("the assign destination must be a left-value");
				}

				if (rDst.ct)
				{
					tkerr("the assign destination cannot be constant");
				}

				if (!canBeScalar(&rDst))
				{
					tkerr("the assign destination must be scalar");
				}

				if (!canBeScalar(r))
				{
					tkerr("the assign source must be scalar");
				}

				if (!convTo(&r->type, &rDst.type))
				{
					tkerr("the assign source cannot be converted to destination");
				}

				r->lval = false;
				r->ct = true;
				return true;
			}
			else
			{
				tkerr("Invalid expression for the assign operation");
			}
		}
	}

	iTk = start;

	if (exprOr(r))
		return true;

	iTk = start;
	return false;
}

// ||
bool exprOr(Ret *r)
{
	Token *start = iTk;
	if (exprAnd(r))
	{
		if (exprOrPrim(r))
			return true;
		else
			tkerr("Invalid expression for the and expression");
	}

	iTk = start;
	return false;
}

// exprOr: exprOr OR exprAnd | exprAnd
//
// exprOr: exprAnd exprOrPrim
// exprOrPrim: OR exprAnd exprOrPrim | epsilon
// merge de ex si x || y && z
bool exprOrPrim(Ret *r)
{
	Token *start = iTk;

	if (consume(OR))
	{
		Ret right;

		if (exprAnd(&right))
		{
			Type tDst;

			if (!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("invalid operand type for ||");
			}

			*r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprOrPrim(r))
				return true;
			else
				tkerr("Invalid expression for the and expression");
		}
		else
			tkerr("Invalid expression for the or operator");
	}

	iTk = start;
	return true;
}

// daca ai &&
bool exprAnd(Ret *r)
{
	Token *start = iTk;

	if (exprEq(r))
	{
		if (exprAndPrim(r))
			return true;
		else
			tkerr("Invalid expression for the eq expression");
	}

	iTk = start;
	return false;
}

// exprAnd: exprAnd AND exprEq | exprEq
//
// exprAnd: exprEq exprAndPrim
// exprAndPrim: AND exprEq exprAndPrim | epsilon

// merge si pe x == 10 && y != z de ex
bool exprAndPrim(Ret *r)
{
	Token *start = iTk;

	if (consume(AND))
	{
		Ret right;

		if (exprEq(&right))
		{
			Type tDst;

			if (!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("invalid operand type for &&");
			}

			*r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprAndPrim(r))
				return true;
			else
				tkerr("Invalid expression for the eq expression");
		}
		else
			tkerr("Invalid expression at the and operator");
	}

	iTk = start;

	return true;
}

//!= sau == sau >= sau ..
bool exprEq(Ret *r)
{
	Token *start = iTk;

	if (exprRel(r))
	{
		if (exprEqPrim(r))
			return true;
		else
			tkerr("Invalid expression at the rel expression");
	}

	iTk = start;
	return false;
}

// exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
//
// exprEq: exprRel exprEqPrim
// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim
// poti avea de ex x < 10 != y + z
bool exprEqPrim(Ret *r)
{
	if (consume(EQUAL) || consume(NOTEQ))
	{
		Ret right;

		if (exprRel(&right))
		{
			Type tDst;

			if (!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("invalid operand type for == or !=");
			}

			*r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprEqPrim(r))
				return true;
			else
				tkerr("Invalid expression at the rel expression");
		}
		else
			tkerr("Invalid expression after the symbol");
	}
	return true;
}

// pt comparatii de ex x + y < 10 * z
bool exprRel(Ret *r)
{
	Token *t = iTk;

	if (exprAdd(r)) // add contine si + si - si mul
	{
		if (exprRelPrim(r))
			return true;
	}

	iTk = t;
	return false;
}

bool exprRelPrim(Ret *r)
{
	Token *t = iTk;
	if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ))
	{
		Ret right;

		if (exprAdd(&right))
		{
			Type tDst;

			if (!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("invalid operand type for <, <=, >, >=");
			}

			*r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprRelPrim(r))
				return true;
		}
		else
			tkerr("Missing right operand");
	}

	iTk = t;
	return true;
}

// exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
//
// exprAdd: exprMul exprAddPrim
// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim
// x + y * 2 - z de ex
bool exprAddPrim(Ret *r)
{
	Token *start = iTk;

	if (consume(ADD) || consume(SUB))
	{
		Ret right;

		if (exprMul(&right))
		{
			Type tDst;

			if (!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("invalid operand type for + or -");
			}

			*r = (Ret){tDst, false, true};

			if (exprAddPrim(r))
				return true;
			else
				tkerr("Invalid expression at the multiplication expression");
		}
		else
			tkerr("Invalid expression after the operation");
	}

	iTk = start;
	return true;
}

bool exprAdd(Ret *r)
{
	Token *start = iTk;

	if (exprMul(r))
	{
		if (exprAddPrim(r))
			return true;
		else
			tkerr("Invalid expression at the multiplication expression");
	}

	iTk = start;
	return false;
}

// x * (y / 2) -> x mul cast
bool exprMul(Ret *r)
{
	Token *start = iTk;

	if (exprCast(r))
	{
		if (exprMulPrim(r))
			return true;
		else
			tkerr("Invalid expression at the cast operation");
	}

	iTk = start;
	return false;
}

// exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
//
// exprMul: exprCast exprMulPrim
// exprMulPrim:( MUL | DIV ) exprCast exprMulPrim
bool exprMulPrim(Ret *r)
{
	Token *start = iTk;

	if (consume(MUL) || consume(DIV))
	{
		Ret right;

		if (exprCast(&right))
		{
			Type tDst;

			if (!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("invalid operand type for * or /");
			}

			*r = (Ret){tDst, false, true};

			if (exprMulPrim(r))
				return true;
			else
				tkerr("Invalid expression at the cast operation");
		}
		else
			tkerr("Invalid expression at the operation");
	}

	iTk = start;
	return true;
}
// cast ca si (int)x
bool exprCast(Ret *r)
{
	Token *start = iTk;

	if (consume(LPAR))
	{
		Type t;
		Ret op;

		if (typeBase(&t))
		{
			if (arrayDecl(&t))
			{
			}

			if (consume(RPAR))
			{
				if (exprCast(&op))
				{
					if (t.tb == TB_STRUCT)
					{
						tkerr("cannot convert to a struct type");
					}

					if (op.type.tb == TB_STRUCT)
					{
						tkerr("cannot convert a struct");
					}

					if (op.type.n >= 0 && t.n < 0)
					{
						tkerr("an array can be converted only to another array");
					}

					if (op.type.n < 0 && t.n >= 0)
					{
						tkerr("a scalar can be converted only to another scalar");
					}

					*r = (Ret){t, false, true};
					return true;
				}
				else
					tkerr("Invalid expression at the cast operation");
			}
			else
				tkerr("Missing ) from the cast operation");
		}
	}

	else if (exprUnary(r))
		return true;

	iTk = start;
	return false;
}

//! true sau -ceva.. -array[index] --x
bool exprUnary(Ret *r)
{
	Token *start = iTk;

	if (consume(SUB) || consume(NOT))
	{
		if (exprUnary(r))
		{
			if (!canBeScalar(r))
			{
				tkerr("unary - or ! must have a scalar operand");
			}

			r->lval = false;
			r->ct = true;
			return true;
		}
		else
			tkerr("Invalid expression after the operator");
	}

	if (exprPostfix(r))
		return true;

	iTk = start;
	return false;
}

// pt variabile de la structuri gen array[3].membru
bool exprPostfix(Ret *r)
{
	Token *start = iTk;

	if (exprPrimary(r))
	{
		if (exprPostfixPrim(r))
			return true;
	}

	iTk = start;
	return false;
}

// exprPostfix: exprPostfix LBRACKET expr RBRACKET
//   | exprPostfix DOT ID
//   | exprPrimary
//
// exprPostfix:exprPrimary exprPostfixPrim
// exprPostfixPrim:LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim

// array[index] sau a[index][index] sau obiect.field.field.field..
bool exprPostfixPrim(Ret *r)
{
	Token *start = iTk;

	if (consume(LBRACKET))
	{
		Ret idx;

		if (expr(&idx))
		{
			if (consume(RBRACKET))
			{
				if (r->type.n < 0)
				{
					tkerr("only an array can be indexed");
				}

				Type tInt = {TB_INT, NULL, -1};

				if (!convTo(&idx.type, &tInt))
				{
					tkerr("the index is not convertible to int");
				}

				r->type.n = -1;
				r->lval = true;
				r->ct = false;

				if (exprPostfixPrim(r))
					return true;
				else
					tkerr("Invalid expression after the ] operator");
			}
			else
				tkerr("Missing ] operator from exprPostfix");
		}
		else
			tkerr("Invalid expression after the [ operator ");
	}

	if (consume(DOT))
	{
		if (consume(ID))
		{
			Token *name = consumedTk;
			if (r->type.tb != TB_STRUCT)
			{
				tkerr("a field can only be selected from a struct");
			}

			Symbol *s = findSymbolInList(r->type.s->structMembers, name->text);

			if (!s)
			{
				tkerr("the structure %s does not have a field %s", r->type.s->name, name->text);
			}

			*r = (Ret){s->type, true, s->type.n >= 0};

			if (exprPostfixPrim(r))
				return true;
			else
				tkerr("Invalid expression after ID");
		}
		else
			tkerr("Invalid id after the dot operator");
	}

	iTk = start;
	return true;
}

// apel functie(a,b), (x+2), 3.14, 'hello'
bool exprPrimary(Ret *r)
{
	Token *start = iTk;

	if (consume(ID))
	{
		Token *name = consumedTk;
		Symbol *s = findSymbol(name->text);

		if (!s)
		{
			tkerr("undefined id: %s", name->text);
		}

		if (consume(LPAR))
		{
			if (s->kind != SK_FN)
				tkerr("only a function can be called");

			Ret rArg;
			Symbol *param = s->fn.params;

			if (expr(&rArg))
			{
				if (!param)
				{
					tkerr("too many arguments in function call");
				}

				if (!convTo(&rArg.type, &param->type))
				{
					tkerr("in call, cannot convert the argument type to the parameter type");
				}

				param = param->next;

				for (;;)
				{
					if (consume(COMMA))
					{
						if (expr(&rArg))
						{
							if (!param)
							{
								tkerr("too many arguments in function call");
							}

							if (!convTo(&rArg.type, &param->type))
							{
								tkerr("in call, cannot convert the argument type to the parameter type");
							}

							param = param->next;
						}
					}
					else
					{
						break;
					}
				}
			}

			if (consume(RPAR))
			{
				if (param)
				{
					tkerr("too few arguments in function call");
				}

				*r = (Ret){s->type, false, true};
			}
			else
			{
				if (s->kind == SK_FN)
				{
					tkerr("a function can only be called");
				}

				*r = (Ret){s->type, true, s->type.n >= 0};
			}
		}
		else
		{
			if (s->kind == SK_FN)
			{
				tkerr("a function can only be called");
			}

			*r = (Ret){s->type, true, s->type.n >= 0};
		}
		return true;
	}

	if (consume(INT))
	{
		*r = (Ret){{TB_INT, NULL, -1}, false, true};
		return true;
	}
	if (consume(DOUBLE))
	{
		*r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
		return true;
	}
	if (consume(CHAR))
	{
		*r = (Ret){{TB_CHAR, NULL, -1}, false, true};
		return true;
	}
	if (consume(STRING))
	{
		*r = (Ret){{TB_CHAR, NULL, 0}, false, true};
		return true;
	}
	if (consume(LPAR))
	{
		if (expr(r))
		{
			if (consume(RPAR))
				return true;
		}
	}

	iTk = start;
	return false;
}

void parse(Token *tokens)
{
	iTk = tokens;
	if (!unit())
		tkerr("syntax error");
}