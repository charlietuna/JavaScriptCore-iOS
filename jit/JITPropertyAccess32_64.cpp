/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(JSVALUE32_64)

#include "JIT.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
#include "Interpreter.h"
#include "LinkBuffer.h"
#include "RepatchBuffer.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {
    
void JIT::emit_op_put_by_index(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_put_by_index);
    stubCall.addArgument(base);
    stubCall.addArgument(Imm32(property));
    stubCall.addArgument(value);
    stubCall.call();
}

void JIT::emit_op_put_getter(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned function = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_put_getter);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(property)));
    stubCall.addArgument(function);
    stubCall.call();
}

void JIT::emit_op_put_setter(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned function = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_put_setter);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(property)));
    stubCall.addArgument(function);
    stubCall.call();
}

void JIT::emit_op_del_by_id(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_del_by_id);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(property)));
    stubCall.call(dst);
}


#if !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

// Treat these as nops - the call will be handed as a regular get_by_id/op_call pair.
void JIT::emit_op_method_check(Instruction*) {}
void JIT::emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&) { ASSERT_NOT_REACHED(); }
#if ENABLE(JIT_OPTIMIZE_METHOD_CALLS)
#error "JIT_OPTIMIZE_METHOD_CALLS requires JIT_OPTIMIZE_PROPERTY_ACCESS"
#endif

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.call(dst);
}

void JIT::emitSlow_op_get_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_put_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.addArgument(value);
    stubCall.call();
}

void JIT::emitSlow_op_put_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_get_by_id_generic);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.call(dst);
    
    m_propertyAccessInstructionIndex++;
}

void JIT::emitSlow_op_get_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    m_propertyAccessInstructionIndex++;
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int ident = currentInstruction[2].u.operand;
    int value = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_put_by_id_generic);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.addArgument(value);
    stubCall.call();
    
    m_propertyAccessInstructionIndex++;
}

void JIT::emitSlow_op_put_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    m_propertyAccessInstructionIndex++;
    ASSERT_NOT_REACHED();
}

#else // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#if ENABLE(JIT_OPTIMIZE_METHOD_CALLS)

void JIT::emit_op_method_check(Instruction* currentInstruction)
{
    // Assert that the following instruction is a get_by_id.
    ASSERT(m_interpreter->getOpcodeID((currentInstruction + OPCODE_LENGTH(op_method_check))->u.opcode) == op_get_by_id);
    
    currentInstruction += OPCODE_LENGTH(op_method_check);
    
    // Do the method check - check the object & its prototype's structure inline (this is the common case).
    m_methodCallCompilationInfo.append(MethodCallCompilationInfo(m_propertyAccessInstructionIndex));
    MethodCallCompilationInfo& info = m_methodCallCompilationInfo.last();
    
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    
    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    
    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceMethodCheck);
    
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), info.structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    DataLabelPtr protoStructureToCompare, protoObj = moveWithPatch(ImmPtr(0), regT2);
    Jump protoStructureCheck = branchPtrWithPatch(NotEqual, Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), protoStructureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    
    // This will be relinked to load the function without doing a load.
    DataLabelPtr putFunction = moveWithPatch(ImmPtr(0), regT0);
    
    END_UNINTERRUPTED_SEQUENCE(sequenceMethodCheck);
    
    move(Imm32(JSValue::CellTag), regT1);
    Jump match = jump();
    
    ASSERT(differenceBetween(info.structureToCompare, protoObj) == patchOffsetMethodCheckProtoObj);
    ASSERT(differenceBetween(info.structureToCompare, protoStructureToCompare) == patchOffsetMethodCheckProtoStruct);
    ASSERT(differenceBetween(info.structureToCompare, putFunction) == patchOffsetMethodCheckPutFunction);
    
    // Link the failure cases here.
    structureCheck.link(this);
    protoStructureCheck.link(this);
    
    // Do a regular(ish) get_by_id (the slow case will be link to
    // cti_op_get_by_id_method_check instead of cti_op_get_by_id.
    compileGetByIdHotPath();
    
    match.link(this);
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_method_check), dst, regT1, regT0);
    
    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeIndex += OPCODE_LENGTH(op_get_by_id);
}

void JIT::emitSlow_op_method_check(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    currentInstruction += OPCODE_LENGTH(op_method_check);
    
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;
    
    compileGetByIdSlowCase(dst, base, &(m_codeBlock->identifier(ident)), iter, true);
    
    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeIndex += OPCODE_LENGTH(op_get_by_id);
}

#else //!ENABLE(JIT_OPTIMIZE_METHOD_CALLS)

// Treat these as nops - the call will be handed as a regular get_by_id/op_call pair.
void JIT::emit_op_method_check(Instruction*) {}
void JIT::emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&) { ASSERT_NOT_REACHED(); }

#endif

PassRefPtr<NativeExecutable> JIT::stringGetByValStubGenerator(JSGlobalData* globalData, ExecutablePool* pool)
{
    JSInterfaceJIT jit;
    JumpList failures;
    failures.append(jit.branchPtr(NotEqual, Address(regT0), ImmPtr(globalData->jsStringVPtr)));
    failures.append(jit.branchTest32(NonZero, Address(regT0, OBJECT_OFFSETOF(JSString, m_fiberCount))));
    
    // Load string length to regT1, and start the process of loading the data pointer into regT0
    jit.load32(Address(regT0, ThunkHelpers::jsStringLengthOffset()), regT1);
    jit.loadPtr(Address(regT0, ThunkHelpers::jsStringValueOffset()), regT0);
    jit.loadPtr(Address(regT0, ThunkHelpers::stringImplDataOffset()), regT0);
    
    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    failures.append(jit.branch32(AboveOrEqual, regT2, regT1));
    
    // Load the character
    jit.load16(BaseIndex(regT0, regT2, TimesTwo, 0), regT0);
    
    failures.append(jit.branch32(AboveOrEqual, regT0, Imm32(0x100)));
    jit.move(ImmPtr(globalData->smallStrings.singleCharacterStrings()), regT1);
    jit.loadPtr(BaseIndex(regT1, regT0, ScalePtr, 0), regT0);
    jit.move(Imm32(JSValue::CellTag), regT1); // We null check regT0 on return so this is safe
    jit.ret();

    failures.link(&jit);
    jit.move(Imm32(0), regT0);
    jit.ret();
    
    LinkBuffer patchBuffer(&jit, pool, 0);
    return adoptRef(new NativeExecutable(patchBuffer.finalizeCode()));
}

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    emitLoad2(base, regT1, regT0, property, regT3, regT2);
    
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));
    
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT3);
    addSlowCase(branch32(AboveOrEqual, regT2, Address(regT0, OBJECT_OFFSETOF(JSArray, m_vectorLength))));
    
    load32(BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4), regT1); // tag
    load32(BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), regT0); // payload
    addSlowCase(branch32(Equal, regT1, Imm32(JSValue::EmptyValueTag)));
    
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_by_val), dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check

    Jump nonCell = jump();
    linkSlowCase(iter); // base array check
    Jump notString = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr));
    emitNakedCall(m_globalData->getThunk(stringGetByValStubGenerator)->generatedJITCode().addressForCall());
    Jump failed = branchTestPtr(Zero, regT0);
    emitStore(dst, regT1, regT0);
    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));
    failed.link(this);
    notString.link(this);
    nonCell.link(this);

    linkSlowCase(iter); // vector length check
    linkSlowCase(iter); // empty value
    
    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.call(dst);
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;
    
    emitLoad2(base, regT1, regT0, property, regT3, regT2);
    
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));
    addSlowCase(branch32(AboveOrEqual, regT2, Address(regT0, OBJECT_OFFSETOF(JSArray, m_vectorLength))));
    
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT3);
    
    Jump empty = branch32(Equal, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4), Imm32(JSValue::EmptyValueTag));
    
    Label storeResult(this);
    emitLoad(value, regT1, regT0);
    store32(regT0, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]))); // payload
    store32(regT1, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4)); // tag
    Jump end = jump();
    
    empty.link(this);
    add32(Imm32(1), Address(regT3, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
    branch32(Below, regT2, Address(regT3, OBJECT_OFFSETOF(ArrayStorage, m_length))).linkTo(storeResult, this);
    
    add32(Imm32(1), regT2, regT0);
    store32(regT0, Address(regT3, OBJECT_OFFSETOF(ArrayStorage, m_length)));
    jump().linkTo(storeResult, this);
    
    end.link(this);
}

void JIT::emitSlow_op_put_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;
    
    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // base not array check
    linkSlowCase(iter); // in vector check
    
    JITStubCall stubPutByValCall(this, cti_op_put_by_val);
    stubPutByValCall.addArgument(base);
    stubPutByValCall.addArgument(property);
    stubPutByValCall.addArgument(value);
    stubPutByValCall.call();
}

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    
    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    compileGetByIdHotPath();
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_by_id), dst, regT1, regT0);
}

