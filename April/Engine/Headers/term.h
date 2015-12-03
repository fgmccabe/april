/*
 * Header file desribing high-level functions for reading and writing terms 
 */
#ifndef _TERM_H_
#define _TERM_H_

retCode outCell(ioPo f,objPo p,long width,int prec,logical alt);
retCode displayType(ioPo f,objPo p,long width,int prec,logical alt);
retCode wStringChr(ioPo f,uniChar ch);
retCode ReadData(ioPo in,objPo *tgt);
retCode readFmtd(ioPo in,processpo p,char *fmt,objPo *result);
retCode decodeICM(ioPo in,objPo *tgt,logical verify);
retCode decInt(ioPo in,integer *ii,uniChar tag);
retCode encodeICM(ioPo out,objPo input);
retCode encodeInt(ioPo out,long val,int tag);

extern logical verifyCode;
char *codeVerify(objPo code,logical allow_priv);

#endif
