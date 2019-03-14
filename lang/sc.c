#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* -------- DEFINITIONS -------- */

#define MAX_STR_LEN 1024

#define error(...) {             \
  fprintf(stderr, "|Error| - "); \
  fprintf(stderr, __VA_ARGS__);  \
}

void panic(char *msg) {
  fprintf(stderr, "|Unexpected Error| - %s\n", msg);
  exit(-1);
}

//---------------------------------------
// UTILITY 
//---------------------------------------

void emit(FILE *out, char *str) {
  fprintf(out, "%s", str);
}

char *str_new(char *str) {
  char *res = malloc(strlen(str) + 1);
  strcpy(res, str);
  return res;
}

//---------------------------------------
// STACK 
//---------------------------------------

struct stack_t {
  void           *obj;
  struct stack_t *next;
};

struct stack_t *stack_new(void *obj) {
  struct stack_t *res = malloc(sizeof(struct stack_t));
  res->obj  = obj;
  res->next = 0;
  return res;
}

void stack_push(struct stack_t **this, void *obj) {
  if(!*this) {
    *this = stack_new(obj);
    return;
  }
  struct stack_t *s = malloc(sizeof(struct stack_t));
  s->obj  = obj;
  s->next = *this;
  *this = s;
}

void *stack_pop(struct stack_t **this) {
  if(!*this) return 0;
  struct stack_t *f = *this;
  void *res = (*this)->obj;
  *this = (*this)->next;
  free(f);
  return res;
}

//---------------------------------------
// TOKEN
//---------------------------------------

#define KEYWORD_LIST      \
  X(BREAK,    "break")    \
  X(CASE,     "case")     \
  X(CONST,    "const")    \
  X(CONTINUE, "continue") \
  X(ELSE,     "else")     \
  X(ELIF,     "elif")     \
  X(IF,       "if")       \
  X(WHILE,    "while")    \
  X(SIZEOF,   "sizeof")   \
  X(VOID,     "void") 

#define OP_LIST          \
  X(ELLIPSIS,     "...") \
  X(RIGHT_ASSIGN, ">>=") \
  X(LEFT_ASSIGN,  "<<=") \
  X(ADD_ASSIGN,   "+=")  \
  X(SUB_ASSIGN,   "-=")  \
  X(MUL_ASSIGN,   "*=")  \
  X(DIV_ASSIGN,   "/=")  \
  X(MOD_ASSIGN,   "%=")  \
  X(AND_ASSIGN,   "&=")  \
  X(XOR_ASSIGN,   "^=")  \
  X(OR_ASSIGN,    "|=")  \
  X(RIGHT_OP,     ">>")  \
  X(LEFT_OP,      "<<")  \
  X(INC_OP,       "++")  \
  X(DEC_OP,       "--")  \
  X(PTR_OP,       "->")  \
  X(AND_OP,       "&&")  \
  X(OR_OP,        "||")  \
  X(LE_OP,        "<=")  \
  X(GE_OP,        ">=")  \
  X(EQ_OP,        "==")  \
  X(NE_OP,        "!=")  \
  X(SEMICOLON,    ";")   \
  X(L_C_B,        "{")   \
  X(R_C_B,        "}")   \
  X(L_R_B,        "(")   \
  X(R_R_B,        ")")   \
  X(L_S_B,        "[")   \
  X(R_S_B,        "]")   \
  X(COMMA,        ",")   \
  X(COLON,        ":")   \
  X(EQUALS,       "=")   \
  X(DOT,          ".")   \
  X(AND,          "&")   \
  X(NOT,          "!")   \
  X(BIT_NOT,      "~")   \
  X(PLUS,         "+")   \
  X(MINUS,        "-")   \
  X(ASTERIX,      "*")   \
  X(DIV,          "/")   \
  X(MOD,          "%")   \
  X(LESS,         "<")   \
  X(GREATER,      ">")   \
  X(BIT_XOR,      "^")   \
  X(BIT_OR,       "|")   \
  X(QUESTION,     "?")

