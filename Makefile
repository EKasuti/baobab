CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
LDLIBS = -lz

OBJS = main.o log.o
TEST_OBJS = test.o log.o

baobab: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

baobab_test: $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS) $(LDLIBS)

test: baobab_test
	./baobab_test

# cstack db_tutorial build
db: db.cpp
	$(CXX) $(CXXFLAGS) -o $@ db.cpp

spec: db
	rspec

%.o: %.cpp log.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf baobab baobab_test db *.o *.dSYM

.PHONY: test spec clean
