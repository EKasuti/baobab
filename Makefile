CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
LDLIBS = -lz

SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

baobab: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f baobab $(OBJS)

.PHONY: clean