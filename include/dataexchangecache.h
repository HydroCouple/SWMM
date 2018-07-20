/*!
 * \file dataexchangecache.h
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


#ifndef DATAEXCHANGECACHE_H
#define DATAEXCHANGECACHE_H

#undef WINDOWS
#ifdef _WIN32
#define WINDOWS
#endif
#ifdef __WIN32__
#define WINDOWS
#endif


#ifdef WINDOWS
  #define DLLEXPORT __declspec(dllexport)
#else
  #define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Project Project;

void DLLEXPORT addNodeLateralInflow(Project* project, int index, double value);
int DLLEXPORT containsNodeLateralInflow(Project* project, int index, double* value);
int DLLEXPORT removeNodeLateralInflow(Project* project, int index);

void DLLEXPORT addNodeDepth(Project* project, int index, double value);
int DLLEXPORT  containsNodeDepth(Project* project, int index, double* value);
int DLLEXPORT  removeNodeDepth(Project* project, int index);

void DLLEXPORT addSubcatchRain(Project* project, int index, double value);
int DLLEXPORT containsSubcatchRain(Project* project, int index, double* value);
int DLLEXPORT removeSubcatchRain(Project* project, int index);

void DLLEXPORT clearDataCache(Project* project);

//-----------------------------------------------------------------------------
//   Coupling Functions
//-----------------------------------------------------------------------------

void DLLEXPORT applyCouplingNodeDepths(Project* project);

void DLLEXPORT applyCouplingLateralInflows(Project* project);


#ifdef __cplusplus
}   // matches the linkage specification from above */
#endif

#endif // DATAEXCHANGECACHE_H

