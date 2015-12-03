/* 
  This is where you define a new escape function so that the compiler and
  the run-time system can see it
  (c) 1994-2002 Imperial College and F.G.McCabe

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  Contact: Francis McCabe <fgm@fla.fujitsu.com>

 */

#include "std-types.h"

  pescape("exit",m_exit,0,True,"PT\1N"); /* terminate with error code  */
  fescape("command_line",m_command_line,1,True,"FtLS");/* command line */
  fescape("__coerce",m__coerce,2,True,":\1FT\2sLS$\1"); /* Coerce types */
  fescape("_coerce",m_coerce,3,False,":\1FT\2sA$\1"); /* Coerce types */
  pescape("__call",m_call,4,False,"PT\2ALS"); /* Arbitrary call */
  fescape("_match",m_match,5,False,"FT\2SSl");
  fescape("_creator",m__creator,7,False,"FT\1hh");
  fescape("self",m_self,8,False,"Fth");
  fescape("creator",m_creator,9,False,"Fth");
  fescape("is_root_process",m_topmost,10,False,"FT\1hl");
  fescape("file_manager",m_filer,11,False,"Fth");
  pescape("_set_file_manager",m_file_manager,12,True,"PT\1h");/* set file mgr */
  fescape("_mailer",m_mailer,13,False,"Fth");/* mailer process */
  pescape("_set_mailer",m_set_mailer,14,True,"PT\1h"); /* set mailer */
  fescape("_monitor",m_monitor,15,False,"Fth");
  pescape("_set_monitor",m_set_monitor,16,True,"PT\1h");/* set monitor */

  fescape("__split_clicks",m_split_clicks,17,False,"FT\2ONO");
  pescape("__credit_clicks",m_credit_clicks,18,True,"PT\3ONO");
  fescape("__clicks",m_clicks,19,False,"FT\1ON");
  fescape("__new_clicks",m_new_clicks,20,True,"FT\1NO");
  pescape("__use_clicks",m_use_clicks,21,True,"PT\1O");
  fescape("__get_clicks",m_get_clicks,22,True,"FT\1hO");
  pescape("__merge_clicks",m_merge_clicks,23,False,"PT\2OO");

  fescape("_canonical_handle",m_canonical,26,False,"FT\1Sh");
  fescape("_handle",m_make_handle,28,False,"FT\2ssh");

  fescape("host",m_host,31,False,"FtS");
  fescape("hosttoip",m_hosttoip,32,False,"FT\1SLS");
  fescape("iptohost",m_iptohost,33,False,"FT\1SS");

  fescape("ticks",m_ticks,40,False,"FtN");
  fescape("now",m_now,41,False,"FtN");
  fescape("today",m_today,42,False,"FtN");
  fescape("timetodate",m_tval2date,43,False,"FT\1N" STD_DATE );
  fescape("timetogmt",m_tval2gmt,44,False,"FT\1N" STD_DATE);
  fescape("datetotime",m_date2tval,45,False,"FT\1" STD_DATE "N");
  fescape("gmttotime",m_gmt2tval,46,False,"FT\1" STD_DATE "N");

  fescape("explode",m_explode,50,False,":\1FT\1sS"); /* explode symbol into characters */
  fescape("implode",m_implode,51,False,"FT\1Ss");   /* implode symbol from characters */
  fescape("expand",m_expand,52,False,"FT\2SSLS");
  fescape("collapse",m_collapse,53,False,"FT\2LSSS");
  fescape("gensym",m_gensym,54,False,"Fts"); /* generate a symbol  */
  fescape("gensym2",m_gensym2,55,False,"FT\1Ss"); 

  fescape("++",m_app,57,False,"FT\2SSS");  /* Synonym for append  */
  
  /* Character class escapes */

  fescape("isCcChar",m_isCcChar,65,False,"FT\1cl"); // is Other, control char
  fescape("isCfChar",m_isCfChar,66,False,"FT\1cl"); // is Other, format char
  fescape("isCnChar",m_isCnChar,67,False,"FT\1cl"); // is Other, unassigned char
  fescape("isCoChar",m_isCoChar,68,False,"FT\1cl"); // is Other, private char
  fescape("isCsChar",m_isCsChar,69,False,"FT\1cl"); // is Other, surrogate char
  fescape("isLlChar",m_isLlChar,70,False,"FT\1cl"); // is Letter, lowercase char
  fescape("isLmChar",m_isLmChar,71,False,"FT\1cl"); // is Letter, modifier char
  fescape("isLoChar",m_isLoChar,72,False,"FT\1cl"); // is Letter, other char
  fescape("isLtChar",m_isLtChar,73,False,"FT\1cl"); // is Letter, title char
  fescape("isLuChar",m_isLuChar,74,False,"FT\1cl"); // is Letter, uppercase char
  fescape("isMcChar",m_isMcChar,75,False,"FT\1cl"); // is Mark, spacing char
  fescape("isMeChar",m_isMeChar,76,False,"FT\1cl"); // is Mark, enclosing char
  fescape("isMnChar",m_isMnChar,77,False,"FT\1cl"); // is Mark, nonspacing char
  fescape("isNdChar",m_isNdChar,78,False,"FT\1cl"); // is Number, decimal digit
  fescape("isNlChar",m_isNlChar,79,False,"FT\1cl"); // is Number, letter char
  fescape("isNoChar",m_isNoChar,80,False,"FT\1cl"); // is Number, other char
  fescape("isPcChar",m_isPcChar,81,False,"FT\1cl"); // is Punctuation, connector
  fescape("isPdChar",m_isPdChar,82,False,"FT\1cl"); // is Punctuation, dash char
  fescape("isPeChar",m_isPeChar,83,False,"FT\1cl"); // is Punctuation, close char
  fescape("isPfChar",m_isPfChar,84,False,"FT\1cl"); // is Punctuation, final quote
  fescape("isPiChar",m_isPiChar,85,False,"FT\1cl"); // is Punctuation, initial quote
  fescape("isPoChar",m_isPoChar,86,False,"FT\1cl"); // is Punctuation, other char
  fescape("isPsChar",m_isPsChar,87,False,"FT\1cl"); // is Punctuation, open char
  fescape("isScChar",m_isScChar,88,False,"FT\1cl"); // is Symbol, currency char
  fescape("isSkChar",m_isSkChar,89,False,"FT\1cl"); // is Symbol, modifier char
  fescape("isSmChar",m_isSmChar,90,False,"FT\1cl"); // is Symbol, math char
  fescape("isSoChar",m_isSoChar,91,False,"FT\1cl"); // is Symbol, other char
  fescape("isZlChar",m_isZlChar,92,False,"FT\1cl"); // is Separator, line char
  fescape("isZpChar",m_isZpChar,93,False,"FT\1cl"); // is Separator, para char
  fescape("isZsChar",m_isZsChar,94,False,"FT\1cl"); // is Separator, space char

  fescape("isLetter",m_isLetterChar,95,False,"FT\1cl"); // is letter char
  fescape("digitCode",m_digitCode,96,False,"FT\1cN"); // convert char to num
  fescape("charOf",m_charOf,97,False,"FT\1Nc"); // convert num to char
  fescape("charCode",m_charCode,98,False,"FT\1cN"); // convert char to code

  fescape("utf2str",m_utf2str,99,False,"FT\1LNS"); // convert list of utf8 codes to a string
  fescape("str2utf",m_str2utf,100,False,"FT\1SLN"); // convert string to list of utf8 codes 

  fescape("sencode",m_sencode,101,False,"FT\1AS");
  fescape("sdecode",m_sdecode,102,False,"FT\1SA");

  fescape("num2str",m_num2str,103,False,"FT\6NNNcllS"); /* format a number */
  fescape("int2str",m_int2str,104,False,"FT\5NNNclS"); /* format an integer */
  fescape("hdl2str",m_hdl2str,105,False,"FT\2hlS"); /* format a handle */
  fescape("strpad",m_strpad,106,False,"FT\3cNSS"); /* format a string */
  fescape("strof",m_strof,109,False,":\1FT\3$\1NNS"); /* format values */
  
  fescape("__parseURL",m_parseURL,110,True,"FT\1ST\6SSNSSS");  /* Parse a URL into component parts */
