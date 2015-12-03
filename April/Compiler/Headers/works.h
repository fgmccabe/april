/*
 * header file for Macintosh specific miscelleneous functions
 */

#ifndef _MAC_MISC_H_
#define _MAC_MISC_H_

void Str255Cat(Str255 dest,Str255 src);
void Str255Cpy(Str255 dest,Str255 src);
char *Str255ToStr(char *dst,const Str255 src);
void StrToStr255(const char *src, Str255 dst);
void Str255Insert(Str255 dest,Str255 src);
void outStr255(ioPo out,Str255 str);

void Report_Error(OSErr errorCode);
void Report_Err_Message(unsigned char *errMess);

#define Arity(A) (size_t)(sizeof(A)/sizeof(A[0]))

ioPo open_vol_file(FSSpec *file);
ioPo open_vol_outfile(FSSpec *file,OSType creator,OSType type);
void filePrefix(FSSpec *spec,Str255 prefix);

#endif
