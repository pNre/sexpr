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
    char *s = "(sym1 sym2 \"str1 ∆ 🧱\" (sym3 (ƒ 10 -5 (9.15 -.1)))) (® \"yes\") (+ 1 2 3 4 5)";
    printf("Parsing %s\n", s);
    list_t *exprs = sexpr_from_string(s, &error);

    if (exprs) {
        list_t *n = exprs;

        while (n) {
            sexpr_print(n->value, 0);
            n = n->next;
        }
    } else {
        fprintf(stderr, "Parse error %d\n", error.type);
    }

    return EXIT_SUCCESS;
}
