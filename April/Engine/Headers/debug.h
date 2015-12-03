#ifndef DEBUG_H_
#define DEBUG_H_

/* Declarations to the debugger hooks */ 

extern logical SymbolDebug;
extern logical TCPDebug;	/* true if debugging with messages */

void initDebugger(uniChar *host,int port);

retCode wait_for_user(processpo p,insPo pc,objPo *fp,objPo env);
void debug_stop(WORD32 count,processpo p,insPo pc,objPo *sp,objPo *fp,objPo env,
		objPo *Lits,objPo prefix);

retCode line_debug(processpo p,objPo file, int lineno);
retCode entry_debug(processpo,objPo pr);
retCode exit_debug(processpo p,objPo proc);
retCode return_debug(processpo p,objPo fun,objPo val);
retCode assign_debug(processpo,objPo var,objPo val);
retCode accept_debug(processpo,objPo,objPo);
retCode fork_debug(processpo parent,objPo child);
retCode die_debug(processpo p);
retCode send_debug(processpo p,objPo msg,objPo from,objPo to);
retCode scope_debug(processpo p,int level);
retCode suspend_debug(processpo);
retCode error_debug(processpo p,objPo msg);

insPo dissass(insPo pc,insPo base,objPo *fp, objPo *e,objPo *lits);

/* Debugging escapes */
retCode m_debug(processpo p,objPo * args);
retCode m_debug_wait(processpo p,objPo *args);
retCode m_break(processpo p,objPo *args);

#endif
