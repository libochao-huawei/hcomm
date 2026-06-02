# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if(NOT INPUT)
    message(FATAL_ERROR "INPUT is required")
endif()
if(NOT OUTPUT)
    message(FATAL_ERROR "OUTPUT is required")
endif()
if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "Input file does not exist: ${INPUT}")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
configure_file("${INPUT}" "${OUTPUT}" COPYONLY)


  整体

  - BasedOnStyle: Google：以 Google C++ 格式为基础。
  - ColumnLimit: 120：单行最长 120 列，超过会尝试换行。
  - SortIncludes: false：不自动排序 #include。
  - UseTab: Never：不用 tab，全用空格。
  - IndentWidth: 4 / TabWidth: 4：缩进宽度 4 个空格。

  访问控制缩进

  - AccessModifierOffset: -4：public:、private:、protected: 比类成员少缩进 4 格，通常贴近类体左侧：

  class A {
  public:
      void Foo();
  private:
      int x_;
  };

  大括号

  - BreakBeforeBraces: Custom：使用自定义大括号规则。
  - AfterFunction: true：函数定义左大括号换行：

  void Foo()
  {
      ...
  }

  - AfterClass/Struct/Enum/Namespace/ControlStatement: false：类、结构体、枚举、命名空间、if/for/while 等大括号不换行：

  if (ok) {
      Run();
  }

  class A {
  };

  - BeforeElse: false / BeforeCatch: false：else、catch 不单独换行：

  } else {
  }

  - SplitEmptyFunction: false：空函数可保持一行：

  void Foo() {}

  - SplitEmptyRecord: true、SplitEmptyNamespace: true：空类/结构体、空 namespace 会拆开。

  换行和对齐

  - AlignAfterOpenBracket: AlwaysBreak：函数调用/声明等在左括号后更倾向换行并对齐。
  - AlignOperands: true：多行表达式操作符对齐。
  - AlignTrailingComments: true：行尾注释对齐。
  - AlignEscapedNewlines: Left：反斜杠续行左对齐。
  - AlwaysBreakTemplateDeclarations: true：模板声明总是换行，例如：

  template <typename T>
  void Foo(T value);

  参数布局

  - BinPackArguments: true：函数调用参数允许尽量塞到同一行。
  - AllowAllArgumentsOnNextLine: true：如果换行，允许所有实参放到下一行。
  - AllowAllParametersOfDeclarationOnNextLine: true：函数声明参数同理。

  指针/引用风格

  - DerivePointerAlignment: false：不从已有代码推断指针风格。
  - PointerAlignment: Left：* 靠近类型：


  int* p;
  const char* s;

  短语句

  - AllowShortFunctionsOnASingleLine: true：短函数允许单行。
  - AllowShortBlocksOnASingleLine: false：普通短代码块不放单行。
  - AllowShortIfStatementsOnASingleLine: false：短 if 不放单行。
  - AllowShortLoopsOnASingleLine: false：短循环不放单行。
  - AllowShortCaseLabelsOnASingleLine: false：短 case 不放单行。

  一句话总结：这个配置是 Google 风格 + 4 空格缩进 + 120 列 + 函数大括号另起一行 + 控制语句/类/namespace 大括号同行 + include 不排序 + 指针星号靠类型。