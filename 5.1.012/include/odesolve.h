/*!
 * \file odesolve.h
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
//  odesolve.h
//
//  Header file for ODE solver contained in odesolve.c
//
//-----------------------------------------------------------------------------

#ifndef ODESOLVE_H
#define ODESOLVE_H

typedef struct Project Project;

// functions that open, close, and use the ODE solver
int  odesolve_open(Project *project, int n);
void odesolve_close(Project *project);
int  odesolve_integrate(Project *project, double ystart[], int n, double x1, double x2,
     double eps, double h1, void (*derivs)(Project *, double, double*, double*));

#endif //ODESOLVE_H
