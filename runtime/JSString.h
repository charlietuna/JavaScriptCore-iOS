/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef JSString_h
#define JSString_h

#include "CallFrame.h"
#include "CommonIdentifiers.h"
#include "Identifier.h"
#include "JSNumberCell.h"
#include "PropertyDescriptor.h"
#include "PropertySlot.h"
#include "RopeImpl.h"

namespace JSC {

    class JSString;

    JSString* jsEmptyString(JSGlobalData*);
    JSString* jsEmptyString(ExecState*);
    JSString* jsString(JSGlobalData*, const UString&); // returns empty string if passed null string
    JSString* jsString(ExecState*, const UString&); // returns empty string if passed null string

    JSString* jsSingleCharacterString(JSGlobalData*, UChar);
    JSString* jsSingleCharacterString(ExecState*, UChar);
    JSString* jsSingleCharacterSubstring(ExecState*, const UString&, unsigned offset);
    JSString* jsSubstring(JSGlobalData*, const UString&, unsigned offset, unsigned length);
    JSString* jsSubstring(ExecState*, const UString&, unsigned offset, unsigned length);

    // Non-trivial strings are two or more characters long.
    // These functions are faster than just calling jsString.
    JSString* jsNontrivialString(JSGlobalData*, const UString&);
    JSString* jsNontrivialString(ExecState*, const UString&);
    JSString* jsNontrivialString(JSGlobalData*, const char*);
    JSString* jsNontrivialString(ExecState*, const char*);

    // Should be used for strings that are owned by an object that will
    // likely outlive the JSValue this makes, such as the parse tree or a
    // DOM object that contains a UString
    JSString* jsOwnedString(JSGlobalData*, const UString&); 
    JSString* jsOwnedString(ExecState*, const UString&); 

    typedef void (*JSStringFinalizerCallback)(JSString*, void* context);
    JSString* jsStringWithFinalizer(ExecState*, const UString&, JSStringFinalizerCallback callback, void* context);

    class JS_EXPORTCLASS JSString : public JSCell {
    public:
        friend class JIT;
        friend class JSGlobalData;
        friend class SpecializedThunkJIT;
        friend struct ThunkHelpers;

        class RopeBuilder {
        public:
            RopeBuilder(unsigned fiberCount)
                : m_index(0)
                , m_rope(RopeImpl::tryCreateUninitialized(fiberCount))
            {
            }

            bool isOutOfMemory() { return !m_rope; }

            void append(RopeImpl::Fiber& fiber)
            {
                ASSERT(m_rope);
                m_rope->initializeFiber(m_index, fiber);
            }
            void append(const UString& string)
            {
                ASSERT(m_rope);
                m_rope->initializeFiber(m_index, string.rep());
            }
            void append(JSString* jsString)
            {
                if (jsString->isRope()) {
                    for (unsigned i = 0; i < jsString->m_fiberCount; ++i)
                        append(jsString->m_other.m_fibers[i]);
                } else
                    append(jsString->string());
            }

            PassRefPtr<RopeImpl> release()
            {
                ASSERT(m_index == m_rope->fiberCount());
                return m_rope.release();
            }

            unsigned length() { return m_rope->length(); }

        private:
            unsigned m_index;
            RefPtr<RopeImpl> m_rope;
        };

        class RopeIterator {
            public:
                RopeIterator() { }

                RopeIterator(RopeImpl::Fiber* fibers, size_t fiberCount)
                {
                    ASSERT(fiberCount);
                    m_workQueue.append(WorkItem(fibers, fiberCount));
                    skipRopes();
                }

                RopeIterator& operator++()
                {
                    WorkItem& item = m_workQueue.last();
                    ASSERT(!RopeImpl::isRope(item.fibers[item.i]));
                    if (++item.i == item.fiberCount)
                        m_workQueue.removeLast();
                    skipRopes();
                    return *this;
                }

                UStringImpl* operator*()
                {
                    WorkItem& item = m_workQueue.last();
                    RopeImpl::Fiber fiber = item.fibers[item.i];
                    ASSERT(!RopeImpl::isRope(fiber));
                    return static_cast<UStringImpl*>(fiber);
                }

                bool operator!=(const RopeIterator& other) const
                {
                    return m_workQueue != other.m_workQueue;
                }

            private:
                struct WorkItem {
                    WorkItem(RopeImpl::Fiber* fibers, size_t fiberCount)
                        : fibers(fibers)
                        , fiberCount(fiberCount)
                        , i(0)
                    {
                    }

