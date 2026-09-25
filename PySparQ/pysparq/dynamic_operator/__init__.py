"""PySparQ dynamic operator extension module - provides runtime compilation and loading of custom C++ operators.

.. warning::
   ``compile_operator()`` merely compiles the user-provided ``operator()``/``dag()``
   into a shared library and calls it via ctypes; a successful compilation only means
   the code passes C++ type checking. It does **not** imply, and **cannot** statically
   or dynamically prove, that the operator is unitary or that ``operator()``/``dag()``
   are inverses of each other. Any implementation that overwrites registers on its
   own, implements ``dag()`` by zeroing registers, or destroys information in some
   other way will compile without complaint.

   Therefore, the QCFD paths supported by this repository (QECC.Lang-driven
   qfvm/qnls/qham) are **forbidden** from using dynamic operators compiled via
   ``compile_operator``. All semantics must be expressed through named, statically
   checkable PySparQ built-in operators (or Python composite operators built from
   built-in operators), and validated with the conformance test matrix provided by
   ``pysparq.conformance`` (arbitrary non-zero outputs, exhaustive/sampled basis
   states, collision detection, linearity on superpositions, positive/negative/
   multiple controls, and forward+dagger and dagger+forward identity).
   ``compile_operator`` remains a general-purpose (not QCFD-specific) runtime
   operator compilation tool that can be used for prototyping, teaching, or
   experiments unrelated to QCFD, but it must not be treated as having passed any
   unitarity proof.
"""

from typing import List, Tuple, Type, Optional

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

from .operator_wrapper import (
    CppOperatorWrapper,
    DynamicOperatorError,
    DynamicOperatorLoadError,
    DynamicOperatorFactoryError,
    create_operator_class,
    cleanup_all_instances,
)

__all__ = [
    # Compilation related
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
    # Dynamic operator related
    "compile_operator",
    "CppOperatorWrapper",
    "DynamicOperatorError",
    "DynamicOperatorLoadError",
    "DynamicOperatorFactoryError",
    "create_operator_class",
    "cleanup_all_instances",
]

__version__ = "0.2.0"


