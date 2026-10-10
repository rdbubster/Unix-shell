CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
TARGET   := mini-shell

all: $(TARGET)

$(TARGET): shell.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: $(TARGET)
	bash tests/test_shell.sh ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all test clean
