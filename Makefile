ifneq ($(DEB_HOST_MULTIARCH),)
	CROSS_COMPILE ?= $(DEB_HOST_MULTIARCH)-
endif

ifeq ($(origin CC),default)
	CC := $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
	CXX := $(CROSS_COMPILE)g++
endif

ifeq ($(DEBUG),)
	BUILD_DIR ?= build/release
else
	BUILD_DIR ?= build/debug
endif

PREFIX = /usr

# extract Git revision and version number from debian/changelog
GIT_REVISION := $(shell git rev-parse HEAD)
DEB_VERSION := $(shell head -1 debian/changelog | awk '{ print $$2 }' | sed 's/[\(\)]//g')

TARGET = wb-mqtt-mbgate
SRC_DIR = src
COMMON_SRCS := $(shell find $(SRC_DIR) -name *.cpp -and -not -name main.cpp)
COMMON_OBJS := $(COMMON_SRCS:%=$(BUILD_DIR)/%.o)

CXXFLAGS = -std=c++14 -Wall -Werror -I$(SRC_DIR) -DWBMQTT_COMMIT="$(GIT_REVISION)" -DWBMQTT_VERSION="$(DEB_VERSION)"
LDFLAGS = -lmodbus -lwbmqtt1 -lpthread

ifeq ($(DEBUG),)
	CXXFLAGS += -O2
else
	CXXFLAGS += -g -O0 -fprofile-arcs -ftest-coverage -Wno-error=cpp
	LDFLAGS += -lgcov
endif

TEST_DIR = test
TEST_SRCS := $(shell find $(TEST_DIR) -name *.cpp)
TEST_OBJS := $(TEST_SRCS:%=$(BUILD_DIR)/%.o)
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lgmock

VALGRIND_FLAGS = --error-exitcode=180 -q

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(COMMON_OBJS) $(BUILD_DIR)/$(SRC_DIR)/main.cpp.o
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/$(TEST_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

test: $(BUILD_DIR)/$(TEST_DIR)/$(TEST_TARGET)
	if [ "$(shell arch)" != "armv7l" ] && [ "$(CROSS_COMPILE)" = "" ] || [ "$(CROSS_COMPILE)" = "x86_64-linux-gnu-" ]; then \
		valgrind $(VALGRIND_FLAGS) $(BUILD_DIR)/$(TEST_DIR)/$(TEST_TARGET) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
			exit 1; \
		else exit 1; fi; \
	else \
		$(BUILD_DIR)/$(TEST_DIR)/$(TEST_TARGET); \
	fi

lcov: test
ifeq ($(DEBUG), 1)
	geninfo --no-external -b . -o $(BUILD_DIR)/coverage.info $(BUILD_DIR)/src
	genhtml $(BUILD_DIR)/coverage.info -o $(BUILD_DIR)/cov_html
endif

distclean: clean

clean:
	-rm -rf build

install:
	install -Dm0644 wb-mqtt-mbgate.wbconfigs $(DESTDIR)/etc/wb-configs.d/31wb-mqtt-mbgate
	install -Dm0644 wb-mqtt-mbgate.schema.json -t $(DESTDIR)$(PREFIX)/share/wb-mqtt-confed/schemas
	install -Dm0755 utils/generate_config.py $(DESTDIR)$(PREFIX)/bin/wb-mqtt-mbgate-confgen
	install -Dm0755 $(BUILD_DIR)/$(TARGET) -t $(DESTDIR)$(PREFIX)/bin

.PHONY: all test clean
