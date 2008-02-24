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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street Fifth Floor, Boston, MA 02110-1301  USA
 */

/** @file Symbol.cpp
 *
 * At present, this file simply forces the generation of non-inlined
 * versions of inlined functions declared in RSymbol.h where these are
 * intended to be callable from C.  It is also used to check that
 * Symbol.h is self-contained, i.e. \#includes anything it needs, and
 * doesn't rely on anything having been previously \#included in the
 * enclosing source file.
 */

#include "CXXR/Symbol.h"

namespace CXXR {
    namespace ForceNonInline {
	Rboolean (*DDVALp)(SEXP x) = DDVAL;
	SEXP (*INTERNALp)(SEXP x) = INTERNAL;
	Rboolean (*isSymbolptr)(SEXP s) = Rf_isSymbol;
	SEXP (*PRINTNAMEp)(SEXP x) = PRINTNAME;
	void (*SET_DDVALp)(SEXP x, int v) = SET_DDVAL;
	SEXP (*SYMVALUEp)(SEXP x) = SYMVALUE;
    }
}