// Used in the bootstrap
  fescape("__mergeURL",m_mergeURL,111,True,"FT\2SSS");  /* Merge base and desired urls into real one */
  fescape("__openURL",m_openURL,112,True,"FT\2SsO");   /* Open a URL */
  fescape("__createURL",m_createURL,113,True,"FT\2SsO");   /* Open a URL to write to */

  fescape("__open",m_fopen,114,True,"FT\2S"OPEN_MODE"O"); /* open a file */
  pescape("__close",m_fclose,115,True,"PT\1O"); /* close file */
  fescape("__dupfile",m_dupfile,116,True,"FT\1OO"); /* duplicate file */
  fescape("__popen",m_popen,117,True,"FT\3SLSLST\3OOO"); /* open a pipe */
  fescape("__eof",m_eof,118,True,"FT\1Ol"); /* end of file test */
  fescape("__ready",m_ready,119,True,"FT\1Ol"); /* file ready test */
  fescape("__inbytes",m_inbytes,120,True,"FT\2ONLN"); /* read block of bytes */
  fescape("__inchars",m_inchars,134,True,"FT\2ONS"); /* read series of characters */
  fescape("__inascii",m_inascii,121,True,"FT\1ON"); /* read byte as ASCII  */
  fescape("__inchar",m_inchar,122,True,"FT\1Oc"); /* read single character    */
  fescape("__inline",m_inline,123,True,"FT\1OS"); /* read a line */
  fescape("__intext",m_intext,124,True,"FT\1OS"); /* read available text */
  fescape("__binline",m_binline,125,True,"FT\1OS"); /* read a balanced line */
  pescape("__outchar",m_outchar,126,True,"PT\2OS"); /* write a string */
  pescape("__outch",m_outch,127,True,"PT\2Oc"); /* write a single character */
  pescape("__outbytes",m_outbytes,128,True,"PT\2OLN"); /* write a string */
  fescape("__stdfile",m_stdfile,129,True,"FT\1NO"); 
  fescape("__read",m_read,130,True,":\1FT\1O$\1"); /* read a term */
  pescape("__encode",m_encode,131,True,":\1PT\2O$\1"); /* encode a term to output */
  fescape("__decode",m_decode,132,True,":\1FT\1O$\1"); /* decode an input term */
  fescape("__tell",m_ftell,133,True,"FT\1ON"); /* report file position */
