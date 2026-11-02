# ---------------------------------------------------------------------------
# Modbus RTU-to-BACnet IP Gateway – Build system
# ---------------------------------------------------------------------------
# Standard : C++11
# Platform : Embedded Linux / Native Linux (POSIX)
# ---------------------------------------------------------------------------

# Detect build target architecture based on CXX compiler name
IS_ARM = $(findstring arm-poky,$(CXX))

ifeq ($(IS_ARM),)
    # x86 host build configuration
    BACSTAC_LIB_DIR = module/bacstac-32_7.11.3-main/x86
    JSONCPP_LIB_DIR = module/jsoncpp/x86
else
    # ARM cross-build configuration
    BACSTAC_LIB_DIR = module/bacstac-32_7.11.3-main/arm
    JSONCPP_LIB_DIR = module/jsoncpp/arm
endif

BACSTAC_DIR = module/bacstac-32_7.11.3-main

# Include directories
INCLUDES      = -I$(BACSTAC_DIR)/include/api \
                -I$(BACSTAC_DIR)/include/arch/linux/include \
                -I$(BACSTAC_DIR)/include/examples/utils \
                -Imodule/jsoncpp/include

# Compilation flags
CXXFLAGS      = -std=c++11 -Wall -Wextra -Wpedantic -Wno-psabi -pthread -g -O2 -DBACSTAC_GATEWAY $(INCLUDES)
CFLAGS        = -std=gnu99 -Wall -Wextra -Wno-psabi -pthread -g -O2 -DBACSTAC_GATEWAY $(INCLUDES)

# Out Of Service support (e.g. make OUT_OF_SERVICE=1 or make tpi OUT_OF_SERVICE=1)
ifeq ($(OUT_OF_SERVICE),1)
    CXXFLAGS += -DOUT_OF_SERVICE
    CFLAGS   += -DOUT_OF_SERVICE
endif

# Host tool to compile PICS template (.tpi -> .c)
CBNOBJ        = $(BACSTAC_DIR)/bin/cbnobj

# BACstac static libraries
BACSTAC_LIBS  = $(BACSTAC_LIB_DIR)/libbacsrv.a \
                $(BACSTAC_LIB_DIR)/libbacacc.a \
                $(BACSTAC_LIB_DIR)/libosl.a \
                $(BACSTAC_LIB_DIR)/exutils.a

LIBS          = -Wl,--start-group $(BACSTAC_LIBS) -Wl,--end-group \
                -L$(JSONCPP_LIB_DIR) -ljsoncpp -lpthread -lrt -lz

# ── Source groups ────────────────────────────────────────────────────────────

BACSTAC_CFG_OBJS = $(BACSTAC_LIB_DIR)/cfg-bip.o \
                   $(BACSTAC_LIB_DIR)/cfg-dl.o \
                   $(BACSTAC_LIB_DIR)/cfg-nl.o \
                   $(BACSTAC_LIB_DIR)/cfg-apil.o \
                   $(BACSTAC_LIB_DIR)/cfg-tsml.o \
                   $(BACSTAC_LIB_DIR)/cfg-logmsg.o \
                   $(BACSTAC_LIB_DIR)/cfg-osa.o

COMMON_SRCS  = src/common/modbus_crc.cpp \
               src/common/serial_port.cpp

GATEWAY_SRCS = src/gateway/main.cpp \
               src/gateway/bacnet_gateway.cpp \
               src/gateway/tpi_exporter.cpp \
               src/gateway/config_loader.cpp \
               src/gateway/modbus_client.cpp \
               src/gateway/cov_manager.cpp \
               src/gateway/fd_monitor.cpp \
               src/gateway/file_logger.cpp \
               src/gateway/dummy_device.c \
               $(COMMON_SRCS)

GATEWAY_OBJS = src/gateway/main.o \
               src/gateway/bacnet_gateway.o \
               src/gateway/tpi_exporter.o \
               src/gateway/config_loader.o \
               src/gateway/modbus_client.o \
               src/gateway/cov_manager.o \
               src/gateway/fd_monitor.o \
               src/gateway/file_logger.o \
               src/gateway/dummy_device.o \
               src/common/modbus_crc.o \
               src/common/serial_port.o

OBJS         = $(GATEWAY_OBJS) \
               $(BACSTAC_CFG_OBJS)

# ── Targets ──────────────────────────────────────────────────────────────────

GATEWAY_TARGET = bacnet_ip_gateway
TEST_TARGET    = verification/unit/run_tests
TPI_TARGET     = current_device.tpi

.PHONY: all clean test run_tests tpi gen2 cdu

