/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995-1998  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1998-2015  The R Core Team.
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

/* The x:y  primitive calls do_colon(); do_colon() calls cross_colon() if
   both arguments are factors and seq_colon() otherwise.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Defn.h>
#include <Internal.h>
#include <float.h>  /* for DBL_EPSILON */
#include <Rmath.h>
#include <R_ext/Itermacros.h>

#include "RBufferUtils.h"
#include "rho/ArgMatcher.hpp"
#include "rho/ExpressionVector.hpp"
#include "rho/GCStackRoot.hpp"

using namespace rho;

static R_StringBuffer cbuff = {nullptr, 0, MAXELTSIZE};

#define _S4_rep_keepClass
/* ==>  rep(<S4>, .) keeps class e.g., for list-like */

static SEXP cross_colon(SEXP call, SEXP s, SEXP t)
{
    SEXP a, la, ls, lt, rs, rt;
    int i, j, k, n, nls, nlt;
    char *cbuf;
    const void *vmax = vmaxget();

    if (length(s) != length(t))
	errorcall(call, _("unequal factor lengths"));
    n = length(s);
    ls = getAttrib(s, R_LevelsSymbol);
    lt = getAttrib(t, R_LevelsSymbol);
    nls = LENGTH(ls);
    nlt = LENGTH(lt);
    PROTECT(a = allocVector(INTSXP, n));
    PROTECT(rs = coerceVector(s, INTSXP));
    PROTECT(rt = coerceVector(t, INTSXP));
    for (i = 0; i < n; i++) {
	int vs = INTEGER(rs)[i];
	int vt = INTEGER(rt)[i];
	if ((vs == NA_INTEGER) || (vt == NA_INTEGER))
	    INTEGER(a)[i] = NA_INTEGER;
	else
	    INTEGER(a)[i] = vt + (vs - 1) * nlt;
    }
    UNPROTECT(2);
    if (!isNull(ls) && !isNull(lt)) {
	PROTECT(la = allocVector(STRSXP, nls * nlt));
	k = 0;
	/* FIXME: possibly UTF-8 version */
	for (i = 0; i < nls; i++) {
	    const char *vi = translateChar(STRING_ELT(ls, i));
	    size_t vs = strlen(vi);
	    for (j = 0; j < nlt; j++) {
		const char *vj = translateChar(STRING_ELT(lt, j));
		size_t vt = strlen(vj), len = vs + vt + 2;
		cbuf = static_cast<char*>(R_AllocStringBuffer(len, &cbuff));
		snprintf(cbuf, len, "%s:%s", vi, vj);
		SET_STRING_ELT(la, k, mkChar(cbuf));
		k++;
	    }
	}
	setAttrib(a, R_LevelsSymbol, la);
	UNPROTECT(1);
    }
    PROTECT(la = mkString("factor"));
    setAttrib(a, R_ClassSymbol, la);
    UNPROTECT(2);
    R_FreeStringBufferL(&cbuff);
    vmaxset(vmax);
    return a;
}

/* interval at which to check interrupts */
#define NINTERRUPT 1000000U

static SEXP seq_colon(double n1, double n2, SEXP call)
{
    double r = fabs(n2 - n1);
    if(r >= R_XLEN_T_MAX)
	errorcall(call, _("result would be too long a vector"));

    SEXP ans;
    R_xlen_t n = (R_xlen_t)(r + 1 + FLT_EPSILON);

    Rboolean useInt = Rboolean((n1 <= INT_MAX) &&  (n1 == (int) n1));
    if(useInt) {
	if(n1 <= INT_MIN || n1 > INT_MAX)
	    useInt = FALSE;
	else {
	    /* r := " the effective 'to' "  of  from:to */
	    double dn = double( n);
	    r = n1 + ((n1 <= n2) ? dn-1 : -(dn-1));
	    if(r <= INT_MIN || r > INT_MAX) useInt = FALSE;
	}
    }
    if (useInt) {
	int in1 = (int)(n1);
	ans = allocVector(INTSXP, n);
	if (n1 <= n2)
	    for (int i = 0; i < n; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		INTEGER(ans)[i] = in1 + i;
	    }
	else
	    for (int i = 0; i < n; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		INTEGER(ans)[i] = in1 - i;
	    }
    } else {
	ans = allocVector(REALSXP, n);
	if (n1 <= n2)
	    for (R_xlen_t i = 0; i < n; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		REAL(ans)[i] = n1 + double(i);
	    }
	else
	    for (R_xlen_t i = 0; i < n; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		REAL(ans)[i] = n1 - double(i);
	    }
    }
    return ans;
}

