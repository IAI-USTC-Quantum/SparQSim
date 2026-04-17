/**
 * @file dynamic_operator_loader.cpp
 * @brief 动态算子加载器实现
 * @details 实现跨平台的动态库加载功能
 */

#include "dynamic_operator_loader.h"

#include <iostream>
#include <cstring>

// 平台相关的头文件
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace pysparq {

// ========== 构造函数 ==========

DynamicOperatorLoader::DynamicOperatorLoader(const std::string& lib_path)
    : handle_(nullptr)
    , lib_path_(lib_path)
    , error_msg_("")
{
    if (lib_path.empty()) {
        error_msg_ = "Library path is empty";
        return;
    }

#ifdef _WIN32
    // Windows: 使用 LoadLibraryA
    handle_ = static_cast<void*>(LoadLibraryA(lib_path.c_str()));
    if (handle_ == nullptr) {
        DWORD error_code = GetLastError();
        char error_buffer[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            error_buffer,
            sizeof(error_buffer),
            nullptr
        );
        error_msg_ = std::string("Failed to load library: ") + error_buffer;
    }
#else
    // Linux/macOS: 使用 dlopen
    // RTLD_NOW: 立即解析所有符号
    // RTLD_LOCAL: 符号不对外可见
    handle_ = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr) {
        const char* dl_error = dlerror();
        error_msg_ = std::string("Failed to load library: ") + (dl_error ? dl_error : "unknown error");
    }
#endif
}

// ========== 析构函数 ==========

DynamicOperatorLoader::~DynamicOperatorLoader() {
    close_library();
}

// ========== 移动构造函数 ==========

DynamicOperatorLoader::DynamicOperatorLoader(DynamicOperatorLoader&& other) noexcept
    : handle_(other.handle_)
    , lib_path_(std::move(other.lib_path_))
    , error_msg_(std::move(other.error_msg_))
{
    other.handle_ = nullptr;
    other.error_msg_.clear();
}

// ========== 移动赋值运算符 ==========

DynamicOperatorLoader& DynamicOperatorLoader::operator=(DynamicOperatorLoader&& other) noexcept {
    if (this != &other) {
        close_library();
        handle_ = other.handle_;
        lib_path_ = std::move(other.lib_path_);
        error_msg_ = std::move(other.error_msg_);
        other.handle_ = nullptr;
        other.error_msg_.clear();
    }
    return *this;
}

// ========== 公共接口 ==========

void* DynamicOperatorLoader::get_symbol(const std::string& name) {
    if (!is_valid()) {
        error_msg_ = "Library is not loaded";
        return nullptr;
    }

    if (name.empty()) {
        error_msg_ = "Symbol name is empty";
        return nullptr;
    }

    clear_error();
    void* symbol = nullptr;

#ifdef _WIN32
    // Windows: 使用 GetProcAddress
    HMODULE hModule = static_cast<HMODULE>(handle_);
    FARPROC proc = GetProcAddress(hModule, name.c_str());
    if (proc == nullptr) {
        DWORD error_code = GetLastError();
        char error_buffer[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            error_buffer,
            sizeof(error_buffer),
            nullptr
        );
        error_msg_ = std::string("Failed to get symbol '") + name + "': " + error_buffer;
        return nullptr;
    }
    symbol = reinterpret_cast<void*>(proc);
#else
    // Linux/macOS: 使用 dlsym
    symbol = dlsym(handle_, name.c_str());
    if (symbol == nullptr) {
        const char* dl_error = dlerror();
        if (dl_error != nullptr) {
            error_msg_ = std::string("Failed to get symbol '") + name + "': " + dl_error;
            return nullptr;
        }
    }
#endif

    return symbol;
}

bool DynamicOperatorLoader::is_valid() const {
    return handle_ != nullptr;
}

std::string DynamicOperatorLoader::get_error() const {
    return error_msg_;
}

const std::string& DynamicOperatorLoader::get_lib_path() const {
    return lib_path_;
}

// ========== 私有辅助方法 ==========

void DynamicOperatorLoader::clear_error() {
    error_msg_.clear();
#ifndef _WIN32
    dlerror();  // 清除之前的错误状态
#endif
}

void DynamicOperatorLoader::set_error_from_system() {
#ifdef _WIN32
    DWORD error_code = GetLastError();
    char error_buffer[256];
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error_buffer,
        sizeof(error_buffer),
        nullptr
    );
    error_msg_ = error_buffer;
#else
    const char* dl_error = dlerror();
    if (dl_error != nullptr) {
        error_msg_ = dl_error;
    }
#endif
}

void DynamicOperatorLoader::close_library() {
    if (handle_ != nullptr) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}

} // namespace pysparq
