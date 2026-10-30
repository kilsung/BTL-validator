default: all

.PHONY: all clean

VPORT_TARGET = vport

CXX ?= g++

LIB_PATH = ./lib
SRC_PATH = ./src
INC_PATH = ./inc
MODULE_PATH = ./module
OUT_PATH = $(SRC_PATH)

CPP11_ENABLED=1
#SHAREDLIB=1
#ONLY_FOR_DEV=1

INCLUDE_BASIC = -I. -I$(INC_PATH)
INCLUDE_MODULE = 

INCLUDES = $(INCLUDE_BASIC) $(INCLUDE_MODULE) $(INCLUDE_OBJS)

CFLAGS = -pthread

CFLAGS += $(INCLUDES)

ifeq ($(ONLY_FOR_DEV),1)
CFLAGS += -g
else
CFLAGS += -O3
endif

ifeq ($(SHAREDLIB),1)
CFLAGS += -fPIC
endif

ifeq ($(ONLY_FOR_DEV),1)
CFLAGS += -DONLY_FOR_DEV=1
endif

WARNING_FLAGS = -Wall -Werror -W
ifeq ($(CPP11_ENABLED),1)
WARNING_FLAGS += -Wno-deprecated-declarations
endif

CFLAGS += $(WARNING_FLAGS)
CFLAGS += -I$(LIBRARY_SERIAL_PATH)/include

CXXFLAGS = $(CFLAGS)

ifeq ($(CPP11_ENABLED),1)
CXXFLAGS += -std=c++11 -DCPP11_ENABLED=1
endif

LDFLAGS = -L. -L$(OUT_PATH)
LDFLAGS += 

OBJS = $(OUT_PATH)/vport.o $(OUT_PATH)/vport_main.o
TEST_OBJS = $(OUT_PATH)/vport_test.o $(OUT_PATH)/vport.o
TEST_TARGET = vport_test

all: $(VPORT_TARGET) $(TEST_TARGET)

$(VPORT_TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) -o $@ $(TEST_OBJS) $(CXXFLAGS) $(LDFLAGS)

%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o:%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o:%.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	find . -name "*.o" | xargs rm -f 
	rm -f $(VPORT_TARGET) $(TEST_TARGET) *.o 

