/*
 * args_module.c — CPython C extension for libargs
 *
 * Exposes a single class:  args.Parser
 *
 *   import args
 *
 *   opts = [
 *       args.opt_bool  ('v', 'verbose', 'be verbose'),
 *       args.opt_string('f', 'file',   'FILE', 'output file', default='a.out'),
 *       args.opt_int   ('n', 'count',  'N',    'repeat N times'),
 *       args.opt_float ('r', 'ratio',  'F',    'a ratio', default=1.0),
 *   ]
 *
 *   p = args.Parser()
 *   r = p.parse(sys.argv, opts)         # returns a ParseResult object
 *
 *   r.flag('verbose')                   # bool
 *   r.string('file')                    # str | None
 *   r.int('count')                      # int | None
 *   r.float('ratio')                    # float | None
 *   r.count('verbose')                  # int (how many times seen)
 *   r.values('file')                    # list[str] (all occurrences)
 *   r.positionals()                     # list[str]
 *   r.dashdash_seen()                   # bool
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* Include libargs from two directories up */
#include "args.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* ParseResult object                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    PyObject_HEAD
    PyObject *flags;        /* dict[str, bool]        */
    PyObject *strings;      /* dict[str, str|None]    */
    PyObject *ints;         /* dict[str, int|None]    */
    PyObject *floats;       /* dict[str, float|None]  */
    PyObject *counts;       /* dict[str, int]         */
    PyObject *all_values;   /* dict[str, list[str]]   */
    PyObject *positionals;  /* list[str]              */
    int       dashdash;
} ParseResultObject;

static void ParseResult_dealloc(ParseResultObject *self)
{
    Py_XDECREF(self->flags);
    Py_XDECREF(self->strings);
    Py_XDECREF(self->ints);
    Py_XDECREF(self->floats);
    Py_XDECREF(self->counts);
    Py_XDECREF(self->all_values);
    Py_XDECREF(self->positionals);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *ParseResult_flag(ParseResultObject *self, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    PyObject *v = PyDict_GetItemString(self->flags, name);
    if (!v) { PyErr_Format(PyExc_KeyError, "unknown option '%s'", name); return NULL; }
    return PyBool_FromLong(PyObject_IsTrue(v));
}

static PyObject *ParseResult_string(ParseResultObject *self, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    PyObject *v = PyDict_GetItemString(self->strings, name);
    if (!v) { Py_RETURN_NONE; }
    Py_INCREF(v); return v;
}

static PyObject *ParseResult_int(ParseResultObject *self, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    PyObject *v = PyDict_GetItemString(self->ints, name);
    if (!v) { Py_RETURN_NONE; }
    Py_INCREF(v); return v;
}

static PyObject *ParseResult_float(ParseResultObject *self, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    PyObject *v = PyDict_GetItemString(self->floats, name);
    if (!v) { Py_RETURN_NONE; }
    Py_INCREF(v); return v;
}

static PyObject *ParseResult_count(ParseResultObject *self, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    PyObject *v = PyDict_GetItemString(self->counts, name);
    if (!v) return PyLong_FromLong(0);
    Py_INCREF(v); return v;
}

static PyObject *ParseResult_values(ParseResultObject *self, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
    PyObject *v = PyDict_GetItemString(self->all_values, name);
    if (!v) return PyList_New(0);
    Py_INCREF(v); return v;
}

static PyObject *ParseResult_positionals(ParseResultObject *self,
                                          PyObject *Py_UNUSED(args))
{
    Py_INCREF(self->positionals);
    return self->positionals;
}

static PyObject *ParseResult_dashdash_seen(ParseResultObject *self,
                                            PyObject *Py_UNUSED(args))
{
    return PyBool_FromLong(self->dashdash);
}

static PyMethodDef ParseResult_methods[] = {
    {"flag",         (PyCFunction)ParseResult_flag,         METH_VARARGS, "flag(name) -> bool"},
    {"string",       (PyCFunction)ParseResult_string,       METH_VARARGS, "string(name) -> str|None"},
    {"int",          (PyCFunction)ParseResult_int,          METH_VARARGS, "int(name) -> int|None"},
    {"float",        (PyCFunction)ParseResult_float,        METH_VARARGS, "float(name) -> float|None"},
    {"count",        (PyCFunction)ParseResult_count,        METH_VARARGS, "count(name) -> int"},
    {"values",       (PyCFunction)ParseResult_values,       METH_VARARGS, "values(name) -> list[str]"},
    {"positionals",  (PyCFunction)ParseResult_positionals,  METH_NOARGS,  "positionals() -> list[str]"},
    {"dashdash_seen",(PyCFunction)ParseResult_dashdash_seen,METH_NOARGS,  "dashdash_seen() -> bool"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject ParseResultType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "args.ParseResult",
    .tp_basicsize = sizeof(ParseResultObject),
    .tp_dealloc   = (destructor)ParseResult_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Result of a successful parse",
    .tp_methods   = ParseResult_methods,
};

/* ------------------------------------------------------------------ */
/* Helper: build key string for an opt_dict entry                      */
/* ------------------------------------------------------------------ */

static const char *opt_key(PyObject *opt_dict)
{
    PyObject *ln = PyDict_GetItemString(opt_dict, "long_name");
    if (ln && ln != Py_None) return PyUnicode_AsUTF8(ln);
    PyObject *sn = PyDict_GetItemString(opt_dict, "short_name");
    if (sn && sn != Py_None) return PyUnicode_AsUTF8(sn);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Parser.parse(argv, opts) -> ParseResult                             */
/* ------------------------------------------------------------------ */

typedef struct {
    PyObject_HEAD
    int exit_on_error;
    int allow_unknown;
    int posix_order;
} ParserObject;

static int Parser_init(ParserObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"exit_on_error","allow_unknown","posix_order",NULL};
    self->exit_on_error = 0;
    self->allow_unknown = 0;
    self->posix_order   = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ppp", kwlist,
            &self->exit_on_error, &self->allow_unknown, &self->posix_order))
        return -1;
    return 0;
}

