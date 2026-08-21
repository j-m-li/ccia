/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef C90_H
#define C90_H

#if defined(TARGET_RISCV32) || defined(__CCIA_RISCV32__) || defined(__riscv) || defined(__riscv__)
#ifndef TARGET_RISCV32
#define TARGET_RISCV32 1
#endif
#endif

#if defined(TARGET_I386) || defined(__CCIA_I386__) || defined(__CC90_I386__) || defined(__i386__) || defined(__i386)
#ifndef TARGET_I386
#define TARGET_I386 1
#endif
#endif

#if defined(TARGET_I386) || defined(TARGET_RISCV32)
#ifndef TARGET_32BIT
#define TARGET_32BIT 1
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "softfloat.h"

/* ========================================================================= */
/* Dynamic Array / Vector & Hash Map & String Buffer Data Structures        */
/* ========================================================================= */

typedef struct Vector {
    void **data;
    int size;
    int capacity;
} Vector;

Vector *vec_new(void);
void vec_push(Vector *v, void *elem);
void *vec_pop(Vector *v);
void *vec_get(Vector *v, int index);
void vec_set(Vector *v, int index, void *elem);
void vec_free(Vector *v);

typedef struct MapEntry {
    char *key;
    void *val;
    struct MapEntry *next;
} MapEntry;

typedef struct Map {
    MapEntry **buckets;
    int bucket_count;
    int size;
} Map;

Map *map_new(void);
void map_put(Map *m, const char *key, void *val);
void *map_get(Map *m, const char *key);
int map_has(Map *m, const char *key);
void map_free(Map *m);

typedef struct StrBuf {
    char *data;
    int length;
    int capacity;
} StrBuf;

StrBuf *strbuf_new(void);
void strbuf_append_char(StrBuf *sb, char c);
void strbuf_append_str(StrBuf *sb, const char *s);
void strbuf_append_buf(StrBuf *sb, const char *s, int len);
char *strbuf_to_string(StrBuf *sb);
void strbuf_free(StrBuf *sb);

/* Utility string and memory functions */
char *c90_strdup(const char *s);
char *c90_strndup(const char *s, int n);
void *c90_malloc(size_t size);
void *c90_calloc(size_t count, size_t size);
void *c90_realloc(void *ptr, size_t size);
void c90_error(const char *filename, int line, const char *fmt, ...);
void c90_warn(const char *filename, int line, const char *fmt, ...);

/* ========================================================================= */
/* Tokens and Lexer                                                         */
/* ========================================================================= */

typedef enum TokenKind {
    TOK_EOF = 0,
    
    /* Literals */
    TOK_INT_LIT = 256,
    TOK_FLOAT_LIT,
    TOK_CHAR_LIT,
    TOK_STR_LIT,
    TOK_IDENT,

    /* C90 Keywords */
    TOK_AUTO,
    TOK_BREAK,
    TOK_CASE,
    TOK_CHAR,
    TOK_CONST,
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DO,
    TOK_DOUBLE,
    TOK_ELSE,
    TOK_ENUM,
    TOK_EXTERN,
    TOK_FLOAT,
    TOK_FOR,
    TOK_GOTO,
    TOK_IF,
    TOK_INT,
    TOK_LONG,
    TOK_REGISTER,
    TOK_RETURN,
    TOK_SHORT,
    TOK_SIGNED,
    TOK_SIZEOF,
    TOK_STATIC,
    TOK_STRUCT,
    TOK_SWITCH,
    TOK_TYPEDEF,
    TOK_UNION,
    TOK_UNSIGNED,
    TOK_VOID,
    TOK_VOLATILE,
    TOK_WHILE,

    /* GNU C / C99 compatibility keywords */
    TOK_ATTRIBUTE,
    TOK_EXTENSION,
    TOK_INLINE,
    TOK_RESTRICT,
    TOK_ASM,
    TOK_BUILTIN_VA_LIST,
    TOK_COMPLEX,
    TOK_BOOL,
    TOK_THREAD,

    /* Multi-character operators and punctuators */
    TOK_ARROW,       /* -> */
    TOK_INC,         /* ++ */
    TOK_DEC,         /* -- */
    TOK_SHL,         /* << */
    TOK_SHR,         /* >> */
    TOK_LE,          /* <= */
    TOK_GE,          /* >= */
    TOK_EQ,          /* == */
    TOK_NE,          /* != */
    TOK_LAND,        /* && */
    TOK_LOR,         /* || */
    TOK_MUL_ASSIGN,  /* *= */
    TOK_DIV_ASSIGN,  /* /= */
    TOK_MOD_ASSIGN,  /* %= */
    TOK_ADD_ASSIGN,  /* += */
    TOK_SUB_ASSIGN,  /* -= */
    TOK_SHL_ASSIGN,  /* <<= */
    TOK_SHR_ASSIGN,  /* >>= */
    TOK_AND_ASSIGN,  /* &= */
    TOK_XOR_ASSIGN,  /* ^= */
    TOK_OR_ASSIGN,   /* |= */
    TOK_ELLIPSIS,    /* ... */
    TOK_HASH,        /* # */
    TOK_HASH_HASH    /* ## */
} TokenKind;

