cmake_minimum_required(VERSION 3.16)
project(AODSOLVE VERSION 1.0.0 LANGUAGES CXX)

# C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译器特定选项
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-O2 -g)
    add_compile_options(-fno-rtti)
endif()

# ------------------------------------------------------------------------------
# LLVM / Clang
# ------------------------------------------------------------------------------
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

# 自动获取 LLVM 库
llvm_map_components_to_libnames(LLVM_LIBRARIES
        core
        support
        option
)

# Clang 库
set(CLANG_LIBRARIES
        clangTooling
        clangFrontend
        clangFrontendTool
        clangSerialization
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangAST
        clangASTMatchers
        clangBasic
        clangLex
)

# ------------------------------------------------------------------------------
# 包含目录
# ------------------------------------------------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/include)

# ------------------------------------------------------------------------------
# AOD 核心库
# ------------------------------------------------------------------------------
set(AOD_SOURCES
        src/aod/core/enhanced_aod_node.cpp
        src/aod/core/enhanced_aod_graph.cpp
)
add_library(aod_core STATIC ${AOD_SOURCES})

# ------------------------------------------------------------------------------
# AST 分析
# ------------------------------------------------------------------------------
set(ANALYSIS_SOURCES
        src/analysis/enhanced_ast_analyzer.cpp
        src/analysis/integrated_cpg_analyzer.cpp
        src/analysis/function_inline_analyzer.cpp
        src/analysis/loop_vectorization_analyzer.cpp
)
add_library(aod_analysis STATIC ${ANALYSIS_SOURCES})
target_link_libraries(aod_analysis aod_core)

# ------------------------------------------------------------------------------
# CPG 框架
# ------------------------------------------------------------------------------
set(CPG_SOURCES
        src/analysis/CPGAnnotation.cpp
)
add_library(aod_cpg STATIC ${CPG_SOURCES})
target_link_libraries(aod_cpg aod_core)

# ------------------------------------------------------------------------------
# 转换器
# ------------------------------------------------------------------------------
set(CONVERTER_SOURCES
        src/conversion/enhanced_cpg_to_aod_converter.cpp
        src/conversion/universal_optimization_engine.cpp
        src/conversion/optimization_rule_system.cpp
)
add_library(aod_converter STATIC ${CONVERTER_SOURCES})
target_link_libraries(aod_converter aod_core aod_analysis aod_cpg)

# ------------------------------------------------------------------------------
# 生成器
# ------------------------------------------------------------------------------
set(GENERATOR_SOURCES
        src/generation/enhanced_code_generator.cpp
)
add_library(aod_generator STATIC ${GENERATOR_SOURCES})
target_link_libraries(aod_generator
        aod_core      # 因为调用 AODGraph、AODNode
        aod_converter # 因为调用 RuleDatabase
)

# ------------------------------------------------------------------------------
# 公共逻辑库（包含 aodsolve_main_analyzer.cpp）
# ------------------------------------------------------------------------------
set(COMMON_SOURCES
        src/tools/aodsolve_main_analyzer.cpp
)
add_library(aodsolve_common STATIC ${COMMON_SOURCES})

target_link_libraries(aodsolve_common
        aod_generator   # 依赖者在前
        aod_converter
        aod_analysis
        aod_cpg
        aod_core        # 被依赖者在后
        ${LLVM_LIBRARIES}
        ${CLANG_LIBRARIES}
)

# ------------------------------------------------------------------------------
# 向量化 Demo
# ------------------------------------------------------------------------------
add_executable(vectorization_demo src/demo/enhanced_vectorization_demo.cpp)

target_link_libraries(vectorization_demo
        aodsolve_common
)

# ------------------------------------------------------------------------------
# 输出目录
# ------------------------------------------------------------------------------
set_target_properties(vectorization_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ------------------------------------------------------------------------------
# 安装
# ------------------------------------------------------------------------------
install(TARGETS vectorization_demo
        RUNTIME DESTINATION bin
)

install(TARGETS aod_core aod_analysis aod_cpg aod_converter aod_generator aodsolve_common
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
)

message(STATUS "AODSOLVE Build Configuration:")
message(STATUS "  LLVM Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "  C++ Standard: CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}")

add_custom_target(pre-build-clean
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning coverage data..."
        COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -type f -delete
        COMMENT "Cleaning old coverage data files"
)
add_dependencies(vectorization_demo pre-build-clean)
