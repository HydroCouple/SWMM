/*!
 * \file mathexpr.h
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

/******************************************************************************
**  MODULE:        MATHEXPR.H
**  PROJECT:       SWMM 5.1
**  DESCRIPTION:   header file for the math expression parser in mathexpr.c.
**  AUTHORS:       L. Rossman, US EPA - NRMRL
**                 F. Shang, University of Cincinnati
**  VERSION:       5.1.001
**  LAST UPDATE:   03/20/14
******************************************************************************/

#ifndef MATHEXPR_H
#define MATHEXPR_H


//  Node in a tokenized math expression list moved mode mathexpr.h
struct ExprNode
{
    int    opcode;                // operator code
    int    ivar;                  // variable index
    double fvalue;                // numerical value
    struct ExprNode *prev;        // previous node
    struct ExprNode *next;        // next node

};

typedef struct ExprNode MathExpr;
typedef struct Project Project;

//  Creates a tokenized math expression from a string
MathExpr* mathexpr_create(Project* project, char* s, int (*getVar) (Project*, char *));

//  Evaluates a tokenized math expression
double mathexpr_eval(Project* project, MathExpr* expr, double (*getVal) (Project*, int));

//  Deletes a tokenized math expression
void  mathexpr_delete(MathExpr* expr);

#endif //MATHEXPR_H
