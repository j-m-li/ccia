/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

/* ========================================================================= */
/* Keyword Table                                                             */
/* ========================================================================= */

typedef struct KeywordEntry {
    const char *name;
    int kind;
} KeywordEntry;

static KeywordEntry keywords[] = {
    {"auto", TOK_AUTO},
    {"break", TOK_BREAK},
    {"case", TOK_CASE},
    {"char", TOK_CHAR},
    {"const", TOK_CONST},
    {"continue", TOK_CONTINUE},
    {"default", TOK_DEFAULT},
    {"do", TOK_DO},
    {"double", TOK_DOUBLE},
    {"else", TOK_ELSE},
    {"enum", TOK_ENUM},
    {"extern", TOK_EXTERN},
    {"float", TOK_FLOAT},
    {"for", TOK_FOR},
    {"goto", TOK_GOTO},
    {"if", TOK_IF},
    {"int", TOK_INT},
    {"long", TOK_LONG},
    {"register", TOK_REGISTER},
    {"return", TOK_RETURN},
    {"short", TOK_SHORT},
    {"signed", TOK_SIGNED},
    {"sizeof", TOK_SIZEOF},
    {"static", TOK_STATIC},
    {"struct", TOK_STRUCT},
    {"switch", TOK_SWITCH},
    {"typedef", TOK_TYPEDEF},
    {"union", TOK_UNION},
    {"unsigned", TOK_UNSIGNED},
    {"void", TOK_VOID},
    {"volatile", TOK_VOLATILE},
    {"while", TOK_WHILE},
    {"__attribute__", TOK_ATTRIBUTE},
    {"__attribute", TOK_ATTRIBUTE},
    {"__extension__", TOK_EXTENSION},
    {"inline", TOK_INLINE},
    {"__inline", TOK_INLINE},
    {"__inline__", TOK_INLINE},
    {"restrict", TOK_RESTRICT},
    {"__restrict", TOK_RESTRICT},
    {"__restrict__", TOK_RESTRICT},
    {"asm", TOK_ASM},
    {"__asm", TOK_ASM},
    {"__asm__", TOK_ASM},
    {"__const", TOK_CONST},
    {"__const__", TOK_CONST},
    {"__signed", TOK_SIGNED},
    {"__signed__", TOK_SIGNED},
    {"__volatile", TOK_VOLATILE},
    {"__volatile__", TOK_VOLATILE},
    {"__builtin_va_list", TOK_BUILTIN_VA_LIST},
    {"__gnuc_va_list", TOK_BUILTIN_VA_LIST},
    {"_Complex", TOK_COMPLEX},
    {"__complex__", TOK_COMPLEX},
    {"__complex", TOK_COMPLEX},
    {"_Bool", TOK_BOOL},
    {"__float128", TOK_DOUBLE},
    {"_Float128", TOK_DOUBLE},
    {"_Float64", TOK_DOUBLE},
    {"_Float64x", TOK_DOUBLE},
    {"_Float32", TOK_FLOAT},
    {"_Float32x", TOK_FLOAT},
    {"__int128", TOK_LONG},
    {"__int128_t", TOK_LONG},
    {"__uint128_t", TOK_LONG},
    {"__thread", TOK_THREAD},
    {"_Thread_local", TOK_THREAD},
    {NULL, 0}
};

