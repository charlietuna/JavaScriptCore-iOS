/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "StringConstructor.h"

#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "PrototypeFunction.h"
#include "StringPrototype.h"

namespace JSC {

static NEVER_INLINE JSValue stringFromCharCodeSlowCase(ExecState* exec, const ArgList& args)
{
    unsigned length = args.size();
    UChar* buf;
    PassRefPtr<UStringImpl> impl = UStringImpl::createUninitialized(length, buf);
    for (unsigned i = 0; i < length; ++i)
        buf[i] = static_cast<UChar>(args.at(i).toUInt32(exec));
    return jsString(exec, impl);
}

static JSValue JSC_HOST_CALL stringFromCharCode(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (LIKELY(args.size() == 1))
        return jsSingleCharacterString(exec, args.at(0).toUInt32(exec));
    return stringFromCharCodeSlowCase(exec, args);
}

ASSERT_CLASS_FITS_IN_CELL(StringConstructor);

StringConstructor::StringConstructor(ExecState* exec, NonNullPassRefPtr<Structure> structure, Structure* prototypeFunctionStructure, StringPrototype* stringPrototype)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, stringPrototype->classInfo()->className))
{
    // ECMA 15.5.3.1 String.prototype
    putDirectWithoutTransition(exec->propertyNames().prototype, stringPrototype, ReadOnly | DontEnum | DontDelete);

    // ECMA 15.5.3.2 fromCharCode()
#if ENABLE(JIT) && ENABLE(JIT_OPTIMIZE_NATIVE_CALL)
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().fromCharCode, exec->globalData().getThunk(fromCharCodeThunkGenerator), stringFromCharCode), DontEnum);
#else
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().fromCharCode, stringFromCharCode), DontEnum);
#endif
    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);
}

// ECMA 15.5.2
static JSObject* constructWithStringConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    if (args.isEmpty())
        return new (exec) StringObject(exec, exec->lexicalGlobalObject()->stringObjectStructure());
    return new (exec) StringObject(exec, exec->lexicalGlobalObject()->stringObjectStructure(), args.at(0).toString(exec));
}

ConstructType StringConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithStringConstructor;
    return ConstructTypeHost;
}

// ECMA 15.5.1
static JSValue JSC_HOST_CALL callStringConstructor(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (args.isEmpty())
        return jsEmptyString(exec);
    return jsString(exec, args.at(0).toString(exec));
}

CallType StringConstructor::getCallData(CallData& callData)
{
    callData.native.function = callStringConstructor;
    return CallTypeHost;
}

} // namespace JSC
