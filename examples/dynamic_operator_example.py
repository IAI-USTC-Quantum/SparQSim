#!/usr/bin/env python3
"""
Dynamic Operator Extension Example

Demonstrates how to use the compile_operator feature to create custom C++ operators at runtime.

Before running, make sure:
1. QRAM-Simulator has been built correctly
2. The PySparQ module can be imported

Usage:
    python examples/dynamic_operator_example.py
"""

import sys
import os

# Add the project root directory to the path
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(project_root, "PySparQ"))  # Source-tree import; uses site-packages when installed

print("=" * 70)
print("PySparQ Dynamic Operator Extension Example")
print("=" * 70)
print()

# Try importing from PySparQ
try:
    from pysparq import compile_operator
    print("✓ Successfully imported compile_operator from pysparq")
except ImportError:
    print("! pysparq not fully installed, trying to import dynamic_operator directly...")
    from pysparq.dynamic_operator import compile_operator
    print("✓ Successfully imported from pysparq.dynamic_operator")

print()

# ============ Example 1: SelfAdjointOperator ============
print("-" * 70)
print("Example 1: SelfAdjointOperator (self-adjoint operator)")
print("-" * 70)
print()
print("SelfAdjointOperator is used for Hermitian operators, whose dagger operation automatically equals the operator itself.")
print("Typical applications: Pauli gates (X, Z), the controlled-NOT gate (CNOT), etc.")
print()

cpp_code_1 = """
class MyFlipOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    MyFlipOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value ^= 1;  // XOR flip
        }
    }
};
"""

try:
    MyFlipOp = compile_operator(
        name="MyFlipOp",
        cpp_code=cpp_code_1,
        base_class="SelfAdjointOperator",
        constructor_args=[("size_t", "reg_id")],
        verbose=True,
    )
    print()
    print("✓ SelfAdjointOperator created successfully")
    print(f"  Class name: {MyFlipOp.__name__}")
    print(f"  Base class: {MyFlipOp._base_class}")
    print(f"  Note: dagger() automatically equals operator()")
    print()
except Exception as e:
    print(f"✗ Creation failed: {e}")
    print()

# ============ Example 2: BaseOperator with dagger ============
print("-" * 70)
print("Example 2: BaseOperator (with dagger implementation)")
print("-" * 70)
print()
print("BaseOperator requires manually implementing the dag() method, for non-Hermitian operators.")
print("Typical applications: phase gates, controlled phase gates, general unitary gates, etc.")
print()

cpp_code_2 = """
class MyPhaseOp : public BaseOperator {
    size_t reg_id;
    double phase;
public:
    MyPhaseOp(size_t r, double p) : reg_id(r), phase(p) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            if (s.get(reg_id).value != 0) {
                s.amplitude *= std::exp(std::complex<double>(0, phase));
            }
        }
    }
    void dag(std::vector<System>& state) const override {
        for (auto& s : state) {
            if (s.get(reg_id).value != 0) {
                s.amplitude *= std::exp(std::complex<double>(0, -phase));
            }
        }
    }
};
"""

try:
    MyPhaseOp = compile_operator(
        name="MyPhaseOp",
        cpp_code=cpp_code_2,
        base_class="BaseOperator",
        constructor_args=[("size_t", "reg_id"), ("double", "phase")],
        verbose=True,
    )
    print()
    print("✓ BaseOperator created successfully")
    print(f"  Class name: {MyPhaseOp.__name__}")
    print(f"  Base class: {MyPhaseOp._base_class}")
    print(f"  Note: the dag() method must be implemented for the inverse operation")
    print()
except Exception as e:
    print(f"✗ Creation failed: {e}")
    print()

# ============ Example 3: Multi-parameter operator ============
print("-" * 70)
print("Example 3: Complex multi-parameter operator")
print("-" * 70)
print()
print("Dynamic operators support multiple constructor arguments, which can be used for complex quantum operations.")
print()

