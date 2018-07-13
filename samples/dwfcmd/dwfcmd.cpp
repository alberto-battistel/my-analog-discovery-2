/************************************************************************/
/*                                                                      */
/*  dwfcmd.cpp  --  Digilent WaveForms SDK utility application          */
/*                                                                      */
/************************************************************************/
/*  Author: Kovacs L. Attila                                            */
/*  Copyright 2012, Digilent Inc.                                       */
/************************************************************************/
/*  File Description:                                                   */
/*                                                                      */
/*                                                                      */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*  2012/10/01(KovacsLA): created                                       */
/*                                                                      */
/************************************************************************/

#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_WARNINGS

/* ------------------------------------------------------------ */
/*              Include File Definitions                        */
/* ------------------------------------------------------------ */

#ifdef WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef WIN32
    #include <math.h>
#else
    #include <cmath>
#endif
#include <float.h>
#include <limits.h>
#include <iostream>

#ifndef WIN32
    #include <unistd.h>
    #include <sys/time.h>
    #define min(a,b) ((a<b)?a:b)
    #define max(a,b) ((a>b)?a:b)
#endif

#include "dwf.h"


/* ------------------------------------------------------------ */
/*              Local Type Definitions                          */
/* ------------------------------------------------------------ */

typedef enum Target : int {TargetDevice, TargetAnalogIn, TargetAnalogOut, TargetAnalogIO, TargetDigitalIO} ;

/* ------------------------------------------------------------ */
/*              Global Variables                                */
/* ------------------------------------------------------------ */

// Buffer for analog in record and analog out play
int cAnalogInSamples = 0;
double* rgdAnalogInData[4] = {NULL,NULL,NULL,NULL};
int rgcAnalogOutPlaySamples[4][3] = {{0,0,0},{0,0,0},{0,0,0},{0,0,0}};
double* rgdAnalogOutPlayData[4][3] = {{NULL,NULL,NULL},{NULL,NULL,NULL},{NULL,NULL,NULL},{NULL,NULL,NULL}};

HDWF hdwf = hdwfNone;
Target target = TargetDevice;
int idxChannel = 0;
AnalogOutNode idxNode = AnalogOutNodeCarrier;
char* rgszAnalogOutNode[3] = {"Carrier", "FM", "AM"};
char* rgszFunc[32] = {"DC","Sine","Square","Triangle","RampUp","RampDown","Noise", // 0..6
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown", // 7..14
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown", // 15..22
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown", //23..29
    "Custom","Play"}; // 30..31
char* rgszTrig[16] = {"None", "PC", "DetectorAnalogIn", "DetectorDigitalIn", 
    "AnalogIn", "DigitalIn", "DigitalOut", "AnalogOut1", "AnalogOut2", "AnalogOut3", "AnalogOut4", 
    "External1", "External2", "External3", "External4", "Unknown"};


double tsWatch = 10;
int debugLevel = 0;



/* ------------------------------------------------------------ */
/*              Local Variables                                 */
/* ------------------------------------------------------------ */

double tsWatchTimer;

/* ------------------------------------------------------------ */
/*              Forward Declarations                            */
/* ------------------------------------------------------------ */
void PrintDateTime(bool fDate);
bool FCommand(char* szarg);
bool FExecute(int argc, char* argv[], FILE* file);
bool FApi(const char * szApi, BOOL fResult);
void StartTimeout();
bool IsTimeout();
bool FPause(double tsPause);
bool FCmdMatch(char* szCmd, char* szParam);
bool FCmdInteger(char* szCmd, char* szParam, char* szUnit, int* pInteger);
bool FCmdInteger(char* szCmd, char* szParam, int* pInteger);
bool FCmdNumber(char* szCmd, char* szParam, char * szUnit, double* pNumber);
bool FCmdNumber(char* szCmd, char* szParam, double* pNumber);
bool FCmdText(char* szCmd, char* szParam, char** szArg);
char* NiceNumber(double dNumber, char* szUnits, char* szText);
char* NiceInteger(int dInteger, char* szUnits, char* szText);
char* NiceHex(unsigned int dHex, unsigned int dMask, char* szText);
bool FCsvRead(char* szFile, double** rgrgdata, int* pnColumns, int* pnSamples);
bool FAnalogInInitialize(HDWF h, int *pnChannels, int *pcEnabledChannels, int *pnSamples);
bool FAnalogOutInitialize(HDWF h, TRIGSRC trigsrcSync, int *pnChannels, int *pcEnabledChannels, int pidxPlaySamples[4][3]);
bool FAnalogInStart(HDWF h);
bool FAnalogInFinish(HDWF h);
bool FAnalogInSave(HDWF h, int idxchannel, char* szFile);
bool FAnalogOutLoadCustomFile(HDWF h, int idxchannel, int idxnode, char* szFile);
bool FAnalogOutLoadPlayFile(HDWF h, int idxchannel, int idxnode, char* szFile);
bool FAnalogOutStart(HDWF h, int idxchannel);
bool FAnalogOutFinish(HDWF h, int idxchannel);
bool FAnalogOutStop(HDWF h, int idxchannel);
bool FAnalogInRecord(HDWF h);
bool FAnalogOutPlay(HDWF h, int idxchannel, int idxnode);
bool FAnalogOutStart(HDWF h);
bool FAnalogStartAll(HDWF h);
bool FInfo(HDWF h, Target tar, int idxchannel);
bool FShow(HDWF h, Target tar, int idxchannel);
bool FEnum();
bool FDeviceByIndex(int idx, HDWF *phdwf);
bool FDeviceByName(char *szDevice, HDWF *phdwf);
bool FCommand(char* szarg);
bool FExecute(int argc, char* argv[], FILE* file);
/* ------------------------------------------------------------ */
/*              Procedure Definitions                           */
/* ------------------------------------------------------------ */