const char *token_kind_str(int kind) {
    if (kind >= 0 && kind < 256) {
        static char buf[4];
        buf[0] = (char)kind;
        buf[1] = '\0';
        return buf;
    }
    switch (kind) {
        case TOK_EOF: return "end of file";
        case TOK_INT_LIT: return "integer literal";
        case TOK_FLOAT_LIT: return "float literal";
        case TOK_CHAR_LIT: return "character literal";
        case TOK_STR_LIT: return "string literal";
        case TOK_IDENT: return "identifier";
        case TOK_AUTO: return "auto";
        case TOK_BREAK: return "break";
        case TOK_CASE: return "case";
        case TOK_CHAR: return "char";
        case TOK_CONST: return "const";
        case TOK_CONTINUE: return "continue";
        case TOK_DEFAULT: return "default";
        case TOK_DO: return "do";
        case TOK_DOUBLE: return "double";
        case TOK_ELSE: return "else";
        case TOK_ENUM: return "enum";
        case TOK_EXTERN: return "extern";
        case TOK_FLOAT: return "float";
        case TOK_FOR: return "for";
        case TOK_GOTO: return "goto";
        case TOK_IF: return "if";
        case TOK_INT: return "int";
        case TOK_LONG: return "long";
        case TOK_REGISTER: return "register";
        case TOK_RETURN: return "return";
        case TOK_SHORT: return "short";
        case TOK_SIGNED: return "signed";
        case TOK_SIZEOF: return "sizeof";
        case TOK_STATIC: return "static";
        case TOK_STRUCT: return "struct";
        case TOK_SWITCH: return "switch";
        case TOK_TYPEDEF: return "typedef";
        case TOK_UNION: return "union";
        case TOK_UNSIGNED: return "unsigned";
        case TOK_VOID: return "void";
        case TOK_VOLATILE: return "volatile";
        case TOK_WHILE: return "while";
        case TOK_ARROW: return "->";
        case TOK_INC: return "++";
        case TOK_DEC: return "--";
        case TOK_SHL: return "<<";
        case TOK_SHR: return ">>";
        case TOK_LE: return "<=";
        case TOK_GE: return ">=";
        case TOK_EQ: return "==";
        case TOK_NE: return "!=";
        case TOK_LAND: return "&&";
        case TOK_LOR: return "||";
        case TOK_MUL_ASSIGN: return "*=";
        case TOK_DIV_ASSIGN: return "/=";
        case TOK_MOD_ASSIGN: return "%=";
        case TOK_ADD_ASSIGN: return "+=";
        case TOK_SUB_ASSIGN: return "-=";
        case TOK_SHL_ASSIGN: return "<<=";
        case TOK_SHR_ASSIGN: return ">>=";
        case TOK_AND_ASSIGN: return "&=";
        case TOK_XOR_ASSIGN: return "^=";
        case TOK_OR_ASSIGN: return "|=";
        case TOK_ELLIPSIS: return "...";
        case TOK_HASH: return "#";
        case TOK_HASH_HASH: return "##";
        default: return "unknown";
    }
}

/* ========================================================================= */
/* Lexer Implementation                                                      */
/* ========================================================================= */

Lexer *lexer_new(const char *source, const char *filename) {
    Lexer *l = (Lexer *)c90_malloc(sizeof(Lexer));
    l->source = source;
    l->filename = filename;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    l->len = (int)strlen(source);
    return l;
}

static char peek_char(Lexer *l, int offset) {
    if (l->pos + offset >= l->len) {
        return '\0';
    }
    return l->source[l->pos + offset];
}

