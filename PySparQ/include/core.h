/**
 * @file core.h
 * @brief PySparQ 绑定层辅助宏定义
 * @details 定义 pybind11 绑定模块（core.cpp）复用的注册宏：
 *          可控方法批量绑定（BIND_CONTROLLABLE_METHODS）、算子类注册
 *          （BIND_BASE_OPERATOR / BIND_SELF_ADJOINT_OPERATOR / BIND_BASE_OPERATOR_SUBNAME）
 *          以及 dagger 方法绑定（BIND_DAG_METHODS）
 */

#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/complex.h"
#include "pybind11/functional.h"
#include "pybind11/operators.h"
#include "pybind11/eigen.h"
#include "sparse_state_simulator.h"


using namespace pybind11::literals;
namespace py = pybind11;
using namespace qram_simulator;

using namespace std;

// Bind ClassControllable classes extra methods and attributes
/**
 * @brief 批量绑定可控（ClassControllable）算子类的条件控制方法与条件变量属性
 * @details 展开为一条 .def(...) 链，包含：
 *          四个 condition_variable_* 只读属性、conditioned_by_nonzeros /
 *          conditioned_by_all_ones / conditioned_by_bit / conditioned_by_value
 *          四组多载条件设置方法，以及对应的 clear_control_* 清除方法
 * @param CLASS_NAME 待绑定可控方法的算子类名
 */
#define BIND_CONTROLLABLE_METHODS(CLASS_NAME)                                                                                                                               \
    .def_readonly("condition_variable_nonzeros", &CLASS_NAME::condition_variable_nonzeros)                                                                                  \
        .def_readonly("condition_variable_all_ones", &CLASS_NAME::condition_variable_all_ones)                                                                              \
        .def_readonly("condition_variable_by_value", &CLASS_NAME::condition_variable_by_value)                                                                              \
        .def_readonly("condition_variable_by_bit", &CLASS_NAME::condition_variable_by_bit) /* conditioned_by_nonzeros */                                                    \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view)>(&CLASS_NAME::conditioned_by_nonzeros),                                   \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::string_view> &)>(&CLASS_NAME::conditioned_by_nonzeros),              \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t)>(&CLASS_NAME::conditioned_by_nonzeros),                                             \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<size_t> &)>(&CLASS_NAME::conditioned_by_nonzeros),                        \
             py::arg("conds"), py::return_value_policy::reference_internal) /* conditioned_by_all_ones */                                                                   \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view)>(&CLASS_NAME::conditioned_by_all_ones),                                   \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::string_view> &)>(&CLASS_NAME::conditioned_by_all_ones),              \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t)>(&CLASS_NAME::conditioned_by_all_ones),                                             \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<size_t> &)>(&CLASS_NAME::conditioned_by_all_ones),                        \
             py::arg("conds"), py::return_value_policy::reference_internal) /* conditioned_by_bit */                                                                        \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view, size_t)>(&CLASS_NAME::conditioned_by_bit),                                     \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<std::string_view, size_t>> &)>(&CLASS_NAME::conditioned_by_bit),     \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t, size_t)>(&CLASS_NAME::conditioned_by_bit),                                               \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<size_t, size_t>> &)>(&CLASS_NAME::conditioned_by_bit),               \
             py::arg("conds"), py::return_value_policy::reference_internal) /* conditioned_by_value */                                                                      \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view, size_t)>(&CLASS_NAME::conditioned_by_value),                                 \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<std::string_view, size_t>> &)>(&CLASS_NAME::conditioned_by_value), \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t, size_t)>(&CLASS_NAME::conditioned_by_value),                                           \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<size_t, size_t>> &)>(&CLASS_NAME::conditioned_by_value),           \
             py::arg("conds"), py::return_value_policy::reference_internal) /* clear methods */                                                                             \
        .def("clear_control_nonzeros", &CLASS_NAME::clear_control_nonzeros)                                                                                                 \
        .def("clear_control_all_ones", &CLASS_NAME::clear_control_all_ones)                                                                                                 \
        .def("clear_control_by_bit", &CLASS_NAME::clear_control_by_bit)                                                                                                     \
        .def("clear_control_by_value", &CLASS_NAME::clear_control_by_value)

/**
 * @brief 注册一个派生自 BaseOperator 的算子类
 * @param NAME C++ 类名（同时作为 Python 侧导出的类名）
 */
#define BIND_BASE_OPERATOR(NAME) \
    py::class_<NAME, BaseOperator>(m, #NAME)

/* Workaround for binding inherited operator() of SelfAdjointOperator
*
    Author: Agony5757
    Date:   2025/2/26

   This is used to solve the issue for Windows platform which has a trouble if
   calling operator() in Python. However, the underlying mechanism is unclear
   to me, so I just use a workaround to avoid the issue. */
/**
 * @brief 注册一个派生自 SelfAdjointOperator 的自伴算子类
 * @details Windows 平台下额外显式绑定 __call__（operator()），
 *          以绕开继承的 operator() 在 Python 调用时出现的问题（见上方英文说明）
 * @param NAME C++ 类名（同时作为 Python 侧导出的类名）
 */
#ifdef _WIN32
#define BIND_SELF_ADJOINT_OPERATOR(NAME)            \
    py::class_<NAME, SelfAdjointOperator>(m, #NAME) \
        .def("__call__", (void (NAME::*)(SparseState &) const) & NAME::operator(), py::arg("state"))
#else
#define BIND_SELF_ADJOINT_OPERATOR(NAME) \
    py::class_<NAME, SelfAdjointOperator>(m, #NAME)
#endif

/**
 * @brief 注册一个派生自 BaseOperator 的算子类（自定义 Python 类名）
 * @param NAME C++ 类名
 * @param PYNAME Python 侧导出的类名（可与 C++ 类名不同）
 */
#define BIND_BASE_OPERATOR_SUBNAME(NAME, PYNAME) \
    py::class_<NAME, BaseOperator>(m, PYNAME)

/**
 * @brief 为算子类绑定 dagger（伴随/逆）方法 dag(state)
 * @param NAME C++ 类名
 */
#define BIND_DAG_METHODS(NAME) \
    .def("dag", (void (NAME::*)(SparseState &) const) & NAME::dag, py::arg("state"))
