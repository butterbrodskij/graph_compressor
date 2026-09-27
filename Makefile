CXX ?= c++
CXXFLAGS ?= -O3 -DNDEBUG -std=c++17 -Wall -Wextra -Wpedantic
SOURCES = main.cpp bit_io.cpp arithmetic.cpp graph_io.cpp codec.cpp
HEADERS = bit_io.h arithmetic.h graph_io.h codec.h

.PHONY: all test test-example clean

all: run

run: $(SOURCES) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $@

test: run
	python3 tests/test_roundtrip.py --binary ./run

test-example: run
	python3 tests/test_roundtrip.py --binary ./run --example small_example.tsv

clean:
	rm -f run
