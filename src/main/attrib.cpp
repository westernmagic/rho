/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1997--2015  The R Core Team
 *  Copyright (C) 2008-2014  Andrew R. Runnalls.
 *  Copyright (C) 2014 and onwards the Rho Project Authors.
 *
 *  Rho is not part of the R project, and bugs and other issues should
 *  not be reported via r-bugs or other R project channels; instead refer
 *  to the Rho website.
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
 *  https://www.R-project.org/Licenses/
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Defn.h>
#include <Internal.h>
#include <Rmath.h>
#include "basedecl.h"
#include "rho/ArgMatcher.hpp"
#include "rho/GCStackRoot.hpp"

using namespace rho;

#undef TRUE
#undef FALSE

static SEXP removeAttrib(SEXP, SEXP);

SEXP comment(SEXP);
static SEXP commentgets(SEXP, SEXP);

static SEXP row_names_gets(SEXP vec , SEXP val)
{
    SEXP ans;

    if (vec == R_NilValue)
	error(_("attempt to set an attribute on NULL"));

    if(isReal(val) && LENGTH(val) == 2 && ISNAN(REAL(val)[0]) ) {
	/* This should not happen, but if a careless user dput()s a
	   data frame and sources the result, it will */
	PROTECT(val = coerceVector(val, INTSXP));
	vec->setAttribute(static_cast<Symbol*>(R_RowNamesSymbol), val);
	ans = val;
	UNPROTECT(1);
	return ans;
    }
    if(isInteger(val)) {
	Rboolean OK_compact = TRUE;
	int i, n = LENGTH(val);
	if(n == 2 && INTEGER(val)[0] == NA_INTEGER) {
	    n = INTEGER(val)[1];
	} else if (n > 2) {
	    for(i = 0; i < n; i++)
		if(INTEGER(val)[i] != i+1) {
		    OK_compact = FALSE;
		    break;
		}
	} else OK_compact = FALSE;
	if(OK_compact) {
	    /* we hide the length in an impossible integer vector */
	    PROTECT(val = allocVector(INTSXP, 2));
	    INTEGER(val)[0] = NA_INTEGER;
	    INTEGER(val)[1] = n;
	    vec->setAttribute(static_cast<Symbol*>(R_RowNamesSymbol), val);
	    ans = val;
	    UNPROTECT(1);
	    return ans;
	}
    } else if(!isString(val))
	error(_("row names must be 'character' or 'integer', not '%s'"),
	      type2char(TYPEOF(val)));
    PROTECT(val);
    vec->setAttribute(static_cast<Symbol*>(R_RowNamesSymbol), val);
    ans =  val;
    UNPROTECT(1);
    return ans;
}

/* NOTE: For environments serialize.c calls this function to find if
   there is a class attribute in order to reconstruct the object bit
   if needed.  This means the function cannot use OBJECT(vec) == 0 to
   conclude that the class attribute is R_NilValue.  If you want to
   rewrite this function to use such a pre-test, be sure to adjust
   serialize.c accordingly.  LT */
SEXP attribute_hidden getAttrib0(SEXP vec, SEXP name)
{
    SEXP s;
    int len, i, any;

    if (!vec) return nullptr;
    if (name == R_NamesSymbol) {
	if(isVector(vec) || isList(vec) || isLanguage(vec)) {
	    s = getAttrib(vec, R_DimSymbol);
	    if(TYPEOF(s) == INTSXP && LENGTH(s) == 1) {
		s = getAttrib(vec, R_DimNamesSymbol);
		if(!isNull(s)) {
		    SET_NAMED(VECTOR_ELT(s, 0), 2);
		    return VECTOR_ELT(s, 0);
		}
	    }
	}
	if (isList(vec) || isLanguage(vec)) {
	    len = length(vec);
	    PROTECT(s = allocVector(STRSXP, len));
	    i = 0;
	    any = 0;
	    for ( ; vec != R_NilValue; vec = CDR(vec), i++) {
		if (TAG(vec) == R_NilValue)
		    SET_STRING_ELT(s, i, R_BlankString);
		else if (isSymbol(TAG(vec))) {
		    any = 1;
		    SET_STRING_ELT(s, i, PRINTNAME(TAG(vec)));
		}
		else
		    error(_("getAttrib: invalid type (%s) for TAG"),
			  type2char(TYPEOF(TAG(vec))));
	    }
	    UNPROTECT(1);
	    if (any) {
		if (!isNull(s)) SET_NAMED(s, 2);
		return (s);
	    }
	    return R_NilValue;
	}
    }
    /* This is where the old/new list adjustment happens. */
    RObject* att = vec->getAttribute(SEXP_downcast<Symbol*>(name));
    if (!att) return nullptr;
    if (name == R_DimNamesSymbol && TYPEOF(att) == LISTSXP)
	error("old list is no longer allowed for dimnames attribute\n");
    SET_NAMED(att, 2);
    return att;
}

SEXP getAttrib(SEXP vec, SEXP name)
{
    if (!vec) return nullptr;
    if(TYPEOF(vec) == CHARSXP)
	error("cannot have attributes on a CHARSXP");
    /* pre-test to avoid expensive operations if clearly not needed -- LT */
    if (!vec->hasAttributes() &&
	! (TYPEOF(vec) == LISTSXP || TYPEOF(vec) == LANGSXP))
	return R_NilValue;

    if (isString(name)) name = installTrChar(STRING_ELT(name, 0));

    /* special test for c(NA, n) rownames of data frames: */
    if (name == R_RowNamesSymbol) {
	SEXP s = getAttrib0(vec, R_RowNamesSymbol);
	if(isInteger(s) && LENGTH(s) == 2 && INTEGER(s)[0] == NA_INTEGER) {
	    int i, n = abs(INTEGER(s)[1]);
	    PROTECT(s = allocVector(INTSXP, n));
	    for(i = 0; i < n; i++)
		INTEGER(s)[i] = i+1;
	    UNPROTECT(1);
	}
	return s;
    } else
	return getAttrib0(vec, name);
}

attribute_hidden
SEXP do_shortRowNames(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* type_)
{
    /* return  n if the data frame 'vec' has c(NA, n) rownames;
     *	       nrow(.) otherwise;  note that data frames with nrow(.) == 0
     *		have no row.names.
     ==> is also used in dim.data.frame() */

    SEXP s = getAttrib0(x_, R_RowNamesSymbol), ans = s;
    int type = asInteger(type_);

    if( type < 0 || type > 2)
	error(_("invalid '%s' argument"), "type");

    if(type >= 1) {
	int n = (isInteger(s) && LENGTH(s) == 2 && INTEGER(s)[0] == NA_INTEGER)
	    ? INTEGER(s)[1] : (isNull(s) ? 0 : LENGTH(s));
	ans = ScalarInteger((type == 1) ? n : abs(n));
    }
    return ans;
}

/* This is allowed to change 'out' */
attribute_hidden
SEXP do_copyDFattr(/*const*/ Expression* call, const BuiltInFunction* op, RObject* xx_, RObject* x_)
{
    SEXP in = xx_, out = x_;
    SET_ATTRIB(out, shallow_duplicate(ATTRIB(in)));
    IS_S4_OBJECT(in) ?  SET_S4_OBJECT(out) : UNSET_S4_OBJECT(out);
    return out;
}


