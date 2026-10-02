#include <cvirte.h>
#include <userint.h>
#include <ansi_c.h>
#include <utility.h>
#include <math.h>
#include <string.h>

#include <tcpsupp.h>
#include <visa.h>

#include "serveur.h"

#define SERVER_PORT 5000

/* ====== Globals ====== */
static int panelHandle = 0;
static int g_serverRunning = 0;

static ViSession rm  = VI_NULL;
static ViSession dmm = VI_NULL;
static ViSession gbf = VI_NULL;



static void CloseVisa(void)
{
    if (gbf != VI_NULL) { viPrintf(gbf, "OUTP OFF\n"); viClose(gbf); gbf = VI_NULL; }
    if (dmm != VI_NULL) { viClose(dmm); dmm = VI_NULL; }
    if (rm  != VI_NULL) { viClose(rm);  rm  = VI_NULL; }
}

static int OpenVisa(const char *addrDmm, const char *addrGbf)
{
    if (viOpenDefaultRM(&rm) < VI_SUCCESS) return -1;
    if (viOpen(rm, (ViRsrc)addrGbf, VI_NULL, VI_NULL, &gbf) < VI_SUCCESS) { CloseVisa(); return -2; }
    if (viOpen(rm, (ViRsrc)addrDmm, VI_NULL, VI_NULL, &dmm) < VI_SUCCESS) { CloseVisa(); return -3; }

    viSetAttribute(gbf, VI_ATTR_TMO_VALUE, 5000);
    viSetAttribute(dmm, VI_ATTR_TMO_VALUE, 5000);
    return 0;
}

/* ====== Sweep + envoi TCP ====== */
static void RunSweepAndSend(unsigned tcpHandle,
                            const char *addrDmm, const char *addrGbf,
                            double ampVpp, double fmin, double fmax, int nSteps)
{
    if (ampVpp <= 0.0) ampVpp = 1.0;
    if (fmin <= 0.0)   fmin = 10.0;
    if (fmax <= fmin)  fmax = 10000.0;
    if (nSteps < 2)    nSteps = 50;

    double Vin_rms = ampVpp / (2.0 * sqrt(2.0));
    if (Vin_rms < 1e-12) Vin_rms = 1e-12;
	
	double *freqArray  = (double*)malloc(nSteps * sizeof(double));
    double *gainArray  = (double*)malloc(nSteps * sizeof(double));
    if (OpenVisa(addrDmm, addrGbf) != 0)
    {
        ServerTCPWrite(tcpHandle, "ERR:VISA\n", 9, 2000);
        return;
    }

    /* Config GBF */
    viPrintf(gbf, "OUTP:LOAD INF\n");
    viPrintf(gbf, "FUNC SIN\n");
    viPrintf(gbf, "VOLT:OFFS 0\n");
    viPrintf(gbf, "VOLT %.6f\n", ampVpp);
    viPrintf(gbf, "OUTP ON\n");

    ServerTCPWrite(tcpHandle, "OK\n", 3, 2000);
	
    for (int i = 0; i < nSteps; ++i)
    {
        double ratio = (double)i / (double)(nSteps - 1);
        double f = fmin * pow(fmax / fmin, ratio);
		freqArray[i]=f;

        if (viPrintf(gbf, "FREQ %.6f\n", f) < VI_SUCCESS) break;


        Delay(0.15);

        double vOut_rms = 0.0;
        ViStatus st = viQueryf(dmm, "MEAS:VOLT:AC?\n", "%lf", &vOut_rms);
        if (st < VI_SUCCESS || vOut_rms < 1e-12) vOut_rms = 1e-12;

        double gainDB = 20.0 * log10(vOut_rms / Vin_rms);
		gainArray[i]=gainDB;

        char line[128];
        snprintf(line, sizeof(line), "%.6f;%.6f\n", f, gainDB);
        ServerTCPWrite(tcpHandle, line, (unsigned)strlen(line), 2000);
        ProcessSystemEvents();
    }

    ServerTCPWrite(tcpHandle, "END\n", 4, 2000);
    CloseVisa();
}

/* ====== TCP callback ====== */
static int CVICALLBACK TcpServerCB(unsigned handle, int xType, int errCode, void *callbackData)
{
    if (xType == TCP_CONNECT)    {
		SetCtrlVal(panelHandle,PANEL_STRING_3,"127.0.0.1");
		printf("[SERVER] Client connecte.\n"); return 0; }
    if (xType == TCP_DISCONNECT) { printf("[SERVER] Client déconnecte.\n"); return 0; }

    if (xType == TCP_DATAREADY)
    {
        char req[2048];
        int r = ServerTCPRead(handle, req, sizeof(req)-1, 3000);
        if (r <= 0) return 0;
        req[r] = '\0';

        /* Lire adresses depuis UI */
        char addrDmm[256] = {0};
        char addrGbf[256] = {0};
        GetCtrlVal(panelHandle, PANEL_STRING,   addrDmm);
        GetCtrlVal(panelHandle, PANEL_STRING_2, addrGbf);

        /* Paramètres par défaut + parsing */
        double amp = 1.0, fmin = 10.0, fmax = 10000.0;
        int nSteps = 50;

        char *p;
        p = strstr(req, "AMP=");    if (p) amp   = atof(p + 4);
        p = strstr(req, "FMIN=");   if (p) fmin  = atof(p + 5);
        p = strstr(req, "FMAX=");   if (p) fmax  = atof(p + 5);
        p = strstr(req, "NSTEPS="); if (p) nSteps= atoi(p + 7);
		
		char string[100];
		char string2[100];

		snprintf(string, sizeof(string),
		         "[SERVER] START: amp=%.3f fmin=%.1f fmax=%.1f n=%d\n",
		         amp, fmin, fmax, nSteps);

		snprintf(string2, sizeof(string2),
		         "[SERVER] VISA: GBF=%s | DMM=%s\n",
		         addrGbf, addrDmm);

        if (strstr(req, "START"))
        {
            SetCtrlVal(panelHandle,PANEL_TEXTBOX,string);
            SetCtrlVal(panelHandle,PANEL_TEXTBOX,string2);
            RunSweepAndSend(handle, addrDmm, addrGbf, amp, fmin, fmax, nSteps);
        }
        else
        {
            ServerTCPWrite(handle, "ERR\n", 4, 2000);
        }
    }
    return 0;
}

/* ====== main / panel ====== */
int main(int argc, char *argv[])
{
    if (!InitCVIRTE(0, argv, 0)) return -1;

    panelHandle = LoadPanel(0, "serveur.uir", PANEL);
    if (panelHandle < 0) return -1;

    int err = RegisterTCPServer(SERVER_PORT, TcpServerCB, NULL);
    if (err < 0) MessagePopup("TCP Server", GetTCPErrorString(err));
    else { g_serverRunning = 1; printf("[SERVER] Démarré port %d\n", SERVER_PORT); }

    DisplayPanel(panelHandle);
    RunUserInterface();
    DiscardPanel(panelHandle);
    return 0;
}

int CVICALLBACK cbPanel(int panel, int event, void *cb, int e1, int e2)
{
    if (event == EVENT_CLOSE)
    {
        if (g_serverRunning) UnregisterTCPServer(SERVER_PORT);
        CloseVisa();
        QuitUserInterface(0);
    }
    return 0;
}
