import os
import clang.cindex
from clang.cindex import CursorKind
import argparse
import re
import shutil
from typing import List, Dict, Optional
import json
import shlex

# -------------------------- 配置与初始化 --------------------------
def init_libclang():
    if os.name == "nt":  # Windows 系统
        lib_path = r"C:\Program Files\LLVM\bin\libclang.dll"
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"未找到 libclang.dll: {lib_path}")
        clang.cindex.Config.set_library_file(lib_path)
    elif os.name == "posix":  # Linux/Mac 系统
        lib_path = "/usr/lib/llvm-18/lib/libclang.so"  # 替换为实际路径
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"未找到 libclang.so: {lib_path}")
        clang.cindex.Config.set_library_file(lib_path)
    return clang.cindex.Index.create()

# -------------------------- files路径处理 --------------------------
def get_target_files(input_paths):
    """收集所有需要处理的files(.c 和 .h),支持目录递归"""
    target_files = []
    for path in input_paths:
        if not os.path.exists(path):
            print(f"警告: 路径不存在 - {path},已跳过")
            continue
        if os.path.isfile(path):
            # 检查files扩展名是否为 .c 或 .h
            if path.lower().endswith((".c", ".h")):
                target_files.append(os.path.abspath(path))
            else:
                print(f"警告: 不支持的files类型 - {path},已跳过(仅处理 .c 和 .h)")
        elif os.path.isdir(path):
            # 递归遍历目录
            for root, _, files in os.walk(path):
                for file in files:
                    if file.lower().endswith((".c", ".h")):
                        file_path = os.path.join(root, file)
                        target_files.append(os.path.abspath(file_path))
    # 去重
    return list(dict.fromkeys(target_files))

# -------------------------- 筛选规则 --------------------------
def is_system_identifier(cursor):
    """判断是否为系统提供的标识符(系统头files或无files关联)"""
    loc = cursor.location
    if not loc.file:  # 无files关联(如关键字、内置类型)
        return True
    file_path = loc.file.name.lower()
    # 系统头files特征: 包含系统目录或标准库路径
    system_dirs = [
        "/usr/", "external_depends/", "tls_adp/", "opensdk/", "include/", "Ascend/"
    ]
    # 需要修改的用户目录
    my_dirs = ["hccp/common/", "hccp/hccp_service/", "hccp/rdma_service/", "hccp/rdma_agent/", "hccp/test.c"]

    is_user_file = any(d in file_path for d in my_dirs)
    is_system_file = any(d in file_path for d in system_dirs)
    return is_system_file or not is_user_file

def is_underscore_naming(name):
    """判断是否符合下划线命名规则(必须包含至少一个下划线),支持带struct/union前缀的情况"""
    # 处理前缀:匹配开头的"struct"或"union",以及后续的空白字符(空格、制表符等)
    # 提取前缀后的核心标识符部分
    prefix_pattern = r'^(struct|union)\s+(.*)$'
    match = re.match(prefix_pattern, name)
    if match:
        # 若存在前缀,取空白后的标识符部分作为检查对象
        name = match.group(2)
    
    # 1. 名称非空,首字符小写,末尾不是下划线,且至少包含一个下划线
    if not name or not name[0].islower() or name[-1] == "_" or "_" not in name:
        return False
    # 2. 所有字符必须是小写字母、数字或单下划线
    for c in name:
        if not (c.islower() or c.isdigit() or c == "_"):
            return False
    # 3. 不允许连续下划线
    return "__" not in name

def should_skip_identifier(cursor, name):
    """判断是否需要跳过该标识符"""
    if is_system_identifier(cursor):
        return True
    # 跳过以_s结尾的函数
    if cursor.kind == CursorKind.FUNCTION_DECL and name.endswith("_s"):
        return True
    return False