static PyObject *Parser_parse(ParserObject *self, PyObject *args)
{
    PyObject *py_argv;   /* list[str] */
    PyObject *py_opts;   /* list[dict] */

    if (!PyArg_ParseTuple(args, "OO", &py_argv, &py_opts)) return NULL;

    if (!PyList_Check(py_argv) && !PyTuple_Check(py_argv)) {
        PyErr_SetString(PyExc_TypeError, "argv must be a list or tuple");
        return NULL;
    }
    if (!PyList_Check(py_opts) && !PyTuple_Check(py_opts)) {
        PyErr_SetString(PyExc_TypeError, "opts must be a list or tuple");
        return NULL;
    }

    Py_ssize_t argc = PySequence_Size(py_argv);
    Py_ssize_t nopt = PySequence_Size(py_opts);

    /* ---- build C argv ---- */
    char **c_argv = calloc(argc + 1, sizeof(char *));
    if (!c_argv) return PyErr_NoMemory();

    /* We keep borrowed refs from PyUnicode_AsUTF8; they are valid as long
       as py_argv elements are alive, which they are throughout this call. */
    for (Py_ssize_t i = 0; i < argc; i++) {
        PyObject *item = PySequence_GetItem(py_argv, i);
        if (!item || !PyUnicode_Check(item)) {
            Py_XDECREF(item);
            free(c_argv);
            PyErr_SetString(PyExc_TypeError, "argv elements must be str");
            return NULL;
        }
        c_argv[i] = (char *)PyUnicode_AsUTF8(item);
        Py_DECREF(item);
        if (!c_argv[i]) { free(c_argv); return NULL; }
    }

    /* ---- build C opt table ---- */
    /* We store CStrings in parallel arrays so they stay alive. */
    args_opt_t *c_opts = calloc(nopt + 1, sizeof(args_opt_t));
    if (!c_opts) { free(c_argv); return PyErr_NoMemory(); }

    /* Storage for strings extracted from Python dicts.
       PyUnicode_AsUTF8 returns a pointer into the PyObject internal buffer,
       valid as long as the object is alive.  py_opts holds refs so we're safe. */

    for (Py_ssize_t i = 0; i < nopt; i++) {
        PyObject *d = PySequence_GetItem(py_opts, i);
        if (!d || !PyDict_Check(d)) {
            Py_XDECREF(d);
            free(c_opts); free(c_argv);
            PyErr_SetString(PyExc_TypeError, "opts elements must be dicts (use opt_bool/opt_string/...)");
            return NULL;
        }

        /* short_name */
        PyObject *sn = PyDict_GetItemString(d, "short_name");
        if (sn && sn != Py_None && PyUnicode_Check(sn)) {
            const char *s = PyUnicode_AsUTF8(sn);
            c_opts[i].short_name = s ? (unsigned char)s[0] : 0;
        }

        /* long_name */
        PyObject *ln = PyDict_GetItemString(d, "long_name");
        if (ln && ln != Py_None)
            c_opts[i].long_name = PyUnicode_AsUTF8(ln);

        /* type */
        PyObject *tp = PyDict_GetItemString(d, "type");
        if (tp) {
            long t = PyLong_AsLong(tp);
            c_opts[i].type = (args_type_t)t;
        }

        /* metavar */
        PyObject *mv = PyDict_GetItemString(d, "metavar");
        if (mv && mv != Py_None)
            c_opts[i].metavar = PyUnicode_AsUTF8(mv);

        /* help */
        PyObject *hp = PyDict_GetItemString(d, "help");
        if (hp && hp != Py_None)
            c_opts[i].help = PyUnicode_AsUTF8(hp);

        /* default_value */
        PyObject *dv = PyDict_GetItemString(d, "default");
        if (dv && dv != Py_None)
            c_opts[i].default_value = PyUnicode_AsUTF8(dv);

        Py_DECREF(d);
    }
    /* sentinel already zero-initialised by calloc */

    /* ---- build context ---- */
    args_ctx_t ctx;
    args_ctx_init(&ctx);
    ctx.exit_on_error = self->exit_on_error;
    ctx.allow_unknown = self->allow_unknown;
    ctx.posix_order   = self->posix_order;

    /* ---- call parser ---- */
    int rc = args_parse((int)argc, c_argv, c_opts, &ctx);

    free(c_argv);

    if (rc != 0) {
        args_free(c_opts, &ctx);
        free(c_opts);
        const char *msg;
        switch (rc) {
            case -1: msg = "unknown option";       break;
            case -2: msg = "missing value";        break;
            case -3: msg = "invalid integer";      break;
            case -4: msg = "invalid float";        break;
            default: msg = "parse error";          break;
        }
        PyErr_SetString(PyExc_ValueError, msg);
        return NULL;
    }

    /* ---- build ParseResult ---- */
    ParseResultObject *result =
        (ParseResultObject *)ParseResultType.tp_alloc(&ParseResultType, 0);
    if (!result) { args_free(c_opts, &ctx); free(c_opts); return NULL; }

    result->flags      = PyDict_New();
    result->strings    = PyDict_New();
    result->ints       = PyDict_New();
    result->floats     = PyDict_New();
    result->counts     = PyDict_New();
    result->all_values = PyDict_New();
    result->positionals= PyList_New(0);
    result->dashdash   = ctx.dashdash_seen;

    for (Py_ssize_t i = 0; i < nopt; i++) {
        args_opt_t *co = &c_opts[i];
        PyObject *d    = PySequence_GetItem(py_opts, i);
        const char *key = opt_key(d);
        Py_XDECREF(d);
        if (!key) continue;

        PyDict_SetItemString(result->counts, key, PyLong_FromLong(co->count));

        switch (co->type) {
        case ARGS_OPT_BOOL:
            PyDict_SetItemString(result->flags, key,
                                 co->bval ? Py_True : Py_False);
            break;
        case ARGS_OPT_STRING: {
            PyObject *v = co->value ? PyUnicode_FromString(co->value) : Py_None;
            if (!co->value) Py_INCREF(Py_None);
            PyDict_SetItemString(result->strings, key, v);
            Py_DECREF(v);
            /* multi-values */
            PyObject *lst = PyList_New(0);
            for (size_t j = 0; j < co->nvalues; j++) {
                PyObject *s = PyUnicode_FromString(co->values[j]);
                PyList_Append(lst, s);
                Py_DECREF(s);
            }
            PyDict_SetItemString(result->all_values, key, lst);
            Py_DECREF(lst);
            break;
        }
        case ARGS_OPT_INT: {
            PyObject *v = co->value ? PyLong_FromLong(co->ival) : Py_None;
            if (!co->value) Py_INCREF(Py_None);
            PyDict_SetItemString(result->ints, key, v);
            Py_DECREF(v);
            break;
        }
        case ARGS_OPT_FLOAT: {
            PyObject *v = co->value ? PyFloat_FromDouble(co->fval) : Py_None;
            if (!co->value) Py_INCREF(Py_None);
            PyDict_SetItemString(result->floats, key, v);
            Py_DECREF(v);
            break;
        }
        }
    }

    for (int i = 0; i < ctx.npositionals; i++) {
        PyObject *s = PyUnicode_FromString(ctx.positionals[i]);
        PyList_Append(result->positionals, s);
        Py_DECREF(s);
    }

    args_free(c_opts, &ctx);
    free(c_opts);
    return (PyObject *)result;
}

