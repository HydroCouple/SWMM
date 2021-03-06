
/*!
 * \file climate.c
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version 5.1.012
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
//   routing.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/19/14  (Build 5.1.000)
//             09/15/14  (Build 5.1.007)
//             04/02/15  (Build 5.1.008)
//             08/05/15  (Build 5.1.010)
//             08/01/16  (Build 5.1.011)
//             03/14/17  (Build 5.1.012)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//
//   Conveyance system routing functions.
//
//   Build 5.1.007:
//   - Nodal evap/seepage losses computed using conditions at start of time step.
//   - DWF pollutant concentrations ignored if DWF is negative.
//   - Separate mass balance accounting made for storage evap. & seepage.
//   - Nodal mass balance accounting for negative lateral inflows corrected.
//
//   Build 5.1.008:
//   - Initialization of flow and quality routing systems moved here from swmm5.c.
//   - Lateral inflows now evaluated at start (not end) of time step.
//   - Flows from LID drains included in lateral inflows.
//   - Conduit evap/seepage losses multiplied by number of barrels before
//     being added into mass balances.
//
//   Build 5.1.010:
//   - Time when a link's setting is changed is recorded.
//
//   Build 5.1.011:
//   - Support added for limiting flow routing to specific events.
//
//   Build 5.1.012:
//   - routing_execute() was re-written so that Routing Events and
//     Skip Steady Flow options work together correctly.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers.h"
#include "lid.h"                                                               //(5.1.008)
#include "dataexchangecache.h"

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
// routing_open            (called by swmm_start in swmm5.c)
// routing_getRoutingStep  (called by swmm_step in swmm5.c)
// routing_execute         (called by swmm_step in swmm5.c)
// routing_close           (called by swmm_end in swmm5.c)

//-----------------------------------------------------------------------------
// Function declarations
//-----------------------------------------------------------------------------
static void addExternalInflows(Project *project, DateTime currentDate);
static void addDryWeatherInflows(Project *project, DateTime currentDate);
static void addWetWeatherInflows(Project *project, double routingTime);
static void addGroundwaterInflows(Project *project, double routingTime);
static void addRdiiInflows(Project *project, DateTime currentDate);
static void addIfaceInflows(Project *project, DateTime currentDate);
static void addLidDrainInflows(Project *project, double routingTime);                            //(5.1.008)
static void removeStorageLosses(Project *project, double tStep);
static void removeConduitLosses(Project *project);
static void removeOutflows(Project *project, double tStep);                                      //(5.1.008)
static int  inflowHasChanged(Project *project);
static void sortEvents(Project *project);                                                  //(5.1.011)

//=============================================================================

int routing_open(Project *project)
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: initializes the routing analyzer.
//
{
    // --- open treatment system
    if ( !treatmnt_open(project) ) return project->ErrorCode;

    // --- topologically sort the links
    project->SortedLinks = NULL;
    if ( project->Nobjects[LINK] > 0 )
    {
        project->SortedLinks = (int *) calloc(project->Nobjects[LINK], sizeof(int));
        if ( !project->SortedLinks )
        {
            report_writeErrorMsg(project, ERR_MEMORY, "");
            return project->ErrorCode;
        }
        toposort_sortLinks(project, project->SortedLinks);
        if ( project->ErrorCode ) return project->ErrorCode;
    }

    // --- open any routing interface files
    iface_openRoutingFiles(project);

    // --- initialize flow and quality routing systems                         //(5.1.008)
    flowrout_init(project, project->RouteModel);                                                 //(5.1.008)
    if ( project->Fhotstart1.mode == NO_FILE ) qualrout_init(project);                         //(5.1.008)

    // --- initialize routing events                                           //(5.1.011)
    if ( project->NumEvents > 0 ) sortEvents(project);                                         //(5.1.011)
    project->NextEvent = 0;                                                             //(5.1.011)
    //InSteadyState = (project->NumEvents > 0);                                         //(5.1.012)
    project->BetweenEvents = (project->NumEvents > 0);                                           //(5.1.012)
    return project->ErrorCode;
}

//=============================================================================

void routing_close(Project *project, int routingModel)
//
//  Input:   routingModel = routing method code
//  Output:  none
//  Purpose: closes down the routing analyzer.
//
{
    // --- close any routing interface files
    iface_closeRoutingFiles(project);

    // --- free allocated memory
    flowrout_close(project, routingModel);
    treatmnt_close(project);
    FREE(project->SortedLinks);
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

double routing_getRoutingStep(Project *project, int routingModel, double fixedStep)
//
//  Input:   routingModel = routing method code
//           fixedStep = user-supplied time step (sec)
//  Output:  returns a routing time step (sec)
//  Purpose: determines time step used for flow routing at current time period.
//
{
    double date1, date2, nextTime;

    if ( project->Nobjects[LINK] == 0 ) return fixedStep;

    // --- find largest step possible if between routing events
    if ( project->NumEvents > 0 && project->BetweenEvents )                                      //(5.1.012)
    {
        nextTime = MIN(project->NewRunoffTime, project->ReportTime);
        date1 = getDateTime(project, project->NewRoutingTime);
        date2 = getDateTime(project, nextTime);
        if ( date2 > date1 && date2 < project->Event[project->NextEvent].start )
        {
            return (nextTime - project->NewRoutingTime) / 1000.0;
        }
        else
        {
            date1 = getDateTime(project, project->NewRoutingTime + 1000.0 * fixedStep);
            if ( date1 < project->Event[project->NextEvent].start ) return fixedStep;
        }
    }

    // --- otherwise use a regular flow-routing based time step
    return flowrout_getRoutingStep(project, routingModel, fixedStep);
}

//=============================================================================

////  This function was re-written for release 5.1.012.  ////                  //(5.1.012)

void routing_execute(Project *project, int routingModel, double routingStep)
//
//  Input:   routingModel = routing method code
//           routingStep = routing time step (sec)
//  Output:  none
//  Purpose: executes the routing process at the current time period.
//
{
    int      j;
    int      stepCount = 1;
    int      actionCount = 0;
    int      inSteadyState = FALSE;
    DateTime currentDate;
    double   stepFlowError;

    // --- update continuity with current state
    //     applied over 1/2 of time step
    if ( project->ErrorCode ) return;
    massbal_updateRoutingTotals(project, routingStep/2.);

    // --- find new link target settings that are not related to
    // --- control rules (e.g., pump on/off depth limits)
    for (j=0; j<project->Nobjects[LINK]; j++) link_setTargetSetting(project, j);

    // --- find new target settings due to control rules
    currentDate = getDateTime(project, project->NewRoutingTime);
    controls_evaluate(project, currentDate, currentDate - project->StartDateTime,
                      routingStep/SECperDAY);

    // --- change each link's actual setting if it differs from its target
    for (j=0; j<project->Nobjects[LINK]; j++)
    {
        if ( project->Link[j].targetSetting != project->Link[j].setting )
        {
            // --- update time when link was switched between open & closed
            if ( project->Link[j].targetSetting * project->Link[j].setting == 0.0 )
                project->Link[j].timeLastSet = currentDate;

            // --- implement the change in the link's setting
            link_setSetting(project, j, routingStep);
            actionCount++;
        } 
    }

    // --- update value of elapsed routing time (in milliseconds)
    project->OldRoutingTime = project->NewRoutingTime;
    project->NewRoutingTime = project->NewRoutingTime + 1000.0 * routingStep;

    // --- initialize mass balance totals for time step
    stepFlowError = massbal_getStepFlowError(project);
    massbal_initTimeStepTotals(project);

    // --- replace old water quality state with new state
    if ( project->Nobjects[POLLUT] > 0 )
    {
        for (j=0; j<project->Nobjects[NODE]; j++) node_setOldQualState(project, j);
        for (j=0; j<project->Nobjects[LINK]; j++) link_setOldQualState(project, j);
    }

    // --- initialize lateral inflows at nodes
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        project->Node[j].oldLatFlow  = project->Node[j].newLatFlow;
        project->Node[j].newLatFlow  = 0.0;
    }

    // --- check if can skip non-event periods
    if ( project->NumEvents > 0 )
    {
        if ( currentDate > project->Event[project->NextEvent].end )
        {
            project->BetweenEvents = TRUE;
            project->NextEvent++;
        }
        else if ( currentDate >= project->Event[project->NextEvent].start && project->BetweenEvents == TRUE )
        {
                        project->BetweenEvents = FALSE;
        }
    }

    // --- if not between routing events
    if ( project->BetweenEvents == FALSE )
    {
        // --- find evap. & seepage losses from storage nodes
        for (j = 0; j < project->Nobjects[NODE]; j++)
        {
            project->Node[j].losses = node_getLosses(project, j, routingStep);
        }

        // --- add lateral inflows and evap/seepage losses at nodes
        addExternalInflows(project, currentDate);
        addDryWeatherInflows(project, currentDate);
        addWetWeatherInflows(project, project->OldRoutingTime);
        addGroundwaterInflows(project, project->OldRoutingTime);
        addLidDrainInflows(project, project->OldRoutingTime);
        addRdiiInflows(project, currentDate);
        addIfaceInflows(project, currentDate);

        // add coupling lateral inflows
        applyCouplingLateralInflows(project);

        // --- check if can skip steady state periods based on flows
        if ( project->SkipSteadyState )
        {
            if ( project->OldRoutingTime == 0.0
            ||   actionCount > 0
            ||   fabs(stepFlowError) > project->SysFlowTol
            ||   inflowHasChanged(project) ) inSteadyState = FALSE;
            else inSteadyState = TRUE;
        }

        // --- find new hydraulic state if system has changed
        if ( inSteadyState == FALSE )
        {
            // --- replace old hydraulic state values with current ones
            for (j = 0; j < project->Nobjects[LINK]; j++) link_setOldHydState(project, j);
            for (j = 0; j < project->Nobjects[NODE]; j++)
            {
                node_setOldHydState(project, j);
                node_initInflow(project, j, routingStep);
            }

            // --- route flow through the drainage network
            if ( project->Nobjects[LINK] > 0 )
            {
                stepCount = flowrout_execute(project, project->SortedLinks, routingModel, routingStep);
            }
        }

        // --- route quality through the drainage network
        if ( project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality )
        {
            qualrout_execute(project, routingStep);
        }

        // --- remove evaporation, infiltration & outflows from system
        removeStorageLosses(project, routingStep);
        removeConduitLosses(project);
        removeOutflows(project, routingStep);
    }
    else inSteadyState = TRUE;
	
    // --- update continuity with new totals
    //     applied over 1/2 of routing step
    massbal_updateRoutingTotals(project, routingStep/2.);

    // --- update summary statistics
    if ( project->RptFlags.flowStats && project->Nobjects[LINK] > 0 )
    {
        stats_updateFlowStats(project, routingStep, getDateTime(project, project->NewRoutingTime),
                              stepCount, inSteadyState);
    }
}

//=============================================================================

void addExternalInflows(Project *project, DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds direct external inflows to nodes at current date.
//
{
    int     j, p;
    double  q, w;
    TExtInflow* inflow;

    // --- for each node with a defined external inflow
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        inflow = project->Node[j].extInflow;
        if ( !inflow ) continue;

        // --- get flow inflow
        q = 0.0;
        while ( inflow )
        {
            if ( inflow->type == FLOW_INFLOW )
            {
                q = inflow_getExtInflow(project, inflow, currentDate);
                break;
            }
            else inflow = inflow->next;
        }
        if ( fabs(q) < FLOW_TOL ) q = 0.0;

        // --- add flow inflow to node's lateral inflow
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(project, EXTERNAL_INFLOW, q);

        // --- add on any inflow (i.e., reverse flow) through an outfall
        if ( project->Node[j].type == OUTFALL && project->Node[j].oldNetInflow < 0.0 )
        {
            q = q - project->Node[j].oldNetInflow;
        }

        // --- get pollutant mass inflows
        inflow = project->Node[j].extInflow;
        while ( inflow )
        {
            if ( inflow->type != FLOW_INFLOW )
            {
                p = inflow->param;
                w = inflow_getExtInflow(project, inflow, currentDate);
                if ( inflow->type == CONCEN_INFLOW ) w *= q;
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(project, EXTERNAL_INFLOW, p, w);
            }
            inflow = inflow->next;
        }
    }
}

//=============================================================================

void addDryWeatherInflows(Project *project, DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds dry weather inflows to nodes at current date.
//
{
    int      j, p;
    int      month, day, hour;
    double   q, w;
    TDwfInflow* inflow;

    // --- get month (zero-based), day-of-week (zero-based),
    //     & hour-of-day for routing date/time
    month = datetime_monthOfYear(currentDate) - 1;
    day   = datetime_dayOfWeek(currentDate) - 1;
    hour  = datetime_hourOfDay(currentDate);

    // --- for each node with a defined dry weather inflow
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        inflow = project->Node[j].dwfInflow;
        if ( !inflow ) continue;

        // --- get flow inflow (i.e., the inflow whose param code is -1)
        q = 0.0;
        while ( inflow )
        {
            if ( inflow->param < 0 )
            {
                q = inflow_getDwfInflow(project, inflow, month, day, hour);
                break;
            }
            inflow = inflow->next;
        }
        if ( fabs(q) < FLOW_TOL ) q = 0.0;

        // --- add flow inflow to node's lateral inflow
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(project, DRY_WEATHER_INFLOW, q);

        // --- stop if inflow is non-positive
        if ( q <= 0.0 ) continue;                                              //(5.1.007)

        // --- add default DWF pollutant inflows
        for ( p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            if ( project->Pollut[p].dwfConcen > 0.0 )
            {
                w = q * project->Pollut[p].dwfConcen;
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(project, DRY_WEATHER_INFLOW, p, w);
            }
        }

        // --- get pollutant mass inflows
        inflow = project->Node[j].dwfInflow;
        while ( inflow )
        {
            if ( inflow->param >= 0 )
            {
                p = inflow->param;
                w = q * inflow_getDwfInflow(project, inflow, month, day, hour);
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(project, DRY_WEATHER_INFLOW, p, w);

                // --- subtract off any default inflow
                if ( project->Pollut[p].dwfConcen > 0.0 )
                {
                    w = q * project->Pollut[p].dwfConcen;
                    project->Node[j].newQual[p] -= w;
                    massbal_addInflowQual(project, DRY_WEATHER_INFLOW, p, -w);
                }
            }
            inflow = inflow->next;
        }
    }
}

//=============================================================================

void addWetWeatherInflows(Project *project, double routingTime)
//
//  Input:   routingTime = elasped time (millisec)
//  Output:  none
//  Purpose: adds runoff inflows to nodes at current elapsed time.
//
{
    int    i, j, p;
    double q, w;
    double f;

    // --- find where current routing time lies between latest runoff times
    if ( project->Nobjects[SUBCATCH] == 0 ) return;
    f = (routingTime - project->OldRunoffTime) / (project->NewRunoffTime - project->OldRunoffTime);
    if ( f < 0.0 ) f = 0.0;
    if ( f > 1.0 ) f = 1.0;

    // for each subcatchment outlet node,
    // add interpolated runoff flow & pollutant load to node's inflow
    for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
    {
        j = project->Subcatch[i].outNode;
        if ( j >= 0)
        {
            // add runoff flow to lateral inflow
            q = subcatch_getWtdOutflow(project, i, f);     // current runoff flow
            project->Node[j].newLatFlow += q;
            massbal_addInflowFlow(project, WET_WEATHER_INFLOW, q);

            // add pollutant load
            for (p = 0; p < project->Nobjects[POLLUT]; p++)
            {
                w = surfqual_getWtdWashoff(project, i, p, f);                           //(5.1.008)
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(project, WET_WEATHER_INFLOW, p, w);
            }
        }
    }
}

//=============================================================================

void addGroundwaterInflows(Project *project, double routingTime)
//
//  Input:   routingTime = elasped time (millisec)
//  Output:  none
//  Purpose: adds groundwater inflows to nodes at current elapsed time.
//
{
    int    i, j, p;
    double q, w;
    double f;
    TGroundwater* gw;

    // --- find where current routing time lies between latest runoff times
    if ( project->Nobjects[SUBCATCH] == 0 ) return;
    f = (routingTime - project->OldRunoffTime) / (project->NewRunoffTime - project->OldRunoffTime);
    if ( f < 0.0 ) f = 0.0;
    if ( f > 1.0 ) f = 1.0;

    // --- for each subcatchment
    for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
    {
        // --- see if subcatch contains groundwater
        gw = project->Subcatch[i].groundwater;
        if ( gw )
        {
            // --- identify node receiving groundwater flow
            j = gw->node;
            if ( j >= 0 )
            {
                // add groundwater flow to lateral inflow
                q = ( (1.0 - f)*(gw->oldFlow) + f*(gw->newFlow) )
                    * project->Subcatch[i].area;
                if ( fabs(q) < FLOW_TOL ) continue;
                project->Node[j].newLatFlow += q;
                massbal_addInflowFlow(project, GROUNDWATER_INFLOW, q);

                // add pollutant load (for positive inflow)
                if ( q > 0.0 )
                {
                    for (p = 0; p < project->Nobjects[POLLUT]; p++)
                    {
                        w = q * project->Pollut[p].gwConcen;
                        project->Node[j].newQual[p] += w;
                        massbal_addInflowQual(project, GROUNDWATER_INFLOW, p, w);
                    }
                }
            }
        }
    }
}

//=============================================================================

////  New function added to release 5.1.008.  ////                             //(5.1.008)

void addLidDrainInflows(Project *project, double routingTime)
//
//  Input:   routingTime = elasped time (millisec)
//  Output:  none
//  Purpose: adds inflows to nodes receiving LID drain flow.
//
{
    int j;
    double f;

    // for each subcatchment
    if ( project->Nobjects[SUBCATCH] == 0 ) return;
    f = (routingTime - project->OldRunoffTime) / (project->NewRunoffTime - project->OldRunoffTime);
    if ( f < 0.0 ) f = 0.0;
    if ( f > 1.0 ) f = 1.0;
    for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
    {
        if ( project->Subcatch[j].area > 0.0 && project->Subcatch[j].lidArea > 0.0 )
            lid_addDrainInflow(project, j, f);
    }
}

//=============================================================================

void addRdiiInflows(Project *project, DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds RDII inflows to nodes at current date.
//
{
    int    i, j, p;
    double q, w;
    int    numRdiiNodes;

    // --- see if any nodes have RDII at current date
    numRdiiNodes = rdii_getNumRdiiFlows(project, currentDate);

    // --- add RDII flow to each node's lateral inflow
    for (i=0; i<numRdiiNodes; i++)
    {
        rdii_getRdiiFlow(project, i, &j, &q);
        if ( j < 0 ) continue;
        if ( fabs(q) < FLOW_TOL ) continue;
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(project, RDII_INFLOW, q);

        // add pollutant load (for positive inflow)
        if ( q > 0.0 )
        {
            for (p = 0; p < project->Nobjects[POLLUT]; p++)
            {
                w = q * project->Pollut[p].rdiiConcen;
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(project, RDII_INFLOW, p, w);
            }
        }
    }
}

//=============================================================================

void addIfaceInflows(Project *project, DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds inflows from routing interface file to nodes at current date.
//
{
    int    i, j, p;
    double q, w;
    int    numIfaceNodes;

    // --- see if any nodes have interface inflows at current date
    if ( project->Finflows.mode != USE_FILE ) return;
    numIfaceNodes = iface_getNumIfaceNodes(project, currentDate);

    // --- add interface flow to each node's lateral inflow
    for (i=0; i<numIfaceNodes; i++)
    {
        j = iface_getIfaceNode(project, i);
        if ( j < 0 ) continue;
        q = iface_getIfaceFlow(project, i);
        if ( fabs(q) < FLOW_TOL ) continue;
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(project, EXTERNAL_INFLOW, q);

        // add pollutant load (for positive inflow)
        if ( q > 0.0 )
        {
            for (p = 0; p < project->Nobjects[POLLUT]; p++)
            {
                w = q * iface_getIfaceQual(project, i, p);
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(project, EXTERNAL_INFLOW, p, w);
            }
        }
    }
}

//=============================================================================

int  inflowHasChanged(Project *project)
//
//  Input:   none
//  Output:  returns TRUE if external inflows or outfall flows have changed
//           from the previous time step
//  Purpose: checks if the hydraulic state of the system has changed from
//           the previous time step.
//
{
    int    j;
    double diff, qOld, qNew;

    // --- check if external inflows or outfall flows have changed 
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        qOld = project->Node[j].oldLatFlow;
        qNew = project->Node[j].newLatFlow;
        if      ( fabs(qOld) > TINY ) diff = (qNew / qOld) - 1.0;
        else if ( fabs(qNew) > TINY ) diff = 1.0;
        else                    diff = 0.0;
        if ( fabs(diff) > project->LatFlowTol ) return TRUE;
        if ( project->Node[j].type == OUTFALL || project->Node[j].degree == 0 )
        {
            qOld = project->Node[j].oldFlowInflow;
            qNew = project->Node[j].inflow;
            if      ( fabs(qOld) > TINY ) diff = (qNew / qOld) - 1.0;
            else if ( fabs(qNew) > TINY ) diff = 1.0;
            else                          diff = 0.0;
            if ( fabs(diff) > project->LatFlowTol ) return TRUE;
        }
    }
    return FALSE;
}

//=============================================================================

////  This function was re-written for release 5.1.008.  ////                  //(5.1.008)

void removeStorageLosses(Project *project, double tStep)
//
//  Input:   tStep = routing time step (sec)
//  Output:  none
//  Purpose: adds flow rate lost from all storage nodes due to evaporation
//           & seepage in current time step to overall mass balance totals.
//
{
    int    i;
    double evapLoss = 0.0,
           exfilLoss = 0.0;

    // --- check each storage node
    for ( i = 0; i < project->Nobjects[NODE]; i++ )
    {
        if (project->Node[i].type == STORAGE)
        {
            // --- update total system storage losses
            evapLoss += project->Storage[project->Node[i].subIndex].evapLoss;
            exfilLoss += project->Storage[project->Node[i].subIndex].exfilLoss;
        }
    }

    // --- add loss rates (ft3/sec) to time step's mass balance 
    massbal_addNodeLosses(project, evapLoss/tStep, exfilLoss/tStep);
}

//=============================================================================

////  This function was modified for release 5.1.008.  ////                    //(5.1.008)

void removeConduitLosses(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: adds flow rate lost from all conduits due to evaporation
//           & seepage over current time step to overall mass balance.
//
{
    int i, k;
    double barrels,
           evapLoss = 0.0,
	   seepLoss = 0.0;

    for ( i = 0; i < project->Nobjects[LINK]; i++ )
    {
        if (project->Link[i].type == CONDUIT)
        {
            // --- retrieve number of barrels
            k = project->Link[i].subIndex;
            barrels =project-> Conduit[k].barrels;

            // --- update total conduit losses
            evapLoss +=project-> Conduit[k].evapLossRate * barrels;
            seepLoss +=project-> Conduit[k].seepLossRate * barrels;
        }
    }
    massbal_addLinkLosses(project, evapLoss, seepLoss);
}

//=============================================================================

////  This function was re-written for release 5.1.008.  ////                  //(5.1.008)

void removeOutflows(Project *project, double tStep)
//
//  Input:   none
//  Output:  none
//  Purpose: finds flows that leave the system and adds these to mass
//           balance totals.
//
{
    int    i, p, k;
    int    isFlooded;
    double q, w, v;

    for ( i = 0; i < project->Nobjects[NODE]; i++ )
    {
        // --- accumulate inflow volume & pollut. load at outfalls
        if ( project->Node[i].type == OUTFALL && project->Node[i].inflow > 0.0 )
        {
            k = project->Node[i].subIndex;
            if ( project->Outfall[k].routeTo >= 0 )
            {
                v = project->Node[i].inflow * tStep;
                project->Outfall[k].vRouted += v;
                for (p = 0; p < project->Nobjects[POLLUT]; p++)
                    project->Outfall[k].wRouted[p] += project->Node[i].newQual[p] * v;
            }
        }

        // --- update mass balance with flow and mass leaving the system
        //     through outfalls and flooded interior nodes
        q = node_getSystemOutflow(project, i, &isFlooded);
        if ( q != 0.0 )
        {
            massbal_addOutflowFlow(project, q, isFlooded);
            for ( p = 0; p < project->Nobjects[POLLUT]; p++ )
            {
                w = q * project->Node[i].newQual[p];
                massbal_addOutflowQual(project, p, w, isFlooded);
            }
        }

        // --- update mass balance with mass leaving system through negative
        //     lateral inflows (lateral flow was previously accounted for)
        q = project->Node[i].newLatFlow;
        if ( q < 0.0 )
        {
            for ( p = 0; p < project->Nobjects[POLLUT]; p++ )
            {
                w = -q * project->Node[i].newQual[p];
                massbal_addOutflowQual(project, p, w, FALSE);
            }
        }

    }
}

//=============================================================================

////  New function added for release 5.1.011.  ////                            //(5.1.011)

void sortEvents(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: sorts the entries of the project->Event array in chronological order.
//
{
    int i, j;
    TEvent temp;

    // Apply simple exchange sort to event list
    for (i = 0; i < project->NumEvents-1; i++)
    {
        for (j = i+1; j < project->NumEvents; j++)
        {
            if ( project->Event[i].start > project->Event[j].start )
            {
                temp = project->Event[j];
                project->Event[j] = project->Event[i];
                project->Event[i] = temp;
            }
        }
    }

    // Adjust for overlapping events
    for (i = 0; i < project->NumEvents-1; i++)
    {
        if ( project->Event[i].end > project->Event[i+1].start ) project->Event[i].end = project->Event[i+1].start;
    }
}

//=============================================================================


