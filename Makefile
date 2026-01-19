CROSS_COMPILE ?= aarch64-linux-gnu-
CC  := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
AR  := $(CROSS_COMPILE)ar
RM  := rm -f

TARGET      := testRTSPClient

# =====================================================
#  目录
# =====================================================
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
OUTPUT_DIR := output
LIB_DIR := libs

# =====================================================
#  编译选项
# =====================================================
CFLAGS   := -Wall -O2 -fPIC \
	-I$(INC_DIR) \
	-I$(INC_DIR)/external/include/live
CXXFLAGS := -Wall -O2 -fPIC \
	-I$(INC_DIR) \
	-I$(INC_DIR)/external/include/live

LDFLAGS := -L$(LIB_DIR)
LDLIBS  := \
	-Wl,--start-group \
	-lliveMedia \
	-lgroupsock \
	-lUsageEnvironment \
	-lBasicUsageEnvironment \
	-Wl,--end-group \
	-lpthread

# =====================================================
#  源文件
# =====================================================
C_SRCS   := $(wildcard $(SRC_DIR)/*.c)
CPP_SRCS := $(wildcard $(SRC_DIR)/*.cpp)

OBJS := \
	$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SRCS)) \
	$(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))

# =====================================================
all: prepare app

prepare:
	@mkdir -p $(OBJ_DIR) $(OUTPUT_DIR)

app: $(OUTPUT_DIR)/$(TARGET)

$(OUTPUT_DIR)/$(TARGET): $(OBJS)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

#  静态库
# static: $(LIB_DIR)/$(STATIC_LIB)

# $(LIB_DIR)/$(STATIC_LIB): $(OBJS)
# 	$(AR) rcs $@ $^

#  动态库
# shared: $(LIB_DIR)/$(SHARED_LIB)

# $(LIB_DIR)/$(SHARED_LIB): $(OBJS)
#	$(CXX) -shared -o $@ $^

# =====================================================
#  编译规则
# =====================================================
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR) $(OUTPUT_DIR) $(LIB_DIR)

.PHONY: all prepare clean static shared app
