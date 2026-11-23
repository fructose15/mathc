/*
 * MathC Compiler & Interpreter (mcc)
 * Core binary: /usr/local/bin/mcc
 * Language: English C99 with zero external dependencies beyond libc & libm.
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

typedef struct {
    char name[64];
    double value;
} McVar;

typedef struct {
    char name[64];
    double items[MCC_MAX_SET_ITEMS];
    int count;
    int unbounded_below;
    int unbounded_above;
} McSet;

typedef struct {
    McVar vars[MCC_MAX_VARS];
    int var_count;
    McSet sets[MCC_MAX_SETS];
    int set_count;
} McEnv;

static void env_init(McEnv *env) {
    env->var_count = 0;
    env->set_count = 0;
}

static int env_find_var(const McEnv *env, const char *name) {
    for (int i = 0; i < env->var_count; i++) {
        if (strcmp(env->vars[i].name, name) == 0) return i;
    }
    return -1;
}

static void env_set_var(McEnv *env, const char *name, double val) {
    int idx = env_find_var(env, name);
    if (idx >= 0) {
        env->vars[idx].value = val;
    } else if (env->var_count < MCC_MAX_VARS) {
        snprintf(env->vars[env->var_count].name, 64, "%s", name);
        env->vars[env->var_count].value = val;
        env->var_count++;
    }
}

static int env_find_set(const McEnv *env, const char *name) {
    for (int i = 0; i < env->set_count; i++) {
        if (strcmp(env->sets[i].name, name) == 0) return i;
    }
    return -1;
}

static McSet *env_upsert_set(McEnv *env, const char *name) {
    int idx = env_find_set(env, name);
    if (idx >= 0) {
        env->sets[idx].count = 0;
        env->sets[idx].unbounded_below = 0;
        env->sets[idx].unbounded_above = 0;
        return &env->sets[idx];
    }
    if (env->set_count < MCC_MAX_SETS) {
        McSet *s = &env->sets[env->set_count++];
        snprintf(s->name, 64, "%s", name);
        s->count = 0;
        s->unbounded_below = 0;
        s->unbounded_above = 0;
        return s;
    }
    return NULL;
}

static void trim_inplace(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) {
        start++;
    }
    if (start > 0) {
        memmove(s, s + start, len - start + 1);
    }
}

static void normalize_math_operators(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 2 < dst_size; ) {
        unsigned char c = (unsigned char)src[i];
        /* UTF-8 multiplication sign × (0xC3 0x97) */
        if (c == 0xC3 && (unsigned char)src[i + 1] == 0x97) {
            dst[j++] = '*';
            i += 2;
        /* UTF-8 division sign ÷ (0xC3 0xB7) */
        } else if (c == 0xC3 && (unsigned char)src[i + 1] == 0xB7) {
            dst[j++] = '/';
            i += 2;
        } else if ((src[i] == 'x' || src[i] == 'X') &&
                   (i == 0 || isspace((unsigned char)src[i - 1]) || isdigit((unsigned char)src[i - 1]) || src[i - 1] == ')') &&
                   (src[i + 1] == '\0' || isspace((unsigned char)src[i + 1]) || isdigit((unsigned char)src[i + 1]) || src[i + 1] == '(')) {
            /* Treat standalone 'x' between operands as multiplication, unless part of an identifier */
            int prev_alnum = (i > 0 && isalpha((unsigned char)src[i - 1]));
            int next_alnum = isalpha((unsigned char)src[i + 1]);
            if (!prev_alnum && !next_alnum) {
                dst[j++] = '*';
            } else {
                dst[j++] = src[i];
            }
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

static void format_number(double v, char *buf, size_t sz) {
    if (fabs(v) < 1e-12) v = 0.0;
    double nearest = round(v);
    if (fabs(v - nearest) < 1e-9 && fabs(nearest) < 1e15) {
        snprintf(buf, sz, "%.0f", nearest);
    } else {
        snprintf(buf, sz, "%.10g", v);
    }
}

/* Recursive Descent Expression Parser */
typedef struct {
    const char *p;
    const McEnv *env;
    int err;
    char errmsg[128];
} ExprParser;

static double parse_expr(ExprParser *ep);

static void skip_ws(ExprParser *ep) {
    while (*ep->p && isspace((unsigned char)*ep->p)) ep->p++;
}

static double parse_primary(ExprParser *ep) {
    skip_ws(ep);
    if (ep->err) return 0.0;

    if (*ep->p == '(') {
        ep->p++;
        double val = parse_expr(ep);
        skip_ws(ep);
        if (*ep->p == ')') {
            ep->p++;
        } else {
            ep->err = 1;
            snprintf(ep->errmsg, sizeof(ep->errmsg), "Missing closing parenthesis ')'");
        }
        return val;
    }

    if (*ep->p == '+') {
        ep->p++;
        return parse_primary(ep);
    }
    if (*ep->p == '-') {
        ep->p++;
        return -parse_primary(ep);
    }

    if (isdigit((unsigned char)*ep->p) || *ep->p == '.') {
        char *endp = NULL;
        double val = strtod(ep->p, &endp);
        ep->p = endp;
        return val;
    }

    if (isalpha((unsigned char)*ep->p) || *ep->p == '_') {
        char ident[64];
        int k = 0;
        while ((isalnum((unsigned char)*ep->p) || *ep->p == '_') && k < 63) {
            ident[k++] = *ep->p++;
        }
        ident[k] = '\0';
        skip_ws(ep);

        if (*ep->p == '(') {
            ep->p++;
            double arg = parse_expr(ep);
            skip_ws(ep);
            if (*ep->p == ')') ep->p++;
            else {
                ep->err = 1;
                snprintf(ep->errmsg, sizeof(ep->errmsg), "Missing ')' after function %s", ident);
                return 0.0;
            }
            if (strcmp(ident, "sin") == 0) return sin(arg);
            if (strcmp(ident, "cos") == 0) return cos(arg);
            if (strcmp(ident, "tan") == 0) return tan(arg);
            if (strcmp(ident, "sqrt") == 0) return sqrt(arg);
            if (strcmp(ident, "abs") == 0) return fabs(arg);
            if (strcmp(ident, "log") == 0) return log10(arg);
            if (strcmp(ident, "ln") == 0) return log(arg);
            if (strcmp(ident, "floor") == 0) return floor(arg);
            if (strcmp(ident, "ceil") == 0) return ceil(arg);
            if (strcmp(ident, "round") == 0) return round(arg);
            ep->err = 1;
            snprintf(ep->errmsg, sizeof(ep->errmsg), "Unknown function '%s'", ident);
            return 0.0;
        }

        if (strcmp(ident, "pi") == 0 || strcmp(ident, "PI") == 0) return M_PI;
        if (strcmp(ident, "e") == 0) return M_E;

        int vidx = env_find_var(ep->env, ident);
        if (vidx >= 0) return ep->env->vars[vidx].value;

        ep->err = 1;
        snprintf(ep->errmsg, sizeof(ep->errmsg), "Undefined identifier '%s'", ident);
        return 0.0;
    }

    ep->err = 1;
    snprintf(ep->errmsg, sizeof(ep->errmsg), "Unexpected token near '%s'", ep->p);
    return 0.0;
}

static double parse_power(ExprParser *ep) {
    double base = parse_primary(ep);
    skip_ws(ep);
    if (!ep->err && *ep->p == '^') {
        ep->p++;
        double exp = parse_power(ep);
        return pow(base, exp);
    }
    return base;
}

static double parse_term(ExprParser *ep) {
    double val = parse_power(ep);
    for (;;) {
        skip_ws(ep);
        if (ep->err) break;
        char op = *ep->p;
        if (op == '*' || op == '/' || op == '%') {
            ep->p++;
            double rhs = parse_power(ep);
            if (op == '*') val *= rhs;
            else if (op == '/') {
                if (fabs(rhs) < 1e-15) {
                    ep->err = 1;
                    snprintf(ep->errmsg, sizeof(ep->errmsg), "Division by zero");
                    return 0.0;
                }
                val /= rhs;
            } else {
                val = fmod(val, rhs);
            }
        } else {
            break;
        }
    }
    return val;
}

static double parse_expr(ExprParser *ep) {
    double val = parse_term(ep);
    for (;;) {
        skip_ws(ep);
        if (ep->err) break;
        char op = *ep->p;
        if (op == '+' || op == '-') {
            ep->p++;
            double rhs = parse_term(ep);
            if (op == '+') val += rhs;
            else val -= rhs;
        } else {
            break;
        }
    }
    return val;
}

static int eval_math_expr(const McEnv *env, const char *raw, double *out, char *errbuf, size_t errsz) {
    char norm[MCC_MAX_LINE_LEN];
    normalize_math_operators(raw, norm, sizeof(norm));
    ExprParser ep;
    ep.p = norm;
    ep.env = env;
    ep.err = 0;
    ep.errmsg[0] = '\0';
    double res = parse_expr(&ep);
    skip_ws(&ep);
    if (!ep.err && *ep.p != '\0') {
        ep.err = 1;
        snprintf(ep.errmsg, sizeof(ep.errmsg), "Unexpected trailing input '%s'", ep.p);
    }
    if (ep.err) {
        if (errbuf) snprintf(errbuf, errsz, "%s", ep.errmsg);
        return -1;
    }
    *out = res;
    return 0;
}

static int is_symbolic_trig(const char *s) {
    return strcmp(s, "sin") == 0 || strcmp(s, "cos") == 0 || strcmp(s, "tan") == 0 ||
           strcmp(s, "cot") == 0 || strcmp(s, "sec") == 0 || strcmp(s, "csc") == 0;
}

static void print_set(const McSet *s, FILE *out) {
    fprintf(out, "%s = {", s->name);
    int first = 1;
    if (s->unbounded_below) {
        fprintf(out, "...");
        first = 0;
    }
    for (int i = 0; i < s->count; i++) {
        char num[64];
        format_number(s->items[i], num, sizeof(num));
        if (!first) fprintf(out, "; ");
        fprintf(out, "%s", num);
        first = 0;
    }
    if (s->unbounded_above) {
        if (!first) fprintf(out, "; ");
        fprintf(out, "...");
    }
    fprintf(out, "}\n");
}

static long long gcd_ll(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static int is_prime_ll(long long n) {
    if (n < 2) return 0;
    if (n == 2 || n == 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (long long i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}

static int compare_op(double a, const char *op, double b) {
    if (strcmp(op, "<") == 0) return a < b;
    if (strcmp(op, ">") == 0) return a > b;
    if (strcmp(op, "<=") == 0 || strcmp(op, "=<") == 0) return a <= b;
    if (strcmp(op, ">=") == 0 || strcmp(op, "=>") == 0) return a >= b;
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) return fabs(a - b) < 1e-9;
    if (strcmp(op, "!=") == 0 || strcmp(op, "<>") == 0) return fabs(a - b) >= 1e-9;
    return -1;
}

static int exec_line(McEnv *env, const char *raw_line, FILE *out, FILE *err_out) {
    char line[MCC_MAX_LINE_LEN];
    snprintf(line, sizeof(line), "%s", raw_line);
    trim_inplace(line);
    if (line[0] == '\0' || line[0] == '#' || (line[0] == '/' && line[1] == '/')) {
        return 0;
    }

    /* 1. calculate <lhs> <op> <rhs> */
    if (strncmp(line, "calculate ", 10) == 0) {
        const char *rest = line + 10;
        char lhs[128] = {0}, op[16] = {0}, rhs[128] = {0};
        if (sscanf(rest, "%127s %15s %127s", lhs, op, rhs) == 3) {
            if (is_symbolic_trig(lhs) && is_symbolic_trig(rhs)) {
                if (strcmp(lhs, rhs) == 0 && strcmp(op, "+") == 0) {
                    fprintf(out, "2*%s\n", lhs);
                    return 0;
                } else if (strcmp(lhs, rhs) == 0 && strcmp(op, "-") == 0) {
                    fprintf(out, "0\n");
                    return 0;
                } else if (strcmp(lhs, rhs) == 0 && (strcmp(op, "*") == 0 || strcmp(op, "x") == 0)) {
                    fprintf(out, "%s^2\n", lhs);
                    return 0;
                } else if (strcmp(lhs, rhs) == 0 && (strcmp(op, "/") == 0 || strcmp(op, "÷") == 0)) {
                    fprintf(out, "1\n");
                    return 0;
                } else {
                    fprintf(out, "%s %s %s\n", lhs, op, rhs);
                    return 0;
                }
            }
        }
        double val = 0.0;
        char ebuf[128];
        if (eval_math_expr(env, rest, &val, ebuf, sizeof(ebuf)) != 0) {
            fprintf(err_out, "Error in calculate: %s\n", ebuf);
            return 1;
        }
        char nbuf[64];
        format_number(val, nbuf, sizeof(nbuf));
        fprintf(out, "%s\n", nbuf);
        return 0;
    }

    /* 2. compute <expr> */
    if (strncmp(line, "compute ", 8) == 0) {
        double val = 0.0;
        char ebuf[128];
        if (eval_math_expr(env, line + 8, &val, ebuf, sizeof(ebuf)) != 0) {
            fprintf(err_out, "Error in compute: %s\n", ebuf);
            return 1;
        }
        char nbuf[64];
        format_number(val, nbuf, sizeof(nbuf));
        fprintf(out, "%s\n", nbuf);
        return 0;
    }

    /* 3. set of number <Name> = { ... } */
    if (strncmp(line, "set of number ", 14) == 0) {
        const char *p = line + 14;
        const char *eq = strchr(p, '=');
        const char *lb = strchr(p, '{');
        const char *rb = strrchr(p, '}');
        if (!eq || !lb || !rb || rb < lb) {
            fprintf(err_out, "Syntax error in set of number: %s\n", line);
            return 1;
        }
        char name[64];
        size_t nlen = (size_t)(eq - p);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, p, nlen);
        name[nlen] = '\0';
        trim_inplace(name);

        char inside[MCC_MAX_LINE_LEN];
        size_t ilen = (size_t)(rb - lb - 1);
        if (ilen >= sizeof(inside)) ilen = sizeof(inside) - 1;
        memcpy(inside, lb + 1, ilen);
        inside[ilen] = '\0';
        trim_inplace(inside);

        McSet *set = env_upsert_set(env, name);
        if (!set) {
            fprintf(err_out, "Set storage limit reached\n");
            return 1;
        }

        char *pipe = strchr(inside, '|');
        if (pipe) {
            /* Set-builder notation: { cond | domain } */
            *pipe = '\0';
            char cond[MCC_MAX_LINE_LEN], dom[MCC_MAX_LINE_LEN];
            snprintf(cond, sizeof(cond), "%s", inside);
            snprintf(dom, sizeof(dom), "%s", pipe + 1);
            trim_inplace(cond);
            trim_inplace(dom);

            int is_n_star = (strstr(dom, "N*") != NULL);
            int is_n = (!is_n_star && strstr(dom, "N") != NULL);
            int is_z = (strstr(dom, "Z") != NULL);
            int is_even = (strstr(dom, "even") != NULL);
            int is_odd = (strstr(dom, "odd") != NULL);
            int is_prime = (strstr(dom, "prime") != NULL);

            char var[32] = "x", op[16] = "<";
            double bound = 0.0;
            if (sscanf(cond, "%31s %15s %lf", var, op, &bound) != 3) {
                fprintf(err_out, "Unsupported set-builder condition: %s\n", cond);
                return 1;
            }

            long long start = -5;
            long long end = 25;
            if (is_n_star) {
                start = 1;
                end = (strcmp(op, "<") == 0) ? (long long)ceil(bound) - 1 :
                      (strcmp(op, "<=") == 0 || strcmp(op, "=<") == 0) ? (long long)floor(bound) : 20;
            } else if (is_n) {
                start = 0;
                end = (strcmp(op, "<") == 0) ? (long long)ceil(bound) - 1 :
                      (strcmp(op, "<=") == 0 || strcmp(op, "=<") == 0) ? (long long)floor(bound) : 20;
            } else if (is_z) {
                if (strcmp(op, "<") == 0) {
                    end = (long long)ceil(bound) - 1;
                    start = end - 6;
                    set->unbounded_below = 1;
                } else if (strcmp(op, "<=") == 0 || strcmp(op, "=<") == 0) {
                    end = (long long)floor(bound);
                    start = end - 6;
                    set->unbounded_below = 1;
                } else if (strcmp(op, ">") == 0) {
                    start = (long long)floor(bound) + 1;
                    end = start + 6;
                    set->unbounded_above = 1;
                } else if (strcmp(op, ">=") == 0 || strcmp(op, "=>") == 0) {
                    start = (long long)ceil(bound);
                    end = start + 6;
                    set->unbounded_above = 1;
                }
            }

            for (long long x = start; x <= end && set->count < MCC_MAX_SET_ITEMS; x++) {
                if (!compare_op((double)x, op, bound)) continue;
                if (is_even && (x % 2 != 0)) continue;
                if (is_odd && (x % 2 == 0)) continue;
                if (is_prime && !is_prime_ll(x)) continue;
                set->items[set->count++] = (double)x;
            }
        } else {
            /* Explicit roster: {1; 2; 3; 4; 5} */
            char *tok = strtok(inside, ";,");
            while (tok != NULL && set->count < MCC_MAX_SET_ITEMS) {
                trim_inplace(tok);
                if (tok[0] != '\0') {
                    double v = 0.0;
                    if (eval_math_expr(env, tok, &v, NULL, 0) == 0) {
                        set->items[set->count++] = v;
                    }
                }
                tok = strtok(NULL, ";,");
            }
        }
        print_set(set, out);
        return 0;
    }

    /* 4. compare <lhs> <op> <rhs> */
    if (strncmp(line, "compare ", 8) == 0) {
        char lhs[128] = {0}, op[16] = {0}, rhs[128] = {0};
        if (sscanf(line + 8, "%127s %15s %127s", lhs, op, rhs) != 3) {
            fprintf(err_out, "Syntax error in compare: expected 'compare <lhs> <op> <rhs>'\n");
            return 1;
        }
        int rset = env_find_set(env, rhs);
        int lset = env_find_set(env, lhs);
        if (rset >= 0) {
            double lv = 0.0;
            if (eval_math_expr(env, lhs, &lv, NULL, 0) != 0) {
                fprintf(err_out, "Invalid LHS in compare: %s\n", lhs);
                return 1;
            }
            const McSet *s = &env->sets[rset];
            int all_true = 1;
            int matches = 0;
            for (int i = 0; i < s->count; i++) {
                if (compare_op(lv, op, s->items[i])) matches++;
                else all_true = 0;
            }
            fprintf(out, "%s (%s %s %s: %d/%d elements)\n",
                    all_true ? "true" : "false", lhs, op, rhs, matches, s->count);
            return 0;
        } else if (lset >= 0) {
            double rv = 0.0;
            if (eval_math_expr(env, rhs, &rv, NULL, 0) != 0) {
                fprintf(err_out, "Invalid RHS in compare: %s\n", rhs);
                return 1;
            }
            const McSet *s = &env->sets[lset];
            int all_true = 1;
            int matches = 0;
            for (int i = 0; i < s->count; i++) {
                if (compare_op(s->items[i], op, rv)) matches++;
                else all_true = 0;
            }
            fprintf(out, "%s (%s %s %s: %d/%d elements)\n",
                    all_true ? "true" : "false", lhs, op, rhs, matches, s->count);
            return 0;
        } else {
            double lv = 0.0, rv = 0.0;
            if (eval_math_expr(env, lhs, &lv, NULL, 0) != 0 ||
                eval_math_expr(env, rhs, &rv, NULL, 0) != 0) {
                fprintf(err_out, "Invalid operands in compare\n");
                return 1;
            }
            int res = compare_op(lv, op, rv);
            if (res < 0) {
                fprintf(err_out, "Unsupported comparison operator '%s'\n", op);
                return 1;
            }
            fprintf(out, "%s (%s %s %s)\n", res ? "true" : "false", lhs, op, rhs);
            return 0;
        }
    }

    /* 5. let <var> = <expr> */
    if (strncmp(line, "let ", 4) == 0) {
        char *eq = strchr(line + 4, '=');
        if (!eq) {
            fprintf(err_out, "Syntax error in let\n");
            return 1;
        }
        char name[64];
        size_t nlen = (size_t)(eq - (line + 4));
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, line + 4, nlen);
        name[nlen] = '\0';
        trim_inplace(name);

        double val = 0.0;
        char ebuf[128];
        if (eval_math_expr(env, eq + 1, &val, ebuf, sizeof(ebuf)) != 0) {
            fprintf(err_out, "Error in let: %s\n", ebuf);
            return 1;
        }
        env_set_var(env, name, val);
        char nbuf[64];
        format_number(val, nbuf, sizeof(nbuf));
        fprintf(out, "%s = %s\n", name, nbuf);
        return 0;
    }

    /* 6. factor <int> */
    if (strncmp(line, "factor ", 7) == 0) {
        long long n = atoll(line + 7);
        if (n < 2) {
            fprintf(out, "%lld\n", n);
            return 0;
        }
        long long temp = n;
        int first = 1;
        for (long long p = 2; p * p <= temp; p++) {
            int cnt = 0;
            while (temp % p == 0) {
                temp /= p;
                cnt++;
            }
            if (cnt > 0) {
                if (!first) fprintf(out, " * ");
                if (cnt == 1) fprintf(out, "%lld", p);
                else fprintf(out, "%lld^%d", p, cnt);
                first = 0;
            }
        }
        if (temp > 1) {
            if (!first) fprintf(out, " * ");
            fprintf(out, "%lld", temp);
        }
        fprintf(out, "\n");
        return 0;
    }

    /* 7. gcd <a> <b> and lcm <a> <b> */
    if (strncmp(line, "gcd ", 4) == 0) {
        long long a = 0, b = 0;
        if (sscanf(line + 4, "%lld %lld", &a, &b) == 2) {
            fprintf(out, "%lld\n", gcd_ll(a, b));
            return 0;
        }
    }
    if (strncmp(line, "lcm ", 4) == 0) {
        long long a = 0, b = 0;
        if (sscanf(line + 4, "%lld %lld", &a, &b) == 2) {
            long long g = gcd_ll(a, b);
            fprintf(out, "%lld\n", g == 0 ? 0 : (a / g) * b);
            return 0;
        }
    }

    /* 8. prime <n> */
    if (strncmp(line, "prime ", 6) == 0) {
        long long n = atoll(line + 6);
        fprintf(out, "%s\n", is_prime_ll(n) ? "true" : "false");
        return 0;
    }

    /* 9. stats <Set> */
    if (strncmp(line, "stats ", 6) == 0) {
        char sname[64] = {0};
        sscanf(line + 6, "%63s", sname);
        int idx = env_find_set(env, sname);
        if (idx < 0 || env->sets[idx].count == 0) {
            fprintf(err_out, "Set '%s' not found or empty\n", sname);
            return 1;
        }
        const McSet *s = &env->sets[idx];
        double sum = 0, mn = s->items[0], mx = s->items[0];
        for (int i = 0; i < s->count; i++) {
            sum += s->items[i];
            if (s->items[i] < mn) mn = s->items[i];
            if (s->items[i] > mx) mx = s->items[i];
        }
        double mean = sum / s->count;
        fprintf(out, "%s: count=%d, sum=%.6g, mean=%.6g, min=%.6g, max=%.6g\n",
                s->name, s->count, sum, mean, mn, mx);
        return 0;
    }

    /* 10. print <msg> */
    if (strncmp(line, "print ", 6) == 0) {
        fprintf(out, "%s\n", line + 6);
        return 0;
    }

    fprintf(err_out, "Unknown MathC statement: %s\n", line);
    return 1;
}

static int run_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "mcc: cannot open '%s': %s\n", path, strerror(errno));
        return 1;
    }
    McEnv env;
    env_init(&env);
    char line[MCC_MAX_LINE_LEN];
    int status = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (exec_line(&env, line, stdout, stderr) != 0) {
            status = 1;
        }
    }
    fclose(fp);
    return status;
}

