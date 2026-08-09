/*
 * args.c — Unix/GNU argument parsing implementation
 *
 * Parsing rules implemented (in priority order):
 *
 *  1. "--"           → end of options; rest → positionals
 *  2. "-"            → treated as positional (means stdin by convention)
 *  3. "--long=val"   → long option with attached value
 *  4. "--long val"   → long option; value is next argv element
 *  5. "--long"       → long boolean flag (count++)
 *  6. "--no-NAME"    → long boolean negation (bval=0, count cleared)
 *  7. "-xyz"         → combined short flags (each must be BOOL)
 *  8. "-xVAL"        → short option -x with value attached
 *  9. "-x val"       → short option -x; value is next argv element
 * 10. "-x"           → short boolean flag
 * 11. non-option arg → positional (or stop if POSIXLY_CORRECT)
 */

#include "args.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static size_t count_opts(const args_opt_t *opts)
{
    size_t n = 0;
    while (opts[n].short_name || opts[n].long_name)
        n++;
    return n;
}

/* Append a string to a heap-allocated array of strings. */
static int push_string(char ***arr, int *cnt, const char *s)
{
    char **tmp = realloc(*arr, (*cnt + 1) * sizeof(char *));
    if (!tmp) return -1;
    *arr = tmp;
    (*arr)[*cnt] = (char *)s;   /* we keep original pointer, not a copy */
    (*cnt)++;
    return 0;
}

/* Append raw value to opt->values[]. */
static int push_value(args_opt_t *opt, const char *val)
{
    const char **tmp = realloc(opt->values,
                               (opt->nvalues + 1) * sizeof(char *));
    if (!tmp) return -1;
    opt->values = tmp;
    opt->values[opt->nvalues++] = val;
    return 0;
}

/* Parse and store a typed value into opt. Returns 0 on success. */
static int store_value(args_opt_t *opt, const char *val,
                       const args_ctx_t *ctx)
{
    opt->value = val;
    push_value(opt, val);   /* ignore OOM here; non-fatal for multi-val */
    opt->count++;

    switch (opt->type) {
    case ARGS_OPT_BOOL:
        opt->bval = 1;
        return 0;

    case ARGS_OPT_STRING:
        return 0;   /* value already stored */

    case ARGS_OPT_INT: {
        char *end;
        errno = 0;
        long v = strtol(val, &end, 0);   /* 0 = auto-detect base (0x…, 0…) */
        if (*end != '\0' || errno != 0) {
            args_error(ctx, "invalid integer argument '%s'", val);
            return ARGS_ERR_BAD_INT;
        }
        opt->ival = v;
        return 0;
    }

    case ARGS_OPT_FLOAT: {
        char *end;
        errno = 0;
        double v = strtod(val, &end);
        if (*end != '\0' || errno != 0) {
            args_error(ctx, "invalid numeric argument '%s'", val);
            return ARGS_ERR_BAD_FLOAT;
        }
        opt->fval = v;
        return 0;
    }
    }
    return 0;
}

