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



#include "dataexchangecache.h"
#include "headers.h"
#include "funcs.h"
#include "couplingdatacache.h"

#include <unordered_map>

typedef struct OpenMIDataCache OpenMIDataCache;

using namespace std;

void initializeCouplingDataCache(Project *project)
{
  if(project->couplingDataCache == nullptr)
  {
    CouplingDataCache* couplingDataCache = new CouplingDataCache();
    project->couplingDataCache = couplingDataCache;

    int max = project->Nobjects[NODE];

    for (int j = 0; j < max; j++)
    {
      couplingDataCache->NodeLateralInflows[j] = 0.0;
    }
  }
}

//node lateral inflow
void addNodeLateralInflow(Project* project, int index, double value)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;
  couplingDataCache->NodeLateralInflows[index] += value;
}

int containsNodeLateralInflow(Project* project, int index, double* const  value)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;

  unordered_map<int, double >::iterator it = couplingDataCache->NodeLateralInflows.find(index);

  if (it != couplingDataCache->NodeLateralInflows.end())
  {
    *value = (*it).second;
    return 1;
  }

  return 0;
}

//node lateral inflow
int removeNodeLateralInflow(Project* project, int index)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;
  return couplingDataCache->NodeLateralInflows.erase(index);
}

//Node Depths
void addNodeDepth(Project* project, int index, double value)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;
  couplingDataCache->NodeDepths[index] = value;
}

int containsNodeDepth(Project* project, int index, double* const value)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;

  unordered_map<int, double > ::iterator it = couplingDataCache->NodeDepths.find(index);

  if (it != couplingDataCache->NodeDepths.end())
  {
    *value = (*it).second;
    return 1;
  }

  return 0;

}

int removeNodeDepth(Project* project, int index)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;
  return couplingDataCache->NodeDepths.erase(index);
}

//SubcatchRainfall
void addSubcatchRain(Project* project, int index, double value)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;
  couplingDataCache->SubcatchRainfall[index] = value;
}

int containsSubcatchRain(Project* project, int index, double* const value)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;

  unordered_map<int, double > ::iterator it = couplingDataCache->SubcatchRainfall.find(index);

  if (it != couplingDataCache->SubcatchRainfall.end())
  {
    *value = (*it).second;
    return 1;
  }

  return 0;
}

int removeSubcatchRain(Project* project, int index)
{
  int removed = 0;

  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;
  removed = couplingDataCache->SubcatchRainfall.erase(index);

  return removed;
}

void clearDataCache(Project *project)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;

  if(couplingDataCache)
  {
    int max = project->Nobjects[NODE];

    for (int j = 0; j < max; j++)
    {
      couplingDataCache->NodeLateralInflows[j] = 0.0;
    }

    couplingDataCache->NodeDepths.clear();
    couplingDataCache->SubcatchRainfall.clear();
    couplingDataCache->XSections.clear();
  }
}

void disposeCoupledDataCache(Project *project)
{
  CouplingDataCache* couplingDataCache  = (CouplingDataCache*)project->couplingDataCache;

  if(couplingDataCache)
  {
    couplingDataCache->NodeLateralInflows.clear();
    couplingDataCache->NodeDepths.clear();
    couplingDataCache->SubcatchRainfall.clear();
    couplingDataCache->XSections.clear();
    delete couplingDataCache;
    project->couplingDataCache = nullptr;
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