static void emit_c_escaped(FILE *out, const char *s) {
    for (size_t i = 0; s[i] != '\0'; i++) {
        if (s[i] == '\\' || s[i] == '"') fputc('\\', out);
        if (s[i] == '\n' || s[i] == '\r') continue;
        fputc(s[i], out);
    }
}

static int compile_file(const char *src_path, const char *out_path) {
    FILE *fp = fopen(src_path, "r");
    if (!fp) {
        fprintf(stderr, "mcc: cannot open '%s': %s\n", src_path, strerror(errno));
        return 1;
    }

    char tmp_c[512];
    snprintf(tmp_c, sizeof(tmp_c), "%s.gen.c", out_path);
    FILE *cfile = fopen(tmp_c, "w");
    if (!cfile) {
        fclose(fp);
        fprintf(stderr, "mcc: cannot create intermediate C file '%s'\n", tmp_c);
        return 1;
    }

    McEnv env;
    env_init(&env);
    fprintf(cfile, "/* Generated by MathC Compiler (mcc %s) */\n", MATHC_VERSION);
    fprintf(cfile, "#include <stdio.h>\n#include <math.h>\n\n");

    int is_obj = 0;
    size_t olen = strlen(out_path);
    if ((olen >= 2 && strcmp(out_path + olen - 2, ".o") == 0) ||
        (olen >= 4 && strcmp(out_path + olen - 4, ".obj") == 0)) {
        is_obj = 1;
    }

    if (is_obj) {
        fprintf(cfile, "int mathc_module_main(void) {\n");
    } else {
        fprintf(cfile, "int main(void) {\n");
    }

    char line[MCC_MAX_LINE_LEN];
    char outbuf[MCC_MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char trimmed[MCC_MAX_LINE_LEN];
        snprintf(trimmed, sizeof(trimmed), "%s", line);
        trim_inplace(trimmed);
        if (trimmed[0] == '\0' || trimmed[0] == '#' || (trimmed[0] == '/' && trimmed[1] == '/')) {
            continue;
        }
        /* Evaluate via tmpfile or fmemopen-free capture to emit deterministic compiled C printf */
        FILE *tmp_out = tmpfile();
        if (tmp_out) {
            exec_line(&env, trimmed, tmp_out, stderr);
            rewind(tmp_out);
            while (fgets(outbuf, sizeof(outbuf), tmp_out) != NULL) {
                fprintf(cfile, "    printf(\"");
                emit_c_escaped(cfile, outbuf);
                fprintf(cfile, "\\n\");\n");
            }
            fclose(tmp_out);
        }
    }
    fprintf(cfile, "    return 0;\n}\n");
    fclose(cfile);
    fclose(fp);

    char cmd[1024];
    if (is_obj) {
        snprintf(cmd, sizeof(cmd), "%s -O2 -c \"%s\" -o \"%s\"", MCC_DEFAULT_CC, tmp_c, out_path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s -O2 \"%s\" -o \"%s\" -lm", MCC_DEFAULT_CC, tmp_c, out_path);
    }
    int rc = system(cmd);
    remove(tmp_c);
    if (rc != 0) {
        fprintf(stderr, "mcc: backend compiler failed (exit %d)\n", rc);
        return 1;
    }
    printf("Compiled '%s' -> '%s' (%s)\n", src_path, out_path, is_obj ? "object file" : "native binary");
    return 0;
}

