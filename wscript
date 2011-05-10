#! /usr/bin/env python

# Copyright (C) 2009 Kevin Ollivier  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
#
# JavaScriptCore build script for the waf build system

import commands

from settings import *

jscore_excludes = ['jsc.cpp', 'ucptable.cpp']
jscore_excludes.extend(get_excludes(jscore_dir, ['*Brew.cpp', '*CF.cpp', '*Symbian.cpp']))

sources = []

jscore_excludes.extend(get_excludes(jscore_dir, ['*None.cpp']))

if building_on_win32:
    jscore_excludes += ['ExecutableAllocatorPosix.cpp', 'MarkStackPosix.cpp', 'ThreadingPthreads.cpp']
    sources += ['jit/ExecutableAllocatorWin.cpp', 'runtime/MarkStackWin.cpp']
else:
    jscore_excludes.append('JSStringRefBSTR.cpp')
    jscore_excludes.extend(get_excludes(jscore_dir, ['*Win.cpp']))

def generate_jscore_derived_sources():
    # build the derived sources
    js_dir = jscore_dir
    if building_on_win32:
        js_dir = get_output('cygpath --unix "%s"' % js_dir)
    derived_sources_dir = os.path.join(jscore_dir, 'DerivedSources')
    if not os.path.exists(derived_sources_dir):
        os.mkdir(derived_sources_dir)

    olddir = os.getcwd()
    os.chdir(derived_sources_dir)

    command = 'make -f %s/DerivedSources.make JavaScriptCore=%s BUILT_PRODUCTS_DIR=%s all FEATURE_DEFINES="%s"' % (js_dir, js_dir, js_dir, ' '.join(feature_defines))
    os.system(command)
    os.chdir(olddir)

def set_options(opt):
    common_set_options(opt)

def configure(conf):
    common_configure(conf)
    generate_jscore_derived_sources()
    
def build(bld):
    import Options

    full_dirs = get_dirs_for_features(jscore_dir, features=[build_port], dirs=jscore_dirs)

    includes = common_includes + full_dirs
    if sys.platform.startswith('darwin'):
        includes.append(os.path.join(jscore_dir, 'icu'))

    # 1. A simple program
    jscore = bld.new_task_gen(
        features = 'cxx cstaticlib',
        includes = '. .. assembler DerivedSources ForwardingHeaders ' + ' '.join(includes),
        source = sources,
        target = 'jscore',
        uselib = 'WX ICU ' + get_config(),
        uselib_local = '',
        install_path = output_dir)

    jscore.find_sources_in_dirs(full_dirs, excludes = jscore_excludes)  
        
    obj = bld.new_task_gen(
        features = 'cxx cprogram',
        includes = '. .. assembler DerivedSources ForwardingHeaders ' + ' '.join(includes),
        source = 'jsc.cpp',
        target = 'jsc',
        uselib = 'WX ICU ' + get_config(),
        uselib_local = 'jscore',
        install_path = output_dir,
        )
        
    # we'll get an error if exceptions are on because of an unwind error when using __try
    if building_on_win32:
        flags = obj.env.CXXFLAGS
        flags.remove('/EHsc')
        obj.env.CXXFLAGS = flags

    bld.install_files(os.path.join(output_dir, 'JavaScriptCore'), 'API/*.h')
