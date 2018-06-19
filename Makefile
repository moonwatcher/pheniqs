# Pheniqs : PHilology ENcoder wIth Quality Statistics
# Copyright (C) 2018  Lior Galanti
# NYU Center for Genetics and System Biology

# Author: Lior Galanti <lior.galanti@nyu.edu>

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.

# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# The PHENIQS_VERSION defaults to $(MAJOR_REVISON).$(MINOR_REVISON) if not provided by the environment.
# git revision checksum is appended if available
# If available in the environment, dependency version are also included when generating version.h

# providing those to make during build will report them on pheniqs --version
# this is especially useful when building a static binary

# PHENIQS_VERSION = $(MAJOR_REVISON).$(MINOR_REVISON).$(PHENIQS_GIT_REVISION)
# ZLIB_VERSION = 1.2.11
# BZIP2_VERSION = 1.0.6
# XZ_VERSION = 5.2.3
# LIBDEFLATE_VERSION = 1.0
# RAPIDJSON_VERSION = 1.1.0
# HTSLIB_VERSION = 1.8

# to build with an explicit compiler with set CXX or provide the path on the command line to make
# for instance to build with gcc 7 from homebrew on MacOS you can install it with `brew install gcc@7`
# and build with `make CXX=/usr/local/bin/g++-7` or `make CXX=clang++` for explicitly building with clang

MAJOR_REVISON  := 2
MINOR_REVISON  := 0
PREFIX          = /usr/local
BIN_PREFIX      = $(PREFIX)/bin
INCLUDE_PREFIX  = $(PREFIX)/include
LIB_PREFIX      = $(PREFIX)/lib

CPPFLAGS        += -Wall -Wsign-compare
CXXFLAGS        += -std=c++11 -O3
LDFLAGS         +=
LIBS            += -lhts -lz -lbz2 -llzma
STATIC_LIBS     += $(LIB_PREFIX)/libhts.a $(LIB_PREFIX)/libz.a $(LIB_PREFIX)/libbz2.a $(LIB_PREFIX)/liblzma.a

# configuration.h : generated from configuration.json
# version.h       : generated version header file
# include.h       : standard library include and using statements
# error.h         : custom exception objects
# nucleotide.h    : IUPAC nucleotide encoding and decoding
# phred.h         : Phred quality scale encoding and decoding
# kstring.h       : wrapper for the kstring_t object from htslib

PHENIQS_SOURCES = \
	accumulate.cpp \
	atom.cpp \
	auxiliary.cpp \
	barcode.cpp \
	channel.cpp \
	command.cpp \
	decoder.cpp \
	environment.cpp \
	fastq.cpp \
	feed.cpp \
	hts.cpp \
	interface.cpp \
	json.cpp \
	pheniqs.cpp \
	pipeline.cpp \
	proxy.cpp \
	read.cpp \
	sequence.cpp \
	transform.cpp \
	url.cpp

PHENIQS_OBJECTS = \
	accumulate.o \
	atom.o \
	auxiliary.o \
	barcode.o \
	channel.o \
	command.o \
	decoder.o \
	environment.o \
	fastq.o \
	feed.o \
	hts.o \
	interface.o \
	json.o \
	pheniqs.o \
	pipeline.o \
	proxy.o \
	read.o \
	sequence.o \
	transform.o \
	url.o

PHENIQS_EXECUTABLE = pheniqs

PHENIQS_GIT_REVISION := $(shell git describe --abbrev=40 --always 2> /dev/null)

PLATFORM := $(shell uname -s)

ifndef PHENIQS_VERSION
    PHENIQS_VERSION := $(MAJOR_REVISON).$(MINOR_REVISON)
endif

ifdef PHENIQS_GIT_REVISION
    override PHENIQS_VERSION := '$(PHENIQS_VERSION).$(PHENIQS_GIT_REVISION)'
endif

ifdef PREFIX
    CPPFLAGS += -I$(INCLUDE_PREFIX)
    LDFLAGS += -L$(LIB_PREFIX)
endif

with-static = 0
with-libdeflate = 0
ifneq ('$(wildcard $(LIB_PREFIX)/libdeflate.a)','')
    ifeq ($(PLATFORM), Darwin)
        with-libdeflate = 1
    else ifeq ($(PLATFORM), Linux)
        ifneq ('$(wildcard $(LIB_PREFIX)/libdeflate.so)','')
            with-libdeflate = 1
        endif
    endif
endif

ifeq ($(with-libdeflate), 1)
    LIBS += -ldeflate
    STATIC_LIBS += $(LIB_PREFIX)/libdeflate.a
endif

ifeq ($(with-static), 1)
    LIBS = $(STATIC_LIBS)
endif

all: $(PHENIQS_SOURCES) generated $(PHENIQS_EXECUTABLE)

