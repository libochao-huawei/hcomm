import os
import re
import sys

# C语言关键字集合（需排除的标识符）
C_KEYWORDS = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do', 'double',
    'else', 'enum', 'extern', 'float', 'for', 'goto', 'if', 'int', 'long', 'register',
    'return', 'short', 'signed', 'sizeof', 'static', 'struct', 'switch', 'typedef',
    'union', 'unsigned', 'void', 'volatile', 'while'
}

# 系统函数集合（需排除的函数名）
SYSTEM_FUNCTIONS = {
    'printf', 'scanf', 'fprintf', 'fscanf', 'sprintf', 'sscanf',
    'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell',
    'malloc', 'free', 'realloc', 'calloc',
    'strlen', 'strcpy', 'strcat', 'strcmp', 'strncpy', 'strncat', 'strncmp',
    'memcpy', 'memset', 'memcmp', 'memmove',
    'exit', 'atoi', 'atol', 'atof', 'itoa',
    'puts', 'gets', 'putchar', 'getchar', 'perror'
}


def collect_files(target_path):
    """收集目标路径下所有.c/.h文件（支持文件、目录递归）"""
    file_list = []
    if os.path.isfile(target_path):
        if target_path.endswith(('.c', '.h')):
            file_list.append(target_path)
    else:
        for root, _, files in os.walk(target_path):
            for file in files:
                if file.endswith(('.c', '.h')):
                    file_list.append(os.path.join(root, file))
    return file_list


def extract_and_replace(content, pattern, placeholder_prefix, extracted_list):
    """用正则匹配目标内容（注释/字符串），替换为占位符并暂存原始内容"""
    def replace_func(match):
        extracted = match.group(0)
        extracted_list.append(extracted)
        return f"{placeholder_prefix}_{len(extracted_list)-1}>"
    return re.sub(pattern, replace_func, content)


def preprocess_content(content):
    """预处理：提取注释和字符串，用占位符替换（避免误改）"""
    extracted_comments = []
    extracted_strings = []
    
    # 处理多行注释 /* ... */（支持跨多行）
    multi_line_comment = re.compile(r'/\*.*?\*/', re.DOTALL)
    content = extract_and_replace(content, multi_line_comment, '<COMMENT', extracted_comments)
    
    # 处理单行注释 // ...（到行尾，确保匹配完整）
    single_line_comment = re.compile(r'//.*?$', re.MULTILINE)
    content = extract_and_replace(content, single_line_comment, '<COMMENT', extracted_comments)
    
    # 处理双引号字符串（支持转义"）
    double_quote_str = re.compile(r'"(?:\\.|[^"\\])*"')
    content = extract_and_replace(content, double_quote_str, '<STRING', extracted_strings)
    
    # 处理单引号字符（支持转义'）
    single_quote_str = re.compile(r"'(?:\\.|[^'\\])*'")
    content = extract_and_replace(content, single_quote_str, '<STRING', extracted_strings)
    
    return content, extracted_comments, extracted_strings


def restore_content(content, extracted_comments, extracted_strings):
    """恢复预处理时替换的注释和字符串"""
    # 恢复注释
    for i, comment in enumerate(extracted_comments):
        content = content.replace(f'<COMMENT_{i}>', comment)
    # 恢复字符串
    for i, string in enumerate(extracted_strings):
        content = content.replace(f'<STRING_{i}>', string)
    return content


def underscore_to_camelcase(underscore_str, is_upper):
    """下划线命名转驼峰命名（is_upper=True为大驼峰，False为小驼峰）"""
    words = [word for word in underscore_str.split('_') if word]  # 过滤空字符串（处理连续下划线）
    if not words:
        return underscore_str  # 异常情况返回原值
    if is_upper:
        return ''.join(word.capitalize() for word in words)  # 大驼峰：每个单词首字母大写
    else:
        return words[0] + ''.join(word.capitalize() for word in words[1:])  # 小驼峰：首单词小写


def identify_functions(preprocessed_content):
    """识别函数名（过滤系统函数、关键字、以_s结尾的函数）
    返回两个列表：需要转换的函数名 和 所有识别到的函数名（包括被过滤的）
    """
    # 匹配函数名：标识符后紧跟(（允许空格）
    func_pattern = re.compile(r'\b([a-z_][a-z0-9_]*)\s*\(')
    all_funcs = set(func_pattern.findall(preprocessed_content))
    
    # 过滤规则：排除关键字、系统函数、以_s结尾的函数
    filtered_funcs = []
    for func in all_funcs:
        if func in C_KEYWORDS or func in SYSTEM_FUNCTIONS or func.endswith('_s'):
            continue
        filtered_funcs.append(func)
    return filtered_funcs, all_funcs


