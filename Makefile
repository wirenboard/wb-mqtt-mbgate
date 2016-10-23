start_red=$(shell echo -e '\033[31m')
end_color=$(shell echo -e '\033[0m')
BUILDCLEAN_HACK =

# Check that tree is clean before building
# ifeq ($(shell git diff-index --quiet HEAD --; echo $$?), 1)
# $(info $(start_red))
# $(info WARNING: Tree is dirty! Think twice before build it in production!)
# $(info $(end_color))
# BUILDCLEAN_HACK +=n
# endif


TARGET = wb-mqtt-mbgate
OBJS = main.o logging.o config_parser.o
SRC_DIR = src

COMMON_OBJS = i_modbus_server_observer.o i_modbus_backend.o modbus_wrapper.o mqtt_converters.o observer.o mqtt_fastwrapper.o

TEST_DIR = test
TEST_OBJS = main.o address_range_test.o modbus_server_test.o mock_modbus_observer.o converters_test.o mock_mqtt_client.o gateway_test.o mqtt_fastwrapper_test.o multi_unitid_test.o
TEST_TARGET = test-app
TEST_LDFLAGS = -lgtest -lpthread -lgmock
TEST_CXXFLAGS = -I. -O0 -I$(SRC_DIR)

TEST_OBJS := $(patsubst %, $(TEST_DIR)/%, $(TEST_OBJS))
COMMON_OBJS := $(patsubst %, $(SRC_DIR)/%, $(COMMON_OBJS))
OBJS := $(patsubst %, $(SRC_DIR)/%, $(OBJS))

CXX=g++
LD=g++
LDFLAGS=-lmodbus -lmosquittopp -lwbmqtt -ljsoncpp -llog4cpp -pthread 
CXXFLAGS=-std=c++0x -Wall -Werror

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
	valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_TARGET)  || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>&2; \
			exit 1; \
		fi

$(TEST_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

distclean: clean

clean:
	rm -rf $(SRC_DIR)/*.o $(TARGET) $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_TARGET)

install: all
	mkdir -p $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin

	install -m 0644 wb-mqtt-mbgate.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-mbgate.schema.json
	
	install -m 0755 utils/generate_config.py $(DESTDIR)/usr/bin/wb-mqtt-mbgate-confgen
	install -m 0755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)


.PHONY: all test clean
