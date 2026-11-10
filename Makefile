CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

PYTHIA_INC := $(shell pythia8-config --cxxflags)
PYTHIA_LIB := $(shell pythia8-config --ldflags)
ROOT_INC := $(shell root-config --cflags)
ROOT_LIB := $(shell root-config --libs)

N ?= 100000

.PHONY: all clean run run_single benchmark

all: generate generate_single_thread

generate: generate.cc
	$(CXX) $(CXXFLAGS) $(PYTHIA_INC) $(ROOT_INC) generate.cc -o generate $(PYTHIA_LIB) $(ROOT_LIB)

generate_single_thread: generate_single_thread.cc
	$(CXX) $(CXXFLAGS) $(PYTHIA_INC) $(ROOT_INC) generate_single_thread.cc -o generate_single_thread $(PYTHIA_LIB) $(ROOT_LIB)

analyze: analyze.cc
	$(CXX) $(CXXFLAGS) $(ROOT_INC) analyze.cc -o analyze $(ROOT_LIB)

run: generate
	./generate $(N)

run_single: generate_single_thread
	./generate_single_thread $(N)

benchmark: generate generate_single_thread
	@./generate_single_thread $(N) > benchmark_single.log 2>&1
	@./generate $(N) > benchmark_multi.log 2>&1
	@sed -n '/===== BENCHMARK/,/^=====/p' benchmark_single.log
	@sed -n '/===== BENCHMARK/,/^=====/p' benchmark_multi.log
	@echo "Pelne logi: benchmark_single.log, benchmark_multi.log"

clean:
	rm -f generate generate_single_thread analyze benchmark_*.log
