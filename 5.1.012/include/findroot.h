/*!
 * \file findroot.h
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
//   findroot.h
//
//   Header file for root finding method contained in findroot.c
//
//   Last modified on 11/19/13.
//-----------------------------------------------------------------------------

#ifndef FINDROOT_H
#define FINDROOT_H

typedef struct Project Project;

int findroot_Newton(Project *project, double x1, double x2, double* rts, double xacc,
		    void (*func) (Project* project, double x, double* f, double* df, void* p),
					void* p);
double findroot_Ridder(Project *project, double x1, double x2, double xacc,
			   double (*func)(Project*, double, void* p), void* p);

#endif //FINDROOT_H
