#!/usr/bin/env python3
"""
动态算子模块单元测试

测试 compile_operator 功能：
- 简单 SelfAdjointOperator 编译
- 带构造函数参数的算子
- BaseOperator 和 SelfAdjointOperator 区别
- 编译错误处理
- 编译缓存机制
"""

import pytest
import sys
import os

# 添加项目根目录到路径
project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, project_root)

from PySparQ.dynamic_operator import (
    compile_operator,
    CompilationError,
    clear_cache,
    get_cache_info,
    DynamicOperatorLoadError,
)


class TestCompileOperator:
    """测试 compile_operator 函数"""

    def test_compile_simple_self_adjoint_operator(self):
        """测试编译简单的 SelfAdjointOperator"""
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
        # 编译算子
        OpClass = compile_operator(
            name="TestFlipOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
            verbose=False,
        )

        # 验证类创建成功
        assert OpClass is not None
        assert OpClass.__name__ == "TestFlipOp"

    def test_operator_with_constructor_args(self):
        """测试带构造函数参数的算子"""
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

        # 验证文档字符串包含参数信息
        assert "reg_id" in OpClass.__doc__
        assert "factor" in OpClass.__doc__
        assert "offset" in OpClass.__doc__

    def test_base_vs_self_adjoint(self):
        """测试 BaseOperator 和 SelfAdjointOperator 的区别"""
        # SelfAdjointOperator 代码
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
        # BaseOperator 代码（需要自定义 dagger）
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
        """测试编译错误的友好提示"""
        # 有语法错误的代码
        bad_cpp_code = """
class BadOp : public BaseOperator {  // 缺少分号
    void operator()(std::vector<System>& state) const override {
        undefined_variable = 42;  // 未定义变量
    }
};
"""
        with pytest.raises(CompilationError) as exc_info:
            compile_operator(
                name="BadOp",
                cpp_code=bad_cpp_code,
                base_class="BaseOperator",
            )

        # 验证错误信息包含编译错误
        error_msg = str(exc_info.value)
        assert "编译失败" in error_msg or "error" in error_msg.lower()

    def test_invalid_base_class(self):
        """测试无效的基类参数"""
        with pytest.raises(ValueError) as exc_info:
            compile_operator(
                name="TestOp",
                cpp_code="class TestOp : public InvalidBase {};",
                base_class="InvalidBase",
            )
        assert "base_class" in str(exc_info.value)

    def test_empty_name(self):
        """测试空名称参数"""
        with pytest.raises(ValueError):
            compile_operator(
                name="",
                cpp_code="class TestOp : public BaseOperator {};",
            )

    def test_empty_cpp_code(self):
        """测试空 C++ 代码参数"""
        with pytest.raises(ValueError):
            compile_operator(
                name="TestOp",
                cpp_code="",
            )


class TestCaching:
    """测试缓存机制"""

    def setup_method(self):
        """每个测试前清理缓存"""
        clear_cache()

    def teardown_method(self):
        """每个测试后清理缓存"""
        clear_cache()

    def test_caching(self):
        """测试编译缓存"""
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
        # 第一次编译
        OpClass1 = compile_operator(
            name="TestCachedOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        # 获取第一次的库路径
        lib_path1 = OpClass1._lib_path

        # 第二次编译相同代码（应该使用缓存）
        OpClass2 = compile_operator(
            name="TestCachedOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
        )

        lib_path2 = OpClass2._lib_path

        # 验证两次使用相同的库文件
        assert lib_path1 == lib_path2

    def test_cache_info(self):
        """测试缓存信息获取"""
        # 清理后应该为空
        clear_cache()
        info = get_cache_info()
        assert info["exists"] == True

        # 编译一个算子
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

        # 获取缓存信息
        info = get_cache_info()
        assert info["exists"] == True
        assert info["file_count"] >= 1

    def test_clear_cache(self):
        """测试清除缓存"""
        # 先编译一个算子
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

        # 确认有缓存
        info_before = get_cache_info()
        assert info_before["file_count"] > 0

        # 清除缓存
        count = clear_cache()
        assert count > 0

        # 确认缓存已清除
        info_after = get_cache_info()
        assert info_after["file_count"] == 0


class TestOperatorWrapper:
    """测试算子包装器功能"""

    def test_operator_repr(self):
        """测试算子的字符串表示"""
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

        # 创建实例并检查 repr
        op = OpClass(reg_id=0, param=3.14)
        repr_str = repr(op)
        assert "TestReprOp" in repr_str
        assert "reg_id=0" in repr_str
        assert "param=" in repr_str


class TestAdvancedFeatures:
    """测试高级功能"""

    def test_complex_operator(self):
        """测试复杂算子编译"""
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
        """测试带额外头文件路径的编译"""
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
        # 使用额外的 include 路径（即使为空也应正常工作）
        OpClass = compile_operator(
            name="TestIncludeOp",
            cpp_code=cpp_code,
            base_class="SelfAdjointOperator",
            constructor_args=[("size_t", "reg_id")],
            extra_includes=[],
        )

        assert OpClass is not None


# ============ 运行测试 ============

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
