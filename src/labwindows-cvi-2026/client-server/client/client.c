#include <cvirte.h>
#include <userint.h>
#include <ansi_c.h>
#include <utility.h>
#include <tcpsupp.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "Client.h"

#define SERVER_PORT 5000
#define SERVER_IP   "127.0.0.1"

/* globals */
static int panelHandle = 0;
static unsigned int g_tcpHandle = 0;
static int g_connected = 0;

/* ===================== UI log (chatbox) ===================== */
static void LogUI(const char *fmt, ...)
{
    char oldText[14546] = {0};
    char newLine[2048]  = {0};
    char out[8700]     = {0};

    va_list args;
    va_start(args, fmt);
    vsnprintf(newLine, sizeof(newLine), fmt, args);
    va_end(args);

    printf("[CLIENT] %s\n", newLine);

    if (panelHandle > 0)
    {

        GetCtrlVal(panelHandle, PANEL_TEXTBOX, oldText);
        snprintf(out, sizeof(out), "%s%s\r\n", oldText, newLine);
        SetCtrlVal(panelHandle, PANEL_TEXTBOX, out);

        /* scroll bas */
        SetCtrlAttribute(panelHandle, PANEL_TEXTBOX, ATTR_TEXT_SELECTION_START, (int)strlen(out));
    }
}

/* ===================== TCP read line (robuste) ===================== */
/* Lit 1 ligne terminée par '\n'. Gère le fait que TCP peut renvoyer plusieurs lignes d'un coup. */

static int TcpReadLine(unsigned int handle, char *out, int outSize)
{
    int total = 0;
    double t0 = Timer();

    while (total < outSize - 1)
    {
        char c = 0;
        int r = ClientTCPRead(handle, &c, 1, 2000); // timeout 2s
        if (r <= 0) break;

        out[total++] = c;
        if (c == '\n') break;

        if ((Timer() - t0) > 10.0) break; // sécurité
    }

    out[total] = '\0';
    return total;
}



static int TcpSendAll(unsigned int handle, const char *msg)
{
    int len = (int)strlen(msg);
    int sent = ClientTCPWrite(handle, msg, len, 3000);
    return (sent < 0) ? -1 : 0;
}

/* ===================== main ===================== */
int main (int argc, char *argv[])
{
    if (!InitCVIRTE(0, argv, 0))
        return -1;

    panelHandle = LoadPanel(0, "client.uir", PANEL);
    if (panelHandle < 0)
        return -1;

    /* vider chatbox au démarrage */
    SetCtrlVal(panelHandle, PANEL_TEXTBOX, "");

    DisplayPanel(panelHandle);
    RunUserInterface();
    DiscardPanel(panelHandle);
    return 0;
}

int CVICALLBACK cbPanel(int panel, int event, void *callbackData, int eventData1, int eventData2)
{
    if (event == EVENT_CLOSE)
    {
        if (g_connected && g_tcpHandle != 0)
        {
            DisconnectFromTCPServer(g_tcpHandle);
            g_tcpHandle = 0;
            g_connected = 0;
        }
        QuitUserInterface(0);
    }
    return 0;
}

/* ===================== Connexion ===================== */
int CVICALLBACK cbConnexion(int panel, int control, int event,
                            void *callbackData, int eventData1, int eventData2)
{
    if (event != EVENT_COMMIT) return 0;

    if (!g_connected)
    {
        LogUI("Connexion a %s:%d ...", SERVER_IP, SERVER_PORT);

        int err = ConnectToTCPServer(&g_tcpHandle, SERVER_PORT, SERVER_IP, NULL, NULL, 5000);
        if (err < 0)
        {
            MessagePopup("Connexion", GetTCPErrorString(err));
            LogUI("ECHEC connexion: %s", GetTCPErrorString(err));
            g_tcpHandle = 0;
            g_connected = 0;
            return 0;
        }

        g_connected = 1;
        SetCtrlAttribute(panel, PANEL_COMMANDBUTTON_CONN, ATTR_LABEL_TEXT, "Deconnexion");
        LogUI("Connecte.");
    }
    else
    {
        DisconnectFromTCPServer(g_tcpHandle);
        g_tcpHandle = 0;
        g_connected = 0;
        SetCtrlAttribute(panel, PANEL_COMMANDBUTTON_CONN, ATTR_LABEL_TEXT, "Connexion");
        LogUI("Deconnecte.");
    }
    return 0;
}

