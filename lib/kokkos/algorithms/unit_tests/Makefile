KOKKOS_PATH = ../..

GTEST_PATH = ../../TPL/gtest

vpath %.cpp ${KOKKOS_PATH}/algorithms/unit_tests

default: build_all
	echo "End Build"

ifneq (,$(findstring Cuda,$(KOKKOS_DEVICES)))
  CXX = $(KOKKOS_PATH)/bin/nvcc_wrapper
else
  CXX = g++
endif

CXXFLAGS = -O3
LINK ?= $(CXX)
LDFLAGS ?=
override LDFLAGS += -lpthread

KOKKOS_USE_DEPRECATED_MAKEFILES=1

include $(KOKKOS_PATH)/Makefile.kokkos

KOKKOS_CXXFLAGS += -I$(GTEST_PATH) -I${KOKKOS_PATH}/algorithms/unit_tests -I${KOKKOS_PATH}/core/unit_test/category_files

TEST_TARGETS =
TARGETS =

tmp := $(foreach device, $(KOKKOS_DEVICELIST), \
  $(if $(filter Test$(device).cpp, $(shell ls Test$(device).cpp 2>/dev/null)),,\
     $(shell echo "$(H)include <Test"${device}"_Category.hpp>" > Test$(device).cpp); \
     $(shell echo "$(H)include <TestRandom.hpp>" >> Test$(device).cpp); \
     $(shell echo "$(H)include <TestSort.hpp>" >> Test$(device).cpp); \
     $(shell echo "$(H)include <TestBinSortA.hpp>" >> Test$(device).cpp); \
     $(shell echo "$(H)include <TestBinSortB.hpp>" >> Test$(device).cpp); \
     $(shell echo "$(H)include <TestNestedSort.hpp>" >> Test$(device).cpp); \
     $(shell echo "$(H)include <TestSortCustomComp.hpp>" >> Test$(device).cpp); \
   ) \
)

ifeq ($(KOKKOS_INTERNAL_USE_CUDA), 1)
	OBJ_CUDA = TestCuda.o TestStdAlgorithmsCommon.o UnitTestMain.o gtest-all.o
	TARGETS += KokkosAlgorithms_UnitTest_Cuda
	TEST_TARGETS += test-cuda
endif

ifeq ($(KOKKOS_INTERNAL_USE_HIP), 1)
	OBJ_HIP = TestHIP.o TestStdAlgorithmsCommon.o UnitTestMain.o gtest-all.o
	TARGETS += KokkosAlgorithms_UnitTest_HIP
	TEST_TARGETS += test-hip
endif

ifeq ($(KOKKOS_INTERNAL_USE_THREADS), 1)
	OBJ_THREADS = TestThreads.o TestStdAlgorithmsCommon.o UnitTestMain.o gtest-all.o
	TARGETS += KokkosAlgorithms_UnitTest_Threads
	TEST_TARGETS += test-threads
endif

ifeq ($(KOKKOS_INTERNAL_USE_OPENMP), 1)
	OBJ_OPENMP = TestOpenMP.o TestStdAlgorithmsCommon.o UnitTestMain.o gtest-all.o
	TARGETS += KokkosAlgorithms_UnitTest_OpenMP
	TEST_TARGETS += test-openmp
endif

ifeq ($(KOKKOS_INTERNAL_USE_HPX), 1)
	OBJ_HPX = TestHPX.o TestStdAlgorithmsCommon.o UnitTestMain.o gtest-all.o
	TARGETS += KokkosAlgorithms_UnitTest_HPX
	TEST_TARGETS += test-hpx
endif

ifeq ($(KOKKOS_INTERNAL_USE_SERIAL), 1)
	OBJ_SERIAL = TestSerial.o TestStdAlgorithmsCommon.o UnitTestMain.o gtest-all.o
	TARGETS += KokkosAlgorithms_UnitTest_Serial
	TEST_TARGETS += test-serial
endif

KokkosAlgorithms_UnitTest_Cuda: $(OBJ_CUDA) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(EXTRA_PATH) $(OBJ_CUDA) $(KOKKOS_LIBS) $(LIB) $(KOKKOS_LDFLAGS) $(LDFLAGS) -o KokkosAlgorithms_UnitTest_Cuda

KokkosAlgorithms_UnitTest_HIP: $(OBJ_HIP) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(EXTRA_PATH) $(OBJ_HIP) $(KOKKOS_LIBS) $(LIB) $(KOKKOS_LDFLAGS) $(LDFLAGS) -o KokkosAlgorithms_UnitTest_HIP

KokkosAlgorithms_UnitTest_Threads: $(OBJ_THREADS) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(EXTRA_PATH) $(OBJ_THREADS) $(KOKKOS_LIBS) $(LIB) $(KOKKOS_LDFLAGS) $(LDFLAGS) -o KokkosAlgorithms_UnitTest_Threads

KokkosAlgorithms_UnitTest_OpenMP: $(OBJ_OPENMP) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(EXTRA_PATH) $(OBJ_OPENMP) $(KOKKOS_LIBS) $(LIB) $(KOKKOS_LDFLAGS) $(LDFLAGS) -o KokkosAlgorithms_UnitTest_OpenMP

KokkosAlgorithms_UnitTest_HPX: $(OBJ_HPX) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(EXTRA_PATH) $(OBJ_HPX) $(KOKKOS_LIBS) $(LIB) $(KOKKOS_LDFLAGS) $(LDFLAGS) -o KokkosAlgorithms_UnitTest_HPX

KokkosAlgorithms_UnitTest_Serial: $(OBJ_SERIAL) $(KOKKOS_LINK_DEPENDS)
	$(LINK) $(EXTRA_PATH) $(OBJ_SERIAL) $(KOKKOS_LIBS) $(LIB) $(KOKKOS_LDFLAGS) $(LDFLAGS) -o KokkosAlgorithms_UnitTest_Serial

test-cuda: KokkosAlgorithms_UnitTest_Cuda
	./KokkosAlgorithms_UnitTest_Cuda

test-hip: KokkosAlgorithms_UnitTest_HIP
	./KokkosAlgorithms_UnitTest_HIP

test-threads: KokkosAlgorithms_UnitTest_Threads
	./KokkosAlgorithms_UnitTest_Threads

test-openmp: KokkosAlgorithms_UnitTest_OpenMP
	./KokkosAlgorithms_UnitTest_OpenMP

test-hpx: KokkosAlgorithms_UnitTest_HPX
	./KokkosAlgorithms_UnitTest_HPX

test-serial: KokkosAlgorithms_UnitTest_Serial
	./KokkosAlgorithms_UnitTest_Serial

build_all: $(TARGETS)

test: $(TEST_TARGETS)

clean: kokkos-clean
	rm -f *.o $(TARGETS)

# Compilation rules

%.o:%.cpp $(KOKKOS_CPP_DEPENDS)
	$(CXX) $(KOKKOS_CPPFLAGS) $(KOKKOS_CXXFLAGS) $(CXXFLAGS) $(EXTRA_INC) -c $<

gtest-all.o:$(GTEST_PATH)/gtest/gtest-all.cc
	$(CXX) $(KOKKOS_CPPFLAGS) $(KOKKOS_CXXFLAGS) $(CXXFLAGS) $(EXTRA_INC) -c $(GTEST_PATH)/gtest/gtest-all.cc
