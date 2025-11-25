import os
import clang.cindex
from clang.cindex import CursorKind
import argparse
import re
import shutil
from typing import List, Dict, Optional, FrozenSet, Tuple, Set
import json
import shlex
from pathlib import Path
from clang.cindex import Cursor, CursorKind, TranslationUnit

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
    my_dirs = ["hccp/common/", "hccp/hccp_service/", "hccp/rdma_service/", "hccp/rdma_agent/", "hccp/test.c",
        "inc/network", "inc/private/network"
    ]

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

def should_skip_identifier(cursor):
    if is_system_identifier(cursor):
        return True
    return False

def extract_valid_vars_from_arg_for_type(
    arg_text: str,
    local_vars: Dict[str, Tuple[Set[Tuple[int, int, int]], str, bool]]  # 中间结构：保留is_in_current标记
) -> List[Tuple[str, int, int]]:
    """不变：提取宏参数中存在于local_vars的所有标识符（含非本文件，后续过滤）"""
    valid_vars = []
    if not arg_text or not local_vars:
        return valid_vars

    identifier_pattern = re.compile(r'\b[a-zA-Z_][a-zA-Z0-9_]*\b')
    for match in identifier_pattern.finditer(arg_text):
        var_name = match.group()
        if var_name in local_vars:
            start_idx = match.start()
            end_idx = match.end() - 1
            valid_vars.append((var_name, start_idx, end_idx))
    
    return valid_vars


def find_variable_positions_in_arg_for_type(
    arg_text: str,
    arg_start_offset: int,
    arg_end_offset: int,
    char_positions: List[Tuple[int, int]],
    valid_vars: List[Tuple[str, int, int]]
) -> List[Tuple[str, int, int, int]]:
    """不变：保持原有有效性校验逻辑"""
    var_positions = []
    arg_len = len(arg_text)
    if arg_len == 0 or not valid_vars or not char_positions:
        return var_positions

    valid_region = [False] * arg_len
    in_double_quote = False
    in_single_quote = False
    in_block_comment = False
    in_line_comment = False

    idx = 0
    while idx < arg_len:
        char = arg_text[idx]
        next_char = arg_text[idx + 1] if idx + 1 < arg_len else ''

        if not in_double_quote and not in_single_quote:
            if not in_block_comment and not in_line_comment and char == '/' and next_char == '*':
                in_block_comment = True
                idx += 2
                continue
            elif in_block_comment and char == '*' and next_char == '/':
                in_block_comment = False
                idx += 2
                continue
            elif not in_block_comment and not in_line_comment and char == '/' and next_char == '/':
                in_line_comment = True
                idx += 2
                continue
            elif in_line_comment and char == '\n':
                in_line_comment = False
                idx += 1
                continue

        if not in_block_comment and not in_line_comment:
            if not in_single_quote and char == '"':
                in_double_quote = not in_double_quote
            elif not in_double_quote and char == "'":
                in_single_quote = not in_single_quote

        valid_region[idx] = not (in_block_comment or in_line_comment or in_double_quote or in_single_quote)
        idx += 1

    for (var_name, var_start_idx, var_end_idx) in valid_vars:
        var_len = var_end_idx - var_start_idx + 1

        if var_start_idx < 0 or var_end_idx >= arg_len:
            continue
        if not all(valid_region[var_start_idx:var_end_idx + 1]):
            continue

        prev_char = arg_text[var_start_idx - 1] if var_start_idx > 0 else ''
        next_char_var = arg_text[var_end_idx + 1] if var_end_idx + 1 < arg_len else ''
        if (prev_char.isalnum() or prev_char == '_') or (next_char_var.isalnum() or next_char_var == '_'):
            continue

        var_total_start = arg_start_offset + var_start_idx
        var_total_end = arg_start_offset + var_end_idx
        if var_total_start >= len(char_positions) or var_total_end > arg_end_offset:
            continue

        var_start_line, var_start_col = char_positions[var_total_start]
        var_positions.append((var_name, var_start_line, var_start_col, var_len))

    return var_positions


def process_macro_arguments_for_type(
    tu: TranslationUnit,
    current_file_path: str,
    local_vars: Dict[str, Tuple[Set[Tuple[int, int, int]], str, bool]]  # 输入：带is_in_current标记
) -> Dict[str, Tuple[Set[Tuple[int, int, int]], str, bool]]:
    """不变：仅更新本文件标识符的位置，保留所有条目供后续过滤"""
    if not local_vars:
        return local_vars

    lines = get_file_lines(current_file_path)
    if not lines:
        return local_vars

    macro_instances = []
    current_file = Path(current_file_path).resolve(strict=True)

    def collect_macros(cursor: Cursor) -> None:
        loc = cursor.location
        if loc.file is None:
            for child_cursor in cursor.get_children():
                collect_macros(child_cursor)
            return
        try:
            node_file = Path(loc.file.name).resolve(strict=False)
            if node_file == current_file and cursor.kind == CursorKind.MACRO_INSTANTIATION and loc.line > 0:
                macro_instances.append((loc.line, loc.column))
        except OSError:
            if Path(loc.file.name).absolute() == current_file:
                if cursor.kind == CursorKind.MACRO_INSTANTIATION and loc.line > 0:
                    macro_instances.append((loc.line, loc.column))

        for child_cursor in cursor.get_children():
            collect_macros(child_cursor)

    collect_macros(tu.cursor)
    FORBIDDEN_WHITESPACE = {' ', '\t', '\n'}

    for (macro_line, macro_col) in macro_instances:
        left_paren_pos = find_left_paren_in_file(lines, macro_line, macro_col)
        if not left_paren_pos:
            continue
        left_line, left_col = left_paren_pos

        a_char_pos: Optional[Tuple[int, int]] = None
        for line_num in range(left_line, macro_line - 1, -1):
            line_idx = line_num - 1
            if line_idx < 0 or line_idx >= len(lines):
                break

            current_line = lines[line_idx]
            start_col = (left_col - 1) - 1 if line_num == left_line else len(current_line) - 1
            if start_col < 0:
                continue

            for col in range(start_col, -1, -1):
                if col >= len(current_line):
                    continue
                char = current_line[col]
                if char not in (' ', '\t'):
                    a_char_pos = (line_num, col)
                    break
            if a_char_pos is not None:
                break

        is_valid_macro = False
        if a_char_pos is None:
            is_valid_macro = True
        else:
            a_line, a_col = a_char_pos
            if a_line == macro_line and a_col >= macro_col:
                interval = lines[a_line - 1][macro_col:a_col + 1]
                if not any(char in FORBIDDEN_WHITESPACE for char in interval):
                    is_valid_macro = True

        if not is_valid_macro:
            continue

        right_paren_pos = match_balanced_parenthesis_in_lines(lines, left_line, left_col)
        if not right_paren_pos:
            continue
        right_line, right_col = right_paren_pos

        bracket_content, char_positions = extract_bracket_content(
            lines=lines,
            left_paren_line=left_line,
            left_paren_col=left_col,
            right_paren_line=right_line,
            right_paren_col=right_col
        )
        if not bracket_content or not char_positions:
            continue

        args_with_offsets = split_macro_arguments_with_offsets(bracket_content, char_positions)
        for (arg_text, arg_start_off, arg_end_off) in args_with_offsets:
            valid_vars = extract_valid_vars_from_arg_for_type(arg_text, local_vars)
            if not valid_vars:
                continue

            var_positions = find_variable_positions_in_arg_for_type(
                arg_text=arg_text,
                arg_start_offset=arg_start_off,
                arg_end_offset=arg_end_off,
                char_positions=char_positions,
                valid_vars=valid_vars
            )

            # 仅更新本文件标识符的位置
            for var_name, pos_line, pos_col, pos_len in var_positions:
                if var_name not in local_vars:
                    continue
                positions_set, new_name, is_in_current = local_vars[var_name]
                if is_in_current:
                    adjusted_pos = (pos_line, pos_col + 1, pos_len)
                    positions_set.add(adjusted_pos)

    return local_vars  # 返回带标记的完整集合，供后续过滤


