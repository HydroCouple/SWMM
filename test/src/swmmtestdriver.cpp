/*!
 * \file swmmtestclass.cpp
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version 5.1.012
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


#ifdef SWMM_TEST

#include <cstdio>
#include "swmmtestclass.h"

int main(int argc, char** argv)
{
   int status = 0;

   //Test One
   {
     SWMMTestClass swmmTestObject;
     status |= QTest::qExec(&swmmTestObject, argc, argv);
   }

   return status;
}

#endif
