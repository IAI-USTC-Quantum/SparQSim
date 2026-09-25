"""
Runtime C++ code compilation system

Provides the ability to compile user-defined C++ operators at runtime, with support for:
- Automatic code skeleton generation
- Invoking g++ to build a shared library (.so)
- Code hash based caching to avoid redundant compilation
- Compilation error capture and formatting
"""

import hashlib
import os
import re
import shutil
import subprocess
import tempfile
import warnings
from pathlib import Path
from typing import Optional, Tuple


class CompilationError(Exception):
    """Compilation error exception."""

    def __init__(self, message: str, stderr: str = "", returncode: int = 0):
        super().__init__(message)
        self.stderr = stderr
        self.returncode = returncode

    def __str__(self) -> str:
        msg = super().__str__()
        if self.stderr:
            msg += f"\n\nCompiler output:\n{self.stderr}"
        return msg


class CompilerConfig:
    """Compiler configuration."""

    # Default code skeleton template
    DEFAULT_TEMPLATE = """#include "basic_components.h"
#include <vector>
#include <complex>

using namespace qram_simulator;

{USER_CPP_CODE}

extern "C" BaseOperator* create_operator({CTOR_PARAMS}) {{
    return new {CLASS_NAME}({CTOR_ARGS});
}}

extern "C" void destroy_operator(BaseOperator* op) {{
    delete op;
}}

extern "C" const char* get_operator_name() {{
    return "{CLASS_NAME}";
}}
"""

    # Python-enhanced template - includes helper functions that support ctypes calls
    # Key point: the C++ SparseState* pointer is obtained via state._cpp_ptr()
    # (exposed by pysparq._core.SparseState); ctypes passes it as c_void_p, ensuring
    # the pointer value is transferred correctly and the ABI stays consistent.
    PYTHON_TEMPLATE = """#include "basic_components.h"
#include <vector>
#include <complex>

using namespace qram_simulator;

{USER_CPP_CODE}

extern "C" BaseOperator* create_operator({CTOR_PARAMS}) {{
    return new {CLASS_NAME}({CTOR_ARGS});
}}

extern "C" void destroy_operator(BaseOperator* op) {{
    delete op;
}}

extern "C" const char* get_operator_name() {{
    return "{CLASS_NAME}";
}}

// Python call helper - applies the operator to a SparseState
// The Python side obtains the C++ SparseState* pointer via state._cpp_ptr(),
// which ctypes passes as ctypes.c_void_p.
extern "C" void apply_operator(BaseOperator* op, SparseState* state) {{
    if (op && state) {{
        (*op)(*state);
    }}
}}

// Python call helper - applies the dagger
extern "C" void apply_operator_dag(BaseOperator* op, SparseState* state) {{
    if (op && state) {{
        op->dag(*state);
    }}
}}

// Returns the base class type
extern "C" const char* get_base_class() {{
    return "{BASE_CLASS}";
}}
"""

    def __init__(
        self,
        cxx: str = "g++",
        std: str = "c++17",
        opt_level: str = "O2",
        include_paths: Optional[list] = None,
        lib_paths: Optional[list] = None,
        libraries: Optional[list] = None,
        extra_flags: Optional[list] = None,
        template: Optional[str] = None,
    ):
        """
        Initialize the compiler configuration

        Args:
            cxx: C++ compiler command (default g++)
            std: C++ standard version (default c++17)
            opt_level: Optimization level (default O2)
            include_paths: Extra header search paths
            lib_paths: Extra library search paths
            libraries: Libraries to link against
            extra_flags: Extra compiler flags
            template: Custom code template
        """
        self.cxx = cxx
        self.std = std
        self.opt_level = opt_level
        self.include_paths = include_paths or []
        self.lib_paths = lib_paths or []
        self.libraries = libraries or []
        self.extra_flags = extra_flags or []
        self.template = template or self.DEFAULT_TEMPLATE

    def get_compile_flags(self) -> list:
        """Generate the list of compiler flags."""
        flags = [
            f"-std={self.std}",
            f"-{self.opt_level}",
            "-fPIC",  # Position-independent code (required for shared libraries)
            "-shared",  # Produce a shared library
        ]

        # Add header search paths
        for path in self.include_paths:
            flags.append(f"-I{path}")

        # Add library search paths
        for path in self.lib_paths:
            flags.append(f"-L{path}")

        # Add libraries to link
        for lib in self.libraries:
            flags.append(f"-l{lib}")

        # Add extra flags
        flags.extend(self.extra_flags)

        return flags