void JIT::compileGetByIdHotPath()
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.
    
    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceGetByIdHotPath);
    
    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;
    m_propertyAccessInstructionIndex++;
    
    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetGetByIdStructure);
    ASSERT(differenceBetween(hotPathBegin, structureCheck) == patchOffsetGetByIdBranchToSlowCase);
    
    Label externalLoad = loadPtrWithPatchToLEA(Address(regT0, OBJECT_OFFSETOF(JSObject, m_externalStorage)), regT2);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetGetByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthGetByIdExternalLoad);
    
    DataLabel32 displacementLabel1 = loadPtrWithAddressOffsetPatch(Address(regT2, patchGetByIdDefaultOffset), regT0); // payload
    ASSERT(differenceBetween(hotPathBegin, displacementLabel1) == patchOffsetGetByIdPropertyMapOffset1);
    DataLabel32 displacementLabel2 = loadPtrWithAddressOffsetPatch(Address(regT2, patchGetByIdDefaultOffset), regT1); // tag
    ASSERT(differenceBetween(hotPathBegin, displacementLabel2) == patchOffsetGetByIdPropertyMapOffset2);
    
    Label putResult(this);
    ASSERT(differenceBetween(hotPathBegin, putResult) == patchOffsetGetByIdPutResult);
    
    END_UNINTERRUPTED_SEQUENCE(sequenceGetByIdHotPath);
}

void JIT::emitSlow_op_get_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;
    
    compileGetByIdSlowCase(dst, base, &(m_codeBlock->identifier(ident)), iter);
}

void JIT::compileGetByIdSlowCase(int dst, int base, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, bool isMethodCheck)
{
    // As for the hot path of get_by_id, above, we ensure that we can use an architecture specific offset
    // so that we only need track one pointer into the slow case code - we track a pointer to the location
    // of the call (which we can use to look up the patch information), but should a array-length or
    // prototype access trampoline fail we want to bail out back to here.  To do so we can subtract back
    // the distance from the call to the head of the slow case.
    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);
    
    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceGetByIdSlowCase);
    
#ifndef NDEBUG
    Label coldPathBegin(this);
#endif
    JITStubCall stubCall(this, isMethodCheck ? cti_op_get_by_id_method_check : cti_op_get_by_id);
    stubCall.addArgument(regT1, regT0);
    stubCall.addArgument(ImmPtr(ident));
    Call call = stubCall.call(dst);
    
    END_UNINTERRUPTED_SEQUENCE(sequenceGetByIdSlowCase);
    
    ASSERT(differenceBetween(coldPathBegin, call) == patchOffsetGetByIdSlowCaseCall);
    
    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].callReturnLocation = call;
    m_propertyAccessInstructionIndex++;
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.
    
    int base = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;
    
    emitLoad2(base, regT1, regT0, value, regT3, regT2);
    
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    
    BEGIN_UNINTERRUPTED_SEQUENCE(sequencePutById);
    
    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;
    m_propertyAccessInstructionIndex++;
    
    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    DataLabelPtr structureToCompare;
    addSlowCase(branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure))));
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetPutByIdStructure);
    
    // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
    Label externalLoad = loadPtrWithPatchToLEA(Address(regT0, OBJECT_OFFSETOF(JSObject, m_externalStorage)), regT0);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetPutByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthPutByIdExternalLoad);
    
    DataLabel32 displacementLabel1 = storePtrWithAddressOffsetPatch(regT2, Address(regT0, patchGetByIdDefaultOffset)); // payload
    DataLabel32 displacementLabel2 = storePtrWithAddressOffsetPatch(regT3, Address(regT0, patchGetByIdDefaultOffset)); // tag
    
    END_UNINTERRUPTED_SEQUENCE(sequencePutById);
    
    ASSERT(differenceBetween(hotPathBegin, displacementLabel1) == patchOffsetPutByIdPropertyMapOffset1);
    ASSERT(differenceBetween(hotPathBegin, displacementLabel2) == patchOffsetPutByIdPropertyMapOffset2);
}

void JIT::emitSlow_op_put_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int base = currentInstruction[1].u.operand;
    int ident = currentInstruction[2].u.operand;
    int direct = currentInstruction[8].u.operand;

    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);
    
    JITStubCall stubCall(this, direct ? cti_op_put_by_id_direct : cti_op_put_by_id);
    stubCall.addArgument(regT1, regT0);
    stubCall.addArgument(ImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.addArgument(regT3, regT2); 
    Call call = stubCall.call();
    
    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].callReturnLocation = call;
    m_propertyAccessInstructionIndex++;
}

// Compile a store into an object's property storage.  May overwrite base.
void JIT::compilePutDirectOffset(RegisterID base, RegisterID valueTag, RegisterID valuePayload, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset;
    if (structure->isUsingInlineStorage())
        offset += OBJECT_OFFSETOF(JSObject, m_inlineStorage) /  sizeof(Register);
    else
        loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    emitStore(offset, valueTag, valuePayload, base);
}

// Compile a load from an object's property storage.  May overwrite base.
void JIT::compileGetDirectOffset(RegisterID base, RegisterID resultTag, RegisterID resultPayload, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset;
    if (structure->isUsingInlineStorage())
        offset += OBJECT_OFFSETOF(JSObject, m_inlineStorage) / sizeof(Register);
    else
        loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    emitLoad(offset, resultTag, resultPayload, base);
}

void JIT::compileGetDirectOffset(JSObject* base, RegisterID temp, RegisterID resultTag, RegisterID resultPayload, size_t cachedOffset)
{
    if (base->isUsingInlineStorage()) {
        load32(reinterpret_cast<char*>(&base->m_inlineStorage[cachedOffset]), resultPayload);
        load32(reinterpret_cast<char*>(&base->m_inlineStorage[cachedOffset]) + 4, resultTag);
        return;
    }
    
    size_t offset = cachedOffset * sizeof(JSValue);
    
    PropertyStorage* protoPropertyStorage = &base->m_externalStorage;
    loadPtr(static_cast<void*>(protoPropertyStorage), temp);
    load32(Address(temp, offset), resultPayload);
    load32(Address(temp, offset + 4), resultTag);
}

void JIT::testPrototype(Structure* structure, JumpList& failureCases)
{
    if (structure->m_prototype.isNull())
        return;
    
    failureCases.append(branchPtr(NotEqual, AbsoluteAddress(&asCell(structure->m_prototype)->m_structure), ImmPtr(asCell(structure->m_prototype)->m_structure)));
}

void JIT::privateCompilePutByIdTransition(StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ReturnAddressPtr returnAddress, bool direct)
{
    // It is assumed that regT0 contains the basePayload and regT1 contains the baseTag.  The value can be found on the stack.
    
    JumpList failureCases;
    failureCases.append(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)));
    failureCases.append(branchPtr(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), ImmPtr(oldStructure)));
    testPrototype(oldStructure, failureCases);
    
    if (!direct) {
        // Verify that nothing in the prototype chain has a setter for this property. 
        for (RefPtr<Structure>* it = chain->head(); *it; ++it)
            testPrototype(it->get(), failureCases);
    }

    // Reallocate property storage if needed.
    Call callTarget;
    bool willNeedStorageRealloc = oldStructure->propertyStorageCapacity() != newStructure->propertyStorageCapacity();
    if (willNeedStorageRealloc) {
        // This trampoline was called to like a JIT stub; before we can can call again we need to
        // remove the return address from the stack, to prevent the stack from becoming misaligned.
        preserveReturnAddressAfterCall(regT3);
        
        JITStubCall stubCall(this, cti_op_put_by_id_transition_realloc);
        stubCall.skipArgument(); // base
        stubCall.skipArgument(); // ident
        stubCall.skipArgument(); // value
        stubCall.addArgument(Imm32(oldStructure->propertyStorageCapacity()));
        stubCall.addArgument(Imm32(newStructure->propertyStorageCapacity()));
        stubCall.call(regT0);
        
        restoreReturnAddressBeforeReturn(regT3);
    }
    
    sub32(Imm32(1), AbsoluteAddress(oldStructure->addressOfCount()));
    add32(Imm32(1), AbsoluteAddress(newStructure->addressOfCount()));
    storePtr(ImmPtr(newStructure), Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)));
    
    load32(Address(stackPointerRegister, OBJECT_OFFSETOF(struct JITStackFrame, args[2]) + sizeof(void*)), regT3);
    load32(Address(stackPointerRegister, OBJECT_OFFSETOF(struct JITStackFrame, args[2]) + sizeof(void*) + 4), regT2);
    
    // Write the value
    compilePutDirectOffset(regT0, regT2, regT3, newStructure, cachedOffset);
    
    ret();
    
    ASSERT(!failureCases.empty());
    failureCases.link(this);
    restoreArgumentReferenceForTrampoline();
    Call failureCall = tailRecursiveCall();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    
    patchBuffer.link(failureCall, FunctionPtr(direct ? cti_op_put_by_id_direct_fail : cti_op_put_by_id_fail));
    
    if (willNeedStorageRealloc) {
        ASSERT(m_calls.size() == 1);
        patchBuffer.link(m_calls[0].from, FunctionPtr(cti_op_put_by_id_transition_realloc));
    }
    
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relinkCallerToTrampoline(returnAddress, entryLabel);
}

