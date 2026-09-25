project(Experiment_QDA)

add_subdirectory(QDA_Simple)
add_subdirectory(QDA_Poiseuille)
add_subdirectory(QDATest)

if (CUDA_FOUND)
	add_executable(${PROJECT_NAME}_cutest qda_cutest.cu)
	target_link_libraries(${PROJECT_NAME}_cutest PRIVATE SparQ_cuda)

	# 目标配置统一由子模块 cmake/CUDA.cmake 的 sparq_setup_cuda_target 提供。
	include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
	sparq_setup_cuda_target(${PROJECT_NAME}_cutest)
endif()