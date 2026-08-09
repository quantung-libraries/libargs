/*
 * args.h — Unix/GNU-style argument parsing library
 *
 * Conventions supported:
 *   -x            short flag
 *   -x val        short option with value (space-separated)
 *   -xval         short option with value (attached)
 *   -xyz          combined short flags (if none take values)
 *   --long        long flag
 *   --long=val    long option with value (= separated)
 *   --long val    long option with value (space-separated)
 *   --no-NAME     long negation (for boolean long opts)
 *   --            end of options; everything after is positional
 *   -             stdin placeholder; treated as positional
 *   POSIXLY_CORRECT env: stop parsing at first non-option
 *
 * Error output goes to stderr per POSIX/GNU convention.
 * Unknown options cause exit(2) by default (configurable).
 *
 * Usage:
 *   1. Declare an args_opt_t[] table (terminated by ARGS_OPT_END).
 *   2. Call args_parse().
 *   3. Inspect opt->count / opt->value / opt->values[].
 *   4. args_positionals() returns remaining argv after --.
 */

#ifndef ARGS_H
#define ARGS_H

#include <stddef.h>  /* size_t */
#include <stdio.h>   /* FILE   */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Option type                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    ARGS_OPT_BOOL   = 0,  /* flag: no argument                        */
    ARGS_OPT_STRING = 1,  /* requires one string argument             */
    ARGS_OPT_INT    = 2,  /* requires one integer argument            */
    ARGS_OPT_FLOAT  = 3,  /* requires one floating-point argument     */
} args_type_t;

/* ------------------------------------------------------------------ */
/* Single option descriptor (one row in the option table)             */
/* ------------------------------------------------------------------ */

typedef struct args_opt {
    /* --- definition (fill these in) --- */
    int          short_name;    /* e.g. 'v', 'f', 0 if none           */
    const char  *long_name;     /* e.g. "verbose", "file", NULL        */
    args_type_t  type;          /* ARGS_OPT_BOOL / STRING / INT / FLOAT*/
    const char  *metavar;       /* shown in help: --file=<FILE>        */
    const char  *help;          /* one-line help text                  */
    const char  *default_value; /* string form of default (or NULL)    */

    /* --- results (filled by args_parse) --- */
    int          count;         /* how many times seen (0 = not set)   */
    const char  *value;         /* last raw string value               */
    long         ival;          /* parsed integer (ARGS_OPT_INT)       */
    double       fval;          /* parsed float   (ARGS_OPT_FLOAT)     */
    int          bval;          /* boolean value  (ARGS_OPT_BOOL)      */

    /* multi-value support */
    const char **values;        /* all raw values (heap, free with args_free) */
    size_t       nvalues;       /* length of values[]                  */
} args_opt_t;

/* Sentinel to end an option table */
#define ARGS_OPT_END  { 0, NULL, ARGS_OPT_BOOL, NULL, NULL, NULL, 0, NULL, 0, 0.0, 0, NULL, 0 }

/* Convenience initialiser macros */
#define ARGS_BOOL(sh, lg, hlp) \
    { (sh), (lg), ARGS_OPT_BOOL,   NULL,   (hlp), NULL, 0,NULL,0,0.0,0,NULL,0 }

#define ARGS_STRING(sh, lg, meta, hlp, def) \
    { (sh), (lg), ARGS_OPT_STRING, (meta), (hlp), (def), 0,NULL,0,0.0,0,NULL,0 }

#define ARGS_INT(sh, lg, meta, hlp, def) \
    { (sh), (lg), ARGS_OPT_INT,    (meta), (hlp), (def), 0,NULL,0,0.0,0,NULL,0 }

#define ARGS_FLOAT(sh, lg, meta, hlp, def) \
    { (sh), (lg), ARGS_OPT_FLOAT,  (meta), (hlp), (def), 0,NULL,0,0.0,0,NULL,0 }

