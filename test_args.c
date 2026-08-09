/*
 * test_args.c — test suite for libargs
 *
 * Tests cover every parsing convention:
 *   - short flags, combined, with value attached / separated
 *   - long flags, with = / space-separated value
 *   - --no-NAME negation
 *   - -- end-of-options sentinel
 *   - "-" as positional (stdin)
 *   - POSIXLY_CORRECT ordering
 *   - default values (string, int, float, bool)
 *   - repeated options (count, multi-values)
 *   - allow_unknown option collection
 *   - int / float type parsing (incl. hex, octal)
 *   - error detection (missing value, unknown opt, bad int/float)
 */

#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Mini test framework                                                 */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(expr) do {                                               \
    if (expr) {                                                        \
        g_pass++;                                                      \
    } else {                                                           \
        fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        g_fail++;                                                      \
    }                                                                  \
} while (0)

#define TEST(name) static void name(void)

/* ------------------------------------------------------------------ */
/* Helper: build argv array from string literals                       */
/* ------------------------------------------------------------------ */

/* Build a NULL-terminated argv.  Pass strings, end with NULL.
   The returned argc and argv are local; do not free.
   For zero extra args, use MAKE_ARGV0(). */
#define MAKE_ARGV(...)  \
    char *_av[] = { "prog", __VA_ARGS__, NULL };  \
    int   _ac   = (int)(sizeof(_av)/sizeof(_av[0])) - 1

/* Zero-arg version */
#define MAKE_ARGV0() \
    char *_av[] = { "prog", NULL }; \
    int   _ac   = 1

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

TEST(test_short_flag)
{
    MAKE_ARGV("-v");
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", "be verbose"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_err_t rc = args_parse(_ac, _av, opts, &ctx);
    CHECK(rc == ARGS_OK);
    CHECK(opts[0].count == 1);
    CHECK(opts[0].bval  == 1);
    args_free(opts, &ctx);
}

