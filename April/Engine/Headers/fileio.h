/*
 * Header for April run-time file system
 */
#ifndef _FILEIO_H_
#define _FILEIO_H_

void initFiles(void);
void setUpAsyncIO(int fd);

logical executableFile(char *file);

#define _F_OPAQUE_ 'F'

ioPo opaqueFilePtr(objPo p);
objPo allocOpaqueFilePtr(ioPo file);
retCode load_code_file(uniChar *sys,uniChar *url,objPo *tgt,logical verify);

typedef enum { input, output } ioMode;
 
retCode attachProcessToFile(ioPo f,processpo p,ioMode mode);
void detachProcessFromFile(ioPo f,processpo p);
void detachAllFromFile(ioPo f,processpo p);

#include <sys/time.h>
int set_in_fdset(fd_set *set);
int set_out_fdset(fd_set *set);
void trigger_io(fd_set *inset,fd_set *outset,int max);

#endif
