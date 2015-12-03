/*
 * Main header file for Macintosh version of April compiler
 */
#ifndef _APRIL_MAC_H_
#define _APRIL_MAC_H_

/* Include system headers */
#include <Types.h>
#include <QuickDraw.h>
#include <Windows.h>
#include <Fonts.h>
#include <Controls.h>
#include <Dialogs.h>
#include <Menus.h>
#include <Devices.h>
#include <Memory.h>
#include <Events.h>
#include <Menus.h>
#include <Devices.h>
#include <Events.h>
#include <Events.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <StandardFile.h>
#include <Errors.h>
#include <Resources.h>
#include <DiskInit.h>

#include <AppleTalk.h>
#include <AppleEvents.h>
#include <EPPC.h>
#include <PPCToolBox.h>
#include <Processes.h>

#include <stdlib.h>

/* April specific headers */
#include "logical.h"
#include "io.h"
#include "works.h"				/* Mac specific miscellaneous functions */

/* High-level Apple Event functions */
logical Init_AEEvents(void);			/* Install AE handlers */
void Do_High_Level(EventRecord *AERecord);	/* Post high-level event to the dispatch table */
void HandleEvent(EventRecord *event);
DialogPtr OpenPanel(void);
pascal Boolean PanelEvent(DialogPtr dlog,EventRecord *theEvent,short *itemHit);
void closePanel(DialogPtr dlog,short item);
void ShowPrefPanel(void);
void ShowAboutBox(void);

#define IN_FRONT		-1
#define DEFAULT_VOL		0

OSErr Munge_File(ioPo input, ioPo output, unsigned char *fileName);

#define AAM_TYPE ((OSType)'.AAM')		/* April binary code type */
#define TREE_TYPE ((OSType)'ATRE')		/* APril parse tree type */
#define COMPILER_CREATOR ((OSType)'APRC')	/* April compiler files */
#define APRIL_CREATOR ((OSType)'APRL')		/* April run-time files */

extern logical guserDone;			/* true when terminating ...*/


#endif
