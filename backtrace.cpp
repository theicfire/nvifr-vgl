/*
 * Modified from here: https://gist.github.com/fmela/591333
 *
 * Copyright (c) 2009-2017, Farooq Mela
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cxxabi.h>    // for __cxa_demangle
#include <dlfcn.h>     // for dladdr
#include <execinfo.h>  // for backtrace

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

std::string get_shared_object_short_name(const char *dli_fname) {
  std::string fullname(dli_fname);
  std::string short_name = fullname.substr(fullname.find_last_of('/') + 1);
  if (short_name.size() > 0) {
    return short_name;
  }
  return "???";
}

std::string maybe_get_demangled_function_name(Dl_info info,
                                              char *stackframe_symbol) {
  if (info.dli_sname[0] == '_') {
    char *demangled = NULL;
    int status = -1;
    demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
    if (status == 0) {
      std::string res(demangled);
      free(demangled);
      return res;
    }
  }
  return std::string(info.dli_sname == 0 ? stackframe_symbol : info.dli_sname);
}

// Produces a stack backtrace with demangled function & method names.
std::string get_backtrace(int skip = 1) {
  void *callstack[128];
  const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
  char buf[1024];
  int nFrames = backtrace(callstack, nMaxFrames);
  char **symbols = backtrace_symbols(callstack, nFrames);

  std::ostringstream trace_buf;

  for (int i = skip; i < nFrames; i++) {
    Dl_info info;
    if (dladdr(callstack[i], &info) && info.dli_sname) {
      snprintf(buf, sizeof(buf), "%-3d %s + %zd\n        in '%s' (%p)\n", i,
               maybe_get_demangled_function_name(info, symbols[i]).c_str(),
               (char *)callstack[i] - (char *)info.dli_saddr,
               get_shared_object_short_name(info.dli_fname).c_str(),
               callstack[i]);
    } else {
      snprintf(buf, sizeof(buf), "%-3d ??? (%p)\n", i, callstack[i]);
    }
    trace_buf << buf;
  }
  free(symbols);
  if (nFrames == nMaxFrames) trace_buf << "[truncated]\n";
  return trace_buf.str();
}