// 134 is inchars
  pescape("__flush",m_flush,135,True,"PT\1O"); /* flush the I/O buffer */
  pescape("__flushall",m_flushall,136,True,"Pt"); /* flush all I/O */

  pescape("__log_msg",m_log_msg,137,True,"PT\1S"); /* write a log msg */

  fescape("__ffile",m_ffile,138,True,"FT\1Sl"); /* test for file */
  fescape("__stat",m_fstat,139,True,"FT\1S" FILE_STAT); /*file status*/
  pescape("__rm",m_frm,140,True,"PT\1S"); /* remove file */
  pescape("__mv",m_fmv,141,True,"PT\2SS"); /* rename file */
  pescape("__mkdir",m_fmkdir,143,True,"PT\2SL"FIL_MODE); /* create directory */
  pescape("__chmod",m_chmod,144,True,"PT\2SL"FIL_MODE); /* modifies permissions */
  fescape("__file_type",m_file_type,145,True,"FT\1S"FILE_TYPE); /* type of file */
  fescape("__fmode",m_fmode,146,True,"FT\1SL"FIL_MODE); /* returns file permissions */
  fescape("__ls",m_fls,147,True,"FT\1SLS"); /* return list of files */
  pescape("__rmdir",m_frmdir,148,True,"PT\1S"); /* remove a directory */

  // used in bootstrap
  fescape("_load_code_",m_load,150,True,"FT\1SFT\2FT\1SASA"); /* special function to load code */

  /* Socket handling functions */
  fescape("_listen",m_listen,151,True,"FT\1NO");
  fescape("_accept",m_accept,152,True,"FT\1OT\2OO");
  fescape("_connect",m_connect,153,True,"FT\2SNT\2OO");
  fescape("_peername",m_peername,154,True,"FT\1OT\2SN");
  fescape("_peerip",m_peerip,155,True,"FT\1OT\2SN");


  /* fork a process - either as a function or a procedure */
  fescape("_fork_",m_fork,156,False,"FT\1Pth");
  pescape("_fork_",m_fork,157,False,"PT\1Pt"); 
  fescape("__fork_",m_fork2,158,True,"FT\006hhhhlPth");
  pescape("__fork_",m_fork2,159,True,"PT\006hhhhlPt");

  pescape("delay",m_delay,160,False,"PT\1N"); /* delay process */
  pescape("sleep",m_sleep,161,False,"PT\1N"); /* delay process */

  pescape("_kill",m_kill,162,True,"PT\1h"); /* kill process */
  pescape("_interrupt",m_interrupt,163,True,"PT\2"SYS_ERROR"h");   /* Force a process to raise an exception */
  fescape("done",m_done,164,False,"FT\1hl"); /* process ended? */
  fescape("state",m_state,165,False,"FT\1h"PRC_STATE); /* process state */

  /* send message */
  pescape("_send",m_send,166,False,"PT\3hLu'" MSG_ATTR_TYPE "'A");
  pescape("__send",m_send2,167,True,"PT\4hLu'" MSG_ATTR_TYPE "'Ah");
  pescape("_front_msg",m_front_msg,168,False,"PT\3ALu'" MSG_ATTR_TYPE "'h");/* msg on front */
  