config:
	@echo 'PHENIQS_VERSION     :  $(PHENIQS_VERSION)'
	@echo 'PLATFORM            :  $(PLATFORM)'
	@echo 'PREFIX              :  $(PREFIX)'
	@echo 'BIN_PREFIX          :  $(BIN_PREFIX)'
	@echo 'INCLUDE_PREFIX      :  $(INCLUDE_PREFIX)'
	@echo 'LIB_PREFIX          :  $(LIB_PREFIX)'
	@echo 'CXX                 :  $(CXX)'
	@echo 'CPPFLAGS            :  $(CPPFLAGS)'
	@echo 'CXXFLAGS            :  $(CXXFLAGS)'
	@echo 'LDFLAGS             :  $(LDFLAGS)'
	@echo 'LIBS                :  $(LIBS)'
	@echo 'with-libdeflate     :  $(with-libdeflate)'
	@echo 'with-static         :  $(with-static)'
	$(if $(ZLIB_VERSION),        @echo 'ZLIB_VERSION        :  $(ZLIB_VERSION)' )
	$(if $(BZIP2_VERSION),       @echo 'BZIP2_VERSION       :  $(BZIP2_VERSION)' )
	$(if $(XZ_VERSION),          @echo 'XZ_VERSION          :  $(XZ_VERSION)' )
	$(if $(LIBDEFLATE_VERSION),  @echo 'LIBDEFLATE_VERSION  :  $(LIBDEFLATE_VERSION)' )
	$(if $(RAPIDJSON_VERSION),   @echo 'RAPIDJSON_VERSION   :  $(RAPIDJSON_VERSION)' )
	$(if $(HTSLIB_VERSION),      @echo 'HTSLIB_VERSION      :  $(HTSLIB_VERSION)' )

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(PHENIQS_EXECUTABLE): $(PHENIQS_OBJECTS)
	$(CXX) $(PHENIQS_OBJECTS) $(LDFLAGS) -pthread $(LIBS) -o $(PHENIQS_EXECUTABLE)

# Regenerate version.h when PHENIQS_VERSION changes
version.h: $(if $(wildcard version.h),$(if $(findstring "$(PHENIQS_VERSION)",$(shell cat version.h)),clean.version))
	@echo version.h generated with PHENIQS_VERSION $(PHENIQS_VERSION)
	$(if $(PHENIQS_VERSION),    @echo '#ifndef PHENIQS_VERSION_H'                           >> $@)
	$(if $(PHENIQS_VERSION),    @echo '#define PHENIQS_VERSION_H\n'                         >> $@)
	$(if $(PHENIQS_VERSION),    @echo '#define PHENIQS_VERSION "$(PHENIQS_VERSION)"'        >> $@)
	$(if $(ZLIB_VERSION),       @echo '#define ZLIB_VERSION "$(ZLIB_VERSION)"'              >> $@)
	$(if $(BZIP2_VERSION),      @echo '#define BZIP2_VERSION "$(BZIP2_VERSION)"'            >> $@)
	$(if $(XZ_VERSION),         @echo '#define XZ_VERSION "$(XZ_VERSION)"'                  >> $@)
	$(if $(LIBDEFLATE_VERSION), @echo '#define LIBDEFLATE_VERSION "$(LIBDEFLATE_VERSION)"'  >> $@)
	$(if $(RAPIDJSON_VERSION),  @echo '#define RAPIDJSON_VERSION "$(RAPIDJSON_VERSION)"'    >> $@)
	$(if $(HTSLIB_VERSION),     @echo '#define HTSLIB_VERSION "$(HTSLIB_VERSION)"'          >> $@)
	$(if $(PHENIQS_VERSION),    @echo '\n#endif /* PHENIQS_VERSION_H */\n'                  >> $@)

clean.version:
	-@rm -f version.h

# Regenerate interface configuration.h from configuration.json
configuration.h: configuration.json
	@echo configuration.h command line interface configuration generated.
	$(shell ./tool/make_configuration_h.sh)

# Regenerate zsh completion from configuration.json
_pheniqs: configuration.json
	@echo zsh completion _pheniqs generated.
	$(shell ./tool/pheniqs-tools.py zsh configuration.json > _pheniqs)

generated: version.h configuration.h _pheniqs

clean.generated: clean.version
	-@rm -f configuration.h
	-@rm -f _pheniqs

clean.object:
	-@rm -f $(PHENIQS_OBJECTS)

clean: clean.generated clean.object
	-@rm -f $(PHENIQS_EXECUTABLE)

install: pheniqs
	if( test ! -d $(PREFIX)/bin ) ; then mkdir -p $(PREFIX)/bin ; fi
	cp -f pheniqs $(PREFIX)/bin/pheniqs
	chmod a+x $(PREFIX)/bin/pheniqs

# Dependencies

json.o: \
	error.h \
	kstring.h \
	json.h

url.o: \
	json.o \
	url.h

command.o: \
	url.o \
	version.h \
	configuration.h \
	command.h

atom.o: \
	json.o \
	atom.h

sequence.o: \
	json.o \
	phred.h \
	nucleotide.h \
	sequence.h

barcode.o: \
	sequence.o \
	barcode.h

auxiliary.o: \
	atom.o \
	barcode.o \
	auxiliary.h

read.o: \
	auxiliary.o \
	read.h

accumulate.o: \
	url.o \
	read.o \
	barcode.o \
	accumulate.h

proxy.o: \
	url.o \
	atom.o \
	proxy.h

feed.o: \
	proxy.o \
	read.o \
	feed.h

fastq.o: \
	feed.o \
	fastq.h

hts.o: \
	feed.o \
	hts.h

transform.o: \
	read.o \
	transform.h

interface.o: \
	command.o \
	barcode.o \
	transform.o \
	interface.h

environment.o: \
	interface.o \
	environment.h

channel.o: \
	feed.o \
	channel.h

decoder.o: \
	transform.o \
	channel.o \
	decoder.h

pipeline.o: \
	accumulate.o \
	environment.o \
	fastq.o \
	hts.o \
	decoder.o \
	pipeline.h

pheniqs.o: \
	pipeline.o