def collect_file_identifiers(
    tu: TranslationUnit,
    current_file_path: str  # 新增:AST对应的当前文件路径(支持相对/绝对路径)
) -> Dict[str, Tuple[FrozenSet[Tuple[int, int, int]], str]]:
    """
    收集单个文件的标识符:仅保留本文件直接出现的符号
    返回 {旧名: (位置集合(行, 列, 长度), 新名)}
    
    Args:
        tu: 翻译单元(AST)
        current_file_path: 当前处理文件的路径(相对/绝对均可,会自动规范化)
    """
    file_identifiers = {}
    macro_set = set()
    PREFIX_PATTERN = re.compile(r'^(struct|union|enum)\s+')

    # 规范化当前文件路径(处理相对路径、跨平台分隔符、大小写)
    try:
        current_file = Path(current_file_path).resolve(strict=True)
    except FileNotFoundError:
        raise ValueError(f"当前文件路径不存在:{current_file_path}")
    except OSError as e:
        raise ValueError(f"路径规范化失败:{current_file_path},ERROR:{str(e)}")

    target_kinds = [
        # 声明节点
        CursorKind.FUNCTION_DECL, CursorKind.DECL_REF_EXPR,
        # CursorKind.STRUCT_DECL, CursorKind.UNION_DECL,
        # CursorKind.ENUM_DECL, CursorKind.ENUM_CONSTANT_DECL,
        # CursorKind.FIELD_DECL, CursorKind.TYPEDEF_DECL,
        # # 引用节点
        #  CursorKind.MEMBER_REF_EXPR,
        # CursorKind.TYPE_REF
    ]

    def is_macro(cursor: Cursor) -> bool:
        """判断节点是否来自宏实例化(仅检查当前文件的宏)"""
        loc = cursor.location
        if loc.file is None:
            return False
        # 先校验文件归属,再判断是否在宏集合
        if not is_current_file(loc.file.name):
            return False
        return f"{loc.file.name}:{loc.line}:{loc.column}" in macro_set

    def is_current_file(file_path: str) -> bool:
        """判断节点所在文件是否为当前处理文件(路径规范化对比)"""
        try:
            # 规范化节点所在文件路径,与当前文件路径对比
            node_file = Path(file_path).resolve(strict=False)
            # 考虑符号链接场景(如果需要跟随符号链接,用 resolve(True))
            return node_file == current_file
        except OSError:
            # 路径无法解析时,直接字符串匹配(降级处理)
            return Path(file_path).absolute() == current_file

    def get_identifier_name(cursor: Cursor) -> Optional[str]:
        """获取标识符名称,自动剥离struct/union前缀及空白"""
        if cursor.kind == CursorKind.MEMBER_REF_EXPR:
            raw_name = cursor.referenced.spelling if cursor.referenced else None
        else:
            raw_name = cursor.spelling
        
        if not raw_name:
            return None
        
        match = PREFIX_PATTERN.match(raw_name)
        if match:
            return raw_name[match.end():].strip()
        return raw_name.strip()

    def get_position(cursor: Cursor) -> Optional[Tuple[int, int, int]]:
        """
        获取标识符在当前文件的位置(仅返回当前文件的位置)
        返回:(行号, 列号(1-based), 名称长度) 或 None
        """
        loc = cursor.location
        # 1. 检查位置是否有效
        if not loc.file or not loc.file.name:
            return None
        # 2. 过滤非当前文件的位置
        if not is_current_file(loc.file.name):
            return None
        # 3. 计算位置信息
        line = loc.line
        if line <= 0:
            return None
        column = loc.column + 1  # 转为1-based列号
        name = get_identifier_name(cursor)
        return (line, column, len(name)) if (name and len(name) > 0) else None

    def skip_rules(cursor: Optional[Cursor], name: str) -> bool:
        if not name:
            return True
        if cursor and should_skip_identifier(cursor):
            return True
        if not is_underscore_naming(name):
            return True
        return False

    def process_macro(cursor: Cursor) -> None:
        return

    def traverse(cursor: Cursor) -> None:
        if is_macro(cursor):
            process_macro(cursor)
            return
        
        origin_cursor = cursor.referenced
        loc = cursor.location
        
        # 仅记录当前文件的宏实例化位置
        if cursor.kind == CursorKind.MACRO_INSTANTIATION:
            if loc.file is not None and is_current_file(loc.file.name):
                macro_set.add(f"{loc.file.name}:{loc.line}:{loc.column}")
            return
        
        # 获取当前文件内的位置(非当前文件返回None)
        pos = get_position(cursor)
        old_name = get_identifier_name(cursor)

        # 确定符号类型和是否跳过
        if cursor.kind in [CursorKind.DECL_REF_EXPR, CursorKind.TYPE_REF, CursorKind.MEMBER_REF_EXPR]:
            if not cursor.referenced:
                print(f"[ERROR] 存在未定义引用! 位置: {cursor.location}")
                exit(77)
            target_kind = cursor.referenced.kind
            should_skip = skip_rules(origin_cursor, old_name) if origin_cursor else True
        else:
            target_kind = cursor.kind
            should_skip = skip_rules(cursor, old_name)

        # 仅保留:目标类型匹配 + 不跳过 + 位置在当前文件的符号
        if target_kind in target_kinds and not should_skip and pos:
            # 根据符号类型生成大/小驼峰
            new_name = to_camel_case(old_name, True)

            # 记录到当前文件的标识符映射(位置去重)
            if old_name not in file_identifiers:
                file_identifiers[old_name] = (set(), new_name)
            positions_set, _ = file_identifiers[old_name]
            positions_set.add(pos)

        # 递归遍历子节点
        for child in cursor.get_children():
            traverse(child)

    traverse(tu.cursor)
    # file_identifiers = process_macro_arguments_for_type(tu, current_file_path, file_identifiers)
    # 冻结位置集合,返回最终结果
    return {
        name: (frozenset(positions), new_name)
        for name, (positions, new_name) in file_identifiers.items()
        if positions  # 过滤空位置集合
    }