SEXP attribute_hidden do_colon(/*const*/ Expression* call, const BuiltInFunction* op, RObject* from_, RObject* to_)
{
    SEXP s1, s2;
    double n1, n2;

    if (inherits(from_, "factor") && inherits(to_, "factor"))
	return(cross_colon(call, from_, to_));

    s1 = from_;
    s2 = to_;
    n1 = length(s1);
    n2 = length(s2);
    if (n1 == 0 || n2 == 0)
	errorcall(call, _("argument of length 0"));
    if (n1 > 1)
	warningcall(call,
		    ngettext("numerical expression has %d element: only the first used",
			     "numerical expression has %d elements: only the first used",
			     (int) n1), (int) n1);
    if (n2 > 1)
	warningcall(call,
		    ngettext("numerical expression has %d element: only the first used",
			     "numerical expression has %d elements: only the first used",
			     (int) n2), (int) n2);
    n1 = asReal(s1);
    n2 = asReal(s2);
    if (ISNAN(n1) || ISNAN(n2))
	errorcall(call, _("NA/NaN argument"));
    return seq_colon(n1, n2, call);
}

/* rep.int(x, times) for a vector times */
static SEXP rep2(SEXP s, SEXP ncopy)
{
    R_xlen_t i, na, nc, n;
    int j;
    SEXP a, t;

    PROTECT(t = coerceVector(ncopy, INTSXP));

    nc = xlength(ncopy);
    na = 0;
    for (i = 0; i < nc; i++) {
//	if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	if (INTEGER(t)[i] == NA_INTEGER || INTEGER(t)[i] < 0)
	    error(_("invalid '%s' value"), "times");
	na += INTEGER(t)[i];
    }

/*    R_xlen_t ni = NINTERRUPT, ratio;
    if(nc > 0) {
	ratio = na/nc; // average no of replications
	if (ratio > 1000U) ni = 1000U;
	} */
    PROTECT(a = allocVector(TYPEOF(s), na));
    n = 0;
    switch (TYPEOF(s)) {
    case LGLSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    for (j = 0; j < INTEGER(t)[i]; j++)
		LOGICAL(a)[n++] = LOGICAL(s)[i];
	}
	break;
    case INTSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    for (j = 0; j < INTEGER(t)[i]; j++)
		INTEGER(a)[n++] = INTEGER(s)[i];
	}
	break;
    case REALSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    for (j = 0; j < INTEGER(t)[i]; j++)
		REAL(a)[n++] = REAL(s)[i];
	}
	break;
    case CPLXSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    for (j = 0; j < INTEGER(t)[i]; j++)
		COMPLEX(a)[n++] = COMPLEX(s)[i];
	}
	break;
    case STRSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    for (j = 0; j < INTEGER(t)[i]; j++)
		SET_STRING_ELT(a, n++, STRING_ELT(s, i));
	}
	break;
    case VECSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    SEXP elt = duplicate(VECTOR_ELT(s, i));
	    for (j = 0; j < INTEGER(t)[i]; j++)
		SET_VECTOR_ELT(a, n++, elt);
	}
	break;
    case EXPRSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    SEXP elt = lazy_duplicate(VECTOR_ELT(s, i));
	    for (j = 0; j < INTEGER(t)[i]; j++)
		SET_XVECTOR_ELT(a, n++, elt);
	    if (j > 1) SET_NAMED(elt, 2);
	}
	break;
    case RAWSXP:
	for (i = 0; i < nc; i++) {
//	    if ((i+1) % ni == 0) R_CheckUserInterrupt();
	    for (j = 0; j < INTEGER(t)[i]; j++)
		RAW(a)[n++] = RAW(s)[i];
	}
	break;
    default:
	UNIMPLEMENTED_TYPE("rep2", s);
    }
    UNPROTECT(2);
    return a;
}