all: $(GATEWAY_TARGET) $(TEST_TARGET)

# ── Model Profile Switching & Build ─────────────────────────────────────────
# make gen2 : model/gen2 설정 복사 및 기존 프로파일 정리 후 빌드
gen2:
	@echo "[Profile] Switching configuration to GEN2..."
	@mkdir -p conf/profiles
	@cp -f model/gen2/gateway.json conf/gateway.json
	@rm -f conf/profiles/*.json
	@cp -f model/gen2/lg_gen2.json conf/profiles/lg_gen2.json
	@echo "[Profile] Successfully configured GEN2 (conf/gateway.json, conf/profiles/lg_gen2.json)"
	@$(MAKE) all

# make cdu  : model/cdu 설정 복사 및 기존 프로파일 정리 후 빌드
cdu:
	@echo "[Profile] Switching configuration to CDU..."
	@mkdir -p conf/profiles
	@cp -f model/cdu/gateway.json conf/gateway.json
	@rm -f conf/profiles/*.json
	@cp -f model/cdu/lg_cdu_c.json conf/profiles/lg_cdu_c.json
	@echo "[Profile] Successfully configured CDU (conf/gateway.json, conf/profiles/lg_cdu_c.json)"
	@$(MAKE) all

# TPI Export Target
# make tpi : -DENABLE_TPI_EXPORT 매크로로 빌드 후 실행하여 current_device.tpi 자동 생성
tpi: CXXFLAGS += -DENABLE_TPI_EXPORT
tpi: CFLAGS += -DENABLE_TPI_EXPORT
tpi: clean $(GATEWAY_TARGET)
	@echo "\n[TPI] Generating $(TPI_TARGET)..."
	./$(GATEWAY_TARGET) ./conf
	@echo "[TPI] Generation complete: $(TPI_TARGET)\n"

# TPI compilation rules (dummy_device.tpi -> dummy_device.c)
#src/gateway/dummy_device.c: src/gateway/dummy_device.tpi
#	$(CBNOBJ) $< $@

# Object file rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/gateway/dummy_device.o: src/gateway/dummy_device.c
	$(CC) $(CFLAGS) -c $< -o $@

# Target application compilation
$(GATEWAY_TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# ── Unit Tests (BACstac-independent, host compiler only) ─────────────────────
# 단위 테스트는 BACstac 라이브러리에 의존하지 않으므로 호스트 g++ 로 직접 빌드합니다.
# 크로스 컴파일 환경과 독립적으로 실행 가능합니다.
#
# 사용법:
#   make test       : 테스트 바이너리 빌드 + 즉시 실행
#   make run_tests  : 이미 빌드된 테스트 바이너리만 실행

HOST_CXX    ?= g++
TEST_CXXFLAGS = -std=c++11 -Wall -Wextra -Wpedantic -pthread -O0 -g \
                -Isrc -Iverification/unit

TEST_TARGET = verification/unit/run_tests

TEST_SRCS   = verification/unit/test_main.cpp \
              verification/unit/test_modbus_crc.cpp \
              verification/unit/test_modbus_frame_builder.cpp \
              verification/unit/test_mock_serial_port.cpp \
              verification/unit/test_config_parser.cpp \
              verification/unit/test_bip_config_path.cpp \
              verification/unit/test_bip_config_file.cpp \
              verification/unit/test_fd_link_state.cpp \
              src/common/modbus_crc.cpp

test: $(TEST_TARGET)
	@echo "\n[Test] Running all unit tests..."
	@mkdir -p verification/log
	./$(TEST_TARGET) | tee verification/log/run_tests.log

$(TEST_TARGET): $(TEST_SRCS)
	@echo "[Test] Compiling unit tests (host g++)..."
	$(HOST_CXX) $(TEST_CXXFLAGS) -o $@ $^

run_tests: $(TEST_TARGET)
	@mkdir -p verification/log
	./$(TEST_TARGET) | tee verification/log/run_tests.log

# ── Clean ─────────────────────────────────────────────────────────────────────
# 일반 clean: 빌드 산출물(.o, 바이너리)만 정리하고, 사전 생성된 dummy_device.c는 보존합니다.
clean:
	rm -f $(GATEWAY_TARGET) $(GATEWAY_OBJS) \
	      src/common/*.o src/gateway/*.o $(TEST_TARGET) $(TPI_TARGET)
	rm -rf verification/log verification/scripts/__pycache__

# 완전 clean: 사전 생성된 dummy_device.c/h 까지 모두 삭제하여 cbnobj 재실행을 강제합니다.
distclean: clean
	rm -f src/gateway/dummy_device.c src/gateway/dummy_device.h