def to_camel_case(underscore_name, is_big_camel):
    words = underscore_name.split("_")
    ret = "".join(word.capitalize() for word in words)
    if not is_big_camel:
        return ret[0].lower() + ret[1:]
    return ret

# -------------------------- 基于语法树得出的符号映射表替换源files中的标识符 --------------------------
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
        # 检查列号是否超出当前行长度(避免索引ERROR)
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
        # 确保返回的字典包含所有必要键,设置合理默认值
        return {
            'std': data.get('std'),  # 保持原有逻辑,可能为None(使用默认c99)
            'defines': data.get('defines', []),  # 列表类型,默认空列表
            'includes': data.get('includes', []),  # 列表类型,默认空列表
            'coption': data.get('coption', '')  # 新增字段,字符串类型,默认空字符串
        }
    except json.JSONDecodeError as e:
        raise ValueError(f"JSON格式ERROR: {e}")
    except Exception as e:
        raise RuntimeError(f"读取JSON文件失败: {e}")

# -------------------------- 解析编译参数--------------------------
def parse_compile_args(args) -> List[str]:
    """解析命令行参数和JSON配置,返回最终的编译参数列表
    
    Args:
        args: 命令行参数对象(需包含 std、define、include、command、print_ccargs 字段)
    
    Returns:
        List[str]: 完整的编译参数列表
    """
    # 1. 初始化默认参数
    json_args: Dict = {'std': None, 'defines': [], 'includes': [], 'coption': ''}
    
    # 2. 从JSON文件加载参数(如果指定)
    if args.command:
        try:
            json_args = load_compile_args_from_json(args.command)
            print(f"已从 {args.command} 加载编译参数")
        except Exception as e:
            print(f"警告:加载JSON文件失败,忽略JSON参数: {e}")
    
    # 3. 处理编译标准(优先级:命令行 > JSON > 默认c99)
    final_std = args.std if args.std is not None else (json_args.get('std') or "c99")
    
    # 4. 处理宏定义(JSON + 命令行,命令行在后优先级更高)
    json_defines = json_args.get('defines', [])
    json_defines = json_defines if isinstance(json_defines, list) else []
    cli_defines = args.define if isinstance(args.define, list) else []
    final_defines = json_defines + cli_defines
    
    # 5. 处理头文件路径(JSON + 命令行,命令行在后优先级更高)
    json_includes = json_args.get('includes', [])
    json_includes = json_includes if isinstance(json_includes, list) else []
    cli_includes = args.include if isinstance(args.include, list) else []
    final_includes = json_includes + cli_includes
    
    # 6. 处理额外编译选项(解析JSON中的coption字符串)
    json_coption = json_args.get('coption', '')
    final_coptions: List[str] = []
    if isinstance(json_coption, str) and json_coption.strip():
        try:
            final_coptions = shlex.split(json_coption.strip())
        except Exception as e:
            print(f"警告:解析coption失败,忽略该字段: {e}")
    
    # 7. 构建最终编译参数(过滤无效参数)
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

def get_file_lines(current_file_path: str) -> Optional[List[str]]:
    """读取文件所有行（保留原始换行符），失败返回None"""
    try:
        with open(current_file_path, 'r', encoding='utf-8') as f:
            return f.readlines()  # 每行包含末尾的\n
    except Exception as e:
        print(f"读取文件失败:{e}")
        return None


def find_left_paren_in_file(
    lines: List[str],
    start_line: int,  # 1-based
    start_col: int    # 1-based
) -> Optional[Tuple[int, int]]:
    """
    修复版:找到宏名后的第一个左括号'('（1-based）
    支持跨多行，逐字符处理注释/字符串，准确跳过无效括号
    """
    start_line_idx = start_line - 1  # 转为0-based
    start_col_idx = start_col - 1

    # 边界检查:起始行无效
    if start_line_idx < 0 or start_line_idx >= len(lines):
        return None

    current_line = lines[start_line_idx]
    line_len = len(current_line)

    # 边界检查:起始列超出当前行，直接从下一行开始扫描
    if start_col_idx >= line_len:
        current_line_idx = start_line_idx + 1
        current_col_idx = 0
    else:
        current_line_idx = start_line_idx
        current_col_idx = start_col_idx

    # 状态变量:处理注释和字符串
    in_double_quote = False
    in_single_quote = False
    in_block_comment = False
    in_line_comment = False

    max_scan_lines = 20  # 扩大扫描范围（适应更多换行场景）
    max_line_idx = min(start_line_idx + max_scan_lines, len(lines) - 1)

    # 逐行扫描（含当前行）
    while current_line_idx <= max_line_idx:
        line = lines[current_line_idx]
        line_len = len(line)

        # 逐字符扫描当前行
        while current_col_idx < line_len:
            char = line[current_col_idx]
            next_char = line[current_col_idx + 1] if (current_col_idx + 1 < line_len) else ''

            # 1. 处理注释状态切换（仅非字符串状态下生效）
            if not in_double_quote and not in_single_quote:
                # 块注释开始 /*
                if not in_block_comment and not in_line_comment and char == '/' and next_char == '*':
                    in_block_comment = True
                    current_col_idx += 1  # 跳过下一个'*'
                    current_col_idx += 1
                    continue
                # 块注释结束 */
                elif in_block_comment and char == '*' and next_char == '/':
                    in_block_comment = False
                    current_col_idx += 1  # 跳过下一个'/'
                    current_col_idx += 1
                    continue
                # 单行注释开始 //
                elif not in_block_comment and not in_line_comment and char == '/' and next_char == '/':
                    in_line_comment = True
                    current_col_idx += 1  # 跳过下一个'/'
                    current_col_idx += 1
                    continue

            # 2. 跳过注释内容
            if in_line_comment:
                break  # 单行注释直接跳过当前行剩余部分
            if in_block_comment:
                current_col_idx += 1
                continue

            # 3. 处理字符串状态切换
            if not in_single_quote and char == '"':
                in_double_quote = not in_double_quote
                current_col_idx += 1
                continue
            if not in_double_quote and char == "'":
                in_single_quote = not in_single_quote
                current_col_idx += 1
                continue

            # 4. 跳过字符串内内容
            if in_double_quote or in_single_quote:
                current_col_idx += 1
                continue

            # 5. 找到有效左括号（非注释/非字符串内）
            if char == '(':
                return (current_line_idx + 1, current_col_idx + 1)  # 转为1-based

            current_col_idx += 1

        # 切换到下一行
        current_line_idx += 1
        current_col_idx = 0
        in_line_comment = False  # 单行注释换行后自动结束

    # 超出扫描范围未找到
    return None


def match_balanced_parenthesis_in_lines(
    lines: List[str],
    left_paren_line: int,  # 1-based
    left_paren_col: int    # 1-based
) -> Optional[Tuple[int, int]]:
    """支持跨多行括号匹配（修复换行/注释场景下无法找到右括号的问题）"""
    left_line_idx = left_paren_line - 1
    left_col_idx = left_paren_col - 1

    # 边界检查:左括号位置无效直接返回
    if left_line_idx < 0 or left_line_idx >= len(lines):
        return None
    left_line = lines[left_line_idx]
    if left_col_idx < 0 or left_col_idx >= len(left_line):
        return None

    count = 1  # 左括号已计数，寻找匹配的右括号
    in_double_quote = False
    in_single_quote = False
    in_block_comment = False
    in_line_comment = False

    current_line_idx = left_line_idx
    current_col_idx = left_col_idx + 1  # 从左括号的下一个字符开始扫描

    while current_line_idx < len(lines):
        line = lines[current_line_idx]
        line_len = len(line)

        while current_col_idx < line_len:
            char = line[current_col_idx]
            next_char = line[current_col_idx + 1] if (current_col_idx + 1 < line_len) else ''

            # 1. 处理注释状态切换（非字符串状态下生效）
            if not in_double_quote and not in_single_quote:
                # 块注释开始 /*:更新状态，跳过下一个 '*'，继续扫描当前行
                if not in_block_comment and not in_line_comment and char == '/' and next_char == '*':
                    in_block_comment = True
                    current_col_idx += 1  # 跳过 '*'
                    current_col_idx += 1  # 移动到下一个字符（不 break，继续扫描）
                    continue
                # 块注释结束 */:更新状态，跳过下一个 '/'，继续扫描当前行
                elif in_block_comment and char == '*' and next_char == '/':
                    in_block_comment = False
                    current_col_idx += 1  # 跳过 '/'
                    current_col_idx += 1  # 移动到下一个字符（不 break，继续扫描）
                    continue
                # 单行注释开始 //:更新状态，后续字符直到行尾都跳过
                elif not in_block_comment and not in_line_comment and char == '/' and next_char == '/':
                    in_line_comment = True
                    current_col_idx += 1  # 跳过 '/'
                    current_col_idx += 1  # 移动到注释内容开始位置
                    continue

            # 2. 跳过注释内容（不中断扫描，仅移动列索引）
            if in_line_comment:
                # 单行注释:直接跳到行尾（跳过当前行剩余所有字符）
                current_col_idx = line_len
                continue
            if in_block_comment:
                # 块注释:继续移动列索引，直到找到结束符（已在上面处理）
                current_col_idx += 1
                continue

            # 3. 处理字符串状态切换（非注释状态下生效）
            if not in_single_quote and char == '"':
                in_double_quote = not in_double_quote
                current_col_idx += 1
                continue
            if not in_double_quote and char == "'":
                in_single_quote = not in_single_quote
                current_col_idx += 1
                continue

            # 4. 跳过字符串内内容
            if in_double_quote or in_single_quote:
                current_col_idx += 1
                continue

            # 5. 括号平衡计数（仅非注释/非字符串内）
            if char == '(':
                count += 1
            elif char == ')':
                count -= 1
                if count == 0:
                    # 找到匹配的右括号，返回 1-based 位置
                    return (current_line_idx + 1, current_col_idx + 1)

            current_col_idx += 1  # 继续扫描当前行下一个字符

        # 切换到下一行:重置列索引，单行注释自动结束
        current_line_idx += 1
        current_col_idx = 0
        in_line_comment = False

    # 遍历所有行未找到匹配的右括号
    return None


def extract_bracket_content(
    lines: List[str],
    left_paren_line: int,  # 1-based
    left_paren_col: int,   # 1-based
    right_paren_line: int, # 1-based
    right_paren_col: int   # 1-based
) -> Tuple[str, List[Tuple[int, int]]]:
    """提取左右括号之间的内容（不含左右括号本身），支持跨多行"""
    content = []
    char_positions = []
    left_line_idx = left_paren_line - 1  # 转为0-based
    left_col_idx = left_paren_col - 1
    right_line_idx = right_paren_line - 1
    right_col_idx = right_paren_col - 1

    # 边界校验:右括号位置不能在左括号之前（逻辑无效）
    if right_line_idx < left_line_idx:
        return '', []
    if right_line_idx == left_line_idx and right_col_idx <= left_col_idx:
        return '', []

    current_line_idx = left_line_idx
    current_col_idx = left_col_idx + 1  # 从左括号的下一个字符开始

    while current_line_idx <= right_line_idx:
        line = lines[current_line_idx]
        line_len = len(line)

        # 计算当前行的提取范围
        start_col = current_col_idx
        if current_line_idx == right_line_idx:
            # 右括号所在行:提取到「右括号前一个字符」（排除右括号）
            end_col = right_col_idx - 1
        else:
            # 非右括号所在行:提取到行尾
            end_col = line_len - 1

        # 边界校验:当前行无有效内容（跳过）
        if start_col > end_col:
            # 切换到下一行，重置列索引
            current_line_idx += 1
            current_col_idx = 0
            continue

        # 遍历当前行的有效范围，收集字符和位置
        for col_idx in range(start_col, end_col + 1):
            if col_idx >= line_len:  # 避免列索引越界（极端情况）
                break
            char = line[col_idx]
            content.append(char)
            # 记录字符的原始位置（转回1-based）
            char_positions.append((current_line_idx + 1, col_idx + 1))

        # 切换到下一行，列索引重置为0
        current_line_idx += 1
        current_col_idx = 0

    return (''.join(content), char_positions)


