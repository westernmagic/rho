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
 *  Copyright (C) 2001-5 The R Development Core Team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

/* Definitions for the X11 module.  Not intended for end-user use */

#ifndef R_X11_MODULE_H
#define R_X11_MODULE_H

#include <Rinternals.h>
#include <Rconnections.h>

typedef SEXP (*R_do_X11)(SEXP call, SEXP op, SEXP args, SEXP rho);
typedef SEXP (*R_X11DataEntryRoutine)(SEXP call, SEXP op, SEXP args, SEXP rho);
typedef SEXP (*R_X11DataViewer)(SEXP call, SEXP op, SEXP args, SEXP rho);
typedef Rboolean (*R_GetX11ImageRoutine)(int d, void *pximage, 
					 int *pwidth, int *pheight);
typedef int (*R_X11_access)();

typedef Rboolean (*R_X11clp)(Rclpconn, char*);


typedef struct {
    R_do_X11 X11;
    R_X11DataEntryRoutine de;
    R_GetX11ImageRoutine  image;
    R_X11_access access;
    R_X11clp readclp;
    R_X11DataViewer dv;
} R_X11Routines;

#ifdef __cplusplus
extern "C" {
#endif
R_X11Routines *R_setX11Routines(R_X11Routines *routines);
#ifdef __cplusplus
}
#endif

#endif /* R_X11_MODULE_H */