/* 'name' should be 1-element STRSXP or SYMSXP */
SEXP setAttrib(SEXP vec, SEXP name, SEXP val)
{
    PROTECT(vec);
    PROTECT(name);

    if (isString(name)) {
	name = installTrChar(STRING_ELT(name, 0));
    }
    if (val == R_NilValue) {
	UNPROTECT(2);
	return removeAttrib(vec, name);
    }

    /* We allow attempting to remove names from NULL */
    if (vec == R_NilValue)
	error(_("attempt to set an attribute on NULL"));

    if (MAYBE_REFERENCED(val)) val = R_FixupRHS(vec, val);
    //SET_NAMED(val, NAMED(val) | NAMED(vec));
    UNPROTECT(2);

    GCStackRoot<> valr(val);
    if (name == R_NamesSymbol)
	return namesgets(vec, val);
    else if (name == R_DimSymbol)
	return dimgets(vec, val);
    else if (name == R_DimNamesSymbol)
	return dimnamesgets(vec, val);
    else if (name == R_ClassSymbol)
	return classgets(vec, val);
    else if (name == R_TspSymbol)
	return tspgets(vec, val);
    else if (name == R_CommentSymbol)
	return commentgets(vec, val);
    else if (name == R_RowNamesSymbol)
	return row_names_gets(vec, val);
    else {
	vec->setAttribute(SEXP_downcast<Symbol*>(name), val);
	return val;
    }
}

/* This is called in the case of binary operations to copy */
/* most attributes from (one of) the input arguments to */
/* the output.	Note that the Dim and Names attributes */
/* should have been assigned elsewhere. */

void copyMostAttrib(SEXP inp, SEXP ans)
{
    SEXP s;

    if (ans == R_NilValue)
	error(_("attempt to set an attribute on NULL"));

    PROTECT(ans);
    PROTECT(inp);
    for (s = ATTRIB(inp); s != R_NilValue; s = CDR(s)) {
	if ((TAG(s) != R_NamesSymbol) &&
	    (TAG(s) != R_DimSymbol) &&
	    (TAG(s) != R_DimNamesSymbol)) {
	    ans->setAttribute(SEXP_downcast<Symbol*>(TAG(s)), CAR(s));
	}
    }
    IS_S4_OBJECT(inp) ?  SET_S4_OBJECT(ans) : UNSET_S4_OBJECT(ans);
    UNPROTECT(2);
}

/* version that does not preserve ts information, for subsetting */
void copyMostAttribNoTs(SEXP inp, SEXP ans)
{
    SEXP s;

    if (ans == R_NilValue)
	error(_("attempt to set an attribute on NULL"));

    PROTECT(ans);
    PROTECT(inp);
    for (s = ATTRIB(inp); s != R_NilValue; s = CDR(s)) {
	if ((TAG(s) != R_NamesSymbol) &&
	    (TAG(s) != R_ClassSymbol) &&
	    (TAG(s) != R_TspSymbol) &&
	    (TAG(s) != R_DimSymbol) &&
	    (TAG(s) != R_DimNamesSymbol)) {
	    ans->setAttribute(SEXP_downcast<Symbol*>(TAG(s)), CAR(s));
	} else if (TAG(s) == R_ClassSymbol) {
	    SEXP cl = CAR(s);
	    int i;
	    Rboolean ists = FALSE;
	    for (i = 0; i < LENGTH(cl); i++)
		if (strcmp(CHAR(STRING_ELT(cl, i)), "ts") == 0) { /* ASCII */
		    ists = TRUE;
		    break;
		}
	    if (!ists) ans->setAttribute(static_cast<Symbol*>(TAG(s)), cl);
	    else if(LENGTH(cl) <= 1) {
	    } else {
		SEXP new_cl;
		int i, j, l = LENGTH(cl);
		PROTECT(new_cl = allocVector(STRSXP, l - 1));
		for (i = 0, j = 0; i < l; i++)
		    if (strcmp(CHAR(STRING_ELT(cl, i)), "ts")) /* ASCII */
			SET_STRING_ELT(new_cl, j++, STRING_ELT(cl, i));
		ans->setAttribute(static_cast<Symbol*>(TAG(s)), new_cl);
		UNPROTECT(1);
	    }
	}
    }
    IS_S4_OBJECT(inp) ?  SET_S4_OBJECT(ans) : UNSET_S4_OBJECT(ans);
    UNPROTECT(2);
}

static SEXP removeAttrib(SEXP vec, SEXP name)
{
    SEXP t;
    if (!vec) return nullptr;  // 2007/07/24 arr
    if(TYPEOF(vec) == CHARSXP)
	error("cannot set attribute on a CHARSXP");
    if (name == R_NamesSymbol && isPairList(vec)) {
	for (t = vec; t != R_NilValue; t = CDR(t))
	    SET_TAG(t, R_NilValue);
	return R_NilValue;
    }
    else {
	if (name == R_DimSymbol)
	    vec->setAttribute(static_cast<Symbol*>(R_DimNamesSymbol), nullptr);
	vec->setAttribute(SEXP_downcast<Symbol*>(name), nullptr);
    }
    return R_NilValue;
}

static void checkNames(SEXP x, SEXP s)
{
    if (isVector(x) || isList(x) || isLanguage(x)) {
	if (!isVector(s) && !isList(s))
	    error(_("invalid type (%s) for 'names': must be vector"),
		  type2char(TYPEOF(s)));
	if (xlength(x) != xlength(s))
	    error(_("'names' attribute [%d] must be the same length as the vector [%d]"), length(s), length(x));
    }
    else if(IS_S4_OBJECT(x)) {
      /* leave validity checks to S4 code */
    }
    else error(_("names() applied to a non-vector"));
}


/* Time Series Parameters */

static void NORET badtsp(void)
{
    error(_("invalid time series parameters specified"));
}

attribute_hidden
SEXP tspgets(SEXP vec, SEXP val)
{
    double start, end, frequency;
    int n;

    if (vec == R_NilValue)
	error(_("attempt to set an attribute on NULL"));

    if(IS_S4_OBJECT(vec)) { /* leave validity checking to validObject */
	if (!isNumeric(val)) /* but should have been checked */
	    error(_("'tsp' attribute must be numeric"));
	vec->setAttribute(static_cast<Symbol*>(R_TspSymbol), val);
	return vec;
    }

    if (!isNumeric(val) || LENGTH(val) != 3)
	error(_("'tsp' attribute must be numeric of length three"));

    if (isReal(val)) {
	start = REAL(val)[0];
	end = REAL(val)[1];
	frequency = REAL(val)[2];
    }
    else {
	start = (INTEGER(val)[0] == NA_INTEGER) ?
	    NA_REAL : INTEGER(val)[0];
	end = (INTEGER(val)[1] == NA_INTEGER) ?
	    NA_REAL : INTEGER(val)[1];
	frequency = (INTEGER(val)[2] == NA_INTEGER) ?
	    NA_REAL : INTEGER(val)[2];
    }
    if (frequency <= 0) badtsp();
    n = nrows(vec);
    if (n == 0) error(_("cannot assign 'tsp' to zero-length vector"));

    /* FIXME:  1.e-5 should rather be == option('ts.eps') !! */
    if (fabs(end - start - (n - 1)/frequency) > 1.e-5)
	badtsp();

    PROTECT(vec);
    val = allocVector(REALSXP, 3);
    PROTECT(val);
    REAL(val)[0] = start;
    REAL(val)[1] = end;
    REAL(val)[2] = frequency;
    vec->setAttribute(static_cast<Symbol*>(R_TspSymbol), val);
    UNPROTECT(2);
    return vec;
}

static SEXP commentgets(SEXP vec, SEXP comment)
{
    if (vec == R_NilValue)
	error(_("attempt to set an attribute on NULL"));

    if (isNull(comment) || isString(comment)) {
	vec->setAttribute(static_cast<Symbol*>(R_CommentSymbol),
			  length(comment) <= 0 ? nullptr : comment);
	return R_NilValue;
    }
    error(_("attempt to set invalid 'comment' attribute"));
    return R_NilValue;/*- just for -Wall */
}

SEXP attribute_hidden do_commentgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* value_)
{
    RObject* object = x_;
    RObject* comment = value_;
    if (MAYBE_SHARED(object)) object = duplicate(object);
    if (length(comment) == 0) comment = R_NilValue;
    setAttrib(object, R_CommentSymbol, comment);
    SET_NAMED(object, 0);
    return object;
}