/* rep_len(x, len), also used for rep.int() with scalar 'times' */
static SEXP rep3(SEXP s, R_xlen_t ns, R_xlen_t na)
{
    R_xlen_t i, j;
    SEXP a;

    PROTECT(a = allocVector(TYPEOF(s), na));

    switch (TYPEOF(s)) {
    case LGLSXP:
	MOD_ITERATE1(na, ns, i, j, {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    LOGICAL(a)[i] = LOGICAL(s)[j];
	});
	break;
    case INTSXP:
	MOD_ITERATE1(na, ns, i, j, {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    INTEGER(a)[i] = INTEGER(s)[j];
	});
	break;
    case REALSXP:
	MOD_ITERATE1(na, ns, i, j,  {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    REAL(a)[i] = REAL(s)[j];
	});
	break;
    case CPLXSXP:
	MOD_ITERATE1(na, ns, i, j, {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    COMPLEX(a)[i] = COMPLEX(s)[j];
	});
	break;
    case RAWSXP:
	MOD_ITERATE1(na, ns, i, j, {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    RAW(a)[i] = RAW(s)[j];
	});
	break;
    case STRSXP:
	MOD_ITERATE1(na, ns, i, j, {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    SET_STRING_ELT(a, i, STRING_ELT(s, j));
	});
	break;
    case VECSXP:
    case EXPRSXP:
	MOD_ITERATE1(na, ns, i, j, {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    SET_VECTOR_ELT(a, i, lazy_duplicate(VECTOR_ELT(s, j)));
	});
	break;
    default:
	UNIMPLEMENTED_TYPE("rep3", s);
    }
    UNPROTECT(1);
    return a;
}

SEXP attribute_hidden do_rep_int(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* times_)
{
    SEXP s = x_, ncopy = times_;
    R_xlen_t nc;
    SEXP a;

    if (!isVector(ncopy))
	error(_("incorrect type for second argument"));

    if (!isVector(s) && s != R_NilValue)
	error(_("attempt to replicate an object of type '%s'"),
	      type2char(TYPEOF(s)));

    nc = xlength(ncopy); // might be 0
    if (nc == xlength(s))
	PROTECT(a = rep2(s, ncopy));
    else {
	if (nc != 1) error(_("invalid '%s' value"), "times");

#ifdef LONG_VECTOR_SUPPORT
	double snc = asReal(ncopy);
	if (!R_FINITE(snc) || snc < 0)
	    error(_("invalid '%s' value"), "times");
	nc = R_xlen_t( snc);
#else
	if ((nc = asInteger(ncopy)) == NA_INTEGER || nc < 0)/* nc = 0 ok */
	    error(_("invalid '%s' value"), "times");
#endif
	R_xlen_t ns = xlength(s);
	PROTECT(a = rep3(s, ns, nc * ns));
    }

#ifdef _S4_rep_keepClass
    if(IS_S4_OBJECT(s)) { /* e.g. contains = "list" */
	setAttrib(a, R_ClassSymbol, getAttrib(s, R_ClassSymbol));
	SET_S4_OBJECT(a);
    }
#endif

    if (inherits(s, "factor")) {
	SEXP tmp;
	if(inherits(s, "ordered")) {
	    PROTECT(tmp = allocVector(STRSXP, 2));
	    SET_STRING_ELT(tmp, 0, mkChar("ordered"));
	    SET_STRING_ELT(tmp, 1, mkChar("factor"));
	} else PROTECT(tmp = mkString("factor"));
	setAttrib(a, R_ClassSymbol, tmp);
	UNPROTECT(1);
	setAttrib(a, R_LevelsSymbol, getAttrib(s, R_LevelsSymbol));
    }
    UNPROTECT(1);
    return a;
}

