DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)

TARGET = wb-mqtt-mbgate
OBJS = main.o
SRC_DIR = src

COMMON_OBJS = i_modbus_server_observer.o i_modbus_backend.o modbus_wrapper.o mqtt_converters.o observer.o config_parser.o mqtt_fastwrapper.o

TEST_DIR = test
TEST_OBJS = main.o address_range_test.o modbus_server_test.o mock_modbus_observer.o converters_test.o mock_mqtt_client.o gateway_test.o mqtt_fastwrapper_test.o
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lpthread -lgmock
TEST_CXXFLAGS = -I. -O0 -I$(SRC_DIR)

TEST_OBJS := $(patsubst %, $(TEST_DIR)/%, $(TEST_OBJS))
COMMON_OBJS := $(patsubst %, $(SRC_DIR)/%, $(COMMON_OBJS))
OBJS := $(patsubst %, $(SRC_DIR)/%, $(OBJS))

CXX=g++
LD=g++
LDFLAGS=-lmodbus -lmosquittopp -lwbmqtt -ljsoncpp -pthread -pg
CXXFLAGS=-std=c++0x -Wall -Werror -Os -pg

all: $(TARGET)

$(TARGET): $(OBJS) $(COMMON_OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

test/%.o: test/%.cpp
	$(CXX) -c $(CXXFLAGS) $(TEST_CXXFLAGS) -o $@ $^

test: $(TEST_DIR)/$(TEST_TARGET)
	valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_TARGET) --gtest_catch_exceptions=0 || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>&2; \
			exit 1; \
		fi

$(TEST_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -rf $(SRC_DIR)/*.o $(TARGET) $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_TARGET)

install: all
	mkdir -p $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/wb-mqtt-mbgate

	install -m 0644 wb-mqtt-mbgate.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-mbgate.schema.json
	install -m 0755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)


.PHONY: all test clean
