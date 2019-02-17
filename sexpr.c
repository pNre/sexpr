#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "sexpr.h"
#include "utf8.h/utf8.h"
#include "utf8proc/utf8proc.h"

enum {
    TOKEN_QUOT = '"',
    TOKEN_FULLSTOP = '.',
    TOKEN_PLUS = '+',
    TOKEN_MINUS = '-',
    TOKEN_LPAR = '(',
    TOKEN_RPAR = ')'
};

struct parse_ctx_s {
    void *data;
    size_t offset;
    size_t size;
};

list_t *list_alloc(void *value, void(*value_free)(void *)) {
    list_t *list = malloc(sizeof(list_t));
    if (!list) {
        return NULL;
    }

    list->value = value;
    list->value_free = value_free;
    list->next = NULL;
    return list;
}

void list_free(list_t *list) {
    while (list) {
        list_t *next = list->next;
        list->value_free(list->value);
        free(list);
        list = next;
    }
}

sexpr_t *sexpr_alloc(sexpr_type_t type, void *value) {
    sexpr_t *s = malloc(sizeof(sexpr_t));
    if (!s) {
        return NULL;
    }

    s->type = type;
    s->any_val = value;
    return s;
}

void sexpr_free(void *sexpr) {
    sexpr_t *_sexpr = sexpr;
    switch (_sexpr->type) {
    case SEXPR_TYPE_LIST:
        list_free(_sexpr->list_val);
        break;
    case SEXPR_TYPE_INT:
    case SEXPR_TYPE_STRING:
    case SEXPR_TYPE_FLOAT:
    case SEXPR_TYPE_SYMBOL:
        free(_sexpr->any_val);
        break;
    }

    free(sexpr);
}

void sexpr_parse_error_set(sexpr_parse_error_t *err, sexpr_parse_error_type_t type, struct parse_ctx_s *ctx) {
    if (!err) {
        return;
    }

    err->type = type;
    err->pos = ctx->offset;
}

void sexpr_parse_error_clear(sexpr_parse_error_t *err) {
    if (!err) {
        return;
    }

    err->type = SEXPR_PARSE_ERROR_NONE;
    err->pos = 0;
}

bool utf8cp_isspace(utf8_int32_t cp) {
    utf8proc_category_t category = utf8proc_category(cp);
    return cp == '\n' || category == UTF8PROC_CATEGORY_ZS;
}

bool utf8cp_issymbol(utf8_int32_t cp) {
    utf8proc_category_t category = utf8proc_category(cp);
    return category == UTF8PROC_CATEGORY_LU
        || category == UTF8PROC_CATEGORY_LL
        || category == UTF8PROC_CATEGORY_LO
        || category == UTF8PROC_CATEGORY_ND
        || category == UTF8PROC_CATEGORY_NL
        || category == UTF8PROC_CATEGORY_PD
        || category == UTF8PROC_CATEGORY_SM
        || category == UTF8PROC_CATEGORY_SO;
}

bool utf8cp_isquotation(utf8_int32_t cp) {
    return cp == TOKEN_QUOT;
}

bool utf8cp_isdigit(utf8_int32_t cp) {
    utf8proc_category_t category = utf8proc_category(cp);
    return category == UTF8PROC_CATEGORY_ND;
}

bool utf8cp_issign(utf8_int32_t cp) {
    return cp == TOKEN_PLUS || cp == TOKEN_MINUS;
}

bool utf8cp_isnumber(utf8_int32_t cp) {
    return utf8cp_isdigit(cp)
        || utf8cp_issign(cp);
}

size_t parse_ctx_next_codepoint_size(struct parse_ctx_s *ctx) {
    const char *s = (const char *)ctx->data + ctx->offset;

    if (0xf0 == (0xf8 & *s)) {
        return 4;
    } else if (0xe0 == (0xf0 & *s)) {
        return 3;
    } else if (0xc0 == (0xe0 & *s)) {
        return 2;
    } else {
        return 1;
    }
}

bool parse_ctx_is_valid(struct parse_ctx_s *ctx) {
    size_t cpsize = parse_ctx_next_codepoint_size(ctx);
    return ctx->offset < ctx->size - cpsize;
}

bool parse_ctx_peek(struct parse_ctx_s *ctx, utf8_int32_t *token) {
    if (!parse_ctx_is_valid(ctx)) {
        return false;
    }

    utf8codepoint(ctx->data + ctx->offset, token);
    return true;
}

