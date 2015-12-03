#ifndef _TYPE_SIG_H_
#define _TYPE_SIG_H_

/* Changed 6/21/2000 to fix a problem with program code size */
/* Changed 6/16/2000 'cos of new type system based on type names */
/* Changed 4/15/2000 'cos of new instructions for apply */
/* Changed 12/31/1999 'cos of new method for handling onerror */
/* Changed 10/28/1999 'cos of new string instructions */
/* Changed 6/29/1999 'cos of new format of type signatures */
/* Changed 11/19/1999 'cos of new style of process accounting */
#define SIGNATURE 0x0304070FL	/* code signature */
#define SIGNBSWAP 0x04030F07L	/* signature when we must swap bytes not words */
#define SIGNWSWAP 0x070F0304L	/* signature to sap words only */
#define SIGNBWSWP 0x0F070403L	/* when we have to swap words and bytes */

/* Type signature codes */
typedef enum {
    number_sig='N',		/* Any number */
    symbol_sig='s',		/* Symbol */
    char_sig='c',               /* Character */
    string_sig='S',		/* String */
    handle_sig='h',		/* Handle object */
    logical_sig='l',		/* Logical value */
    var_sig='$',		/* Type variable */
    unknown_sig='A',		/* Any value -- of unknown type */
    opaque_sig='O',		/* An opaque value, has an owner and a value */

/* Compound type signatures */
    list_sig='L',		/* List pair -- NULL = nil */
    tuple_sig='T',		/* Tuple - followed by length byte */
    empty_sig='t',              /* Empty tuple */
    query_sig='?',		/* Fielded value */
    forall_sig=':',		/* universally quantified formula */

/* Code signatures */
    funct_sig='F',		/* Function object signature */
    proc_sig='P',		/* Procedure signature */

    usr_sig = 'u',		/* define a new user type */
    poly_sig = 'U',		/* polymorphic user type */
} aprilTypeSignature;

#endif