typedef struct Token {
    int kind;            /* TokenKind or ASCII character */
    char *str;           /* Token lexeme string */
    long int_val;        /* Integer value for integer literals */
    int is_unsigned;     /* For unsigned integer constants */
    const char *filename;
    int line;
    int col;
    Vector *hideset;
    struct Token *next;
} Token;

typedef struct Lexer {
    const char *source;
    const char *filename;
    int pos;
    int line;
    int col;
    int len;
} Lexer;

Lexer *lexer_new(const char *source, const char *filename);
Token *lexer_tokenize(Lexer *l);
const char *token_kind_str(int kind);

/* ========================================================================= */
/* Preprocessor                                                              */
/* ========================================================================= */

typedef struct Macro {
    char *name;
    int is_function_like;
    Vector *params;      /* vector of char* param names */
    Vector *body;        /* vector of Token* */
    int is_builtin;
} Macro;

typedef struct Preprocessor {
    Map *macros;
    Vector *include_paths;
    Vector *cond_stack;  /* condition stack for #if / #ifdef */
    const char *current_file;
    int current_line;
} Preprocessor;

Preprocessor *cpp_new(void);
void cpp_add_include_path(Preprocessor *cpp, const char *path);
void cpp_define_macro(Preprocessor *cpp, const char *name, const char *body);
void cpp_undef_macro(Preprocessor *cpp, const char *name);
Token *cpp_process(Preprocessor *cpp, const char *source, const char *filename);

/* ========================================================================= */
/* Types                                                                     */
/* ========================================================================= */

typedef enum TypeKind {
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LDOUBLE,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_FUNC
} TypeKind;

typedef struct Member {
    char *name;
    struct Type *type;
    int offset;
    int bit_offset;
    int bit_width;
    struct Member *next;
} Member;

typedef struct Param {
    char *name;
    struct Type *type;
} Param;

typedef struct Type {
    TypeKind kind;
    int size;
    int align;
    int is_unsigned;
    int is_const;
    int is_volatile;
    
    /* Derived types */
    struct Type *base;       /* PTR, ARRAY, or FUNC return type */
    int array_len;           /* ARRAY length, -1 for unsized array */
    
    /* Struct / Union */
    char *tag;               /* struct/union/enum tag name */
    Member *members;         /* linked list of fields */
    int is_complete;         /* 1 if struct/union body is defined */
    
    /* Function */
    Vector *params;          /* vector of Param* */
    int is_varargs;          /* 1 if function takes ... */
} Type;

/* Predefined types */
extern Type *type_void;
extern Type *type_char;
extern Type *type_uchar;
extern Type *type_short;
extern Type *type_ushort;
extern Type *type_int;
extern Type *type_uint;
extern Type *type_long;
extern Type *type_ulong;
extern Type *type_float;
extern Type *type_double;
extern Type *type_ldouble;

void type_init(void);
Type *type_new(TypeKind kind, int size, int align);
Type *type_pointer_to(Type *base);
Type *type_array_of(Type *base, int len);
Type *type_func_new(Type *ret_type, Vector *params, int is_varargs);
Type *type_struct_new(const char *tag, int is_union);
Type *type_enum_new(const char *tag);
Type *type_copy(Type *src);

int type_is_integer(Type *t);
int type_is_floating(Type *t);
int type_is_arithmetic(Type *t);
int type_is_scalar(Type *t);
int type_is_pointer(Type *t);
int type_is_void_ptr(Type *t);
Type *type_decay(Type *t);
int type_equal(Type *a, Type *b);
int type_is_compatible(Type *a, Type *b);
Type *type_max(Type *a, Type *b);
Member *type_find_member(Type *struct_type, const char *name);

/* ========================================================================= */
/* Symbols and Scope Management                                              */
/* ========================================================================= */

typedef enum StorageClass {
    STORAGE_AUTO,
    STORAGE_REGISTER,
    STORAGE_STATIC,
    STORAGE_EXTERN,
    STORAGE_TYPEDEF
} StorageClass;

typedef enum SymbolKind {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPEDEF,
    SYM_ENUM_CONST
} SymbolKind;

typedef struct Initializer {
    struct AstNode *expr;
    Vector *elements;     /* vector of struct Initializer* */
    int is_compound;
    Type *type;
    int offset;
} Initializer;

