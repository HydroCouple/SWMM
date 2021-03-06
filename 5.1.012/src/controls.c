
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
//   controls.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/21/14 (Build 5.1.001)
//             03/19/15 (Build 5.1.008)
//             04/30/15 (Build 5.1.009)
//             08/05/15 (Build 5.1.010)
//             08/01/16 (Build 5.1.011)
//   Author:   L. Rossman
//
//   Rule-based controls functions.
//
//   Control rules have the format:
//     RULE name
//     IF <premise>
//     AND / OR <premise>
//     etc.
//     THEN <action>
//     AND  <action>
//     etc.
//     ELSE <action>
//     AND  <action>
//     etc.
//     PRIORITY <p>
//
//   <premise> consists of:
//      <variable> <relational operator> value / <variable>
//   where <variable> is <object type> <id name> <attribute>
//   E.g.: Node 123 Depth > 4.5
//         Node 456 Depth < Node 123 Depth
//
//   <action> consists of:
//      <variable> = setting
//   E.g.: Pump abc status = OFF
//         Weir xyz setting = 0.5
//
//  Build 5.1.008:
//  - Support added for r.h.s. variables in rule premises.
//  - Node volume added as a premise variable.
//
//  Build 5.1.009:
//  - Fixed problem with parsing a RHS premise variable.
//
//  Build 5.1.010:
//  - Support added for link TIMEOPEN & TIMECLOSED premises.
//
//  Build 5.1.011:
//  - Support added for DAYOFYEAR attribute.
//  - Modulated controls no longer included in reported control actions.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <string.h>

#ifdef _WIN32
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include <math.h>
#include "headers.h"


//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum RuleState    {r_RULE, r_IF, r_AND, r_OR, r_THEN, r_ELSE, r_PRIORITY,
		   r_ERROR};
enum RuleObject   {r_NODE, r_LINK, r_CONDUIT, r_PUMP, r_ORIFICE, r_WEIR,
		   r_OUTLET, r_SIMULATION};
enum RuleAttrib   {r_DEPTH, r_HEAD, r_VOLUME, r_INFLOW, r_FLOW, r_STATUS,      //(5.1.008)
                   r_SETTING, r_TIMEOPEN, r_TIMECLOSED, r_TIME, r_DATE,        //(5.1.010)
                   r_CLOCKTIME, r_DAYOFYEAR, r_DAY, r_MONTH};                  //(5.1.011)
enum RuleRelation {EQ, NE, LT, LE, GT, GE};
enum RuleSetting  {r_CURVE, r_TIMESERIES, r_PID, r_NUMERIC};

static char* ObjectWords[] =
{"NODE", "LINK", "CONDUIT", "PUMP", "ORIFICE", "WEIR", "OUTLET",
 "SIMULATION", NULL};
static char* AttribWords[] =
{"DEPTH", "HEAD", "VOLUME", "INFLOW", "FLOW", "STATUS", "SETTING",         //(5.1.008)
 "TIMEOPEN", "TIMECLOSED","TIME", "DATE", "CLOCKTIME", "DAYOFYEAR",        //(5.1.011)
 "DAY", "MONTH", NULL};                                                    //(5.1.011)
static char* RelOpWords[] = {"=", "<>", "<", "<=", ">", ">=", NULL};
static char* StatusWords[]  = {"OFF", "ON", NULL};
static char* ConduitWords[] = {"CLOSED", "OPEN", NULL};
static char* SettingTypeWords[] = {"CURVE", "TIMESERIES", "PID", NULL};


//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//     controls_create
//     controls_delete
//     controls_addRuleClause
//     controls_evaluate

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
int    addPremise(Project *project, int r, int type, char* Tok[], int nToks);
int    getPremiseVariable(Project *project, char* tok[], int* k, TVariable* v);
int    getPremiseValue(char* token, int attrib, double* value);
int    addAction(Project *project, int r, char* Tok[], int nToks);

int    evaluatePremise(Project *project, TPremise* p, double tStep);
double getVariableValue(Project *project, TVariable v);
int    compareTimes(Project *project, double lhsValue, int relation, double rhsValue,
                    double halfStep);
int    compareValues(Project *project, double lhsValue, int relation, double rhsValue);

void   updateActionList(Project *project, TAction* a);
int    executeActionList(Project *project, DateTime currentTime);
void   clearActionList(Project *project);
void   deleteActionList(Project *project);
void   deleteRules(Project *project);

int    findExactMatch(char *s, char *keyword[]);
int    setActionSetting(Project *project, char* tok[], int nToks, int* curve, int* tseries,
                        int* attrib, double* value);