/* ------------------------------------------------------------------ */
/* Parser context                                                       */
/* ------------------------------------------------------------------ */

typedef struct args_ctx {
    /* --- configuration (set before args_parse) --- */
    const char   *prog;          /* program name (auto-set from argv[0]) */
    const char   *usage_line;    /* override usage line (or NULL)        */
    const char   *description;   /* shown before option list in --help   */
    const char   *epilog;        /* shown after option list in --help    */
    int           exit_on_error; /* default 1; set 0 to get error codes  */
    int           allow_unknown; /* 1 = collect unknown opts, don't exit */
    int           posix_order;   /* 1 = stop at first non-option arg     */
                                 /* (auto-set if POSIXLY_CORRECT in env) */

    /* --- results (filled by args_parse) --- */
    char        **positionals;   /* non-option arguments (heap)          */
    int           npositionals;  /* count                                */
    char        **unknown_opts;  /* collected when allow_unknown=1       */
    int           nunknown;      /* count                                */
    int           dashdash_seen; /* 1 if -- was encountered              */

    /* internal — do not touch */
    args_opt_t   *_opts;
    size_t        _nopts;
} args_ctx_t;

/* ------------------------------------------------------------------ */
/* Error codes                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    ARGS_OK              =  0,
    ARGS_ERR_UNKNOWN_OPT = -1,
    ARGS_ERR_MISSING_VAL = -2,
    ARGS_ERR_BAD_INT     = -3,
    ARGS_ERR_BAD_FLOAT   = -4,
    ARGS_ERR_OOM         = -5,
} args_err_t;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/*
 * args_ctx_init — initialise a context with defaults.
 * Call this before setting any fields on ctx.
 */
void args_ctx_init(args_ctx_t *ctx);

/*
 * args_parse — parse argv[1..argc-1].
 *
 * opts[]  : option table, terminated by ARGS_OPT_END.
 * ctx     : parser context (pass NULL for default behaviour).
 *
 * Returns ARGS_OK on success, negative args_err_t on error.
 * If ctx->exit_on_error (default), calls exit(2) on error.
 *
 * After return:
 *   - Each matching args_opt_t has count/value/ival/fval/bval set.
 *   - ctx->positionals[0..npositionals-1] hold non-option args.
 *   - Default values are applied to opts that were never seen.
 */
args_err_t args_parse(int argc, char **argv,
                      args_opt_t *opts, args_ctx_t *ctx);

/*
 * Lookup helpers — find an option by short name or long name.
 * Return NULL if not found.
 */
args_opt_t *args_find_short(args_opt_t *opts, int short_name);
args_opt_t *args_find_long (args_opt_t *opts, const char *long_name);

/*
 * args_positionals — convenience: returns ctx->positionals.
 * (ctx may be NULL if you passed one to args_parse.)
 */
char **args_positionals(const args_ctx_t *ctx, int *out_count);

/*
 * args_print_usage — print one-line usage to fp (stdout/stderr).
 */
void args_print_usage(FILE *fp, const args_ctx_t *ctx,
                      const args_opt_t *opts);

/*
 * args_print_help — print full --help style output to fp.
 * Automatically called when -h / --help is seen (if those opts
 * are defined in the table).
 */
void args_print_help(FILE *fp, const args_ctx_t *ctx,
                     const args_opt_t *opts);

/*
 * args_free — release heap memory allocated by args_parse.
 * Pass the same ctx and opts that were used.
 */
void args_free(args_opt_t *opts, args_ctx_t *ctx);

/*
 * args_error — print a GNU-style error message to stderr and,
 * if ctx->exit_on_error, call exit(2).
 * Use this from your own option validation.
 *
 *   args_error(ctx, "file '%s' not found", path);
 *   → "prog: file 'foo.txt' not found\nTry 'prog --help' for more info.\n"
 */
void args_error(const args_ctx_t *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif /* ARGS_H */