//  fescape("commserver",m_commserver,47,False,"Fth")             // Pick up communications server handle

  fescape("__getmsg",m_getmsg,169,False,"FT\1NT\5AhhLu'" MSG_ATTR_TYPE "'N");
  fescape("__nextmsg",m_nextmsg,170,False,"FT\1NT\5AhhLu'" MSG_ATTR_TYPE "'N");
  fescape("__waitmsg",m_waitmsg,171,False,"FT\2NNT\5AhhLu'" MSG_ATTR_TYPE "'N");
  pescape("__replacemsg",m_replacemsg,172,False,"PT\4AhLu'" MSG_ATTR_TYPE "'N");
  pescape("wait_for_msg",m_wait_msg,173,False,"Pt"); /* wait for a message */
  fescape("messages",m_messages,174,False,"FtN"); /* Count o/s messages */

  fescape("_number",m_number,175,False,":\1FT\1$\1N"); /* generate a number */

  fescape("rand",m_rand,176,False,"FT\1NN"); /* random number */
  fescape("irand",m_irand,177,False,"FT\1NN"); /* int random number */
  pescape("seed",m_seed,178,False,"PT\1N"); /* seed random number generator */

  fescape("<>",m_app,179,False,":\1FT\2L$\1L$\1L$\1");  /* append */
  fescape("listlen",m_listlen,180,False,":\1FT\1L$\1N");  /* list length */
  fescape("#",m_nth,181,False,":\1FT\2L$\1N$\1");  /* list element */
  fescape("head",m_head,182,False,":\1FT\1L$\1$\1"); /* Head of a list */
  fescape("front",m_front,183,False,":\1FT\2L$\1NL$\1"); /* Front of list */
  fescape("back",m_back,184,False,":\1FT\2L$\1NL$\1"); /* Back of list */
  fescape("tail",m_tail,185,False,":\1FT\2L$\1NL$\1"); 
  fescape("..",m_iota,186,False,"FT\2NNLN");  /* Generate a list of integers */
  fescape("iota",m_iota3,187,False,"FT\3NNNLN");
  fescape("sort",m_sort,188,False,":\1FT\1L$\1L$\1"); /* sort a list */
  fescape("_setof",m_setof,189,False,":\1FT\1L$\1L$\1"); /* convert list into set */

  fescape("+",m_plus,190,False,"FT\2NNN"); /* escape plus */
  fescape("-",m_minus,191,False,"FT\2NNN");
  fescape("*",m_times,192,False,"FT\2NNN");
  fescape("/",m_div,193,False,"FT\2NNN"); /* division */
  fescape("+.",m_iplus,194,False,"FT\2NNN"); /* integer plus */
  fescape("-.",m_iminus,195,False,"FT\2NNN");
  fescape("*.",m_itimes,196,False,"FT\2NNN");
  fescape("/.",m_idiv,197,False,"FT\2NNN"); /* division */
  fescape("quot",m_quo,198,False,"FT\2NNN"); /* quotient */
  fescape("band",m_band,199,False,"FT\2NNN"); /* bit and */
  fescape("bor",m_bor,200,False,"FT\2NNN"); /* bit or */
  fescape("bxor",m_bxor,201,False,"FT\2NNN"); /* bit xor */
  fescape("bnot",m_bnot,202,False,"FT\1NN"); /* bit not */
  fescape("bleft",m_bleft,203,False,"FT\2NNN"); /* shift */
  fescape("bright",m_bright,204,False,"FT\2NNN"); 
  fescape("rem",m_rem,205,False,"FT\2NNN"); /* remainder */
  fescape("abs",m_abs,206,False,"FT\1NN"); /* absolute value */
  fescape("itrunc",m_itrunc,207,False,":\1FT\1$\1N"); /* anything to integer */
  fescape("trunc",m_trunc,208,False,"FT\1NN"); /* trunc a float to int*/
  fescape("ceil",m_ceil,209,False,"FT\1NN"); /* ceiling function */
  fescape("floor",m_floor,210,False,"FT\1NN"); /* floor function */
  fescape("sqrt",m_sqrt,211,False,"FT\1NN"); /* square root */
  fescape("cbrt",m_cbrt,212,False,"FT\1NN"); /* cube root */
  fescape("pow",m_pow,213,False,"FT\2NNN"); /* x to the power y */
  fescape("exp",m_exp,214,False,"FT\1NN"); /* exponential */
  fescape("log",m_log,215,False,"FT\1NN"); /* natural logarithm */
  fescape("log10",m_log10,216,False,"FT\1NN"); /* logarithm base 10 */
  fescape("ldexp",m_ldexp,217,False,"FT\2NNN"); /* compute x*2**n */
  fescape("frexp",m_frexp,218,False,"FT\1NT\2NN"); /* compute fraction and exponent */
  fescape("modf",m_modf,219,False,"FT\1NT\2NN"); /* compute integer/fraction */
  fescape("pi",m_pi,220,False,"FtN"); /* PI */
  fescape("sin",m_sin,221,False,"FT\1NN"); /* trig sine */
  fescape("cos",m_cos,222,False,"FT\1NN"); /* trig cosine */
  fescape("tan",m_tan,223,False,"FT\1NN"); /* trig tangent */
  fescape("asin",m_asin,224,False,"FT\1NN"); /* inverse sine */
  fescape("acos",m_acos,225,False,"FT\1NN"); /* inverse cosine */
  fescape("atan",m_atan,226,False,"FT\1NN"); /* inverse tangent */
  fescape("sinh",m_sinh,227,False,"FT\1NN"); /* hyperbolic sine */
  fescape("cosh",m_cosh,228,False,"FT\1NN"); /* hyperbolic cosine */
  fescape("tanh",m_tanh,229,False,"FT\1NN"); /* hyperbolic tan */
  fescape("asinh",m_asinh,230,False,"FT\1NN"); /* inv hyper sin */
  fescape("acosh",m_acosh,231,False,"FT\1NN"); /* inv hyper cos */
  fescape("atanh",m_atanh,232,False,"FT\1NN"); /* inv hyper tan*/
  fescape("integral",m_integral,233,False,"FT\1Nl"); /* test for integers*/

  pescape("_debug",m_debug,234,False,"PT\1A"); /* send message to debugger */
  pescape("_debug_wait",m_debug_wait,235,False,"Pt"); /* wait for debugger */

  fescape("shell",m_shell,237,True,"FT\3SLSLSN"); /* run a unix command */
  fescape("exec",m_exec,238,True,"FT\3SLSLSN");/* exec a unix command */
  pescape("exec",m_exec,239,True,"PT\3SLSLS");
  fescape("waitpid",m_waitpid,240,True,"FT\1NN");/* wait for a unix command */
  fescape("envir",m_envir,241,False,"FtLT\2SS"); /* env */
  fescape("getenv",m_getenv,242,False,"FT\1SS");/* get env var */
  pescape("setenv",m_setenv,243,True,"PT\2SS");/* set env var */

  pescape("_break",m_break,244,True,"PT\1S"); /* Invoke user break point */

  fescape("voidfun",m_voidfun,245,False,"FT\1$\1$\2"); /* do nothing function */
  fescape("nullfun",m_voidfun,245,False,"FT\1$\1$\2"); /* do nothing function */
  pescape("fail",m_fail,246,False,"PT\1$\1"); /* do nothing procedure */
  pescape("nullproc",m_fail,246,False,"PT\1$\1"); /* do nothing procedure */

  fescape("===",m_same,247,False,":\1FT\2$\1$\1l"); /* true if same pointer */
  fescape("!==",m_notsame,248,False,":\1FT\2$\1$\1l"); /* true if not same */
  fescape("__dll_load_",m_dll_load,249,True,"FT\2SSO");
  fescape("__dll_fcall_",m_dll_call,250,True,":\1:\2FT\3ON$\1$\2");
  pescape("__dll_pcall_",m_dll_call,251,True,":\1PT\3ON$\1");

/* Last escape = 251 */
