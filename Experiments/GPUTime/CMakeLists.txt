project(Experiment_GPUTime)

# Target configuration is provided uniformly by sparq_setup_cuda_target in the submodule's cmake/CUDA.cmake.
include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)

foreach(_t parallel_test block_encoding_test cks_test qda_test)
    add_executable(${PROJECT_NAME}_${_t} ${_t}.cu)
    target_link_libraries(${PROJECT_NAME}_${_t} PRIVATE SparQ_cuda)
    sparq_setup_cuda_target(${PROJECT_NAME}_${_t})
endforeach()
