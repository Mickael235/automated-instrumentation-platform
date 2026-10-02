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
#define  PANEL_NUMERIC_FMAX               2       /* control type: numeric, callback function: (none) */
#define  PANEL_NUMERIC_FMIN               3       /* control type: numeric, callback function: (none) */
#define  PANEL_COMMANDBUTTON_TEST         4       /* control type: command, callback function: cbTest */
#define  PANEL_COMMANDBUTTON_CONN         5       /* control type: command, callback function: cbConnexion */
#define  PANEL_NUMERIC_AMP                6       /* control type: numeric, callback function: (none) */
#define  PANEL_NUMERIC_NSTEPS             7       /* control type: numeric, callback function: (none) */
#define  PANEL_GRAPH                      8       /* control type: graph, callback function: (none) */
#define  PANEL_NUMERICDIAL                9       /* control type: scale, callback function: (none) */
#define  PANEL_NUMERICDIAL_2              10      /* control type: scale, callback function: (none) */
#define  PANEL_NUMERICDIAL_3              11      /* control type: scale, callback function: (none) */
#define  PANEL_NUMERICDIAL_4              12      /* control type: scale, callback function: (none) */
#define  PANEL_TEXTBOX                    13      /* control type: textBox, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK cbConnexion(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK cbTest(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif