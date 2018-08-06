
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
//   lidproc.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/12   (Build 5.1.001)
//             05/19/14   (Build 5.1.006)
//             09/15/14   (Build 5.1.007)
//             03/19/15   (Build 5.1.008)
//             04/30/15   (Build 5.1.009)
//             08/05/15   (Build 5.1.010)
//             08/01/16   (Build 5.1.011)
//             03/14/17   (Build 5.1.012)
//   Author:   L. Rossman (US EPA)
//
//   This module computes the hydrologic performance of an LID (Low Impact
//   Development) unit at a given point in time.
//
//   Build 5.1.007:
//   - Euler integration now applied to all LID types except Vegetative
//     Swale which continues to use successive approximation.
//   - LID layer flux routines were re-written to more accurately model
//     flooded conditions.
//
//   Build 5.1.008:
//   - MAX_STATE_VARS replaced with MAX_LAYERS.
//   - Optional soil layer added to Porous Pavement LID.
//   - Rooftop Disconnection added to types of LIDs.
//   - Separate accounting of drain flows added.
//   - Indicator for currently wet LIDs added.
//   - Detailed reporting procedure fixed.
//   - Possibile negative head on Bioretention Cell drain avoided.
//   - Bug in computing flow through Green Roof drainage mat fixed.
//
//   Build 5.1.009:
//   - Fixed typo in net flux rate for vegetative swale LID.
//
//   Build 5.1.010:
//   - New modified version of Green-Ampt used for surface layer infiltration.
//
//   Build 5.1.011:
//   - Re-named STOR_INFIL to STOR_EXFIL and StorageInfil to project->StorageExfil to
//     better reflect their meaning.
//   - Evaporation rates from sub-surface layers reduced by fraction of 
//     surface that is pervious (applies to block paver systems)
//   - Flux rate routines for LIDs with underdrains modified to produce more
//     physically meaningful results.
//   - Reporting of detailed results re-written.
//
//   Build 5.1.012:
//   - Modified upper limit for soil layer percolation.
//   - Modified upper limit on surface infiltration into rain gardens.
//   - Modified upper limit on drain flow for LIDs with storage layers.
//   - Used re-defined wasDry variable for LID reports to fix duplicate lines.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lid.h"
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
#define STOPTOL  0.00328     // integration error tolerance in ft (= 1 mm)
#define MINFLOW  2.3e-8      // flow cutoff for dry conditions (= 0.001 in/hr)

//-----------------------------------------------------------------------------
//  Enumerations
//-----------------------------------------------------------------------------
enum LidLayerTypes {
    SURF,                    // surface layer
    SOIL,                    // soil layer
    STOR,                    // storage layer
    PAVE,                    // pavement layer
    DRAIN};                  // underdrain system

enum LidRptVars {
    SURF_INFLOW,             // inflow to surface layer
    TOTAL_EVAP,              // evaporation rate from all layers
    SURF_INFIL,              // infiltration into surface layer
    PAVE_PERC,               // percolation through pavement layer             //(5.1.008)
    SOIL_PERC,               // percolation through soil layer
    STOR_EXFIL,              // exfiltration out of storage layer              //(5.1.011)
    SURF_OUTFLOW,            // outflow from surface layer
    STOR_DRAIN,              // outflow from storage layer
    SURF_DEPTH,              // ponded depth on surface layer
    PAVE_DEPTH,              // water level in pavement layer                  //(5.1.011)
    SOIL_MOIST,              // moisture content of soil layer
    STOR_DEPTH,              // water level in storage layer
    MAX_RPT_VARS};

////  Added to release 5.1.008.  ////                                          //(5.1.008)
//-----------------------------------------------------------------------------
//  Imported variables 
//-----------------------------------------------------------------------------
//extern char project->HasWetLids;      // TRUE if any LIDs are wet (declared in runoff.c)

//-----------------------------------------------------------------------------
//  External Functions (declared in lid.h)
//-----------------------------------------------------------------------------
// lidproc_initWaterBalance  (called by lid_initState)
// lidproc_getOutflow        (called by evalLidUnit in lid.c)
// lidproc_saveResults       (called by evalLidUnit in lid.c)

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
static void   barrelFluxRates(Project *project, double x[], double f[]);
static void   biocellFluxRates(Project *project, double x[], double f[]);
static void   greenRoofFluxRates(Project *project, double x[], double f[]);
static void   pavementFluxRates(Project *project, double x[], double f[]);
static void   trenchFluxRates(Project *project, double x[], double f[]);
static void   swaleFluxRates(Project *project, double x[], double f[]);
static void   roofFluxRates(Project *project, double x[], double f[]);                           //(5.1.008)

static double getSurfaceOutflowRate(Project *project, double depth);
static double getSurfaceOverflowRate(Project *project, double* surfaceDepth);
static double getPavementPermRate(Project *project);
static double getSoilPercRate(Project *project, double theta);                                   //(5.1.007)
static double getStorageExfilRate(Project *project);                                       //(5.1.011)
static double getStorageDrainRate(Project *project, double storageDepth, double soilTheta,       //(5.1.011)
              double paveDepth, double surfaceDepth);                          //(5.1.011)
static double getDrainMatOutflow(Project *project, double depth);
static void   getEvapRates(Project *project, double surfaceVol, double paveVol,                  //(5.1.008)
              double soilVol, double storageVol, double pervFrac);             //(5.1.011)

static void   updateWaterBalance(Project *project, TLidUnit *lidUnit, double inflow,
                                 double evap, double infil, double surfFlow,
                                 double drainFlow, double storage);

static int    modpuls_solve(Project *project, int n, double* x, double* xOld, double* xPrev,
                            double* xMin, double* xMax, double* xTol,
                            double* qOld, double* q, double dt, double omega,  //(5.1.007)
                            void (*derivs)(Project *, double*, double*));


//=============================================================================

void lidproc_initWaterBalance(TLidUnit *lidUnit, double initVol)
//
//  Purpose: initializes the water balance components of a LID unit.
//  Input:   lidUnit = a particular LID unit
//           initVol = initial water volume stored in the unit (ft)
//  Output:  none
//
{
    lidUnit->waterBalance.inflow = 0.0;
    lidUnit->waterBalance.evap = 0.0;
    lidUnit->waterBalance.infil = 0.0;
    lidUnit->waterBalance.surfFlow = 0.0;
    lidUnit->waterBalance.drainFlow = 0.0;
    lidUnit->waterBalance.initVol = initVol;
    lidUnit->waterBalance.finalVol = initVol;                                  //(5.1.008)
}

//=============================================================================

////  This function was modified for release 5.1.008.  ////                    //(5.1.008)

double lidproc_getOutflow(Project *project, TLidUnit* lidUnit, TLidProc* lidProc, double inflow,
                          double evap, double infil, double maxInfil,
                          double tStep, double* lidEvap,
                          double* lidInfil, double* lidDrain)
