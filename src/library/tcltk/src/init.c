/*CXXR $Id$
 *CXXR
 *CXXR This file is part of CXXR, a project to refactor the R interpreter
 *CXXR into C++.  It may consist in whole or in part of program code and
 *CXXR documentation taken from the R project itself, incorporated into
 *CXXR CXXR (and possibly MODIFIED) under the terms of the GNU General Public
 *CXXR Licence.
 *CXXR 
 *CXXR CXXR is Copyright (C) 2008 Andrew R. Runnalls, subject to such other
 *CXXR copyrights and copyright restrictions as may be stated below.
 *CXXR 
 *CXXR CXXR is not part of the R project, and bugs and other issues should
 *CXXR not be reported via r-bugs or other R project channels; instead refer
 *CXXR to the CXXR website.
 *CXXR */

/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2003   The R Development Core Team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

#include <R.h>
#include <Rinternals.h>
#include "tcltk.h"
#include <R_ext/Rdynload.h>

static const R_CMethodDef CEntries[] = {
    {"tcltk_init", (DL_FUNC) &tcltk_init, 0},
#ifdef Win32
    {"tcltk_start", (DL_FUNC) &tcltk_start, 0},
    {"tcltk_end", (DL_FUNC) &tcltk_end, 0},
#else
    {"delTcl", (DL_FUNC) &delTcl, 0},
#ifndef TCL80
    {"RTcl_ActivateConsole", (DL_FUNC) &RTcl_ActivateConsole, 0},
#endif
#endif
    {NULL, NULL, 0}
};

static const R_ExternalMethodDef ExternEntries[] = {
    {"dotTcl", (DL_FUNC) &dotTcl, -1},
    {"dotTclObjv", (DL_FUNC) &dotTclObjv, 1},
    {"dotTclcallback", (DL_FUNC) &dotTclcallback, -1},
    {"RTcl_ObjFromVar", (DL_FUNC) &RTcl_ObjFromVar, 1},
    {"RTcl_AssignObjToVar", (DL_FUNC) &RTcl_AssignObjToVar, 2},
    {"RTcl_StringFromObj", (DL_FUNC) &RTcl_StringFromObj, 1},
    {"RTcl_ObjAsCharVector", (DL_FUNC) &RTcl_ObjAsCharVector, 1},
    {"RTcl_ObjAsDoubleVector", (DL_FUNC) &RTcl_ObjAsDoubleVector, 1},
    {"RTcl_ObjAsIntVector", (DL_FUNC) &RTcl_ObjAsIntVector, 1},
    {"RTcl_ObjFromCharVector", (DL_FUNC) &RTcl_ObjFromCharVector, 2},
    {"RTcl_ObjFromDoubleVector", (DL_FUNC) &RTcl_ObjFromDoubleVector, 2},
    {"RTcl_ObjFromIntVector", (DL_FUNC) &RTcl_ObjFromIntVector, 2},
    {"RTcl_ServiceMode", (DL_FUNC) &RTcl_ServiceMode, 1},
#ifndef TCL80
    {"RTcl_GetArrayElem", (DL_FUNC) &RTcl_GetArrayElem, 2},
    {"RTcl_RemoveArrayElem", (DL_FUNC) &RTcl_RemoveArrayElem, 2},
    {"RTcl_SetArrayElem", (DL_FUNC) &RTcl_SetArrayElem, 3},
#endif
    {NULL, NULL, 0}
};


void R_init_tcltk(DllInfo *dll)
{
    R_registerRoutines(dll, CEntries, NULL, NULL, ExternEntries);
    R_useDynamicSymbols(dll, FALSE);
}