/* Apply default_value to opts that were never seen. */
static void apply_defaults(args_opt_t *opts)
{
    for (args_opt_t *o = opts; o->short_name || o->long_name; o++) {
        if (o->count > 0 || !o->default_value)
            continue;
        o->value = o->default_value;
        switch (o->type) {
        case ARGS_OPT_BOOL:
            o->bval = (strcmp(o->default_value, "0") != 0 &&
                       strcmp(o->default_value, "false") != 0 &&
                       strcmp(o->default_value, "no") != 0);
            break;
        case ARGS_OPT_INT: {
            char *end;
            o->ival = strtol(o->default_value, &end, 0);
            break;
        }
        case ARGS_OPT_FLOAT: {
            char *end;
            o->fval = strtod(o->default_value, &end);
            break;
        }
        case ARGS_OPT_STRING:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public: args_ctx_init                                               */
/* ------------------------------------------------------------------ */

void args_ctx_init(args_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->exit_on_error = 1;
    ctx->allow_unknown = 0;
    ctx->posix_order   = (getenv("POSIXLY_CORRECT") != NULL);
}

/* ------------------------------------------------------------------ */
/* Public: args_error                                                  */
/* ------------------------------------------------------------------ */

void args_error(const args_ctx_t *ctx, const char *fmt, ...)
{
    const char *prog = ctx ? ctx->prog : "program";
    if (!prog) prog = "program";

    fprintf(stderr, "%s: ", prog);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    fprintf(stderr, "Try '%s --help' for more information.\n", prog);

    if (!ctx || ctx->exit_on_error)
        exit(2);
}

/* ------------------------------------------------------------------ */
/* Public: args_find_short / args_find_long                            */
/* ------------------------------------------------------------------ */

args_opt_t *args_find_short(args_opt_t *opts, int short_name)
{
    for (args_opt_t *o = opts; o->short_name || o->long_name; o++)
        if (o->short_name == short_name)
            return o;
    return NULL;
}

args_opt_t *args_find_long(args_opt_t *opts, const char *long_name)
{
    for (args_opt_t *o = opts; o->short_name || o->long_name; o++)
        if (o->long_name && strcmp(o->long_name, long_name) == 0)
            return o;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Internal: handle long option  "--name" or "--name=val" or "--no-x"  */
/* ------------------------------------------------------------------ */

static args_err_t handle_long(const char *arg,   /* without leading "--" */
                              int         i,
                              int         argc,
                              char      **argv,
                              args_opt_t *opts,
                              args_ctx_t *ctx,
                              int        *consumed_next)
{
    *consumed_next = 0;

    /* Split on '=' */
    const char *eq = strchr(arg, '=');
    size_t namelen = eq ? (size_t)(eq - arg) : strlen(arg);
    const char *inline_val = eq ? (eq + 1) : NULL;

    /* Check for --no-NAME negation */
    int negated = 0;
    const char *lookup_name = arg;
    char negbuf[256];
    if (namelen > 3 && strncmp(arg, "no-", 3) == 0) {
        negated = 1;
        /* look up the NAME part */
        size_t nlen = namelen - 3;
        if (nlen >= sizeof(negbuf)) nlen = sizeof(negbuf) - 1;
        strncpy(negbuf, arg + 3, nlen);
        negbuf[nlen] = '\0';
        lookup_name = negbuf;
        namelen = nlen;
    }

    /* Find matching option */
    args_opt_t *opt = NULL;
    for (args_opt_t *o = opts; o->short_name || o->long_name; o++) {
        if (!o->long_name) continue;
        if (strncmp(o->long_name, lookup_name, namelen) == 0 &&
            o->long_name[namelen] == '\0') {
            opt = o;
            break;
        }
    }

    if (!opt) {
        if (ctx->allow_unknown) {
            /* Reconstruct "--arg" and save */
            size_t _flen = strlen(arg) + 3;
            char *full = malloc(_flen);
            if (!full) return ARGS_ERR_OOM;
            snprintf(full, _flen, "--%s", arg);
            push_string(&ctx->unknown_opts, &ctx->nunknown, full);
            return ARGS_OK;
        }
        /* Reconstruct for error message */
        char namebuf[256];
        snprintf(namebuf, sizeof(namebuf), "--%.*s", (int)namelen, arg);
        args_error(ctx, "unrecognized option '%s'", namebuf);
        return ARGS_ERR_UNKNOWN_OPT;
    }

    /* Handle negation */
    if (negated) {
        if (opt->type != ARGS_OPT_BOOL) {
            char namebuf[256];
            snprintf(namebuf, sizeof(namebuf), "--no-%s", opt->long_name);
            args_error(ctx, "'%s' is not a boolean option", namebuf);
            return ARGS_ERR_UNKNOWN_OPT;
        }
        opt->bval  = 0;
        opt->count = 0;   /* reset — negation clears it */
        opt->value = "0";
        return ARGS_OK;
    }

    /* Boolean: no value expected */
    if (opt->type == ARGS_OPT_BOOL) {
        if (inline_val) {
            args_error(ctx, "option '--%s' does not take an argument",
                       opt->long_name);
            return ARGS_ERR_UNKNOWN_OPT;
        }
        opt->bval = 1;
        opt->count++;
        opt->value = "1";
        return ARGS_OK;
    }

    /* Non-boolean: needs a value */
    const char *val = inline_val;
    if (!val) {
        if (i + 1 >= argc) {
            args_error(ctx, "option '--%s' requires an argument",
                       opt->long_name);
            return ARGS_ERR_MISSING_VAL;
        }
        val = argv[i + 1];
        *consumed_next = 1;
    }

    return store_value(opt, val, ctx);
}

/* ------------------------------------------------------------------ */
/* Internal: handle short option cluster  "-xyz" / "-xVAL"            */
/* ------------------------------------------------------------------ */

static args_err_t handle_short(const char *cluster,   /* e.g. "xvfFILE" */
                               int         i,
                               int         argc,
                               char      **argv,
                               args_opt_t *opts,
                               args_ctx_t *ctx,
                               int        *consumed_next)
{
    *consumed_next = 0;
    const char *p = cluster;

    while (*p) {
        int ch = (unsigned char)*p;
        args_opt_t *opt = args_find_short(opts, ch);

        if (!opt) {
            if (ctx->allow_unknown) {
                char *s = malloc(3);
                if (s) { s[0] = '-'; s[1] = (char)ch; s[2] = '\0';
                    push_string(&ctx->unknown_opts, &ctx->nunknown, s); }
                p++;
                continue;
            }
            args_error(ctx, "invalid option -- '%c'", ch);
            return ARGS_ERR_UNKNOWN_OPT;
        }

        p++;   /* move past the option character */

        if (opt->type == ARGS_OPT_BOOL) {
            opt->bval = 1;
            opt->count++;
            opt->value = "1";
            /* continue to next char in cluster */
            continue;
        }

        /* Non-boolean: rest of cluster is the value, or next argv */
        const char *val;
        if (*p) {
            /* value is attached: -xVALUE */
            val = p;
            p += strlen(p);   /* consume rest */
        } else {
            /* value is next argv */
            if (i + 1 >= argc) {
                args_error(ctx, "option requires an argument -- '%c'", ch);
                return ARGS_ERR_MISSING_VAL;
            }
            val = argv[i + 1];
            *consumed_next = 1;
        }
        return store_value(opt, val, ctx);
    }
    return ARGS_OK;
}

/* ------------------------------------------------------------------ */
/* Public: args_parse                                                  */
/* ------------------------------------------------------------------ */

args_err_t args_parse(int argc, char **argv,
                      args_opt_t *opts, args_ctx_t *ctx)
{
    /* Local fallback context if caller passes NULL */
    args_ctx_t local_ctx;
    if (!ctx) {
        args_ctx_init(&local_ctx);
        ctx = &local_ctx;
    }

    /* Auto-detect program name from argv[0] */
    if (!ctx->prog && argc > 0) {
        const char *base = strrchr(argv[0], '/');
        ctx->prog = base ? base + 1 : argv[0];
    }

    /* Respect POSIXLY_CORRECT */
    if (getenv("POSIXLY_CORRECT"))
        ctx->posix_order = 1;

    /* Save opts pointer for help/free */
    ctx->_opts  = opts;
    ctx->_nopts = count_opts(opts);

    int end_of_opts = 0;   /* set after -- */
    args_err_t rc = ARGS_OK;

    for (int i = 1; i < argc; ) {
        const char *arg = argv[i];

        /* ---- end-of-options sentinel ---- */
        if (!end_of_opts && strcmp(arg, "--") == 0) {
            ctx->dashdash_seen = 1;
            end_of_opts = 1;
            i++;
            continue;
        }

        /* ---- after --: everything is positional ---- */
        if (end_of_opts) {
            push_string(&ctx->positionals, &ctx->npositionals, arg);
            i++;
            continue;
        }

        /* ---- "-" alone: conventional stdin marker → positional ---- */
        if (strcmp(arg, "-") == 0) {
            push_string(&ctx->positionals, &ctx->npositionals, arg);
            i++;
            continue;
        }

        /* ---- long option ---- */
        if (arg[0] == '-' && arg[1] == '-') {
            int consumed = 0;
            rc = handle_long(arg + 2, i, argc, argv, opts, ctx, &consumed);
            if (rc != ARGS_OK && ctx->exit_on_error) exit(2);
            i += 1 + consumed;
            continue;
        }

        /* ---- short option(s) ---- */
        if (arg[0] == '-' && arg[1] != '\0') {
            int consumed = 0;
            rc = handle_short(arg + 1, i, argc, argv, opts, ctx, &consumed);
            if (rc != ARGS_OK && ctx->exit_on_error) exit(2);
            i += 1 + consumed;
            continue;
        }

        /* ---- positional argument ---- */
        push_string(&ctx->positionals, &ctx->npositionals, arg);
        i++;

        /* POSIX mode: stop parsing options after first positional */
        if (ctx->posix_order) {
            end_of_opts = 1;
        }
    }

    apply_defaults(opts);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Public: args_positionals                                            */
/* ------------------------------------------------------------------ */

char **args_positionals(const args_ctx_t *ctx, int *out_count)
{
    if (!ctx) { if (out_count) *out_count = 0; return NULL; }
    if (out_count) *out_count = ctx->npositionals;
    return ctx->positionals;
}

/* ------------------------------------------------------------------ */
/* Public: args_free                                                   */
/* ------------------------------------------------------------------ */

void args_free(args_opt_t *opts, args_ctx_t *ctx)
{
    if (opts) {
        for (args_opt_t *o = opts; o->short_name || o->long_name; o++) {
            free(o->values);
            o->values  = NULL;
            o->nvalues = 0;
        }
    }
    if (ctx) {
        free(ctx->positionals);
        ctx->positionals  = NULL;
        ctx->npositionals = 0;

        /* unknown_opts entries were heap-alloc'd in handle_long/short */
        for (int i = 0; i < ctx->nunknown; i++)
            free(ctx->unknown_opts[i]);
        free(ctx->unknown_opts);
        ctx->unknown_opts = NULL;
        ctx->nunknown     = 0;
    }
}
