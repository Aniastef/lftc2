#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"
#include "at.h"

int main()
{
    char *inbuffer = loadFile("./tests/testat.c");
    Token *tokens = tokenize(inbuffer);

    pushDomain();
    vmInit();
    parse(tokens);
    //showDomain(symTable, "global");
    Instr *testCode = genTestProgram();
    run(testCode);
    dropDomain();

    return 0;
}
