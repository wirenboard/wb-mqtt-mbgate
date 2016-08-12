TARGET = wb-mqtt-mbgate
OBJS = main.o

COMMON_OBJS = i_modbus_server_observer.o i_modbus_backend.o modbus_wrapper.o mqtt_converters.o observer.o

TEST_DIR = test
TEST_OBJS = main.o address_range_test.o modbus_server_test.o mock_modbus_observer.o converters_test.o
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lpthread -lgmock
TEST_CXXFLAGS = -I.

TEST_OBJS := $(patsubst %, $(TEST_DIR)/%, $(TEST_OBJS))

CXX=g++
LD=g++
LDFLAGS=-lmodbus -lmosquittopp -lwbmqtt -pthread -pg
CXXFLAGS=-std=c++11 -Wall -Werror -ggdb

all: $(TARGET)

$(TARGET): $(OBJS) $(COMMON_OBJS)
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

$(TEST_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -rf *.o $(TARGET) $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_TARGET)

.PHONY: all test clean