static char next_char(Lexer *l) {
    char c;
    if (l->pos >= l->len) return '\0';
    c = l->source[l->pos++];
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static Token *token_new(int kind, const char *str, const char *filename, int line, int col) {
    Token *tok = (Token *)c90_malloc(sizeof(Token));
    tok->kind = kind;
    tok->str = str ? c90_strdup(str) : NULL;
    tok->int_val = 0;
    tok->is_unsigned = 0;
    tok->filename = filename ? c90_strdup(filename) : NULL;
    tok->line = line;
    tok->col = col;
    tok->hideset = NULL;
    tok->next = NULL;
    return tok;
}

static void skip_whitespace_and_comments(Lexer *l) {
    while (l->pos < l->len) {
        char c = peek_char(l, 0);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            next_char(l);
        } else if (c == '/' && peek_char(l, 1) == '*') {
            /* C block comment */
            next_char(l);
            next_char(l);
            while (l->pos < l->len) {
                if (peek_char(l, 0) == '*' && peek_char(l, 1) == '/') {
                    next_char(l);
                    next_char(l);
                    break;
                }
                next_char(l);
            }
        } else if (c == '/' && peek_char(l, 1) == '/') {
            /* Line comment */
            next_char(l);
            next_char(l);
            while (l->pos < l->len && peek_char(l, 0) != '\n') {
                next_char(l);
            }
        } else if (c == '\\' && peek_char(l, 1) == '\n') {
            /* Line continuation (spliced) */
            l->pos += 2;
        } else if (c == '\\' && peek_char(l, 1) == '\r' && peek_char(l, 2) == '\n') {
            /* CRLF Line continuation */
            l->pos += 3;
        } else {
            break;
        }
    }
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_part(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int check_keyword(const char *name) {
    int i;
    for (i = 0; keywords[i].name != NULL; i++) {
        if (strcmp(keywords[i].name, name) == 0) {
            return keywords[i].kind;
        }
    }
    return TOK_IDENT;
}

static int parse_escape_sequence(Lexer *l) {
    char c = next_char(l);
    switch (c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
        case '\?': return '\?';
        case 'x': case 'X': {
            int val = 0;
            while (isxdigit((unsigned char)peek_char(l, 0))) {
                char h = next_char(l);
                if (h >= '0' && h <= '9') val = val * 16 + (h - '0');
                else if (h >= 'a' && h <= 'f') val = val * 16 + (h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') val = val * 16 + (h - 'A' + 10);
            }
            return val;
        }
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': {
            int val = c - '0';
            int count = 1;
            while (count < 3 && peek_char(l, 0) >= '0' && peek_char(l, 0) <= '7') {
                val = val * 8 + (next_char(l) - '0');
                count++;
            }
            return val;
        }
        default:
            return c;
    }
}

static Token *read_char_literal(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;
    int val = 0;
    
    next_char(l); /* skip opening single quote */
    if (peek_char(l, 0) == '\'') {
        c90_error(l->filename, start_line, "empty character constant");
    }
    
    while (l->pos < l->len && peek_char(l, 0) != '\'') {
        int ch;
        if (peek_char(l, 0) == '\n') {
            c90_error(l->filename, start_line, "newline in character constant");
        }
        if (peek_char(l, 0) == '\\') {
            next_char(l);
            ch = parse_escape_sequence(l);
        } else {
            ch = (unsigned char)next_char(l);
        }
        val = (val << 8) | (ch & 0xFF);
    }
    
    if (peek_char(l, 0) != '\'') {
        c90_error(l->filename, start_line, "unclosed character constant");
    }
    next_char(l); /* skip closing quote */
    
    {
        Token *tok = token_new(TOK_CHAR_LIT, NULL, l->filename, start_line, start_col);
        tok->int_val = val;
        return tok;
    }
}

static Token *read_string_literal(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;
    StrBuf *sb = strbuf_new();
    
    next_char(l); /* skip opening quote */
    while (l->pos < l->len && peek_char(l, 0) != '\"') {
        if (peek_char(l, 0) == '\\') {
            next_char(l);
            strbuf_append_char(sb, (char)parse_escape_sequence(l));
        } else if (peek_char(l, 0) == '\n') {
            c90_error(l->filename, start_line, "newline in string literal");
        } else {
            strbuf_append_char(sb, next_char(l));
        }
    }
    
    if (peek_char(l, 0) != '\"') {
        c90_error(l->filename, start_line, "unclosed string literal");
    }
    next_char(l); /* skip closing quote */
    
    {
        Token *tok = token_new(TOK_STR_LIT, sb->data, l->filename, start_line, start_col);
        tok->int_val = sb->length; /* length of string */
        strbuf_free(sb);
        return tok;
    }
}

static Token *read_number_literal(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;
    StrBuf *sb = strbuf_new();
    int is_float = 0;
    int is_hex = 0;
    int is_oct = 0;
    int is_unsigned = 0;
    int float_suffix = 0; /* 0 = double, 1 = float, 2 = long double */
    
    if (peek_char(l, 0) == '0' && (peek_char(l, 1) == 'x' || peek_char(l, 1) == 'X')) {
        is_hex = 1;
        strbuf_append_char(sb, next_char(l));
        strbuf_append_char(sb, next_char(l));
        while (isxdigit((unsigned char)peek_char(l, 0))) {
            strbuf_append_char(sb, next_char(l));
        }
    } else if (peek_char(l, 0) == '0' && isdigit((unsigned char)peek_char(l, 1))) {
        is_oct = 1;
        while (isdigit((unsigned char)peek_char(l, 0))) {
            strbuf_append_char(sb, next_char(l));
        }
    } else {
        while (isdigit((unsigned char)peek_char(l, 0))) {
            strbuf_append_char(sb, next_char(l));
        }
        if (peek_char(l, 0) == '.' && isdigit((unsigned char)peek_char(l, 1))) {
            is_float = 1;
            strbuf_append_char(sb, next_char(l));
            while (isdigit((unsigned char)peek_char(l, 0))) {
                strbuf_append_char(sb, next_char(l));
            }
        }
        if (peek_char(l, 0) == 'e' || peek_char(l, 0) == 'E') {
            is_float = 1;
            strbuf_append_char(sb, next_char(l));
            if (peek_char(l, 0) == '+' || peek_char(l, 0) == '-') {
                strbuf_append_char(sb, next_char(l));
            }
            while (isdigit((unsigned char)peek_char(l, 0))) {
                strbuf_append_char(sb, next_char(l));
            }
        }
    }
    
    /* Parse integer / float suffixes: u, l, f */
    while (1) {
        char c = peek_char(l, 0);
        if (c == 'u' || c == 'U') {
            is_unsigned = 1;
            next_char(l);
        } else if (c == 'l' || c == 'L') {
            if (is_float) float_suffix = 2;
            next_char(l);
        } else if (c == 'f' || c == 'F') {
            is_float = 1;
            float_suffix = 1;
            next_char(l);
        } else {
            break;
        }
    }
    
    if (is_float) {
        Token *tok = token_new(TOK_FLOAT_LIT, sb->data, l->filename, start_line, start_col);
        tok->is_unsigned = float_suffix;
        strbuf_free(sb);
        return tok;
    } else {
        Token *tok = token_new(TOK_INT_LIT, sb->data, l->filename, start_line, start_col);
        if (is_hex) {
            tok->int_val = (long)strtoul(sb->data, NULL, 16);
            if ((unsigned long)tok->int_val > 2147483647UL && (unsigned long)tok->int_val <= 4294967295UL) {
                is_unsigned = 1;
            }
        } else if (is_oct) {
            tok->int_val = (long)strtoul(sb->data, NULL, 8);
            if ((unsigned long)tok->int_val > 2147483647UL && (unsigned long)tok->int_val <= 4294967295UL) {
                is_unsigned = 1;
            }
        } else {
            tok->int_val = (long)strtoul(sb->data, NULL, 10);
        }
        tok->is_unsigned = is_unsigned;
        strbuf_free(sb);
        return tok;
    }
}

static Token *read_identifier_or_keyword(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;
    StrBuf *sb = strbuf_new();
    int kind;

    while (is_ident_part(peek_char(l, 0))) {
        strbuf_append_char(sb, next_char(l));
    }

    kind = check_keyword(sb->data);
    {
        Token *tok = token_new(kind, sb->data, l->filename, start_line, start_col);
        strbuf_free(sb);
        return tok;
    }
}

Token *lexer_tokenize(Lexer *l) {
    Token head;
    Token *cur = &head;
    head.next = NULL;

    while (1) {
        char c;
        int start_line, start_col;
        skip_whitespace_and_comments(l);

        if (l->pos >= l->len) {
            cur->next = token_new(TOK_EOF, NULL, l->filename, l->line, l->col);
            break;
        }

        c = peek_char(l, 0);
        start_line = l->line;
        start_col = l->col;

        if (c == '\'') {
            cur->next = read_char_literal(l);
            cur = cur->next;
        } else if (c == '\"') {
            cur->next = read_string_literal(l);
            cur = cur->next;
        } else if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)peek_char(l, 1)))) {
            cur->next = read_number_literal(l);
            cur = cur->next;
        } else if (is_ident_start(c)) {
            cur->next = read_identifier_or_keyword(l);
            cur = cur->next;
        } else {
            /* Check multi-character operators */
            Token *tok = NULL;
            char c2 = peek_char(l, 1);
            char c3 = peek_char(l, 2);

            if (c == '.' && c2 == '.' && c3 == '.') {
                next_char(l); next_char(l); next_char(l);
                tok = token_new(TOK_ELLIPSIS, "...", l->filename, start_line, start_col);
            } else if (c == '-' && c2 == '>') {
                next_char(l); next_char(l);
                tok = token_new(TOK_ARROW, "->", l->filename, start_line, start_col);
            } else if (c == '+' && c2 == '+') {
                next_char(l); next_char(l);
                tok = token_new(TOK_INC, "++", l->filename, start_line, start_col);
            } else if (c == '-' && c2 == '-') {
                next_char(l); next_char(l);
                tok = token_new(TOK_DEC, "--", l->filename, start_line, start_col);
            } else if (c == '<' && c2 == '<' && c3 == '=') {
                next_char(l); next_char(l); next_char(l);
                tok = token_new(TOK_SHL_ASSIGN, "<<=", l->filename, start_line, start_col);
            } else if (c == '>' && c2 == '>' && c3 == '=') {
                next_char(l); next_char(l); next_char(l);
                tok = token_new(TOK_SHR_ASSIGN, ">>=", l->filename, start_line, start_col);
            } else if (c == '<' && c2 == '<') {
                next_char(l); next_char(l);
                tok = token_new(TOK_SHL, "<<", l->filename, start_line, start_col);
            } else if (c == '>' && c2 == '>') {
                next_char(l); next_char(l);
                tok = token_new(TOK_SHR, ">>", l->filename, start_line, start_col);
            } else if (c == '<' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_LE, "<=", l->filename, start_line, start_col);
            } else if (c == '>' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_GE, ">=", l->filename, start_line, start_col);
            } else if (c == '=' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_EQ, "==", l->filename, start_line, start_col);
            } else if (c == '!' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_NE, "!=", l->filename, start_line, start_col);
            } else if (c == '&' && c2 == '&') {
                next_char(l); next_char(l);
                tok = token_new(TOK_LAND, "&&", l->filename, start_line, start_col);
            } else if (c == '|' && c2 == '|') {
                next_char(l); next_char(l);
                tok = token_new(TOK_LOR, "||", l->filename, start_line, start_col);
            } else if (c == '*' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_MUL_ASSIGN, "*=", l->filename, start_line, start_col);
            } else if (c == '/' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_DIV_ASSIGN, "/=", l->filename, start_line, start_col);
            } else if (c == '%' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_MOD_ASSIGN, "%=", l->filename, start_line, start_col);
            } else if (c == '+' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_ADD_ASSIGN, "+=", l->filename, start_line, start_col);
            } else if (c == '-' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_SUB_ASSIGN, "-=", l->filename, start_line, start_col);
            } else if (c == '&' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_AND_ASSIGN, "&=", l->filename, start_line, start_col);
            } else if (c == '^' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_XOR_ASSIGN, "^=", l->filename, start_line, start_col);
            } else if (c == '|' && c2 == '=') {
                next_char(l); next_char(l);
                tok = token_new(TOK_OR_ASSIGN, "|=", l->filename, start_line, start_col);
            } else if (c == '#' && c2 == '#') {
                next_char(l); next_char(l);
                tok = token_new(TOK_HASH_HASH, "##", l->filename, start_line, start_col);
            } else {
                /* Single character token */
                char single[2];
                single[0] = next_char(l);
                single[1] = '\0';
                tok = token_new((unsigned char)single[0], single, l->filename, start_line, start_col);
            }
            cur->next = tok;
            cur = cur->next;
        }
    }

    return head.next;
}