static PyMethodDef Parser_methods[] = {
    {"parse", (PyCFunction)Parser_parse, METH_VARARGS,
     "parse(argv, opts) -> ParseResult"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject ParserType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "args.Parser",
    .tp_basicsize = sizeof(ParserObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Unix/GNU argument parser",
    .tp_methods   = Parser_methods,
    .tp_init      = (initproc)Parser_init,
    .tp_new       = PyType_GenericNew,
};

/* ------------------------------------------------------------------ */
/* Module-level helper functions: opt_bool, opt_string, opt_int, opt_float */
/* ------------------------------------------------------------------ */

static PyObject *make_opt(int type, PyObject *args, PyObject *kwds,
                          int want_meta)
{
    const char *short_name = NULL;
    const char *long_name  = NULL;
    const char *metavar    = NULL;
    const char *help       = NULL;
    PyObject   *def        = Py_None;

    if (want_meta) {
        static char *kw[] = {"short","long","metavar","help","default",NULL};
        if (!PyArg_ParseTupleAndKeywords(args, kwds, "zzzz|O", kw,
                &short_name, &long_name, &metavar, &help, &def))
            return NULL;
    } else {
        static char *kw[] = {"short","long","help","default",NULL};
        if (!PyArg_ParseTupleAndKeywords(args, kwds, "zzz|O", kw,
                &short_name, &long_name, &help, &def))
            return NULL;
    }

    PyObject *d = PyDict_New();
    PyDict_SetItemString(d, "type",       PyLong_FromLong(type));
    PyDict_SetItemString(d, "short_name", short_name ? PyUnicode_FromString(short_name) : Py_None);
    PyDict_SetItemString(d, "long_name",  long_name  ? PyUnicode_FromString(long_name)  : Py_None);
    PyDict_SetItemString(d, "metavar",    metavar    ? PyUnicode_FromString(metavar)    : Py_None);
    PyDict_SetItemString(d, "help",       help       ? PyUnicode_FromString(help)       : Py_None);
    PyDict_SetItemString(d, "default",    def);
    return d;
}

static PyObject *py_opt_bool  (PyObject *m, PyObject *a, PyObject *k) { (void)m; return make_opt(ARGS_OPT_BOOL,   a, k, 0); }
static PyObject *py_opt_string(PyObject *m, PyObject *a, PyObject *k) { (void)m; return make_opt(ARGS_OPT_STRING, a, k, 1); }
static PyObject *py_opt_int   (PyObject *m, PyObject *a, PyObject *k) { (void)m; return make_opt(ARGS_OPT_INT,    a, k, 1); }
static PyObject *py_opt_float (PyObject *m, PyObject *a, PyObject *k) { (void)m; return make_opt(ARGS_OPT_FLOAT,  a, k, 1); }

static PyMethodDef module_methods[] = {
    {"opt_bool",   (PyCFunction)py_opt_bool,   METH_VARARGS|METH_KEYWORDS, "Create a boolean option descriptor"},
    {"opt_string", (PyCFunction)py_opt_string, METH_VARARGS|METH_KEYWORDS, "Create a string option descriptor"},
    {"opt_int",    (PyCFunction)py_opt_int,    METH_VARARGS|METH_KEYWORDS, "Create an integer option descriptor"},
    {"opt_float",  (PyCFunction)py_opt_float,  METH_VARARGS|METH_KEYWORDS, "Create a float option descriptor"},
    {NULL, NULL, 0, NULL}
};

/* ------------------------------------------------------------------ */
/* Module init                                                          */
/* ------------------------------------------------------------------ */

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "args",
    "Unix/GNU argument parsing library — Python bindings",
    -1,
    module_methods,
};

PyMODINIT_FUNC PyInit_args(void)
{
    if (PyType_Ready(&ParseResultType) < 0) return NULL;
    if (PyType_Ready(&ParserType)      < 0) return NULL;

    PyObject *m = PyModule_Create(&module_def);
    if (!m) return NULL;

    Py_INCREF(&ParseResultType);
    Py_INCREF(&ParserType);
    PyModule_AddObject(m, "ParseResult", (PyObject *)&ParseResultType);
    PyModule_AddObject(m, "Parser",      (PyObject *)&ParserType);
    return m;
}