SEXP attribute_hidden do_comment(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_)
{
    return getAttrib(x_, R_CommentSymbol);
}

SEXP classgets(SEXP vec, SEXP klass)
{
    if (isNull(klass) || isString(klass)) {
	int ncl = length(klass);
  	if (ncl <= 0) {
	    vec->setAttribute(static_cast<Symbol*>(R_ClassSymbol), nullptr);
	    // problems when package building:  UNSET_S4_OBJECT(vec);
	}
	else {
	    /* When data frames were a special data type */
	    /* we had more exhaustive checks here.  Now that */
	    /* use JMCs interpreted code, we don't need this */
	    /* FIXME : The whole "classgets" may as well die. */

	    /* HOWEVER, it is the way that the object bit gets set/unset */

	    Rboolean isfactor = FALSE;

	    if (vec == R_NilValue)
		error(_("attempt to set an attribute on NULL"));

	    for(int i = 0; i < ncl; i++)
		if(streql(CHAR(STRING_ELT(klass, i)), "factor")) { /* ASCII */
		    isfactor = TRUE;
		    break;
		}
	    if(isfactor && TYPEOF(vec) != INTSXP) {
		/* we cannot coerce vec here, so just fail */
		error(_("adding class \"factor\" to an invalid object"));
	    }

	    vec->setAttribute(static_cast<Symbol*>(R_ClassSymbol), klass);

#ifdef R_classgets_copy_S4
// not ok -- fails at installation around byte-compiling methods
	    if(ncl == 1 && R_has_methods_attached()) { // methods: do not act too early
		SEXP cld = R_getClassDef_R(klass);
		if(!isNull(cld)) {
		    PROTECT(cld);
		    /* More efficient? can we protect? -- rather *assign* in method-ns?
		       static SEXP oldCl = NULL;
		       if(!oldCl) oldCl = R_getClassDef("oldClass");
		       if(!oldCl) oldCl = mkString("oldClass");
		       PROTECT(oldCl);
		    */
		    if(!R_isVirtualClass(cld, R_MethodsNamespace) &&
		       !R_extends(cld, mkString("oldClass"), R_MethodsNamespace)) // set S4 bit :
			// !R_extends(cld, oldCl, R_MethodsNamespace)) // set S4 bit :

			SET_S4_OBJECT(vec);

		    UNPROTECT(1); // UNPROTECT(2);
		}
	    }
#endif
	}
	return R_NilValue;
    }
    error(_("attempt to set invalid 'class' attribute"));
    return R_NilValue;/*- just for -Wall */
}

/* oldClass<-(), primitive */
SEXP attribute_hidden do_classgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* object, RObject* new_class)
{
    if (MAYBE_SHARED(object)) object = shallow_duplicate(object);
    if (length(new_class) == 0) new_class = R_NilValue;
    if(IS_S4_OBJECT(object))
	UNSET_S4_OBJECT(object);
    setAttrib(object, R_ClassSymbol, new_class);
    SET_NAMED(object, 0);
    return object;
}

/* oldClass, primitive */
SEXP attribute_hidden do_class(Expression* call, const BuiltInFunction* op, RObject* x)
{
    SEXP s3class;
    if(IS_S4_OBJECT(x)) {
      if((s3class = S3Class(x)) != R_NilValue) {
	return s3class;
      }
    } /* else */
    return getAttrib(x, R_ClassSymbol);
}

/* character elements corresponding to the syntactic types in the
   grammar */
static SEXP lang2str(SEXP obj, SEXPTYPE t)
{
  SEXP symb = CAR(obj);
  static GCRoot<> if_sym = nullptr, while_sym, for_sym, eq_sym, gets_sym,
    lpar_sym, lbrace_sym, call_sym;
  if(!if_sym) {
    /* initialize:  another place for a hash table */
    if_sym = install("if");
    while_sym = install("while");
    for_sym = install("for");
    eq_sym = install("=");
    gets_sym = install("<-");
    lpar_sym = install("(");
    lbrace_sym = install("{");
    call_sym = install("call");
  }
  if(isSymbol(symb)) {
    if(symb == if_sym || symb == for_sym || symb == while_sym ||
       symb == lpar_sym || symb == lbrace_sym ||
       symb == eq_sym || symb == gets_sym)
      return PRINTNAME(symb);
  }
  return PRINTNAME(call_sym);
}

/* the S4-style class: for dispatch required to be a single string;
   for the new class() function;
   if(!singleString) , keeps S3-style multiple classes.
   Called from the methods package, so exposed.
 */
SEXP R_data_class(SEXP obj, Rboolean singleString)
{
    SEXP value, klass = getAttrib(obj, R_ClassSymbol);
    int n = length(klass);
    if(n == 1 || (n > 0 && !singleString))
	return(klass);
    if(n == 0) {
	SEXP dim = getAttrib(obj, R_DimSymbol);
	int nd = length(dim);
	if(nd > 0) {
	    if(nd == 2)
		klass = mkChar("matrix");
	    else
		klass = mkChar("array");
	}
	else {
	  SEXPTYPE t = TYPEOF(obj);
	  switch(t) {
	  case CLOSXP: case SPECIALSXP: case BUILTINSXP:
	    klass = mkChar("function");
	    break;
	  case REALSXP:
	    klass = mkChar("numeric");
	    break;
	  case SYMSXP:
	    klass = mkChar("name");
	    break;
	  case LANGSXP:
	    klass = lang2str(obj, t);
	    break;
	  default:
	    klass = type2str(t);
	  }
	}
    }
    else
	klass = asChar(klass);
    PROTECT(klass);
    value = ScalarString(klass);
    UNPROTECT(1);
    return value;
}

static GCRoot<> s_dot_S3Class = nullptr;

static GCRoot<> R_S4_extends_table = nullptr;

static SEXP cache_class(const char *class_str, SEXP klass)
{
    if(!R_S4_extends_table) {
	R_S4_extends_table = R_NewHashedEnv(R_NilValue, ScalarInteger(0));
	R_PreserveObject(R_S4_extends_table);
    }
    if(isNull(klass)) { /* retrieve cached value */
	SEXP val = findVarInFrame(R_S4_extends_table, install(class_str));
	return (val == R_UnboundValue) ? klass : val;
    }
    defineVar(install(class_str), klass, R_S4_extends_table);
    return klass;
}

static SEXP S4_extends(SEXP klass, bool use_tab) {
    static Symbol* s_extends = nullptr, *s_extendsForS3 = nullptr;
    SEXP e, val; const char *class_str;
    const void *vmax;
    if(use_tab) vmax = vmaxget();
    if(!s_extends) {
	s_extends= Symbol::obtain("extends");
	s_extendsForS3 = Symbol::obtain(".extendsForS3");
	R_S4_extends_table = R_NewHashedEnv(R_NilValue, ScalarInteger(0));
    }
    /* sanity check for methods package available */
    if(findVar(s_extends, R_GlobalEnv) == R_UnboundValue)
	return klass;
    class_str = translateChar(STRING_ELT(klass, 0)); /* TODO: include package attr. */
    if(use_tab) {
	val = findVarInFrame(R_S4_extends_table, install(class_str));
	vmaxset(vmax);
	if(val != R_UnboundValue)
	    return val;
    }
    // else:  val <- .extendsForS3(klass) -- and cache it
    PROTECT(e = allocVector(LANGSXP, 2));
    SETCAR(e, s_extendsForS3);
    val = CDR(e);
    SETCAR(val, klass);
    val = eval(e, R_MethodsNamespace);
    cache_class(class_str, val);
    UNPROTECT(1);
    return(val);
}

SEXP R_S4_extends(SEXP klass, SEXP useTable)
{
    return S4_extends(klass, asLogical(useTable));
}


/* pre-allocated default class attributes */
static struct {
    SEXP vector;
    SEXP matrix;
    SEXP array;
} Type2DefaultClass[MAX_NUM_SEXPTYPE];


