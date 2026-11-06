CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

# Flagi nagłówków i kompilatora
PYTHIA_INC := $(shell pythia8-config --cxxflags)
ROOT_INC   := $(shell root-config --cflags)

# Flagi linkera i biblioteki współdzielone
PYTHIA_LIB := $(shell pythia8-config --ldflags)
ROOT_LIB   := $(shell root-config --libs)

# Liczba zdarzeń do wygenerowania (można nadpisać: make run N=1000)
N ?= 100000

.PHONY: all clean run run_single benchmark

all: generate generate_single_thread

# Kompilacja generatora wielowątkowego (Pythia 8 + ROOT)
generate: generate.cc
	$(CXX) $(CXXFLAGS) $(PYTHIA_INC) $(ROOT_INC) generate.cc -o generate $(PYTHIA_LIB) $(ROOT_LIB)

# Kompilacja generatora jednowątkowego (Pythia 8 + ROOT)
generate_single_thread: generate_single_thread.cc
	$(CXX) $(CXXFLAGS) $(PYTHIA_INC) $(ROOT_INC) generate_single_thread.cc -o generate_single_thread $(PYTHIA_LIB) $(ROOT_LIB)

# Kompilacja analizatora (sam ROOT)
analyze: analyze.cc
	$(CXX) $(CXXFLAGS) $(ROOT_INC) analyze.cc -o analyze $(ROOT_LIB)

# Uruchomienie generatora wielowątkowego -> events.root
run: generate
	./generate $(N)

# Uruchomienie generatora jednowątkowego -> events.root
run_single: generate_single_thread
	./generate_single_thread $(N)

# Benchmark: oba generatory po kolei, na końcu zestawienie czasów
benchmark: generate generate_single_thread
	@./generate_single_thread $(N) > benchmark_single.log 2>&1
	@./generate $(N) > benchmark_multi.log 2>&1
	@sed -n '/===== BENCHMARK/,/^=====/p' benchmark_single.log
	@sed -n '/===== BENCHMARK/,/^=====/p' benchmark_multi.log
	@echo "Pelne logi: benchmark_single.log, benchmark_multi.log"

clean:
	rm -f generate generate_single_thread analyze benchmark_*.log
