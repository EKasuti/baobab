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

# Insert N rows into a fresh test.db, then select them: make demo N=20
N ?= 15
demo: db
	rm -f test.db
	for i in $$(seq 1 $(N)); do echo "insert $$i user$$i person$$i@example.com"; done \
		| { cat; echo select; echo .exit; } | ./db test.db

%.o: %.cpp log.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf baobab baobab_test db test.db *.o *.dSYM

.PHONY: test spec demo clean