static SEXP createDefaultClass(SEXP part1, SEXP part2, SEXP part3)
{
    int size = 0;
    if (part1 != R_NilValue) size++;
    if (part2 != R_NilValue) size++;
    if (part3 != R_NilValue) size++;

    if (size == 0 || part2 == R_NilValue) return R_NilValue;

    SEXP res = allocVector(STRSXP, size);
    R_PreserveObject(res);

    int i = 0;
    if (part1 != R_NilValue) SET_STRING_ELT(res, i++, part1);
    if (part2 != R_NilValue) SET_STRING_ELT(res, i++, part2);
    if (part3 != R_NilValue) SET_STRING_ELT(res, i, part3);

    MARK_NOT_MUTABLE(res);
    return res;
}

attribute_hidden
void InitS3DefaultTypes()
{
    for(int type = 0; type < MAX_NUM_SEXPTYPE; type++) {
	SEXP part2 = R_NilValue;
	SEXP part3 = R_NilValue;
	int nprotected = 0;

	switch(type) {
	    case CLOSXP:
	    case SPECIALSXP:
	    case BUILTINSXP:
		part2 = PROTECT(mkChar("function"));
		nprotected++;
		break;
	    case INTSXP:
	    case REALSXP:
	        part2 = PROTECT(type2str_nowarn(SEXPTYPE(type)));
		part3 = PROTECT(mkChar("numeric"));
		nprotected += 2;
		break;
	    case LANGSXP:
		/* part2 remains R_NilValue: default type cannot be
		   pre-allocated, as it depends on the object value */
		break;
	    case SYMSXP:
		part2 = PROTECT(mkChar("name"));
		nprotected++;
		break;
	    default:
	        part2 = PROTECT(type2str_nowarn(SEXPTYPE(type)));
		nprotected++;
	}

	Type2DefaultClass[type].vector =
	    createDefaultClass(R_NilValue, part2, part3);
	Type2DefaultClass[type].matrix =
	    createDefaultClass(mkChar("matrix"), part2, part3);
	Type2DefaultClass[type].array =
	    createDefaultClass(mkChar("array"), part2, part3);
	UNPROTECT(nprotected);
    }
}

/* Version for S3-dispatch */
SEXP attribute_hidden R_data_class2 (SEXP obj)
{
    SEXP klass = getAttrib(obj, R_ClassSymbol);
    if(length(klass) > 0) {
	if(IS_S4_OBJECT(obj))
	    return S4_extends(klass, TRUE);
	else
	    return klass;
    }
    else { /* length(klass) == 0 */

	SEXP dim = getAttrib(obj, R_DimSymbol);
	int n = length(dim);
	SEXPTYPE t = TYPEOF(obj);
	SEXP defaultClass;
	switch(n) {
	case 0:  defaultClass = Type2DefaultClass[t].vector; break;
	case 2:  defaultClass = Type2DefaultClass[t].matrix; break;
	default: defaultClass = Type2DefaultClass[t].array;  break;
	}

	if (defaultClass != R_NilValue) {
	    return defaultClass;
	}

	/* now t == LANGSXP, but check to make sure */
	if (t != LANGSXP)
	    error("type must be LANGSXP at this point");
	if (n == 0) {
	    return ScalarString(lang2str(obj, t));
	}
	SEXP part1;
	if (n == 2) {
	    part1 = mkChar("matrix");
	} else {
	    part1 = mkChar("array");
	}
	PROTECT(part1);
	defaultClass = PROTECT(allocVector(STRSXP, 2));
	SET_STRING_ELT(defaultClass, 0, part1);
	SET_STRING_ELT(defaultClass, 1, lang2str(obj, t));
	UNPROTECT(2); /* part1, defaultClass */
	return defaultClass;
    }
}

// class(x)  &  .cache_class(classname, extendsForS3(.)) {called from methods} :
SEXP attribute_hidden R_do_cache_data_class(/*const*/ Expression* call, const BuiltInFunction* op, RObject* klass, RObject* value)
{
    call->check1arg("class");
    if(TYPEOF(klass) != STRSXP || LENGTH(klass) < 1)
	error("invalid class argument to internal .class_cache");
    const char *class_str = translateChar(STRING_ELT(klass, 0));
    return cache_class(class_str, value);
}

SEXP attribute_hidden R_do_data_class(/*const*/ Expression* call, const BuiltInFunction* op, RObject* klass)
{
    call->check1arg("x");
    return R_data_class(klass, FALSE);
}

/* names(object) <- name */
SEXP attribute_hidden do_namesgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* value_)
{
    RObject* object = x_;
    RObject* names = value_;
    /* Special case: removing non-existent names, to avoid a copy */
    if (names == R_NilValue &&
	getAttrib(object, R_NamesSymbol) == R_NilValue)
	return object;

    if (MAYBE_SHARED(object))
	object = shallow_duplicate(object);

    if(IS_S4_OBJECT(object)) {
	const char *klass = CHAR(STRING_ELT(R_data_class(object, FALSE), 0));
	if(getAttrib(object, R_NamesSymbol) == R_NilValue) {
	    /* S4 class w/o a names slot or attribute */
	    if(TYPEOF(object) == S4SXP)
		error(_("class '%s' has no 'names' slot"), klass);
	    else
		warning(_("class '%s' has no 'names' slot; assigning a names attribute will create an invalid object"), klass);
	}
	else if(TYPEOF(object) == S4SXP)
	    error(_("invalid to use names()<- to set the 'names' slot in a non-vector class ('%s')"), klass);
	/* else, go ahead, but can't check validity of replacement*/
    }
    if (names != R_NilValue) {
	GCStackRoot<PairList> tl(new PairList);
      PROTECT(call = new Expression(Rf_install("as.character"), { names }));
	names = eval(call, R_BaseEnv);
	UNPROTECT(1);
    }
    setAttrib(object, R_NamesSymbol, names);
    SET_NAMED(object, 0);
    return object;
}

SEXP namesgets(SEXP vec, SEXP val)
{
    int i;
    SEXP s, rval, tval;

    PROTECT(vec);
    PROTECT(val);

    /* Ensure that the labels are indeed */
    /* a vector of character strings */

    if (isList(val)) {
	if (!isVectorizable(val))
	    error(_("incompatible 'names' argument"));
	else {
	    rval = allocVector(STRSXP, length(vec));
	    PROTECT(rval);
	    /* See PR#10807 */
	    for (i = 0, tval = val;
		 i < length(vec) && tval != R_NilValue;
		 i++, tval = CDR(tval)) {
		s = coerceVector(CAR(tval), STRSXP);
		SET_STRING_ELT(rval, i, STRING_ELT(s, 0));
	    }
	    UNPROTECT(1);
	    val = rval;
	}
    } else val = coerceVector(val, STRSXP);
    UNPROTECT(1);
    PROTECT(val);

    /* Check that the lengths and types are compatible */

    if (xlength(val) < xlength(vec)) {
	val = xlengthgets(val, xlength(vec));
	UNPROTECT(1);
	PROTECT(val);
    }

    checkNames(vec, val);

    /* Special treatment for one dimensional arrays */

    if (isVector(vec) || isList(vec) || isLanguage(vec)) {
	s = getAttrib(vec, R_DimSymbol);
	if (TYPEOF(s) == INTSXP && length(s) == 1) {
	    PROTECT(val = CONS(val, R_NilValue));
	    setAttrib(vec, R_DimNamesSymbol, val);
	    UNPROTECT(3);
	    return vec;
	}
    }

    if (isList(vec) || isLanguage(vec)) {
	/* Cons-cell based objects */
	i = 0;
	for (s = vec; s != R_NilValue; s = CDR(s), i++)
	    if (STRING_ELT(val, i) != R_NilValue
		&& STRING_ELT(val, i) != R_NaString
		&& *CHAR(STRING_ELT(val, i)) != 0) /* test of length */
		SET_TAG(s, installTrChar(STRING_ELT(val, i)));
	    else
		SET_TAG(s, R_NilValue);
    }
    else if (isVector(vec) || IS_S4_OBJECT(vec))
	/* Normal case */
	vec->setAttribute(static_cast<Symbol*>(R_NamesSymbol), val);
    else
	error(_("invalid type (%s) to set 'names' attribute"),
	      type2char(TYPEOF(vec)));
    UNPROTECT(2);
    return vec;
}