# -------------------------- 遍历 AST 收集标识符 --------------------------
def collect_identifiers(tu):
    identifiers = {}
    macro_set = set()
    # 目标节点类型: 包含声明和引用
    PREFIX_PATTERN = re.compile(r'^(struct|union|enum)\s+')

    target_kinds = [
        # 声明节点
        CursorKind.FUNCTION_DECL, CursorKind.VAR_DECL,
        CursorKind.STRUCT_DECL, CursorKind.UNION_DECL,
        CursorKind.ENUM_DECL, CursorKind.ENUM_CONSTANT_DECL,
        CursorKind.FIELD_DECL, CursorKind.TYPEDEF_DECL,
        CursorKind.PARM_DECL,
        # 引用节点
        CursorKind.DECL_REF_EXPR, CursorKind.MEMBER_REF_EXPR,
        CursorKind.TYPE_REF
    ]

    small_camel_kinds = [
        # 声明节点
        CursorKind.VAR_DECL,CursorKind.FIELD_DECL,
        CursorKind.PARM_DECL,CursorKind.MEMBER_REF_EXPR
    ]
    def is_macro(cursor):
        loc = cursor.location
        if loc.file is None:
            return False
        return f"{loc.file.name}:{loc.line}:{loc.column}" in macro_set
    def get_identifier_name(cursor):
        """获取标识符名称,自动剥离struct/union前缀及空白"""
        if cursor.kind == CursorKind.MEMBER_REF_EXPR:
            raw_name = cursor.referenced.spelling if cursor.referenced else None
        else:
            raw_name = cursor.spelling
        
        if not raw_name:
            return None
        
        # 剥离前缀(struct/union + 空白),只保留核心标识符
        match = PREFIX_PATTERN.match(raw_name)
        if match:
            # 前缀匹配成功,返回前缀后的核心名称
            return raw_name[match.end():]
        return raw_name

    def get_position(cursor):
        loc = cursor.location
        if not loc.file or not loc.file.name:
            return None
        line = loc.line
        column = loc.column + 1  # 转换为1-based列号
        name = get_identifier_name(cursor)
        return (line, column, len(name)) if name else None

    def skip_rules(cursor, name):
        if not name:
            return True
        if should_skip_identifier(cursor, name):
            return True
        if not is_underscore_naming(name):
            return True
        return False

    def process_macro(cursor):
        return
    def traverse(cursor):
        if is_macro(cursor):
            process_macro(cursor)
            return
        origin_cursor = cursor.get_definition()
        loc = cursor.location
        if cursor.kind == CursorKind.MACRO_INSTANTIATION:
            if loc.file is None:
                return
            macro_set.add(f"{loc.file.name}:{loc.line}:{loc.column}")
            return
        pos = get_position(cursor)
        name = get_identifier_name(cursor)

        if cursor.kind == CursorKind.DECL_REF_EXPR or cursor.kind == CursorKind.TYPE_REF or cursor.kind == CursorKind.MEMBER_REF_EXPR:
            if not cursor.referenced:
                print(f"[ERROR] exist undefined reference! position: {cursor.location}")
                exit(77)
            kind = cursor.referenced.kind
            if origin_cursor:
                should_skip = skip_rules(origin_cursor, name)
            else:
                should_skip = True
        else :
            kind = cursor.kind
            should_skip = skip_rules(cursor, name)

        if kind in target_kinds and not should_skip and pos:
            if kind not in small_camel_kinds:
                new_name = to_camel_case(name, True)
            else :
                new_name = to_camel_case(name, False)

            if name not in identifiers:
                identifiers[name] = {"positions": set(), "new_name": new_name}
            identifiers[name]["positions"].add(pos)
        # 递归遍历子节点
        for child in cursor.get_children():
            traverse(child)

    traverse(tu.cursor)
    return identifiers

def to_camel_case(underscore_name, is_big_camel):
    words = underscore_name.split("_")
    ret = "".join(word.capitalize() for word in words)
    if not is_big_camel:
        return ret[0].lower() + ret[1:]
    return ret

