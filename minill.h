#ifndef MINILL_H
#define MINILL_H

#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

union tval_t;
struct attr;

int next_token (const char **cp, union tval_t *tval);

struct attr *new_attr(void);
void del_attr(struct attr *attr);
void init_synth(struct attr *attr);
void copy_chained(struct attr * const from, struct attr *to);

// hackishly print out an AST.

struct debug_tree {
  const char * rule_name;
  struct debug_tree *child[128];
  int ret;
};

static int indent = 0;
static void ind(void) {
    int i;
    for (i = 0; i < indent; i++)
      printf(" ");
}

static void output_dtree(struct debug_tree dtree) {
  if (!dtree.ret)
    return;

  ind();
  printf("%s %i\n", dtree.rule_name, dtree.ret);

  indent++;
  int i;
  for (i = 0; i < 128; i++)
    if (dtree.child[i])
      output_dtree(*dtree.child[i]);
  indent--;
}


// declare a new rule.
#define RULEDEC(rule) \
  static int parse_##rule (const char **, struct attr *, struct debug_tree *)

// define a new rule.
#define RULEDEF(rule,body)                                      \
  static int parse_##rule (const char **cp, struct attr *self,  \
                           struct debug_tree *dtree) {          \
    int dcnum = 0;                                              \
    dcnum = dcnum;                                              \
    dtree->rule_name = #rule;                                   \
    {int i; for(i=0;i<128;i++)                                  \
              dtree->child[i] = NULL;}                          \
                                                                \
    /* ind();                                                      \
       printf("%s...\n", #rule); */                                \
    indent++;                                                   \
                                                                \
    union tval_t tval; tval=tval;                               \
    struct symtab *sym = 0; sym=sym;                            \
    const char *p = *cp;                                        \
    body;                                                       \
    *cp = p;                                                    \
                                                                \
    bye(TRUE);                                                  \
  }

// declare a rule's child node.
#define CHILD(name) struct attr name; copy_chained(self, &name)

// end rule processing, indicating failure or success.
#define bye(v) do { indent--; /* ind(); printf("...%s (%i)\n", dtree->rule_name, v); */ dtree->ret = v; return v; } while (0)

// exit the rule, indicating that it failed.
#define fail bye(FALSE);

// next token.
#define next() next_token(&p, &tval)

// expect a token.
#define expect(token) if(!consume_(token, &tval, &p)) bye(FALSE)

// try another rule.
#define try(rule, name) try_(parse_##rule, &p, dtree, &dcnum, &name, self)

// invoke a rule. if it fails, parent rule also fails.
#define invoke(rule, name)                                              \
  if (!try(rule, name)) {                                               \
  /* ind();                                                              \
     printf("%s failed.\n", #rule); */                                  \
    bye(FALSE);                                                         \
  }

// start a choice between alternatives.
#define choose if (FALSE)
// try a rule among alternatives.
#define alt(rule, name) else if (try(rule, name))
// fail out if no alternative matched.
#define or_bail else { /* ind(); printf("no matching choice.\n"); */ bye(FALSE); }

// repeat a rule as many times as possible.
#define repeat(rule, name) while(try(rule,name))

// mock out missing rules.
#define mock                                                            \
  (dtree->child[dcnum] = malloc(sizeof(struct debug_tree)),             \
   mock_until_semi(&p, dtree->child[dcnum++]))
int mock_until_semi (const char **, struct debug_tree *);

// work horses for the combinators above.

int try_ (int (*parser)(const char **, struct attr *,
                        struct debug_tree *),
          const char **pp, struct debug_tree *dtree, int *dcnum,
          struct attr *attr, struct attr *self);

int consume_ (int token, union tval_t *tval, const char **pp);

struct prec_table {
  int prec;
  int right_assoc;
};

int pc_compute (char const **cp, struct attr *value, struct debug_tree *dtree,
                int (*parse_atom)(char const **, struct attr *,
                                  struct debug_tree *),
                int (*compute_op)(int, struct attr*, struct attr*),
                struct prec_table const * const table,
                int min_prec);

// define a new rule.
#define PCDEC(rule,atom,compute_op,table)                               \
  inline static int parse_##rule (const char **cp, struct attr *self,   \
                           struct debug_tree *dtree) {                  \
    return pc_compute(cp, self, dtree, parse_##atom, table, compute_op, 1); \
  }

#endif /* MINILL_H */
