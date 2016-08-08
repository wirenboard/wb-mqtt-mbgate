TARGET = wb-mqtt-mbgate
OBJS = main.o modbus_wrapper.o

TEST_DIR = test
TEST_OBJS = main.o address_range_test.o
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lpthread
TEST_CXXFLAGS = -I.

TEST_OBJS := $(patsubst %, $(TEST_DIR)/%, $(TEST_OBJS))

CXX=g++
LD=g++
LDFLAGS=-lmodbus
CXXFLAGS=-std=c++11 -Wall -Werror -ggdb

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

test/%.o: test/%.cpp
	$(CXX) -c $(CXXFLAGS) $(TEST_CXXFLAGS) -o $@ $^

test: $(TEST_DIR)/$(TEST_TARGET)
	valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_TARGET) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>&2; \
			exit 1; \
		fi

$(TEST_DIR)/$(TEST_TARGET): $(TARGET) $(TEST_OBJS)
	$(CXX) -o $@ $(TEST_OBJS) $(LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -rf *.o $(TARGET) $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_TARGET)

.PHONY: all test clean
