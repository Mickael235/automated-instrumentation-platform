#include <cvirte.h>
#include <userint.h>
#include "Intruvirtuelle.h"
#include <ansi_c.h>
#include <utility.h>
#include <stdlib.h>
#include <math.h>
#include <visa.h>

/* Sessions VISA globales */
static ViSession rm = VI_NULL;
static ViSession dmm = VI_NULL;      /* multimètre */
static ViSession gbf = VI_NULL;      /* générateur 33220A */

static int panelHandle = 0;

/* ----- Prototypes internes ----- */
static int OpenVisaInstruments(const char *addrDmm, const char *addrGbf);
static void CloseVisaInstruments(void);

/* ===================================================================== */
/*                               main()                                   */
/* ===================================================================== */

int main (int argc, char *argv[])
{
    if (InitCVIRTE (0, argv, 0) == 0)
        return -1;     /* out of memory */

    panelHandle = LoadPanel (0, "Intruvirtuelle.uir", PANEL);
    if (panelHandle < 0)
        return -1;

    DisplayPanel (panelHandle);
    RunUserInterface ();
    DiscardPanel (panelHandle);

    return 0;
}

/* ===================================================================== */
/*                          Callback du panneau                           */
/* ===================================================================== */

int CVICALLBACK cbPanel(int panel, int event, void *callbackData,
                        int eventData1, int eventData2)
{
    switch (event)
    {
        case EVENT_CLOSE:
            QuitUserInterface (0);
            break;
    }
    return 0;
}

/* ===================================================================== */
/*                      Callback de lancement du sweep                    */
/*   À attacher à un bouton "Lancer" ou "OK" dans le .uir                */
/* ===================================================================== */