//
//  Purpose: computes runoff outflow from a single LID unit.
//  Input:   lidUnit  = ptr. to specific LID unit being analyzed
//           lidProc  = ptr. to generic LID process of the LID unit
//           inflow   = runoff rate captured by LID unit (ft/s)
//           evap     = potential evaporation rate (ft/s)
//           infil    = infiltration rate to native soil (ft/s)
//           maxInfil = max. infiltration rate to native soil (ft/s)
//           tStep    = time step (sec)
//  Output:  lidEvap  = evaporation rate for LID unit (ft/s)
//           lidInfil = infiltration rate for LID unit (ft/s)
//           lidDrain = drain flow for LID unit (ft/s)
//           returns surface runoff rate from the LID unit (ft/s)
//
{
    int    i;
    double x[MAX_LAYERS];        // layer moisture levels
    double xOld[MAX_LAYERS];     // work vector
    double xPrev[MAX_LAYERS];    // work vector
    double xMin[MAX_LAYERS];     // lower limit on moisture levels
    double xMax[MAX_LAYERS];     // upper limit on moisture levels
    double fOld[MAX_LAYERS];     // previously computed flux rates
    double f[MAX_LAYERS];        // newly computed flux rates

    // convergence tolerance on moisture levels (ft, moisture fraction , ft)
    double xTol[MAX_LAYERS] = {STOPTOL, STOPTOL, STOPTOL, STOPTOL};

    double omega = 0.0;          // integration time weighting

    //... define a pointer to function that computes flux rates through the LID
    void (*fluxRates) (Project*, double *, double *) = NULL;

    //... save references to the LID process and LID unit
    project->theLidProc = lidProc;
    project->theLidUnit = lidUnit;

    //... save evap, max. infil. & time step to shared variables
    project->lidProcEvapRate = evap;
    project->lidProcMaxNativeInfil = maxInfil;
    project->Tstep = tStep;

    //... store current moisture levels in vector x
    x[SURF] = project->theLidUnit->surfaceDepth;
    x[SOIL] = project->theLidUnit->soilMoisture;
    x[STOR] = project->theLidUnit->storageDepth;
    x[PAVE] = project->theLidUnit->paveDepth;

    //... initialize layer flux rates and moisture limits
    project->SurfaceInflow  = inflow;
    project->SurfaceInfil   = 0.0;
    project->SurfaceEvap    = 0.0;
    project->SurfaceOutflow = 0.0;
    project->PaveEvap       = 0.0;
    project->PavePerc       = 0.0;
    project->SoilEvap       = 0.0;
    project->SoilPerc       = 0.0;
    project->StorageInflow  = 0.0;
    project->StorageExfil   = 0.0;                                                      //(5.1.011)
    project->StorageEvap    = 0.0;
    project->StorageDrain   = 0.0;
    for (i = 0; i < MAX_LAYERS; i++)
    {
        f[i] = 0.0;
        fOld[i] = project->theLidUnit->oldFluxRates[i];
        xMin[i] = 0.0;
        xMax[i] = BIG;
        project->Xold[i] = x[i];
    }

    //... find Green-Ampt infiltration from surface layer
    if ( project->theLidProc->lidType == POROUS_PAVEMENT ) project->SurfaceInfil = 0.0;
    else if ( project->theLidUnit->soilInfil.Ks > 0.0 )
    {
        project->SurfaceInfil =
            grnampt_getInfil(project, &project->theLidUnit->soilInfil, project->Tstep,
                             project->SurfaceInflow, project->theLidUnit->surfaceDepth,
                             MOD_GREEN_AMPT);                                  //(5.1.010)
    }
    else project->SurfaceInfil = infil;

    //... set moisture limits for soil & storage layers
    if ( project->theLidProc->soil.thickness > 0.0 )
    {
        xMin[SOIL] = project->theLidProc->soil.wiltPoint;
        xMax[SOIL] = project->theLidProc->soil.porosity;
    }
    if ( project->theLidProc->pavement.thickness > 0.0 )
    {
        xMax[PAVE] = project->theLidProc->pavement.thickness;                           //(5.1.011)
    }
    if ( project->theLidProc->storage.thickness > 0.0 )
    {
        xMax[STOR] = project->theLidProc->storage.thickness;
    }
    if ( project->theLidProc->lidType == GREEN_ROOF )
    {
        xMax[STOR] = project->theLidProc->drainMat.thickness;
    }

    //... determine which flux rate function to use
    switch (project->theLidProc->lidType)
    {
    case BIO_CELL:
    case RAIN_GARDEN:     fluxRates = &biocellFluxRates;   break;
    case GREEN_ROOF:      fluxRates = &greenRoofFluxRates; break;
    case INFIL_TRENCH:    fluxRates = &trenchFluxRates;    break;
    case POROUS_PAVEMENT: fluxRates = &pavementFluxRates;  break;
    case RAIN_BARREL:     fluxRates = &barrelFluxRates;    break;
    case ROOF_DISCON:     fluxRates = &roofFluxRates;      break;
    case VEG_SWALE:       fluxRates = &swaleFluxRates;
                          omega = 0.5;
                          break;
    default:              return 0.0;
    }

    //... update moisture levels and flux rates over the time step
    i = modpuls_solve(project, MAX_LAYERS, x, xOld, xPrev, xMin, xMax, xTol,
                     fOld, f, tStep, omega, fluxRates);

/** For debugging only ********************************************
    if  (i == 0)
    {
        fprintf(Frpt.file,
        "\n  WARNING 09: integration failed to converge at %s %s",
            theDate, theTime);
        fprintf(Frpt.file,
        "\n              for LID %s placed in subcatchment %s.",
            project->theLidProc->ID, theSubcatch->ID);
    }
*******************************************************************/

    //... add any surface overflow to surface outflow
    if ( project->theLidProc->surface.canOverflow || project->theLidUnit->fullWidth == 0.0 )
    {
        project->SurfaceOutflow += getSurfaceOverflowRate(project, &x[SURF]);
    }

    //... save updated results
    project->theLidUnit->surfaceDepth = x[SURF];
    project->theLidUnit->paveDepth    = x[PAVE];                                        //(5.1.011)
    project->theLidUnit->soilMoisture = x[SOIL];
    project->theLidUnit->storageDepth = x[STOR];
    for (i = 0; i < MAX_LAYERS; i++) project->theLidUnit->oldFluxRates[i] = f[i];

    //... assign values to LID unit evaporation, infiltration & drain flow
    *lidEvap = project->SurfaceEvap + project->PaveEvap + project->SoilEvap + project->StorageEvap;
    *lidInfil = project->StorageExfil;
    *lidDrain = project->StorageDrain;

    //... return surface outflow (per unit area) from unit
    return project->SurfaceOutflow;
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

void lidproc_saveResults(Project *project, TLidUnit* lidUnit, double ucfRainfall, double ucfRainDepth)
//
//  Purpose: updates the mass balance for an LID unit and saves
//           current flux rates to the LID report file.
//  Input:   lidUnit = ptr. to LID unit
//           ucfRainfall = units conversion factor for rainfall rate
//           ucfDepth = units conversion factor for rainfall depth
//  Output:  none
//
{
    double ucf;                        // units conversion factor
    double totalEvap;                  // total evaporation rate (ft/s)
    double totalVolume;                // total volume stored in LID (ft)
    double rptVars[MAX_RPT_VARS];      // array of reporting variables
    int    isDry = FALSE;              // true if current state of LID is dry
    char   timeStamp[24];              // date/time stamp
    double elapsedHrs;                 // elapsed hours

    //... find total evap. rate and stored volume
    totalEvap = project->SurfaceEvap + project->PaveEvap + project->SoilEvap + project->StorageEvap;
    totalVolume = project->SurfaceVolume + project->PaveVolume + project->SoilVolume + project->StorageVolume;

    //... update mass balance totals
    updateWaterBalance(project, project->theLidUnit, project->SurfaceInflow, totalEvap, project->StorageExfil,
                       project->SurfaceOutflow, project->StorageDrain, totalVolume);

    //... check if dry-weather conditions hold
    if ( project->SurfaceInflow  < MINFLOW &&
         project->SurfaceOutflow < MINFLOW &&
         project->StorageDrain   < MINFLOW &&
         project->StorageExfil   < MINFLOW &&
		 totalEvap      < MINFLOW
       ) isDry = TRUE;

    //... update status of project->HasWetLids
    if ( !isDry ) project->HasWetLids = TRUE;

    //... write results to LID report file                                     //(5.1.012)
    if ( lidUnit->rptFile )                                                    //(5.1.012)
    {
        //... convert rate results to original units (in/hr or mm/hr)
        ucf = ucfRainfall;
        rptVars[SURF_INFLOW]  = project->SurfaceInflow*ucf;
        rptVars[TOTAL_EVAP]   = totalEvap*ucf;
        rptVars[SURF_INFIL]   = project->SurfaceInfil*ucf;
        rptVars[PAVE_PERC]    = project->PavePerc*ucf;
        rptVars[SOIL_PERC]    = project->SoilPerc*ucf;
        rptVars[STOR_EXFIL]   = project->StorageExfil*ucf;
        rptVars[SURF_OUTFLOW] = project->SurfaceOutflow*ucf;
        rptVars[STOR_DRAIN]   = project->StorageDrain*ucf;

        //... convert storage results to original units (in or mm)
        ucf = ucfRainDepth;
        rptVars[SURF_DEPTH] = project->theLidUnit->surfaceDepth*ucf;
        rptVars[PAVE_DEPTH] = project->theLidUnit->paveDepth;                           //(5.1.011)
        rptVars[SOIL_MOIST] = project->theLidUnit->soilMoisture;
        rptVars[STOR_DEPTH] = project->theLidUnit->storageDepth*ucf;

        //... if the current LID state is wet but the previous state was dry
        //    for more than one period then write the saved previous results   //(5.1.012)
        //    to the report file thus marking the end of a dry period          //(5.10012)
        if ( !isDry && project->theLidUnit->rptFile->wasDry > 1)                        //(5.1.012)
        {
            fprintf(project->theLidUnit->rptFile->file, "%s",
                                  project->theLidUnit->rptFile->results);
        }

        //... write the current results to a string which is saved between
        //    reporting periods
        elapsedHrs = project->NewRunoffTime / 1000.0 / 3600.0;
        datetime_getTimeStamp(M_D_Y, getDateTime(project, project->NewRunoffTime), 24, timeStamp);
        sprintf(project->theLidUnit->rptFile->results,
             "\n%20s\t %8.3f\t %8.3f\t %8.4f\t %8.3f\t %8.3f\t %8.3f\t %8.3f\t"
             "%8.3f\t %8.3f\t %8.3f\t %8.3f\t %8.3f\t %8.3f",
             timeStamp, elapsedHrs, rptVars[0], rptVars[1], rptVars[2],
             rptVars[3], rptVars[4], rptVars[5], rptVars[6], rptVars[7],
             rptVars[8], rptVars[9], rptVars[10], rptVars[11]);

        //... if the current LID state is dry
        if ( isDry )
        {
            //... if the previous state was wet then write the current
            //    results to file marking the start of a dry period
            if ( project->theLidUnit->rptFile->wasDry == 0 )                            //(5.1.012)
            {
                fprintf(project->theLidUnit->rptFile->file, "%s",
                                        project->theLidUnit->rptFile->results);
            }

            //... increment the number of successive dry periods               //(5.1.012)
            project->theLidUnit->rptFile->wasDry++;                                     //(5.1.012)
        }

        //... if the current LID state is wet
        else
        {
            //... write the current results to the report file
                        fprintf(project->theLidUnit->rptFile->file, "%s",
                            project->theLidUnit->rptFile->results);

            //... re-set the number of successive dry periods to 0             //(5.1.012)
            project->theLidUnit->rptFile->wasDry = 0;                                   //(5.1.012)
        }
    }
}

//=============================================================================

////  New function for release 5.1.008.  ////                                  //(5.1.008)

void roofFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates for roof disconnection.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    double surfaceDepth = x[SURF];

    getEvapRates(project, surfaceDepth, 0.0, 0.0, 0.0, 1.0);                            //(5.1.011)
    project->SurfaceVolume = surfaceDepth;
    project->SurfaceInfil = 0.0;
    if ( project->theLidProc->surface.alpha > 0.0 )
      project->SurfaceOutflow = getSurfaceOutflowRate(project, surfaceDepth);
    else getSurfaceOverflowRate(project, &surfaceDepth);
    project->StorageDrain = MIN(project->theLidProc->drain.coeff/UCF(project, RAINFALL), project->SurfaceOutflow);
    project->SurfaceOutflow -= project->StorageDrain;
    f[SURF] = (project->SurfaceInflow - project->SurfaceEvap - project->StorageDrain - project->SurfaceOutflow);
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

void greenRoofFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates from the layers of a green roof.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    // Moisture level variables
    double surfaceDepth;
    double soilTheta;
    double storageDepth;

    // Intermediate variables
    double availVolume;
    double maxRate;

    // Green roof properties
    double soilThickness    = project->theLidProc->soil.thickness;
    double storageThickness = project->theLidProc->storage.thickness;
    double soilPorosity     = project->theLidProc->soil.porosity;
    double storageVoidFrac  = project->theLidProc->storage.voidFrac;
    double soilFieldCap     = project->theLidProc->soil.fieldCap;
    double soilWiltPoint    = project->theLidProc->soil.wiltPoint;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    soilTheta    = x[SOIL];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    project->SurfaceVolume = surfaceDepth * project->theLidProc->surface.voidFrac;
    project->SoilVolume = soilTheta * soilThickness;
    project->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = project->SoilVolume - soilWiltPoint * soilThickness;
    getEvapRates(project, project->SurfaceVolume, 0.0, availVolume, project->StorageVolume, 1.0);
    if ( soilTheta >= soilPorosity ) project->StorageEvap = 0.0;

    //... soil layer perc rate
    project->SoilPerc = getSoilPercRate(project, soilTheta);

    //... limit perc rate by available water
    availVolume = (soilTheta - soilFieldCap) * soilThickness;
    maxRate = MAX(availVolume, 0.0) / project->Tstep - project->SoilEvap;                        //(5.1.012)
    project->SoilPerc = MIN(project->SoilPerc, maxRate);
    project->SoilPerc = MAX(project->SoilPerc, 0.0);

    //... storage (drain mat) outflow rate
    project->StorageExfil = 0.0;
    project->StorageDrain = getDrainMatOutflow(project, storageDepth);

    //... unit is full
    if ( soilTheta >= soilPorosity && storageDepth >= storageThickness )
    {
        //... outflow from both layers equals limiting rate
        maxRate = MIN(project->SoilPerc, project->StorageDrain);
        project->SoilPerc = maxRate;
        project->StorageDrain = maxRate;

        //... adjust inflow rate to soil layer
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
    }

    //... unit not full
    else
    {
        //... limit drainmat outflow by available storage volume
        maxRate = storageDepth * storageVoidFrac / project->Tstep - project->StorageEvap;        //(5.1.012)
        if ( storageDepth >= storageThickness ) maxRate += project->SoilPerc;           //(5.1.012)
        maxRate = MAX(maxRate, 0.0);
        project->StorageDrain = MIN(project->StorageDrain, maxRate);

        //... limit soil perc inflow by unused storage volume
        maxRate = (storageThickness - storageDepth) * storageVoidFrac / project->Tstep +
                  project->StorageDrain + project->StorageEvap;
        project->SoilPerc = MIN(project->SoilPerc, maxRate);
                
        //... adjust surface infil. so soil porosity not exceeded
        maxRate = (soilPorosity - soilTheta) * soilThickness / project->Tstep +
                  project->SoilPerc + project->SoilEvap;
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
    }

    // ... find surface outflow rate
    project->SurfaceOutflow = getSurfaceOutflowRate(project, surfaceDepth);

    // ... compute overall layer flux rates
    f[SURF] = (project->SurfaceInflow - project->SurfaceEvap - project->SurfaceInfil - project->SurfaceOutflow) /
              project->theLidProc->surface.voidFrac;
    f[SOIL] = (project->SurfaceInfil - project->SoilEvap - project->SoilPerc) /
              project->theLidProc->soil.thickness;
    f[STOR] = (project->SoilPerc - project->StorageEvap - project->StorageDrain) /
              project->theLidProc->storage.voidFrac;
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

void biocellFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates from the layers of a bio-retention cell LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    // Moisture level variables
    double surfaceDepth;
    double soilTheta;
    double storageDepth;

    // Intermediate variables
    double availVolume;
    double maxRate;

    // LID layer properties
    double soilThickness    = project->theLidProc->soil.thickness;
    double soilPorosity     = project->theLidProc->soil.porosity;
    double soilFieldCap     = project->theLidProc->soil.fieldCap;
    double soilWiltPoint    = project->theLidProc->soil.wiltPoint;
    double storageThickness = project->theLidProc->storage.thickness;
    double storageVoidFrac  = project->theLidProc->storage.voidFrac;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    soilTheta    = x[SOIL];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    project->SurfaceVolume = surfaceDepth * project->theLidProc->surface.voidFrac;
    project->SoilVolume    = soilTheta * soilThickness;
    project->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = project->SoilVolume - soilWiltPoint * soilThickness;
    getEvapRates(project, project->SurfaceVolume, 0.0, availVolume, project->StorageVolume, 1.0);
    if ( soilTheta >= soilPorosity ) project->StorageEvap = 0.0;

    //... soil layer perc rate
    project->SoilPerc = getSoilPercRate(project, soilTheta);

    //... limit perc rate by available water
    availVolume =  (soilTheta - soilFieldCap) * soilThickness;
    maxRate = MAX(availVolume, 0.0) / project->Tstep - project->SoilEvap;                        //(5.1.012)
    project->SoilPerc = MIN(project->SoilPerc, maxRate);
    project->SoilPerc = MAX(project->SoilPerc, 0.0);

    //... exfiltration rate out of storage layer
    project->StorageExfil = getStorageExfilRate(project);

    //... underdrain flow rate
    project->StorageDrain = 0.0;
    if ( project->theLidProc->drain.coeff > 0.0 )
    {
        project->StorageDrain = getStorageDrainRate(project, storageDepth, soilTheta, 0.0,
                                           surfaceDepth);
    }

    //... special case of no storage layer present
    if ( storageThickness == 0.0 )
    {
        project->StorageEvap = 0.0;
        maxRate = MIN(project->SoilPerc, project->StorageExfil);
        project->SoilPerc = maxRate;
        project->StorageExfil = maxRate;

////  Following code segment added to release 5.1.012  ////                    //(5.1.012)
        //... limit surface infil. by unused soil volume
        maxRate = (soilPorosity - soilTheta) * soilThickness / project->Tstep +
                  project->SoilPerc + project->SoilEvap;
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
//////////////////////////////////////////////////////////

	}

    //... storage & soil layers are full
    else if ( soilTheta >= soilPorosity && storageDepth >= storageThickness )
    {
        //... limiting rate is smaller of soil perc and storage outflow
        maxRate = project->StorageExfil + project->StorageDrain;
        if ( project->SoilPerc < maxRate )
        {
            maxRate = project->SoilPerc;
            if ( maxRate > project->StorageExfil ) project->StorageDrain = maxRate - project->StorageExfil;
            else
            {
                project->StorageExfil = maxRate;
                project->StorageDrain = 0.0;
            }
        }
        else project->SoilPerc = maxRate;

        //... apply limiting rate to surface infil.
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
    }

    //... either layer not full
    else if ( storageThickness > 0.0 )
    {
        //... limit storage exfiltration by available storage volume
        maxRate = project->SoilPerc - project->StorageEvap + storageDepth*storageVoidFrac/project->Tstep;
        project->StorageExfil = MIN(project->StorageExfil, maxRate);
        project->StorageExfil = MAX(project->StorageExfil, 0.0);

        //... limit underdrain flow by volume above drain offset
        if ( project->StorageDrain > 0.0 )
        {
            maxRate = -project->StorageExfil - project->StorageEvap;                              //(5.1.012)
            if ( storageDepth >= storageThickness) maxRate += project->SoilPerc;         //(5.1.012)
            if ( project->theLidProc->drain.offset <= storageDepth )
            {
                maxRate += (storageDepth - project->theLidProc->drain.offset) *
                           storageVoidFrac/project->Tstep;
            }
            maxRate = MAX(maxRate, 0.0);
            project->StorageDrain = MIN(project->StorageDrain, maxRate);
        }

        //... limit soil perc by unused storage volume
        maxRate = project->StorageExfil + project->StorageDrain + project->StorageEvap +
                  (storageThickness - storageDepth) *
                  storageVoidFrac/project->Tstep;
        project->SoilPerc = MIN(project->SoilPerc, maxRate);

        //... limit surface infil. by unused soil volume
        maxRate = (soilPorosity - soilTheta) * soilThickness / project->Tstep +
                  project->SoilPerc + project->SoilEvap;
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
    }

    //... find surface layer outflow rate
    project->SurfaceOutflow = getSurfaceOutflowRate(project, surfaceDepth);

    //... compute overall layer flux rates
    f[SURF] = (project->SurfaceInflow - project->SurfaceEvap - project->SurfaceInfil - project->SurfaceOutflow) /
              project->theLidProc->surface.voidFrac;
    f[SOIL] = (project->SurfaceInfil - project->SoilEvap - project->SoilPerc) /
              project->theLidProc->soil.thickness;
    if ( storageThickness == 0.0 ) f[STOR] = 0.0;
    else f[STOR] = (project->SoilPerc - project->StorageEvap - project->StorageExfil - project->StorageDrain) /
                   project->theLidProc->storage.voidFrac;
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

void trenchFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates from the layers of an infiltration trench LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    // Moisture level variables
    double surfaceDepth;
    double storageDepth;

    // Intermediate variables
    double availVolume;
    double maxRate;

    // Storage layer properties
    double storageThickness = project->theLidProc->storage.thickness;
    double storageVoidFrac = project->theLidProc->storage.voidFrac;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    project->SurfaceVolume = surfaceDepth * project->theLidProc->surface.voidFrac;
    project->SoilVolume = 0.0;
    project->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = (storageThickness - storageDepth) * storageVoidFrac;
    getEvapRates(project, project->SurfaceVolume, 0.0, 0.0, project->StorageVolume, 1.0);

    //... no storage evap if surface ponded
    if ( surfaceDepth > 0.0 ) project->StorageEvap = 0.0;

    //... nominal storage inflow
    project->StorageInflow = project->SurfaceInflow + project->SurfaceVolume / project->Tstep;

    //... exfiltration rate out of storage layer
   project->StorageExfil = getStorageExfilRate(project);

    //... underdrain flow rate
    project->StorageDrain = 0.0;
    if ( project->theLidProc->drain.coeff > 0.0 )
    {
        project->StorageDrain = getStorageDrainRate(project, storageDepth, 0.0, 0.0, surfaceDepth);
    }

    //... limit storage exfiltration by available storage volume
    maxRate = project->StorageInflow - project->StorageEvap + storageDepth*storageVoidFrac/project->Tstep;
    project->StorageExfil = MIN(project->StorageExfil, maxRate);
    project->StorageExfil = MAX(project->StorageExfil, 0.0);

    //... limit underdrain flow by volume above drain offset
    if ( project->StorageDrain > 0.0 )
    {
        maxRate = -project->StorageExfil - project->StorageEvap;                                 //(5.1.012)
        if (storageDepth >= storageThickness ) maxRate += project->StorageInflow;       //(5.1.012)
        if ( project->theLidProc->drain.offset <= storageDepth )
        {
            maxRate += (storageDepth - project->theLidProc->drain.offset) *
                       storageVoidFrac/project->Tstep;
        }
        maxRate = MAX(maxRate, 0.0);
        project->StorageDrain = MIN(project->StorageDrain, maxRate);
    }

    //... limit storage inflow to not exceed storage layer capacity
    maxRate = (storageThickness - storageDepth)*storageVoidFrac/project->Tstep +
              project->StorageExfil + project->StorageEvap + project->StorageDrain;
    project->StorageInflow = MIN(project->StorageInflow, maxRate);

    //... equate surface infil to storage inflow
    project->SurfaceInfil = project->StorageInflow;

    //... find surface outflow rate
    project->SurfaceOutflow = getSurfaceOutflowRate(project, surfaceDepth);

    // ... find net fluxes for each layer
    f[SURF] = project->SurfaceInflow - project->SurfaceEvap - project->StorageInflow - project->SurfaceOutflow /
              project->theLidProc->surface.voidFrac;;
    f[STOR] = (project->StorageInflow - project->StorageEvap - project->StorageExfil - project->StorageDrain) /
              project->theLidProc->storage.voidFrac;
    f[SOIL] = 0.0;
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

void pavementFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates for the layers of a porous pavement LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    //... Moisture level variables
    double surfaceDepth;
    double paveDepth;
    double soilTheta;
    double storageDepth;

    //... Intermediate variables
    double pervFrac = (1.0 - project->theLidProc->pavement.impervFrac);
    double storageInflow;    // inflow rate to storage layer (ft/s)
    double availVolume;
    double maxRate;

    //... LID layer properties
    double paveVoidFrac     = project->theLidProc->pavement.voidFrac * pervFrac;
    double paveThickness    = project->theLidProc->pavement.thickness;
    double soilThickness    = project->theLidProc->soil.thickness;
    double soilPorosity     = project->theLidProc->soil.porosity;
    double soilFieldCap     = project->theLidProc->soil.fieldCap;
    double soilWiltPoint    = project->theLidProc->soil.wiltPoint;
    double storageThickness = project->theLidProc->storage.thickness;
    double storageVoidFrac  = project->theLidProc->storage.voidFrac;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    paveDepth    = x[PAVE];
    soilTheta    = x[SOIL];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    project->SurfaceVolume = surfaceDepth * project->theLidProc->surface.voidFrac;
    project->PaveVolume = paveDepth * paveVoidFrac;
    project->SoilVolume = soilTheta * soilThickness;
    project->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = project->SoilVolume - soilWiltPoint * soilThickness;
    getEvapRates(project, project->SurfaceVolume, project->PaveVolume, availVolume, project->StorageVolume,
                 pervFrac);

    //... no storage evap if soil or pavement layer saturated
    if ( paveDepth >= paveThickness ||
       ( soilThickness > 0.0 && soilTheta >= soilPorosity )
       ) project->StorageEvap = 0.0;

    //... find nominal rate of surface infiltration into pavement layer
    project->SurfaceInfil = project->SurfaceInflow + (project->SurfaceVolume / project->Tstep);

    //... find perc rate out of pavement layer
    project->PavePerc = getPavementPermRate(project);

    //... limit pavement perc by available water
    maxRate = project->PaveVolume/project->Tstep + project->SurfaceInfil - project->PaveEvap;
    maxRate = MAX(maxRate, 0.0);
    project->PavePerc = MIN(project->PavePerc, maxRate);

    //... find soil layer perc rate
    if ( soilThickness > 0.0 )
    {
        project->SoilPerc = getSoilPercRate(project, soilTheta);
        availVolume = (soilTheta - soilFieldCap) * soilThickness;
        maxRate = MAX(availVolume, 0.0) / project->Tstep - project->SoilEvap;                    //(5.1.012)
        project->SoilPerc = MIN(project->SoilPerc, maxRate);
        project->SoilPerc = MAX(project->SoilPerc, 0.0);
    }
    else project->SoilPerc = project->PavePerc;

    //... exfiltration rate out of storage layer
    project->StorageExfil = getStorageExfilRate(project);

    //... underdrain flow rate
    project->StorageDrain = 0.0;
    if ( project->theLidProc->drain.coeff > 0.0 )
    {
        project->StorageDrain = getStorageDrainRate(project, storageDepth, soilTheta, paveDepth,
                                           surfaceDepth);
    }

    //... check for adjacent saturated layers

    //... no soil layer, pavement & storage layers are full
    if ( soilThickness == 0.0 &&
         storageDepth >= storageThickness &&
         paveDepth >= paveThickness )
    {
        //... pavement outflow can't exceed storage outflow
        maxRate = project->StorageEvap + project->StorageDrain + project->StorageExfil;
        if ( project->PavePerc > maxRate ) project->PavePerc = maxRate;

        //... storage outflow can't exceed pavement outflow
        else
        {
            //... use up available exfiltration capacity first
            project->StorageExfil = MIN(project->StorageExfil, project->PavePerc);
            project->StorageDrain = project->PavePerc - project->StorageExfil;
        }

        //... set soil perc to pavement perc
        project->SoilPerc = project->PavePerc;

        //... limit surface infil. by pavement perc
        project->SurfaceInfil = MIN(project->SurfaceInfil, project->PavePerc);
    }

    //... pavement, soil & storage layers are full
    else if ( soilThickness > 0 &&
              storageDepth >= storageThickness &&
              soilTheta >= soilPorosity &&
              paveDepth >= paveThickness )
    {
        //... find which layer has limiting flux rate
        maxRate = project->StorageExfil + project->StorageDrain;
        if ( project->SoilPerc < maxRate) maxRate = project->SoilPerc;
        else maxRate = MIN(maxRate, project->PavePerc);

        //... use up available storage exfiltration capacity first
        if ( maxRate > project->StorageExfil ) project->StorageDrain = maxRate - project->StorageExfil;
        else
        {
            project->StorageExfil = maxRate;
            project->StorageDrain = 0.0;
        }
        project->SoilPerc = maxRate;
        project->PavePerc = maxRate;

        //... limit surface infil. by pavement perc
        project->SurfaceInfil = MIN(project->SurfaceInfil, project->PavePerc);
    }

    //... storage & soil layers are full
    else if ( soilThickness > 0.0 &&
              storageDepth >= storageThickness &&
              soilTheta >= soilPorosity )
    {
        //... soil perc can't exceed storage outflow
        maxRate = project->StorageDrain + project->StorageExfil;
        if ( project->SoilPerc > maxRate ) project->SoilPerc = maxRate;

        //... storage outflow can't exceed soil perc
        else
        {
            //... use up available exfiltration capacity first
            project->StorageExfil = MIN(project->StorageExfil, project->SoilPerc);
            project->StorageDrain = project->SoilPerc - project->StorageExfil;
        }

        //... limit surface infil. by available pavement volume
        availVolume = (paveThickness - paveDepth) * paveVoidFrac;
        maxRate = availVolume / project->Tstep + project->PavePerc + project->PaveEvap;
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
    }

    //... soil and pavement layers are full
    else if ( soilThickness > 0.0 &&
              paveDepth >= paveThickness &&
              soilTheta >= soilPorosity )
    {
        project->PavePerc = MIN(project->PavePerc, project->SoilPerc);
        project->SoilPerc = project->PavePerc;
        project->SurfaceInfil = MIN(project->SurfaceInfil,project->PavePerc);
    }

    //... no adjoining layers are full
    else
    {
        //... limit storage exfiltration by available storage volume
        //    (if no soil layer, project->SoilPerc is same as project->PavePerc)
        maxRate = project->SoilPerc - project->StorageEvap + project->StorageVolume / project->Tstep;
        maxRate = MAX(0.0, maxRate);
        project->StorageExfil = MIN(project->StorageExfil, maxRate);

        //... limit underdrain flow by volume above drain offset
        if ( project->StorageDrain > 0.0 )
        {
            maxRate = -project->StorageExfil - project->StorageEvap;                             //(5.1.012)
            if (storageDepth >= storageThickness ) maxRate += project->SoilPerc;        //(5.1.012)
            if ( project->theLidProc->drain.offset <= storageDepth )
            {
                maxRate += (storageDepth - project->theLidProc->drain.offset) *
                           storageVoidFrac/project->Tstep;
            }
            maxRate = MAX(maxRate, 0.0);
            project->StorageDrain = MIN(project->StorageDrain, maxRate);
        }

        //... limit soil & pavement outflow by unused storage volume
        availVolume = (storageThickness - storageDepth) * storageVoidFrac;
        maxRate = availVolume/project->Tstep + project->StorageEvap + project->StorageDrain + project->StorageExfil;
        maxRate = MAX(maxRate, 0.0);
        if ( soilThickness > 0.0 )
        {
            project->SoilPerc = MIN(project->SoilPerc, maxRate);
            maxRate = (soilPorosity - soilTheta) * soilThickness / project->Tstep +
                      project->SoilPerc;
        }
        project->PavePerc = MIN(project->PavePerc, maxRate);

        //... limit surface infil. by available pavement volume
        availVolume = (paveThickness - paveDepth) * paveVoidFrac;
        maxRate = availVolume / project->Tstep + project->PavePerc + project->PaveEvap;
        project->SurfaceInfil = MIN(project->SurfaceInfil, maxRate);
    }

    //... surface outflow
    project->SurfaceOutflow = getSurfaceOutflowRate(project, surfaceDepth);

    //... compute overall layer flux rates
    f[SURF] = project->SurfaceInflow - project->SurfaceEvap - project->SurfaceInfil - project->SurfaceOutflow;
    f[PAVE] = (project->SurfaceInfil - project->PaveEvap - project->PavePerc) / paveVoidFrac;
    if ( project->theLidProc->soil.thickness > 0.0)
    {
        f[SOIL] = (project->PavePerc - project->SoilEvap - project->SoilPerc) / soilThickness;
        storageInflow = project->SoilPerc;
    }
    else
    {
        f[SOIL] = 0.0;
        storageInflow = project->PavePerc;
        project->SoilPerc = 0.0;
    }
    f[STOR] = (storageInflow - project->StorageEvap - project->StorageExfil - project->StorageDrain) /
              storageVoidFrac;
}

//=============================================================================

void swaleFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates from a vegetative swale LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    double depth;            // depth of surface water in swale (ft)
    double topWidth;         // top width of full swale (ft)
    double botWidth;         // bottom width of swale (ft)
    double length;           // length of swale (ft)
    double surfInflow;       // inflow rate to swale (cfs)
    double surfWidth;        // top width at current water depth (ft)
    double surfArea;         // surface area of current water depth (ft2)
    double flowArea;         // x-section flow area (ft2)
    double lidArea;          // surface area of full swale (ft2)
    double hydRadius;        // hydraulic radius for current depth (ft)
    double slope;            // slope of swale side wall (run/rise)
    double volume;           // swale volume at current water depth (ft3)
    double dVdT;             // change in volume w.r.t. time (cfs)
    double dStore;           // depression storage depth (ft)
    double xDepth;           // depth above depression storage (ft)

    //... retrieve state variable from work vector
    depth = x[SURF];
    depth = MIN(depth, project->theLidProc->surface.thickness);

    //... depression storage depth
    dStore = 0.0;

    //... get swale's bottom width
    //    (0.5 ft minimum to avoid numerical problems)
    slope = project->theLidProc->surface.sideSlope;
    topWidth = project->theLidUnit->fullWidth;
    topWidth = MAX(topWidth, 0.5);
    botWidth = topWidth - 2.0 * slope * project->theLidProc->surface.thickness;
    if ( botWidth < 0.5 )
    {
        botWidth = 0.5;
        slope = 0.5 * (topWidth - 0.5) / project->theLidProc->surface.thickness;
    }

    //... swale's length
    lidArea = project->theLidUnit->area;
    length = lidArea / topWidth;

    //... top width, surface area and flow area of current ponded depth
    surfWidth = botWidth + 2.0 * slope * depth;
    surfArea = length * surfWidth;
    flowArea = (depth * (botWidth + slope * depth)) *
               project->theLidProc->surface.voidFrac;

    //... wet volume and effective depth
    volume = length * flowArea;

    //... surface inflow into swale (cfs)
    surfInflow = project->SurfaceInflow * lidArea;

    //... ET rate in cfs
    project->SurfaceEvap = project->lidProcEvapRate * surfArea;
    project->SurfaceEvap = MIN(project->SurfaceEvap, volume/project->Tstep);

    //... infiltration rate to native soil in cfs
    project->StorageExfil = project->SurfaceInfil * surfArea;                                    //(5.1.011)

    //... no surface outflow if depth below depression storage
    xDepth = depth - dStore;
    if ( xDepth <= ZERO ) project->SurfaceOutflow = 0.0;

    //... otherwise compute a surface outflow
    else
    {
        //... modify flow area to remove depression storage,
        flowArea -= (dStore * (botWidth + slope * dStore)) *
                     project->theLidProc->surface.voidFrac;
        if ( flowArea < ZERO ) project->SurfaceOutflow = 0.0;
        else
        {
            //... compute hydraulic radius
            botWidth = botWidth + 2.0 * dStore * slope;
            hydRadius = botWidth + 2.0 * xDepth * sqrt(1.0 + slope*slope);
            hydRadius = flowArea / hydRadius;

            //... use Manning Eqn. to find outflow rate in cfs
            project->SurfaceOutflow = project->theLidProc->surface.alpha * flowArea *
                             pow(hydRadius, 2./3.);
        }
    }

    //... net flux rate (dV/dt) in cfs
    dVdT = surfInflow - project->SurfaceEvap - project->StorageExfil - project->SurfaceOutflow;           //(5.1.011)

    //... when full, any net positive inflow becomes spillage
    if ( depth == project->theLidProc->surface.thickness && dVdT > 0.0 )
    {
        project->SurfaceOutflow += dVdT;
        dVdT = 0.0;
    }

    //... convert flux rates to ft/s
    project->SurfaceEvap /= lidArea;
    project->StorageExfil /= lidArea;                                                   //(5.1.011)
    project->SurfaceOutflow /= lidArea;
    f[SURF] = dVdT / surfArea;
    f[SOIL] = 0.0;
    f[STOR] = 0.0;

    //... assign values to layer volumes
    project->SurfaceVolume = volume / lidArea;
    project->SoilVolume = 0.0;
    project->StorageVolume = 0.0;
}

//=============================================================================

////  This function was re-written for release 5.1.007.  ////                  //(5.1.007)

void barrelFluxRates(Project *project, double x[], double f[])
//
//  Purpose: computes flux rates for a rain barrel LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    double storageDepth = x[STOR];
	double head;
    double maxValue;

    //... assign values to layer volumes
    project->SurfaceVolume = 0.0;
    project->SoilVolume = 0.0;
    project->StorageVolume = storageDepth;

    //... initialize flows
    project->SurfaceInfil = 0.0;
    project->SurfaceOutflow = 0.0;
    project->StorageDrain = 0.0;

    //... compute outflow if time since last rain exceeds drain delay
    //    (dryTime is updated in lid.evalLidUnit at each time step)
    if ( project->theLidProc->drain.delay == 0.0 ||
             project->theLidUnit->dryTime >= project->theLidProc->drain.delay )
	{
	    head = storageDepth - project->theLidProc->drain.offset;
		if ( head > 0.0 )
	    {
		project->StorageDrain = getStorageDrainRate(project, storageDepth, 0.0, 0.0, 0.0);
		    maxValue = (head/project->Tstep);
			project->StorageDrain = MIN(project->StorageDrain, maxValue);
		}
	}

    //... limit inflow to available storage
    project->StorageInflow = project->SurfaceInflow;
    maxValue = (project->theLidProc->storage.thickness - storageDepth) / project->Tstep +
        project->StorageDrain;
    project->StorageInflow = MIN(project->StorageInflow, maxValue);
    project->SurfaceInfil = project->StorageInflow;

    //... assign values to layer flux rates
    f[SURF] = project->SurfaceInflow - project->StorageInflow;
    f[STOR] = project->StorageInflow - project->StorageDrain;
    f[SOIL] = 0.0;
}

//=============================================================================

double getSurfaceOutflowRate(Project *project, double depth)
//
//  Purpose: computes outflow rate from a LID's surface layer.
//  Input:   depth = depth of ponded water on surface layer (ft)
//  Output:  returns outflow from surface layer (ft/s)
//
//  Note: this function should not be applied to swales or rain barrels.
//
{
    double delta;
    double outflow;

    //... no outflow if ponded depth below storage depth
    delta = depth - project->theLidProc->surface.thickness;
    if ( delta < 0.0 ) return 0.0;

    //... compute outflow from overland flow Manning equation
    outflow = project->theLidProc->surface.alpha * pow(delta, 5.0/3.0) *
              project->theLidUnit->fullWidth / project->theLidUnit->area;
    outflow = MIN(outflow, delta / project->Tstep);
    return outflow;
}

//=============================================================================

double getPavementPermRate(Project *project)
//
//  Purpose: computes reduced permeability of a pavement layer due to
//           clogging.
//  Input:   none
//  Output:  returns the reduced permeability of the pavement layer (ft/s).
//
{
    double permRate;
    double permReduction;

    permReduction = project->theLidProc->pavement.clogFactor;
    if ( permReduction > 0.0 )
    {
        permReduction = project->theLidUnit->waterBalance.inflow / permReduction;
        permReduction = MIN(permReduction, 1.0);
    }
    permRate = project->theLidProc->pavement.kSat * (1.0 - permReduction);
    return permRate;
}

//=============================================================================

////  This function was modified for release 5.1.011.  ////                    //(5.1.011)

double getSoilPercRate(Project *project, double theta)
//
//  Purpose: computes percolation rate of water through a LID's soil layer.
//  Input:   theta = moisture content (fraction)
//  Output:  returns percolation rate within soil layer (ft/s)
//
{
    double delta;            // moisture deficit

    // ... no percolation if soil moisture <= field capacity
    if ( theta <= project->theLidProc->soil.fieldCap ) return 0.0;

    // ... perc rate = unsaturated hydraulic conductivity
    delta = project->theLidProc->soil.porosity - theta;
    return project->theLidProc->soil.kSat * exp(-delta * project->theLidProc->soil.kSlope);

}

//=============================================================================

double getStorageExfilRate(Project *project)                                                   //(5.1.011)
//
//  Purpose: computes exfiltration rate from storage zone into                 //(5.1.011)
//           native soil beneath a LID.
//  Input:   depth = depth of water storage zone (ft)
//  Output:  returns infiltration rate (ft/s)
//
{
    double infil = 0.0;
    double clogFactor = 0.0;

    if ( project->theLidProc->storage.kSat == 0.0 ) return 0.0;
    if ( project->lidProcMaxNativeInfil == 0.0 ) return 0.0;

    //... reduction due to clogging
    clogFactor = project->theLidProc->storage.clogFactor;
    if ( clogFactor > 0.0 )
    {
        clogFactor = project->theLidUnit->waterBalance.inflow / clogFactor;
        clogFactor = MIN(clogFactor, 1.0);
    }

    //... infiltration rate = storage Ksat reduced by any clogging
    infil = project->theLidProc->storage.kSat * (1.0 - clogFactor);

    //... limit infiltration rate by any groundwater-imposed limit
    return MIN(infil, project->lidProcMaxNativeInfil);
}

//=============================================================================

////  This function was modified for release 5.1.011.  ////                    //(5.1.011)

double  getStorageDrainRate(Project *project, double storageDepth, double soilTheta,
                            double paveDepth, double surfaceDepth)
//
//  Purpose: computes underdrain flow rate in a LID's storage layer.
//  Input:   storageDepth = depth of water in storage layer (ft)
//           soilTheta    = moisture content of soil layer
//           paveDepth    = effective depth of water in pavement layer (ft)
//           surfaceDepth = depth of ponded water on surface layer (ft)
//  Output:  returns flow in underdrain (ft/s)
//
//  Note:    drain eqn. is evaluated in user's units.
//  Note:    head on drain is water depth in storage layer plus the
//           layers above it (soil, pavement, and surface in that order)
//           minus the drain outlet offset.
{
    double head = storageDepth;
    double outflow = 0.0;
    double paveThickness    = project->theLidProc->pavement.thickness;
    double soilThickness    = project->theLidProc->soil.thickness;
    double soilPorosity     = project->theLidProc->soil.porosity;
    double soilFieldCap     = project->theLidProc->soil.fieldCap;
    double storageThickness = project->theLidProc->storage.thickness;

    // --- storage layer is full
    if ( storageDepth >= storageThickness )
    {
        // --- a soil layer exists
        if ( soilThickness > 0.0 )
        {
            // --- increase head by fraction of soil layer saturated
            if ( soilTheta > soilFieldCap )
            {
                head += (soilTheta - soilFieldCap) /
                        (soilPorosity - soilFieldCap) * soilThickness;

                // --- soil layer is saturated, increase head by water
                //     depth in layer above it
                if ( soilTheta >= soilPorosity )
                {
                    if ( paveThickness > 0.0 ) head += paveDepth;
                    else head += surfaceDepth;
                }
            }
        }

        // --- no soil layer so increase head by water level in pavement
        //     layer and possibly surface layer
        if ( paveThickness > 0.0 )
        {
            head += paveDepth;
            if ( paveDepth >= paveThickness ) head += surfaceDepth;
        }
    }

    // --- make head relative to drain offset
    head -= project->theLidProc->drain.offset;

    // ... compute drain outflow from underdrain flow equation in user units
    //     (head in inches or mm, flow rate in in/hr or mm/hr)
    if ( head > ZERO )
    {
        head *= UCF(project, RAINDEPTH);
        outflow = project->theLidProc->drain.coeff *
                  pow(head, project->theLidProc->drain.expon);
        outflow /= UCF(project, RAINFALL);
    }
    return outflow;
}

