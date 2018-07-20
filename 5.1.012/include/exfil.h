/*!
 * \file exfil.h
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
//   exfil.h
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    09/15/14   (Build 5.1.007)
//   Author:  L. Rossman (US EPA)
//
//   Public interface for exfiltration functions.
//-----------------------------------------------------------------------------

#ifndef EXFIL_H
#define EXFIL_H


//----------------------------
// EXFILTRATION OBJECT
//----------------------------
typedef struct
{
    TGrnAmpt*  btmExfil;
    TGrnAmpt*  bankExfil;
    double     btmArea;
    double     bankMinDepth;
    double     bankMaxDepth;
    double     bankMaxArea;
}   TExfil;

typedef struct Project Project;

//-----------------------------------------------------------------------------
//   Exfiltration Methods
//-----------------------------------------------------------------------------

int    exfil_readStorageParams(Project *project, int k, char* tok[], int ntoks, int n);
void   exfil_initState(Project *project, int k);
double exfil_getLoss(Project *project, TExfil* exfil, double tStep, double depth, double area);

#endif //EXFIL_H
