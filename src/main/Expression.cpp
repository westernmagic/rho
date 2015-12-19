/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2008-2014  Andrew R. Runnalls.
 *  Copyright (C) 2014 and onwards the CXXR Project Authors.
 *
 *  CXXR is not part of the R project, and bugs and other issues should
 *  not be reported via r-bugs or other R project channels; instead refer
 *  to the CXXR website.
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

/** @file Expression.cpp
 *
 * @brief Class CXXR::Expression and associated C interface.
 */

#include "CXXR/Expression.h"

#include <iostream>
#include "R_ext/Error.h"
#include "localization.h"
#include "CXXR/ArgList.hpp"
#include "CXXR/ClosureContext.hpp"
#include "CXXR/Environment.h"
#include "CXXR/Evaluator.h"
#include "CXXR/FunctionContext.hpp"
#include "CXXR/FunctionBase.h"
#include "CXXR/GCStackFrameBoundary.hpp"
#include "CXXR/GCStackRoot.hpp"
#include "CXXR/PlainContext.hpp"
#include "CXXR/RAllocStack.h"
#include "CXXR/StackChecker.hpp"
#include "CXXR/Symbol.h"

#undef match

using namespace std;
using namespace CXXR;

// Force the creation of non-inline embodiments of functions callable
// from C:
namespace CXXR {
    namespace ForceNonInline {
	SEXP (*lconsp)(SEXP cr, SEXP tl) = Rf_lcons;
   }
}

GCRoot<> R_CurrentExpr;

Expression* Expression::clone() const
{
    return new Expression(*this);
}

RObject* Expression::evaluate(Environment* env)
{
    IncrementStackDepthScope scope;
    RAllocStack::Scope ras_scope;
    ProtectStack::Scope ps_scope;

    FunctionBase* func;
    RObject* head = car();
    if (head->sexptype() == SYMSXP) {
	Symbol* symbol = static_cast<Symbol*>(head);
	func = findFunction(symbol, env);
	if (!func)
	    error(_("could not find function \"%s\""),
		  symbol->name()->c_str());
    } else {
	RObject* val = Evaluator::evaluate(head, env);
	if (!FunctionBase::isA(val))
	    error(_("attempt to apply non-function"));
	func = static_cast<FunctionBase*>(val);
    }
    func->maybeTrace(this);

    ArgList arglist(tail(), ArgList::RAW);
    return evaluateFunctionCall(func, env, &arglist);
}

RObject* Expression::evaluateFunctionCall(const FunctionBase* func,
                                          Environment* env,
                                          ArgList* raw_arglist) const
{
    if (func->sexptype() == CLOSXP) {
      return invokeClosure(static_cast<const Closure*>(func), env,
                           raw_arglist, nullptr);
    }

    assert(func->sexptype() == BUILTINSXP || func->sexptype() == SPECIALSXP);
    return applyBuiltIn(static_cast<const BuiltInFunction*>(func), env,
                        raw_arglist);
}

RObject* Expression::applyBuiltIn(const BuiltInFunction* builtin,
                                  Environment* env, ArgList* raw_arglist) const
{
    RObject* result;

    if (builtin->createsStackFrame()) {
	FunctionContext context(this, env, builtin);
	result = evaluateBuiltInCall(builtin, env, raw_arglist);
    } else {
	PlainContext context;
        result = evaluateBuiltInCall(builtin, env, raw_arglist);
    }

    if (builtin->printHandling() != BuiltInFunction::SOFT_ON) {
	Evaluator::enableResultPrinting(
            builtin->printHandling() != BuiltInFunction::FORCE_OFF);
    }
    return result;
}

RObject* Expression::evaluateBuiltInCall(
    const BuiltInFunction* func, Environment* env, ArgList* arglist) const
{
    if (func->hasDirectCall())
        return evaluateDirectBuiltInCall(func, env, arglist);
    else
      return evaluateIndirectBuiltInCall(func, env, arglist);
}

