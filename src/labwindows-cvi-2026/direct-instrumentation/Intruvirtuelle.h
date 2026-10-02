/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  PANEL                            1
#define  PANEL_NUMERICKNOB                2       /* control type: scale, callback function: (none) */
#define  PANEL_NUMERICKNOB_2              3       /* control type: scale, callback function: (none) */
#define  PANEL_NUMERICKNOB_3              4       /* control type: scale, callback function: (none) */
#define  PANEL_NUMERICKNOB_4              5       /* control type: scale, callback function: (none) */
#define  PANEL_STRING                     6       /* control type: string, callback function: (none) */
#define  PANEL_STRING_2                   7       /* control type: string, callback function: (none) */
#define  PANEL_GRAPH                      8       /* control type: graph, callback function: (none) */
#define  PANEL_COMMANDBUTTON              9       /* control type: command, callback function: cbSweep */
#define  PANEL_STRING_3                   10      /* control type: string, callback function: (none) */
#define  PANEL_STRING_5                   11      /* control type: string, callback function: (none) */
#define  PANEL_STRING_4                   12      /* control type: string, callback function: (none) */
#define  PANEL_RINGMETER                  13      /* control type: slide, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK cbSweep(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif