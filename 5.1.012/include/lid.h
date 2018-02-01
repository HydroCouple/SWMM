/*!
 * \file lid.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version SWMM 5.1.012
 * \description
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU General Public License as published by the Free Software Foundation;
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
//   lid.h
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    03/20/14   (Build 5.1.001)
//            03/19/15   (Build 5.1.008)
//            08/01/16   (Build 5.1.011)
//            03/14/17   (Build 5.1.012)
//   Author:  L. Rossman (US EPA)
//
//   Public interface for LID functions.
//
//   Build 5.1.008:
//   - Support added for Roof Disconnection LID.
//   - Support added for separate routing of LID drain flows.
//   - Detailed LID reporting modified.
//
//   Build 5.1.011:
//   - Water depth replaces moisture content for LID's pavement layer. 
//   - Arguments for lidproc_saveResults() modified.
//
//   Build 5.1.012:
//   - Redefined meaning of wasDry in TLidRptFile structure.
//
//-----------------------------------------------------------------------------

#ifndef LID_H
#define LID_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "infil.h"

//-----------------------------------------------------------------------------
//  Enumerations
//-----------------------------------------------------------------------------
enum LidTypes {
    BIO_CELL,                // bio-retention cell
    RAIN_GARDEN,             // rain garden 
    GREEN_ROOF,              // green roof 
    INFIL_TRENCH,            // infiltration trench
    POROUS_PAVEMENT,         // porous pavement
    RAIN_BARREL,             // rain barrel
    VEG_SWALE,               // vegetative swale
    ROOF_DISCON};            // roof disconnection                             //(5.1.008)

enum TimePeriod {
    PREVIOUS,                // previous time period                           //(5.1.008)
    CURRENT};                // current time period                            //(5.1.008)

//-----------------------------------------------------------------------------
//  Data Structures
//-----------------------------------------------------------------------------
#define MAX_LAYERS 4

// LID Surface Layer
typedef struct
{
    double    thickness;          // depression storage or berm ht. (ft)
    double    voidFrac;           // available fraction of storage volume
    double    roughness;          // surface Mannings n 
    double    surfSlope;          // land surface slope (fraction)
    double    sideSlope;          // swale side slope (run/rise)
    double    alpha;              // slope/roughness term in Manning eqn.
    char      canOverflow;        // 1 if immediate outflow of excess water
}  TSurfaceLayer;

// LID Pavement Layer
typedef struct
{
    double   thickness;           // layer thickness (ft)
    double   voidFrac;            // void volume / total volume
    double   impervFrac;          // impervious area fraction
    double   kSat;                // permeability (ft/sec)
    double   clogFactor;          // clogging factor
}  TPavementLayer;

// LID Soil Layer
typedef struct
{
    double    thickness;          // layer thickness (ft)
    double    porosity;           // void volume / total volume
    double    fieldCap;           // field capacity
    double    wiltPoint;          // wilting point
    double    suction;            // suction head at wetting front (ft)
    double    kSat;               // saturated hydraulic conductivity (ft/sec)
    double    kSlope;             // slope of log(K) v. moisture content curve
}  TSoilLayer;

// LID Storage Layer
typedef struct
{
    double    thickness;          // layer thickness (ft)
    double    voidFrac;           // void volume / total volume
    double    kSat;               // saturated hydraulic conductivity (ft/sec)
    double    clogFactor;         // clogging factor
}  TStorageLayer;

// Underdrain System (part of Storage Layer)
typedef struct
{
    double    coeff;              // underdrain flow coeff. (in/hr or mm/hr)
    double    expon;              // underdrain head exponent (for in or mm)
    double    offset;             // offset height of underdrain (ft)
    double    delay;              // rain barrel drain delay time (sec)
}  TDrainLayer;

// Drainage Mat Layer (for green roofs)
typedef struct
{
    double    thickness;          // layer thickness (ft)
    double    voidFrac;           // void volume / total volume
    double    roughness;          // Mannings n for green roof drainage mats
    double    alpha;              // slope/roughness term in Manning equation
}  TDrainMatLayer;

// LID Process - generic LID design per unit of area
typedef struct
{
    char*          ID;            // identifying name
    int            lidType;       // type of LID
    TSurfaceLayer  surface;       // surface layer parameters
    TPavementLayer pavement;      // pavement layer parameters
    TSoilLayer     soil;          // soil layer parameters
    TStorageLayer  storage;       // storage layer parameters
    TDrainLayer    drain;         // underdrain system parameters
    TDrainMatLayer drainMat;      // drainage mat layer
}  TLidProc;

// Water Balance Statistics
typedef struct
{
    double         inflow;        // total inflow (ft)
    double         evap;          // total evaporation (ft)
    double         infil;         // total infiltration (ft)
    double         surfFlow;      // total surface runoff (ft)
    double         drainFlow;     // total underdrain flow (ft)
    double         initVol;       // initial stored volume (ft)
    double         finalVol;      // final stored volume (ft)
}  TWaterBalance;

// LID Report File
typedef struct
{
    FILE*     file;               // file pointer
    int       wasDry;             // number of successive dry periods          //(5.1.012)
    char      results[256];       // results for current time period           //(5.1.008)
}   TLidRptFile;

// LID Unit - specific LID process applied over a given area
typedef struct
{
    int      lidIndex;       // index of LID process
    int      number;         // number of replicate units
    double   area;           // area of single replicate unit (ft2)
    double   fullWidth;      // full top width of single unit (ft)
    double   botWidth;       // bottom width of single unit (ft)
    double   initSat;        // initial saturation of soil & storage layers
    double   fromImperv;     // fraction of impervious area runoff treated
    int      toPerv;         // 1 if outflow sent to pervious area; 0 if not
    int      drainSubcatch;  // subcatchment receiving drain flow              //(5.1.008)
    int      drainNode;      // node receiving drain flow                      //(5.1.008)
    TLidRptFile* rptFile;    // pointer to detailed report file

    TGrnAmpt soilInfil;      // infil. object for biocell soil layer 
    double   surfaceDepth;   // depth of ponded water on surface layer (ft)
    double   paveDepth;      // depth of water in porous pavement layer        //(5.1.011)
    double   soilMoisture;   // moisture content of biocell soil layer
    double   storageDepth;   // depth of water in storage layer (ft)

    // net inflow - outflow from previous time step for each LID layer (ft/s)
    double   oldFluxRates[MAX_LAYERS];
                                     
    double   dryTime;        // time since last rainfall (sec)
    double   oldDrainFlow;   // previous drain flow (cfs)                      //(5.1.008)
    double   newDrainFlow;   // current drain flow (cfs)                       //(5.1.008)
    TWaterBalance  waterBalance;     // water balance quantites
}  TLidUnit;

//-----------------------------------------------------------------------------
//   LID Methods
//-----------------------------------------------------------------------------
void     lid_create(Project *project, int lidCount, int subcatchCount);
void     lid_delete(Project *project);

int      lid_readProcParams(Project *project, char* tok[], int ntoks);
int      lid_readGroupParams(Project *project, char* tok[], int ntoks);

void     lid_validate(Project *project);
void     lid_initState(Project *project);
void     lid_setOldGroupState(Project *project, int subcatch);                                   //(5.1.008)

double   lid_getPervArea(Project *project, int subcatch);
double   lid_getFlowToPerv(Project *project, int subcatch);
double   lid_getDrainFlow(Project *project, int subcatch, int timePeriod);                       //(5.1.008)
double   lid_getStoredVolume(Project *project, int subcatch);

//double   lid_getSurfaceDepth(int subcatch);                                  //(5.1.008)
//double   lid_getDepthOnPavement(int subcatch, double impervDepth);           //(5.1.008)

void     lid_addDrainLoads(Project *project, int subcatch, double c[], double tStep);            //(5.1.008)
void     lid_addDrainRunon(Project *project, int subcatch);                                      //(5.1.008)
void     lid_addDrainInflow(Project *project, int subcatch, double f);                           //(5.1.008)

void     lid_getRunoff(Project *project, int subcatch, double tStep);                            //(5.1.008)

void     lid_writeSummary(Project *project);
void     lid_writeWaterBalance(Project *project);

//-----------------------------------------------------------------------------

void     lidproc_initWaterBalance(TLidUnit *lidUnit, double initVol);

double   lidproc_getOutflow(Project *project, TLidUnit* lidUnit, TLidProc* lidProc,
         double inflow, double evap, double infil, double maxInfil,            //(5.1.008)
         double tStep, double* lidEvap, double* lidInfil, double* lidDrain);   //(5.1.008)

void     lidproc_saveResults(Project *project, TLidUnit* lidUnit, double ucfRainfall,            //(5.1.011)
         double ucfRainDepth);

#endif //LID_H