/* ------------------------------------------------------------ */
/***    PrintDateTime
**
**  Parameters:
**      fDate   - print date also
**
**  Return Value:
**      none
**
**  Errors:
**      none
**      
**  Description:
**      This function prints local time or date-time.
*/
void PrintDateTime(bool fDate){
    #ifdef WIN32
        SYSTEMTIME t;
        GetLocalTime(&t);
        if(fDate){
            printf("%i-%i-%i %i:%i:%i.%03i\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        }else {
            printf("%i:%i:%i.%03i\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        }
    #else
        time_t ti = time(NULL);
        struct tm t = *localtime(&ti);
        if(fDate){
            printf("%i-%i-%i %i:%i:%i\n", 1900+t.tm_year, 1+t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        }else {
            printf("%i:%i:%i\n", t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        }
    #endif
}

/* ------------------------------------------------------------ */
/***    FApi
**
**  Parameters:
**      szApi       - API name
**      fResult     - API result
**
**      debugLevel  - debug information print level
**
**  Return Value:
**      returns fResult parameter value
**
**  Errors:
**      none
**      
**  Description:
**      This function prints DWF error message in case of API 
**  failure. 
**  Depending on debug level prints API name and time.
**
*/

bool FApi(const char * szApi, BOOL fResult){
    if(debugLevel>=2) printf(">>%s\n", szApi);
    if(fResult == 0){
        char szError[512];
        FDwfGetLastErrorMsg(szError);
        printf("FAILED: %s\n%s\n", szApi, szError);
        return false;
    }
    if(debugLevel>=3) PrintDateTime(false);
    return true;
}

/* ------------------------------------------------------------ */
/***    TimeSec
**
**  Parameters:
**      none
**
**  Return Value:
**      time in seconds
**
**  Errors:
**      none
**      
**  Description:
**      This function returns time expressed in seconds.
*/
#ifdef WIN32
    double TimeSec(){
        return 0.001*GetTickCount();
    }
#else
    double TimeSec(){
        struct timeval tv;
        if(gettimeofday(&tv, NULL) != 0) return 0;

        return 1.0*tv.tv_sec + 0.000001*tv.tv_usec;
    }
#endif

/* ------------------------------------------------------------ */
/***    StartTimeout
**
**  Parameters:
**      none
**
**  Return Value:
**      none
**
**  Errors:
**      none
**      
**  Description:
**      This function starts the watch timer used in loops
**  that can last long.
**  
*/
void StartTimeout(){
    tsWatchTimer = TimeSec();
}

/* ------------------------------------------------------------ */
/***    IsTimeout
**
**  Parameters:
**      tsWatch - (global) timeout value
**
**  Return Value:
**      returns true when timeout
**
**  Errors:
**      none
**      
**  Description:
**      This function verifies timeout condition.
*/
bool IsTimeout(){
    if(tsWatch<=0) return false;

    if((TimeSec() - tsWatchTimer) > tsWatch){
        printf("\nWARNING: Watch timeout of %0.3lf seconds expired.\n", tsWatch);
        printf("Continue execution[Y/n]?");
        char sz[512];
        scanf("%s", &sz);
        if(sz[0]=='y' || sz[0]=='Y' || sz[0]=='\n'){
            StartTimeout();
            return false;
        }
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ */
/***    FPause
**
**  Parameters:
**      tsPause - pause time in seconds
**      tsWatch - (global) timeout value
**      tsWatchTimer - (global) timeout start value
**
**  Return Value:
**      returns false when cancelled
**
**  Errors:
**      none
**      
**  Description:
**      This function returns after the specified time expires.
**  At watch timeout intervals asks to continue or not.
*/
bool FPause(double tsPause){
    StartTimeout();
    if(tsWatch>0) {
        while(tsPause > tsWatch){
            #ifdef WIN32
                Sleep((int)(1000.0*min(tsPause, tsWatch)));
            #else
                usleep((int)(100000.0*min(tsPause, tsWatch)));
            #endif
            if(!IsTimeout()) return false;
            tsPause -= (TimeSec()-tsWatchTimer);
        }
    }else{
        #ifdef WIN32
            Sleep((int)(1000.0*tsPause));
        #else
            usleep((int)(100000.0*tsPause, tsWatch));
        #endif
    }
    if(tsPause>0){
        #ifdef WIN32
            Sleep((int)(1000.0*tsPause));
        #else
            usleep((int)(1000000.0*tsPause));
        #endif
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FCmdMatch
**
**  Parameters:
**      szCmd   - text to be compared
**      szParam - text to compare to
**
**  Return Value:
**      returns true when match
**
**  Errors:
**      none
**      
**  Description:
**      This function compares the two text arguments in case 
**  insensitive mode.
*/
bool FCmdMatch(char* szCmd, char* szParam){
    
    if(strlen(szCmd) != strlen(szParam)){
        return false;
    }

    for(unsigned int i = 0; i < strlen(szParam); i++) {
        if(tolower(szCmd[i]) != tolower(szParam[i])){
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FCmdInteger
**
**  Parameters:
**      szCmd       - text to be compared
**      szParam     - text to compare to
**      szUnit      - optional unit text
**      pInteger    - argument value
**
**  Return Value:
**      returns true when first argument starts with second one 
**  and is followed by an integer number (hex or integer)
**
**  Errors:
**      none
**      
**  Description:
**      This function decodes <parameter=><integer><G|M|k|><unit>.
*/
bool FCmdInteger(char* szCmd, char* szParam, char* szUnit, int* pInteger){
    // 
    if(strlen(szCmd) < strlen(szParam)){
        return false;
    }

    // verify if command starts with the parameter
    for(unsigned int i = 0; i < strlen(szParam); i++, szCmd++) {
        if(tolower(szCmd[0]) != tolower(szParam[i])){
            return false;
        }
    }

    char szCmd2[512];
    strcpy(szCmd2, szCmd);

    // optional check and remove ending unit text
    if(szUnit != NULL && strlen(szCmd2) > strlen(szUnit)){
        for(int i = strlen(szUnit)-1; i >= 0; i--) {
            if(tolower(szCmd2[strlen(szCmd2)-strlen(szUnit)+i]) != tolower(i)){
                break;
            }
            if(i==0){
                szCmd2[strlen(szCmd2)-strlen(szUnit)] = 0;
            }
        }
    }

    // hex number
    if(strlen(szCmd2)>=2 && szCmd2[0]=='0' && tolower(szCmd2[1])=='x'){
        if(sscanf(szCmd2+2, "%X", pInteger)==1){
            return true;
        }
    }
    
    { // try decoding with scale notation
        char c = 0;
        if(sscanf(szCmd2, "%i%c", pInteger, &c) == 2){
            if(c=='G') *pInteger *= 1000000000;
            else if(c=='M') *pInteger *= 1000000;
            else if(c=='k') *pInteger *= 1000;
            else if(c=='m') *pInteger /= 1000;
            else if(c=='u') *pInteger /= 1000000;
            else if(c=='n') *pInteger /= 1000000000;
            else return false;
            return true;
        }
    }

    // too long to be number
    if(strlen(szCmd2)>8){
        return false;
    }

    { // try decoding with as an integer
        if(sscanf(szCmd2, "%i", pInteger) == 1){
            return true;
        }
    }

    return false;
}
bool FCmdInteger(char* szCmd, char* szParam, int* pInteger){
    return FCmdInteger(szCmd, szParam, NULL, pInteger);
}

/* ------------------------------------------------------------ */
/***    FCmdNumber
**
**  Parameters:
**      szCmd   - text to be compared
**      szParam - text to compare to
**      szUnit  - optional unit text
**      pNumber - argument value
**
**  Return Value:
**      returns true when first argument starts with second one 
**  and is followed by a number (hex, floating point or integer)
**
**  Errors:
**      none
**      
**  Description:
**      This function decodes <parameter=><number><G|M|k| |m|u|n><units>.
*/

bool FCmdNumber(char* szCmd, char* szParam, char * szUnit, double* pNumber){
    // 
    if(strlen(szCmd) < strlen(szParam)){
        return false;
    }

    // verify if command starts with the parameter
    for(unsigned int i = 0; i < strlen(szParam); i++, szCmd++) {
        if(tolower(szCmd[0]) != tolower(szParam[i])){
            return false;
        }
    }

    char szCmd2[512];
    strcpy(szCmd2, szCmd);

    // optional check and remove ending unit text
    if(szUnit != NULL && strlen(szCmd2) > strlen(szUnit)){
        for(int i = strlen(szUnit)-1; i >= 0; i--) {
            if(tolower(szCmd2[strlen(szCmd2)-strlen(szUnit)+i]) != tolower(szUnit[i])){
                break;
            }
            if(i==0){
                szCmd2[strlen(szCmd2)-strlen(szUnit)] = 0;
            }
        }
    }

    // hex number
    if(strlen(szCmd2)>=2 && szCmd2[0]=='0' && tolower(szCmd2[1])=='x'){
        int i;
        if(sscanf(szCmd2+2, "%X", &i)==1){
            *pNumber = i;
            return true;
        }
    }
    
    { // try decoding with scale notation
        char c = 0;
        if(sscanf(szCmd2, "%lf%c", pNumber, &c) == 2){
            if(c=='G') *pNumber *= 1000000000.0;
            else if(c=='M') *pNumber *= 1000000.0;
            else if(c=='k') *pNumber *= 1000.0;
            else if(c=='m') *pNumber /= 1000.0;
            else if(c=='u') *pNumber /= 1000000.0;
            else if(c=='n') *pNumber /= 1000000000.0;
            else return false;
            return true;
        }
    }
    { // try decoding with as an number
        if(sscanf(szCmd2, "%lf", pNumber) == 1){
            return true;
        }
    }

    { // try decoding with scale notation
        char c = 0;
        int i = 0;
        if(sscanf(szCmd2, "%i%c", &i, &c) == 2){
            *pNumber = i;
            if(c=='G') *pNumber *= 1000000000.0;
            else if(c=='M') *pNumber *= 1000000.0;
            else if(c=='k') *pNumber *= 1000.0;
            else if(c=='m') *pNumber /= 1000.0;
            else if(c=='u') *pNumber /= 1000000.0;
            else if(c=='n') *pNumber /= 1000000000.0;
            else return false;
            return true;
        }
    }
    { // try decoding with as an integer
        int i = 0;
        if(sscanf(szCmd2, "%i", &i) == 1){
            *pNumber = i;
            return true;
        }
    }

    return false;
}
bool FCmdNumber(char* szCmd, char* szParam, double* pNumber){
    return FCmdNumber(szCmd, szParam, NULL, pNumber);
}

/* ------------------------------------------------------------ */
/***    FCmdText
**
**  Parameters:
**      szCmd       - text to be compared
**      szParam     - parameter text to match
**      szArg       - pointer to return argument start
**
**  Return Value:
**      returns true on match
**
**  Errors:
**      none
**      
**  Description:
**      This function decodes <parameter=><argument>.
*/
bool FCmdText(char* szCmd, char* szParam, char** szArg){
    // 
    if(strlen(szCmd) < strlen(szParam)){
        return false;
    }

    // verify if command starts with the parameter
    for(unsigned int i = 0; i < strlen(szParam); i++, szCmd++) {
        if(tolower(szCmd[0]) != tolower(szParam[i])){
            return false;
        }
    }
    *szArg = szCmd;
    return true;
}

/* ------------------------------------------------------------ */
/***    NiceNumber
**
**      dInteger    - integer value
**      szUnits     - unit text
**      szText      - char buffer to print
**
**  Return Value:
**      szText
**
**  Errors:
**      none
**      
**  Description:
**      This function prints numbers with scale 
**  parameter=<number><G|M|k||m|u|n><units>.
*/

char* NiceNumber(double dNumber, char* szUnits, char* szText){
    double dScale = 1;
    char* szScale = "";
    if(fabs(dNumber)>=1000000000)      {szScale="G"; dScale = 1000000000;}
    else if(fabs(dNumber)>=1000000)    {szScale="M"; dScale = 1000000;}
    else if(fabs(dNumber)>=1000)       {szScale="k"; dScale = 1000;}
    else if(fabs(dNumber)>=1)          {szScale="";  dScale = 1;}
    else if(fabs(dNumber)>=0.001)      {szScale="m"; dScale = 0.001;}
    else if(fabs(dNumber)>=0.000001)   {szScale="u"; dScale = 0.000001;}
    else if(fabs(dNumber)>=0.000000001){szScale="n"; dScale = 0.000000001;}
    double dPrintNumber = dNumber / dScale;

    if(dNumber == floor(0.5+dPrintNumber)*dScale) sprintf(szText, "%.0lf%s%s", dPrintNumber, szScale, szUnits);
    else if(fabs(dPrintNumber)>=100) sprintf(szText, "%.1lf%s%s", dPrintNumber, szScale, szUnits);
    else if(fabs(dPrintNumber)>=10) sprintf(szText, "%.2lf%s%s", dPrintNumber, szScale, szUnits);
    else sprintf(szText, "%.3lf%s%s", dPrintNumber, szScale, szUnits);
    return szText;
}

/* ------------------------------------------------------------ */
/***    NiceInteger
**
**  Parameters:
**      dInteger    - integer value
**      szUnits     - unit text
**      szText      - char buffer to print
**
**  Return Value:
**      szText
**
**  Errors:
**      none
**      
**  Description:
**      This function prints integer values with scale
**  <integer><G|M|k| ><units>.
*/

char* NiceInteger(int dInteger, char* szUnits, char* szText){
    int dScale = 1;
    char* szScale = "";
    if(abs(dInteger)>=1000000000)      {szScale="G"; dScale = 1000000000;}
    else if(abs(dInteger)>=1000000)    {szScale="M"; dScale = 1000000;}
    else if(abs(dInteger)>=1000)       {szScale="k"; dScale = 1000;}

    if(dScale==1 || (dInteger == (dInteger/dScale)*dScale)) sprintf(szText, "%i%s%s", dInteger/dScale, szScale, szUnits);
    else if(abs(dInteger/dScale)>=100) sprintf(szText, "%.1lf%s%s", 1.0*dInteger/dScale, szScale, szUnits);
    else sprintf(szText, "%0.2lf%s%s", 1.0*dInteger/dScale, szScale, szUnits);
    return szText;
}

/* ------------------------------------------------------------ */
/***    NiceHex
**
**  Parameters:
**      dHex    - integer value
**      dMask   - mask
**      szText  - char buffer to print
**
**  Return Value:
**      szText
**
**  Errors:
**      none
**      
**  Description:
**      This function prints hexadecimal value.
*/

char* NiceHex(unsigned int dHex, unsigned int dMask, char* szText){
    int nBits = 31;
    for(; nBits >= 0; nBits--){
        if(dMask & (1<<nBits)){
            nBits+=4;
	    nBits/=4;
            break;
        }
    }
    char sz[32];
    sprintf(sz, "0x%%0%iX", nBits);
    sprintf(szText, sz, dHex);
    return szText;
}

/* ------------------------------------------------------------ */
/***    FCsvRead
**
**  Parameters:
**      szFile      - path to file (in)
**      rgrgdata    - matrix (in-out)
**      pcCol       - columns (in-out)
**      pcRow       - rows (in-out)
**
**  Return Value:
**      returns true when succeeds
**
**  Errors:
**      none
**      
**  Description:
**      This function reads CSV file to matrix.
**  With matrix argument NULL, returns the CSV file parameters.
*/
bool FCsvRead(char* szFile, double** rgrgdata, int* pnColumns, int* pnSamples){

    FILE* file = fopen(szFile, "r");
    if(!file) {
        printf("\nERROR: Reading file: \"%s\"\n", szFile);
        return false;
    }
    char szLine[256];
    int nCol = 0;

    // Parse file to find out how many columns and rows does it contain.
    for(int i = 0; i < *pnSamples;){
        // End of file? 
        if(fgets(szLine, 255, file)==NULL){
            // Return number of read rows.
            *pnSamples = i;
            break;
        }
        // Try to decode line.
        int c = 0;
        for(char* szNext = szLine; szNext != NULL && c < *pnColumns; c++, szNext++){
            if(rgrgdata) rgrgdata[c][i] = strtod(szNext, &szNext);
            else strtod(szNext, &szNext);
        }
        if(c==0) continue;
        nCol = max(c, nCol);
        i++;
    }
    fclose(file);
    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogInInitialize
**
**  Parameters:
**      h                   - handle to opened device
**      pnChannels          - number of analog in channels
**      pcEnabledChannels   - count of enabled channels
**      pnSamples           - number of samples to acquire
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      Initializes AnalogIn channels, allocating PC buffer for 
**  acquisition samples. Returns number of AnalogIn channels, 
**  number of enabled channels and number of samples to acquire.
** 
*/
bool FAnalogInInitialize(HDWF h, int *pnChannels, int *pcEnabledChannels, int *pnSamples){
    int nChannels = 0;
    int cEnabledChannels = 0;
    int nSamples = 0;

    if(!FApi("FDwfAnalogInChannelCount", FDwfAnalogInChannelCount(h, &nChannels))) return false;

    ACQMODE acqmode;
    if(!FApi("FDwfAnalogInAcquisitionModeGet", FDwfAnalogInAcquisitionModeGet(h, &acqmode))) return false;

    double hzFreq;
    double secRun;
    if(!FApi("FDwfAnalogInFrequencyGet", FDwfAnalogInFrequencyGet(h, &hzFreq))) return false;

    if(acqmode == acqmodeSingle){
        if(!FApi("FDwfAnalogInBufferSizeGet", FDwfAnalogInBufferSizeGet(h, &nSamples))) return false;
        secRun = nSamples/hzFreq;
    }else if(acqmode == acqmodeRecord){
        if(!FApi("FDwfAnalogInRecordLengthGet", FDwfAnalogInRecordLengthGet(h, &secRun))) return false;
        double dSamples = secRun*hzFreq;
        if(dSamples<=0){
            printf("\nERROR: Please specify Run for AnalogIn.\n");
            printf("  example: analogin run=2s\n");
            return false;
        }else if(dSamples>100000000){
            printf("\nERROR: Number of record samples is above 100M. Reduce AnalogIn Frequency or Run.\n");
            printf("  example: analogin frequency=10kHz run=2s\n");
            return false;
        }
        nSamples = (int)dSamples;
    }else{
        printf("\nERROR: Unsupported acquisition mode.\n");
        return false;
    }

    if(nSamples!=cAnalogInSamples){
        for(int i = 0; i < 4; i++){
            if(rgdAnalogInData[i] != NULL) {
                delete[] rgdAnalogInData[i];
                rgdAnalogInData[i] = NULL;
            }
        }
    }

    cAnalogInSamples = nSamples;

    for(int i = 0; i < nChannels; i++){
        BOOL fEnable;
        if(!FApi("FDwfAnalogInChannelEnableGet", FDwfAnalogInChannelEnableGet(h, i, &fEnable))) {
            return false;
        }
        if(!fEnable) continue;
        
        if(rgdAnalogInData[i] == NULL) rgdAnalogInData[i] = new double[nSamples];
        
        cEnabledChannels++;
    }
    
    if(cEnabledChannels>0){
        // Print information.
        char szFreq[32];
        char szRun[32];
        char szSmp[32];
        if(acqmode == acqmodeSingle){
        
            printf("Starting AnalogIn single acquisition on %i channels at %s of %i samples, %s.\n", 
                cEnabledChannels, NiceNumber(hzFreq, "Hz", szFreq), nSamples, NiceNumber(secRun, "sec", szRun));
        }else{
            printf("Starting AnalogIn record on %i channels at %s of %s samples, %s.\n", 
                cEnabledChannels, NiceNumber(hzFreq, "Hz", szFreq), NiceNumber(nSamples, "", szSmp), NiceNumber(secRun, "sec", szRun));
        }
    }

    if(pnChannels) *pnChannels = nChannels;
    if(pcEnabledChannels) *pcEnabledChannels = cEnabledChannels;
    if(pnSamples) *pnSamples = nSamples;
    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogOutInitialize
**
**  Parameters:
**      h                   - handle to opened device
**      trigsrc             - trigger source to set if not set
**      pnChannels          - number of analog in channels
**      pcEnabledChannels   - count of enabled ones
**      pidxPlaySamples[4]  - player index to return the prefill
**                            sample count
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      Initializes AnalogOutput channels, checking if samples 
**  are loaded to play, prints information. If no trigger was set
**  sets it in order to start synchronized.
** 
*/
bool FAnalogOutInitialize(HDWF h, TRIGSRC trigsrcSync, int *pnChannels, int *pcEnabledChannels, int pidxPlaySamples[4][3]){

    int nChannels = 0;
    int cEnabledChannels = 0;
    int cPlayChannels = 0;

    // Read the number of AnalogOut channels.
    if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &nChannels))) return false;

    // Check how many are enabled, configure each one.
    for(int i = 0; i < nChannels && i < 4; i++){

        { // If channel is not enabled skip it
            BOOL fEnabled;
            if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, AnalogOutNodeCarrier, &fEnabled))) return false;
            if(!fEnabled) continue;
        }

        cEnabledChannels++;

        { // In order to start the channels synchrnously. When trigger source was not specified 
          // configure it to PC trigger. After each channel is configured start them at once.
            TRIGSRC trigsrc;
            if(!FApi("FDwfAnalogOutTriggerSourceGet", FDwfAnalogOutTriggerSourceGet(h, i, &trigsrc))) return false;
            if(trigsrc == trigsrcNone){
                if(!FApi("FDwfAnalogOutTriggerSourceSet", FDwfAnalogOutTriggerSourceSet(h, i, trigsrcSync))) return false;
            }
        }
        bool fplay = false;
        for(int j = 0; j < 3; j++){
            { // If node is not enabled skip it
                BOOL fEnabled;
                if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, j, &fEnabled))) return false;
                if(!fEnabled) continue;
            }
            FUNC func;
            if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(h, i, j, &func))) return false;
            if(func != funcPlay) continue;

            fplay = true;
            if((func != funcPlay) && (rgdAnalogOutPlayData[i][j]==NULL || rgcAnalogOutPlaySamples[i][j]<=0)){
                printf("\nERROR: No play samples are loaded for AnalogOut channel %i %s\n", i+1, rgszAnalogOutNode[j]);
                printf("  example: analogout channel=1 enable=1 play channel=2 ... channel=0 play=myfile.csv start\n", i+1);
                return false;
            }

            { // Check how for how long will the play run.
                double secRun;
                char szRun[32];
                if(!FApi("FDwfAnalogOutRunGet", FDwfAnalogOutRunGet(h, i, &secRun))) return false;

                if(secRun<=0){
                    printf("\nERROR: Please specify Run for AnalogOut channel %i.\n", i);
                    printf("  example: analogout channel=%i run=2s\n", i+1);
                    return false;
                }
                double hzFreq;
                if(!FApi("FDwfAnalogOutNodeFrequencyGet", FDwfAnalogOutNodeFrequencyGet(h, i, j, &hzFreq))) return false;
                int nSamples = (int)(secRun*hzFreq);
                printf("Starting AnalogOut channel %i %s to play for %s, %i samples.\n", i+1, rgszAnalogOutNode[j], NiceNumber(secRun, "sec", szRun), nSamples);
                
                if(rgcAnalogOutPlaySamples[i][j] != nSamples){
                    printf("WARNING: %i samples where loaded from file.\n", rgcAnalogOutPlaySamples[i][j]);
                }
            }
            // Check the device buffer size.
            int nBufferSize = 0;
            if(!FApi("FDwfAnalogOutNodeDataInfo", FDwfAnalogOutNodeDataInfo(h, i, j, NULL, &nBufferSize))) return false;

            // Prefill the device buffer with the first chunk of data samples.
            nBufferSize = min(nBufferSize, rgcAnalogOutPlaySamples[i][j]);
            if(pidxPlaySamples){
                pidxPlaySamples[i][j] = nBufferSize;
            }
            if(!FApi("FDwfAnalogOutNodeDataSet", FDwfAnalogOutNodeDataSet(h, i, j, rgdAnalogOutPlayData[i][j], nBufferSize))) return false;
        }
        if(!fplay){
            printf("Starting AnalogOut Channel %i\n", i+1);
        }

        if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, i, true))) return false;
    }

    if(pnChannels) *pnChannels = nChannels;
    if(pcEnabledChannels) *pcEnabledChannels = cEnabledChannels;
    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogInStart
**
**  Parameters:
**      h           - handle to opened device
**      channels    - number of channels
**      szFile      - path to file
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function starts single acquisition.
**  This type of acquisition can run up to maximum frequency 
**  because the data is entirely stored in the device buffer.
** 
*/
bool FAnalogInStart(HDWF h){

    int cAnalogInEnabledChannels = 0;
    
    if(!FAnalogInInitialize(h, NULL, &cAnalogInEnabledChannels, NULL)) return false;

    if(cAnalogInEnabledChannels <= 0){
        printf("\nERROR: No AnalogIn channel is enabled.\n");
        printf("example: analogin channel=1 enable start\n");
        return false;
    }

    // Start AnalogIn
    if(!FApi("FDwfAnalogInConfigure", FDwfAnalogInConfigure(h, true, true))) return false;

    printf("Call finish to wait for the acquisition to terminate.\n");

    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogInFinish
**
**  Parameters:
**      h   - handle to opened device
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function waits for AnalogIn acquisition to end and
**  saves the acquisition data to file. 
**  .
*/
bool FAnalogInFinish(HDWF h){

    // Print information.
    printf("Waiting to finish AnalogIn single acquisition.\n");

    // Wait AnalogIn to finish.
    STS sts = stsRdy;
    StartTimeout();
    do{
        if(IsTimeout()) return false;
        
        // Read the AnalogIN status and data from device.
        if(!FApi("FDwfAnalogInStatus", FDwfAnalogInStatus(h, 1, &sts))) return false;
        // Acquisition is running, not yet ended.
    }while(sts == stsCfg || sts == stsArm || sts == stsPrefill || sts == stsTrig);

    if(sts != stsDone){
        // AnalogIn is other state. Probably it was not even started
        printf("\nERROR: First start AnalogIn single acquisition: analogin channel=1 enable=1 ... start\n");
        return false;
    }

    // Get the acquisition samples of enabled channels and save in one file.
    for(int i = 0; i < 4; i++){
        if(rgdAnalogInData[i]==NULL) continue;
        if(!FApi("FDwfAnalogInStatusData", FDwfAnalogInStatusData(h, i, rgdAnalogInData[i], cAnalogInSamples))) return false;
    }
    printf("Call save to export data to file.\n");

    // Reconfigure device AnalogIn in order to clear done state.
    if(!FApi("FDwfAnalogInConfigure", FDwfAnalogInConfigure(h, true, false))) return false;
    return true;
}


/* ------------------------------------------------------------ */
/***    FAnalogInSave
**
**  Parameters:
**      h           - handle to opened device
**      idxchannel  - channel index
**      szFile      - path to file
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function saves AnalogIn acquisition to file.
** 
*/
bool FAnalogInSave(HDWF h, int idxchannel, char* szFile){
    int iChannel = idxchannel-1;
    int cChannel = 0;

    if(idxchannel>0){
        if(idxchannel>=4 || rgdAnalogInData[iChannel] == NULL){
            printf("\nERROR: No samples on AnalogIn Channel %i\n", idxchannel);
            return false;
        }
    }else{
        for(int c = 0; c < 4; c++){
            if(rgdAnalogInData[c]==NULL) continue;        
            cChannel++;
        }
        if(cChannel<=0){
            printf("\nERROR: No samples on any AnalogIn channel.\n");
            return false;
        }
    }
    
    FILE* f = fopen(szFile, "w");
    if(f == NULL){
        printf("\nERROR: Failed to open file: %s\n", szFile);
        return false;
    }

    if(idxchannel>0){
        for(int i = 0; i < cAnalogInSamples; i++){
            fprintf(f, "%lg\n", rgdAnalogInData[iChannel][i]);
        }
        printf("Saved AnalogIn Channel %i samples to file: \"%s\"\n", idxchannel, szFile);
    }else{
        for(int i = 0; i < cAnalogInSamples; i++){
            bool fFirst = true;
            for(int c = 0; c < 4; c++){
                if(rgdAnalogInData[c]==NULL) continue;
                if(!fFirst) fprintf(f, ",");
                fFirst = false;
                fprintf(f, "%lg", rgdAnalogInData[c][i]);
            }
            fprintf(f, "\n");
        }
        printf("Saved AnalogIn samples of %i channels to file: \"%s\"\n", cChannel, szFile);
    }

    fclose(f);
    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogOutCustomFile
**
**  Parameters:
**      h       - handle to opened device
**      idxchannel   - channel index
**      szFile  - path to file
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function loads custom samples for AnalogOut channel.
**  With idxchannel 0 will load sequential columns for each 
**  enabled channel.
** 
*/
bool FAnalogOutLoadCustomFile(HDWF h, int idxchannel, int idxnode, char* szFile) {

    
    if(idxchannel>0){
        // Load custom samples for one channel

        // Check CSV file column and row numbers, up to 1 column and 1M samples.
        int nCsvChannels = 1;
        int nCsvSamples = 1024*1024;
        if(!FCsvRead(szFile, NULL, &nCsvChannels, &nCsvSamples)) return false;
        
        if(nCsvChannels<1 || nCsvSamples<1){
            printf("\nERROR: Empty file? \"%s\"\n", szFile);
            return false;
        }
        double* rgdata = new double[nCsvSamples];

        // Read the data.
        if(!FCsvRead(szFile, &rgdata, &nCsvChannels, &nCsvSamples)) return false;
        
        // Set custom function.
        if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(h, idxchannel-1, idxnode, funcCustom))) return false;

        // Set custom samples.
        if(!FApi("FDwfAnalogOutNodeDataSet", FDwfAnalogOutNodeDataSet(h, idxchannel-1, idxnode, rgdata, nCsvSamples))) return false;
        
        { // Print information
            char szSamples[32];
            printf("Custom AnalogOut Channel %i loaded %s: \"%s\"\n", idxchannel, NiceInteger(nCsvSamples, " samples", szSamples), szFile);

            int nSamples = 0;
            if(!FApi("FDwfAnalogOutNodeDataInfo", FDwfAnalogOutNodeDataInfo(h, idxchannel-1, idxnode, NULL, &nSamples))) return false;
            if(nSamples<nCsvSamples){
                printf("\nWARNING: Samples where interpolated to the %s device buffer size.\n", NiceInteger(nCsvSamples, " samples", szSamples));
            }
        }
    }else{
        // Load consecutive columns from one file for custom AnalogOut channels.

        // Check how many AnalogOut channels does the device have.
        int nChannels = 0;
        if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &nChannels))) return false;
        
        // Check how many channels are configured for custom function.
        int nCustomChannels = 0;
        for(int i = 0; i < nChannels; i++){
            FUNC func;
            if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(h, i, idxnode, &func))) return false;
            if(func != funcCustom) continue;
            nCustomChannels++;
        }

        if(nCustomChannels==0){
            printf("\nERROR: No AnalogOut channel is configured for custom function.\n");
            return false;
        }

        // Check CSV file column and row numbers, up to custom channel count and 1M samples.
        int nCsvChannels = nCustomChannels;
        int nCsvSamples = 1024*1024;
        if(!FCsvRead(szFile, NULL, &nCsvChannels, &nCsvSamples)) return false;
        
        if(nCsvChannels<1 || nCsvSamples<1){
            printf("\nERROR: Empty file? \"%s\"\n", szFile);
            return false;
        }
        double** rgrgdata = new double*[nCsvChannels];
        for(int i = 0; i < nCsvChannels; i++){
            rgrgdata[i] = new double[nCsvSamples];
        }

        // Read the file.
        if(!FCsvRead(szFile, rgrgdata, &nCsvChannels, &nCsvSamples)) return false;
        
        for(int i = 0, iCsvChannel = 0; i < nChannels; i++){
            FUNC func;
            if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(h, i, idxnode, &func))) return false;
            if(func != funcCustom) continue;

            // Set custom samples
            if(!FApi("FDwfAnalogOutNodeDataSet", FDwfAnalogOutNodeDataSet(h, i, idxnode, rgrgdata[iCsvChannel%nCsvChannels], nCsvSamples))) return false;
            
            { // Print information
                char szSamples[32];
                printf("Custom AnalogOut Channel %i loaded %s samples %i. column from file: \"%s\"\n", 
                    i+1, NiceInteger(nCsvSamples, "", szSamples), iCsvChannel+1, szFile);

                int nSamples = 0;
                if(!FApi("FDwfAnalogOutNodeDataInfo", FDwfAnalogOutNodeDataInfo(h, i, idxnode, NULL, &nSamples))) return false;
                if(nSamples<nCsvSamples){
                    printf("\nWARNING: Samples where interpolated to the %s samples device buffer size.\n", NiceInteger(nCsvSamples, "", szSamples));
                }
            }

            iCsvChannel++;
        }
        for(int i = 0; i < nCsvChannels; i++){
            delete[] rgrgdata[i];
        }
        delete[] rgrgdata;
    }

    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogOutLoadPlayFile
