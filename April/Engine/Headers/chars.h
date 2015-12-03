#ifndef _CHARS_H_
#define _CHARS_H_

/* Character literal */
typedef struct _char_record {
  long sign;			/* The sign of the entity */
  long ch;                      /* The character itself */
} charRec, *charPo;

static inline logical isChr(objPo p)
{
  return Tag(p)==charMarker;
}

static inline uniChar CharVal(objPo p)
{
  assert(isChr(p));
  
  return (uniChar)(((charPo)p)->ch);
}

#define charMark objectMark(charMarker,0)
#define CharCellCount CellCount(sizeof(charRec))

#endif