def compute_code_hash(cpp_code: str, class_name: str, config: CompilerConfig) -> str:
    """
    Compute the code hash used for caching

    The hash covers: code content, class name, compiler version, and configuration

    Args:
        cpp_code: User C++ code
        class_name: Operator class name
        config: Compiler configuration

    Returns:
        A 16-character hexadecimal hash string
    """
    # Obtain compiler version info (it affects the ABI)
    compiler_version = ""
    try:
        result = subprocess.run(
            [config.cxx, "--version"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if result.returncode == 0:
            compiler_version = result.stdout.strip()[:100]  # Keep the first 100 characters
    except Exception:
        pass

    # Combine the hash content
    hash_content = "|".join([
        cpp_code,
        class_name,
        config.std,
        config.opt_level,
        ",".join(sorted(config.include_paths)),
        compiler_version,
    ])

    return hashlib.sha256(hash_content.encode("utf-8")).hexdigest()[:16]


def find_project_root() -> Optional[Path]:
    """
    Locate the project root directory or the installed package directory

    For an installed package, the directory layout is:
    - site-packages/pysparq/ (Python package)
    - site-packages/include/ (headers, including basic_components.h)

    For a source checkout (the SparQSim repository):
    - The project root contains extern/qram-simulator/ (C++ core submodule) and PySparQ/

    Returns:
        The project root path or the installed package directory; None if not found
    """
    current = Path(__file__).resolve().parent

    # Check whether we are inside an installed package (site-packages/pysparq/dynamic_operator)
    # In that case, the headers live in site-packages/include/
    for parent in [current] + list(current.parents):
        # Installed package case: check whether include/basic_components.h exists
        # This ensures it is the full header directory, not PySparQ/include (binding headers only)
        if (parent / "include" / "basic_components.h").exists() and (parent / "pysparq").exists():
            return parent
        # Source checkout case: qram-simulator submodule + PySparQ/
        if (parent / "extern" / "qram-simulator").exists() and (parent / "PySparQ").exists():
            return parent
    return None


def generate_cpp_source(
    cpp_code: str,
    class_name: str,
    ctor_params: str = "",
    ctor_args: str = "",
    config: Optional[CompilerConfig] = None,
) -> str:
    """
    Generate the complete C++ source file

    Args:
        cpp_code: User-provided C++ code (containing the class definition)
        class_name: Operator class name
        ctor_params: Constructor parameter declarations (e.g. "int n, double theta")
        ctor_args: Constructor call arguments (e.g. "n, theta")
        config: Compiler configuration (provides the template)

    Returns:
        The complete C++ source code string
    """
    cfg = config or CompilerConfig()
    template = cfg.template

    # Substitute template variables
    source = template.format(
        USER_CPP_CODE=cpp_code,
        CLASS_NAME=class_name,
        CTOR_PARAMS=ctor_params,
        CTOR_ARGS=ctor_args,
    )

    return source


def compile_cpp_code(
    cpp_code: str,
    class_name: str,
    cache_dir: Optional[str] = None,
    ctor_params: str = "",
    ctor_args: str = "",
    config: Optional[CompilerConfig] = None,
    project_root: Optional[str] = None,
    verbose: bool = False,
) -> str:
    """
    Compile C++ code into a shared library

    Args:
        cpp_code: User-provided C++ code (containing the class definition)
        class_name: Operator class name
        cache_dir: Cache directory (defaults to the system temporary directory)
        ctor_params: Constructor parameter declarations
        ctor_args: Constructor call arguments
        config: Compiler configuration
        project_root: Project root directory (auto-detected)
        verbose: Whether to print verbose logs

    Returns:
        Path of the compiled shared library (.so file)

    Raises:
        CompilationError: Compilation failed
        FileNotFoundError: Compiler not found
    """
    cfg = config or CompilerConfig()

    # Auto-detect the project root directory
    if project_root is None:
        detected_root = find_project_root()
        if detected_root is None:
            raise RuntimeError(
                "Failed to auto-detect the project root directory; please specify the project_root parameter manually"
            )
        project_root = str(detected_root)

    project_root_path = Path(project_root)

    # Check whether this is an installed package (headers under the include/ directory)
    installed_include = project_root_path / "include"
    if installed_include.exists():
        # Installed package case: the headers are already under the unified include/ directory
        if str(installed_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(installed_include))
    else:
        # Source checkout case: SparQ framework headers live at this repository's root,
        # while the QRAM core (QRAM/Common/ThirdParty) lives under extern/qram-simulator/
        core_root = project_root_path / "extern" / "qram-simulator"

        sparq_include = project_root_path / "SparQ" / "include"
        if sparq_include.exists() and str(sparq_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(sparq_include))

        algo_include = project_root_path / "SparQ_Algorithm" / "include"
        if algo_include.exists() and str(algo_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(algo_include))

        qram_include = core_root / "QRAM" / "include"
        if qram_include.exists() and str(qram_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(qram_include))

        common_include = core_root / "Common" / "include"
        if common_include.exists() and str(common_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(common_include))

        # Add the Eigen header path
        eigen_include = core_root / "ThirdParty" / "eigen-3.4.0"
        if eigen_include.exists() and str(eigen_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(eigen_include))

        # Add the fmt header path
        fmt_include = core_root / "ThirdParty" / "fmt" / "include"
        if fmt_include.exists() and str(fmt_include) not in cfg.include_paths:
            cfg.include_paths.insert(0, str(fmt_include))

    # Compute the code hash
    code_hash = compute_code_hash(cpp_code, class_name, cfg)

    # Determine the cache directory
    if cache_dir is None:
        cache_dir = os.path.join(tempfile.gettempdir(), "pysparq_dynamic_ops")

    os.makedirs(cache_dir, exist_ok=True)

    # Generate the library file name
    lib_filename = f"{class_name}_{code_hash}.so"
    lib_path = os.path.join(cache_dir, lib_filename)

    # Check the cache
    if os.path.exists(lib_path):
        if verbose:
            print(f"[compiler] Using cache: {lib_path}")
        return lib_path

    # Generate the complete source code
    full_source = generate_cpp_source(cpp_code, class_name, ctor_params, ctor_args, cfg)

    # Create a temporary source file
    source_filename = f"{class_name}_{code_hash}.cpp"
    source_path = os.path.join(cache_dir, source_filename)

    with open(source_path, "w", encoding="utf-8") as f:
        f.write(full_source)

    if verbose:
        print(f"[compiler] Source file: {source_path}")
        print(f"[compiler] Target library: {lib_path}")

    # Check for the compiler
    if not shutil.which(cfg.cxx):
        raise FileNotFoundError(f"C++ compiler not found: {cfg.cxx}")

    # Build the compile command
    cmd = [cfg.cxx] + cfg.get_compile_flags() + ["-o", lib_path, source_path]

    if verbose:
        print(f"[compiler] Compile command: {' '.join(cmd)}")

    # Run the compilation
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,  # 2-minute timeout
        )
    except subprocess.TimeoutExpired:
        raise CompilationError("Compilation timed out (exceeded 2 minutes)")
    except Exception as e:
        raise CompilationError(f"Compiler execution failed: {e}")

    # Check the compilation result
    if result.returncode != 0:
        # Format the error message
        formatted_error = format_compile_error(result.stderr, source_path)
        raise CompilationError(
            f"Compilation failed (return code: {result.returncode})",
            stderr=formatted_error,
            returncode=result.returncode,
        )

    # Compilation succeeded; optionally delete the source file (kept for debugging)
    # os.remove(source_path)

    if verbose:
        print(f"[compiler] Compilation succeeded: {lib_path}")

    return lib_path


def format_compile_error(stderr: str, source_path: str) -> str:
    """
    Format compiler error output

    - Simplifies file paths
    - Highlights error lines
    - Extracts the key error information

    Args:
        stderr: Compiler standard error output
        source_path: Source file path

    Returns:
        The formatted error message
    """
    if not stderr:
        return "Unknown compilation error"

    lines = stderr.strip().split("\n")
    formatted_lines = []

    # Error pattern matching
    error_patterns = [
        r"(.*?):(\d+):(\d+):\s*(error|warning):\s*(.*)",  # GCC/Clang format
        r"(.*?):\s*(error|warning)\s*\w*:\s*(.*)",  # Another format
    ]

    for line in lines:
        line = line.strip()
        if not line:
            continue

        # Simplify paths
        if source_path in line:
            line = line.replace(source_path, "<source>")

        formatted_lines.append(line)

    # Extract an error summary
    errors = []
    warnings = []

    for line in formatted_lines:
        if "error:" in line.lower():
            errors.append(line)
        elif "warning:" in line.lower():
            warnings.append(line)

    # Build the output
    output = []

    if errors:
        output.append(f"Errors ({len(errors)}):")
        for err in errors[:5]:  # Show only the first 5 errors
            output.append(f"  - {err}")
        if len(errors) > 5:
            output.append(f"  ... {len(errors) - 5} more error(s)")

    if warnings:
        output.append(f"\nWarnings ({len(warnings)}):")
        for warn in warnings[:3]:  # Show only the first 3 warnings
            output.append(f"  - {warn}")

    if not errors and not warnings:
        output.append("Compiler output:")
        output.extend(formatted_lines[:20])  # At most 20 lines

    return "\n".join(output)


def clear_cache(cache_dir: Optional[str] = None) -> int:
    """
    Clear the compilation cache

    Args:
        cache_dir: Cache directory (defaults to the system temporary directory)

    Returns:
        The number of deleted files
    """
    if cache_dir is None:
        cache_dir = os.path.join(tempfile.gettempdir(), "pysparq_dynamic_ops")

    if not os.path.exists(cache_dir):
        return 0

    count = 0
    for filename in os.listdir(cache_dir):
        filepath = os.path.join(cache_dir, filename)
        try:
            if os.path.isfile(filepath):
                os.remove(filepath)
                count += 1
        except Exception as e:
            warnings.warn(f"Failed to remove cached file {filepath}: {e}")
            raise

    return count


def get_cache_info(cache_dir: Optional[str] = None) -> dict:
    """
    Get cache information

    Args:
        cache_dir: Cache directory

    Returns:
        A dictionary with cache statistics
    """
    if cache_dir is None:
        cache_dir = os.path.join(tempfile.gettempdir(), "pysparq_dynamic_ops")

    info = {
        "cache_dir": cache_dir,
        "exists": os.path.exists(cache_dir),
        "file_count": 0,
        "so_count": 0,
        "cpp_count": 0,
        "total_size_mb": 0.0,
    }

    if not info["exists"]:
        return info

    total_size = 0
    for filename in os.listdir(cache_dir):
        filepath = os.path.join(cache_dir, filename)
        if os.path.isfile(filepath):
            info["file_count"] += 1
            total_size += os.path.getsize(filepath)

            if filename.endswith(".so"):
                info["so_count"] += 1
            elif filename.endswith(".cpp"):
                info["cpp_count"] += 1

    info["total_size_mb"] = round(total_size / (1024 * 1024), 2)

    return info


# ========== Convenience functions ==========

def quick_compile(
    class_code: str,
    class_name: str,
    verbose: bool = False,
) -> str:
    """
    Quickly compile C++ operator code

    Args:
        class_code: C++ code containing the class definition
        class_name: Class name
        verbose: Whether to print verbose logs

    Returns:
        The shared library file path
    """
    return compile_cpp_code(
        cpp_code=class_code,
        class_name=class_name,
        verbose=verbose,
    )