SEXP attribute_hidden do_rep_len(/*const*/ Expression* call, const BuiltInFunction* op, RObject* x_, RObject* length_out_)
{
    R_xlen_t ns, na;
    SEXP a, s, len;

    s = x_;

    if (!isVector(s) && s != R_NilValue)
	error(_("attempt to replicate non-vector"));

    len = length_out_;
    if(length(len) != 1)
	error(_("invalid '%s' value"), "length.out");
#ifdef LONG_VECTOR_SUPPORT
    double sna = asReal(len);
    if (!R_FINITE(sna) || sna < 0)
	error(_("invalid '%s' value"), "length.out");
    na = R_xlen_t( sna);
#else
    if ((na = asInteger(len)) == NA_INTEGER || na < 0) /* na = 0 ok */
	error(_("invalid '%s' value"), "length.out");
#endif

    if (TYPEOF(s) == NILSXP && na > 0)
	error(_("cannot replicate NULL to a non-zero length"));
    ns = xlength(s);
    if (ns == 0) {
	SEXP a;
	PROTECT(a = duplicate(s));
	if(na > 0) a = xlengthgets(a, na);
	UNPROTECT(1);
	return a;
    }
    PROTECT(a = rep3(s, ns, na));

#ifdef _S4_rep_keepClass
    if(IS_S4_OBJECT(s)) { /* e.g. contains = "list" */
	setAttrib(a, R_ClassSymbol, getAttrib(s, R_ClassSymbol));
	SET_S4_OBJECT(a);
    }
#endif

    if (inherits(s, "factor")) {
	SEXP tmp;
	if(inherits(s, "ordered")) {
	    PROTECT(tmp = allocVector(STRSXP, 2));
	    SET_STRING_ELT(tmp, 0, mkChar("ordered"));
	    SET_STRING_ELT(tmp, 1, mkChar("factor"));
	} else PROTECT(tmp = mkString("factor"));
	setAttrib(a, R_ClassSymbol, tmp);
	UNPROTECT(1);
	setAttrib(a, R_LevelsSymbol, getAttrib(s, R_LevelsSymbol));
    }
    UNPROTECT(1);
    return a;
}

/* rep(), allowing for both times and each */
static SEXP rep4(SEXP x, SEXP times, R_xlen_t len, int each, R_xlen_t nt)
{
    SEXP a;
    R_xlen_t lx = xlength(x);
    R_xlen_t i, j, k, k2, k3, sum;

    // faster code for common special case
    if (each == 1 && nt == 1) return rep3(x, lx, len);

    PROTECT(a = allocVector(TYPEOF(x), len));

    switch (TYPEOF(x)) {
    case LGLSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		LOGICAL(a)[i] = LOGICAL(x)[(i/each) % lx];
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    LOGICAL(a)[k2++] = LOGICAL(x)[i];
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case INTSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		INTEGER(a)[i] = INTEGER(x)[(i/each) % lx];
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    INTEGER(a)[k2++] = INTEGER(x)[i];
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case REALSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		REAL(a)[i] = REAL(x)[(i/each) % lx];
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    REAL(a)[k2++] = REAL(x)[i];
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case CPLXSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		COMPLEX(a)[i] = COMPLEX(x)[(i/each) % lx];
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    COMPLEX(a)[k2++] = COMPLEX(x)[i];
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case STRSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		SET_STRING_ELT(a, i, STRING_ELT(x, (i/each) % lx));
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    SET_STRING_ELT(a, k2++, STRING_ELT(x, i));
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case VECSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		SET_VECTOR_ELT(a, i, VECTOR_ELT(x, (i/each) % lx));
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    SET_VECTOR_ELT(a, k2++, VECTOR_ELT(x, i));
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case EXPRSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		SET_XVECTOR_ELT(a, i, XVECTOR_ELT(x, (i/each) % lx));
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    SET_XVECTOR_ELT(a, k2++, XVECTOR_ELT(x, i));
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    case RAWSXP:
	if(nt == 1)
	    for(i = 0; i < len; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		RAW(a)[i] = RAW(x)[(i/each) % lx];
	    }
	else {
	    for(i = 0, k = 0, k2 = 0; i < lx; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		for(j = 0, sum = 0; j < each; j++) sum += INTEGER(times)[k++];
		for(k3 = 0; k3 < sum; k3++) {
		    RAW(a)[k2++] = RAW(x)[i];
		    if(k2 == len) goto done;
		}
	    }
	}
	break;
    default:
	UNIMPLEMENTED_TYPE("rep4", x);
    }
done:
    UNPROTECT(1);
    return a;
}

