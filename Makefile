CXX=clang++
INCLUDES=-Iincludes/
CXXFLAGS=-std=c++20 -g -fstandalone-debug -Wall -Wextra -Werror -pedantic $(INCLUDES)

exec: bin/exec
tests: bin/tests

bin/exec: src/order.cpp src/price_level.cpp src/order_book.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

bin/tests: obj/catch.o tests/tests.cpp src/order.cpp src/price_level.cpp src/order_book.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

obj/catch.o: tests/catch.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

.DEFAULT_GOAL := exec
.PHONY: clean

clean:
	rm -rf bin/* obj/*