# -------------------------- 基于语法树替换源files中的标识符 --------------------------
def replace_in_file(file_path, identifiers, backup=False):
    """替换标识符,支持 .h 和 .c files(修复位置偏移问题)"""
    with open(file_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    # 收集所有需要替换的操作: (-行号, -列号, 行号, 列号, 长度, 新名称)
    # 用负行号/列号排序,确保按行倒序、同行列倒序处理
    all_changes = []
    for name, data in identifiers.items():
        new_name = data["new_name"]
        for (line_num, col, length) in data["positions"]:
            all_changes.append((-line_num, -col, line_num, col, length, new_name))
            # print(line_num, col, length, new_name)

    # 按行号倒序、列号倒序排序(避免替换后位置偏移)
    all_changes.sort()

    # 执行替换
    for _, _, line_num, col, length, new_name in all_changes:
        line_idx = line_num - 1  # 转换为0-based索引
        col_idx = col -1
        if line_idx < 0 or line_idx >= len(lines):
            continue  # 跳过无效行
        line = lines[line_idx]
        # 检查列号是否超出当前行长度(避免索引错误)
        if col_idx - 1 + length > len(line):
            continue
        # 替换标识符
        new_line = line[:col_idx-1] + new_name + line[col_idx-1 + length:]
        lines[line_idx] = new_line

    # 处理备份
    if backup:
        backup_path = f"{file_path}.bak"
        os.rename(file_path, backup_path)
        print(f"backup: {backup_path}")

    # 写入替换后的内容
    with open(file_path, "w", encoding="utf-8") as f:
        f.writelines(lines)
    print(f"finish: {file_path}")

# -------------------------- 根据字符串的映射全面替换源files中的字符串 --------------------------
def replace_in_file_by_string(file_path, identifiers, backup=False):
    """
    用正则匹配C语言注释和字符串,仅在有效代码区替换字符串
    
    参数:
        file_path (str): 目标files路径
        identifiers (dict): 替换映射,格式同前
        backup (bool): 是否备份原files
    """
    # 提取替换映射(忽略positions)
    replacements = {old: info["new_name"] for old, info in identifiers.items()}
    if not replacements:
        return

    # 读取files内容
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 正则模式:匹配需要排除的部分(注释、字符串)
    # 顺序:多行注释 > 单行注释 > 字符串(避免优先级冲突)
    pattern = re.compile(
        r'(/\*.*?\*/)|'          # 多行注释 /*...*/(非贪婪匹配,支持跨多行)
        r'(//.*?$)|'             # 单行注释 //...(到行尾)
        r'("(?:\\.|[^"\\])*")',  # 字符串 "..."(支持转义字符 \")
        re.DOTALL | re.MULTILINE # DOTALL让.匹配换行(多行注释)；MULTILINE让$匹配行尾
    )

    # 按模式分割内容:结果为 [有效代码, 排除内容, 有效代码, 排除内容, ...]
    parts = pattern.split(content)

    # 按字符串长度降序排序替换目标(避免短串替换干扰长串)
    sorted_replacements = sorted(replacements.items(), key=lambda x: len(x[0]), reverse=True)

    # 处理分割后的部分:仅替换有效代码区
    processed_parts = []
    for part in parts:
        if part is None:  # split会产生None(来自分组),直接跳过
            continue
        # 判断当前部分是否为"有效代码"(即未被正则匹配的部分)
        if not pattern.match(part):
            # 执行替换
            for old_str, new_str in sorted_replacements:
                part = part.replace(old_str, new_str)
        processed_parts.append(part)

    # 拼接处理后的内容
    new_content = ''.join(processed_parts)

    # 备份原files
    if backup:
        shutil.copy2(file_path, f"{file_path}.bak")

    # 写入结果
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

# -------------------------- 打印AST --------------------------
def print_ast(tu, sys, max_depth=10):
    """
    打印TranslationUnit的AST结构(带颜色区分)
    :param tu: 解析后的TranslationUnit对象
    :param max_depth: 最大递归深度(避免层级过深导致输出过长)
    """
    # ANSI颜色转义序列
    COLOR_RESET = "\033[0m"       # 重置颜色
    COLOR_KIND = "\033[94m"       # 节点类型: 蓝色
    COLOR_SPELLING = "\033[92m"   # 节点名称: 绿色
    COLOR_LOCATION = "\033[93m"   # 位置信息: 黄色
    COLOR_TYPE = "\033[96m"       # 类型信息: 青色

    def get_location_str(loc):
        """格式化位置信息(files、行、列)"""
        if not loc.file:
            return "None"
        return f"{loc.file.name}:{loc.line}:{loc.column}"

    def traverse(cursor, sys, depth=0):
        if depth > max_depth:
            return
        indent = "   " * depth  # 层级缩进

        # 节点核心信息
        kind = cursor.kind
        spelling = cursor.spelling or "None"
        location = get_location_str(cursor.location)
        type_cursor = cursor.type.get_canonical()  # 规范化类型

        # 打印节点基本信息
        if sys:
            print(
                f"{indent}[{COLOR_KIND}{kind}{COLOR_RESET}] "
                f"{COLOR_SPELLING}{spelling}{COLOR_RESET} "
                f"({COLOR_LOCATION}{location}{COLOR_RESET})"
            )
            print(f"{indent}{COLOR_TYPE}type: {type_cursor.spelling}{COLOR_RESET}")
        elif not is_system_identifier(cursor):
            print(
                f"{indent}[{COLOR_KIND}{kind}{COLOR_RESET}] "
                f"{COLOR_SPELLING}{spelling}{COLOR_RESET} "
                f"({COLOR_LOCATION}{location}{COLOR_RESET})"
            )
            print(f"{indent}{COLOR_TYPE}type: {type_cursor.spelling}{COLOR_RESET}")

        # 递归遍历子节点
        for child in cursor.get_children():
            traverse(child, sys, depth + 1)

    # 从根节点开始遍历
    traverse(tu.cursor, sys)

# -------------------------- 解析编译命令json文件 --------------------------
def load_compile_args_from_json(json_path: str) -> Dict:

    if not os.path.exists(json_path):
        raise FileNotFoundError(f"JSON文件不存在: {json_path}")
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        # 确保返回的字典包含所有必要键，设置合理默认值
        return {
            'std': data.get('std'),  # 保持原有逻辑，可能为None（使用默认c99）
            'defines': data.get('defines', []),  # 列表类型，默认空列表
            'includes': data.get('includes', []),  # 列表类型，默认空列表
            'coption': data.get('coption', '')  # 新增字段，字符串类型，默认空字符串
        }
    except json.JSONDecodeError as e:
        raise ValueError(f"JSON格式错误: {e}")
    except Exception as e:
        raise RuntimeError(f"读取JSON文件失败: {e}")

# -------------------------- 解析编译参数--------------------------
def parse_compile_args(args) -> List[str]:
    """解析命令行参数和JSON配置，返回最终的编译参数列表
    
    Args:
        args: 命令行参数对象（需包含 std、define、include、command、print_ccargs 字段）
    
    Returns:
        List[str]: 完整的编译参数列表
    """
    # 1. 初始化默认参数
    json_args: Dict = {'std': None, 'defines': [], 'includes': [], 'coption': ''}
    
    # 2. 从JSON文件加载参数（如果指定）
    if args.command:
        try:
            json_args = load_compile_args_from_json(args.command)
            print(f"已从 {args.command} 加载编译参数")
        except Exception as e:
            print(f"警告:加载JSON文件失败,忽略JSON参数: {e}")
    
    # 3. 处理编译标准（优先级：命令行 > JSON > 默认c99）
    final_std = args.std if args.std is not None else (json_args.get('std') or "c99")
    
    # 4. 处理宏定义（JSON + 命令行，命令行在后优先级更高）
    json_defines = json_args.get('defines', [])
    json_defines = json_defines if isinstance(json_defines, list) else []
    cli_defines = args.define if isinstance(args.define, list) else []
    final_defines = json_defines + cli_defines
    
    # 5. 处理头文件路径（JSON + 命令行，命令行在后优先级更高）
    json_includes = json_args.get('includes', [])
    json_includes = json_includes if isinstance(json_includes, list) else []
    cli_includes = args.include if isinstance(args.include, list) else []
    final_includes = json_includes + cli_includes
    
    # 6. 处理额外编译选项（解析JSON中的coption字符串）
    json_coption = json_args.get('coption', '')
    final_coptions: List[str] = []
    if isinstance(json_coption, str) and json_coption.strip():
        try:
            final_coptions = shlex.split(json_coption.strip())
        except Exception as e:
            print(f"警告:解析coption失败,忽略该字段: {e}")
    
    # 7. 构建最终编译参数（过滤无效参数）
    compile_args = [f"-std={final_std}"]
    # 添加头文件路径
    compile_args.extend([
        f"-I{path}" for path in final_includes 
        if path and isinstance(path, str)
    ])
    # 添加宏定义
    compile_args.extend([
        f"-D{d}" for d in final_defines 
        if d and isinstance(d, str)
    ])
    # 添加额外编译选项
    compile_args.extend(final_coptions)
    
    return compile_args


# -------------------------- 主函数 --------------------------
def main():
    parser = argparse.ArgumentParser(description="将C/H代码中的下划线命名转换为大驼峰命名")
    parser.add_argument("paths", nargs="+", help="目标files或目录路径")
    parser.add_argument("--backup", action="store_true", help="是否备份原files")
    parser.add_argument("--std", default=None, help="指定C标准")
    parser.add_argument("-D", "--define", action="append", help="添加宏定义")
    parser.add_argument("-I", "--include", action="append", help="添加头files搜索路径")
    parser.add_argument("--print_ast", "--p", action="store_true", help="是否打印语法树")
    parser.add_argument("--replace", "--r", action="store_true", help="是否进行files替换")
    parser.add_argument("--string", "--s", action="store_true", help="是否按文本替换")
    parser.add_argument("--filter", choices=['onlyc', 'onlyh'], 
                        help="过滤files类型,可选值:onlyc(只处理.c)、onlyh(只处理.h)")
    parser.add_argument("--sys", action="store_true", help="语法树中是否打印系统标识符")
    parser.add_argument("--command", "-c", help="从JSON文件读取编译参数(如--command command.json)")
    parser.add_argument("--print_ccargs", "--pc",action="store_true",help="打印编译参数")
    args = parser.parse_args()

    # 收集所有目标files(.c 和 .h)
    target_files = get_target_files(args.paths)
    if not target_files:
        print("未找到需要处理的 .c 或 .h files")
        return
    
    # 根据过滤选项筛选files
    if args.filter == 'onlyc':
        target_files = [f for f in target_files if f.lower().endswith('.c')]
        print(f"total {len(target_files)}  .c file")
    elif args.filter == 'onlyh':
        target_files = [f for f in target_files if f.lower().endswith('.h')]
        print(f"total {len(target_files)}  .h files")
    
    if not target_files:
        print("no files")
        return
    
    # 文件排序，优先处理.c files,再处理.h files
    target_files.sort(key=lambda x: 0 if x.lower().endswith('.c') else 1)
    print(f"共 {len(target_files)} 个files待处理,处理顺序:.c files优先于 .h files\n")

    # 构建编译参数
    compile_args = parse_compile_args(args)

    if args.print_ccargs:
        print("========编译参数=========")
        for ar in compile_args:
            print(f"\033[92m{ar}\033[0m")

    # 初始化libclang
    try:
        index = init_libclang()
    except Exception as e:
        print(f"初始化libclang失败: {e}")
        return

    # 逐个处理files
    for file_path in target_files:
        print(f"\n===== File: {file_path} =====")
        # 解析files
        parse_options = clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        try:
            tu = index.parse(file_path, args=compile_args, options=parse_options)
        except Exception as e:
            print(f"解析files失败,已跳过: {e}")
            continue
        
        if args.print_ast:
            print_ast(tu, args.sys)
        
        # 收集标识符
        identifiers = collect_identifiers(tu)
        if not identifiers:
            print("no identifiers to transform")
            continue

        # 打印转换计划
        print("synmbol transform rules: ")
        for name, data in identifiers.items():
            print(f"    {name} → {data['new_name']}(appeared {len(data['positions'])} times)")

        # 执行替换
        if args.replace:
            if args.string:
                replace_in_file_by_string(file_path, identifiers, backup=args.backup)
            else:
                replace_in_file(file_path, identifiers, backup=args.backup)


    print("\ndone")

if __name__ == "__main__":
    main()