**
**  Parameters:
**      h       - handle to opened device
**      idxchannel   - channel index
**      szFile  - path to file
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function loads custom samples for AnalogOut channel.
**  With idxchannel 0 will load sequential columns for each 
**  enabled channel.
** 
*/
bool FAnalogOutLoadPlayFile(HDWF h, int idxchannel, int idxnode, char* szFile) {

    int iChannel = idxchannel-1;
    
    if(idxchannel>0){
        // Load samples for one channel

        // Check CSV file column and row numbers, up to 1 column and 100M samples.
        int nCsvChannels = 1;
        int nCsvSamples = 100000000;
        if(!FCsvRead(szFile, NULL, &nCsvChannels, &nCsvSamples)) return false;
        
        if(nCsvChannels<1 || nCsvSamples<1){
            printf("\nERROR: Empty file? \"%s\"\n", szFile);
            return false;
        }
        
        if(rgdAnalogOutPlayData[iChannel][idxnode]!=NULL && rgcAnalogOutPlaySamples[iChannel][idxnode]!=nCsvSamples) {
            delete[] rgdAnalogOutPlayData[iChannel][idxnode];
            rgdAnalogOutPlayData[iChannel][idxnode] = NULL;
        }
        if(rgdAnalogOutPlayData[iChannel][idxnode]==NULL){
            rgdAnalogOutPlayData[iChannel][idxnode] = new double[nCsvSamples];
        }
        rgcAnalogOutPlaySamples[iChannel][idxnode] = nCsvSamples;
        
        // Read the data.
        if(!FCsvRead(szFile, &rgdAnalogOutPlayData[iChannel][idxnode], &nCsvChannels, &nCsvSamples)) return false;
        {
            char szSmp[32];
            printf("Play samples loaded for AnalogOut Channel %i, 1 column and %s: \"%s\"\n", idxchannel, NiceNumber(nCsvSamples, " samples", szSmp), szFile);

        }
    }else{
        // Load consecutive columns from one file for AnalogOut channels.

        // Check how many AnalogOut channels does the device have.
        int nChannels = 0;
        if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &nChannels))) return false;
        
        // Check how many channels are configured for play function.
        int nCustomChannels = 0;
        for(int i = 0; i < nChannels; i++){
            for(int j = 0; j < 3; j++){
                FUNC func;
                if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(h, i, j, &func))) return false;
                if(func != funcPlay) continue;

                nCustomChannels++;
            }
        }

        if(nCustomChannels==0){
            printf("\nERROR: No AnalogOut channel is configured for play function.\n");
            printf("  example: analogout channel=1 play channel=2 play channel=0 play=file_with_2_columns.cvs\n");
            return false;
        }

        int nCsvChannels = nCustomChannels;
        int nCsvSamples = 100000000;

        // Check CSV file column and row numbers, up to custom channel count and 100M samples.
        if(!FCsvRead(szFile, NULL, &nCsvChannels, &nCsvSamples)) return false;
        
        if(nCsvChannels<1 || nCsvSamples<1){
            printf("\nERROR: Empty file? \"%s\"\n", szFile);
            return false;
        }

        double* rgdCsvOrder[4*3];

        // Check how many channels are configured for custom function.
        for(int i = 0, iCustomChannels = 0; i < nChannels && iCustomChannels < 4; i++){
            for(int j = 0; j < 3; j++){
                FUNC func;
                if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(h, i, j, &func))) return false;
                if(func != funcPlay) continue;

                if(rgdAnalogOutPlayData[i][j]!=NULL && rgcAnalogOutPlaySamples[i][j]!=nCsvSamples) {
                    delete[] rgdAnalogOutPlayData[i][j];
                    rgdAnalogOutPlayData[i][j] = NULL;
                }
                if(rgdAnalogOutPlayData[i][j]==NULL){
                    rgdAnalogOutPlayData[i][j] = new double[nCsvSamples];
                }
                rgcAnalogOutPlaySamples[i][j] = nCsvSamples;

                rgdCsvOrder[iCustomChannels] = rgdAnalogOutPlayData[i][j];

                iCustomChannels++;
            }
        }

        // Read the file.
        if(!FCsvRead(szFile, rgdCsvOrder, &nCsvChannels, &nCsvSamples)) return false;
        {
            char szSmp[32];
            printf("Play samples loaded for %i AnalogOut channels, %i column and %s samples: \"%s\"\n", nCustomChannels, nCsvChannels, NiceNumber(nCsvSamples, "", szSmp), szFile);

        }
        if(nCsvChannels < nCustomChannels){
            printf("\nWARINING: File has only %i columns but %i channels are configured for play function.\n", nCsvChannels, nCustomChannels);
        }
    }

    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogOutStart
**
**  Parameters:
**      h       - handle to opened device
**      idxchannel   - channel index
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function starts an AnalogOut channel with any 
**  standard or custom function.
** 
*/
bool FAnalogOutStart(HDWF h, int idxchannel) {

    
    if(idxchannel>0){ 
        // Start only one channel 

        {// Check if channel is enabled
            BOOL fEnabled;
            if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, idxchannel-1, AnalogOutNodeCarrier, &fEnabled))) return false;
            if(!fEnabled){
                printf("\nERROR: AnalogOut Channel %i is not enabled.\n", idxchannel);
                return false;
            }
        }
        {
            double sRun;
            char szRun[32];
            if(!FApi("FDwfAnalogOutRunGet", FDwfAnalogOutRunGet(h, idxchannel-1, &sRun))) return false;
            // Start one AnalogOut channel.
            printf("Starting AnalogOut Channel %i generator, run %s.\n", idxchannel, (sRun<=0)?"infinite":NiceNumber(sRun, "sec", szRun));
        }
        // Start the AnalogOut channel.
        if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, idxchannel-1, true))) return false;
    }else{
        // Start multiple channels synchronized
        printf("Starting AnalogOut channels.\n");
        { // Check if any channel is enabled
            int nChannels;
            if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &nChannels))) return false;

            int cChannels = 0;
            for(int i = 0; i < nChannels; i++){
                BOOL fEnabled;
                if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, AnalogOutNodeCarrier, &fEnabled))) return false;
                if(!fEnabled) continue;
                cChannels++;
            }
            if(cChannels==0){
                printf("\nERROR: No AnalogOut channel is enabled.\n");
                return false;
            }
        }
        // Start all enabled AnalogOut channels
        if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, -1, true))) return false;
    }
    return true;
}


/* ------------------------------------------------------------ */
/***    FAnalogOutFinish
**
**  Parameters:
**      h       - handle to opened device
**      idxchannel   - channel index
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function waits for one or for all enabled AnalogOut 
**  channels to finish. 
**  
*/
bool FAnalogOutFinish(HDWF h, int idxchannel){

    // Check one channel
    if(idxchannel>0){
        STS sts = stsRdy;

        printf("Finishing AnalogOut Channel %i\n", idxchannel);

        StartTimeout();

        do{
            if(IsTimeout()){
                return false;
            }
            
            // Read the AnalogIN status and data from device.
            if(!FApi("FDwfAnalogOutStatus", FDwfAnalogOutStatus(h, idxchannel-1, &sts))) {
                return false;
            }
            // AnalogOut generator is running, not yet ended.
        }while(sts != stsRdy && sts != stsDone);
        
        if(sts != stsDone){
            // Wrong state.
            // Probably it was not even started.
            printf("\nERROR: AnalogOut Channel %i was not started.\n", idxchannel);
            return false;
        }
    }else{ 
        // Check all enabled channels.
        int nChannels;
        if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &nChannels))) return false;

        int cChannels = 0;
        for(int i = 0; i < nChannels; i++){
            BOOL fEnabled;
            if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, AnalogOutNodeCarrier, &fEnabled))) return false;
            if(!fEnabled) continue;

            if(!FAnalogOutFinish(h, i+1)) return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogOutStop
**
**  Parameters:
**      h   - handle to opened device
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function stops one AnalogOut channel or all the 
**  enabled channels.
** 
*/
bool FAnalogOutStop(HDWF h, int idxchannel){
    if(idxchannel>0){

        printf("Stopping AnalogOut Channel %i generator.\n", idxchannel);

        // Stop AnalogOut Channel
        if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, idxchannel-1, false))) {
            return false;
        }
        return true;
    }else{
        int nChannels;
        if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &nChannels))) return false;

        int cChannels = 0;
        for(int i = 0; i < nChannels; i++){
            BOOL fEnabled;
            if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, AnalogOutNodeCarrier, &fEnabled))) return false;
            if(!fEnabled) continue;

            if(!FAnalogOutStop(h, i+1)) return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogInRecord
**
**  Parameters:
**      h           - handle to opened device
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function performs AnalogIn recording. The recording 
**  should be used for longer acquisitions as the device buffer 
**  size. The maximum frequency depends on the available USB 
**  bandwidth for the device communication.
**  - initialize, allocate PC buffer for enabled channels
**  - start AnalogIn, move to armed state
**  - loop until requested number of samples are read
**      - read AnalogIn status and data from device
**      - check how many new samples are available
**      - copy new samples to local buffer
*/
bool FAnalogInRecord(HDWF h){

    int nAnalogInChannels = 0;
    int cAnalogInEnabledChannels = 0;
    int nRecordSamples = 0;

    // Check for enabled channels, allocate buffer memory and print information.
    if(!FAnalogInInitialize(h, &nAnalogInChannels, &cAnalogInEnabledChannels, &nRecordSamples)) return false;

    if(cAnalogInEnabledChannels <= 0){
        printf("\nERROR: No AnalogIn channel is enabled.\n");
        printf("example: analogin record channel=1 enable start\n");
        return false;
    }

    // Start the AnalogIn
    if(!FApi("FDwfAnalogInConfigure", FDwfAnalogInConfigure(h, true, true))) return false;

    // Recording loop. Check the number of avaialable new samples and read them.
    STS sts;
    int cSamplesAvailable, cSamplesLost, cSamplesCorrupted;
    bool fRecordWarning = false, fRecordLost = false;
    #ifdef _DEBUG
    int cRecordSamplesMax = 0, cRecordChunks = 0, cRecordLost = 0;
    #endif
    StartTimeout();

    for(int iRecordSamples = 0; iRecordSamples < nRecordSamples; ){
        if(IsTimeout()) return false;

        // Read the AnalogIN status and data from device.
        if(!FApi("FDwfAnalogInStatus", FDwfAnalogInStatus(h, 1, &sts))) return false;

        if(iRecordSamples == 0 && (sts == stsCfg || sts == stsPrefill || sts == stsArm)){
            // Acquisition not yet started.
            continue;
        }
        
        // Check the number of available new samples.
        if(!FApi("FDwfAnalogInStatusRecord", FDwfAnalogInStatusRecord(h, &cSamplesAvailable, &cSamplesLost, &cSamplesCorrupted))) return false;

        
        if(cSamplesLost > 0){
            #ifdef _DEBUG
            cRecordLost++;
            if(cRecordLost<10){
                printf(" %d at %d/%d lost %d available %d\n", cRecordLost, iRecordSamples, nRecordSamples, cSamplesLost, cSamplesAvailable);
            }
            #endif
            // Skip the lost samples.
            iRecordSamples += cSamplesLost;
            fRecordLost = true;
        }

        // Make sure with the new chunk of data we don't exceed our buffer size.
        cSamplesAvailable = min(cSamplesAvailable, max(0, nRecordSamples-iRecordSamples));

        if(cSamplesCorrupted){
            fRecordWarning = true;
        }


        if(cSamplesAvailable > 0){
            
            #ifdef _DEBUG
            cRecordChunks++;
            cRecordSamplesMax = max(cRecordSamplesMax, cSamplesAvailable+cSamplesLost);
            #endif

            // Get the buffer data.
            for(int i = 0; i < 4; i++){
                if(rgdAnalogInData[i] == NULL) continue;
                if(!FApi("FDwfAnalogInStatusData", FDwfAnalogInStatusData(h, i, &rgdAnalogInData[i][iRecordSamples], cSamplesAvailable))) return false;
            }

            iRecordSamples += cSamplesAvailable;
        }

        if(sts != stsTrig){
            if(iRecordSamples < nRecordSamples){
                // Error. This should not happen
                printf("\nERROR: Finished too early %i.\n",(nRecordSamples-iRecordSamples));
            }
            break;
        }
    }

    if(fRecordLost){
        printf("WARNING: Samples are lost. Reduce the AnalogIn frequency");
        int cb;
        if(FApi("FDwfAnalogInBufferSizeInfo", FDwfAnalogInBufferSizeInfo(h, NULL, &cb))){
            printf(" or for this frequency set samples to %i", cb);
        }
        printf(".\n");
    }else if(fRecordWarning){
        printf("WARNING: Samples could be lost. Reduce the AnalogIn frequency.\n");
    }
    #ifdef _DEBUG
    printf("Buffer fillness: average: %.2f%% max: %.2f%% times lost: %d\n", 100.0*nRecordSamples/cRecordChunks/8192, 100.0*cRecordSamplesMax/8192, cRecordLost);
    #endif
    
    
    if(!FApi("FDwfAnalogInConfigure", FDwfAnalogInConfigure(h, true, false))) return false;

    printf("Call save to export data to file.\n");

    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogInPlay
**
**  Parameters:
**      h       - handle to opened device
**      idxchannel   - channel index
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function performs AnalogOut playing process on
**  one channel. This player function should be used for longer 
**  waveform generation then the device buffer. The maximum 
**  frequency depends on the available USB bandwidth for the 
**  device communication.
**  - checks if channel is enabled, samples are loaded from file
**      and limited run time is set.
**  - prefill device buffer with first chunk of samples, up to 
**      the device buffer size
**  - start channel
**  - loop until done
**      - read the analogout channel device state
**      - check playing progress, how many new samples can be sent
**      - send next chunk of samples
** 
*/
bool FAnalogOutPlay(HDWF h, int idxchannel, int idxnode) {

    if(idxchannel<=0 || idxchannel>4) {
        printf("\nERROR: Invalid channel index.\n");
        return false;
    }

    int iChannel = idxchannel-1;

    // Check if channel is enabled
    BOOL fEnabled;
    if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, iChannel, AnalogOutNodeCarrier, &fEnabled))) return false;
    if(!fEnabled) {
        printf("\nERROR: AnalogOut Channel %i is not enabled.\n", idxchannel);
        printf("  example: analogout channel=%i enable ...\n", idxchannel);
        return false;
    }


    if(rgdAnalogOutPlayData[iChannel][idxnode]==NULL || rgcAnalogOutPlaySamples[iChannel][idxnode]<=0){
        printf("\nERROR: No play samples are loaded for AnalogOut channel %i\n", idxchannel);
        printf("  example: analogout channel=1 enable=1 play channel=2 ... channel=0 play=myfile.csv start\n", idxchannel);
        return false;
    }

    { // Check how for how long will the play run.
        double secRun;
        char szRun[32];
        if(!FApi("FDwfAnalogOutRunGet", FDwfAnalogOutRunGet(h, iChannel, &secRun))) return false;

        if(szRun<=0){
            printf("\nERROR: Please specify Run for AnalogOut channel %i.\n", idxchannel);
            printf("  example: analogout channel=%i run=2s\n", idxchannel);
            return false;
        }
        printf("Starting AnalogOut Channel %i to play for %s.\n", idxchannel, NiceNumber(secRun, "sec", szRun));
    }


    // Check the device buffer size.
    int nAnalogOutBufferSize = 0;
    if(!FApi("FDwfAnalogOutNodeDataInfo", FDwfAnalogOutNodeDataInfo(h, iChannel, AnalogOutNodeCarrier, NULL, &nAnalogOutBufferSize))) return false;

    // Prefill the device buffer with the first chunk of data samples.
    int idxPlaySamples = min(rgcAnalogOutPlaySamples[iChannel][idxnode], nAnalogOutBufferSize);
    if(!FApi("FDwfAnalogOutNodeDataSet", FDwfAnalogOutNodeDataSet(h, iChannel, idxnode, rgdAnalogOutPlayData[iChannel][idxnode], idxPlaySamples))) return false;

    if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, iChannel, true))) return false;
    
    // For play channels, verify the device buffer state and send new data chunk (samples) in the available free space.
    STS sts = stsRdy;
    int cSamplesFree, cSamplesLost, cSamplesCorrupted;
    bool fPlayWarning = false;
    bool fPlayLost = false;

    #ifdef _DEBUG
    int cPlaySamplesMax = 0, cPlayChunks = 0, cPlayLost = 0;
    #endif

    StartTimeout();
    while(true){
        if(IsTimeout()) return false;

        // Read the status from the device.
        if(!FApi("FDwfAnalogOutStatus", FDwfAnalogOutStatus(h, iChannel, &sts))) return false;

        // AnalogOut channel not yet started.
        if(sts == stsCfg || sts == stsArm || sts == stsTrigDly) continue;

        // Check the available free buffer space.
        if(!FApi("FDwfAnalogOutNodePlayStatus", FDwfAnalogOutNodePlayStatus(h, iChannel, idxnode, &cSamplesFree, &cSamplesLost, &cSamplesCorrupted))) return false;

        if(cSamplesLost){
            #ifdef _DEBUG
            cPlayLost++;
            if(cPlayLost<10){
                printf(" Out %d. %d/%d lost %d available %d\n", cPlayLost, idxPlaySamples, rgcAnalogOutPlaySamples[iChannel][idxnode], cSamplesLost, cSamplesFree);
            }
            #endif

            // We lost some samples.
            // The AnalogOut generator is running faster than we can send new data chunks.
            fPlayLost = true;
            // By advancing the pointer we can skip the lost samples.
            idxPlaySamples += cSamplesLost;
        }
        if(cSamplesCorrupted > 0) {
            // We could have had problems during the previous data chunk sending.
            // The writing in the device buffer could overlap the reading of samples.
            fPlayWarning = true;
        }

        // Do we have free space in the device buffer?
        if(cSamplesFree>0){
            #ifdef _DEBUG
            cPlayChunks++;
            cPlaySamplesMax = max(cPlaySamplesMax, cSamplesFree+cSamplesLost);
            #endif

            // Send the new data chunk.
            int idxPlay = idxPlaySamples%rgcAnalogOutPlaySamples[iChannel][idxnode];

            if(idxPlay + cSamplesFree > rgcAnalogOutPlaySamples[iChannel][idxnode]){
                // Make sure we don't exceed our allocated buffer.
                cSamplesFree = rgcAnalogOutPlaySamples[iChannel][idxnode]-idxPlay;
            }
            if(cSamplesFree>0){
                if(!FApi("FDwfAnalogOutPlayData", FDwfAnalogOutPlayData(h, iChannel, &rgdAnalogOutPlayData[iChannel][idxnode][idxPlay], cSamplesFree))) return false;
                idxPlaySamples += cSamplesFree;
            }
        }
        if(sts==stsDone) {
            break;
        }
    }

    if(fPlayLost){
        printf("\nWARNING: Samples were lost. Reduce the AnalogOut frequency.\n");
    }else if(fPlayWarning){
        printf("\nWARNING: Samples could be corrupted. Reduce the AnalogOut frequency.\n");
    }
    #ifdef _DEBUG
    printf("Buffer emptiness: average: %.2f%% max: %.2f%% times lost: %d\n", 100.0*rgcAnalogOutPlaySamples[iChannel][idxnode]/cPlayChunks/4096, 100.0*cPlaySamplesMax/4096, cPlayLost);
    #endif
    
    if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, iChannel, false))) return false;
    return true;

}

