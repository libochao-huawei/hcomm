# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11 -g -shared

# 目标文件名
TARGET = libbase.so

# 源文件和头文件
SRCS = base.cc
HDRS = base.h
OBJS = $(SRCS:.cc=.o)

# 默认目标
all: $(TARGET)

# 链接生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# 编译源文件生成目标文件
%.o: %.cc $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理编译生成的文件
clean:
	rm -f $(OBJS) $(TARGET)

# 伪目标（不代表真实文件）
.PHONY: all clean

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 显示编译选项帮助
help:
	@echo "Available targets:"
	@echo "  make        - 编译程序"
	@echo "  make clean  - 清理编译文件"
	@echo "  make run    - 编译并运行程序"
	@echo "  make help   - 显示此帮助信息"