#!/usr/bin/env python3
"""
Dynamic operator module unit tests

Tests compile_operator functionality:
- Simple SelfAdjointOperator compilation
- Operators with constructor arguments
- Difference between BaseOperator and SelfAdjointOperator
- Compilation error handling
- Compilation caching mechanism
"""

import pytest
import shutil
import sys
import os

# JIT compilation depends on g++ (see CONTRIBUTING); skip the whole module in
# environments without a compiler (e.g. a Windows runner without MinGW),
# rather than failing case by case
if not shutil.which("g++"):
    pytest.skip("g++ not available — dynamic operator JIT tests skipped",
                allow_module_level=True)

# Add the project root directory to the path
project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, project_root)

from pysparq.dynamic_operator import (
    compile_operator,
    CompilationError,
    clear_cache,
    get_cache_info,
    DynamicOperatorLoadError,
)


class TestCompileOperator:
    """Test the compile_operator function"""

    def test_compile_simple_self_adjoint_operator(self):
        """Test compiling a simple SelfAdjointOperator"""
        cpp_code = """
class TestFlipOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestFlipOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value ^= 1;
        }
    }
};
"""
        # Compile the operator
        OpClass = compile_operator(
            name="TestFlipOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
            verbose=False,
        )

        # Verify the class was created successfully
        assert OpClass is not None
        assert OpClass.__name__ == "TestFlipOp"

    def test_operator_with_constructor_args(self):
        """Test an operator with constructor arguments"""
        cpp_code = """
class TestMultiParamOp : public SelfAdjointOperator {
    size_t reg_id;
    double factor;
    int offset;
public:
    TestMultiParamOp(size_t r, double f, int o) : reg_id(r), factor(f), offset(o) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value = static_cast<size_t>(s.get(reg_id).value * factor + offset);
        }
    }
};
"""
        OpClass = compile_operator(
            name="TestMultiParamOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[
                ("size_t", "reg_id"),
                ("double", "factor"),
                ("int", "offset"),
            ],
        )

        assert OpClass is not None
        assert OpClass.__name__ == "TestMultiParamOp"

        # Verify the docstring contains parameter information
        assert "reg_id" in OpClass.__doc__
        assert "factor" in OpClass.__doc__
        assert "offset" in OpClass.__doc__

    def test_base_vs_self_adjoint(self):
        """Test the difference between BaseOperator and SelfAdjointOperator"""
        # SelfAdjointOperator code
        self_adjoint_code = """
class TestSelfAdjoint : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestSelfAdjoint(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.amplitude *= -1.0;
        }
    }
};
"""
        # BaseOperator code (requires a custom dagger)
        base_code = """
class TestBaseOp : public BaseOperator {
    size_t reg_id;
    double phase;
public:
    TestBaseOp(size_t r, double p) : reg_id(r), phase(p) {}
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
        SelfAdjointOp = compile_operator(
            name="TestSelfAdjoint",
            cpp_code=self_adjoint_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        BaseOp = compile_operator(
            name="TestBaseOp",
            cpp_code=base_code,
            base_class="BaseOperator",
            constructor_args=[("size_t", "reg_id"), ("double", "phase")],
        )

        assert SelfAdjointOp is not None
        assert BaseOp is not None
        assert SelfAdjointOp._base_class == "SelfAdjointOperator"
        assert BaseOp._base_class == "BaseOperator"

    def test_compilation_error_handling(self):
        """Test the friendly message for compilation errors"""
        # Code with syntax errors
        bad_cpp_code = """
class BadOp : public BaseOperator {  // missing semicolon
    void operator()(std::vector<System>& state) const override {
        undefined_variable = 42;  // undefined variable
    }
};
"""
        with pytest.raises(CompilationError) as exc_info:
            compile_operator(
                name="BadOp",
                cpp_code=bad_cpp_code,
                base_class="BaseOperator",
            )

        # Verify the error message mentions the compilation failure
        error_msg = str(exc_info.value)
        assert "Compilation failed" in error_msg or "error" in error_msg.lower()

    def test_invalid_base_class(self):
        """Test an invalid base_class argument"""
        with pytest.raises(ValueError) as exc_info:
            compile_operator(
                name="TestOp",
                cpp_code="class TestOp : public InvalidBase {};",
                base_class="InvalidBase",
            )
        assert "base_class" in str(exc_info.value)

    def test_empty_name(self):
        """Test an empty name argument"""
        with pytest.raises(ValueError):
            compile_operator(
                name="",
                cpp_code="class TestOp : public BaseOperator {};",
            )

    def test_empty_cpp_code(self):
        """Test an empty cpp_code argument"""
        with pytest.raises(ValueError):
            compile_operator(
                name="TestOp",
                cpp_code="",
            )


class TestCaching:
    """Test the caching mechanism"""

    def setup_method(self):
        """Clear the cache before each test"""
        clear_cache()

    def teardown_method(self):
        """Clear the cache after each test"""
        clear_cache()

    def test_caching(self):
        """Test compilation caching"""
        cpp_code = """
class TestCachedOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestCachedOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value ^= 1;
        }
    }
};
"""
        # First compilation
        OpClass1 = compile_operator(
            name="TestCachedOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        # Get the library path from the first compilation
        lib_path1 = OpClass1._lib_path

        # Second compilation of the same code (should use the cache)
        OpClass2 = compile_operator(
            name="TestCachedOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        lib_path2 = OpClass2._lib_path

        # Verify both compilations used the same library file
        assert lib_path1 == lib_path2

    def test_cache_info(self):
        """Test cache info retrieval"""
        # Should be empty after clearing
        clear_cache()
        info = get_cache_info()
        assert info["exists"] == True

        # Compile an operator
        cpp_code = """
class TestCacheInfoOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestCacheInfoOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {}
};
"""
        compile_operator(
            name="TestCacheInfoOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        # Get cache info
        info = get_cache_info()
        assert info["exists"] == True
        assert info["file_count"] >= 1

    def test_clear_cache(self):
        """Test clearing the cache"""
        # First compile an operator
        cpp_code = """
class TestClearCacheOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestClearCacheOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {}
};
"""
        compile_operator(
            name="TestClearCacheOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        # Confirm the cache exists
        info_before = get_cache_info()
        assert info_before["file_count"] > 0

        # Clear the cache
        count = clear_cache()
        assert count > 0

        # Confirm the cache was cleared
        info_after = get_cache_info()
        assert info_after["file_count"] == 0


class TestOperatorWrapper:
    """Test operator wrapper functionality"""

    def test_operator_repr(self):
        """Test the string representation of an operator"""
        cpp_code = """
class TestReprOp : public SelfAdjointOperator {
    size_t reg_id;
    double param;
public:
    TestReprOp(size_t r, double p) : reg_id(r), param(p) {}
    void operator()(std::vector<System>& state) const override {}
};
"""
        OpClass = compile_operator(
            name="TestReprOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id"), ("double", "param")],
        )

        # Create an instance and check its repr
        op = OpClass(reg_id=0, param=3.14)
        repr_str = repr(op)
        assert "TestReprOp" in repr_str
        assert "reg_id=0" in repr_str
        assert "param=" in repr_str


class TestAdvancedFeatures:
    """Test advanced features"""

    def test_complex_operator(self):
        """Test compiling a complex operator"""
        cpp_code = """
class TestComplexOp : public SelfAdjointOperator {
    size_t reg_a;
    size_t reg_b;
    double angle;
public:
    TestComplexOp(size_t a, size_t b, double theta) 
        : reg_a(a), reg_b(b), angle(theta) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            auto val_a = s.get(reg_a).value;
            auto val_b = s.get(reg_b).value;
            s.amplitude *= std::exp(std::complex<double>(0, angle * (val_a + val_b)));
        }
    }
};
"""
        OpClass = compile_operator(
            name="TestComplexOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[
                ("size_t", "reg_a"),
                ("size_t", "reg_b"),
                ("double", "angle"),
            ],
        )

        assert OpClass is not None
        op = OpClass(reg_a=0, reg_b=1, angle=1.57)
        assert op is not None

    def test_operator_with_extra_includes(self):
        """Test compilation with extra include paths"""
        cpp_code = """
class TestIncludeOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestIncludeOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value = 0;
        }
    }
};
"""
        # Use extra include paths (should work even when empty)
        OpClass = compile_operator(
            name="TestIncludeOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
            extra_includes=[],
        )

        assert OpClass is not None


# ============ Run tests ============

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