SEXP attribute_hidden do_names(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_)
{
    SEXP ans;
    ans = x_;
    if (isVector(ans) || isList(ans) || isLanguage(ans) ||
	IS_S4_OBJECT(ans))
	ans = getAttrib(ans, R_NamesSymbol);
    else if (isEnvironment(ans))
	ans = R_lsInternal3(ans, TRUE, FALSE);
    else ans = R_NilValue;
    return ans;
}

SEXP attribute_hidden do_dimnamesgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* names_)
{
    RObject* object = x_;
    if (MAYBE_SHARED(object)) object = shallow_duplicate(object);
    setAttrib(object, R_DimNamesSymbol, names_);
    SET_NAMED(object, 0);
    return object;
}

static SEXP dimnamesgets1(SEXP val1)
{
    SEXP this2;

    if (LENGTH(val1) == 0) return R_NilValue;
    /* if (isObject(val1)) dispatch on as.character.foo, but we don't
       have the context at this point to do so */

    if (inherits(val1, "factor"))  /* mimic as.character.factor */
	return asCharacterFactor(val1);

    if (!isString(val1)) { /* mimic as.character.default */
	PROTECT(this2 = coerceVector(val1, STRSXP));
	this2->clearAttributes();
	UNPROTECT(1);
	return this2;
    }
    return val1;
}


SEXP dimnamesgets(SEXP vec, SEXP val)
{
    SEXP dims, top, newval;
    int i, k;

    PROTECT(vec);
    PROTECT(val);

    if (!isArray(vec) && !isList(vec))
	error(_("'dimnames' applied to non-array"));
    /* This is probably overkill, but you never know; */
    /* there may be old pair-lists out there */
    /* There are, when this gets used as names<- for 1-d arrays */
    if (!isPairList(val) && !isNewList(val))
	error(_("'%s' must be a list"), "dimnames");
    dims = getAttrib(vec, R_DimSymbol);
    if ((k = LENGTH(dims)) < length(val))
	error(_("length of 'dimnames' [%d] must match that of 'dims' [%d]"),
	      length(val), k);
    if (length(val) == 0) {
	removeAttrib(vec, R_DimNamesSymbol);
	UNPROTECT(2);
	return vec;
    }
    /* Old list to new list */
    if (isList(val)) {
	newval = allocVector(VECSXP, k);
	for (i = 0; i < k; i++) {
	    SET_VECTOR_ELT(newval, i, CAR(val));
	    val = CDR(val);
	}
	UNPROTECT(1);
	PROTECT(val = newval);
    }
    if (length(val) > 0 && length(val) < k) {
	newval = lengthgets(val, k);
	UNPROTECT(1);
	PROTECT(val = newval);
    }
    if (MAYBE_REFERENCED(val)) {
	newval = shallow_duplicate(val);
	UNPROTECT(1);
	PROTECT(val = newval);
    }
    if (k != length(val))
	error(_("length of 'dimnames' [%d] must match that of 'dims' [%d]"),
	      length(val), k);
    for (i = 0; i < k; i++) {
	SEXP _this = VECTOR_ELT(val, i);
	if (_this != R_NilValue) {
	    if (!isVector(_this))
		error(_("invalid type (%s) for 'dimnames' (must be a vector)"),
		      type2char(TYPEOF(_this)));
	    if (INTEGER(dims)[i] != LENGTH(_this) && LENGTH(_this) != 0)
		error(_("length of 'dimnames' [%d] not equal to array extent"),
		      i+1);
	    SET_VECTOR_ELT(val, i, dimnamesgets1(_this));
	}
    }
    vec->setAttribute(static_cast<Symbol*>(R_DimNamesSymbol), val);
    if (isList(vec) && k == 1) {
	top = VECTOR_ELT(val, 0);
	i = 0;
	for (val = vec; !isNull(val); val = CDR(val))
	    SET_TAG(val, installTrChar(STRING_ELT(top, i++)));
    }
    UNPROTECT(2);
    return vec;
}

SEXP attribute_hidden do_dimnames(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_)
{
    return getAttrib(x_, R_DimNamesSymbol);
}

SEXP attribute_hidden do_dim(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_)
{
    return getAttrib(x_, R_DimSymbol);
}

SEXP attribute_hidden do_dimgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* value_)
{
    SEXP x = x_;
    /* Duplication might be expensive */
    if (value_ == R_NilValue) {
	SEXP s;
	for (s = ATTRIB(x); s != R_NilValue; s = CDR(s))
	    if (TAG(s) == R_DimSymbol || TAG(s) == R_NamesSymbol) break;
	if (s == R_NilValue) return x;
    }
    if (MAYBE_SHARED(x)) x = shallow_duplicate(x);
    setAttrib(x, R_DimSymbol, value_);
    setAttrib(x, R_NamesSymbol, R_NilValue);
    SET_NAMED(x, 0);
    return x;
}

SEXP dimgets(SEXP vec, SEXP val)
{
    int i, ndim;
    R_xlen_t len, total;
    PROTECT(vec);
    PROTECT(val);
    if ((!isVector(vec) && !isList(vec)))
	error(_("invalid first argument"));

    if (!isVector(val) && !isList(val))
	error(_("invalid second argument"));
    val = coerceVector(val, INTSXP);
    UNPROTECT(1);
    PROTECT(val);

    len = xlength(vec);
    ndim = length(val);
    if (ndim == 0)
	error(_("length-0 dimension vector is invalid"));
    total = 1;
    for (i = 0; i < ndim; i++) {
	/* need this test first as NA_INTEGER is < 0 */
	if (INTEGER(val)[i] == NA_INTEGER)
	    error(_("the dims contain missing values"));
	if (INTEGER(val)[i] < 0)
	    error(_("the dims contain negative values"));
	total *= INTEGER(val)[i];
    }
    if (total != len) {
	if (total > INT_MAX || len > INT_MAX)
	    error(_("dims do not match the length of object"), total, len);
	else

	    error(_("dims [product %d] do not match the length of object [%d]"), total, len);
    }
    removeAttrib(vec, R_DimNamesSymbol);
    vec->setAttribute(static_cast<Symbol*>(R_DimSymbol), val);

    /* Mark as immutable so nested complex assignment can't made the
       dim attribute inconsistent with the length */
    MARK_NOT_MUTABLE(val);

    UNPROTECT(2);
    return vec;
}

SEXP attribute_hidden do_attributes(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x)
{
    SEXP names, namesattr, value;
    int nvalues;

    if (TYPEOF(x) == ENVSXP)
	R_CheckStack(); /* in case attributes might lead to a cycle */

    namesattr = R_NilValue;
    GCStackRoot<> attrs(ATTRIB(x));
    nvalues = length(attrs);
    if (isList(x)) {
	namesattr = getAttrib(x, R_NamesSymbol);
	if (namesattr != R_NilValue)
	    nvalues++;
    }
    /* FIXME */
    if (nvalues <= 0)
	return R_NilValue;
    /* FIXME */
    PROTECT(namesattr);
    PROTECT(value = allocVector(VECSXP, nvalues));
    PROTECT(names = allocVector(STRSXP, nvalues));
    nvalues = 0;
    if (namesattr != R_NilValue) {
	SET_VECTOR_ELT(value, nvalues, namesattr);
	SET_STRING_ELT(names, nvalues, PRINTNAME(R_NamesSymbol));
	nvalues++;
    }
    while (attrs != R_NilValue) {
	/* treat R_RowNamesSymbol specially */
	if (TAG(attrs) == R_RowNamesSymbol)
	    SET_VECTOR_ELT(value, nvalues,
			   getAttrib(x, R_RowNamesSymbol));
	else
	    SET_VECTOR_ELT(value, nvalues, CAR(attrs));
	if (TAG(attrs) == R_NilValue)
	    SET_STRING_ELT(names, nvalues, R_BlankString);
	else
	    SET_STRING_ELT(names, nvalues, PRINTNAME(TAG(attrs)));
	attrs = CDR(attrs);
	nvalues++;
    }
    setAttrib(value, R_NamesSymbol, names);
    SET_NAMED(value, NAMED(x));
    UNPROTECT(3);
    return value;
}

