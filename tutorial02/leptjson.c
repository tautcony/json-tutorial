#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <math.h>    /* HUGE_VAL */

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define ISNUMBERPART(ch)     (ISDIGIT(ch) || (ch) == '+' || (ch) == '-' || (ch) == 'e' || (ch) == 'E' || (ch) == '.')

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}
#include <stdio.h>
static int lept_parse_literal(lept_context* c, lept_value* v) {
    switch (c->json[0]) {
    case 'n':
        if (c->json[1] != 'u' || c->json[2] != 'l' || c->json[3] != 'l')
            return LEPT_PARSE_INVALID_VALUE;
        c->json += 4;
        v->type = LEPT_NULL;
        break;
    case 't':
        if (c->json[1] != 'r' || c->json[2] != 'u' || c->json[3] != 'e')
            return LEPT_PARSE_INVALID_VALUE;
        c->json += 4;
        v->type = LEPT_TRUE;
        break;
    case 'f':
        if (c->json[1] != 'a' || c->json[2] != 'l' || c->json[3] != 's' || c->json[4] != 'e')
            return LEPT_PARSE_INVALID_VALUE;
        c->json += 5;
        v->type = LEPT_FALSE;
        break;
    default:
        return LEPT_PARSE_INVALID_VALUE;
    }
    return LEPT_PARSE_OK;
}

typedef enum {
    LEPT_PARSE_NUMBER_STATE_BEGIN, LEPT_PARSE_NUMBER_STATE_INVALID, LEPT_PARSE_NUMBER_STATE_FIN,
    LEPT_PARSE_NUMBER_STATE_PRECEDING_ZERO, LEPT_PARSE_NUMBER_STATE_PRECEDING_MINUS,
    LEPT_PARSE_NUMBER_STATE_INTEGER, LEPT_PARSE_NUMBER_STATE_DOT, LEPT_PARSE_NUMBER_STATE_DECIMAL,
    LEPT_PARSE_NUMBER_STATE_EXPO, LEPT_PARSE_NUMBER_STATE_EXPO_PRECEDING_SIGN,
    LEPT_PARSE_NUMBER_STATE_EXPO_INTEGER
} LEPT_PARSE_NUMBER_STATE;

const char* LEPT_PARSE_NUMBER_STATE_STRING[] = {
    "STATE_BEGIN", "STATE_INVALID", "STATE_FIN",
    "STATE_PRECEDING_ZERO", "STATE_PRECEDING_MINUS",
    "STATE_INTEGER", "STATE_DOT", "STATE_DECIMAL",
    "STATE_EXPO", "STATE_EXPO_PRECEDING_SIGN",
    "STATE_EXPO_INTEGER"
};