                    bool operator!=(const WorkItem& other) const
                    {
                        return fibers != other.fibers || fiberCount != other.fiberCount || i != other.i;
                    }

                    RopeImpl::Fiber* fibers;
                    size_t fiberCount;
                    size_t i;
                };

                void skipRopes()
                {
                    if (m_workQueue.isEmpty())
                        return;

                    while (1) {
                        WorkItem& item = m_workQueue.last();
                        RopeImpl::Fiber fiber = item.fibers[item.i];
                        if (!RopeImpl::isRope(fiber))
                            break;
                        RopeImpl* rope = static_cast<RopeImpl*>(fiber);
                        if (++item.i == item.fiberCount)
                            m_workQueue.removeLast();
                        m_workQueue.append(WorkItem(rope->fibers(), rope->fiberCount()));
                    }
                }

                Vector<WorkItem, 16> m_workQueue;
        };

        ALWAYS_INLINE JSString(JSGlobalData* globalData, const UString& value)
            : JSCell(globalData->stringStructure.get())
            , m_length(value.size())
            , m_value(value)
            , m_fiberCount(0)
        {
            ASSERT(!m_value.isNull());
            Heap::heap(this)->reportExtraMemoryCost(value.cost());
        }

        enum HasOtherOwnerType { HasOtherOwner };
        JSString(JSGlobalData* globalData, const UString& value, HasOtherOwnerType)
            : JSCell(globalData->stringStructure.get())
            , m_length(value.size())
            , m_value(value)
            , m_fiberCount(0)
        {
            ASSERT(!m_value.isNull());
        }
        JSString(JSGlobalData* globalData, PassRefPtr<UStringImpl> value, HasOtherOwnerType)
            : JSCell(globalData->stringStructure.get())
            , m_length(value->length())
            , m_value(value)
            , m_fiberCount(0)
        {
            ASSERT(!m_value.isNull());
        }
        JSString(JSGlobalData* globalData, PassRefPtr<RopeImpl> rope)
            : JSCell(globalData->stringStructure.get())
            , m_length(rope->length())
            , m_fiberCount(1)
        {
            m_other.m_fibers[0] = rope.releaseRef();
        }
        // This constructor constructs a new string by concatenating s1 & s2.
        // This should only be called with fiberCount <= 3.
        JSString(JSGlobalData* globalData, unsigned fiberCount, JSString* s1, JSString* s2)
            : JSCell(globalData->stringStructure.get())
            , m_length(s1->length() + s2->length())
            , m_fiberCount(fiberCount)
        {
            ASSERT(fiberCount <= s_maxInternalRopeLength);
            unsigned index = 0;
            appendStringInConstruct(index, s1);
            appendStringInConstruct(index, s2);
            ASSERT(fiberCount == index);
        }
        // This constructor constructs a new string by concatenating s1 & s2.
        // This should only be called with fiberCount <= 3.
        JSString(JSGlobalData* globalData, unsigned fiberCount, JSString* s1, const UString& u2)
            : JSCell(globalData->stringStructure.get())
            , m_length(s1->length() + u2.size())
            , m_fiberCount(fiberCount)
        {
            ASSERT(fiberCount <= s_maxInternalRopeLength);
            unsigned index = 0;
            appendStringInConstruct(index, s1);
            appendStringInConstruct(index, u2);
            ASSERT(fiberCount == index);
        }
        // This constructor constructs a new string by concatenating s1 & s2.
        // This should only be called with fiberCount <= 3.
        JSString(JSGlobalData* globalData, unsigned fiberCount, const UString& u1, JSString* s2)
            : JSCell(globalData->stringStructure.get())
            , m_length(u1.size() + s2->length())
            , m_fiberCount(fiberCount)
        {
            ASSERT(fiberCount <= s_maxInternalRopeLength);
            unsigned index = 0;
            appendStringInConstruct(index, u1);
            appendStringInConstruct(index, s2);
            ASSERT(fiberCount == index);
        }
        // This constructor constructs a new string by concatenating v1, v2 & v3.
        // This should only be called with fiberCount <= 3 ... which since every
        // value must require a fiberCount of at least one implies that the length
        // for each value must be exactly 1!
        JSString(ExecState* exec, JSValue v1, JSValue v2, JSValue v3)
            : JSCell(exec->globalData().stringStructure.get())
            , m_length(0)
            , m_fiberCount(s_maxInternalRopeLength)
        {
            unsigned index = 0;
            appendValueInConstructAndIncrementLength(exec, index, v1);
            appendValueInConstructAndIncrementLength(exec, index, v2);
            appendValueInConstructAndIncrementLength(exec, index, v3);
            ASSERT(index == s_maxInternalRopeLength);
        }