def split_macro_arguments_with_offsets(
    bracket_content: str,
    char_positions: List[Tuple[int, int]]
) -> List[Tuple[str, int, int]]:
    """支持跨多行参数分割，返回的偏移排除 arg 前后空白（保留内部空白）"""
    args = []
    current_arg_start = 0
    paren_count = 0
    in_double_quote = False
    in_single_quote = False
    in_block_comment = False
    in_line_comment = False
    content_len = len(bracket_content)

    def skip_whitespace(start_idx: int) -> int:
        """从 start_idx 开始，跳过所有空白字符（空格、制表符、换行、回车），返回第一个非空白字符的索引"""
        idx = start_idx
        while idx < content_len:
            char = bracket_content[idx]
            if char in (' ', '\t', '\n', '\r'):
                idx += 1
            else:
                break
        return idx

    for idx, char in enumerate(bracket_content):
        next_char = bracket_content[idx + 1] if idx + 1 < content_len else ''

        # 1. 处理注释/字符串状态（原有逻辑不变）
        if not in_double_quote and not in_single_quote:
            if not in_block_comment and not in_line_comment and char == '/' and next_char == '*':
                in_block_comment = True
                continue
            elif in_block_comment and char == '*' and next_char == '/':
                in_block_comment = False
                continue
            elif not in_block_comment and not in_line_comment and char == '/' and next_char == '/':
                in_line_comment = True
                continue
            elif in_line_comment and char == '\n':
                in_line_comment = False
                continue

        # 2. 跳过注释/字符串内的内容（原有逻辑不变）
        if in_block_comment or in_line_comment or in_double_quote or in_single_quote:
            if not in_single_quote and char == '"':
                in_double_quote = not in_double_quote
            elif not in_double_quote and char == "'":
                in_single_quote = not in_single_quote
            continue

        # 3. 括号平衡计数（原有逻辑不变）
        if char == '(':
            paren_count += 1
        elif char == ')':
            paren_count -= 1
        # 4. 顶层逗号（括号外）:分割参数（核心修改部分）
        elif char == ',' and paren_count == 0:
            # 跳过 arg 前面的空白，得到真实起始索引
            real_start = skip_whitespace(current_arg_start)
            # 跳过 arg 后面的空白（从逗号前开始向前跳），得到真实结束索引
            real_end = idx - 1
            while real_end >= real_start and bracket_content[real_end] in (' ', '\t', '\n', '\r'):
                real_end -= 1
            # 验证有效 arg（real_start <= real_end 才是有内容的）
            if real_start <= real_end:
                arg_text = bracket_content[real_start:real_end + 1]  # 无需 strip()，已排除前后空白
                args.append((arg_text, real_start, real_end))
            # 更新 current_arg_start 为逗号后跳过空白的位置，为下一个 arg 准备
            current_arg_start = skip_whitespace(idx + 1)

    # 5. 处理最后一个参数（核心修改部分）
    real_start = skip_whitespace(current_arg_start)
    real_end = skip_whitespace(content_len - 1)  # 从末尾向前跳空白
    # 验证有效 arg
    if real_start <= real_end:
        arg_text = bracket_content[real_start:real_end + 1]  # 无需 strip()
        args.append((arg_text, real_start, real_end))

    return args


def find_variable_positions_in_arg(
    arg_text: str,
    arg_start_offset: int,
    arg_end_offset: int,
    char_positions: List[Tuple[int, int]],
    valid_vars: List[Tuple[str, int, int]]  # 输入:extract_valid_vars_from_arg的返回值
) -> List[Tuple[str, int, int, int]]:
    """
    优化:保持跨多行查找、引号/注释过滤逻辑，新增成员访问符（->/.）过滤
    输入:valid_vars - 变量名+位置候选列表
    返回:[(var_name, 起始行1-based, 起始列0-based, 长度)] - 仅有效变量位置（非成员/非引号注释内）
    """
    var_positions = []
    arg_len = len(arg_text)
    if arg_len == 0 or not valid_vars or not char_positions:
        return var_positions

    # 预处理:标记arg_text中每个位置是否在有效区域（不在引号/注释中）
    valid_region = [False] * arg_len
    in_double_quote = False
    in_single_quote = False
    in_block_comment = False
    in_line_comment = False

    idx = 0
    while idx < arg_len:
        char = arg_text[idx]
        next_char = arg_text[idx + 1] if idx + 1 < arg_len else ''

        # 处理注释状态切换（仅在未引号包裹时生效）
        if not in_double_quote and not in_single_quote:
            if not in_block_comment and not in_line_comment and char == '/' and next_char == '*':
                in_block_comment = True
                idx += 2  # 跳过/*
                continue
            elif in_block_comment and char == '*' and next_char == '/':
                in_block_comment = False
                idx += 2  # 跳过*/
                continue
            elif not in_block_comment and not in_line_comment and char == '/' and next_char == '/':
                in_line_comment = True
                idx += 2  # 跳过//
                continue
            elif in_line_comment and char == '\n':
                in_line_comment = False
                idx += 1
                continue

        # 处理引号状态切换（仅在未注释时生效）
        if not in_block_comment and not in_line_comment:
            if not in_single_quote and char == '"':
                in_double_quote = not in_double_quote
            elif not in_double_quote and char == "'":
                in_single_quote = not in_single_quote

        # 标记当前位置是否有效（不在引号/注释中）
        valid_region[idx] = not (in_block_comment or in_line_comment or in_double_quote or in_single_quote)
        idx += 1

    # 遍历每个变量候选，验证有效性
    for (var_name, var_start_idx, var_end_idx) in valid_vars:
        var_len = var_end_idx - var_start_idx + 1

        # 1. 校验变量区间是否完全在有效区域（非引号/注释内）
        if var_start_idx < 0 or var_end_idx >= arg_len:
            continue  # 超出arg_text范围
        if not all(valid_region[var_start_idx:var_end_idx + 1]):
            continue  # 部分/全部在引号/注释中

        # 2. 校验是否为完整标识符（前后非字母数字/下划线，避免部分匹配）
        prev_char = arg_text[var_start_idx - 1] if var_start_idx > 0 else ''
        next_char_var = arg_text[var_end_idx + 1] if var_end_idx + 1 < arg_len else ''
        if (prev_char.isalnum() or prev_char == '_') or (next_char_var.isalnum() or next_char_var == '_'):
            continue  # 是其他标识符的一部分

        # 3. 核心优化:过滤成员访问符（->/.）后面的变量
        is_member = False
        # 情况1:变量前紧跟.（如 a.b 中的b）
        if var_start_idx > 0 and arg_text[var_start_idx - 1] == '.':
            is_member = True
        # 情况2:变量前紧跟->（如 a->b 中的b）
        elif var_start_idx > 1 and arg_text[var_start_idx - 2] == '-' and arg_text[var_start_idx - 1] == '>':
            is_member = True
        if is_member:
            continue  # 成员变量不纳入统计

        # 4. 校验偏移量有效性（确保映射到正确的文件位置）
        var_total_start = arg_start_offset + var_start_idx
        var_total_end = arg_start_offset + var_end_idx
        if var_total_start >= len(char_positions) or var_total_end > arg_end_offset:
            continue

        # 5. 获取文件行号/列号（char_positions存储:(行1-based, 列0-based)）
        var_start_line, var_start_col = char_positions[var_total_start]

        # 记录有效位置（包含var_name，用于后续关联到local_vars）
        var_positions.append((var_name, var_start_line, var_start_col, var_len))

    return var_positions

