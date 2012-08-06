#include <stdio.h>

#include "minill.h"

int try_ (int (*parser)(const char **, struct attr *,
                               struct debug_tree *),
                 const char **pp, struct debug_tree *dtree, int *dcnum,
                 struct attr *attr, struct attr *self) {

  dtree->child[*dcnum] = malloc(sizeof(struct debug_tree));

  init_synth(attr);
  copy_chained(self, attr);
  int r =  parser(pp, attr, dtree->child[(*dcnum)++]);

  if (r)
    copy_chained(attr, self);

  return r;
}

int consume_ (int token, union tval_t *tval, const char **pp) {
  int rtoken;
  if ((rtoken = next_token(pp, tval)) != token) { 
    /* ind(); */
    /* printf("%s instead of %s\n", */
    /*        token_strings[rtoken], */
    /*        token_strings[token]); */
    return FALSE;
  }

  return TRUE;
}

int pc_compute (char const **cp, struct attr *value, struct debug_tree *dtree,
                int (*parse_atom)(char const **, struct attr *,
                                  struct debug_tree *),
                int (*compute_op)(int, struct attr*, struct attr*),
                struct prec_table const * const table,
                int min_prec) {
  if (!parse_atom(cp, value, dtree))
    return FALSE;

  while (TRUE) {
    char const * p = *cp;
    int token = next_token(&p, NULL);

    if (table[token].prec < min_prec)
      break;

    *cp = p;

    struct attr *right = new_attr();
    init_synth(right);
    copy_chained(value, right);

    if (!pc_compute(cp, right, dtree,
                    parse_atom, compute_op, table,
                    table[token].prec + (table[token].right_assoc ? 0 : 1))) {
      del_attr(right);
      return FALSE;
    }

    if (!compute_op(token, value, right)) {
      del_attr(right);
      return FALSE;
    }

    copy_chained(right, value);
  };

  return TRUE;
}