        // This constructor constructs a new string by concatenating u1 & u2.
        JSString(JSGlobalData* globalData, const UString& u1, const UString& u2)
            : JSCell(globalData->stringStructure.get())
            , m_length(u1.size() + u2.size())
            , m_fiberCount(2)
        {
            unsigned index = 0;
            appendStringInConstruct(index, u1);
            appendStringInConstruct(index, u2);
            ASSERT(index <= s_maxInternalRopeLength);
        }

        // This constructor constructs a new string by concatenating u1, u2 & u3.
        JSString(JSGlobalData* globalData, const UString& u1, const UString& u2, const UString& u3)
            : JSCell(globalData->stringStructure.get())
            , m_length(u1.size() + u2.size() + u3.size())
            , m_fiberCount(s_maxInternalRopeLength)
        {
            unsigned index = 0;
            appendStringInConstruct(index, u1);
            appendStringInConstruct(index, u2);
            appendStringInConstruct(index, u3);
            ASSERT(index <= s_maxInternalRopeLength);
        }

        JSString(JSGlobalData* globalData, const UString& value, JSStringFinalizerCallback finalizer, void* context)
            : JSCell(globalData->stringStructure.get())
            , m_length(value.size())
            , m_value(value)
            , m_fiberCount(0)
        {
            ASSERT(!m_value.isNull());
            // nasty hack because we can't union non-POD types
            m_other.m_finalizerCallback = finalizer;
            m_other.m_finalizerContext = context;
            Heap::heap(this)->reportExtraMemoryCost(value.cost());
        }

        ~JSString()
        {
            ASSERT(vptr() == JSGlobalData::jsStringVPtr);
            for (unsigned i = 0; i < m_fiberCount; ++i)
                RopeImpl::deref(m_other.m_fibers[i]);

            if (!m_fiberCount && m_other.m_finalizerCallback)
                m_other.m_finalizerCallback(this, m_other.m_finalizerContext);
        }

        const UString& value(ExecState* exec) const
        {
            if (isRope())
                resolveRope(exec);
            return m_value;
        }
        const UString& tryGetValue() const
        {
            if (isRope())
                resolveRope(0);
            return m_value;
        }
        unsigned length() { return m_length; }

        bool getStringPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getStringPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        bool getStringPropertyDescriptor(ExecState*, const Identifier& propertyName, PropertyDescriptor&);

        bool canGetIndex(unsigned i) { return i < m_length; }
        JSString* getIndex(ExecState*, unsigned);
        JSString* getIndexSlowCase(ExecState*, unsigned);

        JSValue replaceCharacter(ExecState*, UChar, const UString& replacement);

        static PassRefPtr<Structure> createStructure(JSValue proto) { return Structure::create(proto, TypeInfo(StringType, OverridesGetOwnPropertySlot | NeedsThisConversion), AnonymousSlotCount); }

    private:
        enum VPtrStealingHackType { VPtrStealingHack };
        JSString(VPtrStealingHackType) 
            : JSCell(0)
            , m_fiberCount(0)
        {
        }

        void resolveRope(ExecState*) const;
        JSString* substringFromRope(ExecState*, unsigned offset, unsigned length);

        void appendStringInConstruct(unsigned& index, const UString& string)
        {
            UStringImpl* impl = string.rep();
            impl->ref();
            m_other.m_fibers[index++] = impl;
        }

        void appendStringInConstruct(unsigned& index, JSString* jsString)
        {
            if (jsString->isRope()) {
                for (unsigned i = 0; i < jsString->m_fiberCount; ++i) {
                    RopeImpl::Fiber fiber = jsString->m_other.m_fibers[i];
                    fiber->ref();
                    m_other.m_fibers[index++] = fiber;
                }
            } else
                appendStringInConstruct(index, jsString->string());
        }

        void appendValueInConstructAndIncrementLength(ExecState* exec, unsigned& index, JSValue v)
        {
            if (v.isString()) {
                ASSERT(asCell(v)->isString());
                JSString* s = static_cast<JSString*>(asCell(v));
                ASSERT(s->size() == 1);
                appendStringInConstruct(index, s);
                m_length += s->length();
            } else {
                UString u(v.toString(exec));
                UStringImpl* impl = u.rep();
                impl->ref();
                m_other.m_fibers[index++] = impl;
                m_length += u.size();
            }
        }

        virtual JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;
        virtual UString toString(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;

        // Actually getPropertySlot, not getOwnPropertySlot (see JSCell).
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);

