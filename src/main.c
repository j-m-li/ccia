/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

Config g_config;

static void print_usage(const char *progname) {
    printf("Usage: %s [options] <input-file.c>\n\n", progname);
    printf("Options:\n");
    printf("  -o <file>       Place the output into <file>\n");
#ifdef TARGET_I386
    printf("  -S              Compile only; generate x86 (i386 32-bit) assembly (.s)\n");
#else
    printf("  -S              Compile only; generate x86_64 assembly (.s)\n");
#endif
    printf("  -c              Compile and assemble, but do not link (.o)\n");
    printf("  -E              Preprocess only; do not compile\n");
    printf("  -I <dir>        Add directory to include search path\n");
    printf("  -D <macro>[=v]  Define macro with optional value\n");
    printf("  -v, --version   Display compiler version and public domain notice\n");
    printf("  -h, --help      Display this information\n\n");
#ifdef TARGET_I386
    printf("This is a C90 (ANSI C89) compiler targeting x86 32-bit (i386) Linux.\n");
#else
    printf("This is a C90 (ANSI C89) compiler targeting x86_64 Linux.\n");
#endif
    printf("Released into the Public Domain under the Unlicense.\n");
}

static void print_version(void) {
#ifdef TARGET_I386
    printf("ccia-i386 version 1.0.0 (i386-linux)\n");
    printf("A Public Domain C90 / ANSI C89 Compiler targeting 32-bit x86 (i386).\n");
#else
    printf("ccia version 1.0.0 (x86_64-linux)\n");
    printf("A Public Domain C90 / ANSI C89 Compiler written in C90.\n");
#endif
    printf("This software is dedicated to the public domain (Unlicense).\n");
}

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    long size;
    char *buf;
    size_t read_bytes;

    if (!fp) {
        fprintf(stderr, "ccia: fatal error: cannot open input file '%s'\n", path);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (char *)c90_malloc(size + 1);
    read_bytes = fread(buf, 1, size, fp);
    buf[read_bytes] = '\0';
    fclose(fp);
    return buf;
}

static char *replace_extension(const char *filename, const char *new_ext) {
    char buf[1024];
    char *dot;
    strcpy(buf, filename);
    dot = strrchr(buf, '.');
    if (dot) {
        strcpy(dot, new_ext);
    } else {
        strcat(buf, new_ext);
    }
    return c90_strdup(buf);
}

static char *get_softfloat_obj(const char *progname) {
    static char path[2048];
    char dir[1024];
    char *slash;
    FILE *fp;

#ifdef TARGET_I386
    const char *sf_name = "src/softfloat.i386.o";
    const char *sf_base = "softfloat.i386.o";
#else
    const char *sf_name = "src/softfloat.o";
    const char *sf_base = "softfloat.o";
#endif

    /* Check local src directory first */
    fp = fopen(sf_name, "rb");
    if (fp) { fclose(fp); return (char *)sf_name; }

    /* Check directory of executable */
    if (strlen(progname) < 1000) {
        strcpy(dir, progname);
        slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            sprintf(path, "%s/%s", dir, sf_base);
            fp = fopen(path, "rb");
            if (fp) { fclose(fp); return path; }

            sprintf(path, "%s/src/%s", dir, sf_base);
            fp = fopen(path, "rb");
            if (fp) { fclose(fp); return path; }

            sprintf(path, "%s/%s", dir, sf_name);
            fp = fopen(path, "rb");
            if (fp) { fclose(fp); return path; }
        }
    }

    return (char *)sf_name;
}