def identify_types(preprocessed_content):
    """识别类型名（结构体/枚举/联合体标签、typedef定义的类型）"""
    types = set()
    
    # 增强typedef识别：匹配typedef定义的类型
    typedef_pattern = re.compile(
        r'typedef\s+(?:struct\s*\{.*?\}|union\s*\{.*?\}|enum\s*\{.*?\}|[\w\s\*]+?)\s*([a-z_][a-z0-9_]*)\s*;',
        re.DOTALL
    )
    types.update(typedef_pattern.findall(preprocessed_content))
    
    # 匹配结构体标签
    struct_pattern = re.compile(r'struct\s+([a-z_][a-z0-9_]*)\s*\{', re.DOTALL)
    types.update(struct_pattern.findall(preprocessed_content))
    
    # 匹配枚举标签
    enum_pattern = re.compile(r'enum\s+([a-z_][a-z0-9_]*)\s*\{', re.DOTALL)
    types.update(enum_pattern.findall(preprocessed_content))
    
    # 匹配联合体标签
    union_pattern = re.compile(r'union\s+([a-z_][a-z0-9_]*)\s*\{', re.DOTALL)
    types.update(union_pattern.findall(preprocessed_content))
    
    # 过滤关键字
    return [t for t in types if t not in C_KEYWORDS]


def identify_variables(preprocessed_content, all_funcs, types):
    """识别变量名（排除所有函数、类型、关键字、系统函数）"""
    # 匹配所有小写标识符
    identifier_pattern = re.compile(r'\b([a-z_][a-z0-9_]*)\b')
    all_identifiers = set(identifier_pattern.findall(preprocessed_content))
    
    # 变量 = 所有标识符 - 所有函数 - 类型 - 关键字 - 系统函数
    variables = []
    for ident in all_identifiers:
        if ident in C_KEYWORDS or ident in SYSTEM_FUNCTIONS or ident in all_funcs or ident in types:
            continue
        variables.append(ident)
    return variables


def replace_identifiers(content, funcs, types, variables):
    """替换标识符为驼峰命名（按长度降序替换，避免部分匹配）"""
    # 构建替换映射：原标识符 -> 转换后标识符
    replace_map = {}
    # 函数和类型用大驼峰
    for func in funcs:
        replace_map[func] = underscore_to_camelcase(func, is_upper=True)
    for type_ in types:
        replace_map[type_] = underscore_to_camelcase(type_, is_upper=True)
    # 变量用小驼峰
    for var in variables:
        replace_map[var] = underscore_to_camelcase(var, is_upper=False)
    
    # 按标识符长度降序排序（避免短标识符匹配长标识符的前缀）
    sorted_idents = sorted(replace_map.keys(), key=lambda x: -len(x))
    
    # 替换每个标识符（用\b确保完整匹配单词）
    for ident in sorted_idents:
        # 转义特殊字符，确保正则安全
        pattern = re.compile(r'\b' + re.escape(ident) + r'\b')
        content = pattern.sub(replace_map[ident], content)
    
    return content


def process_file(file_path):
    """处理单个文件：完整流程"""
    # 读取文件内容
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 预处理：提取注释和字符串，避免误改
    preprocessed, comments, strings = preprocess_content(content)
    
    # 识别函数（返回需要转换的函数和所有函数）、类型、变量
    funcs_to_convert, all_funcs = identify_functions(preprocessed)
    types = identify_types(preprocessed)
    variables = identify_variables(preprocessed, all_funcs, types)
    
    # 替换标识符为驼峰命名
    replaced = replace_identifiers(preprocessed, funcs_to_convert, types, variables)
    
    # 恢复注释和字符串，得到最终内容
    final_content = restore_content(replaced, comments, strings)
    
    # 写回文件
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(final_content)


def main(target_path):
    """主函数：收集文件并批量处理"""
    files = collect_files(target_path)
    if not files:
        print("No .c/.h files found.")
        return
    for file in files:
        print(f"Processing: {file}")
        process_file(file)
    print("All files processed successfully.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python c_naming_converter.py <target_path>")
        print("  <target_path>: path to a .c/.h file or a directory (recursive)")
        sys.exit(1)
    main(sys.argv[1])