TARGET = wb-mqtt-mbgate
OBJS = main.o logging.o config_parser.o
SRC_DIR = src

COMMON_OBJS = i_modbus_server_observer.o i_modbus_backend.o modbus_wrapper.o mqtt_converters.o observer.o mqtt_fastwrapper.o

DEBUG_CXXFLAGS = -O0 -fprofile-arcs -ftest-coverage
DEBUG_LDFLAGS = -lgcov

NORMAL_CXXFLAGS = -O2
NORMAL_LDFLAGS =

TEST_DIR = test
TEST_OBJS = main.o address_range_test.o modbus_server_test.o mock_modbus_observer.o converters_test.o mock_mqtt_client.o gateway_test.o mqtt_fastwrapper_test.o multi_unitid_test.o
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lpthread -lgmock
TEST_CXXFLAGS = -I. -I$(SRC_DIR)

TEST_OBJS := $(patsubst %, $(TEST_DIR)/%, $(TEST_OBJS))
COMMON_OBJS := $(patsubst %, $(SRC_DIR)/%, $(COMMON_OBJS))
OBJS := $(patsubst %, $(SRC_DIR)/%, $(OBJS))

ifeq ($(DEB_TARGET_ARCH),armel)
CROSS_COMPILE=arm-linux-gnueabi-
endif

CXX=$(CROSS_COMPILE)g++
CXX_PATH := $(shell which $(CROSS_COMPILE)g++-4.7)

CC=$(CROSS_COMPILE)gcc
CC_PATH := $(shell which $(CROSS_COMPILE)gcc-4.7)

ifneq ($(CXX_PATH),)
	CXX=$(CROSS_COMPILE)g++-4.7
endif

ifneq ($(CC_PATH),)
	CC=$(CROSS_COMPILE)gcc-4.7
endif
LD=$(CXX)
LDFLAGS=-lmodbus -lmosquittopp -lwbmqtt -ljsoncpp -llog4cpp -pthread
CXXFLAGS=-std=c++0x -Wall -Werror


DEBUG=
NDEBUG?=
ifeq ($(NDEBUG),)
DEBUG=y
endif

CXXFLAGS += $(if $(or $(DEBUG),$(filter test, $(MAKECMDGOALS))), $(DEBUG_CXXFLAGS), $(NORMAL_CXXFLAGS))
LDFLAGS += $(if $(or $(DEBUG),$(filter test, $(MAKECMDGOALS))), $(DEBUG_LDFLAGS), $(NORMAL_LDFLAGS))

# fail build if tree is dirty and package is being built
ifneq ($(NDEBUG),)
BUILDCLEAN_HACK +=n
endif

ifeq ("$(strip $(BUILDCLEAN_HACK))", "n n")
$(info $(start_red))
$(info Please commit all changes before building a package)
$(info $(end_color))
$(error "Can't build production package on dirty tree")
endif

# extract Git revision and version number from debian/changelog
GIT_REVISION:=$(shell git rev-parse HEAD)
DEB_VERSION:=$(shell head -1 debian/changelog | awk '{ print $$2 }' | sed 's/[\(\)]//g')

CXXFLAGS += -DWBMQTT_COMMIT="$(GIT_REVISION)" -DWBMQTT_VERSION="$(DEB_VERSION)"

all: $(TARGET)

$(TARGET): $(OBJS) $(COMMON_OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

test/%.o: test/%.cpp
	$(CXX) -c $(CXXFLAGS) $(TEST_CXXFLAGS) -o $@ $^

test: $(TEST_DIR)/$(TEST_TARGET)
	if [ "$(shell arch)" = "armv7l" ]; then \
		$(TEST_DIR)/$(TEST_TARGET) \
	else \
		valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_TARGET)  || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>&2; \
			exit 1; \
		fi; \
	fi

$(TEST_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

distclean: clean

clean:
	rm -rf $(SRC_DIR)/*.o $(TARGET) $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_TARGET)
	rm -rf $(SRC_DIR)/*.gcda $(SRC_DIR)/*.gcno $(TEST_DIR)/*.gcda $(TEST_DIR)/*.gcno

install: all
	mkdir -p $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin

	install -m 0644 wb-mqtt-mbgate.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-mbgate.schema.json

	install -m 0755 utils/generate_config.py $(DESTDIR)/usr/bin/wb-mqtt-mbgate-confgen
	install -m 0755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)


.PHONY: all test clean
