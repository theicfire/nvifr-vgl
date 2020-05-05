################################################################################
#
# Copyright 1993-2014 NVIDIA Corporation.  All rights reserved.
#
# NOTICE TO USER:
#
# This source code is subject to NVIDIA ownership rights under U.S. and
# international Copyright laws.
#
# NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
# CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
# IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
# IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
# OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
# OR PERFORMANCE OF THIS SOURCE CODE.
#
# U.S. Government End Users.  This source code is a "commercial item" as
# that term is defined at 48 C.F.R. 2.101 (OCT 1995), consisting  of
# "commercial computer software" and "commercial computer software
# documentation" as such terms are used in 48 C.F.R. 12.212 (SEPT 1995)
# and is provided to the U.S. Government only as a commercial end item.
# Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
# 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
# source code with only those rights set forth herein.
#
################################################################################

# This is used to detects architecture as well as provide overrides
OSUPPER = $(shell uname -s 2>/dev/null | tr "[:lower:]" "[:upper:]")
OSLOWER = $(shell uname -s 2>/dev/null | tr "[:upper:]" "[:lower:]")

OS_SIZE = $(shell uname -m | sed -e "s/i.86/32/" -e "s/x86_64/64/" -e "s/armv7l/32/")
OS_ARCH = $(shell uname -m)

# Detecting the location of the CUDA Toolkit if needed
CUDA_PATH ?= /usr/local/cuda
GCC ?= gcc
NVCC := $(CUDA_PATH)/bin/nvcc -ccbin $(GCC)

# This enables cross compilation from the developer
ifeq ($(i386),1)
        OS_SIZE = 32
        OS_ARCH = i386
endif
ifeq ($(i686),1)
        OS_SIZE = 32
        OS_ARCH = i686
endif
ifeq ($(x86_64),1)
        OS_SIZE = 64
        OS_ARCH = x86_64
endif

##############################################################################
# function to generate a list of object files from their corresponding
# source files; example usage:
#
# OBJECTS = $(call BUILD_OBJECT_LIST,$(SOURCES))
##############################################################################

BUILD_OBJECT_LIST = $(notdir $(addsuffix .o,$(basename $(1))))

##############################################################################
# function to define a rule to build an object file; $(1) is the source
# file to compile, and $(2) is any other dependencies.
##############################################################################

define DEFINE_OBJECT_RULE
  $$(call BUILD_OBJECT_LIST,$(1)): $(1) $(2)
	$(CC) -m$(OS_SIZE) $(CFLAGS) -c $$< -o $$@
endef

