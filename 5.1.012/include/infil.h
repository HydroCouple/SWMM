/*!
 * \file infil.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version SWMM 5.1.012
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
//   infil.h
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    03/20/14   (Build 5.1.001)
//            09/15/14   (Build 5.1.007)
//            08/05/15   (Build 5.1.010)
//   Author:  L. Rossman (US EPA)
//
//   Public interface for infiltration functions.
//
//   Build 5.1.010:
//   - New Modified Green Ampt infiltration option added.
//-----------------------------------------------------------------------------

#ifndef INFIL_H
#define INFIL_H

//---------------------
// Enumerated Constants
//---------------------
enum InfilType {
     HORTON,                      // Horton infiltration
     MOD_HORTON,                  // Modified Horton infiltration
     GREEN_AMPT,                  // Green-Ampt infiltration
     MOD_GREEN_AMPT,              // Modified Green-Ampt infiltration          //(5.1.010)
     CURVE_NUMBER};               // SCS Curve Number infiltration


//---------------------
// Horton Infiltration moxed from exfil.h
//---------------------
typedef struct
{
    double        f0;              // initial infil. rate (ft/sec)
    double        fmin;            // minimum infil. rate (ft/sec)
    double        Fmax;            // maximum total infiltration (ft);
    double        decay;           // decay coeff. of infil. rate (1/sec)
    double        regen;           // regeneration coeff. of infil. rate (1/sec)
    //-----------------------------
    double        tp;              // present time on infiltration curve (sec)
    double        Fe;              // cumulative infiltration (ft)
}  THorton;


//-------------------------
// Green-Ampt Infiltration
//-------------------------

// ----  Some variable names changed for release 5.1.007  ----                 //(5.1.007)
typedef struct
{
    double        S;               // avg. capillary suction (ft)
    double        Ks;              // saturated conductivity (ft/sec)
    double        IMDmax;          // max. soil moisture deficit (ft/ft)
    //-----------------------------
    double        IMD;             // current initial soil moisture deficit
    double        F;               // current cumulative infiltrated volume (ft)
    double        Fu;              // current upper zone infiltrated volume (ft)
    double        Lu;              // depth of upper soil zone (ft)
    double        T;               // time until start of next rain event (sec)
    char          Sat;             // saturation flag

} TGrnAmpt;


//--------------------------
// Curve Number Infiltration
//--------------------------
typedef struct
{
    double        Smax;            // max. infiltration capacity (ft)
    double        regen;           // infil. capacity regeneration constant (1/sec)
    double        Tmax;            // maximum inter-event time (sec)
    //-----------------------------
    double        S;               // current infiltration capacity (ft)
    double        F;               // current cumulative infiltration (ft)
    double        P;               // current cumulative precipitation (ft)
    double        T;               // current inter-event time (sec)
    double        Se;              // current event infiltration capacity (ft)
    double        f;               // previous infiltration rate (ft/sec)

} TCurveNum;

//-----------------------------------------------------------------------------
//   Exported Variables
//-----------------------------------------------------------------------------
//extern THorton*   HortInfil;
//extern TGrnAmpt*  GAInfil;
//extern TCurveNum* CNInfil;

typedef struct Project Project;

//-----------------------------------------------------------------------------
//   Infiltration Methods
//-----------------------------------------------------------------------------
void    infil_create(Project *project, int subcatchCount, int model);
void    infil_delete(Project *project);
int     infil_readParams(Project *project, int model, char* tok[], int ntoks);
void    infil_initState(Project *project, int area, int model);
void    infil_getState(Project *project, int j, int m, double x[]);
void    infil_setState(Project *project, int j, int m, double x[]);
double  infil_getInfil(Project *project, int area, int model, double tstep, double rainfall,
        double runon, double depth);

int     grnampt_setParams(Project *project, TGrnAmpt *infil, double p[]);
void    grnampt_initState(TGrnAmpt *infil);
double  grnampt_getInfil(Project *project, TGrnAmpt *infil, double tstep, double irate,
        double depth, int modelType);                                          //(5.1.010)

#endif //INFIL_H