def compile_operator(
    name: str,
    cpp_code: str,
    base_class: str = "BaseOperator",
    extra_includes: List[str] = None,
    extra_libs: List[str] = None,
    constructor_args: List[Tuple[str, str]] = None,
    cache_dir: Optional[str] = None,
    verbose: bool = False,
) -> Type:
    """Compile C++ code into a dynamic operator class.

    This is a high-level function that compiles user-provided C++ code into a
    shared library and wraps it into an operator class that can be used directly
    from Python. A dynamic operator can be applied to a SparseState just like a
    native PySparQ operator.

    Warning:
        A successful compilation only means ``operator()``/``dag()`` passed C++
        type checking; it **constitutes no unitarity proof whatsoever**: this
        function neither statically nor dynamically verifies that the generated
        operator is unitary, or that ``dag()`` is actually the inverse of
        ``operator()``. Hence the supported QCFD paths (QECC.Lang-driven
        qfvm/qnls/qham) forbid the use of dynamic operators compiled by this
        function; QCFD semantics must use named PySparQ built-in operators and
        be validated with the ``pysparq.conformance`` conformance test matrix.

    Args:
        name: Operator class name. Must be a valid Python class name and must match the class name in the C++ code.
        cpp_code: C++ source code, containing only the class definition part. The code must inherit from
            BaseOperator or SelfAdjointOperator and implement the operator() method.
        base_class: Base class name, determines dagger behavior. Allowed values:
            - "BaseOperator": general operator, requires a manually implemented dag() method
            - "SelfAdjointOperator": Hermitian operator, dag() automatically equals operator()
            Defaults to "BaseOperator".
        extra_includes: List of extra header search paths. PySparQ headers are included automatically.
        extra_libs: List of extra libraries to link. Most operators need no extra libraries.
        constructor_args: List of constructor arguments in the form [(type, name), ...].
            Supported types: size_t, int, long, double, float, bool, uint64_t.
            Example: [("size_t", "reg_id"), ("double", "phase")]
        cache_dir: Cache directory path. Defaults to pysparq_dynamic_ops/ under the system temporary directory.
        verbose: Whether to print verbose compilation logs, useful for debugging.

    Returns:
        The dynamically generated operator class. Instances are created with keyword
        arguments, e.g.: OpClass(reg_id=0, phase=1.0)

    Raises:
        CompilationError: C++ compilation failed. The error message contains detailed compiler output.
        DynamicOperatorLoadError: Failed to load the dynamic library.
        ValueError: Invalid arguments (e.g. empty name, invalid base class, etc.).

    Example:
        Create a simple flip operator:

        >>> from pysparq.dynamic_operator import compile_operator
        >>>
        >>> cpp_code = '''
        ... class FlipOp : public SelfAdjointOperator {
        ...     size_t reg_id;
        ... public:
        ...     FlipOp(size_t r) : reg_id(r) {}
        ...     void operator()(std::vector<System>& state) const override {
        ...         for (auto& s : state) {
        ...             s.get(reg_id).value ^= 1;
        ...         }
        ...     }
        ... };
        ... '''
        >>>
        >>> FlipOp = compile_operator(
        ...     name="FlipOp",
        ...     cpp_code=cpp_code,
        ...     base_class="SelfAdjointOperator",
        ...     constructor_args=[("size_t", "reg_id")]
        ... )
        >>>
        >>> # Create an instance
        >>> op = FlipOp(reg_id=0)
        >>> print(repr(op))  # FlipOp(reg_id=0)

    Note:
        - Compiled libraries are cached by code hash to avoid redundant compilation.
        - ABI compatibility issues may exist on Windows (MSVC vs MinGW).
        - The C++ class name must match the Python name parameter.
        - State access inside operators: s.get(reg_id).value gets the value, s.amplitude gets the amplitude.

    See Also:
        get_cache_info: Query the compilation cache status.
        clear_cache: Clear the compilation cache.
        CompilerConfig: Advanced compiler configuration.
    """
    if extra_includes is None:
        extra_includes = []
    if extra_libs is None:
        extra_libs = []
    if constructor_args is None:
        constructor_args = []

    # Argument validation
    if not name or not isinstance(name, str):
        raise ValueError("name must be a valid string")
    if not cpp_code or not isinstance(cpp_code, str):
        raise ValueError("cpp_code must be a valid C++ code string")

    valid_base_classes = ["BaseOperator", "SelfAdjointOperator"]
    if base_class not in valid_base_classes:
        raise ValueError(f"base_class must be one of {valid_base_classes}")

    # Build the constructor argument strings
    ctor_params = ", ".join(f"{arg_type} {arg_name}" for arg_type, arg_name in constructor_args)
    ctor_args = ", ".join(arg_name for _, arg_name in constructor_args)

    # Use the Python-enhanced template
    config = CompilerConfig(
        include_paths=extra_includes,
        libraries=extra_libs,
        template=CompilerConfig.PYTHON_TEMPLATE.replace("{BASE_CLASS}", base_class),
    )

    if verbose:
        print(f"[compile_operator] Compiling operator: {name}")
        print(f"[compile_operator] Base class: {base_class}")
        print(f"[compile_operator] Parameters: {ctor_params}")

    # Auto-detect the project root directory
    project_root = find_project_root()
    if project_root is None:
        raise RuntimeError(
            "Failed to auto-detect the project root directory. Make sure the SparQ/ and PySparQ/ directories exist."
        )

    # Compile the C++ code
    lib_path = compile_cpp_code(
        cpp_code=cpp_code,
        class_name=name,
        cache_dir=cache_dir,
        ctor_params=ctor_params,
        ctor_args=ctor_args,
        config=config,
        project_root=str(project_root),
        verbose=verbose,
    )

    if verbose:
        print(f"[compile_operator] Compilation succeeded: {lib_path}")
        print(f"[compile_operator] Creating the Python class...")

    # Create the Python class
    OpClass = create_operator_class(
        name=name,
        lib_path=lib_path,
        base_class=base_class,
        constructor_args=constructor_args,
    )

    if verbose:
        print(f"[compile_operator] Operator class {name} created")

    return OpClass