int CVICALLBACK cbSweep(int panel, int control, int event,
                        void *callbackData, int eventData1, int eventData2)
{
    if (event != EVENT_COMMIT)
        return 0;

    double amplitude = 0.0;
    double fLow = 0.0, fHigh = 0.0;
    double nSteps = 0;
    char addrDmm[256] = {0};
    char addrGbf[256] = {0};

    GetCtrlVal(panel, PANEL_NUMERICKNOB,   &amplitude);      // Amplitude
    GetCtrlVal(panel, PANEL_NUMERICKNOB_2, &nSteps);         // Nombre de pas
    GetCtrlVal(panel, PANEL_NUMERICKNOB_3, &fLow);           // Fréquence basse
    GetCtrlVal(panel, PANEL_NUMERICKNOB_4, &fHigh);          // Fréquence haute
    GetCtrlVal(panel, PANEL_STRING,        addrDmm);         // Adresse Multimètre
    GetCtrlVal(panel, PANEL_STRING_2,      addrGbf);         // Adresse GBF

    if (nSteps < 2 || fHigh <= fLow || amplitude <= 0.0)
    {
        MessagePopup("Erreur paramètres",
                     "Vérifiez: amplitude > 0, nSteps >= 2 et f_high > f_low.");
        return 0;
    }

    /* 2. Ouvrir les instruments VISA */
    if (OpenVisaInstruments(addrDmm, addrGbf) != 0)
    {
        MessagePopup("Erreur", "Impossible d'ouvrir les instruments VISA.");
        return 0;
    }


    double Vin_rms = amplitude / (2.0 * sqrt(2.0));
    if (Vin_rms <= 0.0) 
        Vin_rms = 1e-9;


    DeleteGraphPlot(panel, PANEL_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    
	
    int nStepsInt = (int)nSteps;
    double *freqArray  = (double*)malloc(nStepsInt * sizeof(double));
    double *gainArray  = (double*)malloc(nStepsInt * sizeof(double));

    if (!freqArray || !gainArray)
    {
        MessagePopup("Erreur", "Mémoire insuffisante.");
        CloseVisaInstruments();
        if (freqArray) free(freqArray);
        if (gainArray) free(gainArray);
        return 0;
    }

    /* 4. Configuration initiale du GBF */
    viPrintf(gbf, "OUTP:LOAD INF\n");           // Haute impédance
    viPrintf(gbf, "FUNC SIN\n");                // Forme sinusoïdale
    viPrintf(gbf, "VOLT:OFFS 0\n");             // Offset à 0V
    viPrintf(gbf, "OUTP ON\n");                 // Activer la sortie

    /* 5. Boucle de sweep en fréquence */
 
    int nValid = 0;
    
    for (int i = 0; i < nStepsInt; ++i)
    {

        double f = fLow*pow(fHigh/fLow,(double)i/(nStepsInt - 1));
        double vOut = 0.0;
        ViStatus status;

        freqArray[i] = f;

        status = viPrintf(gbf, "FREQ %.6f\n", f);
        if (status < VI_SUCCESS)
        {
            char msg[256];
            sprintf(msg, "Erreur GBF FREQ à f=%.1f Hz : 0x%08X", f, (unsigned int)status);
            MessagePopup("Erreur VISA", msg);
            break;
        }

        status = viPrintf(gbf, "VOLT %.lf\n", amplitude);
        if (status < VI_SUCCESS)
        {
            char msg[256];
            sprintf(msg, "Erreur GBF VOLT à f=%.1f Hz : 0x%08X", f, (unsigned int)status);
            MessagePopup("Erreur VISA", msg);
            break;
        }

        SetCtrlVal(panel, PANEL_RINGMETER, (int)f);
        ProcessSystemEvents();

        Delay(0.2);

        status = viQueryf(dmm, "MEAS:VOLT:AC?\n", "%lf", &vOut);
        if (status < VI_SUCCESS)
        {
            char msg[256];
            sprintf(msg, "Erreur mesure DMM à f=%.1f Hz : 0x%08X", f, (unsigned int)status);
            MessagePopup("Erreur VISA", msg);
            break;
        }
		
        if (vOut > 1e-9 && Vin_rms > 1e-9)
        {
            gainArray[i] = 20.0 * log10(vOut / Vin_rms);
        }
     
        nValid++;
        ProcessSystemEvents();
    }

    viPrintf(gbf, "OUTP OFF\n");

    if (nValid > 0)
    {

		double gainMax = gainArray[0];
		for (int i = 1; i < nValid; ++i)
		    if (gainArray[i] > gainMax) gainMax = gainArray[i];

		double seuil = gainMax - 3.0;

		double fc = freqArray[nValid-1];
		double gout_fc = gainArray[nValid-1];

		for (int i = 0; i < nValid; ++i)
		    {
		        if (gainArray[i] <= seuil)
		        {
		            fc = freqArray[i];
					gout_fc= gainArray[i];
		            break;
		        }
		    }
		
		
	
        /* 8. Tension de sortie à fc (RMS) */
        double vout_fc_rms = Vin_rms * pow(10.0, gout_fc / 20.0);

        /* Affichage des résultats */
        char buf[64];

        sprintf(buf, "%.2f", fc);
        SetCtrlVal(panel, PANEL_STRING_3, buf);         // Fréquence de coupure

        sprintf(buf, "%.2f", gout_fc);
        SetCtrlVal(panel, PANEL_STRING_4, buf);         // Gain à fc

        sprintf(buf, "%.3f", vout_fc_rms);
        SetCtrlVal(panel, PANEL_STRING_5, buf);         // Tension sortie à fc
        
        /*Tracer la courbe G(f) sur le graphe avec marqueur de fc */
        PlotXY(panel, PANEL_GRAPH,
               freqArray, gainArray,
               nValid,
               VAL_DOUBLE, VAL_DOUBLE,
               VAL_THIN_LINE, VAL_EMPTY_SQUARE,
               VAL_SOLID, 1, VAL_WHITE);
        
        /* Ajouter un marqueur rouge à la fréquence de coupure */
        double fc_marker[1];
        double gain_marker[1];
        fc_marker[0] = fc;
        gain_marker[0] = gout_fc;
        
        PlotXY(panel, PANEL_GRAPH,
               fc_marker, gain_marker,
               1,
               VAL_DOUBLE, VAL_DOUBLE,
               VAL_SCATTER, VAL_SOLID_CIRCLE,
               VAL_SOLID, 10, VAL_RED);  /* Point rouge, taille 10 */
        
        /* Ajouter une annotation textuelle "fc = XXX Hz" */
        char annotation[64];
        sprintf(annotation, "fc = %.1f Hz", fc);
        PlotText(panel, PANEL_GRAPH, fc, gout_fc + 0.5, annotation, 
                 VAL_APP_META_FONT, VAL_RED, VAL_TRANSPARENT);
    }
    else if (nValid > 0)
    {
        /* Si pas de calcul de fc mais des données valides, tracer quand même */
        PlotXY(panel, PANEL_GRAPH,
               freqArray, gainArray,
               nValid,
               VAL_DOUBLE, VAL_DOUBLE,
               VAL_THIN_LINE, VAL_EMPTY_SQUARE,
               VAL_SOLID, 1, VAL_WHITE);
    }

    /* Libération de la mémoire */
    free(freqArray);
    free(gainArray);

    /* 10. Fermer les instruments VISA */
    CloseVisaInstruments();

    MessagePopup("Sweep terminé", "L'acquisition de la réponse en fréquence est terminée.");

    return 0;
}

/* ===================================================================== */
/*                  Fonctions internes d'ouverture VISA                   */
/* ===================================================================== */

static int OpenVisaInstruments(const char *addrDmm, const char *addrGbf)
{
    ViStatus status;

    /* Ouvrir le Resource Manager */
    if (viOpenDefaultRM(&rm) < VI_SUCCESS)
    {
        MessagePopup("Erreur VISA", "Impossible d'ouvrir le Resource Manager VISA.");
        return -1;
    }

    /* Ouvrir le GBF (générateur) */
    status = viOpen(rm, (ViRsrc)addrGbf, VI_NULL, VI_NULL, &gbf);
    if (status < VI_SUCCESS)
    {
        char msg[512];
        sprintf(msg, "Impossible d'ouvrir le GBF à l'adresse:\n%s\n\nCode erreur: 0x%08X\n\nVérifiez l'adresse et la connexion.", 
                addrGbf, (unsigned int)status);
        MessagePopup("Erreur VISA GBF", msg);
        viClose(rm);
        rm = VI_NULL;
        return -1;
    }

    /* Ouvrir le multimètre */
    status = viOpen(rm, (ViRsrc)addrDmm, VI_NULL, VI_NULL, &dmm);
    if (status < VI_SUCCESS)
    {
        char msg[512];
        sprintf(msg, "Impossible d'ouvrir le multimètre à l'adresse:\n%s\n\nCode erreur: 0x%08X\n\nVérifiez l'adresse et la connexion.", 
                addrDmm, (unsigned int)status);
        MessagePopup("Erreur VISA Multimètre", msg);
        viClose(gbf);
        viClose(rm);
        gbf = VI_NULL;
        rm  = VI_NULL;
        return -1;
    }

    /* Configuration des timeouts (5 secondes) */
    viSetAttribute(gbf, VI_ATTR_TMO_VALUE, 5000);
    viSetAttribute(dmm, VI_ATTR_TMO_VALUE, 5000);

    /* Test de communication avec les instruments */
    char idn[256];
    
    /* Test GBF */
    status = viQueryf(gbf, "*IDN?\n", "%t", idn);
    if (status < VI_SUCCESS)
    {
        MessagePopup("Avertissement", "Le GBF ne répond pas à la commande *IDN?");
    }

    /* Test Multimètre */
    status = viQueryf(dmm, "*IDN?\n", "%t", idn);
    if (status < VI_SUCCESS)
    {
        MessagePopup("Avertissement", "Le multimètre ne répond pas à la commande *IDN?");
    }

    return 0;
}

static void CloseVisaInstruments(void)
{
    /* Désactiver la sortie du GBF avant de fermer */
    if (gbf != VI_NULL)
    {
        viPrintf(gbf, "OUTP OFF\n");
    }

    /* Fermer les sessions dans l'ordre inverse */
    if (dmm  != VI_NULL) 
    { 
        viClose(dmm);  
        dmm = VI_NULL; 
    }
    
    if (gbf  != VI_NULL) 
    { 
        viClose(gbf);  
        gbf = VI_NULL; 
    }
    
    if (rm   != VI_NULL) 
    { 
        viClose(rm);   
        rm  = VI_NULL; 
    }
}