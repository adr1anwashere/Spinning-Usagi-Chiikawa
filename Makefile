CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
TARGET = Usagi
SRC = Usagi.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

spiderman: $(TARGET)
	./$(TARGET) --spiderman

ascii: $(TARGET)
	./$(TARGET) --ascii

clean:
	rm -f $(TARGET)

.PHONY: all run spiderman ascii clean
