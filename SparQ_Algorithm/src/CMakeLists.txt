project(SparQ_Algorithm)

file(GLOB_RECURSE src "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
					  "${CMAKE_CURRENT_SOURCE_DIR}/BlockEncoding/*.cpp"
					  "${CMAKE_CURRENT_SOURCE_DIR}/DiscreteAdiabatic/*.cpp")
add_library(${PROJECT_NAME} STATIC ${src})

if (CUDA_FOUND)
    file(GLOB_RECURSE cuda_src ${CMAKE_CURRENT_SOURCE_DIR}/cuda/*.cu
                               ${CMAKE_CURRENT_SOURCE_DIR}/BlockEncoding/cuda/*.cu
                               ${CMAKE_CURRENT_SOURCE_DIR}/DiscreteAdiabatic/cuda/*.cu)

    # 以 nvcc 重编全部源码（USE_CUDA=1 切换数据结构布局），
    # 目标配置统一由子模块 cmake/CUDA.cmake 的 sparq_setup_cuda_target 提供。
    include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
    add_library(${PROJECT_NAME}_cuda STATIC ${cuda_src} ${src})
    sparq_setup_cuda_target(${PROJECT_NAME}_cuda)
endif()

# The umbrella `SparQ` target is defined in the repository ROOT CMakeLists
# (it aggregates this library plus SparQ_Simulator and the QRAM-Simulator
# submodule's SparQ_QRAMSimulator / SparQ_Common / fmt).

target_compile_definitions(SparQ_Algorithm PRIVATE EIGEN_NO_CUDA)