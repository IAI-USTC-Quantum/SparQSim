"""
PySparQ 动态算子扩展模块

提供运行时编译和加载自定义 C++ 算子的功能。

基本用法:
    >>> from PySparQ.dynamic_operator import compile_cpp_code, CompilerConfig
    >>> 
    >>> # 定义自定义算子
    >>> cpp_code = '''
    ... class MyOperator : public BaseOperator {
    ... public:
    ...     void operator()(std::vector<System>& state) const override {
    ...         // 实现算子逻辑
    ...     }
    ... };
    ... '''
    >>> 
    >>> # 编译为共享库
    >>> lib_path = compile_cpp_code(cpp_code, class_name="MyOperator")
    >>> print(f"编译成功: {lib_path}")

高级用法:
    >>> from PySparQ.dynamic_operator import CompilerConfig, CompilationError
    >>> 
    >>> # 自定义编译配置
    >>> config = CompilerConfig(
    ...     cxx="clang++",
    ...     std="c++20",
    ...     opt_level="O3",
    ...     extra_flags=["-march=native"],
    ... )
    >>> 
    >>> try:
    ...     lib_path = compile_cpp_code(cpp_code, class_name="MyOperator", config=config)
    ... except CompilationError as e:
    ...     print(f"编译失败: {e}")

缓存管理:
    >>> from PySparQ.dynamic_operator import get_cache_info, clear_cache
    >>> 
    >>> # 查看缓存信息
    >>> info = get_cache_info()
    >>> print(f"缓存文件数: {info['file_count']}")
    >>> 
    >>> # 清除缓存
    >>> clear_cache()
"""

from .compiler import (
    CompilerConfig,
    CompilationError,
    compile_cpp_code,
    compute_code_hash,
    find_project_root,
    generate_cpp_source,
    format_compile_error,
    clear_cache,
    get_cache_info,
    quick_compile,
)

__all__ = [
    "CompilerConfig",
    "CompilationError",
    "compile_cpp_code",
    "compute_code_hash",
    "find_project_root",
    "generate_cpp_source",
    "format_compile_error",
    "clear_cache",
    "get_cache_info",
    "quick_compile",
]

__version__ = "0.1.0"