SEXP attribute_hidden do_levelsgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* value_)
{
    RObject* object = x_;
    RObject* levels = value_;
    if(!isNull(levels) && any_duplicated(levels, FALSE))
	warningcall(call, _("duplicated levels in factors are deprecated"));
/* TODO errorcall(call, _("duplicated levels are not allowed in factors anymore")); */
    if (MAYBE_SHARED(object)) object = duplicate(object);
    setAttrib(object, R_LevelsSymbol, levels);
    return object;
}

/* attributes(object) <- attrs */
SEXP attribute_hidden do_attributesgets(/*const*/ Expression* call, const BuiltInFunction* op, RObject* object, RObject* attrs)
{
/* NOTE: The following code ensures that when an attribute list */
/* is attached to an object, that the "dim" attibute is always */
/* brought to the front of the list.  This ensures that when both */
/* "dim" and "dimnames" are set that the "dim" is attached first. */

    SEXP names = R_NilValue /* -Wall */;
    int i, i0 = -1, nattrs;

    /* Do checks before duplication */
    if (!isNewList(attrs))
	error(_("attributes must be a list or NULL"));
    nattrs = length(attrs);
    if (nattrs > 0) {
	names = getAttrib(attrs, R_NamesSymbol);
	if (names == R_NilValue)
	    error(_("attributes must be named"));
	for (i = 1; i < nattrs; i++) {
	    if (STRING_ELT(names, i) == R_NilValue ||
		CHAR(STRING_ELT(names, i))[0] == '\0') { /* all ASCII tests */
		error(_("all attributes must have names [%d does not]"), i+1);
	    }
	}
    }

    if (object == R_NilValue) {
	if (attrs == R_NilValue)
	    return R_NilValue;
	else
	    PROTECT(object = allocVector(VECSXP, 0));
    } else {
	/* Unlikely to have NAMED == 0 here.
	   As from R 2.7.0 we don't optimize NAMED == 1 _if_ we are
	   setting any attributes as an error later on would leave
	   'obj' changed */
	if (MAYBE_SHARED(object) || (MAYBE_REFERENCED(object) && nattrs))
	    object = shallow_duplicate(object);
	PROTECT(object);
    }


    /* Empty the existing attribute list */

    /* FIXME: the code below treats pair-based structures */
    /* in a special way.  This can probably be dropped down */
    /* the road (users should never encounter pair-based lists). */
    /* Of course, if we want backward compatibility we can't */
    /* make the change. :-( */

    if (isList(object))
	setAttrib(object, R_NamesSymbol, R_NilValue);
    object->clearAttributes();
    /* Probably need to fix up S4 bit in other cases, but
       definitely in this one */
    if(nattrs == 0) UNSET_S4_OBJECT(object);

    /* We do two passes through the attributes; the first */
    /* finding and transferring "dim" and the second */
    /* transferring the rest.  This is to ensure that */
    /* "dim" occurs in the attribute list before "dimnames". */

    if (nattrs > 0) {
	for (i = 0; i < nattrs; i++) {
	    if (!strcmp(CHAR(STRING_ELT(names, i)), "dim")) {
		i0 = i;
		setAttrib(object, R_DimSymbol, VECTOR_ELT(attrs, i));
		break;
	    }
	}
	for (i = 0; i < nattrs; i++) {
	    if (i == i0) continue;
	    setAttrib(object, installTrChar(STRING_ELT(names, i)),
		      VECTOR_ELT(attrs, i));
	}
    }
    UNPROTECT(1);
    return object;
}

/*  This code replaces an R function defined as

    attr <- function (x, which)
    {
	if (!is.character(which))
	    stop("attribute name must be of mode character")
	if (length(which) != 1)
	    stop("exactly one attribute name must be given")
	attributes(x)[[which]]
   }

The R functions was being called very often and replacing it by
something more efficient made a noticeable difference on several
benchmarks.  There is still some inefficiency since using getAttrib
means the attributes list will be searched twice, but this seems
fairly minor.  LT */

SEXP attribute_hidden do_attr(SEXP call, SEXP op, SEXP args, SEXP env)
{
    SEXP tag = R_NilValue, alist, ans, x, which, exact_;
    const char *str;
    int nargs = length(args), exact = 0;
    enum { NONE, PARTIAL, PARTIAL2, FULL } match = NONE;

    /* argument matching */
    static GCRoot<ArgMatcher> matcher
	= new ArgMatcher({ "x", "which", "exact" });
    ArgList arglist(SEXP_downcast<PairList*>(args), ArgList::EVALUATED);
    matcher->match(arglist, { &x, &which, &exact_ });

    if (nargs < 2 || nargs > 3)
	errorcall(call, "either 2 or 3 arguments are required");

    if (!isString(which))
	errorcall(call, _("'which' must be of mode character"));
    if (length(which) != 1)
	errorcall(call, _("exactly one attribute 'which' must be given"));

    if (TYPEOF(x) == ENVSXP)
	R_CheckStack(); /* in case attributes might lead to a cycle */

    if(exact_ != R_MissingArg) {
	exact = asLogical(exact_);
	if(exact == NA_LOGICAL) exact = 0;
    }

    if(STRING_ELT(which, 0) == NA_STRING) {
	UNPROTECT(1);
	return R_NilValue;
    }
    str = translateChar(STRING_ELT(which, 0));
    size_t n = strlen(str);

    /* try to find a match among the attributes list */
    for (alist = ATTRIB(x); alist != R_NilValue; alist = CDR(alist)) {
	SEXP tmp = TAG(alist);
	const char *s = CHAR(PRINTNAME(tmp));
	if (! strncmp(s, str, n)) {
	    if (strlen(s) == n) {
		tag = tmp;
		match = FULL;
		break;
	    }
	    else if (match == PARTIAL || match == PARTIAL2) {
		/* this match is partial and we already have a partial match,
		   so the query is ambiguous and we will return R_NilValue
		   unless a full match comes up.
		*/
		match = PARTIAL2;
	    } else {
		tag = tmp;
		match = PARTIAL;
	    }
	}
    }
    if (match == PARTIAL2) {
	UNPROTECT(1);
	return R_NilValue;
    }

    /* Unless a full match has been found, check for a "names" attribute.
       This is stored via TAGs on pairlists, and via rownames on 1D arrays.
    */
    if (match != FULL && strncmp("names", str, n) == 0) {
	if (strlen("names") == n) {
	    /* we have a full match on "names", if there is such an
	       attribute */
	    tag = R_NamesSymbol;
	    match = FULL;
	}
	else if (match == NONE && !exact) {
	    /* no match on other attributes and a possible
	       partial match on "names" */
	    tag = R_NamesSymbol;
	    SEXP t = PROTECT(getAttrib(x, tag));
	    if(t != R_NilValue && R_warn_partial_match_attr)
		warningcall(call, _("partial match of '%s' to '%s'"), str,
			    CHAR(PRINTNAME(tag)));
	    UNPROTECT(2);
	    return t;
	}
	else if (match == PARTIAL && strcmp(CHAR(PRINTNAME(tag)), "names")) {
	    /* There is a possible partial match on "names" and on another
	       attribute. If there really is a "names" attribute, then the
	       query is ambiguous and we return R_NilValue.  If there is no
	       "names" attribute, then the partially matched one, which is
	       the current value of tag, can be used. */
	    if (getAttrib(x, R_NamesSymbol) != R_NilValue) {
		UNPROTECT(1);
		return R_NilValue;
	    }
	}
    }

    if (match == NONE  || (exact && match != FULL)) {
	UNPROTECT(1);
	return R_NilValue;
    }
    if (match == PARTIAL && R_warn_partial_match_attr)
	warningcall(call, _("partial match of '%s' to '%s'"), str,
		    CHAR(PRINTNAME(tag)));

    ans =  getAttrib(x, tag);
    UNPROTECT(1);
    return ans;
}