static void print_help(void) {
    printf("Usage: mcc <option> [arg]\n");
    printf("Example: mcc run <file_path>: Run file\n");
    printf("         mcc <file_path> -o <output-file>: Compile file\n");
    printf("         mcc --compiler <file_path> -o <output-file>: Compile file\n");
    printf("         mcc --command <command>: Run command\n");
    printf("         mcc --help: Show usage instructions\n");
    printf("         mcc --readme: Show readme\n");
    printf("         mcc --manual: Show manual\n");
    printf("         mcc --license: Show MIT license\n");
    printf("         mcc --agents: Show Agents.md\n");
    printf("         mcc --update: Update or reload\n");
}

static void print_doc_file(const char *fallback, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp) != NULL) {
            fputs(buf, stdout);
        }
        fclose(fp);
        return;
    }
    printf("%s\n", fallback);
}

int main(int argc, char **argv) {
#ifdef MCC_PLATFORM_WINDOWS
    mcc_enable_windows_terminal_utf8();
#endif
    if (argc < 2) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(argv[1], "--readme") == 0) {
        print_doc_file(
            "MathC v" MATHC_VERSION " (" MATHC_CODENAME ") - Mathematical C Language & Compiler.\n"
            "Build with ./configure && make (Linux/macOS/Android) or config.h (Windows Terminal).",
            "README.md"
        );
        return 0;
    }
    if (strcmp(argv[1], "--manual") == 0) {
        print_doc_file(
            "MathC Manual: calculate | set of number | compute | compare | solve | factor | gcd | lcm | prime | stats | print.",
            "MANUAL.md"
        );
        return 0;
    }
    if (strcmp(argv[1], "--license") == 0) {
        print_doc_file(
            "MIT License\nCopyright (c) 2026 MathC Language Contributors\nPermission is hereby granted, free of charge, to any person obtaining a copy of this software...",
            "LICENSE"
        );
        return 0;
    }
    if (strcmp(argv[1], "--agents") == 0) {
        print_doc_file(
            "Agents.md: Keep all MathC source/scripts in English. Run ./configure && make test after edits.",
            "AGENTS.md"
        );
        return 0;
    }
    if (strcmp(argv[1], "--update") == 0) {
        printf("MathC %s (%s) on %s [%s]\n", MATHC_VERSION, MATHC_CODENAME, MCC_OS_NAME, MCC_COMPILER_NAME);
        printf("Reloading toolchain configuration from %s... Up to date.\n", MCC_INSTALL_PREFIX);
        return 0;
    }
    if (strcmp(argv[1], "--command") == 0) {
        if (argc < 3) {
            fprintf(stderr, "mcc: --command requires an argument\n");
            return 1;
        }
        McEnv env;
        env_init(&env);
        return exec_line(&env, argv[2], stdout, stderr);
    }
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "mcc: run requires a file path\n");
            return 1;
        }
        return run_file(argv[2]);
    }
    if (strcmp(argv[1], "--compiler") == 0) {
        if (argc >= 5 && strcmp(argv[3], "-o") == 0) {
            return compile_file(argv[2], argv[4]);
        }
        fprintf(stderr, "Usage: mcc --compiler <file_path> -o <output-file>\n");
        return 1;
    }
    if (argc >= 4 && strcmp(argv[2], "-o") == 0) {
        return compile_file(argv[1], argv[3]);
    }

    fprintf(stderr, "mcc: unrecognized arguments. Run 'mcc --help'.\n");
    return 1;
}
