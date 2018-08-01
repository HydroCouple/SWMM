/*!
 * \file hash.h
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
//   hash.h
//
//   Header file for Hash Table module hash.c.
//-----------------------------------------------------------------------------

#ifndef HASH_H
#define HASH_H

#define HTMAXSIZE 1999
#define NOTFOUND  -1

struct HTentry
{
    char   *key;
    int    data;
    struct HTentry *next;
};

typedef struct HTentry *HTtable;

HTtable *HTcreate(void);
int     HTinsert(HTtable *, char *, int);
int     HTfind(HTtable *, char *);
char    *HTfindKey(HTtable *, char *);
void    HTfree(HTtable *);


#endif //HASH_H
