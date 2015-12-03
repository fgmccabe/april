/*
 * Grammar rules for a version of april that can be parsed with an
 * LALR(1) grammar
 */

/* Keyword tokens */
%token PROGRAM EXECUTE MODULE EXPORT 
%token TYPEOF

%token TYPEDEF
%right SEMI 
%nonassoc ONERROR FUN PROC
%right BAR
%nonassoc FN ST
%nonassoc DEFN FIELD ASSIGN QUERY
%left DOT
%right COMMA
%nonassoc CONS
%nonassoc GUARD
%nonassoc MATCH
%token SHRIEK
%token ID NUM TVAR QUOTED QSTRING
%token VALOF COLLECT SETOF TRY EXCEPTION

%left MINUS
%token LPAR RPAR LBRA RBRA LBRCE RBRCE 
%%

source : typedecs program
 | typedecs module
 ;

typedecs : typedecs typedec
 |
 ;

typedec : typetemplate TYPEDEF type ;

typetemplate : ID
 | ID LPAR typetemplateargs RPAR
 ;

typetemplateargs : TVAR
 | typetemplateargs COMMA TVAR
 ;

type: type0
 | type FN type0
 | type LBRA RBRA 
 | type LBRCE RBRCE
 | type BAR type0
 | ID QUERY type0
 | ID LPAR tpltype RPAR
 ;
type0 : 
   TVAR
 | ID
 | TVAR DOT type0
 | TVAR MINUS type0
 | LPAR tpltype RPAR
 | TYPEOF exp00
 ;

tpltype : type
 | tpltype COMMA type
 |
 ;

program : PROGRAM theta EXECUTE ID ;

module : MODULE theta EXPORT exp ;

theta : LBRCE thfields RBRCE;

thfields : thfield SEMI
 | thfields SEMI thfield
 ;

thfield : ID DEFN exp
 | ID FIELD exp
 | ID ptn FN exp
 | ID ptn LBRCE stmt RBRCE
 ;

/* Pattern */
ptn: uptn
 | uptn GUARD exp
 ;

/* Unguarded pattern */
uptn: uptn0
 | uptn0 MATCH ID
 | ID QUERY type0
 ;

/* Primitive unguarded pattern */
uptn0 : ID
 | ID LPAR tplptn RPAR
 | LPAR tplptn RPAR
 | LBRA ptn lstptn RBRA
 | LBRA RBRA
 | SHRIEK exp0
 ;

lstptn: COMMA ptn lstptn
 | CONS ptn
 ;

tplptn : tplptn COMMA ptn
 | ptn
 |
 ;

exp : exp0
 | NUM
 | QUOTED
 | QSTRING
 | LBRA listelmts RBRA
 | FUN function
 | PROC procedure
 | VALOF LBRCE stmts RBRCE
 | COLLECT LBRCE stmts RBRCE
 | SETOF LBRCE stmts RBRCE
 | TRY exp ONERROR function
 | EXCEPTION exp
 | theta
 ;


/* These rule express the syntax for function application */
exp0 : exp00
 | exp0 exp00
 ;

exp00 : ID
 | LPAR tplexp RPAR
 ;



listelmts : exp  
 | listelmts COMMA exp
 | listelmts CONS exp 
 |
 ;

tplexp : exp  
 | tplexp COMMA exp
 |
 ;

function: equation
 | function BAR equation
 ;

equation: ptn FN exp
 ;

procedure: clause
 | procedure BAR clause
 ;

clause: ptn ST stmt;

stmt : ptn DEFN exp
 | ptn FIELD exp
 | ptn ASSIGN exp
 | LBRCE stmts RBRCE
 ;

stmts : stmt
 | stmts SEMI stmt
 ;
