KOKKOS_DEVICES=Cuda
KOKKOS_CUDA_OPTIONS=enable_lambda
KOKKOS_ARCH = "SNB,Volta70"

KOKKOS_USE_DEPRECATED_MAKEFILES=1

MAKEFILE_PATH := $(subst Makefile,,$(abspath $(lastword $(MAKEFILE_LIST))))

ifndef KOKKOS_PATH
  KOKKOS_PATH = $(MAKEFILE_PATH)../..
endif

SRC = $(wildcard $(MAKEFILE_PATH)*.cpp)
HEADERS = $(wildcard $(MAKEFILE_PATH)*.hpp)

vpath %.cpp $(sort $(dir $(SRC)))

default: build
	echo "Start Build"

ifneq (,$(findstring Cuda,$(KOKKOS_DEVICES)))
CXX = ${KOKKOS_PATH}/bin/nvcc_wrapper
EXE = atomic_perf.cuda
else
CXX = g++
EXE = atomic_perf.exe
endif

CXXFLAGS ?= -O3 -g
override CXXFLAGS += -I$(MAKEFILE_PATH)

DEPFLAGS = -M
LINK = ${CXX}
LINKFLAGS =

OBJ = $(notdir $(SRC:.cpp=.o))
LIB =

include $(KOKKOS_PATH)/Makefile.kokkos

build: $(EXE)

$(EXE): $(OBJ) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(KOKKOS_LDFLAGS) $(LINKFLAGS) $(EXTRA_PATH) $(OBJ) $(KOKKOS_LIBS) $(LIB) -o $(EXE)

clean: kokkos-clean
	rm -f *.o atomic_perf.cuda atomic_perf.exe

# Compilation rules

%.o:%.cpp $(KOKKOS_CPP_DEPENDS) $(HEADERS)
	$(CXX) $(KOKKOS_CPPFLAGS) $(KOKKOS_CXXFLAGS) $(CXXFLAGS) $(EXTRA_INC) -c $< -o $(notdir $@)
