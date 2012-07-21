#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minill.h"

enum token {
  T_EOF = 0,

  // operators
  T_PLUS, T_MINUS, T_MUL, T_DIV,

  // parens
  T_OPEN, T_CLOSE,

  // semicolon, assign
  T_SEMI, T_EQUAL,

  // ident, literals
  T_IDENT, T_CONSTANT, T_STRING
};

static char const * const token_strings[] = {
  "EOF",

  // operators
  "+", "-", "*", "/",

  // parens
  "(", ")",

  // semicolon
  ";", "=",

  // ident, literals
  "IDENT", "NUM", "STRING"
};

static const int token_snum = 12;

static const char * const id_start =
  "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char * const digits =
  "0123456789";

union tval_t {
  char *s;
  int i;
};

// Lexer

#define START_IDENT(c) strchr(id_start, c)
#define IN_IDENT(c) (strchr(id_start, c) || strchr(digits, c))

static void overread_ws (const char **cp) {
  while (**cp == ' ' || **cp == '\n' || **cp == '\t')
    (*cp)++;
}

static int tmatch (const char *p, int token, const char **np) {
  const char *q = *np = p;
  const char *t = token_strings[token];

  while (*t)
    if (*(q++) != *(t++))
      return FALSE;

  if (START_IDENT(*p) && IN_IDENT(*q))
    return FALSE;

  *np = q;
  return TRUE;
}

static int next_token (const char **cp, union tval_t *tval) {
  const char *start;

  if (!**cp)
    return T_EOF;

  overread_ws(cp);

  // keyword or operator

  int i;
  for (i = 1; i < token_snum; i++)
    if (tmatch(*cp, i, cp))
      return i;

  // identifier

  if (START_IDENT(**cp)) {
    start = *cp;
    while (IN_IDENT(**cp))
      (*cp)++;

    if (tval) tval->s = strndup(start, *cp - start);
    return T_IDENT;
  }

  // integer

  start = *cp;
  int num = strtol(start, (char**)cp, 0);
  if (start != *cp) {
    if (tval) tval->i = num;
    return T_CONSTANT;
  }

  // unknown: bail

  printf("unknown rest: '%s'\n", *cp);
  return T_EOF;
}


// Parser

struct attr {
  int value;
  int rolling;
};

static void init_synth(struct attr *attr) {
  attr->value = 0;
  attr->rolling = 0;
}

static void copy_chained(struct attr * const from,
                         struct attr * to) {
  to->rolling = from->rolling;
}

RULEDEC(block);
RULEDEC(stmt);

RULEDEC(assign);

RULEDEC(expr);
RULEDEC(paren_term);
RULEDEC(sum);
RULEDEC(sum_tail);
RULEDEC(product);
RULEDEC(product_tail);
RULEDEC(product_factor);

RULEDEC(unary_minus);

RULEDEC(num);
RULEDEC(var);

RULEDEF(block, {
    CHILD(block_stmts);
    repeat(stmt, block_stmts);
  })

RULEDEF(stmt, {
    CHILD(stmt_expr);
    CHILD(stmt_assign);

    choose;
    alt(assign, stmt_assign);
    alt(expr, stmt_expr) {
      printf("evaluated expr: %i\n", stmt_expr.value);
    }
    or_bail;

    switch(next()) {
    case T_SEMI:
    case T_EOF:
      break;
    default:
      fail;
    }
  })

RULEDEF(assign, {
    CHILD(rvalue);

    expect(T_IDENT);
    char *ident = tval.s;

    expect(T_EQUAL);
    invoke(expr, rvalue);

    printf("assigning %i to %s\n", rvalue.value, ident);
  })

RULEDEF(expr, {
    CHILD(top);

    invoke(sum, top);

    self->value = top.value;
  })

RULEDEF(paren_term, {
    CHILD(term_sum);

    expect(T_OPEN);
    invoke(sum, term_sum);
    expect(T_CLOSE);
    
    self->value = term_sum.value;    
  })

RULEDEF(sum, {
    CHILD(first);
    CHILD(tail);

    invoke(product, first);

    self->rolling = first.value;

    try(sum_tail, tail);

    self->value = self->rolling;
  })

RULEDEF(sum_tail, {
    CHILD(summand);
    CHILD(tail);

    int current = self->rolling;

    enum token t;

    switch (t = next()) {
    case T_PLUS:
    case T_MINUS:
      invoke(product, summand);
      self->rolling = current +
        (t == T_MINUS ? -summand.value : summand.value);
      break;

    default:
      fail;
    }

    try(sum_tail, tail);
  })

RULEDEF(product, {
    CHILD(first);
    CHILD(tail);

    invoke(product_factor, first);

    self->rolling = first.value;

    try(product_tail, tail);

    self->value = self->rolling;
  })

RULEDEF(product_tail, {
    CHILD(factor);
    CHILD(tail);

    int current = self->rolling;

    enum token t;
    switch (t = next()) {
    case T_MUL:

      invoke(product_factor, factor);

      self->rolling = current * factor.value;
      break;
    default:
      fail;
    }

    try(product_tail, tail);
  })

RULEDEF(product_factor, {
    CHILD(node);

    choose;
    alt(paren_term, node);
    alt(num, node);
    alt(var, node);
    alt(unary_minus, node);
    or_bail;

    self->value = node.value;
  });

RULEDEF(num, {
    expect(T_CONSTANT);
    self->value = tval.i;
  })

RULEDEF(var, {
    expect(T_IDENT);
    self->value = 99;
  })

RULEDEF(unary_minus, {
    CHILD(value);
    expect(T_MINUS);
    invoke(product_factor, value);
    self->value = -value.value;
  })

int main (int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr, "incorrect usage.\n");
    return 2;
  }

  struct debug_tree dtree;
  struct attr attr;
  init_synth(&attr);

  char const * cp = argv[1];

  int r = parse_block(&cp, &attr, &dtree);

  printf("result: %i\n", r);

  // output_dtree(dtree);

  printf("block value: %i\n", attr.value);

  return 0;
}
