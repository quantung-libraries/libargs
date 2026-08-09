/*
 * args_help.c — help / usage text generation
 *
 * Output format follows GNU conventions:
 *
 *   Usage: prog [OPTION]... FILE...
 *
 *   Short description paragraph.
 *
 *   Mandatory arguments to long options are mandatory for short options too.
 *
 *     -v, --verbose        be verbose
 *     -o, --output=FILE    write output to FILE (default: out.txt)
 *         --no-color       disable colour output
 *     -n, --count=N        repeat N times (default: 1)
 *
 *   Epilog paragraph.
 */

#include "args.h"

#include <stdio.h>
#include <string.h>

/* Width of the option column before the help text starts */
#define COL_WIDTH 28

/* ------------------------------------------------------------------ */
/* Internal: write one option line                                      */
/* ------------------------------------------------------------------ */

static void print_opt_line(FILE *fp, const args_opt_t *o)
{
    /* Build option syntax into a buffer, then pad to COL_WIDTH */
    char buf[128];
    int pos = 0;

    /* Leading spaces */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "  ");

    /* Short option part */
    if (o->short_name) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "-%c", o->short_name);
        if (o->long_name)
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
        else if (o->type != ARGS_OPT_BOOL && o->metavar)
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                            " <%s>", o->metavar);
    } else {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "    ");
    }

    /* Long option part */
    if (o->long_name) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "--%s", o->long_name);
        if (o->type != ARGS_OPT_BOOL) {
            const char *mv = o->metavar ? o->metavar : "ARG";
            pos += snprintf(buf + pos, sizeof(buf) - pos, "=<%s>", mv);
        }
    }

    /* Pad to column */
    fprintf(fp, "%s", buf);
    int pad = COL_WIDTH - pos;
    if (pad <= 0) {
        /* Help text on next line, indented */
        fprintf(fp, "\n%*s", COL_WIDTH, "");
    } else {
        fprintf(fp, "%*s", pad, "");
    }

    /* Help text */
    if (o->help)
        fprintf(fp, "%s", o->help);

    /* Default value */
    if (o->default_value)
        fprintf(fp, " (default: %s)", o->default_value);

    /* Negation note for booleans */
    if (o->long_name && o->type == ARGS_OPT_BOOL)
        fprintf(fp, "  [--no-%s to disable]", o->long_name);

    fprintf(fp, "\n");
}

/* ------------------------------------------------------------------ */
/* Public: args_print_usage                                            */
/* ------------------------------------------------------------------ */

void args_print_usage(FILE *fp, const args_ctx_t *ctx,
                      const args_opt_t *opts __attribute__((unused)))
{
    const char *prog = ctx && ctx->prog ? ctx->prog : "program";

    if (ctx && ctx->usage_line) {
        fprintf(fp, "Usage: %s\n", ctx->usage_line);
        return;
    }

    fprintf(fp, "Usage: %s [OPTION]...", prog);

    /* If the user set a usage_line use it; otherwise we just show OPTION */
    fprintf(fp, "\n");
}

/* ------------------------------------------------------------------ */
/* Public: args_print_help                                             */
/* ------------------------------------------------------------------ */

void args_print_help(FILE *fp, const args_ctx_t *ctx,
                     const args_opt_t *opts)
{
    args_print_usage(fp, ctx, opts);

    if (ctx && ctx->description) {
        fprintf(fp, "\n%s\n", ctx->description);
    }

    /* Check whether we have both long and short options mixed */
    int has_both = 0;
    for (const args_opt_t *o = opts; o->short_name || o->long_name; o++) {
        if (o->short_name && o->long_name) { has_both = 1; break; }
    }
    if (has_both) {
        fprintf(fp,
            "\nMandatory arguments to long options are mandatory"
            " for short options too.\n");
    }

    fprintf(fp, "\n");

    for (const args_opt_t *o = opts; o->short_name || o->long_name; o++)
        print_opt_line(fp, o);

    if (ctx && ctx->epilog)
        fprintf(fp, "\n%s\n", ctx->epilog);
}
