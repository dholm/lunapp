LUALIB = /opt/local/lib/liblua.a

CFLAGS = -O3 -I/opt/local/include
LDFLAGS = $(LUALIB)

.PHONY: all
all: test

%.cc.o: %.cc
	$(CXX) -c -o $@ $(CFLAGS) $^

test: test.cc.o
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	@rm -f test *.cc.o *~
