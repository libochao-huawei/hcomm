#!/usr/bin/env python3
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Dict, Tuple

def load_json_config(config_path: str) -> Dict:
    """加载并校验JSON配置文件"""
    config_file = Path(config_path)
    if not config_file.exists():
        print(f"[ERROR] 配置文件不存在：{config_path}")
        sys.exit(1)
    try:
        with open(config_file, 'r', encoding='utf-8') as f:
            config = json.load(f)
    except json.JSONDecodeError as e:
        print(f"[ERROR] JSON配置格式错误：{e}")
        sys.exit(1)
    
    # 校验必填字段
    required_fields = ['std', 'coption', 'defines', 'includes', 'sources']
    missing_fields = [f for f in required_fields if f not in config]
    if missing_fields:
        print(f"[ERROR] JSON配置缺少必填字段：{', '.join(missing_fields)}")
        sys.exit(1)
    
    # 标准化配置（确保列表类型）
    config['defines'] = config['defines'] if isinstance(config['defines'], list) else [config['defines']]
    config['includes'] = config['includes'] if isinstance(config['includes'], list) else [config['includes']]
    config['sources'] = config['sources'] if isinstance(config['sources'], list) else [config['sources']]
    return config

def collect_c_files(sources: List[str]) -> List[Path]:
    """收集所有待编译的C文件（支持目录递归和单个文件）"""
    c_files = []
    for source in sources:
        path = Path(source)
        if not path.exists():
            print(f"[WARNING] 源路径不存在，跳过：{source}")
            continue
        
        if path.is_dir():
            # 递归查找目录下所有.c文件
            dir_c_files = list(path.rglob("*.c"))
            c_files.extend(dir_c_files)
            print(f"[INFO] 从目录递归收集到 {len(dir_c_files)} 个C文件：{path}")
        elif path.is_file() and path.suffix == '.c':
            # 单个.c文件
            c_files.append(path)
            print(f"[INFO] 收集到单个C文件：{path}")
        else:
            print(f"[WARNING] 非C文件/目录，跳过：{source}")
    
    # 去重并排序（避免重复编译）
    unique_c_files = list(sorted(set(c_files)))
    if not unique_c_files:
        print(f"[ERROR] 未找到任何待编译的C文件")
        sys.exit(1)
    print(f"[INFO] 共收集到 {len(unique_c_files)} 个有效C文件")
    return unique_c_files

def build_compile_cmd(
    c_file: Path,
    config: Dict
) -> Tuple[List[str], Path]:
    """构造单个C文件的编译命令（-c选项，生成.o文件）"""
    # 目标文件路径：与源文件同目录，替换后缀为.o
    o_file = c_file.with_suffix('.o')
    
    # 构造编译参数
    cmd = [
        'gcc',  # 若使用clang，可改为'clang'，保持与AST解析工具一致
        '-c',  # 只编译不链接
        f'-std={config["std"]}',  # 语言标准（如gnu11）
    ]
    
    # 添加头文件搜索路径（-I）
    for include in config['includes']:
        include_path = Path(include).resolve()
        if include_path.exists():
            cmd.append(f'-I{include_path}')
        else:
            print(f"[WARNING] 头文件路径不存在，跳过：{include}")
    
    # 添加宏定义（-D）
    for define in config['defines']:
        cmd.append(f'-D{define}')
    
    # 添加编译选项（coption）
    cmd.extend(config['coption'].split())
    
    # 源文件和目标文件
    cmd.append(str(c_file.resolve()))  # 源文件绝对路径
    cmd.append(f'-o{str(o_file.resolve())}')  # 目标文件绝对路径
    
    return cmd, o_file

def execute_compile(cmd: List[str], c_file: Path, o_file: Path) -> Tuple[bool, str]:
    """执行编译命令，返回执行结果（成功/失败）和输出信息"""
    print(f"\n[INFO] 正在编译：{c_file.name}")
    print(f"[DEBUG] 编译命令：{' '.join(cmd)}")
    
    try:
        # 执行命令，捕获stdout和stderr（合并输出）
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding='utf-8',
            timeout=30  # 超时时间30秒，防止卡死
        )
    except subprocess.TimeoutExpired:
        return False, f"编译超时（超过30秒）"
    except Exception as e:
        return False, f"编译执行失败：{str(e)}"
    
    # 判断编译结果（returncode=0为成功）
    if result.returncode == 0:
        return True, "编译成功"
    else:
        return False, f"编译失败（返回码{result.returncode}）：\n{result.stdout}"

def clean_output_files(o_files: List[Path]) -> None:
    """清理生成的.o目标文件"""
    print(f"\n[INFO] 开始清理编译输出文件...")
    for o_file in o_files:
        if o_file.exists():
            try:
                o_file.unlink()  # 删除文件
                print(f"[INFO] 已删除：{o_file}")
            except Exception as e:
                print(f"[WARNING] 无法删除文件：{o_file}，错误：{str(e)}")
        else:
            print(f"[INFO] 文件不存在，跳过：{o_file}")

def main():
    # 检查命令行参数（需要传入JSON配置文件路径）
    if len(sys.argv) != 2:
        print(f"用法：{sys.argv[0]} <配置文件路径.json>")
        sys.exit(1)
    
    # 1. 加载配置
    config = load_json_config(sys.argv[1])
    
    # 2. 收集C文件
    c_files = collect_c_files(config['sources'])
    
    # 3. 编译所有文件并记录结果
    compile_results = []  # 元素：(c_file, 成功与否, 信息)
    o_files = []  # 记录生成的.o文件，用于后续清理
    success_count = 0
    fail_count = 0
    
    for c_file in c_files:
        cmd, o_file = build_compile_cmd(c_file, config)
        o_files.append(o_file)  # 无论编译是否成功，都记录要清理
        
        success, msg = execute_compile(cmd, c_file, o_file)
        compile_results.append((c_file, success, msg))
        
        if success:
            success_count += 1
        else:
            fail_count += 1
    
    # 4. 输出验证汇总报告
    print("\n" + "="*60)
    print("编译验证汇总报告")
    print("="*60)
    print(f"总文件数：{len(c_files)}")
    print(f"成功数：{success_count}")
    print(f"失败数：{fail_count}")
    print("\n失败文件详情：")
    for c_file, success, msg in compile_results:
        if not success:
            print(f"- {c_file}：{msg}")
    print("="*60)
    
    # 5. 清理输出文件
    clean_output_files(o_files)
    
    # 脚本退出码：0=全部成功，1=存在失败
    sys.exit(0 if fail_count == 0 else 1)

if __name__ == "__main__":
    main()