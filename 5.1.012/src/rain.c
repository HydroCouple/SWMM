
/*!
 * \file climate.c
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version 5.1.012
 * \description
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

//-----------------------------------------------------------------------------
//   rain.c
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    03/20/14  (Build 5.1.001)
//            08/05/15  (Build 5.1.010)
//            08/22/16  (Build 5.1.011)
//   Author:  L. Rossman
//
//   Places rainfall data from external files into a SWMM rainfall
//   interface file.
//
//   The following types of external data files are supported:
//   NWS_TAPE:            NCDC NWS TD 3240 or 3260 data in fixed field widths
//   NWS_SPACE_DELIMITED: NCDC NWS TD (DSI) 3240 or 3260 data in space delimited
//                        format, with or without header lines, with or without
//                        station name
//   NWS_COMMA_DELIMITED: NCDC NWS TD (DSI) 3240 or 3260 data in comma delimited
//                        format, with or without header lines
//   NWS_ONLINE_60:       NCDC NWS hourly space delimited online format
//   NWS_ONLINE_15:       NCDC NWS fifteen minute space delimited online format
//   AES_HLY:             Canadian AES hourly data with 3-digit year
//   CMC_HLY:             Canadian CMC hourly data in HLY03 or HLY21 format
//   CMC_FIF:             Canadian CMC fifteen minute data in in FIF21 format
//   STD_SPACE_DELIMITED: standard SWMM space delimted format:
//                        StaID  Year  Month  Day  Hour  Minute  Rainfall
//
//   The layout of the SWMM binary rainfall interface file is:
//     File stamp ("SWMM5-RAIN") (10 bytes)
//     Number of SWMM rain gages in file (4-byte int)
//     Repeated for each rain gage:
//       recording station ID (not SWMM rain gage ID) (MAXMSG+1 (=80) bytes)
//       gage recording interval (seconds) (4-byte int)
//       starting byte of rain data in file (4-byte int)
//       ending byte+1 of rain data in file (4-byte int)
//     For each gage:
//       For each time period with non-zero rain:
//         Date/time for start of period (8-byte double)
//         Rain depth (inches) (4-byte float)
//
//   Release 5.1.010:
//   - Modified error message for records out of sequence in std. format file.
//
//   Release 5.1.011:
//   - Can now read decimal rainfall values in newer NWS online format.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum RainFileFormat {UNKNOWN_FORMAT, NWS_TAPE, NWS_SPACE_DELIMITED,
                     NWS_COMMA_DELIMITED, NWS_ONLINE_60, NWS_ONLINE_15,
                     AES_HLY, CMC_HLY, CMC_FIF, STD_SPACE_DELIMITED};
enum ConditionCodes {NO_CONDITION, ACCUMULATED_PERIOD, DELETED_PERIOD,
                     MISSING_PERIOD};


//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  rain_open   (called by swmm_start in swmm5.c)
//  rain_close  (called by swmm_end in swmm5.c)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void createRainFile(Project *project, int count);
static int  rainFileConflict(Project *project, int i);
static void initRainFile(Project *project);
static int  findGageInFile(Project *project, int i, int kount);
static int  addGageToRainFile(Project *project, int i);
static int  findFileFormat(Project *project, FILE *f, int i, int *hdrLines);
static int  findNWSOnlineFormat(Project *project, FILE *f, char *line);
static void readFile(Project *project, FILE *f, int fileFormat, int hdrLines, DateTime day1,
            DateTime day2);
static int  readNWSLine(Project *project, char *line, int fileFormat, DateTime day1,
            DateTime day2);
static int  readNwsOnlineValue(char* s, long* v, char* flag);                  //(5.1.011)
static int  readCMCLine(Project *project, char *line, int fileFormat, DateTime day1,
            DateTime day2);
static int  readStdLine(Project *project, char *line, DateTime day1, DateTime day2);
static void saveAccumRainfall(Project *project, DateTime date1, int hour, int minute, long v);
static void saveRainfall(Project *project, DateTime date1, int hour, int minute, float x,
            char isMissing);
static void setCondition(Project *project, char flag);
static int  getNWSInterval(char *elemType);
static int  parseStdLine(Project *project, char *line, int *year, int *month, int *day,
            int *hour, int *minute, float *value);

//=============================================================================

void  rain_open(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: opens binary rain interface file and RDII processor.
//
{
    int i;
    int count;

    // --- see how many gages get their data from a file
    count = 0;
    for (i = 0; i < project->Nobjects[GAGE]; i++)
    {
        if ( project->Gage[i].dataSource == RAIN_FILE ) count++;
    }
    project->Frain.file = NULL;
    if ( count == 0 )
    {
        project->Frain.mode = NO_FILE;
    }

    // --- see what kind of rain interface file to open
    else switch ( project->Frain.mode )
    {
      case SCRATCH_FILE:
        getTempFileName(project, project->Frain.name);
        if ( (project->Frain.file = fopen(project->Frain.name, "w+b")) == NULL)
        {
            report_writeErrorMsg(project, ERR_RAIN_FILE_SCRATCH, "");
            return;
        }
        break;

      case USE_FILE:
        if ( (project->Frain.file = fopen(project->Frain.name, "r+b")) == NULL)
        {
            report_writeErrorMsg(project, ERR_RAIN_FILE_OPEN, project->Frain.name);
            return;
        }
        break;

      case SAVE_FILE:
        if ( (project->Frain.file = fopen(project->Frain.name, "w+b")) == NULL)
        {
            report_writeErrorMsg(project, ERR_RAIN_FILE_OPEN, project->Frain.name);
            return;
        }
        break;
    }

    // --- create new rain file if required
    if ( project->Frain.mode == SCRATCH_FILE || project->Frain.mode == SAVE_FILE )
    {
        createRainFile(project, count);
    }

    // --- initialize rain file
    if ( project->Frain.mode != NO_FILE ) initRainFile(project);

    // --- open RDII processor (creates/opens a RDII interface file)
    rdii_openRdii(project);
}

//=============================================================================

void rain_close(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: closes rain interface file and RDII processor.
//
{
    if ( project->Frain.file )
    {
        fclose(project->Frain.file);
        if ( project->Frain.mode == SCRATCH_FILE ) remove(project->Frain.name);
    }
    project->Frain.file = NULL;
    rdii_closeRdii(project);
}

//=============================================================================

void createRainFile(Project *project, int count)
//
//  Input:   count = number of files to include in rain interface file
//  Output:  none
//  Purpose: adds rain data from all rain gage files to the interface file.
//
{
    int   i, k;
    int   kount = count;               // number of gages in data file
    int   filePos1;                    // starting byte of gage's header data
    int   filePos2;                    // starting byte of gage's rain data
    int   filePos3;                    // starting byte of next gage's data
    int   interval;                    // recording interval (sec)
    int   dummy = -1;
    char  staID[MAXMSG+1];             // gage's ID name
    char  fileStamp[] = "SWMM5-RAIN";

    // --- make sure interface file is open and no error condition
    if ( project->ErrorCode || !project->Frain.file ) return;

    // --- write file stamp & # gages to file
    fwrite(fileStamp, sizeof(char), strlen(fileStamp), project->Frain.file);
    fwrite(&kount, sizeof(int), 1, project->Frain.file);
    filePos1 = ftell(project->Frain.file);

    // --- write default fill-in header records to file for each gage
    //     (will be replaced later with actual records)
    if ( count > 0 ) report_writeRainStats(project, -1, &project->RainStats);
    for ( i = 0;  i < count; i++ )
    {
        fwrite(staID, sizeof(char), MAXMSG+1, project->Frain.file);
        for ( k = 1; k <= 3; k++ )
            fwrite(&dummy, sizeof(int), 1, project->Frain.file);
    }
    filePos2 = ftell(project->Frain.file);

    // --- loop through project's  rain gages,
    //     looking for ones using rain files
    for ( i = 0; i < project->Nobjects[GAGE]; i++ )
    {
        if ( project->ErrorCode || project->Gage[i].dataSource != RAIN_FILE ) continue;
        if ( rainFileConflict(project, i) ) break;

        // --- position rain file to where data for gage will begin
        fseek(project->Frain.file, filePos2, SEEK_SET);

        // --- add gage's data to rain file
        if ( addGageToRainFile(project, i) )
        {
            // --- write header records for gage to beginning of rain file
            filePos3 = ftell(project->Frain.file);
            fseek(project->Frain.file, filePos1, SEEK_SET);
            sstrncpy(staID, project->Gage[i].staID, MAXMSG);
            interval = project->Interval;
            fwrite(staID,      sizeof(char), MAXMSG+1, project->Frain.file);
            fwrite(&interval,  sizeof(int), 1, project->Frain.file);
            fwrite(&filePos2,  sizeof(int), 1, project->Frain.file);
            fwrite(&filePos3,  sizeof(int), 1, project->Frain.file);
            filePos1 = ftell(project->Frain.file);
            filePos2 = filePos3;
            report_writeRainStats(project, i, &project->RainStats);
        }
    }

    // --- if there was an error condition, then delete newly created file
    if ( project->ErrorCode )
    {
        fclose(project->Frain.file);
        project->Frain.file = NULL;
        remove(project->Frain.name);
    }
}

//=============================================================================

int rainFileConflict(Project *project, int i)
//
//  Input:   i = rain gage index
//  Output:  returns 1 if file conflict found, 0 if not
//  Purpose: checks if a rain gage's station ID matches another gage's
//           station ID but the two use different rain data files.
//
{
    int j;
    char* staID = project->Gage[i].staID;
    char* fname = project->Gage[i].fname;
    for (j = 1; j < i; j++)
    {
        if ( strcomp(project->Gage[j].staID, staID) && !strcomp(project->Gage[j].fname, fname) )
        {
            report_writeErrorMsg(project, ERR_RAIN_FILE_CONFLICT, project->Gage[i].ID);
            return 1;
        }
    }
    return 0;
}

//=============================================================================

int addGageToRainFile(Project *project, int i)
//
//  Input:   i = rain gage index
//  Output:  returns 1 if successful, 0 if not
//  Purpose: adds a gage's rainfall record to rain interface file
//
{
    FILE* f;                           // pointer to rain file
    int   fileFormat;                  // file format code
    int   hdrLines;                    // number of header lines skipped

    // --- let project->StationID point to NULL
    project->StationID = NULL;

    // --- check that rain file exists
    if ( (f = fopen(project->Gage[i].fname, "rt")) == NULL )
        report_writeErrorMsg(project, ERR_RAIN_FILE_DATA, project->Gage[i].fname);
    else
    {
        fileFormat = findFileFormat(project, f, i, &hdrLines);
        if ( fileFormat == UNKNOWN_FORMAT )
        {
            report_writeErrorMsg(project, ERR_RAIN_FILE_FORMAT, project->Gage[i].fname);
        }
        else
        {
            project->GageIndex = i;
            readFile(project, f, fileFormat, hdrLines, project->Gage[i].startFileDate,
                     project->Gage[i].endFileDate);
        }
        fclose(f);
    }
    if ( project->ErrorCode ) return 0;
    else
    return 1;
}

//=============================================================================

void initRainFile(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: initializes rain interface file for reading.
//
{
    char  fileStamp[] = "SWMM5-RAIN";
    char  fStamp[] = "SWMM5-RAIN";
    int   i;
    int   kount;
    long  filePos;

    // --- make sure interface file is open and no error condition
    if ( project->ErrorCode || !project->Frain.file ) return;

    // --- check that interface file contains proper file stamp
    rewind(project->Frain.file);
    fread(fStamp, sizeof(char), strlen(fileStamp), project->Frain.file);
    if ( strcmp(fStamp, fileStamp) != 0 )
    {
        report_writeErrorMsg(project, ERR_RAIN_IFACE_FORMAT, "");
        return;
    }
    fread(&kount, sizeof(int), 1, project->Frain.file);
    filePos = ftell(project->Frain.file);

    // --- locate information for each raingage in interface file
    for ( i = 0; i < project->Nobjects[GAGE]; i++ )
    {
        if ( project->ErrorCode || project->Gage[i].dataSource != RAIN_FILE ) continue;

        // --- match station ID for gage with one in file
        fseek(project->Frain.file, filePos, SEEK_SET);
        if ( !findGageInFile(project, i, (int)kount) ||
             project->Gage[i].startFilePos == project->Gage[i].endFilePos )
        {
            report_writeErrorMsg(project, ERR_RAIN_FILE_GAGE, project->Gage[i].ID);
        }
    }
}

//=============================================================================

int findGageInFile(Project *project, int i, int kount)
//
//  Input:   i     = rain gage index
//           kount = number of rain gages stored on interface file
//  Output:  returns TRUE if successful, FALSE if not
//  Purpose: checks if rain gage's station ID appears in interface file.
//
{
    int   k;
    int  interval;
    int  filePos1, filePos2;
    char  staID[MAXMSG+1];

    for ( k = 1; k <= kount; k++ )
    {
        fread(staID,      sizeof(char), MAXMSG+1, project->Frain.file);
        fread(&interval,  sizeof(int), 1, project->Frain.file);
        fread(&filePos1,  sizeof(int), 1, project->Frain.file);
        fread(&filePos2,  sizeof(int), 1, project->Frain.file);
        if ( strcmp(staID, project->Gage[i].staID) == 0 )
        {
            // --- match found; save file parameters
            project->Gage[i].rainType     = RAINFALL_VOLUME;
            project->Gage[i].rainInterval = interval;
            project->Gage[i].startFilePos = (long)filePos1;
            project->Gage[i].endFilePos   = (long)filePos2;
            project->Gage[i].currentFilePos = project->Gage[i].startFilePos;
            return TRUE;
        }
    }
    return FALSE;
}

//=============================================================================

int findFileFormat(Project *project, FILE *f, int i, int *hdrLines)
//
//  Input:   f = ptr. to rain gage's rainfall data file
//           i = rain gage index
//  Output:  hdrLines  = number of header lines found in data file;
//           returns type of format used in a rainfall data file
//  Purpose: finds the format of a gage's rainfall data file.
//
{
    int   fileFormat;
    int   lineCount;
    int   maxCount = 5;
    int   n;
    int   div;
    long  sn2;
    char  recdType[4];
    char  elemType[5];
    char  coopID[6];
    char  line[MAXLINE];
    int   year, month, day, hour, minute;
    int   elem;
    float x;

    // --- check first few lines for known formats
    fileFormat = UNKNOWN_FORMAT;
    project->hasStationName = FALSE;
    project->UnitsFactor = 1.0;
    project->Interval = 0;
    *hdrLines = 0;
    for (lineCount = 1; lineCount <= maxCount; lineCount++)
    {
        if ( fgets(line, MAXLINE, f) == NULL ) return fileFormat;

        // --- check for NWS space delimited format
        n = sscanf(line, "%6ld %2d %4s", &sn2, &div, elemType);
        if ( n == 3 )
        {
            project->Interval = getNWSInterval(elemType);
            project->TimeOffset = project->Interval;
            if ( project->Interval > 0 )
            {
                fileFormat = NWS_SPACE_DELIMITED;
                break;
            }
        }

        // --- check for NWS space delimited format w/ station name
        n = sscanf(&line[37], "%2d %4s %2s %4d", &div, elemType, recdType, &year);
        if ( n == 4 )
        {
            project->Interval = getNWSInterval(elemType);
            project->TimeOffset = project->Interval;
            if ( project->Interval > 0 )
            {
                fileFormat = NWS_SPACE_DELIMITED;
                project->hasStationName = TRUE;
                break;
            }
        }

        // --- check for NWS coma delimited format
        n = sscanf(line, "%6ld,%2d,%4s", &sn2, &div, elemType);
        if ( n == 3 )
        {
            project->Interval = getNWSInterval(elemType);
            project->TimeOffset = project->Interval;
            if ( project->Interval > 0 )
            {
                fileFormat = NWS_COMMA_DELIMITED;
                break;
            }
        }

        // --- check for NWS comma delimited format w/ station name
        n = sscanf(&line[37], "%2d,%4s,%2s,%4d", &div, elemType, recdType, &year);
        if ( n == 4 )
        {
            project->Interval = getNWSInterval(elemType);
            project->TimeOffset = project->Interval;
            if ( project->Interval > 0 )
            {
                fileFormat = NWS_COMMA_DELIMITED;
                project->hasStationName = TRUE;
                break;
            }
        }

        // --- check for NWS TAPE format
        n = sscanf(line, "%3s%6ld%2d%4s", recdType, &sn2, &div, elemType);
        if ( n == 4 )
        {
            project->Interval = getNWSInterval(elemType);
            project->TimeOffset = project->Interval;
            if ( project->Interval > 0 )
            {
                fileFormat = NWS_TAPE;
                break;
            }
        }

        // --- check for NWS Online Retrieval format
        n = sscanf(line, "%5s%6ld", coopID, &sn2);
        if ( n == 2 && strcmp(coopID, "COOP:") == 0 )
        {
            fileFormat = findNWSOnlineFormat(project, f, line);
            break;
        }

        // --- check for AES type
        n = sscanf(line, "%7ld%3d%2d%2d%3d", &sn2, &year, &month, &day, &elem);
        if ( n == 5 )
        {
            if ( elem == 123 && strlen(line) >= 185 )
            {
                fileFormat = AES_HLY;
                project->Interval = 3600;
                project->TimeOffset = project->Interval;
                project->UnitsFactor = 1.0/MMperINCH;
                break;
            }
        }

        // --- check for CMC types
        n = sscanf(line, "%7ld%4d%2d%2d%3d", &sn2, &year, &month, &day, &elem);
        if ( n == 5 )
        {
            if ( elem == 159 && strlen(line) >= 691 )
            {
                fileFormat = CMC_FIF;
                project->Interval = 900;
            }
            else if ( elem == 123 && strlen(line) >= 186 )
            {
                fileFormat = CMC_HLY;
                project->Interval = 3600;
            }
            if ( fileFormat == CMC_FIF || fileFormat == CMC_HLY )
            {
                project->TimeOffset = project->Interval;
                project->UnitsFactor = 1.0/MMperINCH;
                break;
            }
        }

        // --- check for standard format
        if ( parseStdLine(project, line, &year, &month, &day, &hour, &minute, &x) )
        {
            fileFormat = STD_SPACE_DELIMITED;
            project->RainType = project->Gage[i].rainType;
            project->Interval = project->Gage[i].rainInterval;
            if ( project->Gage[i].rainUnits == SI ) project->UnitsFactor = 1.0/MMperINCH;
            project->TimeOffset = 0;
            project->StationID = project->Gage[i].staID;
            break;
        }
        (*hdrLines)++;

    }
    if ( fileFormat != UNKNOWN_FORMAT ) project->Gage[i].rainInterval = project->Interval;
    return fileFormat;
}

//=============================================================================

int findNWSOnlineFormat(Project *project, FILE *f, char *line)
//
//  Input:   f = pointer to rainfall data file
//           line = line read from rainfall data file
//  Output:
//  Purpose: determines the file format for an NWS Online Retrieval data file.
//
{
    int n;
    int fileFormat = UNKNOWN_FORMAT;
    char* str;

    // --- read in the first header line of the file
    rewind(f);
    fgets(line, MAXLINE, f);

    // --- if 'HPCP' appears then file is for hourly data
    if ( (str = strstr(line, "HPCP")) != NULL )
    {
        project->Interval = 3600;
        project->TimeOffset = project->Interval;
        project->ValueOffset = str - line;
        fileFormat = NWS_ONLINE_60;
    }

    // --- if 'QPCP" appears then file is for 15 minute data
    else if ( (str = strstr(line, "QPCP")) != NULL )
    {
        project->Interval = 900;
        project->TimeOffset = project->Interval;
        project->ValueOffset = str - line;
        fileFormat = NWS_ONLINE_15;
    }
    else return UNKNOWN_FORMAT;

    // --- find position in line where rainfall date begins
    //     (11 characters before last occurrence of ':')
    // --- read in first line of data
    for (n = 1; n <= 5; n++)
    {
        if ( fgets(line, MAXLINE, f) == NULL ) return UNKNOWN_FORMAT;
        if ( strstr(line, "COOP:") == NULL ) continue;

        // --- find pointer to last occurrence of time separator character (':')
        str = strrchr(line, ':');
        if ( str == NULL ) return UNKNOWN_FORMAT;

        // --- use pointer arithmetic to convert pointer to character position
        n = str - line;
        project->DataOffset = n - 11;
        return fileFormat;
    }
    return UNKNOWN_FORMAT;
}

//=============================================================================

int getNWSInterval(char *elemType)
//
//  Input:   elemType = code from NWS rainfall file
//  Output:  returns rainfall recording interval (sec)
//  Purpose: decodes NWS rain gage recording interval value
//
{
    if      ( strcmp(elemType, "HPCP") == 0 ) return 3600; // 1 hr rainfall
    else if ( strcmp(elemType, "QPCP") == 0 ) return 900;  // 15 min rainfall
    else if ( strcmp(elemType, "QGAG") == 0 ) return 900;  // 15 min rainfall
    else return 0;
}

//=============================================================================

void readFile(Project *project, FILE *f, int fileFormat, int hdrLines, DateTime day1,
              DateTime day2)
//
//  Input:   f          = ptr. to gage's rainfall data file
//           fileFormat = code of data file's format
//           hdrLines   = number of header lines in data file
//           day1       = starting day of record of interest
//           day2       = ending day of record of interest
//  Output:  none
//  Purpose: reads rainfall records from gage's data file to interface file.
//
{
    char line[MAXLINE];
    int  i, n;

    rewind(f);
    project->RainStats.startDate  = NO_DATE;
    project->RainStats.endDate    = NO_DATE;
    project->RainStats.periodsRain = 0;
    project->RainStats.periodsMissing = 0;
    project->RainStats.periodsMalfunc = 0;
    project->RainAccum = 0.0;
    project->AccumStartDate = NO_DATE;
    project->PreviousDate = NO_DATE;

    for (i = 1; i <= hdrLines; i++)
    {
        if ( fgets(line, MAXLINE, f) == NULL ) return;
    }
    while ( fgets(line, MAXLINE, f) != NULL )
    {
       switch (fileFormat)
       {
         case STD_SPACE_DELIMITED:
          n = readStdLine(project, line, day1, day2);
          break;

         case NWS_TAPE:
         case NWS_SPACE_DELIMITED:
         case NWS_COMMA_DELIMITED:
         case NWS_ONLINE_60:
         case NWS_ONLINE_15:
           n = readNWSLine(project, line, fileFormat, day1, day2);
           break;

         case AES_HLY:
         case CMC_FIF:
         case CMC_HLY:
           n = readCMCLine(project, line, fileFormat, day1, day2);
           break;

         default:
           n = -1;
           break;
       }
       if ( n < 0 ) break;
    }
}

//=============================================================================

int readNWSLine(Project *project, char *line, int fileFormat, DateTime day1, DateTime day2)
//
//  Input:   line       = line of data from rainfall data file
//           fileFormat = code of data file's format
//           day1       = starting day of record of interest
//           day2       = ending day of record of interest
//  Output:  returns -1 if past end of desired record, 0 if data line could
//           not be read successfully or 1 if line read successfully
//  Purpose: reads a line of data from a rainfall data file and writes its
//           data to the rain interface file.
//
{
    char     flag1, flag2, isMissing;
    DateTime date1;
    long     result = 1;
    int      k, y, m, d, n;
    int      hour, minute;
    long     v;
    float    x;
    int      lineLength = strlen(line)-1;
    int      nameLength = 0;

    // --- get year, month, & day from line
    switch ( fileFormat )
    {
      case NWS_TAPE:
        if ( lineLength <= 30 ) return 0;
        if (sscanf(&line[17], "%4d%2d%4d%3d", &y, &m, &d, &n) < 4) return 0;
        k = 30;
        break;

      case NWS_SPACE_DELIMITED:
        if ( project->hasStationName ) nameLength = 31;
        if ( lineLength <= 28 + nameLength ) return 0;
        k = 18 + nameLength;
        if (sscanf(&line[k], "%4d %2d %2d", &y, &m, &d) < 3) return 0;
        k = k + 10;
        break;

      case NWS_COMMA_DELIMITED:
        if ( lineLength <= 28 ) return 0;
        if ( sscanf(&line[18], "%4d,%2d,%2d", &y, &m, &d) < 3 ) return 0;
        k = 28;
        break;

      case NWS_ONLINE_60:
      case NWS_ONLINE_15:
        if ( lineLength <= project->DataOffset + 23 ) return 0;
        if ( sscanf(&line[project->DataOffset], "%4d%2d%2d", &y, &m, &d) < 3 ) return 0;
        k = project->DataOffset + 8;
        break;

      default: return 0;
    }

    // --- see if date is within period of record requested
    date1 = datetime_encodeDate(y, m, d);
    if ( day1 != NO_DATE && date1 < day1 ) return 0;
    if ( day2 != NO_DATE && date1 > day2 ) return -1;

    // --- read each recorded rainfall time, value, & codes from line
    while ( k < lineLength )
    {
		flag1 = 0;
        flag2 = 0;
		v = 99999;
		hour = 25;
		minute = 0;
        switch ( fileFormat )
        {
          case NWS_TAPE:
            n = sscanf(&line[k], "%2d%2d%6ld%c%c",
                       &hour, &minute, &v, &flag1, &flag2);
            k += 12;
            break;

          case NWS_SPACE_DELIMITED:
            n = sscanf(&line[k], " %2d%2d %6ld %c %c",
                       &hour, &minute, &v, &flag1, &flag2);
            k += 16;
            break;

          case NWS_COMMA_DELIMITED:
            n = sscanf(&line[k], ",%2d%2d,%6ld,%c,%c",
                       &hour, &minute, &v, &flag1, &flag2);
            k += 16;
            break;

          case NWS_ONLINE_60:
          case NWS_ONLINE_15:
              n = sscanf(&line[k], " %2d:%2d", &hour, &minute);
                          n += readNwsOnlineValue(&line[project->ValueOffset], &v, &flag1);         //(5.1.011)

              // --- ending hour 0 is really hour 24 of previous day
              if ( hour == 0 )
              {
                  hour = 24;
                  date1 -= 1.0;
              }
              k += lineLength;
              break;

          default: n = 0;
        }

        // --- check that we at least have an hour, minute & value
        //     (codes might be left off of the line)
        if ( n < 3 || hour >= 25 ) break;

        // --- set special condition code & update daily & hourly counts

        setCondition(project, flag1);
        if ( project->Condition == DELETED_PERIOD ||
             project->Condition == MISSING_PERIOD ||
             flag1 == 'M' ) isMissing = TRUE;
        else if ( v >= 9999 ) isMissing = TRUE;
        else isMissing = FALSE;

        // --- handle accumulation codes
        if ( flag1 == 'a' )
        {
            project->AccumStartDate = date1 + datetime_encodeTime(hour, minute, 0);
        }
        else if ( flag1 == 'A' )
        {
            saveAccumRainfall(project, date1, hour, minute, v);
        }

        // --- handle all other conditions
        else
        {
            // --- convert rain measurement to inches & save it
            x = (float)v / 100.0f;
            if ( x > 0 || isMissing )
                saveRainfall(project, date1, hour, minute, x, isMissing);
        }

        // --- reset condition code if special condition period ended
        if ( flag1 == 'A' || flag1 == '}' || flag1 == ']') project->Condition = 0;
    }
    return result;
}

//=============================================================================

////  New function added to release 5.1.011  ////                              //(5.1.011)

int readNwsOnlineValue(char* s, long* v, char* flag)
//
//  Input:   s = portion of rainfall record in NWS online format
//  Output:  v = rainfall amount in hundreths of an inch
//           flag = special condition flag
//           returns number of items read from s.
//  Purpose: reads rainfall value and condition flag from a NWS online
//           rainfall record.
//
{
	int    n;
	float  x = 99.99;

	// --- check for newer format of decimal inches
	if ( strchr(s, '.') )
    {
		n = sscanf(s, "%f %c", &x, flag);

		// --- convert to integer hundreths of an inch
		*v = (long)(100.0f * x + 0.5f);
	}

	// --- older format of hundreths of an inch
	else n = sscanf(s, "%ld %c", v, flag);
	return n;
}

//=============================================================================

void  setCondition(Project *project, char flag)
{
    switch ( flag )
    {
      case 'a':
      case 'A':
        project->Condition = ACCUMULATED_PERIOD;
        break;
      case '{':
      case '}':
        project->Condition = DELETED_PERIOD;
        break;
      case '[':
      case ']':
        project->Condition = MISSING_PERIOD;
        break;
      default:
        project->Condition = NO_CONDITION;
    }
}

//=============================================================================

int readCMCLine(Project *project, char *line, int fileFormat, DateTime day1, DateTime day2)
//
//  Input:   line = line of data from rainfall data file
//           fileFormat = code of data file's format
//           day1 = starting day of record of interest
//           day2 = ending day of record of interest
//  Output:  returns -1 if past end of desired record, 0 if data line could
//           not be read successfully or 1 if line read successfully
//  Purpose: reads a line of data from an AES or CMC rainfall data file and
//           writes its data to the rain interface file.
//
{
    char     flag, isMissing;
    DateTime date1;
    long     sn, v;
    int      col, j, jMax, elem, y, m, d, hour, minute;
    float    x;

    // --- get year, month, day & element code from line
    if ( fileFormat == AES_HLY )
    {
        if ( sscanf(line, "%7ld%3d%2d%2d%3d", &sn, &y, &m, &d, &elem) < 5 )
            return 0;
        if ( y < 100 ) y = y + 2000;
        else           y = y + 1000;
        col = 17;
    }
    else
    {
        if ( sscanf(line, "%7ld%4d%2d%2d%3d", &sn, &y, &m, &d, &elem) < 5 )
            return 0;
        col = 18;
    }

    // --- see if date is within period of record requested
    date1 = datetime_encodeDate(y, m, d);
    if ( day1 != NO_DATE && date1 < day1 ) return 0;
    if ( day2 != NO_DATE && date1 > day2 ) return -1;

    // --- make sure element code is for rainfall
    if ( fileFormat == AES_HLY && elem != 123 ) return 0;
    else if ( fileFormat == CMC_FIF && elem != 159 ) return 0;
    else if ( fileFormat == CMC_HLY && elem != 123 ) return 0;

    // --- read rainfall from each recording interval
    hour = 0;                          // starting hour
    minute = 0;                        // starting minute
    jMax = 24;                         // # recording intervals
    if ( fileFormat == CMC_FIF ) jMax = 96;
    for (j=1; j<=jMax; j++)
    {
        if ( sscanf(&line[col], "%6ld%c", &v, &flag) < 2 ) return 0;
        col += 7;
        if ( v == -99999 ) isMissing = TRUE;
        else               isMissing = FALSE;

        // --- convert rain measurement from 0.1 mm to inches and save it
        x = (float)( (double)v / 10.0 / MMperINCH);
        if ( x > 0 || isMissing)
        {
            saveRainfall(project, date1, hour, minute, x, isMissing);
        }

        // --- update hour & minute for next interval
        if ( fileFormat == CMC_FIF )
        {
            minute += 15;
            if ( minute == 60 )
            {
                minute = 0;
                hour++;
            }
        }
        else hour++;
    }
    return 1;
}

//=============================================================================

int readStdLine(Project *project, char *line, DateTime day1, DateTime day2)
//
//  Input:   line = line of data from a standard rainfall data file
//           day1 = starting day of record of interest
//           day2 = ending day of record of interest
//  Output:  returns -1 if past end of desired record, 0 if data line could
//           not be read successfully or 1 if line read successfully
//  Purpose: reads a line of data from a standard rainfall data file and
//           writes its data to the rain interface file.
//
{
    DateTime date1;
    DateTime date2;
    int      year, month, day, hour, minute;
    float    x;

    // --- parse data from input line
    if (!parseStdLine(project, line, &year, &month, &day, &hour, &minute, &x)) return 0;

    // --- see if date is within period of record requested
    date1 = datetime_encodeDate(year, month, day);
    if ( day1 != NO_DATE && date1 < day1 ) return 0;
    if ( day2 != NO_DATE && date1 > day2 ) return -1;

    // --- see if record is out of sequence
    date2 = date1 + datetime_encodeTime(hour, minute, 0);
    if ( date2 <= project->PreviousDate )
    {
        report_writeErrorMsg(project, ERR_RAIN_FILE_SEQUENCE, project->Gage[project->GageIndex].fname);   //(5.1.010)
        report_writeLine(project, line);
        return -1;
    }
    project->PreviousDate = date2;

    switch (project->RainType)
    {
      case RAINFALL_INTENSITY:
        x = x * project->Interval / 3600.0f;
        break;

      case CUMULATIVE_RAINFALL:
        if ( x >= project->RainAccum )
        {
            x = x - project->RainAccum;
            project->RainAccum += x;
        }
        else project->RainAccum = x;
        break;
    }
    x *= (float)project->UnitsFactor;

    // --- save rainfall to binary interface file
    saveRainfall(project, date1, hour, minute, x, FALSE);
    return 1;
}

//=============================================================================

int parseStdLine(Project *project, char *line, int *year, int *month, int *day, int *hour,
                 int *minute, float *value)
//
//  Input:   line = line of data from a standard rainfall data file
//  Output:  *year = year when rainfall occurs
//           *month = month of year when rainfall occurs
//           *day = day of month when rainfall occurs
//           *hour = hour of day when rainfall occurs
//           *minute = minute of hour when rainfall occurs
//           *value = rainfall value (user units);
//           returns 0 if data line could not be parsed successfully or
//           1 if line parsed successfully
//  Purpose: parses a line of data from a standard rainfall data file.
//
{
    int n;
    char token[MAXLINE];

    n = sscanf(line, "%s %d %d %d %d %d %f", token, year, month, day, hour, minute, value);
    if ( n < 7 ) return 0;
    if ( project->StationID != NULL && !strcomp(token, project->StationID) ) return 0;
    return 1;
}

//=============================================================================

void saveAccumRainfall(Project *project, DateTime date1, int hour, int minute, long v)
//
//  Input:   date1 = date of latest rainfall reading (in DateTime format)
//           hour = hour of day of latest rain reading
//           minute = minute of hour of latest rain reading
//           v = accumulated rainfall reading in hundreths of inches
//  Output:  none
//  Purpose: divides accumulated rainfall evenly into individual recording
//           periods over the accumulation period and writes each period's
//           rainfall to the binary rainfall file.
//
{
    DateTime date2;
    int      n, j;
    float    x;

    // --- return if accumulated start date is missing
    if ( project->AccumStartDate == NO_DATE ) return;

    // --- find number of recording intervals over accumulation period
    date2 = date1 + datetime_encodeTime(hour, minute, 0);
    n = (datetime_timeDiff(date2, project->AccumStartDate) / project->Interval) + 1;

    // --- update count of rain or missing periods
    if ( v == 99999 )
    {
        project->RainStats.periodsMissing += n;
        return;
    }
    project->RainStats.periodsRain += n;

    // --- divide accumulated amount evenly into each period
    x = (float)v / (float)n / 100.0f;

    // --- save this amount to file for each period
    if ( x > 0.0f )
    {
        date2 = datetime_addSeconds(project->AccumStartDate, -project->TimeOffset);
        if ( project->RainStats.startDate == NO_DATE ) project->RainStats.startDate = date2;
        for (j = 0; j < n; j++)
        {
            fwrite(&date2, sizeof(DateTime), 1, project->Frain.file);
            fwrite(&x, sizeof(float), 1, project->Frain.file);
            date2 = datetime_addSeconds(date2, project->Interval);
            project->RainStats.endDate = date2;
        }
    }

    // --- reset start of accumulation period
    project->AccumStartDate = NO_DATE;
}


//=============================================================================

void saveRainfall(Project *project, DateTime date1, int hour, int minute, float x, char isMissing)
//
//  Input:   date1 = date of rainfall reading (in DateTime format)
//           hour = hour of day of current rain reading
//           minute = minute of hour of current rain reading
//           x = rainfall reading in inches
//           isMissing = TRUE if rainfall value is missing
//  Output:  none
//  Purpose: writes current rainfall reading from an external rainfall file
//           to project's binary rainfall file.
//
{
    DateTime date2;
    double   seconds;

    if ( isMissing ) project->RainStats.periodsMissing++;
    else             project->RainStats.periodsRain++;

    // --- if rainfall not missing then save it to rainfall interface file
    if ( !isMissing )
    {
        seconds = 3600*hour + 60*minute - project->TimeOffset;
        date2 = datetime_addSeconds(date1, seconds);

        // --- write date & value (in inches) to interface file
        fwrite(&date2, sizeof(DateTime), 1, project->Frain.file);
        fwrite(&x, sizeof(float), 1, project->Frain.file);

        // --- update actual start & end of record dates
        if ( project->RainStats.startDate == NO_DATE ) project->RainStats.startDate = date2;
        project->RainStats.endDate = date2;
    }
}
//=============================================================================
