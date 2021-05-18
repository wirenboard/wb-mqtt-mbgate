ifneq ($(DEB_HOST_MULTIARCH),)
	CROSS_COMPILE ?= $(DEB_HOST_MULTIARCH)-
endif

ifeq ($(origin CC),default)
	CC := $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
	CXX := $(CROSS_COMPILE)g++
endif

TARGET = wb-mqtt-mbgate
OBJS = main.o config_parser.o
SRC_DIR = src

COMMON_OBJS = i_modbus_server_observer.o i_modbus_backend.o modbus_wrapper.o mqtt_converters.o observer.o modbus_lmb_backend.o log.o

DEBUG_CXXFLAGS = -O0 -fprofile-arcs -ftest-coverage
DEBUG_LDFLAGS = -lgcov

NORMAL_CXXFLAGS = -O2
NORMAL_LDFLAGS =

TEST_DIR = test
TEST_OBJS = main.o address_range_test.o modbus_server_test.o mock_modbus_observer.o converters_test.o mock_mqtt_client.o gateway_test.o multi_unitid_test.o
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lpthread -lgmock
TEST_CXXFLAGS = -I. -I$(SRC_DIR)

TEST_OBJS := $(patsubst %, $(TEST_DIR)/%, $(TEST_OBJS))
COMMON_OBJS := $(patsubst %, $(SRC_DIR)/%, $(COMMON_OBJS))
OBJS := $(patsubst %, $(SRC_DIR)/%, $(OBJS))

LDFLAGS=-lmodbus -lwbmqtt1 -ljsoncpp -lpthread
CXXFLAGS=-std=c++14 -Wall -Werror

DEBUG=
NDEBUG?=y
ifeq ($(NDEBUG),)
DEBUG=y
endif

CXXFLAGS += $(if $(or $(DEBUG)),$(DEBUG_CXXFLAGS),$(NORMAL_CXXFLAGS))
LDFLAGS += $(if $(or $(DEBUG)),$(DEBUG_LDFLAGS),$(NORMAL_LDFLAGS))

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
	${CXX} -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $^

test/%.o: test/%.cpp
	$(CXX) -c $(CXXFLAGS) $(TEST_CXXFLAGS) -o $@ $^

test: $(TEST_DIR)/$(TEST_TARGET)
	if [ "$(shell arch)" != "armv7l" ] && [ "$(CROSS_COMPILE)" = "" ] || [ "$(CROSS_COMPILE)" = "x86_64-linux-gnu-" ]; then \
		valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_TARGET) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
			exit 1; \
		fi \
   else \
        $(TEST_DIR)/$(TEST_TARGET); \
	fi

$(TEST_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

distclean: clean

clean:
	rm -rf $(SRC_DIR)/*.o $(TARGET) $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_TARGET)
	rm -rf $(SRC_DIR)/*.gcda $(SRC_DIR)/*.gcno $(TEST_DIR)/*.gcda $(TEST_DIR)/*.gcno

install:
	install -D -m 0644  wb-mqtt-mbgate.wbconfigs $(DESTDIR)/etc/wb-configs.d/31wb-mqtt-mbgate
	install -D -m 0644 wb-mqtt-mbgate.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-mbgate.schema.json
	install -D -m 0755 utils/generate_config.py $(DESTDIR)/usr/bin/wb-mqtt-mbgate-confgen
	install -m 0755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)


.PHONY: all test clean
