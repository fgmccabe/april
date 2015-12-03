#ifndef _UNIX_COMP_H_
#define _UNIX_COMP_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* 
 * Unix specific functions 
 */

ioPo open_stdout(void);
ioPo open_stdin(void);

ioPo OpenLogFile(void);

#endif
