# YMODEM项目Makefile

# 编译器和标志
CC = gcc
# 基本编译标志
CFLAGS_COMMON = -Wall -Wextra
# 调试模式标志
CFLAGS_DEBUG = -g -DYMODEM_DEBUG_ENABLE=1 -O0
# 发布模式标志
CFLAGS_RELEASE = -O2 -DYMODEM_DEBUG_ENABLE=0
# 默认为调试模式
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_DEBUG)
LDFLAGS = -lrt

# 目录
SRC_DIR = src
INCLUDE_DIR = include
EXAMPLE_DIR = examples
BUILD_DIR = build
BUILD_DEBUG_DIR = $(BUILD_DIR)/debug
BUILD_RELEASE_DIR = $(BUILD_DIR)/release

# 头文件搜索路径
INCLUDES = -I$(INCLUDE_DIR)

# 源文件
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
EXAMPLE_FILES = $(wildcard $(EXAMPLE_DIR)/*.c)

# 目标文件 - 调试版本
OBJ_FILES_DEBUG = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DEBUG_DIR)/%.o,$(SRC_FILES))
EXAMPLE_OBJ_DEBUG = $(patsubst $(EXAMPLE_DIR)/%.c,$(BUILD_DEBUG_DIR)/%.o,$(EXAMPLE_FILES))

# 目标文件 - 发布版本
OBJ_FILES_RELEASE = $(patsubst $(SRC_DIR)/%.c,$(BUILD_RELEASE_DIR)/%.o,$(SRC_FILES))
EXAMPLE_OBJ_RELEASE = $(patsubst $(EXAMPLE_DIR)/%.c,$(BUILD_RELEASE_DIR)/%.o,$(EXAMPLE_FILES))

# 可执行文件
EXAMPLE_EXE_DEBUG = $(BUILD_DEBUG_DIR)/demo
EXAMPLE_EXE_RELEASE = $(BUILD_RELEASE_DIR)/demo

# 默认目标
all: debug

# 调试版本目标
debug: CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_DEBUG)
debug: directories_debug $(EXAMPLE_EXE_DEBUG)
	@echo "Debug build complete: $(EXAMPLE_EXE_DEBUG)"

# 发布版本目标
release: CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_RELEASE)
release: directories_release $(EXAMPLE_EXE_RELEASE)
	@echo "Release build complete: $(EXAMPLE_EXE_RELEASE)"

# 创建构建目录 - 调试版本
directories_debug:
	mkdir -p $(BUILD_DEBUG_DIR)

# 创建构建目录 - 发布版本
directories_release:
	mkdir -p $(BUILD_RELEASE_DIR)

# 编译库源文件 - 调试版本
$(BUILD_DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (debug): $<"

# 编译示例源文件 - 调试版本
$(BUILD_DEBUG_DIR)/%.o: $(EXAMPLE_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (debug): $<"

# 编译库源文件 - 发布版本
$(BUILD_RELEASE_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (release): $<"

# 编译示例源文件 - 发布版本
$(BUILD_RELEASE_DIR)/%.o: $(EXAMPLE_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (release): $<"

# 链接示例可执行文件 - 调试版本
$(EXAMPLE_EXE_DEBUG): $(OBJ_FILES_DEBUG) $(EXAMPLE_OBJ_DEBUG)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Linked (debug): $@"

# 链接示例可执行文件 - 发布版本
$(EXAMPLE_EXE_RELEASE): $(OBJ_FILES_RELEASE) $(EXAMPLE_OBJ_RELEASE)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Linked (release): $@"

# 清理构建文件
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory"

# 测试目标 - 调试版本
test_debug: $(EXAMPLE_EXE_DEBUG)
	./$(EXAMPLE_EXE_DEBUG)

# 测试目标 - 发布版本
test_release: $(EXAMPLE_EXE_RELEASE)
	./$(EXAMPLE_EXE_RELEASE)

# 构建静态库 - 调试版本
lib_debug: $(OBJ_FILES_DEBUG)
	ar rcs $(BUILD_DEBUG_DIR)/libymodem.a $^
	@echo "Created debug library: $(BUILD_DEBUG_DIR)/libymodem.a"

# 构建静态库 - 发布版本
lib_release: $(OBJ_FILES_RELEASE)
	ar rcs $(BUILD_RELEASE_DIR)/libymodem.a $^
	@echo "Created release library: $(BUILD_RELEASE_DIR)/libymodem.a"

# 安装 - 默认安装发布版本
install: lib_release
	mkdir -p /usr/local/include/ymodem
	cp $(INCLUDE_DIR)/*.h /usr/local/include/ymodem/
	cp $(BUILD_RELEASE_DIR)/libymodem.a /usr/local/lib/
	@echo "Installed library to /usr/local"

# 安装调试版本
install_debug: lib_debug
	mkdir -p /usr/local/include/ymodem
	cp $(INCLUDE_DIR)/*.h /usr/local/include/ymodem/
	cp $(BUILD_DEBUG_DIR)/libymodem.a /usr/local/lib/libymodem_debug.a
	@echo "Installed debug library to /usr/local"

# 卸载
uninstall:
	rm -rf /usr/local/include/ymodem
	rm -f /usr/local/lib/libymodem.a
	rm -f /usr/local/lib/libymodem_debug.a
	@echo "Uninstalled libraries"

# 打印帮助信息
help:
	@echo "YMODEM项目Makefile帮助"
	@echo "可用目标:"
	@echo "  all (默认)    - 同 debug"
	@echo "  debug         - 构建调试版本（启用调试输出和符号信息）"
	@echo "  release       - 构建发布版本（优化性能，禁用调试）"
	@echo "  clean         - 删除所有构建文件"
	@echo "  test_debug    - 测试调试版本"
	@echo "  test_release  - 测试发布版本"
	@echo "  lib_debug     - 构建调试版静态库"
	@echo "  lib_release   - 构建发布版静态库"
	@echo "  install       - 安装发布版库和头文件"
	@echo "  install_debug - 安装调试版库和头文件"
	@echo "  uninstall     - 卸载已安装的库和头文件"

.PHONY: all debug release directories_debug directories_release clean test_debug test_release lib_debug lib_release install install_debug uninstall help