/* ===================== Test ===================== */
int CVICALLBACK cbTest(int panel, int control, int event,
                       void *callbackData, int eventData1, int eventData2)
{
    if (event != EVENT_COMMIT) return 0;

    if (!g_connected)
    {
        MessagePopup("Erreur", "Connecte-toi d'abord au serveur.");
        return 0;
    }

    double amp = 0.0, fmin = 0.0, fmax = 0.0;
    double nSteps_d = 0.0;
    int nSteps = 0;

    /* Tes IDs actuels */
    GetCtrlVal(panel, PANEL_NUMERICDIAL,   &amp);
    GetCtrlVal(panel, PANEL_NUMERICDIAL_3, &fmin);
    GetCtrlVal(panel, PANEL_NUMERICDIAL_4, &fmax);
    GetCtrlVal(panel, PANEL_NUMERICDIAL_2, &nSteps_d);
    nSteps = (int)(nSteps_d + 0.5);

    if (amp <= 0.0 || fmin <= 0.0 || fmax <= fmin || nSteps < 2)
    {
        MessagePopup("Parametres invalides", "Verifie amp>0, fmin>0, fmax>fmin, nSteps>=2.");
        return 0;
    }

    char req[256];
    snprintf(req, sizeof(req),
             "AMP=%.6f\nFMIN=%.6f\nFMAX=%.6f\nNSTEPS=%d\nSTART\n",
             amp, fmin, fmax, nSteps);

    LogUI("Envoi requete des données amplitude,fmin,fmax;nombre_pas...");
    if (TcpSendAll(g_tcpHandle, req) != 0)
    {
        MessagePopup("Erreur", "Envoi TCP echoue.");
        LogUI("Erreur envoi TCP.");
        return 0;
    }

    /* Lire OK (timeout 6s) */
    char line[512];
    LogUI("Attente OK...");
    int n = TcpReadLine(g_tcpHandle, line, sizeof(line));
    if (n <= 0)
    {
        MessagePopup("Erreur", "Timeout: pas de reponse du serveur.");
        LogUI("Timeout: serveur ne repond pas.");
        return 0;
    }

    LogUI("Recu: %s", line);

    if (strncmp(line, "OK", 2) != 0)
    {
        MessagePopup("Serveur", line);
        LogUI("Serveur a repondu erreur (pas OK).");
        return 0;
    }

    double *freqs = (double*)malloc(nSteps * sizeof(double));
    double *gains = (double*)malloc(nSteps * sizeof(double));
    if (!freqs || !gains)
    {
        free(freqs); free(gains);
        MessagePopup("Erreur", "Memoire insuffisante.");
        return 0;
    }

    int count = 0;
    LogUI("Reception points...");

    while (1)
    {
        n = TcpReadLine(g_tcpHandle, line, sizeof(line));
        if (n <= 0)
        {
            LogUI("Timeout pendant reception.");
            break;
        }

        if (strncmp(line, "END", 3) == 0)
        {
            LogUI("Tous les points ont ete recus.");
            break;
        }

        double f = 0.0, g = 0.0;

		
        if (sscanf(line, "%lf;%lf", &f, &g) == 2)
        {
            if (count < nSteps)
            {
                freqs[count] = f;
                gains[count] = g;
                count++;
            }
        }
		
    }

	if (count > 0)
    {

		double gainMax = gains[0];
		for (int i = 1; i < count; ++i)
		    if (gains[i] > gainMax) gainMax = gains[i];

		double seuil = gainMax - 3.0;

		double fc = freqs[count-1];
		double gout_fc = gains[count-1];

		for (int i = 0; i < count; ++i)
		    {
		        if (gains[i] <= seuil)
		        {
		            fc = freqs[i];
					gout_fc= gains[i];
		            break;
		        }
		    }
		
		
      	 double Vin_rms = amp / (2.0 * sqrt(2.0));
    	if (Vin_rms < 1e-12) Vin_rms = 1e-12;

  		  double vout_fc_rms = Vin_rms * pow(10.0, gout_fc / 20.0);


        /* Affichage des résultats */
        LogUI("Frequence_coupure = %.2f\nGain_fc =%.2f\nVout_fc=%.2f\n ",fc,gout_fc,vout_fc_rms);
   
    if (count < 2)
    {
        free(freqs); free(gains);
        MessagePopup("Erreur", "Pas assez de points reçus.");
        LogUI("Pas assez de points.");
        return 0;
    }

    DeleteGraphPlot(panel, PANEL_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    PlotXY(panel, PANEL_GRAPH,
           freqs, gains, count,
           VAL_DOUBLE, VAL_DOUBLE,
           VAL_THIN_LINE, VAL_EMPTY_SQUARE,
           VAL_SOLID, 1, VAL_RED);
	
	double fc_marker[1];
    double gain_marker[1];
    fc_marker[0] = fc;
    gain_marker[0] = gout_fc;
        
    PlotXY(panel, PANEL_GRAPH,
               fc_marker, gain_marker,
               1,
               VAL_DOUBLE, VAL_DOUBLE,
               VAL_SCATTER, VAL_SOLID_CIRCLE,
               VAL_SOLID, 10, VAL_GREEN); 
	
	 }
    LogUI("Trace OK (%d points).", count);
    free(freqs);
    free(gains);
    return 0;
}
