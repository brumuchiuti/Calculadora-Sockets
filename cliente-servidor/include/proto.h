#ifndef PROTO_H
#define PROTO_H

#include <stdbool.h>
#include <stddef.h> 

#define DEFAULT_PORT 5050

#ifdef LINE_MAX
#undef LINE_MAX
#endif
#define LINE_MAX 1024

#define ERR_INV "EINV"
#define ERR_DIV "EZDV"
#define ERR_SRV "ESRV"

typedef enum {
    OP_NONE = 0,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIVI
} op_t;

typedef struct {
    bool ok;
    op_t op;
    double a, b;
    const char *err_code;
    const char *err_msg;
} parse_result_t;

void remove_spaces(char *s);

void set_c_locale(void);

void format_number(double x, char *out, size_t outsz);

parse_result_t parse_request_line(const char *line);

#endif