typedef struct Symbol {
    SymbolKind kind;
    char *name;
    Type *type;
    StorageClass storage;
    int is_global;
    int is_defined;
    int stack_offset;     /* For local variables, offset from RBP */
    char *asm_label;      /* For static variables / string literals */
    int enum_value;       /* For SYM_ENUM_CONST */
    Initializer *init;    /* For initialized globals/locals */
    int scope_level;
} Symbol;

typedef struct Scope {
    Map *vars;            /* Identifiers: variables, functions, typedefs, enum constants */
    Map *tags;            /* Struct, union, and enum tags */
    int level;
    struct Scope *parent;
} Scope;

Scope *scope_new(Scope *parent);
void scope_enter(void);
void scope_exit(void);
Symbol *symbol_new(SymbolKind kind, const char *name, Type *type);
Symbol *scope_lookup(const char *name);
Symbol *scope_lookup_current(const char *name);
Type *scope_lookup_tag(const char *tag);
Type *scope_lookup_tag_current(const char *tag);
void scope_add_symbol(Symbol *sym);
void scope_add_tag(const char *tag, Type *type);

/* ========================================================================= */
/* Abstract Syntax Tree (AST)                                               */
/* ========================================================================= */

typedef enum AstKind {
    /* Literals & Primary */
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_CHAR_LIT,
    AST_STR_LIT,
    AST_VAR,

    /* Binary Operations */
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,
    AST_SHL,
    AST_SHR,
    AST_BITAND,
    AST_BITOR,
    AST_BITXOR,
    AST_EQ,
    AST_NE,
    AST_LT,
    AST_LE,
    AST_GT,
    AST_GE,
    AST_ASSIGN,
    AST_COMMA,
    AST_LOGAND,
    AST_LOGOR,

    /* Compound assignments */
    AST_ADD_ASSIGN,
    AST_SUB_ASSIGN,
    AST_MUL_ASSIGN,
    AST_DIV_ASSIGN,
    AST_MOD_ASSIGN,
    AST_SHL_ASSIGN,
    AST_SHR_ASSIGN,
    AST_AND_ASSIGN,
    AST_XOR_ASSIGN,
    AST_OR_ASSIGN,

    /* Unary Operations */
    AST_POS,
    AST_NEG,
    AST_BITNOT,
    AST_LOGNOT,
    AST_ADDR,
    AST_DEREF,
    AST_CAST,
    AST_SIZEOF,
    AST_PRE_INC,
    AST_PRE_DEC,
    AST_POST_INC,
    AST_POST_DEC,

    /* Member Access & Calls */
    AST_MEMBER,       /* . or -> */
    AST_CALL,
    AST_VA_ARG,
    AST_COND,         /* ? : */

    /* Statements */
    AST_BLOCK,
    AST_IF,
    AST_WHILE,
    AST_DO_WHILE,
    AST_FOR,
    AST_SWITCH,
    AST_CASE,
    AST_DEFAULT,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_GOTO,
    AST_LABEL,
    AST_EXPR_STMT,
    AST_DECL_STMT,

    /* Top-level */
    AST_FUNC_DEF,
    AST_TRANSLATION_UNIT
} AstKind;