/* ------------------------------------------------------------ */
/***    FAnalogOutStart
**
**  Parameters:
**      h   - handle to opened device
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function starts enabled AnalogOut channels and 
**  performs playing process on channels set to play function.
** 
*/
bool FAnalogOutStart(HDWF h) {

    int nAnalogOutChannels = 0;
    int cAnalogOutEnabledChannels = 0;
    int idxPlaySamples[4][3] = {{0,0,0},{0,0,0},{0,0,0},{0,0,0}};
    bool idxPlayChannels[4];
    bool idxPlayNodes[4][3];

    // Read the number of AnalogOut channels.
    if(!FAnalogOutInitialize(h, trigsrcPC, &nAnalogOutChannels, &cAnalogOutEnabledChannels, idxPlaySamples)) return false;
    for(int i = 0; i < 4; i++){
        idxPlayChannels[i] = false;
        for(int j = 0; j < 3; j++){
            idxPlayNodes[i][j] = (idxPlaySamples[i][j]!=0);
            if(!idxPlayNodes[i][j]) continue;
            idxPlayChannels[i] = true;
        }
    }

    if(!FApi("FDwfDeviceTriggerPC", FDwfDeviceTriggerPC(h))) return false;
    
    // For play channels, verify the device buffer state and send new data chunk (samples) in the available free space.
    STS sts = stsRdy;
    int cSamplesFree, cSamplesLost, cSamplesCorrupted;
    bool fPlayWarning = false, fPlayLost = false;


    StartTimeout();

    while(true){
        if(IsTimeout()) return false;

        bool fDoneAll = true;

        for(int i = 0; i < 4 && i < nAnalogOutChannels; i++){

            if(!idxPlayChannels[i]) continue;

            // Read the status from the device.
            if(!FApi("FDwfAnalogOutStatus", FDwfAnalogOutStatus(h, i, &sts))) return false;

            // AnalogOut channel not yet started.
            if(sts == stsCfg || sts == stsArm || sts == stsTrigDly) continue;

            for(int j = 0; j < 3; j++){
                if(!idxPlayNodes[i][j]) continue;

                // Check the available free buffer space.
                if(!FApi("FDwfAnalogOutNodePlayStatus", FDwfAnalogOutNodePlayStatus(h, i, j, &cSamplesFree, &cSamplesLost, &cSamplesCorrupted))) return false;

                if(cSamplesLost){
                    // We lost some samples.
                    // The AnalogOut generator is running faster than we can send new data chunks.
                    fPlayLost = true;
                    // By advancing the pointer we can skip the lost samples.
                    idxPlaySamples[i][j] += cSamplesLost;
                }
                if(cSamplesCorrupted > 0) {
                    // We could have had problems during the previous data chunk sending.
                    // The writing in the device buffer could overlap the reading of samples.
                    fPlayWarning = true;
                }

                // Do we have free space in the device buffer?
                if(cSamplesFree>0){
                    // Send the new data chunk.
                    int idxPlay = idxPlaySamples[i][j]%rgcAnalogOutPlaySamples[i][j];
                    if(idxPlay + cSamplesFree > rgcAnalogOutPlaySamples[i][j]){
                        // Make sure we don't exceed our allocated buffer.
                        cSamplesFree = rgcAnalogOutPlaySamples[i][j]-idxPlay;
                    }
                    if(cSamplesFree>0){
                        if(!FApi("FDwfAnalogOutNodePlayData", FDwfAnalogOutNodePlayData(h, i, j, &rgdAnalogOutPlayData[i][j][idxPlay], cSamplesFree))) return false;
                        idxPlaySamples[i][j] += cSamplesFree;
                    }
                }
                if(sts!=stsDone) {
                    fDoneAll = false;
                }
            } 
        }

        if(fDoneAll){
            break;
        } 
    }

    if(fPlayLost){
        printf("\nWARNING: Samples were lost. Reduce the AnalogOut frequency.\n");
    }else if(fPlayWarning){
        printf("\nWARNING: Samples could be corrupted. Reduce the AnalogOut frequency.\n");
    }
    
    for(int i = 0; i < 4 && i < nAnalogOutChannels; i++){
        if(!idxPlayChannels[i]) continue;
        if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, i, false))) return false;
    }

    return true;
}

/* ------------------------------------------------------------ */
/***    FAnalogStartAll
**
**  Parameters:
**      h   - handle to opened device
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function performs AnalogOut play function, by
**  pushing data sample chunks to the device. This player 
**  mode should be used for longer waveform generation then
**  the device buffer. The maximum frequency depends on the 
**  device communication bandwidth.
** 
*/
bool FAnalogStartAll(HDWF h) {

    int nAnalogInChannels = 0;
    int cAnalogInEnabledChannels = 0;
    bool fAnalogInRecord = false;
    int nAnalogInRecordSamples = 0;
    int idxAnalogInRecord = 0;

    { // Check if any AnalogIn channel is enabled.
        if(!FAnalogInInitialize(h, &nAnalogInChannels, &cAnalogInEnabledChannels, &nAnalogInRecordSamples)) return false;
        ACQMODE acqmode;
        if(!FApi("FDwfAnalogOutCount", FDwfAnalogInAcquisitionModeGet(h, &acqmode))) return false;
        fAnalogInRecord = (acqmode==acqmodeRecord);
    }

    int nAnalogOutChannels = 0;
    int cAnalogOutEnabledChannels = 0;
    int idxPlaySamples[4][3] = {{0,0,0},{0,0,0},{0,0,0},{0,0,0}};
    bool idxPlayChannels[4];
    bool idxPlayNodes[4][3];

    // In order to start synchronuously all the channels: 
    // We will use AnalogIn as trigger source for AnalogOut channels,
    // When no AnalogIn channel is used we will use PC trigger signal.
    TRIGSRC trigscSync = (cAnalogInEnabledChannels)?trigsrcAnalogIn:trigsrcPC;

    if(!FAnalogOutInitialize(h, trigscSync, &nAnalogOutChannels, &cAnalogOutEnabledChannels, idxPlaySamples)) return false;
    // look for channels used for play
    for(int i = 0; i < 4; i++){
        idxPlayChannels[i] = false;
        for(int j = 0; j < 3; j++){
            idxPlayNodes[i][j] = (idxPlaySamples[i][j]!=0);
            if(!idxPlayNodes[i][j]) continue;
            idxPlayChannels[i] = true;
        }
    }

    // Start the AnalogIn
    if(!FApi("FDwfAnalogInConfigure", FDwfAnalogInConfigure(h, true, true))) return false;

    // Toggle PC trigger
    if(!FApi("FDwfDeviceTriggerPC", FDwfDeviceTriggerPC(h))) return false;
    
    // For play channels, verify the device buffer state and send new data chunk (samples) in the available free space.
    STS sts = stsRdy;
    int cSamplesFree, cSamplesAvailable, cSamplesLost, cSamplesCorrupted;
    bool fPlayWarning = false, fRecordWarning = false, fPlayLost = false, fRecordLost = false;

    StartTimeout();
    while(true){
        if(IsTimeout()) return false;

        bool fDoneAll = true;

        if(fAnalogInRecord && idxAnalogInRecord < nAnalogInRecordSamples){
            fDoneAll = false;

            // Read the AnalogIN status and data from device.
            if(!FApi("FDwfAnalogInStatus", FDwfAnalogInStatus(h, 1, &sts))) return false;

            if(sts == stsTrig || sts == stsDone){
                // Check the number of available new samples.
                if(!FApi("FDwfAnalogInStatusRecord", FDwfAnalogInStatusRecord(h, &cSamplesAvailable, &cSamplesLost, &cSamplesCorrupted))) return false;
                
                if(cSamplesLost > 0){
                    // Skip the lost samples.
                    idxAnalogInRecord += cSamplesLost;
                    fRecordLost = true;
                }

                // Make sure with the new chunk of data we don't exceed our buffer size.
                cSamplesAvailable = min(cSamplesAvailable, max(0, nAnalogInRecordSamples-idxAnalogInRecord));

                if(cSamplesCorrupted){
                    fRecordWarning = true;
                }

                if(cSamplesAvailable > 0){

                    // Get the buffer data.
                    for(int i = 0; i < 4; i++){
                        if(rgdAnalogInData[i] == NULL) continue;
                        if(!FApi("FDwfAnalogInStatusData", FDwfAnalogInStatusData(h, i, &rgdAnalogInData[i][idxAnalogInRecord], cSamplesAvailable))) return false;
                    }

                    idxAnalogInRecord += cSamplesAvailable;
                }
            }
        }

        for(int i = 0; i < 4 && i < nAnalogOutChannels; i++){

            if(!idxPlayChannels[i]) continue;

            // Read the status from the device.
            if(!FApi("FDwfAnalogOutStatus", FDwfAnalogOutStatus(h, i, &sts))) return false;
            // AnalogOut channel not yet started.
            if(sts == stsCfg || sts == stsArm || sts == stsTrigDly) {
                fDoneAll = false;
                continue;
            }

            for(int j = 0; j < 3; j++){

                if(!idxPlayNodes[i][j]) continue;

                // Check the available free buffer space.
                if(!FApi("FDwfAnalogOutNodePlayStatus", FDwfAnalogOutNodePlayStatus(h, i, j, &cSamplesFree, &cSamplesLost, &cSamplesCorrupted))) return false;

                if(cSamplesLost){
                    // We lost some samples.
                    // The AnalogOut generator is running faster than we can send new data chunks.
                    fPlayLost = true;
                    // By advancing the pointer we can skip the lost samples.
                    idxPlaySamples[i][j] += cSamplesLost;
                }
                if(cSamplesCorrupted > 0) {
                    // We could have had problems during the previous data chunk sending.
                    // The writing in the device buffer could overlap the reading of samples.
                    fPlayWarning = true;
                }

                // Do we have free space in the device buffer?
                if(cSamplesFree>0){

                    // Send the new data chunk.
                    int idxPlay = idxPlaySamples[i][j]%rgcAnalogOutPlaySamples[i][j];
                    if(idxPlay + cSamplesFree > rgcAnalogOutPlaySamples[i][j]){
                        // Make sure we don't exceed our allocated buffer.
                        cSamplesFree = rgcAnalogOutPlaySamples[i][j]-idxPlay;
                    }
                    if(cSamplesFree>0){
                        if(!FApi("FDwfAnalogOutNodePlayData", FDwfAnalogOutNodePlayData(h, i, j, &rgdAnalogOutPlayData[i][j][idxPlay], cSamplesFree))) return false;
                        idxPlaySamples[i][j]+= cSamplesFree;
                    }
                }
                if(sts!=stsDone) {
                    fDoneAll = false;
                }
            }
        }

        if(fDoneAll){
            break;
        }
    }

    if(fPlayLost){
        printf("\nWARNING: Samples were lost. Reduce the AnalogOut frequency.\n");
    }else if(fPlayWarning){
        printf("\nWARNING: Samples could be corrupted. Reduce the AnalogOut frequency.\n");
    }
    if(fRecordLost){
        printf("WARNING: Samples are lost. Reduce the AnalogIn frequency");
        int cb;
        if(FApi("FDwfAnalogInBufferSizeInfo", FDwfAnalogInBufferSizeInfo(h, NULL, &cb))){
            printf(" or for this frequency set samples to %i", cb);
        }
        printf(".\n");
    }else if(fRecordWarning){
        printf("WARNING: Samples could be lost. Reduce the AnalogIn frequency.\n");
    }

    // In case we used AnalogIn for recording stop it, change Done state to Ready.
    // We do this in order to detect wrong finish commands.
    if(fAnalogInRecord){
        if(!FApi("FDwfAnalogInConfigure", FDwfAnalogInConfigure(h, false, false))) return false;
    }
    // Similar we do for AnalgOut channels used for play.
    for(int i = 0; i < 4 && i < nAnalogOutChannels; i++){
        if(!idxPlayChannels[i]) continue;
        if(!FApi("FDwfAnalogOutConfigure", FDwfAnalogOutConfigure(h, i, false))) return false;
    }

    return true;
}