TEST(test_short_flag_repeated)
{
    MAKE_ARGV("-v", "-v", "-v");
    args_opt_t opts[] = {
        ARGS_BOOL('v', NULL, "verbosity level"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 3);
    CHECK(opts[0].bval  == 1);
    args_free(opts, &ctx);
}

TEST(test_short_combined)
{
    MAKE_ARGV("-xvz");
    args_opt_t opts[] = {
        ARGS_BOOL('x', NULL, "extract"),
        ARGS_BOOL('v', NULL, "verbose"),
        ARGS_BOOL('z', NULL, "gzip"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].bval == 1);
    CHECK(opts[1].bval == 1);
    CHECK(opts[2].bval == 1);
    args_free(opts, &ctx);
}

TEST(test_short_value_space)
{
    MAKE_ARGV("-f", "output.txt");
    args_opt_t opts[] = {
        ARGS_STRING('f', "file", "FILE", "output file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 1);
    CHECK(strcmp(opts[0].value, "output.txt") == 0);
    args_free(opts, &ctx);
}

TEST(test_short_value_attached)
{
    MAKE_ARGV("-foutput.txt");
    args_opt_t opts[] = {
        ARGS_STRING('f', "file", "FILE", "output file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 1);
    CHECK(strcmp(opts[0].value, "output.txt") == 0);
    args_free(opts, &ctx);
}

TEST(test_long_flag)
{
    MAKE_ARGV("--verbose");
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", "be verbose"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 1);
    CHECK(opts[0].bval  == 1);
    args_free(opts, &ctx);
}

TEST(test_long_value_equals)
{
    MAKE_ARGV("--file=data.csv");
    args_opt_t opts[] = {
        ARGS_STRING('f', "file", "FILE", "input file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 1);
    CHECK(strcmp(opts[0].value, "data.csv") == 0);
    args_free(opts, &ctx);
}

TEST(test_long_value_space)
{
    MAKE_ARGV("--file", "data.csv");
    args_opt_t opts[] = {
        ARGS_STRING('f', "file", "FILE", "input file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 1);
    CHECK(strcmp(opts[0].value, "data.csv") == 0);
    args_free(opts, &ctx);
}

TEST(test_long_no_negation)
{
    MAKE_ARGV("--verbose", "--no-verbose");
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", "be verbose"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    /* --no-verbose resets count to 0 and bval to 0 */
    CHECK(opts[0].bval  == 0);
    CHECK(opts[0].count == 0);
    args_free(opts, &ctx);
}

TEST(test_dashdash_end_of_opts)
{
    MAKE_ARGV("-v", "--", "-f", "not-an-opt");
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", "verbose"),
        ARGS_STRING('f', "file", "FILE", "file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    /* -v parsed as option */
    CHECK(opts[0].bval == 1);
    /* -f and not-an-opt became positionals */
    CHECK(opts[1].count == 0);
    CHECK(ctx.npositionals == 2);
    CHECK(strcmp(ctx.positionals[0], "-f") == 0);
    CHECK(strcmp(ctx.positionals[1], "not-an-opt") == 0);
    CHECK(ctx.dashdash_seen == 1);
    args_free(opts, &ctx);
}

TEST(test_stdin_dash_positional)
{
    MAKE_ARGV("-");
    args_opt_t opts[] = { ARGS_OPT_END };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(ctx.npositionals == 1);
    CHECK(strcmp(ctx.positionals[0], "-") == 0);
    args_free(opts, &ctx);
}

TEST(test_positionals_mixed)
{
    MAKE_ARGV("-v", "file1.txt", "file2.txt");
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", "verbose"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].bval == 1);
    CHECK(ctx.npositionals == 2);
    CHECK(strcmp(ctx.positionals[0], "file1.txt") == 0);
    CHECK(strcmp(ctx.positionals[1], "file2.txt") == 0);
    args_free(opts, &ctx);
}

TEST(test_posix_order)
{
    MAKE_ARGV("file1.txt", "-v");
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", "verbose"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx);
    ctx.exit_on_error = 0;
    ctx.posix_order   = 1;   /* stop at first non-option */
    args_parse(_ac, _av, opts, &ctx);
    /* -v is NOT parsed as option; both args are positional */
    CHECK(opts[0].bval == 0);
    CHECK(ctx.npositionals == 2);
    args_free(opts, &ctx);
}

TEST(test_default_string)
{
    MAKE_ARGV0();   /* no args */
    args_opt_t opts[] = {
        ARGS_STRING('o', "output", "FILE", "output file", "out.txt"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count == 0);
    CHECK(strcmp(opts[0].value, "out.txt") == 0);
    args_free(opts, &ctx);
}

TEST(test_default_int)
{
    MAKE_ARGV0();
    args_opt_t opts[] = {
        ARGS_INT('n', "count", "N", "repeat N times", "42"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].ival == 42);
    args_free(opts, &ctx);
}

TEST(test_int_parsing)
{
    MAKE_ARGV("--count=255", "--hex=0xff", "--octal=0777");
    args_opt_t opts[] = {
        ARGS_INT('n', "count",  "N", "decimal",  NULL),
        ARGS_INT( 0,  "hex",    "N", "hex",      NULL),
        ARGS_INT( 0,  "octal",  "N", "octal",    NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].ival == 255);
    CHECK(opts[1].ival == 255);
    CHECK(opts[2].ival == 511);
    args_free(opts, &ctx);
}

TEST(test_float_parsing)
{
    MAKE_ARGV("--ratio=3.14");
    args_opt_t opts[] = {
        ARGS_FLOAT('r', "ratio", "F", "a ratio", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    double diff = opts[0].fval - 3.14;
    CHECK(diff > -0.0001 && diff < 0.0001);
    args_free(opts, &ctx);
}

TEST(test_multi_value)
{
    MAKE_ARGV("-I", "inc1", "-I", "inc2", "-I", "inc3");
    args_opt_t opts[] = {
        ARGS_STRING('I', "include", "DIR", "add include dir", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);
    CHECK(opts[0].count   == 3);
    CHECK(opts[0].nvalues == 3);
    CHECK(strcmp(opts[0].values[0], "inc1") == 0);
    CHECK(strcmp(opts[0].values[1], "inc2") == 0);
    CHECK(strcmp(opts[0].values[2], "inc3") == 0);
    args_free(opts, &ctx);
}

TEST(test_allow_unknown)
{
    MAKE_ARGV("--known", "--unknown-flag", "-Z");
    args_opt_t opts[] = {
        ARGS_BOOL(0, "known", "a known flag"),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx);
    ctx.exit_on_error = 0;
    ctx.allow_unknown = 1;
    args_err_t rc = args_parse(_ac, _av, opts, &ctx);
    CHECK(rc == ARGS_OK);
    CHECK(opts[0].count == 1);
    CHECK(ctx.nunknown  == 2);
    args_free(opts, &ctx);
}

TEST(test_no_args)
{
    MAKE_ARGV0();
    args_opt_t opts[] = { ARGS_OPT_END };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_err_t rc = args_parse(_ac, _av, opts, &ctx);
    CHECK(rc == ARGS_OK);
    CHECK(ctx.npositionals == 0);
    args_free(opts, &ctx);
}

TEST(test_find_short)
{
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", ""),
        ARGS_STRING('f', "file", "F", "", NULL),
        ARGS_OPT_END
    };
    CHECK(args_find_short(opts, 'v') == &opts[0]);
    CHECK(args_find_short(opts, 'f') == &opts[1]);
    CHECK(args_find_short(opts, 'z') == NULL);
}

TEST(test_find_long)
{
    args_opt_t opts[] = {
        ARGS_BOOL('v', "verbose", ""),
        ARGS_STRING('f', "file", "F", "", NULL),
        ARGS_OPT_END
    };
    CHECK(args_find_long(opts, "verbose") == &opts[0]);
    CHECK(args_find_long(opts, "file")    == &opts[1]);
    CHECK(args_find_long(opts, "nope")    == NULL);
}

TEST(test_error_unknown_opt)
{
    MAKE_ARGV("--no-such-option");
    args_opt_t opts[] = { ARGS_OPT_END };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_err_t rc = args_parse(_ac, _av, opts, &ctx);
    CHECK(rc == ARGS_ERR_UNKNOWN_OPT);
    args_free(opts, &ctx);
}

TEST(test_error_missing_value)
{
    MAKE_ARGV("--file");   /* --file requires a value */
    args_opt_t opts[] = {
        ARGS_STRING('f', "file", "FILE", "file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_err_t rc = args_parse(_ac, _av, opts, &ctx);
    CHECK(rc == ARGS_ERR_MISSING_VAL);
    args_free(opts, &ctx);
}

TEST(test_error_bad_int)
{
    MAKE_ARGV("--count=abc");
    args_opt_t opts[] = {
        ARGS_INT('n', "count", "N", "count", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_err_t rc = args_parse(_ac, _av, opts, &ctx);
    CHECK(rc == ARGS_ERR_BAD_INT);
    args_free(opts, &ctx);
}

TEST(test_complex_combined)
{
    /*  tar-style: -xvf archive.tar  */
    MAKE_ARGV("-xvf", "archive.tar", "--", "--not-an-option", "file.txt");
    args_opt_t opts[] = {
        ARGS_BOOL  ('x', "extract", "extract files"),
        ARGS_BOOL  ('v', "verbose", "verbose"),
        ARGS_STRING('f', "file",   "FILE", "archive file", NULL),
        ARGS_OPT_END
    };
    args_ctx_t ctx; args_ctx_init(&ctx); ctx.exit_on_error = 0;
    args_parse(_ac, _av, opts, &ctx);

    CHECK(opts[0].bval  == 1);           /* -x */
    CHECK(opts[1].bval  == 1);           /* -v */
    CHECK(opts[2].count == 1);           /* -f */
    CHECK(strcmp(opts[2].value, "archive.tar") == 0);
    CHECK(ctx.npositionals == 2);
    CHECK(strcmp(ctx.positionals[0], "--not-an-option") == 0);
    CHECK(strcmp(ctx.positionals[1], "file.txt") == 0);
    args_free(opts, &ctx);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    test_short_flag();
    test_short_flag_repeated();
    test_short_combined();
    test_short_value_space();
    test_short_value_attached();
    test_long_flag();
    test_long_value_equals();
    test_long_value_space();
    test_long_no_negation();
    test_dashdash_end_of_opts();
    test_stdin_dash_positional();
    test_positionals_mixed();
    test_posix_order();
    test_default_string();
    test_default_int();
    test_int_parsing();
    test_float_parsing();
    test_multi_value();
    test_allow_unknown();
    test_no_args();
    test_find_short();
    test_find_long();
    test_error_unknown_opt();
    test_error_missing_value();
    test_error_bad_int();
    test_complex_combined();

    int total = g_pass + g_fail;
    printf("\n%d/%d tests passed", g_pass, total);
    if (g_fail)
        printf("  (%d FAILED)", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
