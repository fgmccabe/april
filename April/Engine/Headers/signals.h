/*
 * Signal management functions
 */
#ifndef _A_SIGNAL_H_
#define _A_SIGNAL_H_

#include <signal.h>

void setupSignals(void);
void startInterrupts(sigset_t blocked);	/* enable control-C interrupts */
sigset_t stopInterrupts(void);	/* stop control-C interruptes */
#endif
