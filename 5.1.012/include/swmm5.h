/*!
 * \file swmm5.h
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
//   swmm5.h
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    03/24/14  (Build 5.1.001)
//            08/01/16  (Build 5.1.011)
//   Author:  L. Rossman
//
//   Prototypes for SWMM5 functions exported to swmm5.dll.
//
//-----------------------------------------------------------------------------
#ifndef SWMM5_H
#define SWMM5_H

// --- define WINDOWS

#undef WINDOWS
#ifdef _WIN32
#define WINDOWS
#endif
#ifdef __WIN32__
#define WINDOWS
#endif

// --- define DLLEXPORT

#ifdef WINDOWS
#define DLLEXPORT __declspec(dllexport) __stdcall
#else
#if __GNUC__ >= 4
#define DLLEXPORT __attribute__ ((visibility ("default")))
#else
#define DLLEXPORT
#define DLLEXPORT_S
#endif
#endif

// --- use "C" linkage for C++ programs

#ifdef __cplusplus
extern "C" { 
#endif 

typedef struct Project Project;

#ifdef WINDOWS
  __declspec(dllexport) Project* __stdcall swmm_createProject();
#else
 Project* DLLEXPORT swmm_createProject();
#endif

void DLLEXPORT swmm_deleteProject(Project* project);
int  DLLEXPORT  swmm_run(Project *project, char* f1, char* f2, char* f3);
int  DLLEXPORT  swmm_open(Project *project, char* f1, char* f2, char* f3);
int  DLLEXPORT  swmm_start(Project *project, int saveFlag);
int  DLLEXPORT  swmm_step(Project *project, double* elapsedTime);
int  DLLEXPORT  swmm_end(Project *project);
int  DLLEXPORT  swmm_report(Project *project);
int  DLLEXPORT  swmm_getMassBalErr(Project *project, float* runoffErr, float* flowErr, float* qualErr);
int  DLLEXPORT  swmm_close(Project *project);
int  DLLEXPORT  swmm_getVersion(void);
int  DLLEXPORT  swmm_getError(Project *project, char* errMsg, int msgLen);                      //(5.1.011)
int  DLLEXPORT  swmm_getWarnings(Project *project);                                       //(5.1.011)


#ifdef __cplusplus 
}   // matches the linkage specification from above */ 
#endif

#endif