//=============================================================================

////  This function was modified for release 5.1.007.  ////                    //(5.1.007)

double getDrainMatOutflow(Project *project, double depth)
//
//  Purpose: computes flow rate through a green roof's drainage mat.
//  Input:   depth = depth of water in drainage mat (ft)
//  Output:  returns flow in drainage mat (ft/s)
//
{
    //... default is to pass all inflow
    double result = project->SoilPerc;

    //... otherwise use Manning eqn. if its parameters were supplied
    if ( project->theLidProc->drainMat.alpha > 0.0 )
    {
        result = project->theLidProc->drainMat.alpha * pow(depth, 5.0/3.0) *
                 project->theLidUnit->fullWidth / project->theLidUnit->area *
                 project->theLidProc->drainMat.voidFrac;                                //(5.1.008)
    }
    return result;
}

//=============================================================================

////  This function was re-written for release 5.1.008.  ////                  //(5.1.008)

void getEvapRates(Project *project, double surfaceVol, double paveVol, double soilVol,
    double storageVol, double pervFrac)                                        //(5.1.011)
//
//  Purpose: computes surface, pavement, soil, and storage evaporation rates.
//  Input:   surfaceVol = volume/area of ponded water on surface layer (ft)
//           paveVol    = volume/area of water in pavement pores (ft)
//           soilVol    = volume/area of water in soil (or pavement) pores (ft)
//           storageVol = volume/area of water in storage layer (ft)
//           pervFrac   = fraction of surface layer that is pervious           //(5.1.011)
//  Output:  none
//
{
    double availEvap;

    //... surface evaporation flux
    availEvap = project->lidProcEvapRate;
    project->SurfaceEvap = MIN(availEvap, surfaceVol/project->Tstep);
    project->SurfaceEvap = MAX(0.0, project->SurfaceEvap);
    availEvap = MAX(0.0, (availEvap - project->SurfaceEvap));
    availEvap *= pervFrac;                                                     //(5.1.011)

    //... no subsurface evap if water is infiltrating
    if ( project->SurfaceInfil > 0.0 )
    {
        project->PaveEvap = 0.0;
        project->SoilEvap = 0.0;
        project->StorageEvap = 0.0;
    }
    else
    {
        //... pavement evaporation flux
        project->PaveEvap = MIN(availEvap, paveVol / project->Tstep);
        availEvap = MAX(0.0, (availEvap - project->PaveEvap));

        //... soil evaporation flux
        project->SoilEvap = MIN(availEvap, soilVol / project->Tstep);
        availEvap = MAX(0.0, (availEvap - project->SoilEvap));

        //... storage evaporation flux
        project->StorageEvap = MIN(availEvap, storageVol / project->Tstep);
    }
}

//=============================================================================

double getSurfaceOverflowRate(Project *project, double* surfaceDepth)
//
//  Purpose: finds surface overflow rate from a LID unit.
//  Input:   surfaceDepth = depth of water stored in surface layer (ft)
//  Output:  returns the overflow rate (ft/s)
//
{
    double delta = *surfaceDepth - project->theLidProc->surface.thickness;
    if (  delta <= 0.0 ) return 0.0;
    *surfaceDepth = project->theLidProc->surface.thickness;
    return delta * project->theLidProc->surface.voidFrac / project->Tstep;
}

//=============================================================================

void updateWaterBalance(Project *project, TLidUnit *lidUnit, double inflow, double evap,
    double infil, double surfFlow, double drainFlow, double storage)
//
//  Purpose: updates components of the water mass balance for a LID unit
//           over the current time step.
//  Input:   lidUnit   = a particular LID unit
//           inflow    = runon + rainfall to the LID unit (ft/s)
//           evap      = evaporation rate from the unit (ft/s)
//           infil     = infiltration out the bottom of the unit (ft/s)
//           surfFlow  = surface runoff from the unit (ft/s)
//           drainFlow = underdrain flow from the unit
//           storage   = volume of water stored in the unit (ft)
//  Output:  none
//
{
    lidUnit->waterBalance.inflow += inflow * project->Tstep;
    lidUnit->waterBalance.evap += evap * project->Tstep;
    lidUnit->waterBalance.infil += infil * project->Tstep;
    lidUnit->waterBalance.surfFlow += surfFlow * project->Tstep;
    lidUnit->waterBalance.drainFlow += drainFlow * project->Tstep;
    lidUnit->waterBalance.finalVol = storage;
}

//=============================================================================

int modpuls_solve(Project *project, int n, double* x, double* xOld, double* xPrev,
                  double* xMin, double* xMax, double* xTol,
                  double* qOld, double* q, double dt, double omega,            //(5.1.007)
                  void (*derivs)(Project*, double*, double*))
//
//  Purpose: solves system of equations dx/dt = q(x) for x at end of time step
//           dt using a modified Puls method.
//  Input:   n = number of state variables
//           x = vector of state variables
//           xOld = state variable values at start of time step
//           xPrev = state variable values from previous iteration
//           xMin = lower limits on state variables
//           xMax = upper limits on state variables
//           xTol = convergence tolerances on state variables
//           qOld = flux rates at start of time step
//           q = flux rates at end of time step
//           dt = time step (sec)
//           omega = time weighting parameter (use 0 for Euler method          //(5.1.007)
//                   or 0.5 for modified Puls method)                          //(5.1.007)
//           derivs = pointer to function that computes flux rates q as a
//                    function of state variables x
//  Output:  returns number of steps required for convergence (or 0 if
//           process doesn't converge)
//
{
    int i;
    int canStop;
    int steps = 1;
    int maxSteps = 20;

    //... initialize state variable values
    for (i=0; i<n; i++)
    {
        xOld[i] = x[i];
        xPrev[i] = x[i];
    }

    //... repeat until convergence achieved
    while (steps < maxSteps)
    {
        //... compute flux rates for current state levels
        canStop = 1;
        derivs(project, x, q);

        //... update state levels based on current flux rates
        for (i=0; i<n; i++)
        {
            x[i] = xOld[i] + (omega*qOld[i] + (1.0 - omega)*q[i]) * dt;
            x[i] = MIN(x[i], xMax[i]);
            x[i] = MAX(x[i], xMin[i]);

            if ( omega > 0.0 &&                                                //(5.1.007)
                 fabs(x[i] - xPrev[i]) > xTol[i] ) canStop = 0;
            xPrev[i] = x[i];
        }

        //... return if process converges
        if (canStop) return steps;
        steps++;
    }

    //... no convergence so return 0
    return 0;
}
