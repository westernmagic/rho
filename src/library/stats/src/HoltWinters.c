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

/*  R : A Computer Language for Statistical Data Analysis
 *
 *  Copyright (C) 2003-7  The R Development Core Team
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
 *  http://www.r-project.org/Licenses/.
 */

#include <R.h>
#include "ts.h"
#include <stdlib.h>

void HoltWinters (double *x,
		  int    *xl,
		  double *alpha,
		  double *beta,
		  double *gamma,
		  int    *start_time,
		  int    *seasonal,
		  int    *period,
		  double *a,
		  double *b,
		  double *s,

		  /* return values */
		  double *SSE,
		  double *level,
		  double *trend,
		  double *season
    )
	
{
    double res = 0, xhat = 0, stmp = 0;
    int i, i0, s0;

    /* copy start values to the beginning of the vectors */
    level[0] = *a;
    if (*beta > 0) trend[0] = *b;
    if (*gamma > 0) memcpy(season, s, *period * sizeof(double));
    
    for (i = *start_time - 1; i < *xl; i++) {
	/* indices for period i */
	i0 = i - *start_time + 2;
	s0 = i0 + *period - 1;
	
        /* forecast *for* period i */
        xhat = level[i0 - 1] + (*beta > 0 ? trend[i0 - 1] : 0);
	stmp = *gamma > 0 ? season[s0 - *period] : (*seasonal != 1);
	if (*seasonal == 1)
	    xhat += stmp;
	else
	    xhat *= stmp;

	/* Sum of Squared Errors */
	res   = x[i] - xhat;
	*SSE += res * res;

	/* estimate of level *in* period i */
	if (*seasonal == 1)
	    level[i0] = *alpha       * (x[i] - stmp)
		      + (1 - *alpha) * (level[i0 - 1] + trend[i0 - 1]);
	else
	    level[i0] = *alpha       * (x[i] / stmp)
		      + (1 - *alpha) * (level[i0 - 1] + trend[i0 - 1]);

	/* estimate of trend *in* period i */
	if (*beta > 0)
	    trend[i0] = *beta        * (level[i0] - level[i0 - 1])
		      + (1 - *beta)  * trend[i0 - 1];

	/* estimate of seasonal component *in* period i */
	if (*gamma > 0) {
	    if (*seasonal == 1)
		season[s0] = *gamma       * (x[i] - level[i0])
 		           + (1 - *gamma) * stmp;
	    else
		season[s0] = *gamma       * (x[i] / level[i0])
			   + (1 - *gamma) * stmp;
	}
    }
}


