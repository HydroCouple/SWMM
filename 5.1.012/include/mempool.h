/*!
 * \file mempool.h
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
//  mempool.h
//
//  Header for mempool.c
//
//  The type alloc_handle_t provides an opaque reference to the
//  alloc pool - only the alloc routines know its structure.
//-----------------------------------------------------------------------------

#ifndef MEMPOOL_H
#define MEMPOOL_H

typedef struct
{
   long  dummy;
}  alloc_handle_t;

typedef struct Project Project;

alloc_handle_t *AllocInit(Project *project);
char           *Alloc(Project *project, long);
alloc_handle_t *AllocSetPool(Project *project, alloc_handle_t *);
void            AllocReset(Project *project);
void            AllocFreePool(Project *project);

#endif //MEMPOOL_H
