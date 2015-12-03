%token MARK COLON BAR SEMI 
%token ID NUM BRACE CENTBRACE
%token TOKEN LEFT RIGHT NONASSOC PREC START EXPECT TYPE

%%

  apg : preamble MARK rules MARK ;

  preamble : definition
     | preamble definition 
     ;

  definition : tokendef
    | includedef
    | startdef
    | expectdef
    | typedef
    ;

  tokendef : TOKEN tokenspecs
    | LEFT tokenspecs
    | RIGHT tokenspecs
    | NONASSOC tokenspecs
    ;
  tokenspecs : tokenspec
    | tokenspecs tokenspec
    ;

  tokenspec : ID
    | ID BRACE
    | ID CENTBRACE
    ;

  includedef : CENTBRACE 
    ;

  typedef : TYPE CENTBRACE
    ;

  startdef : START ID 
    ;

  expectdef : EXPECT NUM
    ;

  rules : rule
    | rules rule
    ;

  rule : ID COLON ruleset SEMI ;
    
  ruleset : ruleset BAR production
    | production
    ;

  production : rhs action
    | rhs PREC ID action
    ;

  rhs : ID
  | rhs ID
  |
  ;

  action :
    BRACE
  |
  ;

%%