int main(int argc, char **argv) {
    int i;
    char *source;
    Preprocessor *cpp;
    Token *toks;
    Parser *parser;
    AstNode *ast;
    FILE *asm_out;
    char *asm_file = NULL;
    int need_cleanup_asm = 0;

    g_config.input_file = NULL;
    g_config.output_file = NULL;
    g_config.dump_tokens = 0;
    g_config.dump_ast = 0;
    g_config.preprocess_only = 0;
    g_config.compile_only = 0;
    g_config.assemble_only = 0;
    g_config.verbose = 0;
    g_config.include_paths = vec_new();
    g_config.defines = vec_new();

    type_init();

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-S") == 0) {
            g_config.compile_only = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            g_config.assemble_only = 1;
        } else if (strcmp(argv[i], "-E") == 0) {
            g_config.preprocess_only = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ccia: error: missing argument to '-o'\n");
                return 1;
            }
            g_config.output_file = argv[++i];
        } else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ccia: error: missing argument to '-I'\n");
                return 1;
            }
            vec_push(g_config.include_paths, argv[++i]);
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            vec_push(g_config.include_paths, argv[i] + 2);
        } else if (strcmp(argv[i], "-D") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ccia: error: missing argument to '-D'\n");
                return 1;
            }
            vec_push(g_config.defines, argv[++i]);
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            vec_push(g_config.defines, argv[i] + 2);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "ccia: warning: unrecognized command-line option '%s'\n", argv[i]);
        } else {
            if (g_config.input_file) {
                fprintf(stderr, "ccia: error: multiple input files are not supported\n");
                return 1;
            }
            g_config.input_file = argv[i];
        }
    }

    if (!g_config.input_file) {
        fprintf(stderr, "ccia: fatal error: no input files\n");
        return 1;
    }

    /* Set up preprocessor */
    cpp = cpp_new();
    for (i = 0; i < g_config.include_paths->size; i++) {
        cpp_add_include_path(cpp, (char *)vec_get(g_config.include_paths, i));
    }
    for (i = 0; i < g_config.defines->size; i++) {
        char *def = (char *)vec_get(g_config.defines, i);
        char *eq = strchr(def, '=');
        if (eq) {
            char name[256];
            int nlen = (int)(eq - def);
            strncpy(name, def, nlen);
            name[nlen] = '\0';
            cpp_define_macro(cpp, name, eq + 1);
        } else {
            cpp_define_macro(cpp, def, "1");
        }
    }

    source = read_file(g_config.input_file);
    toks = cpp_process(cpp, source, g_config.input_file);

    /* Handle -E (preprocess only) */
    if (g_config.preprocess_only) {
        FILE *out = stdout;
        Token *t = toks;
        int last_line = 1;
        if (g_config.output_file) {
            out = fopen(g_config.output_file, "w");
            if (!out) {
                fprintf(stderr, "ccia: cannot open output file '%s'\n", g_config.output_file);
                return 1;
            }
        }
        while (t && t->kind != TOK_EOF) {
            while (last_line < t->line) {
                fprintf(out, "\n");
                last_line++;
            }
            if (t->str) fprintf(out, "%s ", t->str);
            else fprintf(out, "%s ", token_kind_str(t->kind));
            t = t->next;
        }
        fprintf(out, "\n");
        if (out != stdout) fclose(out);
        return 0;
    }

    /* Parse AST */
    parser = parser_new(toks);
    ast = parser_parse(parser);

    /* Determine assembly output filename */
    if (g_config.compile_only) {
        if (g_config.output_file) {
            asm_file = c90_strdup(g_config.output_file);
        } else {
            asm_file = replace_extension(g_config.input_file, ".s");
        }
    } else {
        asm_file = replace_extension(g_config.input_file, ".tmp.s");
        need_cleanup_asm = 1;
    }

    asm_out = fopen(asm_file, "w");
    if (!asm_out) {
        fprintf(stderr, "ccia: cannot open output assembly file '%s'\n", asm_file);
        return 1;
    }

    /* Emit code */
    {
        CodeGen *gen = codegen_new(asm_out, ast);
        codegen_generate(gen);
    }
    fclose(asm_out);

    if (g_config.compile_only) {
        return 0;
    }

    /* Handle assembling / linking */
    if (g_config.assemble_only) {
        char cmd[2048];
        char *obj_file;
        if (g_config.output_file) {
            obj_file = g_config.output_file;
        } else {
            obj_file = replace_extension(g_config.input_file, ".o");
        }
#ifdef TARGET_I386
        sprintf(cmd, "as --32 -o %s %s", obj_file, asm_file);
#else
        sprintf(cmd, "as -o %s %s", obj_file, asm_file);
#endif
        if (system(cmd) != 0) {
            fprintf(stderr, "ccia: assembler failed\n");
            if (need_cleanup_asm) remove(asm_file);
            return 1;
        }
        if (need_cleanup_asm) remove(asm_file);
        return 0;
    }

    /* Full executable compilation and linking */
    {
        char cmd[4096];
        char *exe_file = g_config.output_file ? g_config.output_file : "a.out";
        char *sf_obj = get_softfloat_obj(argv[0]);
#ifdef TARGET_I386
        sprintf(cmd, "gcc -m32 -no-pie -o %s %s %s -lm", exe_file, asm_file, sf_obj);
        if (system(cmd) != 0) {
            sprintf(cmd, "gcc -m32 -o %s %s %s -lm", exe_file, asm_file, sf_obj);
            if (system(cmd) != 0) {
                fprintf(stderr, "ccia: linking failed\n");
                if (need_cleanup_asm) remove(asm_file);
                return 1;
            }
        }
#else
        sprintf(cmd, "gcc -no-pie -o %s %s %s -lm", exe_file, asm_file, sf_obj);
        if (system(cmd) != 0) {
            /* Try without -no-pie */
            sprintf(cmd, "gcc -o %s %s %s -lm", exe_file, asm_file, sf_obj);
            if (system(cmd) != 0) {
                fprintf(stderr, "ccia: linking failed\n");
                if (need_cleanup_asm) remove(asm_file);
                return 1;
            }
        }
#endif
        if (need_cleanup_asm) remove(asm_file);
    }

    return 0;
}