enum token_type_e {
  ID,
  INTEGER,
  FLOAT,
  STRING,

#define X(tok, val) tok, 
KEYWORD_LIST
OP_LIST
#undef X
};

struct token_t {
  enum token_type_e token_type;
  union {
    char   *str_val;
    int    int_val;
    double float_val;
  };
};

struct token_t *token_new(enum token_type_e type) {
  struct token_t *res = malloc(sizeof(struct token_t));
  res->token_type = type;
  res->str_val    = 0;
  res->int_val    = 0;
  res->float_val  = 0;
  return res;
}

struct token_t *token_new_str(enum token_type_e type, char *str) {
  struct token_t *res = token_new(type);
  res->str_val = str;
  return res;
}

struct token_t *token_new_int(enum token_type_e type, int val) {
  struct token_t *res = token_new(type);
  res->int_val = val;
  return res;
}

struct token_t *token_new_float(enum token_type_e type, float val) {
  struct token_t *res = token_new(type);
  res->float_val = val;
  return res;
}

struct token_t *token_copy(struct token_t *this) {
  struct token_t *res = malloc(sizeof(struct token_t));
  memcpy(res, this, sizeof(struct token_t));
  return res;
}

void token_print(struct token_t *this) {
  printf("TOKEN|\n");
#define X(tok, val) if(tok == this->token_type) printf("type: %s\n", val);
  OP_LIST
  KEYWORD_LIST
#undef X
  switch(this->token_type) {
    case ID: 
      printf("type: id\nval: %s\n", this->str_val);
      break;
    case STRING: 
      printf("type: string\nval: %s\n", this->str_val);
      break;
    case INTEGER: 
      printf("type: integer\nval: %d\n", this->int_val);
      break;
    case FLOAT: 
      printf("type: float\nval: %f\n", this->float_val);
      break;
    default:
      break;
  }
}
 
//---------------------------------------
// LEXER
//---------------------------------------

struct lexer_t {
  FILE *in;
  
  struct token_t *current;
  
  // all read tokens are stored on stack
  struct stack_t *stack;
  // all rewinded tokens are stored on from
  struct stack_t *from;
};

// -- LEXER_UTIL ------------------------

char char_next(FILE *file) {
  int c = fgetc(file);
  if(c == EOF) {
    fclose(file);
    panic("end of file");
  }
  return (char)c;
}

char char_peek(FILE *file) {
  int c = fgetc(file);
  fseek(file, (long int)-1, SEEK_CUR);
  return (char)c;
}

void char_unget(FILE *file) {
  fseek(file, (long int)-1, SEEK_CUR);
}

int char_skip(FILE *file, char *set) {
  char c = 0;
  int count = -1;
  do { 
    c = char_next(file);
    count++;
    if(c == EOF) return -1;
  } while(strchr(set, c));
  char_unget(file);
  return count;
}

// checks if char is 0 .. 9
int is_num(char _c) {
  return (_c >= '0' && _c <= '9');
}

