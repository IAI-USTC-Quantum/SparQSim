/**
 * @file dynamic_operator_loader.h
 * @brief C++ 动态算子加载器
 * @details 用于运行时加载编译后的动态链接库（.so/.dll），获取算子工厂函数
 */

#pragma once

#include <string>
#include <functional>
#include <memory>

namespace pysparq {

/**
 * @brief 动态库加载器
 * @details 封装了平台相关的动态库加载功能，支持 Linux(dlopen)、Windows(LoadLibrary) 和 macOS
 */
class DynamicOperatorLoader {
public:
    /**
     * @brief 构造函数，加载指定的动态库
     * @param lib_path 动态库文件路径（.so/.dll/.dylib）
     */
    explicit DynamicOperatorLoader(const std::string& lib_path);

    /**
     * @brief 析构函数，自动卸载动态库
     */
    ~DynamicOperatorLoader();

    // 禁止拷贝（动态库句柄不可复制）
    DynamicOperatorLoader(const DynamicOperatorLoader&) = delete;
    DynamicOperatorLoader& operator=(const DynamicOperatorLoader&) = delete;

    // 允许移动
    DynamicOperatorLoader(DynamicOperatorLoader&& other) noexcept;
    DynamicOperatorLoader& operator=(DynamicOperatorLoader&& other) noexcept;

    /**
     * @brief 获取动态库中的符号（工厂函数）
     * @param name 符号名称（如 "create_operator"）
     * @return 指向符号的指针，失败返回 nullptr
     */
    void* get_symbol(const std::string& name);

    /**
     * @brief 检查动态库是否成功加载
     * @return true 表示加载成功，false 表示加载失败
     */
    bool is_valid() const;

    /**
     * @brief 获取最后一次错误信息
     * @return 错误描述字符串，无错误时返回空字符串
     */
    std::string get_error() const;

    /**
     * @brief 获取动态库路径
     * @return 库文件路径
     */
    const std::string& get_lib_path() const;

private:
    void* handle_;              ///< 动态库句柄（平台相关）
    std::string lib_path_;      ///< 库文件路径
    std::string error_msg_;     ///< 错误信息

    /**
     * @brief 清除当前错误信息
     */
    void clear_error();

    /**
     * @brief 设置错误信息（从系统获取）
     */
    void set_error_from_system();

    /**
     * @brief 关闭动态库（内部实现）
     */
    void close_library();
};

/**
 * @brief 工厂函数类型别名
 * @details 用于创建 BaseOperator 派生对象的工厂函数签名
 */
using CreateOperatorFunc = void* (*)();

/**
 * @brief 带类型的符号获取辅助函数
 * @tparam FuncType 函数指针类型
 * @param loader 动态库加载器
 * @param name 符号名称
 * @return 类型化的函数指针，失败返回 nullptr
 */
template<typename FuncType>
FuncType get_typed_symbol(DynamicOperatorLoader& loader, const std::string& name) {
    void* symbol = loader.get_symbol(name);
    if (symbol == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<FuncType>(symbol);
}

} // namespace pysparq