void   updateActionValue(Project *project, TAction* a, DateTime currentTime, double dt);
double getPIDSetting(Project *project, TAction* a, double dt);

//=============================================================================

int  controls_create(Project *project, int n)
//
//  Input:   n = total number of control rules
//  Output:  returns error code
//  Purpose: creates an array of control rules.
//
{
  int r;
  project->ActionList = NULL;
  project->InputState = r_PRIORITY;
  project->RuleCount = n;
  if ( n == 0 ) return 0;
  project->Rules = (TRule *) calloc(project->RuleCount, sizeof(TRule));
  if (project->Rules == NULL) return ERR_MEMORY;
  for ( r=0; r<project->RuleCount; r++ )
  {
    project->Rules[r].ID = NULL;
    project->Rules[r].firstPremise = NULL;
    project->Rules[r].lastPremise = NULL;
    project->Rules[r].thenActions = NULL;
    project->Rules[r].elseActions = NULL;
    project->Rules[r].priority = 0.0;
  }
  return 0;
}

//=============================================================================

void controls_delete(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: deletes all control rules.
//
{
  if ( project->RuleCount == 0 ) return;
  deleteActionList(project);
  deleteRules(project);
}

//=============================================================================

int  controls_addRuleClause(Project *project, int r, int keyword, char* tok[], int nToks)
//
//  Input:   r = rule index
//           keyword = the clause's keyword code (IF, THEN, etc.)
//           tok = an array of string tokens that comprises the clause
//           nToks = number of tokens
//  Output:  returns an error  code
//  Purpose: addd a new clause to a control rule.
//
{
  switch (keyword)
  {
    case r_RULE:
      if ( project->Rules[r].ID == NULL )
        project->Rules[r].ID = project_findID(project, CONTROL, tok[1]);
      project->InputState = r_RULE;
      if ( nToks > 2 ) return ERR_RULE;
      return 0;

    case r_IF:
      if ( project->InputState != r_RULE ) return ERR_RULE;
      project->InputState = r_IF;
      return addPremise(project, r, r_AND, tok, nToks);

    case r_AND:
      if ( project->InputState == r_IF ) return addPremise(project, r, r_AND, tok, nToks);
      else if ( project->InputState == r_THEN || project->InputState == r_ELSE )
        return addAction(project, r, tok, nToks);
      else return ERR_RULE;

    case r_OR:
      if ( project->InputState != r_IF ) return ERR_RULE;
      return addPremise(project, r, r_OR, tok, nToks);

    case r_THEN:
      if ( project->InputState != r_IF ) return ERR_RULE;
      project->InputState = r_THEN;
      return addAction(project, r, tok, nToks);

    case r_ELSE:
      if ( project->InputState != r_THEN ) return ERR_RULE;
      project->InputState = r_ELSE;
      return addAction(project, r, tok, nToks);

    case r_PRIORITY:
      if ( project->InputState != r_THEN && project->InputState != r_ELSE ) return ERR_RULE;
      project->InputState = r_PRIORITY;
      if ( !getDouble(tok[1], &project->Rules[r].priority) ) return ERR_NUMBER;
      if ( nToks > 2 ) return ERR_RULE;
      return 0;
  }
  return 0;
}

//=============================================================================

int controls_evaluate(Project *project, DateTime currentTime, DateTime elapsedTime, double tStep)
//
//  Input:   currentTime = current simulation date/time
//           elapsedTime = decimal days since start of simulation
//           tStep = simulation time step (days)
//  Output:  returns number of new actions taken
//  Purpose: evaluates all control rules at current time of the simulation.
//
{
  int    r;                          // control rule index
  int    result;                     // TRUE if rule premises satisfied
  TPremise* p;                // pointer to rule premise clause
  TAction*  a;                // pointer to rule action clause

  // --- save date and time to shared variables
  project->CurrentDate = floor(currentTime);
  project->CurrentTime = currentTime - floor(currentTime);
  project->ElapsedTime = elapsedTime;

  // --- evaluate each rule
  if ( project->RuleCount == 0 ) return 0;
  clearActionList(project);
  for (r=0; r<project->RuleCount; r++)
  {
    // --- evaluate rule's premises
    result = TRUE;
    p = project->Rules[r].firstPremise;
    while (p)
    {
      if ( p->type == r_OR )
      {
        if ( result == FALSE )
          result = evaluatePremise(project, p, tStep);
      }
      else
      {
        if ( result == FALSE ) break;
        result = evaluatePremise(project, p, tStep);
      }
      p = p->next;
    }

    // --- if premises true, add THEN clauses to action list
    //     else add ELSE clauses to action list
    if ( result == TRUE ) a = project->Rules[r].thenActions;
    else                  a = project->Rules[r].elseActions;
    while (a)
    {
      updateActionValue(project, a, currentTime, tStep);
      updateActionList(project, a);
      a = a->next;
    }
  }

  // --- execute actions on action list
  if ( project->ActionList ) return executeActionList(project, currentTime);
  else return 0;
}

//=============================================================================

//  This function was revised to add support for r.h.s. premise variables. //  //(5.1.008)

int  addPremise(Project *project, int r, int type, char* tok[], int nToks)
//
//  Input:   r = control rule index
//           type = type of premise (IF, AND, OR)
//           tok = array of string tokens containing premise statement
//           nToks = number of string tokens
//  Output:  returns an error code
//  Purpose: adds a new premise to a control rule.
//
{
  int    relation, n, err = 0;
  double value = MISSING;
  TPremise* p;
  TVariable v1;
  TVariable v2;

  // --- check for minimum number of tokens
  if ( nToks < 5 ) return ERR_ITEMS;

  // --- get LHS variable
  n = 1;
  err = getPremiseVariable(project,  tok, &n, &v1);
  if ( err > 0 ) return err;

  // --- get relational operator
  n++;
  relation = findExactMatch(tok[n], RelOpWords);
  if ( relation < 0 ) return error_setInpError(ERR_KEYWORD, tok[n]);
  n++;

  // --- initialize RHS variable
  v2.attribute = -1;
  v2.link = -1;
  v2.node = -1;

  // --- check that more tokens remain
  if ( n >= nToks ) return error_setInpError(ERR_ITEMS, "");

  // --- see if a RHS variable is supplied
  if ( findmatch(tok[n], ObjectWords) >= 0 && n + 3 >= nToks )
  {
    err = getPremiseVariable(project, tok, &n, &v2);
    if ( err > 0 ) return ERR_RULE;                                        //(5.1.009)
    if ( v1.attribute != v2.attribute)                                     //(5.1.009)
      report_writeWarningMsg(project, WARN11, project->Rules[r].ID);                       //(5.1.009)
  }

  // --- otherwise get value to which LHS variable is compared to
  else
  {
    err = getPremiseValue(tok[n], v1.attribute, &value);
    n++;
  }
  if ( err > 0 ) return err;

  // --- make sure another clause is not on same line
  if ( n < nToks && findmatch(tok[n], RuleKeyWords) >= 0 ) return ERR_RULE;

  // --- create the premise object
  p = (TPremise *) malloc(sizeof(TPremise));
  if ( !p ) return ERR_MEMORY;
  p->type      = type;
  p->lhsVar    = v1;
  p->rhsVar    = v2;
  p->relation  = relation;
  p->value     = value;
  p->next      = NULL;
  if ( project->Rules[r].firstPremise == NULL )
  {
    project->Rules[r].firstPremise = p;
  }
  else
  {
    project->Rules[r].lastPremise->next = p;
  }
  project->Rules[r].lastPremise = p;
  return 0;
}

//=============================================================================

int getPremiseVariable(Project *project, char* tok[], int* k, TVariable* v)
//
//  Input:   tok = array of string tokens containing premise statement
//           k = index of current token
//  Output:  returns an error code; updates k to new current token and
//           places identity of specified variable in v
//  Purpose: parses a variable (e.g., Node 123 Depth) specified in a
//           premise clause of a control rule.
//
{
  int    n = *k;
  int    node = -1;
  int    link = -1;
  int    obj, attrib;

  // --- get object type
  obj = findmatch(tok[n], ObjectWords);
  if ( obj < 0 ) return error_setInpError(ERR_KEYWORD, tok[n]);

  // --- get object index from its name
  n++;
  switch (obj)
  {
    case r_NODE:
      node = project_findObject(project, NODE, tok[n]);
      if ( node < 0 ) return error_setInpError(ERR_NAME, tok[n]);
      break;

    case r_LINK:
    case r_CONDUIT:
    case r_PUMP:
    case r_ORIFICE:
    case r_WEIR:
    case r_OUTLET:
      link = project_findObject(project, LINK, tok[n]);
      if ( link < 0 ) return error_setInpError(ERR_NAME, tok[n]);
      break;
    default: n--;
  }
  n++;

  // --- get attribute index from its name
  attrib = findmatch(tok[n], AttribWords);
  if ( attrib < 0 ) return error_setInpError(ERR_KEYWORD, tok[n]);

  // --- check that attribute belongs to object type
  if ( obj == r_NODE ) switch (attrib)
  {
    case r_DEPTH:
    case r_HEAD:
    case r_VOLUME:                                                           //(5.1.008)
    case r_INFLOW: break;
    default: return error_setInpError(ERR_KEYWORD, tok[n]);
  }

  ////  Added to release 5.1.010.  ////                                          //(5.1.010)
  // --- check for link TIMEOPEN & TIMECLOSED attributes
  else if ( link >= 0  &&
            ( (attrib == r_TIMEOPEN ||
               attrib == r_TIMECLOSED)
              ))
  {

  }
  ////

  else if ( obj == r_LINK || obj == r_CONDUIT ) switch (attrib)
  {
    case r_STATUS:
    case r_DEPTH:
    case r_FLOW: break;
    default: return error_setInpError(ERR_KEYWORD, tok[n]);
  }
  else if ( obj == r_PUMP ) switch (attrib)
  {
    case r_FLOW:
    case r_STATUS: break;
    default: return error_setInpError(ERR_KEYWORD, tok[n]);
  }
  else if ( obj == r_ORIFICE || obj == r_WEIR ||
            obj == r_OUTLET ) switch (attrib)
  {
    case r_SETTING: break;
    default: return error_setInpError(ERR_KEYWORD, tok[n]);
  }
  else switch (attrib)
  {
    case r_TIME:
    case r_DATE:
    case r_CLOCKTIME:
    case r_DAY:
    case r_MONTH:
    case r_DAYOFYEAR: break;                                                 //(5.1.011)
    default: return error_setInpError(ERR_KEYWORD, tok[n]);
  }

  // --- populate variable structure
  v->node      = node;
  v->link      = link;
  v->attribute = attrib;
  *k = n;
  return 0;
}

//=============================================================================

int getPremiseValue(char* token, int attrib, double* value)
//
//  Input:   token = a string token
//           attrib = index of a node/link attribute
//  Output:  value = attribute value;
//           returns an error code;
//  Purpose: parses the numerical value of a particular node/link attribute
//           in the premise clause of a control rule.
//
{
  char   strDate[25];                                                        //(5.1.011)
  switch (attrib)
  {
    case r_STATUS:
      *value = findmatch(token, StatusWords);
      if ( *value < 0.0 ) *value = findmatch(token, ConduitWords);
      if ( *value < 0.0 ) return error_setInpError(ERR_KEYWORD, token);
      break;

    case r_TIME:
    case r_CLOCKTIME:
    case r_TIMEOPEN:                                                         //(5.1.010)
    case r_TIMECLOSED:                                                       //(5.1.010)
      if ( !datetime_strToTime(token, value) )
        return error_setInpError(ERR_DATETIME, token);
      break;

    case r_DATE:
      if ( !datetime_strToDate(token, value) )
        return error_setInpError(ERR_DATETIME, token);
      break;

    case r_DAY:
      if ( !getDouble(token, value) )
        return error_setInpError(ERR_NUMBER, token);
      if ( *value < 1.0 || *value > 7.0 )
        return error_setInpError(ERR_DATETIME, token);
      break;

    case r_MONTH:
      if ( !getDouble(token, value) )
        return error_setInpError(ERR_NUMBER, token);
      if ( *value < 1.0 || *value > 12.0 )
        return error_setInpError(ERR_DATETIME, token);
      break;

      ////  This code block added to release 5.1.011.  ////                          //(5.1.011)
    case r_DAYOFYEAR:
      strncpy(strDate, token, 6);
      strcat(strDate, "/1947");
      if ( datetime_strToDate(strDate, value) )
      {
        *value = datetime_dayOfYear(*value);
      }
      else if ( !getDouble(token, value) || *value < 1 || *value > 365 )
        return error_setInpError(ERR_DATETIME, token);
      break;
      ////////////////////////////////////////////////////

    default: if ( !getDouble(token, value) )
        return error_setInpError(ERR_NUMBER, token);
  }
  return 0;
}

//=============================================================================

int  addAction(Project *project, int r, char* tok[], int nToks)
//
//  Input:   r = control rule index
//           tok = array of string tokens containing action statement
//           nToks = number of string tokens
//  Output:  returns an error code
//  Purpose: adds a new action to a control rule.
//
{
  int    obj, link, attrib;
  int    curve = -1, tseries = -1;
  int    n;
  int    err;
  double values[] = {1.0, 0.0, 0.0};

  TAction* a;

  // --- check for proper number of tokens
  if ( nToks < 6 ) return error_setInpError(ERR_ITEMS, "");

  // --- check for valid object type
  obj = findmatch(tok[1], ObjectWords);
  if ( obj != r_LINK && obj != r_CONDUIT && obj != r_PUMP &&
       obj != r_ORIFICE && obj != r_WEIR && obj != r_OUTLET )
    return error_setInpError(ERR_KEYWORD, tok[1]);

  // --- check that object name exists and is of correct type
  link = project_findObject(project, LINK, tok[2]);
  if ( link < 0 ) return error_setInpError(ERR_NAME, tok[2]);
  switch (obj)
  {
    case r_CONDUIT:
      if ( project->Link[link].type != CONDUIT )
        return error_setInpError(ERR_NAME, tok[2]);
      break;
    case r_PUMP:
      if ( project->Link[link].type != PUMP )
        return error_setInpError(ERR_NAME, tok[2]);
      break;
    case r_ORIFICE:
      if ( project->Link[link].type != ORIFICE )
        return error_setInpError(ERR_NAME, tok[2]);
      break;
    case r_WEIR:
      if ( project->Link[link].type != WEIR )
        return error_setInpError(ERR_NAME, tok[2]);
      break;
    case r_OUTLET:
      if ( project->Link[link].type != OUTLET )
        return error_setInpError(ERR_NAME, tok[2]);
      break;
  }

  // --- check for valid attribute name
  attrib = findmatch(tok[3], AttribWords);
  if ( attrib < 0 ) return error_setInpError(ERR_KEYWORD, tok[3]);

  // --- get control action setting
  if ( obj == r_CONDUIT )
  {
    if ( attrib == r_STATUS )
    {
      values[0] = findmatch(tok[5], ConduitWords);
      if ( values[0] < 0.0 )
        return error_setInpError(ERR_KEYWORD, tok[5]);
    }
    else return error_setInpError(ERR_KEYWORD, tok[3]);
  }

  else if ( obj == r_PUMP )
  {
    if ( attrib == r_STATUS )
    {
      values[0] = findmatch(tok[5], StatusWords);
      if ( values[0] < 0.0 )
        return error_setInpError(ERR_KEYWORD, tok[5]);
    }
    else if ( attrib == r_SETTING )
    {
      err = setActionSetting(project, tok, nToks, &curve, &tseries,
                             &attrib, values);
      if ( err > 0 ) return err;
    }
    else return error_setInpError(ERR_KEYWORD, tok[3]);
  }

  else if ( obj == r_ORIFICE || obj == r_WEIR || obj == r_OUTLET )
  {
    if ( attrib == r_SETTING )
    {
      err = setActionSetting(project, tok, nToks, &curve, &tseries,
                             &attrib, values);
      if ( err > 0 ) return err;
      if (  attrib == r_SETTING
            && (values[0] < 0.0 || values[0] > 1.0) )
        return error_setInpError(ERR_NUMBER, tok[5]);
    }
    else return error_setInpError(ERR_KEYWORD, tok[3]);
  }
  else return error_setInpError(ERR_KEYWORD, tok[1]);

  // --- check if another clause is on same line
  n = 6;
  if ( curve >= 0 || tseries >= 0 ) n = 7;
  if ( attrib == r_PID ) n = 9;
  if ( n < nToks && findmatch(tok[n], RuleKeyWords) >= 0 ) return ERR_RULE;

  // --- create the action object
  a = (TAction *) malloc(sizeof(TAction));
  if ( !a ) return ERR_MEMORY;
  a->rule      = r;
  a->link      = link;
  a->attribute = attrib;
  a->curve     = curve;
  a->tseries   = tseries;
  a->value     = values[0];
  if ( attrib == r_PID )
  {
    a->kp = values[0];
    a->ki = values[1];
    a->kd = values[2];
    a->e1 = 0.0;
    a->e2 = 0.0;
  }
  if ( project->InputState == r_THEN )
  {
    a->next = project->Rules[r].thenActions;
    project->Rules[r].thenActions = a;
  }
  else
  {
    a->next = project->Rules[r].elseActions;
    project->Rules[r].elseActions = a;
  }
  return 0;
}

//=============================================================================

int  setActionSetting(Project *project, char* tok[], int nToks, int* curve, int* tseries,
                      int* attrib, double values[])
//
//  Input:   tok = array of string tokens containing action statement
//           nToks = number of string tokens
//  Output:  curve = index of controller curve
//           tseries = index of controller time series
//           attrib = r_PID if PID controller used
//           values = values of control settings
//           returns an error code
//  Purpose: identifies how control actions settings are determined.
//
{
  int k, m;

  // --- see if control action is determined by a Curve or Time Series
  if (nToks < 6) return error_setInpError(ERR_ITEMS, "");
  k = findmatch(tok[5], SettingTypeWords);
  if ( k >= 0 && nToks < 7 ) return error_setInpError(ERR_ITEMS, "");
  switch (k)
  {

    // --- control determined by a curve - find curve index
    case r_CURVE:
      m = project_findObject(project, CURVE, tok[6]);
      if ( m < 0 ) return error_setInpError(ERR_NAME, tok[6]);
      *curve = m;
      break;

      // --- control determined by a time series - find time series index
    case r_TIMESERIES:
      m = project_findObject(project, TSERIES, tok[6]);
      if ( m < 0 ) return error_setInpError(ERR_NAME, tok[6]);
      *tseries = m;
      project->Tseries[m].refersTo = CONTROL;
      break;

      // --- control determined by PID controller
    case r_PID:
      if (nToks < 9) return error_setInpError(ERR_ITEMS, "");
      for (m=6; m<=8; m++)
      {
        if ( !getDouble(tok[m], &values[m-6]) )
          return error_setInpError(ERR_NUMBER, tok[m]);
      }
      *attrib = r_PID;
      break;

      // --- direct numerical control is used
    default:
      if ( !getDouble(tok[5], &values[0]) )
        return error_setInpError(ERR_NUMBER, tok[5]);
  }
  return 0;
}

//=============================================================================

void  updateActionValue(Project *project, TAction* a, DateTime currentTime, double dt)
//
//  Input:   a = an action object
//           currentTime = current simulation date/time (days)
//           dt = time step (days)
//  Output:  none
//  Purpose: updates value of actions found from Curves or Time Series.
//
{
  if ( a->curve >= 0 )
  {
    a->value = table_lookup(&project->Curve[a->curve], project->ControlValue);
  }
  else if ( a->tseries >= 0 )
  {
    a->value = table_tseriesLookup(&project->Tseries[a->tseries], currentTime, TRUE);
  }
  else if ( a->attribute == r_PID )
  {
    a->value = getPIDSetting(project, a, dt);
  }
}

//=============================================================================

double getPIDSetting(Project *project, TAction* a, double dt)
//
//  Input:   a = an action object
//           dt = current time step (days)
//  Output:  returns a new link setting 
//  Purpose: computes a new setting for a link subject to a PID controller.
//
//  Note:    a->kp = gain coefficient,
//           a->ki = integral time (minutes)
//           a->k2 = derivative time (minutes)
//           a->e1 = error from previous time step
//           a->e2 = error from two time steps ago
{
  double e0, setting;
  double p, i, d, update;
  double tolerance = 0.0001;

  // --- convert time step from days to minutes
  dt *= 1440.0;

  // --- determine relative error in achieving controller set point
  e0 = project->SetPoint - project->ControlValue;
  if ( fabs(e0) > TINY )
  {
    if ( project->SetPoint != 0.0 ) e0 = e0/project->SetPoint;
    else                   e0 = e0/project->ControlValue;
  }

  // --- reset previous errors to 0 if controller gets stuck
  if (fabs(e0 - a->e1) < tolerance)
  {
    a->e2 = 0.0;
    a->e1 = 0.0;
  }

  // --- use the recursive form of the PID controller equation to
  //     determine the new setting for the controlled link
  p = (e0 - a->e1);
  if ( a->ki == 0.0 ) i = 0.0;
  else i = e0 * dt / a->ki;
  d = a->kd * (e0 - 2.0*a->e1 + a->e2) / dt;
  update = a->kp * (p + i + d);
  if ( fabs(update) < tolerance ) update = 0.0;
  setting = project->Link[a->link].targetSetting + update;

  // --- update previous errors
  a->e2 = a->e1;
  a->e1 = e0;

  // --- check that new setting lies within feasible limits
  if ( setting < 0.0 ) setting = 0.0;
  if (project->Link[a->link].type != PUMP && setting > 1.0 ) setting = 1.0;
  return setting;
}

//=============================================================================

void updateActionList(Project *project, TAction* a)
//
//  Input:   a = an action object
//  Output:  none
//  Purpose: adds a new action to the list of actions to be taken.
//
{
  TActionList* listItem;
  TAction* a1;
  double priority = project->Rules[a->rule].priority;

  // --- check if link referred to in action is already listed
  listItem = project->ActionList;
  while ( listItem )
  {
    a1 = listItem->action;
    if ( !a1 ) break;
    if ( a1->link == a->link )
    {
      // --- replace old action if new action has higher priority
      if ( priority > project->Rules[a1->rule].priority ) listItem->action = a;
      return;
    }
    listItem = listItem->next;
  }

  // --- action not listed so add it to ActionList
  if ( !listItem )
  {
    listItem = (TActionList *) malloc(sizeof(TActionList));
    listItem->next = project->ActionList;
    project->ActionList = listItem;
  }
  listItem->action = a;
}

//=============================================================================

int executeActionList(Project *project, DateTime currentTime)
//
//  Input:   currentTime = current date/time of the simulation
//  Output:  returns number of new actions taken
//  Purpose: executes all actions required by fired control rules.
//
{
  TActionList* listItem;
  TActionList* nextItem;
  TAction* a1;
  int count = 0;

  listItem = project->ActionList;
  while ( listItem )
  {
    a1 = listItem->action;
    if ( !a1 ) break;
    if ( a1->link >= 0 )
    {
      if ( project->Link[a1->link].targetSetting != a1->value )
      {
        project->Link[a1->link].targetSetting = a1->value;
        if ( project->RptFlags.controls && a1->curve < 0                        //(5.1.011)
             && a1->tseries < 0 && a1->attribute != r_PID )            //(5.1.011)
          report_writeControlAction(project, currentTime, project->Link[a1->link].ID,
              a1->value, project->Rules[a1->rule].ID);
        count++;
      }
    }
    nextItem = listItem->next;
    listItem = nextItem;
  }
  return count;
}

//=============================================================================

////  This function was re-written for release 5.1.011.  ////                  //(5.1.011)

int evaluatePremise(Project *project, TPremise* p, double tStep)
//
//  Input:   p = a control rule premise condition
//           tStep = current time step (days)
//  Output:  returns TRUE if the condition is true or FALSE otherwise
//  Purpose: evaluates the truth of a control rule premise condition.
//
{
  double lhsValue, rhsValue;
  int    result = FALSE;

  lhsValue = getVariableValue(project, p->lhsVar);
  if ( p->value == MISSING ) rhsValue = getVariableValue(project, p->rhsVar);
  else                       rhsValue = p->value;
  if ( lhsValue == MISSING || rhsValue == MISSING ) return FALSE;
  switch (p->lhsVar.attribute)
  {
    case r_TIME:
    case r_CLOCKTIME:
      return compareTimes(project, lhsValue, p->relation, rhsValue, tStep/2.0);
    case r_TIMEOPEN:
    case r_TIMECLOSED:
      result = compareTimes(project, lhsValue, p->relation, rhsValue, tStep/2.0);
      project->ControlValue = lhsValue * 24.0;  // convert time from days to hours
      return result;
    default:
      return compareValues(project, lhsValue, p->relation, rhsValue);
  }
}

//=============================================================================

double getVariableValue(Project *project, TVariable v)
{
  int i = v.node;
  int j = v.link;

  switch ( v.attribute )
  {
    case r_TIME:
      return project->ElapsedTime;

    case r_DATE:
      return project->CurrentDate;

    case r_CLOCKTIME:
      return project->CurrentTime;

    case r_DAY:
      return datetime_dayOfWeek(project->CurrentDate);

    case r_MONTH:
      return datetime_monthOfYear(project->CurrentDate);

    case r_DAYOFYEAR:                                                        //(5.1.011)
      return datetime_dayOfYear(project->CurrentDate);                                //(5.1.011)

    case r_STATUS:
      if ( j < 0 ||
           (project->Link[j].type != CONDUIT && project->Link[j].type != PUMP) ) return MISSING;
      else return project->Link[j].setting;

    case r_SETTING:
      if ( j < 0 || (project->Link[j].type != ORIFICE && project->Link[j].type != WEIR) )
        return MISSING;
      else return project->Link[j].setting;

    case r_FLOW:
      if ( j < 0 ) return MISSING;
      else return project->Link[j].direction*project->Link[j].newFlow*UCF(project, FLOW);

    case r_DEPTH:
      if ( j >= 0 ) return project->Link[j].newDepth*UCF(project, LENGTH);
      else if ( i >= 0 )
        return project->Node[i].newDepth*UCF(project, LENGTH);
      else return MISSING;

    case r_HEAD:
      if ( i < 0 ) return MISSING;
      return (project->Node[i].newDepth + project->Node[i].invertElev) * UCF(project, LENGTH);

    case r_VOLUME:                                                           //(5.1.008)
      if ( i < 0 ) return MISSING;
      return (project->Node[i].newVolume * UCF(project, VOLUME));

    case r_INFLOW:
      if ( i < 0 ) return MISSING;
      else return project->Node[i].newLatFlow*UCF(project, FLOW);

      ////  This section added to release 5.1.010.  ////                             //(5.1.010)
    case r_TIMEOPEN:
      if ( j < 0 ) return MISSING;
      if ( project->Link[j].setting <= 0.0 ) return MISSING;
      return project->CurrentDate + project->CurrentTime - project->Link[j].timeLastSet;

    case r_TIMECLOSED:
      if ( j < 0 ) return MISSING;
      if ( project->Link[j].setting > 0.0 ) return MISSING;
      return project->CurrentDate + project->CurrentTime - project->Link[j].timeLastSet;
      ////

    default: return MISSING;
  }
}

//=============================================================================

int compareTimes(Project *project, double lhsValue, int relation, double rhsValue, double halfStep)
//
//  Input:   lhsValue = date/time value on left hand side of relation
//           relation = relational operator code (see RuleRelation enumeration)
//           rhsValue = date/time value on right hand side of relation 
//           halfStep = 1/2 the current time step (days)
//  Output:  returns TRUE if time relation is satisfied
//  Purpose: evaluates the truth of a relation between two date/times.
//
{
  if ( relation == EQ )
  {
    if ( lhsValue >= rhsValue - halfStep
         &&   lhsValue < rhsValue + halfStep ) return TRUE;
    return FALSE;
  }
  else if ( relation == NE )
  {
    if ( lhsValue < rhsValue - halfStep
         ||   lhsValue >= rhsValue + halfStep ) return TRUE;
    return FALSE;
  }
  else return compareValues(project, lhsValue, relation, rhsValue);
}

//=============================================================================

int compareValues(Project *project, double lhsValue, int relation, double rhsValue)
//  Input:   lhsValue = value on left hand side of relation
//           relation = relational operator code (see RuleRelation enumeration)
//           rhsValue = value on right hand side of relation 
//  Output:  returns TRUE if relation is satisfied
//  Purpose: evaluates the truth of a relation between two values.
{
  project->SetPoint = rhsValue;
  project->ControlValue = lhsValue;
  switch (relation)
  {
    case EQ: if ( lhsValue == rhsValue ) return TRUE; break;
    case NE: if ( lhsValue != rhsValue ) return TRUE; break;
    case LT: if ( lhsValue <  rhsValue ) return TRUE; break;
    case LE: if ( lhsValue <= rhsValue ) return TRUE; break;
    case GT: if ( lhsValue >  rhsValue ) return TRUE; break;
    case GE: if ( lhsValue >= rhsValue ) return TRUE; break;
  }
  return FALSE;
}

//=============================================================================

void clearActionList(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: clears the list of actions to be executed.
//
{
  TActionList* listItem;
  listItem = project->ActionList;
  while ( listItem )
  {
    listItem->action = NULL;
    listItem = listItem->next;
  }
}

//=============================================================================

void  deleteActionList(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: frees the memory used to hold the list of actions to be executed.
//
{
  TActionList* listItem;
  TActionList* nextItem;
  listItem = project->ActionList;
  while ( listItem )
  {
    nextItem = listItem->next;
    free(listItem);
    listItem = nextItem;
  }
  project->ActionList = NULL;
}

//=============================================================================

void  deleteRules(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: frees the memory used for all of the control rules.
//
{
  TPremise* p;
  TPremise* pnext;
  TAction*  a;
  TAction*  anext;

  int r;
  for (r=0; r<project->RuleCount; r++)
  {
    p = project->Rules[r].firstPremise;
    while ( p )
    {
      pnext = p->next;
      free(p);
      p = pnext;
    }
    a = project->Rules[r].thenActions;
    while (a )
    {
      anext = a->next;
      free(a);
      a = anext;
    }
    a = project->Rules[r].elseActions;
    while (a )
    {
      anext = a->next;
      free(a);
      a = anext;
    }
  }
  FREE(project->Rules);
  project->RuleCount = 0;
}

//=============================================================================

int  findExactMatch(char *s, char *keyword[])
//
//  Input:   s = character string
//           keyword = array of keyword strings
//  Output:  returns index of keyword which matches s or -1 if no match found  
//  Purpose: finds exact match between string and array of keyword strings.
//
{
  int i = 0;
  while (keyword[i] != NULL)
  {
    if ( strcomp(s, keyword[i]) ) return(i);
    i++;
  }
  return(-1);
}

//=============================================================================