void parse_ctx_read(struct parse_ctx_s *ctx) {
    utf8_int32_t token;
    if (!parse_ctx_peek(ctx, &token)) {
        return;
    }

    ctx->offset += utf8codepointsize(token);
}

void parse_ctx_read_whitespaces(struct parse_ctx_s *ctx) {
    utf8_int32_t token;
    while (parse_ctx_peek(ctx, &token) && utf8cp_isspace(token)) {
        parse_ctx_read(ctx);
    }
}

bool parse_ctx_expect(struct parse_ctx_s *ctx, utf8_int32_t token) {
    utf8_int32_t next_token;
    parse_ctx_read_whitespaces(ctx);
    parse_ctx_peek(ctx, &next_token);
    parse_ctx_read(ctx);
    return next_token == token;
}

void *parse_ctx_read_cond(struct parse_ctx_s *ctx, bool until, bool(*condition)(utf8_int32_t)) {
    utf8_int32_t token;
    void *data = ctx->data + ctx->offset;
    size_t length = 0;

    while (parse_ctx_peek(ctx, &token)) {
        if (until) {
            parse_ctx_read(ctx);
        }

        if (until == condition(token)) {
            break;
        }

        if (!until) {
            parse_ctx_read(ctx);
        }

        length += utf8codepointsize(token);
    }

    return utf8ndup(data, length);
}

sexpr_t *parse_ctx_read_string(struct parse_ctx_s *ctx, sexpr_parse_error_t *err) {
    if (!parse_ctx_expect(ctx, TOKEN_QUOT)) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_TOKEN, ctx);
        return NULL;
    }

    void *string = parse_ctx_read_cond(ctx, true, utf8cp_isquotation);
    return sexpr_alloc(SEXPR_TYPE_STRING, string);
}

sexpr_t *parse_ctx_read_symbol(struct parse_ctx_s *ctx) {
    void *symbol = parse_ctx_read_cond(ctx, false, utf8cp_issymbol);
    return sexpr_alloc(SEXPR_TYPE_SYMBOL, symbol);
}

float *parse_ctx_parse_float(struct parse_ctx_s *ctx, size_t length, sexpr_parse_error_t *err) {
    float *num = malloc(sizeof(float));
    if (!num) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_OUT_OF_MEMORY, ctx);
        return NULL;
    }

    void *number = utf8ndup(ctx->data + ctx->offset - length, length);
    *num = strtof(number, NULL);
    free(number);
    return num;
}

long int *parse_ctx_parse_int(struct parse_ctx_s *ctx, size_t length, sexpr_parse_error_t *err) {
    long int *num = malloc(sizeof(long int));
    if (!num) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_OUT_OF_MEMORY, ctx);
        return NULL;
    }

    void *number = utf8ndup(ctx->data + ctx->offset - length, length);
    *num = strtol(number, NULL, 10);
    free(number);
    return num;
}

sexpr_t *parse_ctx_read_number(struct parse_ctx_s *ctx, sexpr_parse_error_t *err) {
    utf8_int32_t token;
    size_t prefix_length = 0;
    size_t length = 0;
    bool is_float = false;

    if (!parse_ctx_peek(ctx, &token)) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_EOS, ctx);
        return NULL;
    }

    if (utf8cp_issign(token)) {
        parse_ctx_read(ctx);
        prefix_length = utf8codepointsize(token);
    }

    while (parse_ctx_peek(ctx, &token)) {
        if (!utf8cp_isdigit(token) && token != TOKEN_FULLSTOP) {
            break;
        }

        parse_ctx_read(ctx);

        if (token == TOKEN_FULLSTOP) {
            if (is_float) {
                sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_TOKEN, ctx);
                return NULL;
            }
            is_float = true;
        }

        length += utf8codepointsize(token);
    }

    if (length == 0) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_TOKEN, ctx);
        return NULL;
    }

    if (is_float) {
        float *num = parse_ctx_parse_float(ctx, length + prefix_length, err);
        if (!num) {
            return NULL;
        }

        return sexpr_alloc(SEXPR_TYPE_FLOAT, num);
    } else {
        long int *num = parse_ctx_parse_int(ctx, length + prefix_length, err);
        if (!num) {
            return NULL;
        }

        return sexpr_alloc(SEXPR_TYPE_INT, num);
    }
}