/* We are careful to use MissingArgHandling::Keep here (inside
   DispatchOrEval) to avoid dropping missing arguments so e.g.
   rep(1:3,,8) matches length.out */

/* This is a primitive SPECIALSXP with internal argument matching */
SEXP attribute_hidden do_rep(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP ans, x, times, length_out, each_, ignored;
    int each = 1, nprotect = 3;
    R_xlen_t i, lx, len = NA_INTEGER, nt;
    ArgList arglist(SEXP_downcast<PairList*>(args), ArgList::RAW);

    /* includes factors, POSIX[cl]t, Date */
    auto dispatched = Rf_DispatchOrEval(SEXP_downcast<Expression*>(call),
                                        SEXP_downcast<BuiltInFunction*>(op),
                                        &arglist,
                                        SEXP_downcast<Environment*>(rho),
                                        MissingArgHandling::Keep);
    if (dispatched.first)
        return dispatched.second;

    /* This is a primitive, and we have not dispatched to a method
       so we manage the argument matching ourselves.  We pretend this is
       rep(x, times, length.out, each, ...)
    */
    static GCRoot<ArgMatcher> matcher = new ArgMatcher(
	{ "x", "times", "length.out", "each", "..." });
    matcher->match(arglist, { &x, &times, &length_out, &each_, &ignored });

    /* supported in R 2.15.x */
    if (TYPEOF(x) == LISTSXP)
	errorcall(call, "replication of pairlists is defunct");

    lx = xlength(x);

    double slen = asReal(length_out);
    if (R_FINITE(slen)) {
	if(slen < 0)
	    errorcall(call, _("invalid '%s' argument"), "length.out");
	len = R_xlen_t( slen);
    } else {
	len = asInteger(length_out);
	if(len != NA_INTEGER && len < 0)
	    errorcall(call, _("invalid '%s' argument"), "length.out");
    }
    if(length(length_out) != 1)
	warningcall(call, _("first element used of '%s' argument"),
		    "length.out");

    each = asInteger(each_);
    if(each != NA_INTEGER && each < 0)
	errorcall(call, _("invalid '%s' argument"), "each");
    if(length(each_) != 1)
	warningcall(call, _("first element used of '%s' argument"), "each");
    if(each == NA_INTEGER) each = 1;

    if(lx == 0) {
	if(len > 0 && x == R_NilValue)
	    warningcall(call, "'x' is NULL so the result will be NULL");
	SEXP a;
	PROTECT(a = duplicate(x));
	if(len != NA_INTEGER && len > 0) a = xlengthgets(a, len);
	UNPROTECT(3);
	return a;
    }
    if (!isVector(x))
	errorcall(call, "attempt to replicate an object of type '%s'",
		  type2char(TYPEOF(x)));

    /* So now we know x is a vector of positive length.  We need to
       replicate it, and its names if it has them. */

    /* First find the final length using 'times' and 'each' */
    if(len != NA_INTEGER) { /* takes precedence over times */
	nt = 1;
    } else {
	R_xlen_t sum = 0;
	if(times == R_MissingArg) PROTECT(times = ScalarInteger(1));
	else PROTECT(times = coerceVector(times, INTSXP));
	nprotect++;
	nt = XLENGTH(times);
	if(nt != 1 && nt != lx * each)
	    errorcall(call, _("invalid '%s' argument"), "times");
	if(nt == 1) {
	    int it = INTEGER(times)[0];
	    if (it == NA_INTEGER || it < 0)
		errorcall(call, _("invalid '%s' argument"), "times");
	    len = lx * it * each;
	} else {
	    for(i = 0; i < nt; i++) {
		int it = INTEGER(times)[i];
		if (it == NA_INTEGER || it < 0)
		    errorcall(call, _("invalid '%s' argument"), "times");
		sum += it;
	    }
	    len = sum;
	}
    }
    if(len > 0 && each == 0)
	errorcall(call, _("invalid '%s' argument"), "each");
    SEXP xn = getAttrib(x, R_NamesSymbol);

    PROTECT(ans = rep4(x, times, len, each, nt));
    if (length(xn) > 0)
	setAttrib(ans, R_NamesSymbol, rep4(xn, times, len, each, nt));

#ifdef _S4_rep_keepClass
    if(IS_S4_OBJECT(x)) { /* e.g. contains = "list" */
	setAttrib(ans, R_ClassSymbol, getAttrib(x, R_ClassSymbol));
	SET_S4_OBJECT(ans);
    }
#endif
    UNPROTECT(nprotect);
    return ans;
}