        static const unsigned s_maxInternalRopeLength = 3;

        // A string is represented either by a UString or a RopeImpl.
        unsigned m_length;
        mutable UString m_value;
        mutable unsigned m_fiberCount;
        // This structure exists to support a temporary workaround for a GC issue.
        struct JSStringFinalizerStruct {
            JSStringFinalizerStruct() : m_finalizerCallback(0) {}
            union {
                mutable RopeImpl::Fiber m_fibers[s_maxInternalRopeLength];
                struct {
                    JSStringFinalizerCallback m_finalizerCallback;
                    void* m_finalizerContext;
                };
            };
        } m_other;

        bool isRope() const { return m_fiberCount; }
        UString& string() { ASSERT(!isRope()); return m_value; }
        unsigned size() { return m_fiberCount ? m_fiberCount : 1; }

        friend JSValue jsString(ExecState* exec, JSString* s1, JSString* s2);
        friend JSValue jsString(ExecState* exec, const UString& u1, JSString* s2);
        friend JSValue jsString(ExecState* exec, JSString* s1, const UString& u2);
        friend JSValue jsString(ExecState* exec, Register* strings, unsigned count);
        friend JSValue jsString(ExecState* exec, JSValue thisValue, const ArgList& args);
        friend JSString* jsStringWithFinalizer(ExecState*, const UString&, JSStringFinalizerCallback callback, void* context);
        friend JSString* jsSubstring(ExecState* exec, JSString* s, unsigned offset, unsigned length);
    };

    JSString* asString(JSValue);

    // When an object is created from a different DLL, MSVC changes vptr to a "local" one right after invoking a constructor,
    // see <http://groups.google.com/group/microsoft.public.vc.language/msg/55cdcefeaf770212>.
    // This breaks isJSString(), and we don't need that hack anyway, so we change vptr back to primary one.
    // The below function must be called by any inline function that invokes a JSString constructor.
#if COMPILER(MSVC) && !defined(BUILDING_JavaScriptCore)
    inline JSString* fixupVPtr(JSGlobalData* globalData, JSString* string) { string->setVPtr(globalData->jsStringVPtr); return string; }
#else
    inline JSString* fixupVPtr(JSGlobalData*, JSString* string) { return string; }
#endif

    inline JSString* asString(JSValue value)
    {
        ASSERT(asCell(value)->isString());
        return static_cast<JSString*>(asCell(value));
    }

    inline JSString* jsEmptyString(JSGlobalData* globalData)
    {
        return globalData->smallStrings.emptyString(globalData);
    }