cpp_code_3 = """
class MyControlledOp : public SelfAdjointOperator {
    size_t control_reg;
    size_t target_reg;
    double angle;
public:
    MyControlledOp(size_t c, size_t t, double a)
        : control_reg(c), target_reg(t), angle(a) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            if (s.get(control_reg).value != 0) {
                s.amplitude *= std::exp(std::complex<double>(0, angle));
            }
        }
    }
};
"""

try:
    MyControlledOp = compile_operator(
        name="MyControlledOp",
        cpp_code=cpp_code_3,
        base_class="SelfAdjointOperator",
        constructor_args=[
            ("size_t", "control_reg"),
            ("size_t", "target_reg"),
            ("double", "angle"),
        ],
        verbose=True,
    )
    print()
    print("✓ Multi-parameter operator created successfully")
    print(f"  Class name: {MyControlledOp.__name__}")
    print(f"  Parameter list:")
    print(f"    - control_reg (size_t): control register ID")
    print(f"    - target_reg (size_t): target register ID")
    print(f"    - angle (double): phase angle (in radians)")
    print()
except Exception as e:
    print(f"✗ Creation failed: {e}")
    print()

# ============ Example 4: Creating and using instances ============
print("-" * 70)
print("Example 4: Creating operator instances")
print("-" * 70)
print()

try:
    # Create instances using the operator classes defined above
    if 'MyFlipOp' in dir():
        flip_op = MyFlipOp(reg_id=0)
        print(f"MyFlipOp instance: {repr(flip_op)}")

    if 'MyPhaseOp' in dir():
        import math
        phase_op = MyPhaseOp(reg_id=0, phase=math.pi/4)
        print(f"MyPhaseOp instance: {repr(phase_op)}")

    if 'MyControlledOp' in dir():
        ctrl_op = MyControlledOp(control_reg=0, target_reg=1, angle=math.pi/2)
        print(f"MyControlledOp instance: {repr(ctrl_op)}")

    print()
    print("Note: applying operators to an actual quantum state requires the fully compiled PySparQ module.")
    print("Once compiled, you can use: op(state) or op.dag(state)")

except Exception as e:
    print(f"✗ Failed to create instance: {e}")

print()

# ============ Example 5: Cache management ============
print("-" * 70)
print("Example 5: Compilation cache management")
print("-" * 70)
print()

try:
    from pysparq.dynamic_operator import get_cache_info, clear_cache

    info = get_cache_info()
    print(f"Cache directory: {info['cache_dir']}")
    print(f"Number of cache files: {info['so_count']}")
    print(f"Cache size: {info['total_size_mb']:.2f} MB")
    print()
    print("Compilation results are cached automatically, avoiding repeated compilation of identical code.")
    print("To clear the cache, call: clear_cache()")

except ImportError:
    print("Could not import the cache management functions")

print()

# ============ Summary ============
print("=" * 70)
print("Example completed")
print("=" * 70)

print("""
Summary:
-----
This example demonstrated three basic ways to use dynamic operators:

1. SelfAdjointOperator (self-adjoint operator):
   - dagger() automatically equals operator()
   - Suitable for Hermitian operators such as Pauli gates and CNOT
   - Simpler to implement: only a single operator() method is needed

2. BaseOperator (general operator):
   - Requires manually implementing the dag() method
   - Suitable for non-Hermitian operators such as phase gates and general unitary gates
   - Supports custom inverse operations

3. Multi-parameter operators:
   - Support multiple parameter types: size_t, int, double, float, bool, uint64_t
   - Specify the parameter list via constructor_args
   - Use keyword arguments when creating instances

For more information, refer to:
- docs/sphinx/source/guide/dynamic_operators.rst (full user guide)
- docs/sphinx/source/api/dynamic_operator.rst (API reference)
- examples/dynamic_operator_quantum.py (end-to-end quantum circuit example)
- PySparQ/test/test_dynamic_operator.py (unit tests)
""")
