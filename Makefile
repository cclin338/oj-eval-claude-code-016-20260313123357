CXX = g++-13
CXXFLAGS = -std=c++17 -O2 -Wall

code: main.cpp
	$(CXX) $(CXXFLAGS) -o code main.cpp

clean:
	rm -f code *.o *.dat

.PHONY: clean