void JIT::patchGetByIdSelf(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);
    
    // We don't want to patch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to JITStubs::cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_self_fail));
    
    int offset = sizeof(JSValue) * cachedOffset;
    
    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        repatchBuffer.repatchLoadPtrToLEA(stubInfo->hotPathBegin.instructionAtOffset(patchOffsetGetByIdExternalLoad));
    
    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset1), offset); // payload
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset2), offset + 4); // tag
}

void JIT::patchMethodCallProto(CodeBlock* codeBlock, MethodCallLinkInfo& methodCallLinkInfo, JSFunction* callee, Structure* structure, JSObject* proto, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);
    
    ASSERT(!methodCallLinkInfo.cachedStructure);
    methodCallLinkInfo.cachedStructure = structure;
    structure->ref();
    
    Structure* prototypeStructure = proto->structure();
    methodCallLinkInfo.cachedPrototypeStructure = prototypeStructure;
    prototypeStructure->ref();
    
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel, structure);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoObj), proto);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoStruct), prototypeStructure);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckPutFunction), callee);
    
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id));
}

void JIT::patchPutByIdReplace(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress, bool direct)
{
    RepatchBuffer repatchBuffer(codeBlock);
    
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(direct ? cti_op_put_by_id_direct_generic : cti_op_put_by_id_generic));
    
    int offset = sizeof(JSValue) * cachedOffset;
    
    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        repatchBuffer.repatchLoadPtrToLEA(stubInfo->hotPathBegin.instructionAtOffset(patchOffsetPutByIdExternalLoad));
    
    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset1), offset); // payload
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset2), offset + 4); // tag
}

void JIT::privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);
    
    // regT0 holds a JSCell*
    
    // Check for array
    Jump failureCases1 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr));
    
    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT2);
    load32(Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_length)), regT2);
    
    Jump failureCases2 = branch32(Above, regT2, Imm32(INT_MAX));
    move(regT2, regT0);
    move(Imm32(JSValue::Int32Tag), regT1);
    Jump success = jump();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    
    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);
    
    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));
    
    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;
    
    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
    
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_array_fail));
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    
    Jump failureCases1 = checkStructure(regT0, structure);
    
    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if CPU(X86_64)
    move(ImmPtr(prototypeStructure), regT3);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif
    bool needsStubLink = false;
    // Checks out okay!
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT2, regT2, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(ImmPtr(protoObject));
        stubCall.addArgument(ImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(ImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);
    
    Jump success = jump();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    
    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);
    
    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;
    
    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
    
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}


