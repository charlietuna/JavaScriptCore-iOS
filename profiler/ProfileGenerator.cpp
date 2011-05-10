/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "ProfileGenerator.h"

#include "CallFrame.h"
#include "CodeBlock.h"
#include "JSGlobalObject.h"
#include "JSStringRef.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "Profile.h"
#include "Profiler.h"
#include "Tracing.h"

namespace JSC {

static const char* NonJSExecution = "(idle)";

PassRefPtr<ProfileGenerator> ProfileGenerator::create(const UString& title, ExecState* originatingExec, unsigned uid)
{
    return adoptRef(new ProfileGenerator(title, originatingExec, uid));
}

ProfileGenerator::ProfileGenerator(const UString& title, ExecState* originatingExec, unsigned uid)
    : m_originatingGlobalExec(originatingExec ? originatingExec->lexicalGlobalObject()->globalExec() : 0)
    , m_profileGroup(originatingExec ? originatingExec->lexicalGlobalObject()->profileGroup() : 0)
{
    m_profile = Profile::create(title, uid);
    m_currentNode = m_head = m_profile->head();
    if (originatingExec)
        addParentForConsoleStart(originatingExec);
}

void ProfileGenerator::addParentForConsoleStart(ExecState* exec)
{
    int lineNumber;
    intptr_t sourceID;
    UString sourceURL;
    JSValue function;

    exec->interpreter()->retrieveLastCaller(exec, lineNumber, sourceID, sourceURL, function);
    m_currentNode = ProfileNode::create(Profiler::createCallIdentifier(exec, function ? function.toThisObject(exec) : 0, sourceURL, lineNumber), m_head.get(), m_head.get());
    m_head->insertNode(m_currentNode.get());
}

const UString& ProfileGenerator::title() const
{
    return m_profile->title();
}

void ProfileGenerator::willExecute(const CallIdentifier& callIdentifier)
{
    if (JAVASCRIPTCORE_PROFILE_WILL_EXECUTE_ENABLED()) {
        CString name = callIdentifier.m_name.UTF8String();
        CString url = callIdentifier.m_url.UTF8String();
        JAVASCRIPTCORE_PROFILE_WILL_EXECUTE(m_profileGroup, const_cast<char*>(name.data()), const_cast<char*>(url.data()), callIdentifier.m_lineNumber);
    }

    if (!m_originatingGlobalExec)
        return;

    ASSERT_ARG(m_currentNode, m_currentNode);
    m_currentNode = m_currentNode->willExecute(callIdentifier);
}

void ProfileGenerator::didExecute(const CallIdentifier& callIdentifier)
{
    if (JAVASCRIPTCORE_PROFILE_DID_EXECUTE_ENABLED()) {
        CString name = callIdentifier.m_name.UTF8String();
        CString url = callIdentifier.m_url.UTF8String();
        JAVASCRIPTCORE_PROFILE_DID_EXECUTE(m_profileGroup, const_cast<char*>(name.data()), const_cast<char*>(url.data()), callIdentifier.m_lineNumber);
    }

    if (!m_originatingGlobalExec)
        return;

    ASSERT_ARG(m_currentNode, m_currentNode);
    if (m_currentNode->callIdentifier() != callIdentifier) {
        RefPtr<ProfileNode> returningNode = ProfileNode::create(callIdentifier, m_head.get(), m_currentNode.get());
        returningNode->setStartTime(m_currentNode->startTime());
        returningNode->didExecute();
        m_currentNode->insertNode(returningNode.release());
        return;
    }

    m_currentNode = m_currentNode->didExecute();
}

void ProfileGenerator::stopProfiling()
{
    m_profile->forEach(&ProfileNode::stopProfiling);

    removeProfileStart();
    removeProfileEnd();

    ASSERT_ARG(m_currentNode, m_currentNode);

    // Set the current node to the parent, because we are in a call that
    // will not get didExecute call.
    m_currentNode = m_currentNode->parent();

   if (double headSelfTime = m_head->selfTime()) {
        RefPtr<ProfileNode> idleNode = ProfileNode::create(CallIdentifier(NonJSExecution, UString(), 0), m_head.get(), m_head.get());

        idleNode->setTotalTime(headSelfTime);
        idleNode->setSelfTime(headSelfTime);
        idleNode->setVisible(true);

        m_head->setSelfTime(0.0);
        m_head->addChild(idleNode.release());
    }
}

// The console.ProfileGenerator that started this ProfileGenerator will be the first child.
void ProfileGenerator::removeProfileStart()
{
    ProfileNode* currentNode = 0;
    for (ProfileNode* next = m_head.get(); next; next = next->firstChild())
        currentNode = next;

    if (currentNode->callIdentifier().m_name != "profile")
        return;

    // Attribute the time of the node aobut to be removed to the self time of its parent
    currentNode->parent()->setSelfTime(currentNode->parent()->selfTime() + currentNode->totalTime());
    currentNode->parent()->removeChild(currentNode);
}

// The console.ProfileGeneratorEnd that stopped this ProfileGenerator will be the last child.
void ProfileGenerator::removeProfileEnd()
{
    ProfileNode* currentNode = 0;
    for (ProfileNode* next = m_head.get(); next; next = next->lastChild())
        currentNode = next;

    if (currentNode->callIdentifier().m_name != "profileEnd")
        return;

    // Attribute the time of the node aobut to be removed to the self time of its parent
    currentNode->parent()->setSelfTime(currentNode->parent()->selfTime() + currentNode->totalTime());

    ASSERT(currentNode->callIdentifier() == (currentNode->parent()->children()[currentNode->parent()->children().size() - 1])->callIdentifier());
    currentNode->parent()->removeChild(currentNode);
}

} // namespace JSC