sexpr_t *parse_ctx_read_atom(struct parse_ctx_s *ctx, sexpr_parse_error_t *err) {
    utf8_int32_t token;

    if (!parse_ctx_peek(ctx, &token)) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_EOS, ctx);
        return NULL;
    }

    if (utf8cp_isquotation(token)) {
        return parse_ctx_read_string(ctx, err);
    } else if (utf8cp_isnumber(token)) {
        struct parse_ctx_s ctx_checkpoint = *ctx;

        sexpr_t *sexpr = parse_ctx_read_number(ctx, err);
        if (sexpr) {
            return sexpr;
        }

        *ctx = ctx_checkpoint;
        sexpr = parse_ctx_read_symbol(ctx);
        if (sexpr) {
            sexpr_parse_error_clear(err);
        }

        return sexpr;
    } else if (utf8cp_issymbol(token)) {
        return parse_ctx_read_symbol(ctx);
    } else {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_TOKEN, ctx);
        return NULL;
    }
}

sexpr_t *parse_ctx_read_list(struct parse_ctx_s *ctx, sexpr_parse_error_t *err) {
    list_t *list_head = NULL;
    list_t *list_current = NULL;
    utf8_int32_t token;

    if (!parse_ctx_expect(ctx, TOKEN_LPAR)) {
        sexpr_parse_error_set(err, SEXPR_PARSE_ERROR_UNEXPECTED_TOKEN, ctx);
        return NULL;
    }

    while (parse_ctx_peek(ctx, &token)) {
        if (token == TOKEN_RPAR) {
            parse_ctx_read(ctx);
            break;
        }

        sexpr_t *expr;
        if (token == TOKEN_LPAR) {
            expr = parse_ctx_read_list(ctx, err);
        } else {
            expr = parse_ctx_read_atom(ctx, err);
        }

        if (!expr) {
            if (list_head) {
                list_free(list_head);
            }
            return NULL;
        }

        list_t *node = list_alloc(expr, sexpr_free);

        if (!list_current) {
            list_head = node;
            list_current = node;
        } else {
            list_current->next = node;
            list_current = node;
        }

        parse_ctx_read_whitespaces(ctx);
    }

    return sexpr_alloc(SEXPR_TYPE_LIST, list_head);
}

list_t *parse_ctx_parse(struct parse_ctx_s *ctx, sexpr_parse_error_t *err) {
    list_t *list_head = NULL;
    list_t *list_current = NULL;

    while (parse_ctx_is_valid(ctx)) {
        sexpr_t *expr = parse_ctx_read_list(ctx, err);
        if (!expr) {
            if (list_head) {
                list_free(list_head);
            }
            return NULL;
        }

        list_t *node = list_alloc(expr, sexpr_free);

        if (!list_current) {
            list_head = node;
            list_current = node;
        } else {
            list_current->next = node;
            list_current = node;
        }
    }

    return list_head;
}

list_t *sexpr_from_string(void *s, sexpr_parse_error_t *err) {
    struct parse_ctx_s ctx = {
        .data = s,
        .offset = 0,
        .size = utf8size(s)
    };

    return parse_ctx_parse(&ctx, err);
}

sexpr_t *sexpr_list_with_symbol_at(sexpr_t *expr, const char *path) {
    if (expr->type != SEXPR_TYPE_LIST || !expr->list_val) {
        return NULL;
    }

    size_t component_length;
    const char *component = strchr(path, '.');
    if (component) {
        component_length = component - path;
    } else {
        component_length = strlen(path);
    }

    list_t *list_head = expr->list_val;
    while (list_head) {
        sexpr_t *curr_sexpr = list_head->value;
        switch (curr_sexpr->type) {
        case SEXPR_TYPE_LIST:
            if (component) {
                sexpr_t *next_sexpr = sexpr_list_with_symbol_at(curr_sexpr, component + 1);
                if (next_sexpr) {
                    return next_sexpr;
                }
            }
            break;
        case SEXPR_TYPE_SYMBOL:
            if (!strncmp(curr_sexpr->symbol_val, path, component_length)) {
                if (component) {
                    const char *next_component = strchr(component + 1, '.');
                    if (next_component) {
                        component_length = next_component - component - 1;
                        component = next_component;
                    }
                } else {
                    return expr;
                }
            }
            break;
        default:
            break;
        }

        list_head = list_head->next;
    }

    return NULL;
}

sexpr_t *sexpr_list_nth_item(sexpr_t *expr, int nth) {
    if (expr->type != SEXPR_TYPE_LIST) {
        return NULL;
    }

    list_t *list = expr->list_val;
    while (nth > 0 && list) {
        list = list->next;
        nth--;
    }

    if (!list) {
        return NULL;
    }

    return list->value;
}

