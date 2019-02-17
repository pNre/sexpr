#include <stdio.h>
#include <stdlib.h>
#include "sexpr.h"

void sexpr_print(sexpr_t *s, int nesting_level) {
    for (int i = 0; i < nesting_level; i++) {
        printf("\t");
    }

    switch (s->type) {
    case SEXPR_TYPE_STRING:
        printf("str=\"%s\"\n", s->string_val);
        break;
    case SEXPR_TYPE_SYMBOL:
        printf("sym=%s\n", s->symbol_val);
        break;
    case SEXPR_TYPE_INT:
        printf("int=%ld\n", *(s->integer_val));
        break;
    case SEXPR_TYPE_FLOAT:
        printf("float=%f\n", *(s->float_val));
        break;
    case SEXPR_TYPE_LIST:
        printf("list=(\n");
        list_t *n = s->list_val;
        while (n) {
            sexpr_print(n->value, nesting_level + 1);
            n = n->next;
        }
        for (int i = 0; i < nesting_level; i++) {
            printf("\t");
        }
        printf(")\n");
        break;
    }
}

int main(int argc, char *argv[]) {
    sexpr_parse_error_t error;
    char *s = "(sym1 sym2 \"str1 ∆ 🧱\" (sym3 \"hello world\" (ƒ 10 -5 (9.15 -.1)))) (® \"yes\") (+ 1 2 3 4 5)";

    printf("Parsing %s\n", s);
    list_t *exprs = sexpr_from_string(s, &error);
    if (!exprs) {
        fprintf(stderr, "Parse error %d\n", error.type);
        return EXIT_FAILURE;
    }

    list_t *n = exprs;
    while (n) {
        sexpr_print(n->value, 0);
        n = n->next;
    }

    printf("Searching for sym3\n");
    sexpr_t *sym3 = sexpr_list_with_symbol_at(exprs->value, "sym1.sym3");
    if (sym3) {
        sexpr_t *str = sexpr_list_nth_item(sym3, 1);
        printf("sym3 found, %s\n", str->string_val);
    }

    list_free(exprs);

    return EXIT_SUCCESS;
}