/* ------------------------------------------------------------ */
/***    FInfo
**
**  Parameters:
**      h        - handle to opened device
**      tar      - target
**      idxchannel  - channel index
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function prints device information.
** 
*/
bool FInfo(HDWF h, Target tar, int idxchannel){
    int c;
    int i0;
    double d0;
    double d1;
    double d2;
    BOOL f0;
    BOOL f1;
    char sz0[512];
    char sz1[512];
    char sz2[512];

    if(tar == TargetDevice || tar == TargetAnalogIn){
        printf("\nAnalogIn information:\n");

        if(!FApi("FDwfAnalogInChannelCount", FDwfAnalogInChannelCount(h, &i0))) return false;
        printf("Channels: %i\n", i0);

        if(!FApi("FDwfAnalogInBufferSizeInfo", FDwfAnalogInBufferSizeInfo(h, NULL, &i0))) return false;
        printf("Buffer size: %i samples\n", i0);

        if(!FApi("FDwfAnalogInBitsInfo", FDwfAnalogInBitsInfo(h, &i0))) return false;
        printf("ADC bits:  %i\n", i0);

        if(!FApi("FDwfAnalogInChannelRangeInfo", FDwfAnalogInChannelRangeInfo(h, &d0, &d1, &d2))) return false;
        printf("Range from %s to %s in %s steps\n", NiceNumber(d0, "V", sz0), NiceNumber(d1, "V", sz1), NiceInteger((int)d2, "", sz2));

        if(!FApi("FDwfAnalogInChannelOffsetInfo", FDwfAnalogInChannelOffsetInfo(h, &d0, &d1, &d2))) return false;
        printf("Offset from %s to %s in %s steps\n", NiceNumber(d0, "V", sz0), NiceNumber(d1, "V", sz1), NiceInteger((int)d2, "", sz2));
    }


    if(tar == TargetDevice || tar == TargetAnalogOut){
        
        if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &c))) return false;
        if(idxchannel<=0) printf("\nAnalogOut channels: %i\n", c);

        for(int i = 0; i < c; i++){
            if(idxchannel>0 && i!=(idxchannel-1)) continue;

            printf("\nChannel %i\n", i+1);

            int fsnodes;
            if(!FApi("FDwfAnalogOutNodeDataInfo", FDwfAnalogOutNodeInfo(h, i, &fsnodes))) return false;

            for(int j = 0; j < 3; j++){
                if(!IsBitSet(fsnodes, j)) continue;

                printf("%s\n", rgszAnalogOutNode[j]);
                
                if(!FApi("FDwfAnalogOutNodeDataInfo", FDwfAnalogOutNodeDataInfo(h, i, j, NULL, &i0))) return false;
                printf("Buffer size: %i samples\n", i0);

                if(!FApi("FDwfAnalogOutNodeAmplitudeInfo", FDwfAnalogOutNodeAmplitudeInfo(h, i, j, &d0, &d1))) return false;
                printf("Amplitude from %s to %s\n", NiceNumber(d0, "V", sz0), NiceNumber(d1, (char*)(j==0?"V":"%"), sz1));

                if(!FApi("FDwfAnalogOutNodeOffsetInfo", FDwfAnalogOutNodeOffsetInfo(h, i, j, &d0, &d1))) return false;
                printf("Offset from %s to %s\n", NiceNumber(d0, "V", sz0), NiceNumber(d1, (char*)(j==0?"V":"%"), sz1));

                if(!FApi("FDwfAnalogOutNodeFrequencyInfo", FDwfAnalogOutNodeFrequencyInfo(h, i, j, &d0, &d1))) return false;
                printf("Frequency from %s to %s\n", NiceNumber(d0, "Hz", sz0), NiceNumber(d1, "Hz", sz1));
            }
        }
    }

    if(tar == TargetDevice || tar == TargetAnalogIO){
        if(!FApi("FDwfAnalogIOChannelCount", FDwfAnalogIOChannelCount(h, &c))) return false;

        if(idxchannel<=0) {
            printf("\nAnalogIO channels: %i\n", c);

            if(!FApi("FDwfAnalogIOEnableInfo", FDwfAnalogIOEnableInfo(h, &f0, &f1))) return false;
            if(f0 || f1) printf("Master Enable: %s%s\n", f0?"Setting ":"", f1?"Reading":"");
        }

        for(int i = 0; i < c; i++){
            if(idxchannel>0 && i!=(idxchannel-1)) continue;

            printf("\nChannel %i\n", i+1);

            if(!FApi("FDwfAnalogIOChannelName", FDwfAnalogIOChannelName(h, i, sz0, sz1))) continue;
            printf("Name: \"%s\" Label: \"%s\"\n", sz0, sz1);

            int n;
            if(!FApi("FDwfAnalogIOChannelInfo", FDwfAnalogIOChannelInfo(h, i, &n))) return false;
            for(int j = 0; j < n; j++){
                char szUnit[8];
                if(!FApi("FDwfAnalogIOChannelNodeName", FDwfAnalogIOChannelNodeName(h, i, j, sz0, szUnit))) return false;
                printf("%s \n", sz0);
                if(!FApi("FDwfAnalogIOChannelNodeSetInfo", FDwfAnalogIOChannelNodeSetInfo(h, i, j, &d0, &d1, &i0))) return false;
                if(i0>1){
                    printf("\tSetting from %s to %s in %s steps\n", NiceNumber(d0, szUnit, sz0), NiceNumber(d1, szUnit, sz1), NiceInteger(i0, "", sz2));
                }else if(i0==1){
                    if(d0 != d1) {
                        printf("\tNon settable range from %s to %s\n", NiceNumber(d0, szUnit, sz0), NiceNumber(d1, szUnit, sz1));
                    }else{
                        printf("\tConstant output %s\n", NiceNumber(d0, szUnit, sz0));
                    }
                }
                if(!FApi("FDwfAnalogIOChannelNodeStatusInfo", FDwfAnalogIOChannelNodeStatusInfo(h, i, j, &d0, &d1, &i0))) return false;
                if(i0>1){
                    printf("\tReading from %s to %s in %s steps\n", NiceNumber(d0, szUnit, sz0), NiceNumber(d1, szUnit, sz1), NiceInteger(i0, "", sz2));
                }else if(i0==1){
                    if(d0 != d1) {
                        printf("\tInput range from %s to %s\n", NiceNumber(d0, szUnit, sz0), NiceNumber(d1, szUnit, sz1));
                    }else{
                        printf("\tConstant input %s\n", NiceNumber(d0, szUnit, sz0));
                    }
                }
            }
        }
    }

    if(tar == TargetDevice || tar == TargetDigitalIO){
        printf("\nDigitalIO information:\n");

        if(!FApi("FDwfDigitalIOOutputEnableInfo", FDwfDigitalIOOutputEnableInfo(h, (unsigned int*)&i0))) return false;
        printf("OE Mask:\t0x%08X\n", i0);

        if(!FApi("FDwfDigitalIOOutputInfo", FDwfDigitalIOOutputInfo(h, (unsigned int*)&i0))) return false;
        printf("Output Mask:\t0x%08X\n", i0);

        if(!FApi("FDwfDigitalIOInputInfo", FDwfDigitalIOInputInfo(h, (unsigned int*)&i0))) return false;
        printf("Input Mask:\t0x%08X\n", i0);
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FShow
**
**  Parameters:
**      h           - handle to opened device
**      tar         - target
**      idxchannel  - channel index
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function prints device configuration and status, 
**  reading information.
** 
*/
bool FShow(HDWF h, Target tar, int idxchannel){
    int c;
    int i0;
    int i1;
    BOOL f0;
    BOOL f1;

    if(tar == TargetDevice || tar == TargetAnalogIn){
    
        target = TargetAnalogIn;

        if(idxchannel<=0){
            if(!FCommand("trigger")) return false;
            if(!FCommand("mode")) return false;
            if(!FCommand("frequency")) return false;
            if(!FCommand("run")) return false;
            if(!FCommand("samples")) return false;

            ACQMODE acqmode;
            if(!FApi("FDwfAnalogInTriggerTypeGet", FDwfAnalogInTriggerTypeGet(hdwf, &acqmode))) return false;
            if(acqmode == acqmodeSingle){
                if(!FCommand("position")) return false;
            }

            TRIGSRC trigsrc;
            if(!FApi("FDwfAnalogInTriggerSourceGet", FDwfAnalogInTriggerSourceGet(h, &trigsrc))) return false;

            if(trigsrc == trigsrcDetectorAnalogIn){
                if(!FCommand("type")) return false;
                if(!FCommand("condition")) return false;
                if(!FCommand("level")) return false;
                if(!FCommand("hysteresis")) return false;

                TRIGTYPE trigtype;
                if(!FApi("FDwfAnalogInTriggerTypeGet", FDwfAnalogInTriggerTypeGet(hdwf, &trigtype))) return false;

                if(trigtype == trigtypePulse || trigtype == trigtypeTransition){
                    if(!FCommand("lengthcondition")) return false;
                    if(!FCommand("length")) return false;
                }
            }
        }
        
        if(!FApi("FDwfAnalogInChannelCount", FDwfAnalogInChannelCount(h, &c))) return false;

        for(int i = 0; i < c; i++){

            if(idxchannel>0 && i!=(idxchannel-1)) continue;

            idxChannel = i+1;

            if(!FCommand("enable")) return false;

            if(!FApi("FDwfAnalogInChannelEnableGet", FDwfAnalogInChannelEnableGet(h, i, &f0))) return false;
            if(!f0) continue;

            if(!FCommand("range")) return false;
            if(!FCommand("offset")) return false;
            if(!FCommand("filter")) return false;
            if(!FCommand("voltage")) return false;
        }
    }

    if(tar == TargetDevice || tar == TargetAnalogOut){

        target = TargetAnalogOut;

        if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(h, &c))) return false;

        for(int i = 0; i < c; i++){
            if(idxchannel>0 && i!=(idxchannel-1)) continue;

            idxChannel = i+1;

            if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, 0, &f0))) return false;
            if(f0){
                if(!FCommand("trigger")) return false;
                if(!FCommand("wait")) return false;
                if(!FCommand("run")) return false;
                if(!FCommand("repeat")) return false;

                for(int j = 0; j < 3; j++){
                    idxNode = j;

                    if(!FCommand("enable")) return false;
                    if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(h, i, j, &f0))) return false;
                    if(!f0) continue;

                    if(!FCommand("function")) return false;
                    FUNC func;
                    if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(h, i, j, &func))) return false;
                    if(func==funcPlay) if(!FCommand("samples")) return false;

                    if(!FCommand("offset")) return false;

                    if(func==funcDC) continue;

                    if(!FCommand("amplitude")) return false;
                    if(!FCommand("frequency")) return false;

                    if(func!=funcCustom && func!=funcPlay) {
                        if(!FCommand("phase")) return false;
                        if(!FCommand("symmetry")) return false;
                    }
                }
            }else{
                if(!FCommand("enable")) return false;
            }
        }
    }

    if(tar == TargetDevice || tar == TargetAnalogIO){

        target = TargetAnalogIO;

        if(idxchannel<=0){
            if(!FApi("FDwfAnalogIOEnableInfo", FDwfAnalogIOEnableInfo(h, &f0, &f1))) return false;
            if(f0 || f1) {
                if(!FCommand("masterenable")) return false;
            }
        }

        if(!FApi("FDwfAnalogIOChannelCount", FDwfAnalogIOChannelCount(h, &c))) return false;

        for(int i = 0; i < c; i++){
            if(idxchannel>0 && i!=(idxchannel-1)) continue;

            idxChannel = i+1;

            int n;
            if(!FApi("FDwfAnalogIOChannelInfo", FDwfAnalogIOChannelInfo(h, i, &n))) return false;

            for(int j = 0; j < n; j++){
                if(!FApi("FDwfAnalogIOChannelNodeSetInfo", FDwfAnalogIOChannelNodeSetInfo(h, i, j, NULL, NULL, &i0))) return false;
                if(!FApi("FDwfAnalogIOChannelNodeStatusInfo", FDwfAnalogIOChannelNodeStatusInfo(h, i, j, NULL, NULL, &i1))) return false;
                if(i0>1 || i1>1){
                    char szName[32];
                    char szUnit[8];
                    if(!FApi("FDwfAnalogIOChannelNodeName", FDwfAnalogIOChannelNodeName(h, i, j, szName, szUnit))) return false;
                    if(!FCommand(szName)) return false;
                }
            }
        }
    }

    if(tar == TargetDevice || tar == TargetDigitalIO){

        target = TargetDigitalIO;

        if(!FCommand("oe")) return false;
        if(!FCommand("io")) return false;
    }

    target = tar;
    idxChannel = idxchannel;

    return true;
}

