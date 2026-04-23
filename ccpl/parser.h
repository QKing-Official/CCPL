#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "lexer.h"

/* parse token stream and emit C code to out.
   auto_install: if non-zero, call barite-cli to install packages automatically.
   returns 0 on success, 1 on error. */
int parse(Token *t, FILE *out, int auto_install);

#endif