def remove_strings_and_comments(text: str) -> str:
    """保持原有逻辑（用于变量候选提取）"""
    text = re.sub(r'/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)
    text = re.sub(r'"([^"\\]|\\.)*"', '', text)
    text = re.sub(r"'([^'\\]|\\.)*'", '', text)
    return text



def extract_valid_vars_from_arg(
    arg_text: str,
    local_vars: Dict[str, Tuple[Set[Tuple[int, int, int]], str]]
) -> List[Tuple[str, int, int]]:
    """
    优化:提取参数文本中存在于local_vars的变量名及其在arg_text中的位置信息
    返回格式:[(var_name, start_idx, end_idx)]，其中start_idx/end_idx是arg_text内的0-based闭区间索引
    功能:准确匹配C/C++标识符，为后续过滤成员访问符提供位置基础
    """
    valid_vars = []
    if not arg_text or not local_vars:
        return valid_vars

    # 匹配C/C++合法标识符（字母/下划线开头，后跟字母/数字/下划线）
    # 使用单词边界\b确保完整匹配（避免部分匹配，如"abc"匹配"ab"）
    identifier_pattern = re.compile(r'\b[a-zA-Z_][a-zA-Z0-9_]*\b')
    
    # 遍历所有匹配的标识符，筛选出存在于local_vars中的变量
    for match in identifier_pattern.finditer(arg_text):
        var_name = match.group()
        if var_name in local_vars:
            start_idx = match.start()  # 标识符起始索引（0-based）
            end_idx = match.end() - 1  # 标识符结束索引（0-based闭区间）
            valid_vars.append((var_name, start_idx, end_idx))
    
    return valid_vars



def process_macro_arguments(
    tu: TranslationUnit,
    current_file_path: str,
    local_vars: Dict[str, Tuple[Set[Tuple[int, int, int]], str]]
) -> Dict[str, Tuple[FrozenSet[Tuple[int, int, int]], str]]:
    """保持原有逻辑（处理宏参数中的变量位置）"""
    if not local_vars:
        return local_vars

    lines = get_file_lines(current_file_path)
    if not lines:
        return local_vars

    macro_instances = []
    current_file = Path(current_file_path).resolve(strict=True)

    def collect_macros(cursor: Cursor) -> None:
        # 1. 检查当前节点是否是目标宏实例（当前文件 + 宏调用类型）
        loc = cursor.location
        if loc.file is None:
            # 跳过无文件关联的节点（如系统宏、内置节点）
            pass
        else:
            try:
                # 解析节点所属文件路径，与当前文件对比（避免跨文件收集）
                node_file = Path(loc.file.name).resolve(strict=False)
                if node_file != current_file:
                    # 跳过其他文件的宏实例
                    pass
                else:
                    # 只收集「当前文件」的「宏调用实例」，且行号有效
                    if cursor.kind == CursorKind.MACRO_INSTANTIATION and loc.line > 0:
                        # 列号转换:libclang 返回的列是 0-based，转为 1-based（符合编辑器习惯）
                        macro_instances.append((loc.line, loc.column))
            except OSError:
                # 路径解析失败时，降级用绝对路径对比
                if Path(loc.file.name).absolute() != current_file:
                    pass

        # 2. 递归遍历所有子节点（关键修复:遍历 AST 树的所有层级）
        for child_cursor in cursor.get_children():
            collect_macros(child_cursor)

    collect_macros(tu.cursor)
    FORBIDDEN_WHITESPACE = {' ', '\t', '\n'}

    for (macro_line, macro_col) in macro_instances:
        left_paren_pos = find_left_paren_in_file(lines, macro_line, macro_col)
        if not left_paren_pos:
            continue
        left_line, left_col = left_paren_pos

        a_char_pos: Optional[Tuple[int, int]] = None  # (A字符行1-based, A字符列0-based)
        # 从左括号所在行向上遍历（包含左括号行），直到宏行（避免超出范围）
        for line_num in range(left_line, macro_line - 1, -1):
            line_idx = line_num - 1  # 转为lines的0-based索引
            if line_idx < 0 or line_idx >= len(lines):
                break

            current_line = lines[line_idx]
            # 确定当前行的查找起始列（从右向左找）
            if line_num == left_line:
                # 左括号行:从左括号前一列（1-based→0-based:left_col-1 - 1？不:左括号1-based列→0-based是left_col-1，前一列是left_col-2）
                start_col = (left_col - 1) - 1  # 左括号左侧第一个字符的0-based列
            else:
                # 上方行:从行尾开始找
                start_col = len(current_line) - 1

            if start_col < 0:
                continue  # 当前行无有效字符，跳过

            # 从start_col向左找第一个非空白字符（排除空格/制表符）
            for col in range(start_col, -1, -1):
                if col >= len(current_line):
                    continue
                char = current_line[col]
                if char not in (' ', '\t'):  # A字符:非空格/制表符（换行符不算，因为按行遍历）
                    a_char_pos = (line_num, col)  # 记录A字符位置（1-based行，0-based列）
                    break  # 找到当前行的A字符，停止当前行查找

            if a_char_pos is not None:
                break  # 找到A字符，停止跨行查找

        # -------------------------- 核心步骤2:检查A字符→宏起始位置之间无任何空白 --------------------------
        is_valid_macro = False

        # 场景1:A字符不存在（左括号左侧全是空白）→ 无区间可查，默认合法
        if a_char_pos is None:
            is_valid_macro = True
        # 场景2:A字符存在 → 检查区间
        else:
            a_line, a_col = a_char_pos  # A字符:1-based行，0-based列
            macro_col_0 = macro_col  # 宏起始列:0-based（和A字符列格式一致）
            macro_line_1 = macro_line  # 宏行:1-based（和A字符行格式一致）

            # 子判断1:A字符和宏必须同行（不同行则区间含换行符，违法）
            if a_line == macro_line_1:
                # 子判断2:A字符必须在宏右侧（逻辑上必然成立，防异常）
                if a_col >= macro_col_0:
                    # 区间:宏起始列（macro_col_0）→ A字符列（a_col），包含两端
                    interval = lines[a_line - 1][macro_col_0 : a_col + 1]  # +1是因为切片左闭右开
                    # 检查区间内是否有任何禁止的空白
                    if not any(char in FORBIDDEN_WHITESPACE for char in interval):
                        is_valid_macro = True

        # 不合法 → 跳过后续处理
        if not is_valid_macro:
            continue

        right_paren_pos = match_balanced_parenthesis_in_lines(lines, left_line, left_col)
        if not right_paren_pos:
            continue
        right_line, right_col = right_paren_pos

        bracket_content, char_positions = extract_bracket_content(
            lines=lines,
            left_paren_line=left_line,
            left_paren_col=left_col,
            right_paren_line=right_line,
            right_paren_col=right_col
        )
        if not bracket_content or not char_positions:
            continue

        args_with_offsets = split_macro_arguments_with_offsets(bracket_content, char_positions)
        if not args_with_offsets:
            continue

        for (arg_text, arg_start_off, arg_end_off) in args_with_offsets:
            valid_vars = extract_valid_vars_from_arg(arg_text, local_vars)
            if not valid_vars:
                continue

            for var_name in valid_vars:
                var_positions = find_variable_positions_in_arg(
                arg_text=arg_text,
                arg_start_offset=arg_start_off,
                arg_end_offset=arg_end_off,
                char_positions=char_positions,
                valid_vars=valid_vars
            )

                if var_positions:
                    for var_name, pos_line, pos_col, pos_len in var_positions:
                        positions_set, _ = local_vars[var_name]
                        adjusted_pos = (pos_line, pos_col + 1, pos_len)  # 0-based列→1-based
                        positions_set.add(adjusted_pos)

    return {
        name: (frozenset(positions), new_name)
        for name, (positions, new_name) in local_vars.items()
        if positions
    }

def collect_local_variables(
    tu: TranslationUnit,
    current_file_path: str
) -> Dict[str, Tuple[FrozenSet[Tuple[int, int, int]], str]]:
    """
    仅提取单个文件中函数内部的局部变量（含函数参数）及其引用
    返回 {旧名: (位置集合(行, 列, 长度), 新名)}
    
    处理范围:
    - 函数参数（PARM_DECL）
    - 函数体内部声明的局部变量（VAR_DECL）
    - 上述变量的引用（DECL_REF_EXPR）
    仅保留符合下划线命名规则、非系统标识符的变量
    """
    macro_set = set()
    local_vars = {}
    PREFIX_PATTERN = re.compile(r'^(struct|union|enum)\s+')

    # 规范化当前文件路径
    try:
        current_file = Path(current_file_path).resolve(strict=True)
    except (FileNotFoundError, OSError) as e:
        raise ValueError(f"文件路径处理失败:{current_file_path}, 错误:{str(e)}")

    def is_current_file(file_path: str) -> bool:
        """判断节点是否属于当前处理文件"""
        try:
            node_file = Path(file_path).resolve(strict=False)
            return node_file == current_file
        except OSError:
            return Path(file_path).absolute() == current_file

    def get_identifier_name(cursor: Cursor) -> Optional[str]:
        """获取变量名称，剥离前缀"""
        raw_name = cursor.spelling
        if not raw_name:
            return None
        match = PREFIX_PATTERN.match(raw_name)
        if match:
            return raw_name[match.end():].strip()
        return raw_name.strip()

    def get_position(cursor: Cursor) -> Optional[Tuple[int, int, int]]:
        """获取变量在当前文件的位置（行, 列(1-based), 名称长度）"""
        loc = cursor.location
        if not loc.file or not is_current_file(loc.file.name):
            return None
        line = loc.line
        if line <= 0:
            return None
        column = loc.column + 1  # 保持1-based列号（与原框架替换逻辑一致）
        name = get_identifier_name(cursor)
        return (line, column, len(name)) if (name and len(name) > 0) else None

    def is_macro(cursor: Cursor) -> bool:
        """判断节点是否来自宏实例化(仅检查当前文件的宏)"""
        loc = cursor.location
        if loc.file is None:
            return False
        # 先校验文件归属,再判断是否在宏集合
        if not is_current_file(loc.file.name):
            return False
        return f"{loc.file.name}:{loc.line}:{loc.column}" in macro_set

    def traverse(cursor: Cursor) -> None:
        nonlocal local_vars
        if is_macro(cursor):
            for child in cursor.get_children():
                traverse(child)
            return
        loc = cursor.location
        
        # 仅记录当前文件的宏实例化位置
        if cursor.kind == CursorKind.MACRO_INSTANTIATION:
            if loc.file is not None and is_current_file(loc.file.name):
                macro_set.add(f"{loc.file.name}:{loc.line}:{loc.column}")
            return
        
        target_node = None
        var_name = None

        # 1. 处理变量声明（参数/局部变量,函数声明）
        if cursor.kind in [CursorKind.PARM_DECL, CursorKind.VAR_DECL, CursorKind.FUNCTION_DECL]:
            var_name = get_identifier_name(cursor)
            if var_name and not should_skip_local_var(cursor, var_name):
                target_node = cursor

        # 2. 处理变量引用（关联到声明）
        elif cursor.kind == CursorKind.DECL_REF_EXPR:
            referenced = cursor.referenced
            if not referenced:
                print("ERROR no reference")
                exit(777)
            if referenced and referenced.kind in [CursorKind.PARM_DECL, CursorKind.VAR_DECL, CursorKind.FUNCTION_DECL]:
                var_name = get_identifier_name(referenced)
                # 检查引用的变量是否符合规则（声明节点需非系统、符合命名规范）
                if var_name and not should_skip_local_var(referenced, var_name):
                    target_node = cursor  # 引用节点的位置需要替换

        # 收集有效节点的位置
        if target_node and var_name:
            pos = get_position(target_node)
            if pos:
                # 局部变量统一用小驼峰,函数使用大驼峰
                is_big_camel = cursor.kind == CursorKind.FUNCTION_DECL or (cursor.kind == CursorKind.DECL_REF_EXPR and cursor.referenced.kind == CursorKind.FUNCTION_DECL)
                new_name = to_camel_case(var_name, is_big_camel)
                if var_name not in local_vars:
                    local_vars[var_name] = (set(), new_name)
                positions_set, _ = local_vars[var_name]
                positions_set.add(pos)

        # 递归遍历子节点
        for child in cursor.get_children():
            traverse(child)

    traverse(tu.cursor)
    local_vars = process_macro_arguments(tu, current_file_path, local_vars)

    # 冻结位置集合，过滤空集合
    return {
        name: (frozenset(positions), new_name)
        for name, (positions, new_name) in local_vars.items()
        if positions
    }

def should_skip_local_var(cursor: Cursor, name: str) -> bool:
    """判断是否需要跳过该局部变量"""
    # 跳过系统标识符、不符合下划线命名规则的变量
    if is_system_identifier(cursor) or not is_underscore_naming(name):
        return True
    return False

def process_local_variables_in_file(
    file_path: str,
    compile_args: List[str],
    backup: bool = False,
    use_string_replace: bool = False,
    p_ast: bool = False,
    print_sys: bool = False
) -> None:
    """
    逐文件处理函数局部变量:提取 → 替换
    处理完成即替换当前文件，无需全局合并（局部变量作用域仅限函数内）
    
    Args:
        file_path: 待处理文件路径
        compile_args: 编译参数列表
        backup: 是否备份原文件
        use_string_replace: 是否使用文本替换（而非语法位置替换）
        print_ast: 是否打印AST
        print_sys: 是否显示系统标识符（仅AST打印时生效）
    """
    print(f"\n===== 开始处理文件（局部变量）: {file_path} =====")
    
    # 初始化libclang（复用全局初始化逻辑，避免重复初始化）
    try:
        index = clang.cindex.Index.create()
    except Exception as e:
        print(f"初始化Index失败:{e}")
        return

    # 解析文件生成AST
    parse_options = clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
    try:
        tu = index.parse(file_path, args=compile_args, options=parse_options)
    except Exception as e:
        print(f"解析文件失败，已跳过:{e}")
        return

    # 打印AST（如果启用）
    if p_ast:
        print(f"\n----- {file_path} 的AST结构 -----")
        print_ast(tu, print_sys)

    # 提取当前文件的局部变量
    try:
        local_vars = collect_local_variables(tu, file_path)
    except Exception as e:
        print(f"提取局部变量失败:{e}")
        return

    if not local_vars:
        print("该文件无需要转换的局部变量")
        return

    # 打印当前文件的转换规则
    print(f"\n该文件局部变量转换规则:")
    for old_name, (positions, new_name) in local_vars.items():
        print(f"  {old_name} → {new_name}（出现 {len(positions)} 次）")

    # 转换为替换函数需要的格式
    replace_map = {
        old_name: {
            "new_name": new_name,
            "positions": positions
        }
        for old_name, (positions, new_name) in local_vars.items()
    }

    # 执行替换
    try:
        if use_string_replace:
            replace_in_file_by_string(file_path, replace_map, backup=backup)
        else:
            replace_in_file(file_path, replace_map, backup=backup)
        print(f"局部变量替换完成:{file_path}")
    except Exception as e:
        print(f"替换局部变量失败:{e}")

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
    parser.add_argument("--local-only", action="store_true", help="仅处理函数内部局部变量（含参数）")
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
    
    # 文件排序,优先处理.c files,再处理.h files
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

    if args.local_only:
        print("===== 启动局部变量处理模式 =====")
        for file_path in target_files:
            process_local_variables_in_file(
                file_path=file_path,
                compile_args=compile_args,
                backup=args.backup,
                use_string_replace=args.string,
                p_ast=args.print_ast,
                print_sys=args.sys
            )
        print("\ndone（局部变量处理完成）")
        exit(0)


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
        identifiers = collect_file_identifiers(tu, file_path)
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
    exit(0)

    global_identifiers = {}
    print("===== 开始收集所有文件的标识符 =====")
    for file_path in target_files:
        print(f"\n===== 处理文件(收集阶段): {file_path} =====")
        # 解析文件(仅收集,不替换)
        parse_options = clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        try:
            tu = index.parse(file_path, args=compile_args, options=parse_options)
        except Exception as e:
            print(f"解析文件失败,已跳过: {e}")
            continue
        
        if args.print_ast:
            print_ast(tu, args.sys)

        # 收集当前文件的标识符(返回:{旧名: (位置集合, 新名)})
        file_identifiers = collect_file_identifiers(tu, file_path)
        if not file_identifiers:
            print("该文件没有需要转换的标识符")
            continue

        # 合并到全局映射表,检查冲突(同一旧名必须对应同一新名)
        for old_name, (positions, new_name) in file_identifiers.items():
            if old_name not in global_identifiers:
                # 新增全局映射:包含新名和各文件的位置信息
                global_identifiers[old_name] = {
                    "new_name": new_name,
                    "file_positions": {file_path: positions}  # key:文件路径,value:位置集合
                }
            else:
                # 检查新名一致性(避免同一旧名在不同文件生成不同新名)
                existing_new_name = global_identifiers[old_name]["new_name"]
                if existing_new_name != new_name:
                    print(f"[ERROR] 标识符冲突:旧名 '{old_name}' 在文件 '{file_path}' 生成新名 '{new_name}',但已在其他文件生成 '{existing_new_name}'")
                    exit(1)
                # 合并当前文件的位置(去重)
                if file_path not in global_identifiers[old_name]["file_positions"]:
                    global_identifiers[old_name]["file_positions"][file_path] = set()
                global_identifiers[old_name]["file_positions"][file_path].update(positions)

    # 打印全局转换规则(汇总所有文件的符号)
    print("\n===== 全局符号转换规则 =====")
    if not global_identifiers:
        print("没有需要转换的标识符")
    else:
        for old_name, data in global_identifiers.items():
            total_count = sum(len(pos) for pos in data["file_positions"].values())
            involved_files = len(data["file_positions"])
            print(f"    {old_name} → {data['new_name']} (共出现 {total_count} 次,涉及 {involved_files} 个文件)")

    # 第二步:统一替换所有文件(使用全局映射表)
    if args.replace and global_identifiers:
        print("\n===== 开始统一替换所有文件 =====")
        for file_path in target_files:
            print(f"\n===== 替换文件: {file_path} =====")
            # 提取当前文件需要替换的标识符(适配原替换函数的参数格式)
            file_replace_map = {}
            for old_name, data in global_identifiers.items():
                # 只取当前文件的位置信息
                if file_path in data["file_positions"]:
                    file_replace_map[old_name] = {
                        "new_name": data["new_name"],
                        "positions": data["file_positions"][file_path]
                    }
            
            if not file_replace_map:
                print("该文件没有需要替换的标识符")
                continue
            
            # 执行替换(复用原替换逻辑)
            try:
                if args.string:
                    replace_in_file_by_string(file_path, file_replace_map, backup=args.backup)
                else:
                    replace_in_file(file_path, file_replace_map, backup=args.backup)
                print(f"替换完成")
            except Exception as e:
                print(f"替换文件失败: {e}")
                continue

    print("\ndone")

if __name__ == "__main__":
    main()