/* ------------------------------------------------------------ */
/***    FEnum
**
**  Parameters:
**      none
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function enumerates and prints the found devices.
** 
*/
bool FEnum(){
    int c = 0;
    char sz[512];
    BOOL fBusy;
    DEVID devid;
    DEVVER devver;
    HDWF h = hdwfNone;

    if(!FApi("FDwfEnum", FDwfEnum(enumfilterAll, &c))) return false;
    for(int i = 0; i < c; i++){
        printf("\nDevice %d\n", i+1);

        if(!FApi("FDwfEnumDeviceName", FDwfEnumDeviceName(i, sz))) continue;
        printf("Device Name:\t%s\n", sz);

        if(!FApi("FDwfEnumSN", FDwfEnumSN(i, sz))) continue;
        printf("Serial Number:\t%s\n", sz);

        if(!FApi("FDwfEnumUserName", FDwfEnumUserName(i, sz))) continue;
        printf("User Name:\t%s\n", sz);
        
        if(!FApi("FDwfEnumDeviceType", FDwfEnumDeviceType(i, &devid, &devver))) continue;
        printf("Device ID:\t%d version %d\n", devid, devver);

        if(!FApi("FDwfEnumDeviceIsOpened", FDwfEnumDeviceIsOpened(i, &fBusy))) continue;
        printf("Is Busy?:\t%s\n", fBusy?"YES":"NO");
        
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FDeviceByIndex
**
**  Parameters:
**      idx     - device enumeration index
**      phdwf   - pointer to return the device handle
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function opens a device by enumeration index.
** 
*/
bool FDeviceByIndex(int idx, HDWF *phdwf){
    if(idx>0){
        // We enumerate here because this application did quit after the earlier 
        // enumeration and printing.
        if(!FApi("FDwfEnum", FDwfEnum(enumfilterAll, NULL))) return false;
        if(!FApi("FDwfDeviceOpen", FDwfDeviceOpen(idx-1, phdwf))){
            return false;
        }
    }else{
        // Device index -1 will autmatically enumerate the connected devices 
        // and open the firs available one.
        if(!FApi("FDwfDeviceOpen", FDwfDeviceOpen(-1, phdwf))){
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FDeviceByName
**
**  Parameters:
**      szDevice- device identification text (serial number, user or device name)
**      phdwf   - pointer to return the device handle
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function opens a device identified by text.
** 
*/
bool FDeviceByName(char *szDevice, HDWF *phdwf){
    int idx = -1;
    int cDevices = 0;

    char szSN[32];
    char szName[32];
    char szUser[32];
    BOOL fBusy;

    // Enumerate devices.
    if(!FApi("FDwfEnum", FDwfEnum(enumfilterAll, &cDevices))) return false;

    // Look for matching SN, user or device name
    for(int i = 0; i < cDevices; i++){
        szSN[0] = 0;
        szUser[0] = 0;
        szName[0] = 0;
        FApi("FDwfEnumSN", FDwfEnumSN(i, szSN));
        FApi("FDwfEnumUserName", FDwfEnumUserName(i, szUser));
        FApi("FDwfEnumDeviceName", FDwfEnumDeviceName(i, szName));
        if(strcmp(szDevice, szSN)==0 || strcmp(szDevice, szUser)==0 || strcmp(szDevice, szName)==0){
            idx = i;
            FApi("FDwfEnumDeviceIsOpened", FDwfEnumDeviceIsOpened(i, &fBusy));
            if(!fBusy){
                break;
            }
        }
    }

    if(idx<0){
        printf("\nERROR: Device \"%s\" not found.\n", szDevice);
        FEnum();
        return false;
    }

    if(!FApi("FDwfDeviceOpen", FDwfDeviceOpen(idx, phdwf))){
        return false;
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FCommand
**
**  Parameters:
**      szCmd   - command
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function identifies the text command and executes it.
** 
*/
bool FCommand(char* szCmd){
    char* szParam = NULL;
    double dParam;
    int iParam;
    char szNumber[64];

    if(FCmdMatch(szCmd, "enum") || FCmdMatch(szCmd, "enumerate")){
        if(!FEnum()) return false;
    }else if(FCmdInteger(szCmd, "connect=", &iParam)){
        if(!FDeviceByIndex(iParam, &hdwf)) return false;
    }else if(FCmdText(szCmd, "connect=", &szParam)){
        if(!FDeviceByName(szParam, &hdwf)) return false;
    }else if(FCmdMatch(szCmd, "connect")){
        if(!FDeviceByIndex(-1, &hdwf)) return false;
    }else if(FCmdMatch(szCmd, "error")){
        char sz[256];
        FDwfGetLastErrorMsg(sz);
        printf(sz);
    }else if(FCmdMatch(szCmd, "disconnect")){
        if(!FApi("FDwfDeviceClose", FDwfDeviceClose(hdwf))) return false;
        hdwf = hdwfNone;
    }else if(FCmdText(szCmd, "execute=", &szParam)){
        FILE* fileExecute = fopen(szParam, "r");
        if(fileExecute == NULL){
            printf("\nERROR: File opening failed: %s\n", szCmd);
            return false;
        }
        printf("Executing file: %s\n", szParam);
        if(!FExecute(0, NULL, fileExecute)) {
            printf("  file: %s\n", szParam);            
            fclose(fileExecute);
            return false;
        }
        fclose(fileExecute);
    }else if(FCmdMatch(szCmd, "target=device") || FCmdMatch(szCmd, "target=dev") || FCmdMatch(szCmd, "device") || FCmdMatch(szCmd, "dev")){
        target = TargetDevice;
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "target=analogin") || FCmdMatch(szCmd, "target=ai") || FCmdMatch(szCmd, "analogin") || FCmdMatch(szCmd, "ai")){
        target = TargetAnalogIn;
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "target=analogout") || FCmdMatch(szCmd, "target=ao") || FCmdMatch(szCmd, "analogout") || FCmdMatch(szCmd, "ao")){
        target = TargetAnalogOut;
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "target=analogio") || FCmdMatch(szCmd, "target=aio") || FCmdMatch(szCmd, "analogio") || FCmdMatch(szCmd, "aio")){
        target = TargetAnalogIO;
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "target=digitalio") || FCmdMatch(szCmd, "target=dio") || FCmdMatch(szCmd, "digitalio") || FCmdMatch(szCmd, "dio")){
        target = TargetDigitalIO;
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "target=digitalio") || FCmdMatch(szCmd, "target=dio") || FCmdMatch(szCmd, "digitalio") || FCmdMatch(szCmd, "dio")){
        target = TargetDigitalIO;
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "target")){
        switch(target){
            case TargetDevice:      printf("Target=Device\n"); break;
            case TargetAnalogIn:    printf("Target=AnalogIn\n"); break;
            case TargetAnalogOut:   printf("Target=AnalogOut\n"); break;
            case TargetAnalogIO:    printf("Target=AnalogIO\n"); break;
            case TargetDigitalIO:   printf("Target=DigitalIO\n"); break;
            default:                printf("Target=UNKNOWN\n"); break;
        }
        idxChannel = 0;
    }else if(FCmdMatch(szCmd, "info") || FCmdMatch(szCmd, "information")){
        if(!FInfo(hdwf, target, idxChannel)) return false;
    }else if(FCmdMatch(szCmd, "show")){
        if(!FShow(hdwf, target, idxChannel)) return false;
    }else if(FCmdNumber(szCmd, "pause=", "sec", &dParam) || FCmdNumber(szCmd, "pause=", "s", &dParam)){
        if(!FPause(dParam)) return false;
    }else if(FCmdNumber(szCmd, "watch=", "sec", &dParam) || FCmdNumber(szCmd, "watch=", "s", &dParam)){
        tsWatch = dParam;
    }else if(FCmdMatch(szCmd, "watch")){
        printf("Watch Timeout=%s\n", NiceNumber(tsWatch, "s", szNumber));
    }else if(FCmdText(szCmd, "print=", &szParam)){
        printf("%s\n", szParam);
    }else if(FCmdMatch(szCmd, "datetime")){
        PrintDateTime(true);
    }else if(FCmdMatch(szCmd, "time")){
        PrintDateTime(false);
    }else if(FCmdMatch(szCmd, "reset")){

        target = TargetDevice;
        idxChannel = 0;
        idxNode = 0;

        cAnalogInSamples = 0;
        for(int i = 0; i < 4; i++){
            delete[] rgdAnalogInData[i];
            rgdAnalogInData[i] = NULL;
            for(int j = 0; j < 3; j++) {
                rgcAnalogOutPlaySamples[i][j] = 0;
                delete[] rgdAnalogOutPlayData[i][j];
                rgdAnalogOutPlayData[i][j] = NULL;
            }
        }

        if(!FApi("FDwfAnalogInReset", FDwfAnalogInReset(hdwf))) return false; 
        if(!FApi("FDwfAnalogOutReset", FDwfAnalogOutReset(hdwf, -1))) return false; 
        if(!FApi("FDwfAnalogIOReset", FDwfAnalogIOReset(hdwf))) return false; 
        if(!FApi("FDwfDigitalIOReset", FDwfDigitalIOReset(hdwf))) return false; 

    }else if(FCmdInteger(szCmd, "debug=", &iParam)){
        debugLevel = iParam;
        printf("Debug level set to %i\n", debugLevel);
    }else if(FCmdMatch(szCmd, "debug")){
        printf("Debug level: %i\n", debugLevel);
    }else{
        // 
        switch(target){
            case TargetDevice:
                if(FCmdMatch(szCmd, "start")){
                    if(!FAnalogStartAll(hdwf)) return false;
                    break;
                }
            case TargetAnalogIn: 
                if(FCmdNumber(szCmd, "frequency=", "Hz", &dParam) || FCmdNumber(szCmd, "freq=", "Hz", &dParam)){
                    if(!FApi("FDwfAnalogInFrequencySet", FDwfAnalogInFrequencySet(hdwf, dParam))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "frequency") || FCmdMatch(szCmd, "freq")){
                    if(!FApi("FDwfAnalogInFrequencyGet", FDwfAnalogInFrequencyGet(hdwf, &dParam))) return false; 
                    printf("AnalogIn Frequency=%s\n", NiceNumber(dParam, "Hz", szNumber));
                    break;
                }else if(FCmdMatch(szCmd, "mode=single") || FCmdMatch(szCmd, "single")){
                    if(!FApi("FDwfAnalogInAcquisitionModeSet", FDwfAnalogInAcquisitionModeSet(hdwf, acqmodeSingle))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "mode=record") || FCmdMatch(szCmd, "record")){
                    if(!FApi("FDwfAnalogInAcquisitionModeSet", FDwfAnalogInAcquisitionModeSet(hdwf, acqmodeRecord))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "mode")){
                    ACQMODE acqmode;
                    if(!FApi("FDwfAnalogInAcquisitionModeGet", FDwfAnalogInAcquisitionModeGet(hdwf, &acqmode))) return false; 
                    switch(acqmode){
                        case acqmodeSingle: printf("AnalogIn Acquisition Mode=Single\n"); break;
                        case acqmodeScanScreen: printf("AnalogIn Acquisition Mode=ScanScreen\n"); break;
                        case acqmodeScanShift: printf("AnalogIn Acquisition Mode=ScanShift\n"); break;
                        case acqmodeRecord: printf("AnalogIn Acquisition Mode=Record\n"); break;
                        default: printf("AnalogIn Acquisition Mode=UNKNOWN\n"); break;
                    }
                    break;
                }else if(FCmdNumber(szCmd, "run=", "sec", &dParam) || FCmdNumber(szCmd, "run=", "s", &dParam)){
                    if(!FApi("FDwfAnalogInRecordLengthSet", FDwfAnalogInRecordLengthSet(hdwf, dParam))) return false; 
                    {
                        double hzFreq;
                        if(!FApi("FDwfAnalogInFrequencyGet", FDwfAnalogInFrequencyGet(hdwf, &hzFreq))) return false; 
                        if(!FApi("FDwfAnalogInBufferSizeSet", FDwfAnalogInBufferSizeSet(hdwf, (int)(dParam*hzFreq)))) return false; 
                    }
                    break;
                }else if(FCmdMatch(szCmd, "run")){
                    ACQMODE acqmode;
                    if(!FApi("FDwfAnalogInAcquisitionModeGet", FDwfAnalogInAcquisitionModeGet(hdwf, &acqmode))) return false; 
                    if(acqmode==acqmodeSingle){
                        double hzFreq;
                        int cSamples;
                        if(!FApi("FDwfAnalogInFrequencyGet", FDwfAnalogInFrequencyGet(hdwf, &hzFreq))) return false; 
                        if(!FApi("FDwfAnalogInBufferSizeGet", FDwfAnalogInBufferSizeGet(hdwf, &cSamples))) return false; 
                        printf("AnalogIn Run=%s\n", NiceNumber(cSamples/hzFreq, "sec", szNumber));
                    }else{
                        if(!FApi("FDwfAnalogInRecordLengthGet", FDwfAnalogInRecordLengthGet(hdwf, &dParam))) return false; 
                        printf("AnalogIn Run=%s\n", NiceNumber(dParam, "sec", szNumber));
                    }
                    break;
                }else if(FCmdInteger(szCmd, "samples=", &iParam)){
                    if(!FApi("FDwfAnalogInBufferSizeSet", FDwfAnalogInBufferSizeSet(hdwf, iParam))) return false; 
                    {
                        double hzFreq;
                        if(!FApi("FDwfAnalogInFrequencyGet", FDwfAnalogInFrequencyGet(hdwf, &hzFreq))) return false; 
                        if(!FApi("FDwfAnalogInRecordLengthSet", FDwfAnalogInRecordLengthSet(hdwf, 1.0*iParam/hzFreq))) return false; 
                    }
                    break;
                }else if(FCmdMatch(szCmd, "samples")){
                    ACQMODE acqmode;
                    if(!FApi("FDwfAnalogInAcquisitionModeGet", FDwfAnalogInAcquisitionModeGet(hdwf, &acqmode))) return false; 
                    if(acqmode==acqmodeRecord){
                        double dHz;
                        double dRun;
                        if(!FApi("FDwfAnalogInFrequencyGet", FDwfAnalogInFrequencyGet(hdwf, &dHz))) return false; 
                        if(!FApi("FDwfAnalogInRecordLengthGet", FDwfAnalogInRecordLengthGet(hdwf, &dRun))) return false; 
                        printf("AnalogIn Samples=%s\n", NiceNumber(dHz*dRun, "", szNumber));
                    }else{
                        if(!FApi("FDwfAnalogInBufferSizeGet", FDwfAnalogInBufferSizeGet(hdwf, &iParam))) return false; 
                        printf("AnalogIn Samples=%i\n", iParam);
                    }
                    break;
                }else if(FCmdNumber(szCmd, "position=", "s", &dParam)){
                    if(!FApi("FDwfAnalogInTriggerPositionSet", FDwfAnalogInTriggerPositionSet(hdwf, dParam))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "position")){
                    if(!FApi("FDwfAnalogInTriggerPositionGet", FDwfAnalogInTriggerPositionGet(hdwf, &dParam))) return false; 
                    printf("AnalogIn Position=%s\n", NiceNumber(dParam, "s", szNumber));
                    break;
                }else if(FCmdInteger(szCmd, "channel=", &iParam)){
                    int cChannel = 0;
                    if(!FApi("FDwfAnalogInChannelEnableSet", FDwfAnalogInChannelCount(hdwf, &cChannel))) return false;
                    if(iParam<0 || iParam>cChannel){
                        printf("\nERROR: AnalogIn Channel index from 0 (all) to %i accepted\n", cChannel);
                        return false;
                    }
                    idxChannel = iParam;
                    break;
                }else if(FCmdMatch(szCmd, "channel")){
                    printf("AnalogIn Channel=%i\n", idxChannel);
                    break;
                }else if(FCmdText(szCmd, "enable=", &szParam)){
                    if(FCmdMatch(szParam, "yes") || FCmdMatch(szParam, "true") || FCmdMatch(szParam, "y") || FCmdMatch(szParam, "t") || FCmdMatch(szParam, "1")){
                        if(!FApi("FDwfAnalogInChannelEnableSet", FDwfAnalogInChannelEnableSet(hdwf, idxChannel-1, true))) return false;
                    }else if(FCmdMatch(szParam, "no") || FCmdMatch(szParam, "false") || FCmdMatch(szParam, "n") || FCmdMatch(szParam, "f") || FCmdMatch(szParam, "0")){
                        if(!FApi("FDwfAnalogInChannelEnableSet", FDwfAnalogInChannelEnableSet(hdwf, idxChannel-1, false))) return false;
                    }else{
                        printf("\nERROR: Unknown command\n");
                        return false;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "enable")){
                    BOOL f;
                    if(!FApi("FDwfAnalogInChannelEnableGet", FDwfAnalogInChannelEnableGet(hdwf, idxChannel-1, &f))) return false;
                    printf("AnalogIn Channel %i Enable=%i\n", idxChannel, f?1:0);
                    break;
                }else if(FCmdNumber(szCmd, "range=", "V", &dParam)){
                    if(!FApi("FDwfAnalogInChannelRangeSet", FDwfAnalogInChannelRangeSet(hdwf, idxChannel-1, dParam))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "range")){
                    if(!FApi("FDwfAnalogInChannelRangeGet", FDwfAnalogInChannelRangeGet(hdwf, idxChannel-1, &dParam))) return false; 
                    printf("AnalogIn Channel %i Range=%s\n", idxChannel, NiceNumber(dParam, "V", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "offset=", "V", &dParam)){
                    if(!FApi("FDwfAnalogInChannelOffsetSet", FDwfAnalogInChannelOffsetSet(hdwf, idxChannel-1, dParam))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "offset")){
                    if(!FApi("FDwfAnalogInChannelOffsetGet", FDwfAnalogInChannelOffsetGet(hdwf, idxChannel-1, &dParam))) return false; 
                    printf("AnalogIn Channel %i Offset=%s\n", idxChannel, NiceNumber(dParam, "V", szNumber));
                    break;
                }else if(FCmdText(szCmd, "filter=", &szParam)){
                    FILTER filter;
                    if(FCmdMatch(szParam, "decimate")){
                        filter = filterDecimate;
                    }else if(FCmdMatch(szParam, "minmax")){
                        filter = filterMinMax;
                    }else if(FCmdMatch(szParam, "average")){
                        filter = filterAverage;
                    }else{
                        printf("\nERROR: Unknown command\n");
                        return false;
                    }
                    if(!FApi("FDwfAnalogInChannelFilterSet", FDwfAnalogInChannelFilterSet(hdwf, idxChannel-1, filter))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "filter")){
                    FILTER filter;
                    if(!FApi("FDwfAnalogInChannelFilterGet", FDwfAnalogInChannelFilterGet(hdwf, idxChannel-1, &filter))) return false; 
                    switch(filter){
                        case filterDecimate: printf("AnalogIn Channel %i Filter=Decimate\n", idxChannel); break;
                        case filterMinMax: printf("AnalogIn Channel %i Filter=MinMax\n", idxChannel); break;
                        case filterAverage: printf("AnalogIn Channel %i Filter=Average\n", idxChannel); break;
                        default: printf("AnalogIn Channel %i Filter=UNKNOWN\n", idxChannel); break;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "voltage")){
                    if(!FApi("FDwfAnalogInStatus", FDwfAnalogInStatus(hdwf, false, NULL))) return false; 
                    if(idxChannel>0){
                        if(!FApi("FDwfAnalogInStatusSample", FDwfAnalogInStatusSample(hdwf, idxChannel-1, &dParam))) return false;
                        printf("AnalogIn Channel %i Voltage=%s\n", idxChannel, NiceNumber(dParam, "V", szNumber));
                    }else{
                        int c;
                        if(!FApi("FDwfAnalogInChannelCount", FDwfAnalogInChannelCount(hdwf, &c))) return false; 
                        for(int i = 0; i < c; i++){
                            BOOL fEnable;
                            if(!FApi("FDwfAnalogInChannelEnableGet", FDwfAnalogInChannelEnableGet(hdwf, i, &fEnable))) return false;
                            if(!fEnable) continue;
                            if(!FApi("FDwfAnalogInStatusSample", FDwfAnalogInStatusSample(hdwf, i, &dParam))) return false;
                            printf("AnalogIn Channel %i Voltage=%s\n", (i+1), NiceNumber(dParam, "V", szNumber));
                        }
                    }
                    break;
                }else if(FCmdMatch(szCmd, "trigger=none")){
                    if(!FApi("FDwfAnalogInTriggerSourceSet", FDwfAnalogInTriggerSourceSet(hdwf, trigsrcNone))) return false; 
                    break;
                }else if(FCmdInteger(szCmd, "trigger=external", &iParam) || FCmdInteger(szCmd, "trigger=ext", &iParam)){
                    if(!FApi("FDwfAnalogInTriggerSourceSet", FDwfAnalogInTriggerSourceSet(hdwf, trigsrcExternal1+iParam-1))) return false; 
                    break;
                }else if(FCmdInteger(szCmd, "trigger=analogout", &iParam) || FCmdInteger(szCmd, "trigger=ao", &iParam)){
                    if(!FApi("FDwfAnalogInTriggerSourceSet", FDwfAnalogInTriggerSourceSet(hdwf, trigsrcAnalogOut1+iParam-1))) return false; 
                    break;
                }else if(FCmdInteger(szCmd, "trigger=channel", &iParam) || FCmdInteger(szCmd, "trigger=", &iParam)){
                    if(!FApi("FDwfAnalogInTriggerSourceSet", FDwfAnalogInTriggerSourceSet(hdwf, trigsrcDetectorAnalogIn))) return false;
                    if(!FApi("FDwfAnalogInTriggerChannelSet", FDwfAnalogInTriggerChannelSet(hdwf, iParam-1))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "trigger")){
                    TRIGSRC trigsrc;
                    if(!FApi("FDwfAnalogInTriggerSourceGet", FDwfAnalogInTriggerSourceGet(hdwf, &trigsrc))) return false; 
                    if(trigsrc==trigsrcDetectorAnalogIn){
                        int i;
                        if(!FApi("FDwfAnalogInTriggerChannelGet", FDwfAnalogInTriggerChannelGet(hdwf, &i))) return false;
                        printf("AnalogIn Trigger=Channel%i\n", i+1); 
                    }else{
                        printf("AnalogIn Trigger=%s\n", rgszTrig[trigsrc]);
                    }
                    break;
                }else if(FCmdMatch(szCmd, "edge") || FCmdMatch(szCmd, "type=edge")){
                    if(!FApi("FDwfAnalogInTriggerTypeSet", FDwfAnalogInTriggerTypeSet(hdwf, trigtypeEdge))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "pulse") || FCmdMatch(szCmd, "type=pulse")){
                    if(!FApi("FDwfAnalogInTriggerTypeSet", FDwfAnalogInTriggerTypeSet(hdwf, trigtypePulse))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "transition") || FCmdMatch(szCmd, "type=transition")){
                    if(!FApi("FDwfAnalogInTriggerTypeSet", FDwfAnalogInTriggerTypeSet(hdwf, trigtypeTransition))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "type")){
                    TRIGTYPE trigtype;
                    if(!FApi("FDwfAnalogInTriggerTypeGet", FDwfAnalogInTriggerTypeGet(hdwf, &trigtype))) return false;
                    switch(trigtype){
                        case trigtypeEdge:      printf("AnalogIn Trigger Type=Edge\n"); break;
                        case trigtypePulse:      printf("AnalogIn Trigger Type=Pulse\n"); break;
                        case trigtypeTransition: printf("AnalogIn Trigger Type=Transition\n"); break;
                        default:                printf("AnalogIn Trigger Type=UNKNOWN\n"); break;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "rising") || FCmdMatch(szCmd, "positive") || FCmdMatch(szCmd, "condition=rising") || FCmdMatch(szCmd, "condition=positive")){
                    if(!FApi("FDwfAnalogInTriggerConditionSet", FDwfAnalogInTriggerConditionSet(hdwf, trigcondRisingPositive))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "falling") || FCmdMatch(szCmd, "negative") || FCmdMatch(szCmd, "condition=falling") || FCmdMatch(szCmd, "condition=negative")){
                    if(!FApi("FDwfAnalogInTriggerConditionSet", FDwfAnalogInTriggerConditionSet(hdwf, trigcondFallingNegative))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "condition")){
                    TRIGTYPE trigtype;
                    if(!FApi("FDwfAnalogInTriggerTypeGet", FDwfAnalogInTriggerTypeGet(hdwf, &trigtype))) return false;
                    TRIGCOND trigcond;
                    if(!FApi("FDwfAnalogInTriggerConditionGet", FDwfAnalogInTriggerConditionGet(hdwf, &trigcond))) return false; 
                    printf("AnalogIn Trigger Condition=%s\n", ((trigcond==trigcondRisingPositive)?((trigtype==trigtypePulse)?"Positive":"Rising"):((trigtype==trigtypePulse)?"Negative":"Falling")));
                    break;
                }else if(FCmdNumber(szCmd, "auto=", "s", &dParam)){
                    if(!FApi("FDwfAnalogInTriggerAutoTimeoutSet", FDwfAnalogInTriggerAutoTimeoutSet(hdwf, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "auto")){
                    if(!FApi("FDwfAnalogInTriggerAutoTimeoutGet", FDwfAnalogInTriggerAutoTimeoutGet(hdwf, &dParam))) return false;
                    printf("AnalogIn Auto Timeout=%s\n", NiceNumber(dParam, "s", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "level=", "V", &dParam)){
                    if(!FApi("FDwfAnalogInTriggerLevelSet", FDwfAnalogInTriggerLevelSet(hdwf, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "level")){
                    if(!FApi("FDwfAnalogInTriggerLevelGet", FDwfAnalogInTriggerLevelGet(hdwf, &dParam))) return false;
                    printf("AnalogIn Trigger Level=%s\n", NiceNumber(dParam, "V", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "length=", "sec", &dParam) || FCmdNumber(szCmd, "length=", "s", &dParam)){
                    if(!FApi("FDwfAnalogInTriggerLengthSet", FDwfAnalogInTriggerLengthSet(hdwf, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "length")){
                    if(!FApi("FDwfAnalogInTriggerLengthGet", FDwfAnalogInTriggerLengthGet(hdwf, &dParam))) return false;
                    printf("AnalogIn Trigger Length=%s\n", NiceNumber(dParam, "sec", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "hysteresis=", "V", &dParam)){
                    if(!FApi("FDwfAnalogInTriggerHysteresisSet", FDwfAnalogInTriggerHysteresisSet(hdwf, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "hysteresis")){
                    if(!FApi("FDwfAnalogInTriggerHysteresisGet", FDwfAnalogInTriggerHysteresisGet(hdwf, &dParam))) return false;
                    printf("AnalogIn Trigger Hysteresis=%s\n", NiceNumber(dParam, "V", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "holdoff=", "sec", &dParam)){
                    if(!FApi("FDwfAnalogInTriggerHoldOffSet", FDwfAnalogInTriggerHoldOffSet(hdwf, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "holdoff")){
                    if(!FApi("FDwfAnalogInTriggerHoldOffGet", FDwfAnalogInTriggerHoldOffGet(hdwf, &dParam))) return false;
                    printf("AnalogIn Trigger HoldOff=%s\n", NiceNumber(dParam, "sec", szNumber));
                    break;
                }else if(FCmdMatch(szCmd, "less") || FCmdMatch(szCmd, "lengthcondition=less")){
                    if(!FApi("FDwfAnalogInTriggerLengthConditionSet", FDwfAnalogInTriggerLengthConditionSet(hdwf, triglenLess))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "timeout") || FCmdMatch(szCmd, "lengthcondition=timeout")){
                    if(!FApi("FDwfAnalogInTriggerLengthConditionSet", FDwfAnalogInTriggerLengthConditionSet(hdwf, triglenTimeout))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "more") || FCmdMatch(szCmd, "lengthcondition=more")){
                    if(!FApi("FDwfAnalogInTriggerLengthConditionSet", FDwfAnalogInTriggerLengthConditionSet(hdwf, triglenMore))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "lengthcondition")){
                    TRIGLEN triglen;
                    if(!FApi("FDwfAnalogInTriggerLengthConditionGt", FDwfAnalogInTriggerLengthConditionGet(hdwf, &triglen))) return false;
                    switch(triglen){
                        case triglenLess:   printf("AnalogIn Trigger Length Condition=Less\n"); break;
                        case triglenTimeout:printf("AnalogIn Trigger Length Condition=Timeout\n"); break;
                        case triglenMore:   printf("AnalogIn Trigger Length Condition=More\n"); break;
                        default:            printf("AnalogIn Trigger Length Condition=UNKNOWN\n"); break;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "start")){
                    ACQMODE acqmode;
                    if(!FApi("FDwfAnalogInAcquisitionModeGet", FDwfAnalogInAcquisitionModeGet(hdwf, &acqmode))) return false; 
                    if(acqmode==acqmodeRecord){
                        if(!FAnalogInRecord(hdwf)) return false;
                    }else{
                        if(!FAnalogInStart(hdwf)) return false;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "finish")){
                    if(!FAnalogInFinish(hdwf)) return false;
                    break;
                }else if(FCmdText(szCmd, "save=", &szParam)){
                    if(!FAnalogInSave(hdwf, idxChannel, szParam)) return false;
                    break;
                }else{
                    printf("\nERROR: Unknown command\n");
                    return false;
                }
            case TargetAnalogOut: 
                if(FCmdInteger(szCmd, "channel=", &iParam)){
                    int cChannel = 0;
                    if(!FApi("FDwfAnalogOutCount", FDwfAnalogOutCount(hdwf, &cChannel))) return false;
                    if(iParam<0 || iParam>cChannel){
                        printf("\nERROR: AnalogOut Channel index from 0 (all) to %i accepted\n", cChannel);
                        return false;
                    }
                    idxChannel = iParam;
                    break;
                }else if(FCmdMatch(szCmd, "channel")){
                    printf("AnalogOut Channel %i\n", idxChannel);
                    break;
                }else if(FCmdMatch(szCmd, "node=carrier") || FCmdMatch(szCmd, "carrier")){
                    idxNode = AnalogOutNodeCarrier;
                    break;
                }else if(FCmdMatch(szCmd, "node=fm") || FCmdMatch(szCmd, "fm")){
                    idxNode = AnalogOutNodeFM;
                    break;
                }else if(FCmdMatch(szCmd, "node=am") || FCmdMatch(szCmd, "am")){
                    idxNode = AnalogOutNodeAM;
                    break;
                }else if(FCmdMatch(szCmd, "node")){
                    printf("AnalogOut Node=%s\n", rgszAnalogOutNode[idxNode]); break;
                    break;
                }else if(FCmdMatch(szCmd, "trigger=none")){
                    if(!FApi("FDwfAnalogOutTriggerSourceSet", FDwfAnalogOutTriggerSourceSet(hdwf, idxChannel-1, trigsrcNone))) return false;
                    break;
                }else if(FCmdInteger(szCmd, "trigger=external", &iParam) || FCmdInteger(szCmd, "trigger=ext", &iParam)){
                    if(!FApi("FDwfAnalogOutTriggerSourceSet", FDwfAnalogOutTriggerSourceSet(hdwf, idxChannel-1, trigsrcExternal1+iParam-1))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "trigger=analogin")){
                    if(!FApi("FDwfAnalogOutTriggerSourceSet", FDwfAnalogOutTriggerSourceSet(hdwf, idxChannel-1, trigsrcAnalogIn))) return false;
                    break;
                }else if(FCmdInteger(szCmd, "trigger=analogout", &iParam) || FCmdInteger(szCmd, "trigger=ao", &iParam)){
                    if(!FApi("FDwfAnalogOutTriggerSourceSet", FDwfAnalogOutTriggerSourceSet(hdwf, idxChannel-1, trigsrcAnalogOut1+iParam-1))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "trigger")){
                    TRIGSRC trigsrc;
                    if(!FApi("FDwfAnalogOutTriggerSourceGet", FDwfAnalogOutTriggerSourceGet(hdwf, idxChannel-1, &trigsrc))) return false; 
                    printf("AnalogOut Channel %i Trigger=%s\n", idxChannel, rgszTrig[trigsrc]); break;
                    break;
                }else if(FCmdNumber(szCmd, "run=", "sec", &dParam) || FCmdNumber(szCmd, "run=", "s", &dParam)){
                    if(!FApi("FDwfAnalogOutRunSet", FDwfAnalogOutRunSet(hdwf, idxChannel-1, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "run")){
                    if(!FApi("FDwfAnalogOutRunGet", FDwfAnalogOutRunGet(hdwf, idxChannel-1, &dParam))) return false;
                    printf("AnalogOut Channel %i Run=%s\n", idxChannel, NiceNumber(dParam, "sec", szNumber));
                    break;
                }else if(FCmdInteger(szCmd, "samples=", &iParam)){
                    FUNC func = funcDC;
                    if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(hdwf, idxChannel-1, idxNode, &func))) return false;
                    if(func != funcPlay){
                        printf("\nERROR: Use samples only with Play function\n");
                        return false;
                    }
                    double hzFreq;
                    if(!FApi("FDwfAnalogOutNodeFrequencyGet", FDwfAnalogOutNodeFrequencyGet(hdwf, idxChannel-1, idxNode, &hzFreq))) return false; 
                    if(!FApi("FDwfAnalogOutRunSet", FDwfAnalogOutRunSet(hdwf, idxChannel-1, 1.0*iParam/hzFreq))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "samples")){
                    FUNC func = funcDC;
                    if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(hdwf, idxChannel-1, idxNode, &func))) return false;
                    if(func != funcPlay){
                        printf("\nERROR: Use samples only with Play function\n");
                        return false;
                    }
                    double hzFreq;
                    if(!FApi("FDwfAnalogOutNodeFrequencyGet", FDwfAnalogOutNodeFrequencyGet(hdwf, idxChannel-1, idxNode, &hzFreq))) return false; 
                    if(!FApi("FDwfAnalogOutRunGet", FDwfAnalogOutRunGet(hdwf, idxChannel-1, &dParam))) return false; 
                    printf("AnalogOut Channel %i Node %s Samples=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceInteger((int)(0.499+dParam*hzFreq), "", szNumber));
                    break;
                }else if(FCmdInteger(szCmd, "periods=", &iParam)){
                    FUNC func = funcDC;
                    if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(hdwf, idxChannel-1, idxNode, &func))) return false;
                    if(func == funcPlay){
                        printf("\nERROR: Use periods for other functions than Play\n");
                        return false;
                    }
                    double hzFreq;
                    if(!FApi("FDwfAnalogOutNodeFrequencyGet", FDwfAnalogOutNodeFrequencyGet(hdwf, idxChannel-1, idxNode, &hzFreq))) return false; 
                    if(!FApi("FDwfAnalogOutRunSet", FDwfAnalogOutRunSet(hdwf, idxChannel-1, 1.0*iParam/hzFreq))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "periods")){
                    FUNC func = funcDC;
                    if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(hdwf, idxChannel-1, idxNode, &func))) return false;
                    if(func == funcPlay){
                        printf("\nERROR: Use periods for other functions than Play\n");
                        return false;
                    }
                    double hzFreq;
                    if(!FApi("FDwfAnalogOutNodeFrequencyGet", FDwfAnalogOutNodeFrequencyGet(hdwf, idxChannel-1, idxNode, &hzFreq))) return false; 
                    if(!FApi("FDwfAnalogOutRunGet", FDwfAnalogOutRunGet(hdwf, idxChannel-1, &dParam))) return false; 
                    printf("AnalogOut Channel %i %s Periods=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceNumber(dParam*hzFreq, "", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "wait=", "sec", &dParam) || FCmdNumber(szCmd, "wait=", "s", &dParam)){
                    if(!FApi("FDwfAnalogOutWaitSet", FDwfAnalogOutWaitSet(hdwf, idxChannel-1, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "wait")){
                    if(!FApi("FDwfAnalogOutWaitGet", FDwfAnalogOutWaitGet(hdwf, idxChannel-1, &dParam))) return false;
                    printf("AnalogOut Channel %i Wait=%s\n", idxChannel, NiceNumber(dParam, "sec", szNumber));
                    break;
                }else if(FCmdInteger(szCmd, "repeat=", &iParam)){
                    if(!FApi("FDwfAnalogOutRepeatSet", FDwfAnalogOutRepeatSet(hdwf, idxChannel-1, iParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "repeat")){
                    if(!FApi("FDwfAnalogOutRepeatGet", FDwfAnalogOutRepeatGet(hdwf, idxChannel-1, &iParam))) return false;
                    printf("AnalogOut Channel %i Repeat=%s\n", idxChannel, NiceInteger(iParam, "", szNumber));
                    break;
                }else if(FCmdText(szCmd, "enable=", &szParam)){
                    if(FCmdMatch(szParam, "yes") || FCmdMatch(szParam, "true") || FCmdMatch(szParam, "y") || FCmdMatch(szParam, "t") || FCmdMatch(szParam, "1")){
                        if(!FApi("FDwfAnalogOutNodeEnableSet", FDwfAnalogOutNodeEnableSet(hdwf, idxChannel-1, idxNode, true))) return false;
                    }else if(FCmdMatch(szParam, "no") || FCmdMatch(szParam, "false") || FCmdMatch(szParam, "n") || FCmdMatch(szParam, "f") || FCmdMatch(szParam, "0")){
                        if(!FApi("FDwfAnalogOutNodeEnableSet", FDwfAnalogOutNodeEnableSet(hdwf, idxChannel-1, idxNode, false))) return false;
                    }else{
                        printf("\nERROR: Unknown command\n");
                        return false;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "enable")){
                    BOOL f;
                    if(!FApi("FDwfAnalogOutNodeEnableGet", FDwfAnalogOutNodeEnableGet(hdwf, idxChannel-1, idxNode, &f))) return false;
                    printf("AnalogOut Channel %i %s Enable=%i\n", idxChannel, rgszAnalogOutNode[idxNode], f?1:0);
                    break;
                }else if(FCmdMatch(szCmd, "function")){
                    FUNC func = funcDC;
                    if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(hdwf, idxChannel-1, idxNode, &func))) return false;
                    printf("AnalogOut Channel %i %s Function=%s\n", idxChannel, rgszAnalogOutNode[idxNode], rgszFunc[func]); 
                    break;
                }else if(FCmdMatch(szCmd, "dc") || FCmdMatch(szCmd, "function=dc")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcDC))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "sine") || FCmdMatch(szCmd, "function=sine")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcSine))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "square") || FCmdMatch(szCmd, "function=square")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcSquare))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "triangle") || FCmdMatch(szCmd, "function=triangle")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcTriangle))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "rampup") || FCmdMatch(szCmd, "function=rampup")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcRampUp))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "rampdown") || FCmdMatch(szCmd, "function=rampdown")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcRampDown))) return false; 
                    break;
                }else if(FCmdMatch(szCmd, "noise") || FCmdMatch(szCmd, "function=noise")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcNoise))) return false;  
                    break;
                }else if(FCmdMatch(szCmd, "custom") || FCmdMatch(szCmd, "function=custom")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcCustom))) return false;
                    break;
                }else if(FCmdText(szCmd, "custom=", &szParam)){
                    if(idxChannel>0){
                        if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcCustom))) return false;
                    }
                    if(!FAnalogOutLoadCustomFile(hdwf, idxChannel, idxNode, szParam)) return false;
                    break;
                }else if(FCmdMatch(szCmd, "play") || FCmdMatch(szCmd, "function=play")){
                    if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcPlay))) return false;
                    break;
                }else if(FCmdText(szCmd, "play=", &szParam)){
                    if(idxChannel>0){
                        if(!FApi("FDwfAnalogOutNodeFunctionSet", FDwfAnalogOutNodeFunctionSet(hdwf, idxChannel-1, idxNode, funcPlay))) return false;
                    }
                    if(!FAnalogOutLoadPlayFile(hdwf, idxChannel, idxNode, szParam)) return false;
                    break;
                }else if(FCmdNumber(szCmd, "frequency=", "Hz", &dParam) || FCmdNumber(szCmd, "freq=", "Hz", &dParam)){
                    if(!FApi("FDwfAnalogOutNodeFrequencySet", FDwfAnalogOutNodeFrequencySet(hdwf, idxChannel-1, idxNode, dParam))) return false;   
                    break;
                }else if(FCmdMatch(szCmd, "frequency") || FCmdMatch(szCmd, "freq")){
                    if(!FApi("FDwfAnalogOutNodeFrequencyGet", FDwfAnalogOutNodeFrequencyGet(hdwf, idxChannel-1, idxNode, &dParam))) return false; 
                    printf("AnalogOut Channel %i %s Frequency=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceNumber(dParam, "Hz", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "amplitude=", (char*)(idxNode==AnalogOutNodeCarrier?"V":"%"), &dParam)){
                    if(!FApi("FDwfAnalogOutNodeAmplitudeSet", FDwfAnalogOutNodeAmplitudeSet(hdwf, idxChannel-1, idxNode, dParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "amplitude")){
                    if(!FApi("FDwfAnalogOutNodeAmplitudeGet", FDwfAnalogOutNodeAmplitudeGet(hdwf, idxChannel-1, idxNode, &dParam))) return false;    
                    printf("AnalogOut Channel %i %s Amplitude=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceNumber(dParam, (char*)(idxNode==AnalogOutNodeCarrier?"V":"%"), szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "offset=", (char*)(idxNode==AnalogOutNodeCarrier?"V":"%"), &dParam)){
                    if(!FApi("FDwfAnalogOutNodeOffsetSet", FDwfAnalogOutNodeOffsetSet(hdwf, idxChannel-1, idxNode, dParam))) return false;    
                    break;
                }else if(FCmdMatch(szCmd, "offset")){
                    if(!FApi("FDwfAnalogOutNodeOffsetGet", FDwfAnalogOutNodeOffsetGet(hdwf, idxChannel-1, idxNode, &dParam))) return false;    
                    printf("AnalogOut Channel %i %s Offset=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceNumber(dParam, (char*)(idxNode==AnalogOutNodeCarrier?"V":"%"), szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "symmetry=", "%", &dParam)){
                    if(!FApi("FDwfAnalogOutNodeSymmetrySet", FDwfAnalogOutNodeSymmetrySet(hdwf, idxChannel-1, idxNode, dParam))) return false;    
                    break;
                }else if(FCmdMatch(szCmd, "symmetry")){
                    if(!FApi("FDwfAnalogOutNodeSymmetryGet", FDwfAnalogOutNodeSymmetryGet(hdwf, idxChannel-1, idxNode, &dParam))) return false;    
                    printf("AnalogOut Channel %i %s Symmetry=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceNumber(dParam, "%", szNumber));
                    break;
                }else if(FCmdNumber(szCmd, "phase=", "deg", &dParam)){
                    if(!FApi("FDwfAnalogOutNodePhaseSet", FDwfAnalogOutNodePhaseSet(hdwf, idxChannel-1, idxNode, dParam))) return false;    
                    break;
                }else if(FCmdMatch(szCmd, "phase")){
                    if(!FApi("FDwfAnalogOutNodePhaseGet", FDwfAnalogOutNodePhaseGet(hdwf, idxChannel-1, idxNode, &dParam))) return false;    
                    printf("AnalogOut Channel %i %s Phase=%s\n", idxChannel, rgszAnalogOutNode[idxNode], NiceNumber(dParam, "deg", szNumber));
                    break;
                }else if(FCmdMatch(szCmd, "start")){
                    if(idxChannel>0){
                        FUNC func;
                        if(!FApi("FDwfAnalogOutNodeFunctionGet", FDwfAnalogOutNodeFunctionGet(hdwf, idxChannel-1, idxNode, &func))) return false;
                        if(func==funcPlay){
                            if(!FAnalogOutPlay(hdwf, idxChannel, idxNode)) return false;
                        }else{
                            if(!FAnalogOutStart(hdwf, idxChannel)) return false;
                        }
                    }else{
                        if(!FAnalogOutStart(hdwf)) return false;
                    }
                    break;
                }else if(FCmdMatch(szCmd, "finish")){
                    if(!FAnalogOutFinish(hdwf, idxChannel)) return false;
                    break;
                }else if(FCmdMatch(szCmd, "stop")){
                    if(!FAnalogOutStop(hdwf, idxChannel)) return false;
                    break;
                }else{
                    printf("\nERROR: Unknown command: %s\n", szCmd);
                    return false;
                }
            case TargetAnalogIO: 
                if(FCmdInteger(szCmd, "channel=", &iParam)){
                    int cChannel = 0;
                    if(!FApi("FDwfAnalogIOChannelCount", FDwfAnalogIOChannelCount(hdwf, &cChannel))) return false;
                    if(iParam<0 || iParam>cChannel){
                        printf("\nERROR: AnalogIO Channel index from 0 (all) to %i accepted\n", cChannel);
                        return false;
                    }
                    idxChannel = iParam;
                    break;
                }else if(FCmdMatch(szCmd, "channel")){
                    printf("AnalogIO Channel %i\n", idxChannel);
                    break;
                }else if(FCmdInteger(szCmd, "masterenable=", &iParam)){
                    if(!FApi("FDwfAnalogIOEnableSet", FDwfAnalogIOEnableSet(hdwf, (iParam!=0)?true:false))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "masterenable")){
                    BOOL f;
                    if(!FApi("FDwfAnalogIOStatus", FDwfAnalogIOStatus(hdwf))) return false; 
                    if(!FApi("FDwfAnalogIOEnableStatus", FDwfAnalogIOEnableStatus(hdwf, &f))) return false;    
                    printf("AnalogIO MasterEnable=%i\n", f?1:0);
                    break;
                }else {
                    int n;
                    if(!FApi("FDwfAnalogIOChannelInfo", FDwfAnalogIOChannelInfo(hdwf, idxChannel-1, &n))) return false;
                    bool fDone = false;
                    for(int j = 0; j < n; j++){
                        char szNode[36];
                        char szUnit[8];
                        if(!FApi("FDwfAnalogIOChannelNodeName", FDwfAnalogIOChannelNodeName(hdwf, idxChannel-1, j, szNode, szUnit))) return false;
                        if(FCmdMatch(szCmd, szNode)){
                            if(!FApi("FDwfAnalogIOStatus", FDwfAnalogIOStatus(hdwf))) return false; 
                            if(!FApi("FDwfAnalogIOChannelNodeStatus", FDwfAnalogIOChannelNodeStatus(hdwf, idxChannel-1, j, &dParam))) return false;    
                            printf("AnalogIO Channel %i %s=%s\n", idxChannel, szNode, NiceNumber(dParam,szUnit,szNumber));
                            fDone = true;
                            break;
                        }
                        strcat(szNode, "=");
                        if(FCmdNumber(szCmd, szNode, szUnit, &dParam)){
                            if(!FApi("FDwfAnalogIOChannelNodeSet", FDwfAnalogIOChannelNodeSet(hdwf, idxChannel-1, j, dParam))) return false;
                            fDone = true;
                            break;
                        }
                    }
                    if(!fDone){
                        printf("\nERROR: Unknown command\n");
                        return false;
                    }
                    break;
                }
            case TargetDigitalIO: 
                if(FCmdInteger(szCmd, "oe=", &iParam)){
                    if(!FApi("FDwfDigitalIOOutputEnableSet", FDwfDigitalIOOutputEnableSet(hdwf, iParam))) return false;    
                    break;
                }else if(FCmdMatch(szCmd, "oe")){
                    unsigned int iMask;
                    if(!FApi("FDwfDigitalIOOutputEnableInfo", FDwfDigitalIOOutputEnableInfo(hdwf, (unsigned int*)&iMask))) return false;
                    if(!FApi("FDwfDigitalIOOutputEnableGet", FDwfDigitalIOOutputEnableGet(hdwf, (unsigned int*)&iParam))) return false;    
                    printf("Digital OE=%s\n", NiceHex(iParam, iMask, szNumber));
                    break;
                }else if(FCmdInteger(szCmd, "out=", &iParam) || FCmdInteger(szCmd, "io=", &iParam)){
                    if(!FApi("FDwfDigitalIOOutputSet", FDwfDigitalIOOutputSet(hdwf, iParam))) return false;
                    break;
                }else if(FCmdMatch(szCmd, "out")){
                    unsigned int iMask;
                    if(!FApi("FDwfDigitalIOOutputInfo", FDwfDigitalIOOutputInfo(hdwf, (unsigned int*)&iMask))) return false;
                    if(!FApi("FDwfDigitalIOOutputGet", FDwfDigitalIOOutputGet(hdwf, (unsigned int*)&iParam))) return false;
                    printf("Digital Out=%s\n", NiceHex(iParam, iMask, szNumber));
                    break;
                }else if(FCmdMatch(szCmd, "in") || FCmdMatch(szCmd, "io")){
                    unsigned int iMask;
                    if(!FApi("FDwfDigitalIOStatus", FDwfDigitalIOStatus(hdwf))) return false;
                    if(!FApi("FDwfDigitalIOInputInfo", FDwfDigitalIOInputInfo(hdwf, (unsigned int*)&iMask))) return false;
                    if(!FApi("FDwfDigitalIOInputStatus", FDwfDigitalIOInputStatus(hdwf, (unsigned int*)&iParam))) return false; 
                    printf("Digital In=%s\n", NiceHex(iParam, iMask, szNumber));
                    break;
                }else{
                    printf("\nERROR: Unknown command\n");
                    return false;
                }
            default:
                printf("\nERROR: Unknown command\n");
                return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------ */
/***    FExecute
**
**  Parameters:
**      argc    - number of arguments
**      argv    - arguments array
**      file    - command file
**
**  Return Value:
**      returns true on success
**
**  Errors:
**      none
**      
**  Description:
**      This function executes the commands provided in array,
**  application arguments or listed in a text file.
** 
*/
bool FExecute(int argc, char* argv[], FILE* file){
    char* szarg = NULL;
    char* szParam = NULL;
    char szLine[256];

    for(int iarg = 1; (file == NULL && iarg < argc && (szarg = argv[iarg]) != NULL) || (file != NULL && (szarg = fgets(szLine, 255, file)) != NULL); iarg++){

        // Trim start space, tab.
        while(szarg[0]==' ' || szarg[0]=='\t') szarg++;

        // Remove \n\r from line end.
        if(strlen(szarg)>0 && szarg[strlen(szarg)-1]=='\n') szarg[strlen(szarg)-1] = 0;
        if(strlen(szarg)>0 && szarg[strlen(szarg)-1]=='\r') szarg[strlen(szarg)-1] = 0;

        // Trim end space, tab.
        while(strlen(szarg)>0 && (szarg[strlen(szarg)-1]==' ' || szarg[strlen(szarg)-1]=='\t')) szarg[strlen(szarg)-1] = 0;

        // If debug level is >= 1 print line.
        if(debugLevel>=1) printf(">%s\n", szarg);
        
        // Skip empty or comment lines.
        if(szarg[0]==0 || szarg[0]=='\\' || szarg[0]=='#' || szarg[0]=='/') continue;
        
        // Process command text.
        if(!FCommand(szarg)){
            if(file==NULL) printf("ERROR in command: %i\n", iarg);
            else printf("ERROR in line: %i\n", iarg);
            printf("  command: %s\n", szarg);
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]){

    if(argc<=1 || FCmdMatch(argv[1], "help")){
        printf("Digilent WaveForms utility application. Version 0.1.1\n");
        printf("Supports the following commands as application argument or listed in text file in separate lines. The assignment commands can also be used to print the current value.\n");
        printf("\n");
        printf(" connect\n\tAutomatically connect to the first available device.\n");
        printf(" enum|enumerate\n\tEnumerate connected devices.\n");
        printf(" connect=<index|name|SN> \n\tConnect to the device by enumeration index, name or SN.\n");
        printf(" disconnect\n\tDisconnect from device.\n");
        printf(" info|information\n\tPrint information about the device or selected instrument, channel.\n");
        printf(" show\n\tPrint configuration and reading of the device or selected instrument, channel.\n");
        printf(" execute=<file>\n\tExecutes the command file.\n");
        printf(" reset\n\tReset all the configurations to default values.\n");
        printf(" pause=<s>\n\tPause execution for specified seconds. Ex: pause=0.1, pause=100ms\n");
        printf(" print=<text>\n\tPrint the text on the output.\n");
        printf(" datetime\n\tPrint the date and time on the output.\n");
        printf(" time\n\tPrint the time on the output.\n");
        printf(" debug=<level>\n\tDebug level: 0-disabled, 1-print input commands,\n\t2-API calls, 3-time after API execution\n");
        printf(" watch=<sec>\n\tWatch timeout for play/record processes. Default is 10s, 0 disables it.\n");
        printf("\n");
        printf(" target=<dev|device> or dev|device\n\tSelect Device\n");
        printf("   start\n\tStart the enabled AnalogOut and AnalogIn channels.\n");
        printf("\n");
        printf(" target=<ai|analogin> or ai|analogin\n\tSelect AnalogIn\n");
        printf("   mode=<single|record> or single|record\n\tSelect acquisition mode.\n");
        printf("     \tsingle - samples are stored in device buffer, up to maximum frequency\n");
        printf("           position parameter is relative to buffer middle point\n");
        printf("     \trecord - long acquisition, frequency limited by USB bandwidth\n");
        printf("   freq|frequency=<value>\n\tSet sample frequency expressed in hertz.\n");
        printf("   channel=<index>\n\tSelect channel to configure.\n");
        printf("     enable=<1|0>\n\tEnable or disable channel.\n");
        printf("     range=<value>\n\tSet channel voltage range.\n");
        printf("     offset=<value>\n\tSet channel offset.\n");
        printf("     filter=<average|decimate|minmax>\n\tSet channel filter.\n");
        printf("     voltage\n\tRead channel voltage input.\n");
        printf("   trigger=<none|channel#|ext#|analogout#>\n\tSelect trigger source: none (default),\n\t channel index, external trigger or an analogout channel.\n");
        printf("   type=<edge|pulse|transition> or edge|pulse|transition\n\tConfigure the trigger type.\n");
        printf("   condition=<rising|falling|positive|negative> or rising|falling|...\n\tSet trigger edge, polarity or slope.\n");
        printf("   lengthcondition=<less|timeout|more> or less|timeout|more\n\tLength condition for pulse or transition triggering.\n");
        printf("   level=<volts>\n\tSet trigger level.\n");
        printf("   length=<sec>\n\tSet trigger pulse or transition length.\n");
        printf("   hysteresis=<volts>\n\tSet trigger hysteresis, level +/-hysteresis.\n");
        printf("   holdoff=<sec>\n\tSet trigger holdoff, to trigger on start of burst signal.\n");
        printf("   run=<sec>\n\tSet the record acquisition length, expressed in seconds.\n");
        printf("   samples=<value>\n\tSet the number of samples for single acquisition.\n");
        printf("   position=<sec>\n\tSet the horizontal trigger position for single acquisition.\n");
        printf("   start\n\tStart the analog in acquisition or performs recording.\n");
        printf("   finish\n\tFinish the analog in single acquisition. \n\tWait to complete and save the samples in file.\n");
        printf("   save=<file>\n\tSave acquisition data to file.\n");
        printf("\n");
        printf(" target=<ao|analogout> or ao|analogout\n\tSelect AnalogOut\n");
        printf("   channel=<index>\n\tSelect channel to enabled and configure.\n\tWith 0 (default) commands will be applied for each enabled channel.\n");
        printf("     node=<carrier|fm|am>\n\tSelect node.\n");
        printf("       enable=<1|0>\n\tEnable or disable channel.\n");
        printf("       function=<dc|sine|square|triangle|rampup|rampdown|noise|custom|play>\n\tSet function.\n");
        printf("       custom=<file>\n\tLoad custom samples from file for custom channels.\n\tIf channel is selected sets custom function and loads samples for this.\n");
        printf("       play=<file>\n\tLoad samples from file for custom channels.\n\tIf channel is selected sets play function and loads samples for this.\n");
        printf("       freq|frequency=<value>\n\tSet frequency in hertz.\n");
        printf("       amplitude=<value>\n\tSet amplitude.\n");
        printf("       offset=<value>\n\tSet offset.\n");
        printf("       symmetry=<value>\n\tSet symmetry of standard waveforms.\n");
        printf("       phase=<value>\n\tSet phase of standard waveforms.\n");
        printf("     trigger=<none|ext#|analogin|analogout#>\n\tSet the trigger source.\n");
        printf("     wait=<value>\n\tSet wait time, expressed in seconds.\n");
        printf("     run=<value>\n\tSet the run time, expressed in seconds. For 0 will run continuously.\n"); 
        printf("     samples=<value>\n\tSet the number of samples to play. This sets Run = Samples/Frequency\n");
        printf("     periods=<value>\n\tSet the number of periods to generate. This sets Run = Periods/Frequency\n");
        printf("     repeat=<value>\n\tSet the number of repeats. For 0 will repeat infinitely.\n");
        printf("     start\n\tStarts the selected analog out channel.\n\tWhen channel is 0 all enabled channels will be started.\n");
        printf("     finish\n\tWaits for the analog out generator channel to finish.\n");
        printf("     stop\n\tStops the analog out generator channel.\n");
        printf("\n");
        printf(" target=<aio|analogio> or aio|analogio\n\tSelect AnalogIO\n");
        printf("   masterenable=<1|0>\n\tEnable or disable master switch.\n");
        printf("   channel=<index>\n\tSelect channel. \n");
        printf("     enable=<1|0>\n\tEnable or disable channel.\n");
        printf("     voltage=<value>\n\tSet channel voltage output.\n");
        printf("     voltage\n\tRead channel voltage input.\n");
        printf("     current=<value>\n\tSet channel current output.\n");
        printf("     current\n\tRead channel current input.\n");
        printf("     temperature\n\tRead channel temperature input.\n");
        printf("\n");
        printf(" target=<dio|digitalio> or dio|digitalio\n\tSelect DigitalIO\n");
        printf("   oe=<value>\n\tSet output enable as bit field set.\n");
        printf("   out=<value>\n\tSet output value as bit field set.\n");
        printf("   in\n\tRead input value as bit field set.\n");
        printf("\n");
        printf("Examples:\n");
        printf("    dwfcmd enum\n");
        printf("    dwfcmd connect digitalio oe=0x0F out=0x0A pause=10ms in\n");
        printf("    dwfcmd sample1.txt\n");
        printf("    Set range and print the actual obtained range:\n");
        printf("    dwfcmd connect analogio channel=1 range=5V range\n");
        printf("\n");
        return 0;
    }

    // When there is one argument and this is a file open and execute it
    // otherwise execute arguments.

    FILE* file = NULL;
    if(argc == 2) {
        file = fopen(argv[1], "r");
    }

    if(file){
        if(!FExecute(0, NULL, file)){
            printf("  file: %s\n", argv[1]);            
        }
        fclose(file);
    }else{
        if(!FExecute(argc, argv, NULL)){
        }
    }

    FApi("FDwfDeviceCloseAll", FDwfDeviceCloseAll());
	return 0;
}