static void check_slot_assign(SEXP obj, SEXP input, SEXP value, SEXP env)
{
    SEXP
	valueClass = PROTECT(R_data_class(value, FALSE)),
	objClass   = PROTECT(R_data_class(obj, FALSE));
    static GCRoot<> checkAt = NULL;
    // 'methods' may *not* be in search() ==> do as if calling  methods::checkAtAssignment(..)
    if(!isMethodsDispatchOn()) { // needed?
	SEXP e = PROTECT(lang1(install("initMethodDispatch")));
	eval(e, R_MethodsNamespace); // only works with methods loaded
	UNPROTECT(1);
    }
    if(checkAt == NULL)
	checkAt = findFun(install("checkAtAssignment"), R_MethodsNamespace);
    SEXP e = PROTECT(lang4(checkAt, objClass, input, valueClass));
    eval(e, env);
    UNPROTECT(3);
}


SEXP attribute_hidden do_slotgets(SEXP call, SEXP op, SEXP args, SEXP env)
{
    /*  attr@nlist  <-  value  */
    ArgList arglist(SEXP_downcast<PairList*>(args), ArgList::RAW);
    SEXP input;
    
    SEXP nlist = arglist.get(1);
    if (isSymbol(nlist))
	input = Rf_ScalarString(PRINTNAME(nlist));
    else if(isString(nlist)) {
        input = length(nlist) == 1
            ? nlist : Rf_ScalarString(STRING_ELT(nlist, 0));
    }
    else {
	error(_("invalid type '%s' for slot name"),
	      type2char(TYPEOF(nlist)));
	return R_NilValue; /*-Wall*/
    }
    PROTECT(input);
    
    /* replace the second argument with a string */
    arglist.set(1, input);
    UNPROTECT(1); // 'input' is now protected
    
    auto disptached = Rf_DispatchOrEval(SEXP_downcast<Expression*>(call),
                                        SEXP_downcast<BuiltInFunction*>(op),
                                        &arglist,
                                        SEXP_downcast<Environment*>(env),
                                        MissingArgHandling::Keep);
    if (disptached.first)
        return disptached.second;

    RObject* obj = arglist.get(0);
    RObject* value = arglist.get(2);
    check_slot_assign(obj, input, value, env);
    value = R_do_slot_assign(obj, input, value);
    UNPROTECT(2);
    return value;
}

SEXP attribute_hidden do_attrgets(SEXP call, SEXP op, SEXP args, SEXP env)
{
    /*  attr(x, which = "<name>")  <-  value  */
    SEXP obj = CAR(args);
    if (MAYBE_SHARED(obj))
	PROTECT(obj = shallow_duplicate(obj));
    else
	PROTECT(obj);

    /* argument matching */
    static GCRoot<ArgMatcher> matcher
	= new ArgMatcher({ "x", "which", "value" });
    ArgList arglist(SEXP_downcast<PairList*>(args), ArgList::EVALUATED);
    SEXP ignored, name, value;
    matcher->match(arglist, { &ignored, &name, &value });

    if (!isValidString(name) || STRING_ELT(name, 0) == NA_STRING)
	error(_("'name' must be non-null character string"));
    /* TODO?  if (isFactor(obj) && !strcmp(asChar(name), "levels"))
     * ---         if(any_duplicated(CADDR(args)))
     *                  error(.....)
     */
    setAttrib(obj, name, value);
    UNPROTECT(1);
    SET_NAMED(obj, 0);
    return obj;
}


/* These provide useful shortcuts which give access to */
/* the dimnames for matrices and arrays in a standard form. */

/* NB: this may return R_alloc-ed rn and dn */
void GetMatrixDimnames(SEXP x, SEXP *rl, SEXP *cl,
		       const char **rn, const char **cn)
{
    SEXP dimnames = getAttrib(x, R_DimNamesSymbol);
    SEXP nn;

    if (isNull(dimnames)) {
	*rl = R_NilValue;
	*cl = R_NilValue;
	*rn = nullptr;
	*cn = nullptr;
    }
    else {
	*rl = VECTOR_ELT(dimnames, 0);
	*cl = VECTOR_ELT(dimnames, 1);
	nn = getAttrib(dimnames, R_NamesSymbol);
	if (isNull(nn)) {
	    *rn = nullptr;
	    *cn = nullptr;
	}
	else {
	    *rn = translateChar(STRING_ELT(nn, 0));
	    *cn = translateChar(STRING_ELT(nn, 1));
	}
    }
}


SEXP GetArrayDimnames(SEXP x)
{
    return getAttrib(x, R_DimNamesSymbol);
}


/* the code to manage slots in formal classes. These are attributes,
   but without partial matching and enforcing legal slot names (it's
   an error to get a slot that doesn't exist. */


static GCRoot<> pseudo_NULL = nullptr;

static GCRoot<> s_dot_Data;
static GCRoot<> s_getDataPart;
static GCRoot<> s_setDataPart;

static void init_slot_handling(void) {
    s_dot_Data = install(".Data");
    s_dot_S3Class = install(".S3Class");
    s_getDataPart = install("getDataPart");
    s_setDataPart = install("setDataPart");
    /* create and preserve an object that is NOT R_NilValue, and is used
       to represent slots that are NULL (which an attribute can not
       be).  The point is not just to store NULL as a slot, but also to
       provide a check on invalid slot names (see get_slot below).

       The object has to be a symbol if we're going to check identity by
       just looking at referential equality. */
    pseudo_NULL = install("\001NULL\001");
}

static SEXP data_part(SEXP obj) {
    SEXP e, val;
    if(!s_getDataPart)
	init_slot_handling();
    PROTECT(e = allocVector(LANGSXP, 2));
    SETCAR(e, s_getDataPart);
    val = CDR(e);
    SETCAR(val, obj);
    val = eval(e, R_MethodsNamespace);
    UNSET_S4_OBJECT(val); /* data part must be base vector */
    UNPROTECT(1);
    return(val);
}

static SEXP set_data_part(SEXP obj,  SEXP rhs) {
    SEXP e, val;
    if(!s_setDataPart)
	init_slot_handling();
    PROTECT(e = allocVector(LANGSXP, 3));
    SETCAR(e, s_setDataPart);
    val = CDR(e);
    SETCAR(val, obj);
    val = CDR(val);
    SETCAR(val, rhs);
    val = eval(e, R_MethodsNamespace);
    SET_S4_OBJECT(val);
    UNPROTECT(1);
    return(val);
}

SEXP S3Class(SEXP obj)
{
    if(!s_dot_S3Class) init_slot_handling();
    return getAttrib(obj, s_dot_S3Class);
}

/* Slots are stored as attributes to
   provide some back-compatibility
*/

/**
 * R_has_slot() : a C-level test if a obj@<name> is available;
 *                as R_do_slot() gives an error when there's no such slot.
 */
int R_has_slot(SEXP obj, SEXP name) {

#define R_SLOT_INIT							\
    if(!(isSymbol(name) || (isString(name) && LENGTH(name) == 1)))	\
	error(_("invalid type or length for slot name"));		\
    if(!s_dot_Data)							\
	init_slot_handling();						\
    if(isString(name)) name = installChar(STRING_ELT(name, 0))

    R_SLOT_INIT;
    if(name == s_dot_Data && TYPEOF(obj) != S4SXP)
	return(1);
    /* else */
    return(getAttrib(obj, name) != R_NilValue);
}

