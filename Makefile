# CXX=clang++
# CXX=g++-5
CXX=g++

HERE:=$(shell pwd)

TARGET=gridle
SRCEXT=cpp
SRCDIR=src
OBJDIR=obj
BINDIR=bin
INC=$(HERE)/IncludeThird/ $(HERE)/IncludeThird/tclap-1.2.1/include/ $(HERE)/IncludeThird/Eigen

CXXFLAGS=-std=c++11 -Wall

OPTFLAGS1=-O0 -g -march=native -mfpmath=sse # Debug
OPTFLAGS2=-O2 -march=native -mfpmath=sse -flto -B/usr/lib/gold-ld # Clang Linux Release build
OPTFLAGS3=-O2 -march=native -mfpmath=sse # GCC Release Linux / Clang Release Mac
OPTFLAGS4=-O2 -march=native -mfpmath=sse -Wa,-q -mmacosx-version-min=10.9 # GCC-5 Release Mac, use Clang linker
OPTFLAGS5=-O2 -g -march=native -mfpmath=sse # Linux perf profile
OBJECT=-c
ASM=-S
OPTFLAGS=$(OPTFLAGS5)
# OBJECT=$(ASM)

# Set true to enable threading, empty for not
ENABLEOPENMP=true

FEATUREFLAGS1=-DDEBUG -DMAT_ACC -DUSE_SIMD -fno-omit-frame-pointer # Debug
FEATUREFLAGS2=-DNDEBUG -DMAT_ACC -DUSE_SIMD # Release
FEATUREFLAGS3=-DDEBUG -DMAT_ACC # Debug no SIMD
FEATUREFLAGS4=-DNDEBUG -DMAT_ACC # Release no SIMD
FFLAGS=$(FEATUREFLAGS1)

WARNINGFLAGS=-Wall -Wextra

LDFLAGS=
LIBS=
LIBDIR=

## Increase clang++ template depth up to the suggested limit for C++11
ifeq ($(CXX),clang++)
FFLAGS+= -ftemplate-depth=1024
endif
## Enable openmp on g++
ifdef ENABLEOPENMP
ifeq ($(CXX),$(filter $(CXX),g++ g++-5))
FFLAGS+= -DGOMP
OPTFLAGS+= -fopenmp
endif
endif


## Platform specific library requirements
UNAME_S:=$(shell uname -s)
ifeq ($(UNAME_S),Linux)
LIBS+= dl
LDFLAGS+= -pthread
endif

# SDLFLAGS=$(shell $(HERE)/third/sdlBuild/bin/sdl2-config --cflags)
# SDLLIBS=$(shell $(HERE)/third/sdlBuild/bin/sdl2-config --libs)
# SDLLIBS=$(shell $(HERE)/third/sdlBuild/bin/sdl2-config --static-libs)

INC:=$(addprefix -I,$(INC))
LIBS:=$(addprefix -l,$(LIBS))
LIBDIR:=$(addprefix -L,$(LIBDIR))
CXXFLAGS+=$(OBJECT) $(OPTFLAGS) $(WARNINGFLAGS) $(FFLAGS) $(INC)
SOURCES:=$(shell find $(SRCDIR) -name '*.$(SRCEXT)')
SRCDIRS:=$(shell find . -name '*.$(SRCEXT)' -exec dirname {} \; | uniq)
OBJECTS:=$(patsubst %.$(SRCEXT),$(OBJDIR)/%.o,$(SOURCES))
CXXFLAGS+=
LIBS+=
LDFLAGS+=$(LIBDIR) $(LIBS)
DEPS:=$(OBJECTS:.o=.d)

.phony: all clean distclean

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): buildrepo $(OBJECTS)
	@mkdir -p `dirname $@`
	@echo "Linking $@..."
	@$(CXX) $(OBJECTS) $(OPTFLAGS) $(LDFLAGS) -o $@

run: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET)

plot: run
	gnuplot $(shell ls Plot/*.gpi)
	#gnome-open Grid.png &

-include $(DEPS)

$(OBJDIR)/%.o: %.$(SRCEXT)
	@echo "Generating dependency list for $<..."
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $< -o $@

clean:
	$(RM) -r $(OBJDIR)

distclean: clean
	$(RM) -r $(BINDIR)/$(TARGET)

buildrepo:
	@$(call make-repo)

# These use gcc/clang's abilities to output makefile requirements for a file,
# so they generate a depencency list for each file, if for example a header
# included by that file is changed, then it will be recompiled
define make-repo
   for dir in $(SRCDIRS); \
   do \
	mkdir -p $(OBJDIR)/$$dir; \
   done
endef

define make-depend
  $(CXX) -MM       \
         -MF $3    \
         -MP       \
         -MT $2    \
         $(CXXFLAGS) \
         $1
endef
