project(Experiment_GPUTime)

# 目标配置统一由子模块 cmake/CUDA.cmake 的 sparq_setup_cuda_target 提供。
include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)

foreach(_t parallel_test block_encoding_test cks_test qda_test)
    add_executable(${PROJECT_NAME}_${_t} ${_t}.cu)
    target_link_libraries(${PROJECT_NAME}_${_t} PRIVATE SparQ_cuda)
    sparq_setup_cuda_target(${PROJECT_NAME}_${_t})
endforeach()