// checks if char is a .. z | A .. Z | _
int is_alpha(char _c) {
  return ((_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z') || _c == '_');
}

// checks if char is a .. z | A .. Z | 0 .. 9 | _
int is_alpha_num(char _c) {
  return ((_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z') || (_c >= '0' && _c <= '9') || _c == '_');
}

// checks if char is valid str char
int is_str(char _c) {
  return (_c >= 32 && _c <= 126);
}

#define char_move(_in) (rc++, char_next(_in))

#define char_fail() {                 \
  fseek(in, (long int)-rc, SEEK_CUR); \
  return (struct token_t*)0;          \
}

// -- LEXER_FUNCTIONS -------------------

struct token_t *lexer_identifier(FILE *in) {
  int rc = 0;
  char buffer[MAX_STR_LEN] = { 0 };
  if(!is_alpha(buffer[0] = char_move(in))) char_fail();
  size_t i = 1;
  for(;is_alpha_num(buffer[i] = char_move(in)); i++) {
    if(i >= MAX_STR_LEN) panic("identifier string too long");
  }
  char_unget(in);
  buffer[i] = 0;
  return token_new_str(ID, str_new(buffer));
}

struct token_t *lexer_integer(FILE *in) {
  int rc = 0;
  char buffer[MAX_STR_LEN] = { 0 };
  size_t i = 0;
  if(!is_num(char_peek(in))) char_fail();
  for(;is_num(buffer[i] = char_move(in)); i++) {
    if(i >= MAX_STR_LEN) panic("integer string too long");
  }
  if(strchr(".f", buffer[i])) char_fail(); // TODO: what if: 123fid
  char_unget(in);
  return token_new_int(INTEGER, strtol(buffer, 0, 10));
}

struct token_t *lexer_float(FILE *in) {
  int rc = 0;
  char buffer[MAX_STR_LEN] = { 0 };
  size_t i = 0;
  if(!is_num(char_peek(in))) char_fail();
  for(; is_num(buffer[i] = char_move(in)); i++) {
    if(i >= MAX_STR_LEN) panic("float string too long");
  }
  if(buffer[i] == 'f') {
    return token_new_float(FLOAT, strtod(buffer, 0));
  } else if(buffer[i] == '.') {
    for(i++; is_num(buffer[i] = char_move(in)); i++) {
      if(i >= MAX_STR_LEN) panic("float string too long");
    }
    char_unget(in);
    return token_new_float(FLOAT, strtod(buffer, 0));
  }
  char_fail();
}

struct token_t *lexer_string(FILE *in) {
  int rc = 0;
  char buffer[MAX_STR_LEN] = { 0 };
  if(char_move(in) != '\"') char_fail();
  size_t i = 0;
  for(;is_str(buffer[i] = char_move(in)); i++) {
    if(i >= MAX_STR_LEN) panic("string string too long");
    if(buffer[i] == '\"' && (i == 0 || buffer[i - 1] != '\\')) break;
  }
  if(buffer[i] != '\"') panic("string not properly ended (missing \")");
  buffer[i] = 0;
  return token_new_str(STRING, str_new(buffer));
}

struct token_t *lexer_keyword(FILE *in, int is_op, char *key, enum token_type_e type) {
  int rc = 0;
  for(size_t i = 0; key[i]; i++) {
    if(char_move(in) != key[i]) char_fail();
  }
  if(!is_op && is_alpha_num(char_peek(in))) char_fail();
  return token_new(type);
}

// --  ----------------------------------

void lexer_crt(struct lexer_t *this, FILE *in) {
  this->in      = in;
  this->current = 0;
  this->stack   = 0;
  this->from    = 0;
}

struct token_t *lexer_current(struct lexer_t *this) {
  return this->current;
}

// returns 0 if no token can be found anymore
struct token_t *lexer_next(struct lexer_t *this) {
  struct token_t *res = 0;
#define set_res() {              \
  stack_push(&this->stack, res); \
  this->current = res;           \
  return res;                    \
}
  
  // return 0 on eof
  if(char_peek(this->in) == EOF) return (struct token_t*)0;

  // if token is already on from stack
  // return it immediately
  if((res = stack_pop(&this->from))) set_res();
  
  // ignore all whitespaces, newlines, tabs, etc.
  char *skip_set = " \n\r\t";
  if(char_skip(this->in, skip_set) == -1) return (struct token_t*)0;

  // parser all tokens in order 
  // operators -> keywords -> identifier -> integer -> float -> string
#define X(tok, val) if((res = lexer_keyword(this->in, 1, val, tok))) set_res();
  OP_LIST
#undef X
#define X(tok, val) if((res = lexer_keyword(this->in, 0, val, tok))) set_res();
  KEYWORD_LIST
#undef X
  if((res = lexer_identifier(this->in))) set_res();
  if((res = lexer_integer(this->in)))    set_res();
  if((res = lexer_float(this->in)))      set_res();
  if((res = lexer_string(this->in)))     set_res();

  return (struct token_t*)0;
}

// resets n tokens from stack to from
void lexer_rewind(struct lexer_t *this, size_t n) {
  void *obj = 0;
  for(size_t i = 0; i < n; i++) {
    obj = stack_pop(&this->stack);
    if(!obj) return;
    stack_push(&this->from, obj);
  }
}

struct token_t *lexer_peek(struct lexer_t *this) {
  struct token_t *res = lexer_next(this);
  lexer_rewind(this, 1);
  return res;
}

void lexer_clear(struct lexer_t *this) {
  for(void *obj = 0; (obj = stack_pop(&this->stack)); free(obj));
}

//---------------------------------------
// AST_TYPES
//---------------------------------------

struct struct_t;
struct var_t;
struct function_t;
struct stm_t;
struct con_stm_t;
struct loop_stm_t;
struct var_stm_t;
struct type_t;
struct arr_type_t;
struct fun_type_t;
struct exp_t;

// -- STRUCT ----------------------------

struct struct_t {
  struct token_t *id;
  struct stack_t *vars; // <var_t>
};

// -- VARIABLE --------------------------

struct var_t {
  struct token_t *id;
  struct type_t  *type;
};

// -- FUNCTION --------------------------

struct function_t {
  struct token_t *id;
  struct type_t  *type;
  struct stack_t *params; // <var_t>
  struct stack_t *stms;   // <stm_t>
};

// -- STATEMENT -------------------------

#define STM_LIST                          \
  X(CON_STM)     /* stack_t<con_stm_t> */ \
  X(LOOP_STM)    /* loop_stm_t         */ \
  X(STRUCT_STM)  /* struct_t           */ \
  X(EXP_STM)     /* exp_t              */ \
  X(VAR_STM)     /* var_stm_t          */ \
  X(LST_STM)     /* stack_t            */ 

enum stm_type_e {
#define X(tok) tok,
#undef X
};

struct stm_t {
  enum stm_type_e stm_type;
  void            *stm;
};

struct con_stm_t {
  struct exp_t   *con;  // else has con = 0
  struct stack_t *stms; // <stm_t>
};

struct loop_stm_t {
  struct exp_t   *con;
  struct stack_t *stms; // <stm_t>
};

struct var_stm_t {
  struct var_t *var;
  struct exp_t *val;
};

// -- TYPE ------------------------------

#define TYPE_LIST        \
  X(I8_T)                \
  X(U8_T)                \
  X(I16_T)               \
  X(U16_T)               \
  X(I32_T)               \
  X(U32_T)               \
  X(I64_T)               \
  X(U64_T)               \
  X(F32_T)               \
  X(F64_T)               \
  X(VOID_T)              \
  X(ID_T)  /* token_t */ \
  X(ARR_T)               \
  X(FUN_T)               \
  X(REF_T) /* type_t */

enum type_type_e {
#define X(tok) tok,
#undef X
};

struct type_t {
  enum type_type_e type_type;
  void             *type;
};

struct arr_type_t {
  struct type_t *type;
  struct exp_t  *size;
};

struct fun_type_t {
  struct type_t  *type;
  struct stack_t *params;
};

// -- EXPRESSION ------------------------

#define EXP_LIST \
  X(COMP_EXP)    /* (type) { exp, ... } | comp_exp_t */ \
  X(INT_EXP)     /* 10 */ \
  X(FLOAT_EXP)   /* 10.0 */ \
  X(STRING_EXP)  /* "str" */\
  X(ID_EXP)      /* x */\
  X(SIZEOF_EXP)  /* sizeof(type) */ \
  X(BRACKET_EXP) /* ( exp ) */ \
  X(INC_EXP)     /* ++ exp */ \
  X(DEC_EXP)     /* -- exp */ \
  X(POS_EXP)     /* + exp */ \
  X(MIN_EXP)     /* - exp */ \
  X(NOT_EXP)     /* ! exp */ \
  X(BIT_NOT_EXP) /* ~ exp */ \
  X(CAST_EXP)    /* ( type ) exp */ \
  X(DEREF_EXP)   /* * exp */ \
  X(REF_EXP)     /* & exp */ \
  X(DOT_EXP)     /* exp . exp */ \
  X(ARROW_EXP)   /* exp -> exp */ \
  X(ARR_EXP)     /* exp [ exp ] */ \
  X(CALL_EXP)    /* exp ( exp, ... ) */ \
  X(MULT_EXP)    /* exp * exp */ \
  X(DIV_EXP)     /* exp / exp */ \
  X(MOD_EXP)     /* exp % exp */ \
  X(ADD_EXP)     /* exp + exp */ \
  X(SUB_EXP)     /* exp - exp */ \
  X(LS_EXP)      /* exp << exp */ \
  X(RS_EXP)      /* exp >> exp */ \
  X(LT_EXP)      /* exp < exp */ \
  X(GT_EXP)      /* exp > exp */ \
  X(LE_EXP)      /* exp <= exp */ \
  X(GE_EXP)      /* exp >= exp */ \
  X(EQ_EXP)      /* exp == exp */ \
  X(NE_EXP)      /* exp != exp */ \
  X(BIT_AND_EXP) /* exp & exp */ \
  X(BIT_XOR_EXP) /* exp ^ exp */ \
  X(BIT_OR_EXP)  /* exp | exp */ \
  X(AND_EXP)     /* exp && exp */ \
  X(OR_EXP)      /* exp || exp */ \
  X(TER_EXP)     /* exp ? exp : exp */ \
  X(ASG_EXP)     /* exp = exp */ \
  X(ADDA_EXP)    /* exp += exp */ \
  X(SUBA_EXP)    /* exp += exp */ \
  X(MULTA_EXP)   /* exp *= exp */ \
  X(DIVA_EXP)    /* exp /= exp */ \
  X(MODA_EXP)    /* exp %= exp */ \
  X(LSA_EXP)     /* exp <<= exp */ \
  X(RSA_EXP)     /* exp >>= exp */ \
  X(ANDA_EXP)    /* exp &= exp */ \
  X(XORA_EXP)    /* exp ^= exp */ \
  X(ORA_EXP)     /* exp |= exp */ \
  X(COMMA_EXP)   /* exp , exp */ 

struct exp_t {
};

//---------------------------------------
// PARSER 
//---------------------------------------

struct parser_t {
  FILE *in;
  FILE *out;
  
  struct lexer_t lexer;
};

int parse_struct(struct parser_t *this) {
}

int parse_var(struct parser_t *this) {
}

int parser_global(struct parser_t *this) {
}

void parse(FILE *in, FILE *out) {
  struct parser_t parser;
  parser.in  = in;
  parser.out = out;
  lexer_crt(&parser.lexer, in);
  
  struct token_t *token = 0;
  for(;;) {
    if(!(token = lexer_next(&parser.lexer))) break;
    token_print(token);
  }
}

//---------------------------------------
// MAIN PROGRAMM 
//---------------------------------------

int main(int argc, char **argv) {
  printf("+---------------------------+\n");
  printf("| Starting SC-Lang Compiler |\n");
  printf("| Author:  Gerrit Proessl   |\n");
  printf("| Version: 0.0.1            |\n");
  printf("+---------------------------+\n");
  
  FILE *in = 0;
  FILE *out = stdout;
  
  if(argc >= 3) {
    out = fopen(argv[2], "w");
    if(!out) panic("unable to open output file");
  }
 
  if(argc >= 2) {
    in = fopen(argv[1], "r");
    if(!in) panic("unable to open input file");
    parse(in, stdout);
    fclose(in);
    if(out != stdout) fclose(out);
  } else {
    panic("no input or output file specified");
  }

  return 0;
}