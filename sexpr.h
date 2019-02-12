#ifndef SEXPRLIB_H
#define SEXPRLIB_H

#include <stddef.h>

typedef enum {
    SEXPR_TYPE_STRING,
    SEXPR_TYPE_INT,
    SEXPR_TYPE_FLOAT,
    SEXPR_TYPE_SYMBOL,
    SEXPR_TYPE_LIST
} sexpr_type_t;

typedef struct list_s {
    void *value;
    void (*value_free)(void *);
    struct list_s *next;
} list_t;

typedef struct {
    sexpr_type_t type;
    union {
        void *string_val;
        long int *integer_val;
        float *float_val;
        void *symbol_val;
        list_t *list_val;
        void *any_val;
    };
} sexpr_t;

typedef enum {
    SEXPR_PARSE_ERROR_UNEXPECTED_TOKEN,
    SEXPR_PARSE_ERROR_UNEXPECTED_EOS,
    SEXPR_PARSE_ERROR_OUT_OF_MEMORY
} sexpr_parse_error_type_t;

typedef struct {
    sexpr_parse_error_type_t type;
    size_t pos;
} sexpr_parse_error_t;

list_t *sexpr_from_string(void *s, sexpr_parse_error_t *err);

#endif