RObject* Expression::evaluateDirectBuiltInCall(
    const BuiltInFunction* func, Environment* env, ArgList* arglist) const
{
    if (arglist->has3Dots())
        arglist->evaluate(env);

    // Create an array on stack to write arguments to.
    int num_evaluated_args = listLength(arglist->list());
    RObject** evaluated_arg_array = (RObject**)alloca(
        num_evaluated_args * sizeof(RObject*));

    // Copy the arguments to the stack, evaluating if necessary.
    arglist->evaluateToArray(env, num_evaluated_args, evaluated_arg_array);

    if (func->printHandling() == BuiltInFunction::SOFT_ON) {
	Evaluator::enableResultPrinting(true);
    }

#ifdef Win32
    // This is an inlined version of Rwin_fpreset (src/gnuwin/extra.c)
    // and resets the precision, rounding and exception modes of a
    // ix86 fpu.
    // It gets called prior to every builtin function, just in case a badly
    // behaved DLL has changed the fpu control word.
    __asm__ ( "fninit" );
#endif

    return func->invoke(this, env, evaluated_arg_array, num_evaluated_args,
                        arglist->list());
}

RObject* Expression::evaluateIndirectBuiltInCall(
    const BuiltInFunction* func, Environment* env, ArgList* arglist) const
{
    if (func->sexptype() == BUILTINSXP
	&& arglist->status() != ArgList::EVALUATED)
    {
      arglist->evaluate(env);
    }

    if (func->printHandling() == BuiltInFunction::SOFT_ON) {
	Evaluator::enableResultPrinting(true);
    }

#ifdef Win32
    // This is an inlined version of Rwin_fpreset (src/gnuwin/extra.c)
    // and resets the precision, rounding and exception modes of a
    // ix86 fpu.
    // It gets called prior to every builtin function, just in case a badly
    // behaved DLL has changed the fpu control word.
    __asm__ ( "fninit" );
#endif

    return func->invoke(this, env, arglist);
}

RObject* Expression::invokeClosure(const Closure* func,
                                   Environment* calling_env,
                                   ArgList* arglist,
                                   const Frame* method_bindings) const
{
  return GCStackFrameBoundary::withStackFrameBoundary(
      [=]() { return invokeClosureImpl(func, calling_env, arglist,
                                       method_bindings); });
}

RObject* Expression::invokeClosureImpl(const Closure* func,
                                       Environment* calling_env,
                                       ArgList* arglist,
                                       const Frame* method_bindings) const
{
  if (arglist->status() != ArgList::PROMISED)
    arglist->wrapInPromises(calling_env);

    Environment* execution_env = func->createExecutionEnv();

    // Perform argument matching:
    {
        ClosureContext context(this, calling_env, func,
                               func->environment(), arglist->list());
	func->matcher()->match(execution_env, arglist);
    }

    // If this is a method call, merge in supplementary bindings and modify
    // calling_env.
    if (method_bindings) {
      importMethodBindings(method_bindings, execution_env->frame());
      calling_env = getMethodCallingEnv();
    }

    RObject* result;
    {
      // Evaluate the function.
      ClosureContext context(this, calling_env, func,
                             execution_env, arglist->list());
      result = func->execute(execution_env);
    }

    Environment::monitorLeaks(result);
    execution_env->maybeDetachFrame();

    return result;
}

void Expression::importMethodBindings(const Frame* method_bindings,
                                      Frame* newframe)
{
  method_bindings->visitBindings([&](const Frame::Binding* binding) {
      const Symbol* sym = binding->symbol();
      if (!newframe->binding(sym)) {
        newframe->importBinding(binding);
      }
    });
}

Environment* Expression::getMethodCallingEnv() {
  FunctionContext* fctxt = FunctionContext::innermost();
  while (fctxt && fctxt->function()->sexptype() == SPECIALSXP)
      fctxt = FunctionContext::innermost(fctxt->nextOut());
  return (fctxt ? fctxt->callEnvironment() : Environment::global());

}

const char* Expression::typeName() const
{
    return staticTypeName();
}

// ***** C interface *****

SEXP Rf_currentExpression()
{
    return R_CurrentExpr;
}

SEXP Rf_lcons(SEXP cr, SEXP tl)
{
    GCStackRoot<> crr(cr);
    GCStackRoot<PairList> tlr(SEXP_downcast<PairList*>(tl));
    return new Expression(crr, tlr);
}

void Rf_setCurrentExpression(SEXP e)
{
    R_CurrentExpr = e;
}