typedef struct AstNode {
    AstKind kind;
    Type *type;
    const char *filename;
    int line;

    union {
        /* AST_INT_LIT, AST_CHAR_LIT */
        struct {
            long val;
            int is_unsigned;
        } int_val;

        /* AST_FLOAT_LIT */
        struct {
            char *label;
            unsigned int u128_words[4];
        } float_val;

        /* AST_STR_LIT */
        struct {
            char *str;
            int len;
            char *label;
        } str_val;

        /* AST_VAR */
        Symbol *sym;

        /* Binary ops: lhs, rhs */
        struct {
            struct AstNode *lhs;
            struct AstNode *rhs;
        } binop;

        /* Unary ops: operand */
        struct {
            struct AstNode *operand;
        } unop;

        /* AST_CAST */
        struct {
            struct AstNode *operand;
            Type *target_type;
        } cast;

        /* AST_SIZEOF */
        struct {
            struct AstNode *operand;
            Type *type_arg;
        } sizeof_expr;

        /* AST_COND (ternary) */
        struct {
            struct AstNode *cond;
            struct AstNode *then_expr;
            struct AstNode *else_expr;
        } cond;

        /* AST_MEMBER (. or ->) */
        struct {
            struct AstNode *target;
            char *member_name;
            int is_arrow;
            Member *member;
        } member;

        /* AST_CALL */
        struct {
            struct AstNode *func;
            Vector *args;        /* vector of AstNode* */
        } call;

        /* AST_VA_ARG */
        struct {
            struct AstNode *ap;
        } va_arg;

        /* AST_BLOCK */
        struct {
            Vector *stmts;       /* vector of AstNode* */
        } block;

        /* AST_IF */
        struct {
            struct AstNode *cond;
            struct AstNode *then_stmt;
            struct AstNode *else_stmt;
        } if_stmt;

        /* AST_WHILE, AST_DO_WHILE */
        struct {
            struct AstNode *cond;
            struct AstNode *body;
            char *break_label;
            char *continue_label;
        } loop_stmt;

        /* AST_FOR */
        struct {
            struct AstNode *init;
            struct AstNode *cond;
            struct AstNode *step;
            struct AstNode *body;
            char *break_label;
            char *continue_label;
        } for_stmt;

        /* AST_SWITCH */
        struct {
            struct AstNode *cond;
            struct AstNode *body;
            Vector *cases;       /* vector of AstNode* (AST_CASE or AST_DEFAULT) */
            char *break_label;
            char *default_label;
        } switch_stmt;

        /* AST_CASE */
        struct {
            long val;
            struct AstNode *stmt;
            char *label;
        } case_stmt;

        /* AST_DEFAULT */
        struct {
            struct AstNode *stmt;
            char *label;
        } default_stmt;

        /* AST_RETURN */
        struct {
            struct AstNode *expr;
        } return_stmt;

        /* AST_GOTO, AST_LABEL */
        struct {
            char *name;
            struct AstNode *stmt;
        } label_stmt;

        /* AST_EXPR_STMT */
        struct {
            struct AstNode *expr;
        } expr_stmt;

        /* AST_DECL_STMT */
        struct {
            Symbol *sym;
            Initializer *init;
        } decl_stmt;

        /* AST_FUNC_DEF */
        struct {
            Symbol *sym;
            Vector *params;      /* vector of Symbol* */
            struct AstNode *body;
            int stack_size;
            Vector *locals;      /* vector of Symbol* */
        } func_def;

        /* AST_TRANSLATION_UNIT */
        struct {
            Vector *decls;       /* vector of AstNode* */
            Vector *globals;     /* vector of Symbol* */
            Vector *strings;     /* vector of AstNode* (AST_STR_LIT) */
            Vector *floats;      /* vector of AstNode* (AST_FLOAT_LIT) */
        } trans_unit;
    } u;
} AstNode;

AstNode *ast_new(AstKind kind, const char *filename, int line);
AstNode *ast_int_lit(long val, int is_unsigned, Type *type, const char *file, int line);
AstNode *ast_float_lit(Type *type, const char *file, int line);
AstNode *ast_str_lit(const char *str, int len, const char *file, int line);
AstNode *ast_char_lit(int c, const char *file, int line);
AstNode *ast_var(Symbol *sym, const char *file, int line);
AstNode *ast_binary(AstKind kind, AstNode *lhs, AstNode *rhs, const char *file, int line);
AstNode *ast_unary(AstKind kind, AstNode *operand, const char *file, int line);
AstNode *ast_cast(Type *target, AstNode *operand, const char *file, int line);
AstNode *ast_call(AstNode *func, Vector *args, const char *file, int line);
AstNode *ast_member(AstNode *target, const char *member, int is_arrow, const char *file, int line);
long eval_const_expr(AstNode *n);
int eval_const_float_expr(AstNode *n, unsigned int words[4], Type *type);

/* ========================================================================= */
/* Parser                                                                    */
/* ========================================================================= */

typedef struct Parser {
    Token *tokens;
    Token *current;
    Vector *globals;
    Vector *strings;
    Vector *floats;
    Symbol *current_func;
    Vector *current_func_locals;
    int current_stack_offset;
    int label_seq;
    Vector *switch_stack;
} Parser;

Parser *parser_new(Token *tokens);
AstNode *parser_parse(Parser *p);

/* ========================================================================= */
/* Code Generator (x86_64 System V ABI)                                      */
/* ========================================================================= */

typedef struct CodeGen {
    FILE *out;
    AstNode *root;
    int label_count;
    char *func_ret_label;
    Vector *break_stack;
    Vector *continue_stack;
    int scratch_base;
    int ldouble_slot;
    int current_stack_size;
    Symbol *current_func;
} CodeGen;

CodeGen *codegen_new(FILE *out, AstNode *root);
void codegen_generate(CodeGen *gen);

/* ========================================================================= */
/* Compiler Driver Configuration                                             */
/* ========================================================================= */

typedef struct Config {
    char *input_file;
    char *output_file;
    int dump_tokens;
    int dump_ast;
    int preprocess_only;  /* -E */
    int compile_only;     /* -S */
    int assemble_only;    /* -c */
    int verbose;
    Vector *include_paths;
    Vector *defines;
    Vector *link_objs;
} Config;

extern Config g_config;

#endif /* C90_H */