/*
  This is a primitive SPECIALSXP with internal argument matching,
  implementing seq.int.

   'along' has to be used on an unevaluated argument, and evalList
   tries to evaluate language objects.
 */

#define FEPS 1e-10
#define myabs(x) (x < 0 ? x : -x)
/* to match seq.default */
SEXP attribute_hidden do_seq(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP ans = R_NilValue /* -Wall */, from, to, by, len, along, ignored;
    int nargs = length(args), lf;
    Rboolean One = RHOCONSTRUCT(Rboolean, nargs == 1);
    R_xlen_t i, lout = NA_INTEGER;

    ArgList arglist(SEXP_downcast<PairList*>(args), ArgList::EVALUATED);
    auto dispatched = Rf_Dispatch(SEXP_downcast<Expression*>(call),
                                  SEXP_downcast<BuiltInFunction*>(op),
                                  arglist,
                                  SEXP_downcast<Environment*>(rho));
    if (dispatched.first)
        return dispatched.second;

    /* This is a primitive and we manage argument matching ourselves.
       We pretend this is
       seq(from, to, by, length.out, along.with, ...)
    */
    static GCRoot<ArgMatcher> matcher = new ArgMatcher(
	{ "from", "to", "by", "length.out", "along.with", "..." });
    matcher->match(arglist,
		   { &from, &to, &by, &len, &along, &ignored });

    if(One && from != R_MissingArg) {
	lf = length(from);
	if(lf == 1 && (TYPEOF(from) == INTSXP || TYPEOF(from) == REALSXP)) {
	    double rfrom = asReal(from);
	    if (!R_FINITE(rfrom))
		errorcall(call, "'from' cannot be NA, NaN or infinite");
	    ans = seq_colon(1.0, rfrom, call);
	}
	else if (lf)
	    ans = seq_colon(1.0, double(lf), call);
	else
	    ans = allocVector(INTSXP, 0);
	goto done;
    }
    if(along != R_MissingArg) {
	lout = XLENGTH(along);
	if(One) {
	    ans = lout ? seq_colon(1.0, double(lout), call) : allocVector(INTSXP, 0);
	    goto done;
	}
    } else if(len != R_MissingArg && len != R_NilValue) {
	double rout = asReal(len);
	if(ISNAN(rout) || rout <= -0.5)
	    errorcall(call, _("'length.out' must be a non-negative number"));
	if(length(len) != 1)
	    warningcall(call, _("first element used of '%s' argument"),
			"length.out");
	lout = R_xlen_t( ceil(rout));
    }

    if(lout == NA_INTEGER) {
	double rfrom = asReal(from), rto = asReal(to), rby = asReal(by), *ra;
	if(from == R_MissingArg) rfrom = 1.0;
	else if(length(from) != 1) error("'from' must be of length 1");
	if(to == R_MissingArg) rto = 1.0;
	else if(length(to) != 1) error("'to' must be of length 1");
	if (!R_FINITE(rfrom))
	    errorcall(call, "'from' cannot be NA, NaN or infinite");
	if (!R_FINITE(rto))
	    errorcall(call, "'to' cannot be NA, NaN or infinite");
	if(by == R_MissingArg)
	    ans = seq_colon(rfrom, rto, call);
	else {
	    if(length(by) != 1) error("'by' must be of length 1");
	    double del = rto - rfrom, n, dd;
	    R_xlen_t nn;
	    if(!R_FINITE(rfrom))
		errorcall(call, _("'from' must be finite"));
	    if(!R_FINITE(rto))
		errorcall(call, _("'to' must be finite"));
	    if(del == 0.0 && rto == 0.0) {
		ans = to;
		goto done;
	    }
	    /* printf("from = %f, to = %f, by = %f\n", rfrom, rto, rby); */
	    n = del/rby;
	    if(!R_FINITE(n)) {
		if(del == 0.0 && rby == 0.0) {
		    ans = from;
		    goto done;
		} else
		    errorcall(call, _("invalid '(to - from)/by' in 'seq'"));
	    }
	    dd = fabs(del)/fmax2(fabs(rto), fabs(rfrom));
	    if(dd < 100 * DBL_EPSILON) {
		ans = from;
		goto done;
	    }
#ifdef LONG_VECTOR_SUPPORT
	    if(n > 100 * double( INT_MAX))
#else
	    if(n > double( INT_MAX))
#endif
		errorcall(call, _("'by' argument is much too small"));
	    if(n < - FEPS)
		errorcall(call, _("wrong sign in 'by' argument"));
	    if(TYPEOF(from) == INTSXP &&
	       TYPEOF(to) == INTSXP &&
	       TYPEOF(by) == INTSXP) {
		int *ia, ifrom = asInteger(from), iby = asInteger(by);
		/* With the current limits on integers and FEPS
		   reduced below 1/INT_MAX this is the same as the
		   next, so this is future-proofing against longer integers.
		*/
		/* seq.default gives integer result from
		   from + (0:n)*by
		*/
		nn = R_xlen_t( n);
		ans = allocVector(INTSXP, nn+1);
		ia = INTEGER(ans);
		for(i = 0; i <= nn; i++)
		    ia[i] = int(ifrom + i * iby);
	    } else {
		nn = int(n + FEPS);
		ans = allocVector(REALSXP, nn+1);
		ra = REAL(ans);
		for(i = 0; i <= nn; i++)
		    ra[i] = rfrom + double(i) * rby;
		/* Added in 2.9.0 */
		if (nn > 0)
		    if((rby > 0 && ra[nn] > rto) || (rby < 0 && ra[nn] < rto))
			ra[nn] = rto;
	    }
	}
    } else if (lout == 0) {
	ans = allocVector(INTSXP, 0);
    } else if (One) {
	ans = seq_colon(1.0, double(lout), call);
    } else if (by == R_MissingArg) {
	double rfrom = asReal(from), rto = asReal(to), rby;
	if(to == R_MissingArg) rto = rfrom + double(lout) - 1;
	if(from == R_MissingArg) rfrom = rto - double(lout) + 1;
	if(!R_FINITE(rfrom))
	    errorcall(call, _("'from' must be finite"));
	if(!R_FINITE(rto))
	    errorcall(call, _("'to' must be finite"));
	ans = allocVector(REALSXP, lout);
	if(lout > 0) REAL(ans)[0] = rfrom;
	if(lout > 1) REAL(ans)[lout - 1] = rto;
	if(lout > 2) {
	    rby = (rto - rfrom)/double(lout - 1);
	    for(i = 1; i < lout-1; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		REAL(ans)[i] = rfrom + double(i)*rby;
	    }
	}
    } else if (to == R_MissingArg) {
	double rfrom = asReal(from), rby = asReal(by), rto;
	if(from == R_MissingArg) rfrom = 1.0;
	if(!R_FINITE(rfrom))
	    errorcall(call, _("'from' must be finite"));
	if(!R_FINITE(rby))
	    errorcall(call, _("'by' must be finite"));
	rto = rfrom + double(lout-1)*rby;
	if(rby == int(rby) && rfrom <= INT_MAX && rfrom >= INT_MIN
	   && rto <= INT_MAX && rto >= INT_MIN) {
	    ans = allocVector(INTSXP, lout);
	    for(i = 0; i < lout; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		INTEGER(ans)[i] = int(rfrom + double(i)*rby);
	    }
	} else {
	    ans = allocVector(REALSXP, lout);
	    for(i = 0; i < lout; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		REAL(ans)[i] = rfrom + double(i)*rby;
	    }
	}
    } else if (from == R_MissingArg) {
	double rto = asReal(to), rby = asReal(by),
	    rfrom = rto - double(lout-1)*rby;
	if(!R_FINITE(rto))
	    errorcall(call, _("'to' must be finite"));
	if(!R_FINITE(rby))
	    errorcall(call, _("'by' must be finite"));
	if(rby == int(rby) && rfrom <= INT_MAX && rfrom >= INT_MIN
	   && rto <= INT_MAX && rto >= INT_MIN) {
	    ans = allocVector(INTSXP, lout);
	    for(i = 0; i < lout; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		INTEGER(ans)[i] = int(rto - double(lout - 1 - i)*rby);
	    }
	} else {
	    ans = allocVector(REALSXP, lout);
	    for(i = 0; i < lout; i++) {
//		if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
		REAL(ans)[i] = rto - double(lout - 1 - i)*rby;
	    }
	}
    } else
	errorcall(call, _("too many arguments"));

done:
    UNPROTECT(1);
    return ans;
}

