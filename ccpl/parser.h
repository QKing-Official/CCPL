#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "lexer.h"

// Parse the token stream
int parse(Token *t, FILE *out, int auto_install);

#endif