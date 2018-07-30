/*!
 * \file dataexchangecache.cpp
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


#include "stdafx.h"

#include <map>
#include <unordered_map>

#include "dataexchangecache.h"
#include "headers.h"
#include "funcs.h"

using namespace std;

unordered_map<Project*, unordered_map<int, double>> NodeLateralInflows;
unordered_map<Project*, unordered_map<int, double>> NodeDepths;
unordered_map<Project*, unordered_map<int, double>> SubcatchRainfall;
unordered_map<Project*, unordered_map<int, map<double,double>>> XSections;

typedef struct OpenMIDataCache OpenMIDataCache;

//node lateral inflow
void addNodeLateralInflow(Project* project, int index, double value)
{
   NodeLateralInflows[project][index] = value;
}

int containsNodeLateralInflow(Project* project, int index, double* const  value)
{
    if(NodeLateralInflows.size())
    {
        unordered_map<Project*, unordered_map<int, double> >::iterator it = NodeLateralInflows.find(project);

        if (it != NodeLateralInflows.end())
        {
            unordered_map<int, double > foundProject = (*it).second;

            unordered_map<int, double > ::iterator it1 = foundProject.find(index);

            if (it1 != foundProject.end())
            {
                *value = (*it1).second;
                return 1;
            }
        }
    }

   return 0;
}

//node lateral inflow
int removeNodeLateralInflow(Project* project, int index)
{
   unordered_map<Project*, unordered_map<int, double> >::iterator it = NodeLateralInflows.find(project);

   if (it != NodeLateralInflows.end())
   {
      unordered_map<int, double > foundProject = (*it).second;
      return foundProject.erase(index);
   }

   return 0;
}

//Node Depths
void addNodeDepth(Project* project, int index, double value)
{
   NodeDepths[project][index] = value;
}

int containsNodeDepth(Project* project, int index, double* const value)
{
    if(NodeDepths.size())
    {
        unordered_map<Project*, unordered_map<int, double> >::iterator it = NodeDepths.find(project);

        if (it != NodeDepths.end())
        {
            unordered_map<int, double > foundProject = (*it).second;

            unordered_map<int, double > ::iterator it1 = foundProject.find(index);

            if (it1 != foundProject.end())
            {
                *value = (*it1).second;
                return 1;
            }
        }
    }

    return 0;
}

int removeNodeDepth(Project* project, int index)
{
   unordered_map<Project*, unordered_map<int, double> >::iterator it = NodeDepths.find(project);

   if (it != NodeDepths.end())
   {
      unordered_map<int, double > foundProject = (*it).second;
      return foundProject.erase(index);
   }

   return 0;
}

//SubcatchRainfall
void addSubcatchRain(Project* project, int index, double value)
{
   SubcatchRainfall[project][index] = value;
}

int containsSubcatchRain(Project* project, int index, double* const value)
{

   unordered_map<Project*, unordered_map<int, double> >::iterator it = SubcatchRainfall.find(project);

   if (it != SubcatchRainfall.end())
   {
      unordered_map<int, double > foundProject = (*it).second;

      unordered_map<int, double > ::iterator it1 = foundProject.find(index);

      if (it1 != foundProject.end())
      {
         *value = (*it1).second;
         return 1;
      }
   }

   return 0;
}

int removeSubcatchRain(Project* project, int index)
{
   unordered_map<Project*, unordered_map<int, double> >::iterator it = SubcatchRainfall.find(project);

   if (it != SubcatchRainfall.end())
   {
      unordered_map<int, double > foundProject = (*it).second;
      return foundProject.erase(index);
   }

   return 0;
}

void clearDataCache(Project *project)
{
#ifdef USE_OPENMP
#pragma omp critical (SWMM5)
#endif
    {
        NodeLateralInflows.erase(project);
        NodeDepths.erase(project);
        SubcatchRainfall.erase(project);
    }
}

/*!
 * \brief applyCouplingNodeDepths
 * \param project
 */
void applyCouplingNodeDepths(Project* project)
{
  int j;
  int max = project->Nobjects[NODE];

  for (j = 0; j < max; j++)
  {
    double value = 0;
    TNode *node = &project->Node[j];
    node->depthSetExternally = 0;

    if (containsNodeDepth(project, j, &value))
    {

      node->oldDepth = value;
      node->newDepth = value;
      node->depthSetExternally = 1;

      if(value > node->fullDepth && node->pondedArea > 0)
      {
        if(value <= node->fullDepth)
        {
          node->oldVolume = node_getVolume(project, j, value);
        }
        else
        {
          node->oldVolume = node->fullVolume + (node->oldDepth - node->fullDepth) * node->pondedArea;
        }
      }
    }
  }
}

/*!
 * \brief applyCouplingLateralInflows
 * \param project
 */
void applyCouplingLateralInflows(Project* project)
{
  int j;
  int max = project->Nobjects[NODE];

  for(j = 0; j < max; j++)
  {
    double value = 0;

    if(containsNodeLateralInflow(project, j, &value))
    {
      TNode* node = &project->Node[j];
      node->newLatFlow += value;
      massbal_addInflowFlow(project, EXTERNAL_INFLOW, value);
    }
  }
}