SEXP attribute_hidden do_seq_along(/*const*/ Expression* call, const BuiltInFunction* op, Environment* rho, RObject* const* args, int num_args, const PairList* tags)
{
    SEXP ans;

    static BuiltInFunction* length_op = BuiltInFunction::obtainPrimitive(
	"length");
    // The arguments have already been evaluated, so call do_length directly.
    RObject* length = do_length(call, length_op, rho, args, num_args, tags);
    R_xlen_t len = length->sexptype() == INTSXP ?
	INTEGER(length)[0] : REAL(length)[0];

#ifdef LONG_VECTOR_SUPPORT
    if (len > INT_MAX) {
	ans = allocVector(REALSXP, len);
	double *p = REAL(ans);
	for(R_xlen_t i = 0; i < len; i++) {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    p[i] = double( (i+1));
	}
    } else
#endif
    {
	ans = allocVector(INTSXP, len);
	int *p = INTEGER(ans);
	for(int i = 0; i < len; i++) {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    p[i] = i+1;
	}
    }
    return ans;
}

SEXP attribute_hidden do_seq_len(/*const*/ Expression* call, const BuiltInFunction* op, RObject* length_)
{
    SEXP ans;
    R_xlen_t len;

    if(length(length_) != 1)
	warningcall(call, _("first element used of '%s' argument"),
		    "length.out");

 #ifdef LONG_VECTOR_SUPPORT
    double dlen = asReal(length_);
    if(!R_FINITE(dlen) || dlen < 0)
	errorcall(call, _("argument must be coercible to non-negative integer"));
    len = R_xlen_t( dlen);
#else
    len = asInteger(length_);
    if(len == NA_INTEGER || len < 0)
	errorcall(call, _("argument must be coercible to non-negative integer"));
#endif

 #ifdef LONG_VECTOR_SUPPORT
    if (len > INT_MAX) {
	ans = allocVector(REALSXP, len);
	double *p = REAL(ans);
	for(R_xlen_t i = 0; i < len; i++) {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    p[i] = double( (i+1));
	}
    } else
#endif
    {
	ans = allocVector(INTSXP, len);
	int *p = INTEGER(ans);
	for(int i = 0; i < len; i++) {
//	    if ((i+1) % NINTERRUPT == 0) R_CheckUserInterrupt();
	    p[i] = i+1;
	}
    }
    return ans;
}
