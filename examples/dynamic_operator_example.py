#!/usr/bin/env python3
"""
动态算子扩展示例

演示如何使用 compile_operator 功能在运行时创建自定义 C++ 算子。

运行前请确保：
1. QRAM-Simulator 已正确构建
2. PySparQ 模块可导入

使用方法：
    python dynamic_operator_example.py
"""

import sys
import os

# 将项目根目录添加到路径
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, project_root)

# 尝试从 PySparQ 导入，如果失败则使用动态模块
# 注意：这需要 PySparQ 已正确安装
print("=" * 60)
print("动态算子扩展示例")
print("=" * 60)

try:
    # 首选：从 pysparq 导入 compile_operator
    from pysparq import compile_operator
    print("✓ 成功从 pysparq 导入 compile_operator")
except ImportError:
    # 备选：直接从 dynamic_operator 模块导入
    print("! pysparq 未完全安装，尝试直接导入 dynamic_operator...")
    from pysparq.dynamic_operator import compile_operator
    print("✓ 成功从 pysparq.dynamic_operator 导入")

print()

# ============ 示例 1: SelfAdjointOperator ============
print("-" * 60)
print("示例 1: SelfAdjointOperator (自伴算子)")
print("-" * 60)

cpp_code_1 = """
class MyFlipOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    MyFlipOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value ^= 1;
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
    print("✓ SelfAdjointOperator 创建成功")
    print(f"  类名: {MyFlipOp.__name__}")
    print(f"  基类: {MyFlipOp._base_class}")
    print()
except Exception as e:
    print(f"✗ 创建失败: {e}")
    print()

# ============ 示例 2: BaseOperator with dagger ============
print("-" * 60)
print("示例 2: BaseOperator (带 dagger 实现)")
print("-" * 60)

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
    print("✓ BaseOperator 创建成功")
    print(f"  类名: {MyPhaseOp.__name__}")
    print(f"  基类: {MyPhaseOp._base_class}")
    print()
except Exception as e:
    print(f"✗ 创建失败: {e}")
    print()

# ============ 示例 3: 复杂算子 ============
print("-" * 60)
print("示例 3: 复杂算子 (多参数)")
print("-" * 60)

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
                // 当控制位为 1 时，对目标位应用相位
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
    print("✓ 复杂算子创建成功")
    print(f"  类名: {MyControlledOp.__name__}")
    print(f"  参数: control_reg, target_reg, angle")
    print()
except Exception as e:
    print(f"✗ 创建失败: {e}")
    print()

# ============ 示例 4: 使用示例 ============
print("-" * 60)
print("示例 4: 使用动态算子 (如果 pysparq 可用)")
print("-" * 60)

try:
    from pysparq import SparseState, System, Boolean

    # 创建一个简单的量子态
    System.clear()
    q = System.add_register("q", Boolean, 1)
    state = SparseState()

    print(f"✓ 创建量子态，寄存器数量: {System.get_activated_register_size()}")

    # 如果前面的算子创建成功，尝试使用它们
    if 'MyFlipOp' in dir():
        flip_op = MyFlipOp(reg_id=q)
        print(f"✓ 创建 MyFlipOp 实例: {repr(flip_op)}")
        # 注意：实际调用需要完整的 SparseState 支持
        # state = flip_op(state)

    print()

except ImportError:
    print("! pysparq._core 未编译，跳过使用示例")
    print("  要测试完整功能，请先构建 PySparQ:")
    print("    pip install -e .")
    print()

# ============ 总结 ============
print("=" * 60)
print("示例完成")
print("=" * 60)

print("""
说明:
1. SelfAdjointOperator: dagger 操作自动等于自身，适合厄米算子
2. BaseOperator: 需要手动实现 dagger 方法，适合一般算子
3. compile_operator 会自动缓存编译结果，避免重复编译

更多信息请参考:
- PySparQ/dynamic_operator/README.md
- PySparQ/test/test_dynamic_operator.py (单元测试)
""")
