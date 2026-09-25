project(Experiment_CKS)


add_executable(${PROJECT_NAME}_Test HamiltonianSimulationTest.cpp)
target_link_libraries(${PROJECT_NAME}_Test SparQ)


add_executable(${PROJECT_NAME}_Noisy HamiltonianSimulationTest_Noisy.cpp)
target_link_libraries(${PROJECT_NAME}_Noisy SparQ)


if (CUDA_FOUND)
	add_executable(${PROJECT_NAME}_cutest cks_cutest.cu)
	target_link_libraries(${PROJECT_NAME}_cutest PRIVATE SparQ_cuda)

	# 目标配置统一由子模块 cmake/CUDA.cmake 的 sparq_setup_cuda_target 提供。
	include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
	sparq_setup_cuda_target(${PROJECT_NAME}_cutest)
endif()