    inline JSString* jsSingleCharacterString(JSGlobalData* globalData, UChar c)
    {
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, UString(&c, 1)));
    }

    inline JSString* jsSingleCharacterSubstring(ExecState* exec, const UString& s, unsigned offset)
    {
        JSGlobalData* globalData = &exec->globalData();
        ASSERT(offset < static_cast<unsigned>(s.size()));
        UChar c = s.data()[offset];
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, UString(UStringImpl::create(s.rep(), offset, 1))));
    }

    inline JSString* jsNontrivialString(JSGlobalData* globalData, const char* s)
    {
        ASSERT(s);
        ASSERT(s[0]);
        ASSERT(s[1]);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s));
    }

    inline JSString* jsNontrivialString(JSGlobalData* globalData, const UString& s)
    {
        ASSERT(s.size() > 1);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s));
    }

    inline JSString* JSString::getIndex(ExecState* exec, unsigned i)
    {
        ASSERT(canGetIndex(i));
        if (isRope())
            return getIndexSlowCase(exec, i);
        ASSERT(i < m_value.size());
        return jsSingleCharacterSubstring(exec, m_value, i);
    }

    inline JSString* jsString(JSGlobalData* globalData, const UString& s)
    {
        int size = s.size();
        if (!size)
            return globalData->smallStrings.emptyString(globalData);
        if (size == 1) {
            UChar c = s.data()[0];
            if (c <= 0xFF)
                return globalData->smallStrings.singleCharacterString(globalData, c);
        }
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s));
    }

    inline JSString* jsStringWithFinalizer(ExecState* exec, const UString& s, JSStringFinalizerCallback callback, void* context)
    {
        ASSERT(s.size() && (s.size() > 1 || s.data()[0] > 0xFF));
        JSGlobalData* globalData = &exec->globalData();
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s, callback, context));
    }
    
    inline JSString* jsSubstring(ExecState* exec, JSString* s, unsigned offset, unsigned length)
    {
        ASSERT(offset <= static_cast<unsigned>(s->length()));
        ASSERT(length <= static_cast<unsigned>(s->length()));
        ASSERT(offset + length <= static_cast<unsigned>(s->length()));
        JSGlobalData* globalData = &exec->globalData();
        if (!length)
            return globalData->smallStrings.emptyString(globalData);
        if (s->isRope())
            return s->substringFromRope(exec, offset, length);
        return jsSubstring(globalData, s->m_value, offset, length);
    }

    inline JSString* jsSubstring(JSGlobalData* globalData, const UString& s, unsigned offset, unsigned length)
    {
        ASSERT(offset <= static_cast<unsigned>(s.size()));
        ASSERT(length <= static_cast<unsigned>(s.size()));
        ASSERT(offset + length <= static_cast<unsigned>(s.size()));
        if (!length)
            return globalData->smallStrings.emptyString(globalData);
        if (length == 1) {
            UChar c = s.data()[offset];
            if (c <= 0xFF)
                return globalData->smallStrings.singleCharacterString(globalData, c);
        }
        return fixupVPtr(globalData, new (globalData) JSString(globalData, UString(UStringImpl::create(s.rep(), offset, length)), JSString::HasOtherOwner));
    }

    inline JSString* jsOwnedString(JSGlobalData* globalData, const UString& s)
    {
        int size = s.size();
        if (!size)
            return globalData->smallStrings.emptyString(globalData);
        if (size == 1) {
            UChar c = s.data()[0];
            if (c <= 0xFF)
                return globalData->smallStrings.singleCharacterString(globalData, c);
        }
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s, JSString::HasOtherOwner));
    }

    inline JSString* jsEmptyString(ExecState* exec) { return jsEmptyString(&exec->globalData()); }
    inline JSString* jsString(ExecState* exec, const UString& s) { return jsString(&exec->globalData(), s); }
    inline JSString* jsSingleCharacterString(ExecState* exec, UChar c) { return jsSingleCharacterString(&exec->globalData(), c); }
    inline JSString* jsSubstring(ExecState* exec, const UString& s, unsigned offset, unsigned length) { return jsSubstring(&exec->globalData(), s, offset, length); }
    inline JSString* jsNontrivialString(ExecState* exec, const UString& s) { return jsNontrivialString(&exec->globalData(), s); }
    inline JSString* jsNontrivialString(ExecState* exec, const char* s) { return jsNontrivialString(&exec->globalData(), s); }
    inline JSString* jsOwnedString(ExecState* exec, const UString& s) { return jsOwnedString(&exec->globalData(), s); } 

    ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
    {
        if (propertyName == exec->propertyNames().length) {
            slot.setValue(jsNumber(exec, m_length));
            return true;
        }

        bool isStrictUInt32;
        unsigned i = propertyName.toStrictUInt32(&isStrictUInt32);
        if (isStrictUInt32 && i < m_length) {
            slot.setValue(getIndex(exec, i));
            return true;
        }

        return false;
    }
        
    ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
    {
        if (propertyName < m_length) {
            slot.setValue(getIndex(exec, propertyName));
            return true;
        }

        return false;
    }

    inline bool isJSString(JSGlobalData* globalData, JSValue v) { return v.isCell() && v.asCell()->vptr() == globalData->jsStringVPtr; }

    // --- JSValue inlines ----------------------------

    inline UString JSValue::toString(ExecState* exec) const
    {
        if (isString())
            return static_cast<JSString*>(asCell())->value(exec);
        if (isInt32())
            return exec->globalData().numericStrings.add(asInt32());
        if (isDouble())
            return exec->globalData().numericStrings.add(asDouble());
        if (isTrue())
            return "true";
        if (isFalse())
            return "false";
        if (isNull())
            return "null";
        if (isUndefined())
            return "undefined";
        ASSERT(isCell());
        return asCell()->toString(exec);
    }

    inline UString JSValue::toPrimitiveString(ExecState* exec) const
    {
        if (isString())
            return static_cast<JSString*>(asCell())->value(exec);
        if (isInt32())
            return exec->globalData().numericStrings.add(asInt32());
        if (isDouble())
            return exec->globalData().numericStrings.add(asDouble());
        if (isTrue())
            return "true";
        if (isFalse())
            return "false";
        if (isNull())
            return "null";
        if (isUndefined())
            return "undefined";
        ASSERT(isCell());
        return asCell()->toPrimitive(exec, NoPreference).toString(exec);
    }

} // namespace JSC

#endif // JSString_h