/* the @ operator, and its assignment form.  Processed much like $
   (see do_subset3) but without S3-style methods.
*/
/* currently, R_get_slot() ["methods"] is a trivial wrapper for this: */
SEXP R_do_slot(SEXP obj, SEXP name) {
    R_SLOT_INIT;
    if(name == s_dot_Data)
	return data_part(obj);
    else {
	SEXP value = getAttrib(obj, name);
	if(value == R_NilValue) {
	    SEXP input = name, classString;
	    if(name == s_dot_S3Class) /* defaults to class(obj) */
		return R_data_class(obj, FALSE);
	    else if(name == R_NamesSymbol &&
		    TYPEOF(obj) == VECSXP) /* needed for namedList class */
		return value;
	    if(isSymbol(name) ) {
		input = PROTECT(ScalarString(PRINTNAME(name)));
		classString = getAttrib(obj, R_ClassSymbol);
		if(isNull(classString)) {
		    UNPROTECT(1);
		    error(_("cannot get a slot (\"%s\") from an object of type \"%s\""),
			  translateChar(asChar(input)),
			  CHAR(type2str(TYPEOF(obj))));
		}
		UNPROTECT(1);
	    }
	    else classString = R_NilValue; /* make sure it is initialized */
	    /* not there.  But since even NULL really does get stored, this
	       implies that there is no slot of this name.  Or somebody
	       screwed up by using attr(..) <- NULL */

	    error(_("no slot of name \"%s\" for this object of class \"%s\""),
		  translateChar(asChar(input)),
		  translateChar(asChar(classString)));
	}
	else if(value == pseudo_NULL)
	    value = R_NilValue;
	return value;
    }
}
#undef R_SLOT_INIT

/* currently, R_set_slot() ["methods"] is a trivial wrapper for this: */
SEXP R_do_slot_assign(SEXP obj, SEXP name, SEXP value) {
#ifndef _R_ver_le_2_11_x_
    if (isNull(obj))/* cannot use !IS_S4_OBJECT(obj), because
		     *  slot(obj, name, check=FALSE) <- value  must work on
		     * "pre-objects", currently only in makePrototypeFromClassDef() */
	error(_("attempt to set slot on NULL object"));
#endif
    PROTECT(obj); PROTECT(value);
                                /* Ensure that name is a symbol */
    if(isString(name) && LENGTH(name) == 1)
	name = installTrChar(STRING_ELT(name, 0));
    if(TYPEOF(name) == CHARSXP)
	name = installTrChar(name);
    if(!isSymbol(name) )
	error(_("invalid type or length for slot name"));

    if(!s_dot_Data)		/* initialize */
	init_slot_handling();

    if(name == s_dot_Data) {	/* special handling */
	obj = set_data_part(obj, value);
    } else {
	if(isNull(value))		/* Slots, but not attributes, can be NULL.*/
	    value = pseudo_NULL;	/* Store a special symbol instead. */

#ifdef _R_ver_le_2_11_x_
	setAttrib(obj, name, value);
#else
	/* simplified version of setAttrib(obj, name, value);
	   here we do *not* treat "names", "dimnames", "dim", .. specially : */
	PROTECT(name);
	if (MAYBE_REFERENCED(value)) value = R_FixupRHS(obj, value);
	//SET_NAMED(value, NAMED(value) | NAMED(obj));
	UNPROTECT(1);
	obj->setAttribute(static_cast<Symbol*>(name), value);
#endif
    }
    UNPROTECT(2);
    return obj;
}

SEXP attribute_hidden do_AT(SEXP call, SEXP op, SEXP args, SEXP env)
{
    SEXP  nlist, object, ans, klass;

    checkArity(op, args);
    if(!isMethodsDispatchOn())
	error(_("formal classes cannot be used without the 'methods' package"));
    nlist = CADR(args);
    /* Do some checks here -- repeated in R_do_slot, but on repeat the
     * test expression should kick out on the first element. */
    if(!(isSymbol(nlist) || (isString(nlist) && LENGTH(nlist) == 1)))
	error(_("invalid type or length for slot name"));
    if(isString(nlist)) nlist = installTrChar(STRING_ELT(nlist, 0));
    PROTECT(object = eval(CAR(args), env));
    if(!s_dot_Data) init_slot_handling();
    if(nlist != s_dot_Data && !IS_S4_OBJECT(object)) {
	klass = getAttrib(object, R_ClassSymbol);
	if(length(klass) == 0)
	    error(_("trying to get slot \"%s\" from an object of a basic class (\"%s\") with no slots"),
		  CHAR(PRINTNAME(nlist)),
		  CHAR(STRING_ELT(R_data_class(object, FALSE), 0)));
	else
	    error(_("trying to get slot \"%s\" from an object (class \"%s\") that is not an S4 object "),
		  CHAR(PRINTNAME(nlist)),
		  translateChar(STRING_ELT(klass, 0)));
    }

    ans = R_do_slot(object, nlist);
    UNPROTECT(1);
    return ans;
}

/* Return a suitable S3 object (OK, the name of the routine comes from
   an earlier version and isn't quite accurate.) If there is a .S3Class
   slot convert to that S3 class.
   Otherwise, unless type == S4SXP, look for a .Data or .xData slot.  The
   value of type controls what's wanted.  If it is S4SXP, then ONLY
   .S3class is used.  If it is ANYSXP, don't check except that automatic
   conversion from the current type only applies for classes that extend
   one of the basic types (i.e., not S4SXP).  For all other types, the
   recovered data must match the type.
   Because S3 objects can't have type S4SXP, .S3Class slot is not searched
   for in that type object, unless ONLY that class is wanted.
   (Obviously, this is another routine that has accumulated barnacles and
   should at some time be broken into separate parts.)
*/
SEXP attribute_hidden
R_getS4DataSlot(SEXP obj, SEXPTYPE type)
{
  static Symbol* s_xData = Symbol::obtain(".xData");
  static Symbol* s_dotData = Symbol::obtain(".Data");

  SEXP value = R_NilValue;
  PROTECT_INDEX opi;

  PROTECT_WITH_INDEX(obj, &opi);

  if(TYPEOF(obj) != S4SXP || type == S4SXP) {
    SEXP s3class = S3Class(obj);
    if(s3class == R_NilValue && type == S4SXP) {
      UNPROTECT(1); /* obj */
      return R_NilValue;
    }
    PROTECT(s3class);
    if(MAYBE_REFERENCED(obj))
      REPROTECT(obj = shallow_duplicate(obj), opi);
    if(s3class != R_NilValue) {/* replace class with S3 class */
      setAttrib(obj, R_ClassSymbol, s3class);
      setAttrib(obj, s_dot_S3Class, R_NilValue); /* not in the S3 class */
    }
    else { /* to avoid inf. recursion, must unset class attribute */
      setAttrib(obj, R_ClassSymbol, R_NilValue);
    }
    UNPROTECT(1); /* s3class */
    UNSET_S4_OBJECT(obj);
    if(type == S4SXP) {
      UNPROTECT(1); /* obj */
      return obj;
    }
    value = obj;
  }
  else
      value = getAttrib(obj, s_dotData);
  if(value == R_NilValue)
      value = getAttrib(obj, s_xData);

  UNPROTECT(1); /* obj */
/* the mechanism for extending abnormal types.  In the future, would b
   good to consolidate under the ".Data" slot, but this has
   been used to mean S4 objects with non-S4 type, so for now
   a secondary slot name, ".xData" is used to avoid confusion
*/
  if(value != R_NilValue &&
     (type == ANYSXP || type == TYPEOF(value)))
     return value;
  else
     return R_NilValue;
}
