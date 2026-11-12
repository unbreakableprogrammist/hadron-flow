CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

PYTHIA_INC := $(shell pythia8-config --cxxflags)
PYTHIA_LIB := $(shell pythia8-config --ldflags)
ROOT_INC := $(shell root-config --cflags)
ROOT_LIB := $(shell root-config --libs)

N ?= 100000

.PHONY: all clean run run_single analysis benchmark

all: generate generate_single_thread analyze

generate: generate.cc
	$(CXX) $(CXXFLAGS) $(PYTHIA_INC) $(ROOT_INC) generate.cc -o generate $(PYTHIA_LIB) $(ROOT_LIB)

generate_single_thread: generate_single_thread.cc
	$(CXX) $(CXXFLAGS) $(PYTHIA_INC) $(ROOT_INC) generate_single_thread.cc -o generate_single_thread $(PYTHIA_LIB) $(ROOT_LIB)

analyze: analyze.cc
	$(CXX) $(CXXFLAGS) $(ROOT_INC) analyze.cc -o analyze $(ROOT_LIB)

# Etap 1: generacja -> events.root
run: generate
	./generate $(N)

run_single: generate_single_thread
	./generate_single_thread $(N)

# Etap 2: analiza events.root -> analysis.root
analysis: analyze
	./analyze events.root analysis.root

# Benchmark liczy sie w osobnym katalogu, zeby nie nadpisac glownego events.root
benchmark: generate generate_single_thread
	@mkdir -p benchmark_tmp
	@cd benchmark_tmp && ../generate_single_thread $(N) > ../benchmark_single.log 2>&1
	@cd benchmark_tmp && ../generate $(N) > ../benchmark_multi.log 2>&1
	@rm -rf benchmark_tmp
	@echo "=== generate_single_thread ==="
	@grep -E '^(Zapisano |Czas: |Uwaga: )' benchmark_single.log
	@echo "=== generate ==="
	@grep -E '^(Zapisano |Czas: |Uwaga: )' benchmark_multi.log
	@echo "Pelne logi: benchmark_single.log, benchmark_multi.log"

clean:
	rm -rf generate generate_single_thread analyze benchmark_*.log benchmark_tmp