static int lept_parse_number_validate(lept_context* c) {
    LEPT_PARSE_NUMBER_STATE state = LEPT_PARSE_NUMBER_STATE_BEGIN;
    int i;
    char ch;
    printf("BEGIN: %s\n", c->json);
    for (i = 0; c->json[i] != '\0'; ++i) {
        ch = c->json[i];
        /*printf("%c %s\n", c->json[i-1], LEPT_PARSE_NUMBER_STATE_STRING[state]);*/
        switch (state) {
            case LEPT_PARSE_NUMBER_STATE_BEGIN:
                if (ISDIGIT1TO9(ch)) {
                    state = LEPT_PARSE_NUMBER_STATE_INTEGER;
                    break;
                }
                switch (ch) {
                    case '-': state = LEPT_PARSE_NUMBER_STATE_PRECEDING_MINUS; break;
                    case '0': state = LEPT_PARSE_NUMBER_STATE_PRECEDING_ZERO;  break;
                    default: state = LEPT_PARSE_NUMBER_STATE_INVALID; break;
                }
                break;
            case LEPT_PARSE_NUMBER_STATE_INVALID: printf("INVALID\n"); return 0;
            case LEPT_PARSE_NUMBER_STATE_FIN: printf("FIN!\n");return i;
            case LEPT_PARSE_NUMBER_STATE_PRECEDING_ZERO:
                if ((!ISNUMBERPART(ch)) || ISDIGIT(ch)) {
                    state = LEPT_PARSE_NUMBER_STATE_FIN; /* Lead result to LEPT_PARSE_ROOT_NOT_SINGULAR */
                    assert(i == 1);
                    break;
                }
                switch (ch) {
                    case '.': state = LEPT_PARSE_NUMBER_STATE_DOT; break;
                    case 'e': case 'E': state = LEPT_PARSE_NUMBER_STATE_EXPO; break;
                    default: state = LEPT_PARSE_NUMBER_STATE_INVALID; break;
                }
                break;
            case LEPT_PARSE_NUMBER_STATE_PRECEDING_MINUS:
                if (ch == '0') state = LEPT_PARSE_NUMBER_STATE_PRECEDING_ZERO;
                else if (ISDIGIT1TO9(ch)) state = LEPT_PARSE_NUMBER_STATE_INTEGER;
                else state = LEPT_PARSE_NUMBER_STATE_INVALID;
                break;
            case LEPT_PARSE_NUMBER_STATE_INTEGER:
                if (!ISNUMBERPART(ch)) {
                   state = LEPT_PARSE_NUMBER_STATE_FIN;
                   break;
                }
                switch (ch) {
                    case '.': state = LEPT_PARSE_NUMBER_STATE_DOT; break;
                    case 'e': case 'E': state = LEPT_PARSE_NUMBER_STATE_EXPO; break;
                    default: state = LEPT_PARSE_NUMBER_STATE_INVALID; break;
                }
                break;
            case LEPT_PARSE_NUMBER_STATE_DOT:
                if (ISDIGIT(ch)) state = LEPT_PARSE_NUMBER_STATE_DECIMAL;
                else state = LEPT_PARSE_NUMBER_STATE_INVALID;
                break;
            case LEPT_PARSE_NUMBER_STATE_DECIMAL:
                if (ch == 'e' || ch == 'E') state = LEPT_PARSE_NUMBER_STATE_EXPO;
                else if (!ISNUMBERPART(ch)) {
                   state = LEPT_PARSE_NUMBER_STATE_FIN;
                   break;
                }
                else if (ISDIGIT(ch)) break;
                else state = LEPT_PARSE_NUMBER_STATE_INVALID;
                break;
            case LEPT_PARSE_NUMBER_STATE_EXPO:
                if (ch == '+' || ch == '-') state = LEPT_PARSE_NUMBER_STATE_EXPO_PRECEDING_SIGN;
                else if (ISDIGIT(ch)) state = LEPT_PARSE_NUMBER_STATE_EXPO_INTEGER;
                else state = LEPT_PARSE_NUMBER_STATE_INVALID;
                break;
            case LEPT_PARSE_NUMBER_STATE_EXPO_PRECEDING_SIGN:
                if (ISDIGIT(ch)) state = LEPT_PARSE_NUMBER_STATE_EXPO_INTEGER;
                else state = LEPT_PARSE_NUMBER_STATE_INVALID;
                break;
            case LEPT_PARSE_NUMBER_STATE_EXPO_INTEGER:
                if (!ISNUMBERPART(ch)) {
                   state = LEPT_PARSE_NUMBER_STATE_FIN;
                   break;
                }
                break;
        }
    }
    if (state == LEPT_PARSE_NUMBER_STATE_DOT) return 0;
    /*printf("END STATE: %s\n", LEPT_PARSE_NUMBER_STATE_STRING[state]);*/
    return i;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    int length = lept_parse_number_validate(c);
    if (length == 0)
        return LEPT_PARSE_INVALID_VALUE;
    v->n = strtod(c->json, NULL);
    if (v->n == HUGE_VAL || v->n == -HUGE_VAL)
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json += length;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't': case 'f': case 'n': return lept_parse_literal(c, v);
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-': return lept_parse_number(c, v);
        default: return LEPT_PARSE_INVALID_VALUE;
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
