#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minill.h"

struct token_param {
  char const * const string;
};

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

static struct token_param token_params[] = {
  { "EOF" },
  { "+" },
  { "-" },
  { "*" },
  { "/" },
  { "(" },
  { ")" },
  { ";" },
  { "=" },
  { "IDENT" },
  { "NUM" },
  { "STRING" },

  { NULL }
};

static struct prec_table op_prec_table[] = {
  { 0, FALSE },
  { 2, FALSE },
  { 2, FALSE },
  { 3, FALSE },
  { 3, FALSE },
  { 0, FALSE },
  { 0, FALSE },
  { 0, FALSE },
  { 1, TRUE },
  { 0, FALSE },
  { 0, FALSE },
  { 0, FALSE },
};

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
  const char *t = token_params[token].string;

  if (!t)
    return FALSE;

  while (*t)
    if (*(q++) != *(t++))
      return FALSE;

  if (START_IDENT(*p) && IN_IDENT(*q))
    return FALSE;

  *np = q;
  return TRUE;
}

int next_token (const char **cp, union tval_t *tval) {
  const char *start;

  if (!**cp)
    return T_EOF;

  overread_ws(cp);

  // keyword or operator

  int i;
  for (i = 0; token_params[i].string; i++)
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

  fprintf(stderr, "unknown rest: '%s'\n", *cp);
  return T_EOF;
}

// Parser

struct attr {
  int value;
  int rolling;
};

struct attr *new_attr (void) {
  return malloc(sizeof(struct attr));
}

void del_attr (struct attr *attr) {
  free(attr);
}

void init_synth (struct attr *attr) {
  attr->value = 0;
  attr->rolling = 0;
}

void copy_chained (struct attr * const from,
                          struct attr * to) {
  to->rolling = from->rolling;
}


static int compute_op (int op, struct attr *left, struct attr *right) {
    switch(op) {
    case T_PLUS:
      left->value += right->value;
      break;
    case T_MINUS:
      left->value -= right->value;
      break;
    case T_MUL:
      left->value *= right->value;
      break;
    case T_DIV:
      left->value /= right->value;
      break;
    default:
      fprintf(stderr, "unimplemented op\n");
      return FALSE;
    }

    return TRUE;
}

RULEDEC(block);
RULEDEC(stmt);

RULEDEC(assign);

RULEDEC(atom);

RULEDEC(paren_term);
RULEDEC(unary_minus);

RULEDEC(num);
RULEDEC(var);

PCDEC(expr, atom, op_prec_table, compute_op);

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

RULEDEF(paren_term, {
    CHILD(term_sum);

    expect(T_OPEN);
    invoke(expr, term_sum);
    expect(T_CLOSE);

    self->value = term_sum.value;
  })

RULEDEF(atom, {
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
    invoke(atom, value);
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