void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset)
{
    // regT0 holds a JSCell*
    Jump failureCase = checkStructure(regT0, structure);
    bool needsStubLink = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        if (!structure->isUsingInlineStorage()) {
            move(regT0, regT1);
            compileGetDirectOffset(regT1, regT2, regT1, structure, cachedOffset);
        } else
            compileGetDirectOffset(regT0, regT2, regT1, structure, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(regT0);
        stubCall.addArgument(ImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(ImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(regT0, regT1, regT0, structure, cachedOffset);

    Jump success = jump();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }    
    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = polymorphicStructures->list[currentIndex - 1].stubRoutine;
    if (!lastProtoBegin)
        lastProtoBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    
    patchBuffer.link(failureCase, lastProtoBegin);
    
    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    
    structure->ref();
    polymorphicStructures->list[currentIndex].set(entryLabel, structure);
    
    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdProtoList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, Structure* prototypeStructure, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    
    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(regT0, structure);
    
    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if CPU(X86_64)
    move(ImmPtr(prototypeStructure), regT3);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif
    
    bool needsStubLink = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT2, regT2, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(ImmPtr(protoObject));
        stubCall.addArgument(ImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(ImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);
    
    Jump success = jump();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }
    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;
    patchBuffer.link(failureCases1, lastProtoBegin);
    patchBuffer.link(failureCases2, lastProtoBegin);
    
    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));
    
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    
    structure->ref();
    prototypeStructure->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, prototypeStructure);
    
    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdChainList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, StructureChain* chain, size_t count, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    ASSERT(count);
    
    JumpList bucketsOfFail;
    
    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(regT0, structure));
    
    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();
        
        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if CPU(X86_64)
        move(ImmPtr(currStructure), regT3);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);
    
    bool needsStubLink = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT2, regT2, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(ImmPtr(protoObject));
        stubCall.addArgument(ImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(ImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);

    Jump success = jump();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }
    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;
    
    patchBuffer.link(bucketsOfFail, lastProtoBegin);
    
    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));
    
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    
    // Track the stub we have created so that it will be deleted later.
    structure->ref();
    chain->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, chain);
    
    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    ASSERT(count);
    
    JumpList bucketsOfFail;
    
    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(regT0, structure));
    
    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();
        
        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if CPU(X86_64)
        move(ImmPtr(currStructure), regT3);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);
    
    bool needsStubLink = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT2, regT2, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(ImmPtr(protoObject));
        stubCall.addArgument(ImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(ImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(ImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);
    Jump success = jump();
    
    LinkBuffer patchBuffer(this, m_codeBlock->executablePool(), 0);
    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }
    // Use the patch information to link the failure cases back to the original slow case routine.
    patchBuffer.link(bucketsOfFail, stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall));
    
    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));
    
    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;
    
    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
    
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

void JIT::compileGetDirectOffset(RegisterID base, RegisterID resultTag, RegisterID resultPayload, RegisterID structure, RegisterID offset)
{
    ASSERT(sizeof(((Structure*)0)->m_propertyStorageCapacity) == sizeof(int32_t));
    ASSERT(sizeof(JSObject::inlineStorageCapacity) == sizeof(int32_t));
    ASSERT(sizeof(JSValue) == 8);
    
    Jump notUsingInlineStorage = branch32(NotEqual, Address(structure, OBJECT_OFFSETOF(Structure, m_propertyStorageCapacity)), Imm32(JSObject::inlineStorageCapacity));
    loadPtr(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSObject, m_inlineStorage)+OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayload);
    loadPtr(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSObject, m_inlineStorage)+OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTag);
    Jump finishedLoad = jump();
    notUsingInlineStorage.link(this);
    loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    loadPtr(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultPayload);
    loadPtr(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultTag);
    finishedLoad.link(this);
}

void JIT::emit_op_get_by_pname(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    unsigned expected = currentInstruction[4].u.operand;
    unsigned iter = currentInstruction[5].u.operand;
    unsigned i = currentInstruction[6].u.operand;
    
    emitLoad2(property, regT1, regT0, base, regT3, regT2);
    emitJumpSlowCaseIfNotJSCell(property, regT1);
    addSlowCase(branchPtr(NotEqual, regT0, payloadFor(expected)));
    // Property registers are now available as the property is known
    emitJumpSlowCaseIfNotJSCell(base, regT3);
    emitLoadPayload(iter, regT1);
    
    // Test base's structure
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT0);
    addSlowCase(branchPtr(NotEqual, regT0, Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedStructure))));
    load32(addressFor(i), regT3);
    sub32(Imm32(1), regT3);
    addSlowCase(branch32(AboveOrEqual, regT3, Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_numCacheableSlots))));
    compileGetDirectOffset(regT2, regT1, regT0, regT0, regT3);    
    
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_by_pname), dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_pname(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    linkSlowCaseIfNotJSCell(iter, property);
    linkSlowCase(iter);
    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);
    linkSlowCase(iter);
    
    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.call(dst);
}

} // namespace JSC

#endif // ENABLE(JIT)

#endif // ENABLE(JSVALUE32_64)

