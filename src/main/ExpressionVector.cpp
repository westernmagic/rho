/*CXXR $Id$
 *CXXR
 *CXXR This file is part of CXXR, a project to refactor the R interpreter
 *CXXR into C++.  It may consist in whole or in part of program code and
 *CXXR documentation taken from the R project itself, incorporated into
 *CXXR CXXR (and possibly MODIFIED) under the terms of the GNU General Public
 *CXXR Licence.
 *CXXR 
 *CXXR CXXR is Copyright (C) 2008-13 Andrew R. Runnalls, subject to such other
 *CXXR copyrights and copyright restrictions as may be stated below.
 *CXXR 
 *CXXR CXXR is not part of the R project, and bugs and other issues should
 *CXXR not be reported via r-bugs or other R project channels; instead refer
 *CXXR to the CXXR website.
 *CXXR */

/*
 *  R : A Computer Language for Statistical Data Analysis
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

/** @file ExpressionVector.cpp
 *
 * Implementation of class ExpressionVector and related functions.
 */

#include "CXXR/ExpressionVector.h"

using namespace CXXR;

// Force the creation of non-inline embodiments of functions callable
// from C:
namespace CXXR {
    namespace ForceNonInline {
	Rboolean (*isExpressionptr)(SEXP s) = Rf_isExpression;
	SEXP (*XVECTOR_ELTp)(const SEXP x, int i) = XVECTOR_ELT;
    }
}

// ***** C interface *****

SEXP SET_XVECTOR_ELT(SEXP x, int i, SEXP v)
{
    ExpressionVector* ev = SEXP_downcast<ExpressionVector*>(x, false);
    (*ev)[i] = v;
    return v;
}

// Needed for the instantiation in BOOST_CLASS_EXPORT_IMPLEMENT:
#include "CXXR/PairList.h"

BOOST_CLASS_EXPORT_IMPLEMENT(CXXR::ExpressionVector)
