#include "tools/aodsolve_main_analyzer.h"
#include "conversion/enhanced_cpg_to_aod_converter.h"
#include "generation/enhanced_code_generator.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

using namespace aodsolve;

class AODSolveDemo {
public:
    // 案例 1: AVX2 SIMD 代码转换为 SVE 代码
    void runStringProcessingDemo();

    // 案例 4: 标量循环自动向量化为 NEON 代码
    void runScalarLoopVectorizationDemo();

    // 案例 5: 跨函数标量向量化 (内联 + NEON)
    void runCrossFunctionVectorizationDemo();

private:
    void saveToFile(const std::string& content, const std::string& filename);
    void runClangAnalysis(const std::string& code, const std::string& filename, const std::string& target_arch);
};

// ========================================================
// 案例 1 实现: 字符串处理 (AVX2 -> SVE)
// ========================================================
void AODSolveDemo::runStringProcessingDemo() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "   Case 1: String Processing (AVX2 -> SVE)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::string case1_code = R"(
#include <immintrin.h>
#include <stdint.h>
#include <stddef.h>

void lower_case_avx2(uint8_t* dst, const uint8_t* src, size_t len) {
#if defined(__AVX2__)
    const __m256i _A = _mm256_set1_epi8('A' - 1);
    const __m256i Z_ = _mm256_set1_epi8('Z' + 1);
    const __m256i delta = _mm256_set1_epi8('a' - 'A');
    uint8_t* q = dst;

    while (len >= 32) {
        __m256i op = _mm256_loadu_si256((__m256i*)src);
        __m256i gt = _mm256_cmpgt_epi8(op, _A);
        __m256i lt = _mm256_cmpgt_epi8(Z_, op);
        __m256i mingle = _mm256_and_si256(gt, lt);
        __m256i add = _mm256_and_si256(mingle, delta);
        __m256i lower = _mm256_add_epi8(op, add);
        _mm256_storeu_si256((__m256i *)q, lower);
        src += 32;
        q += 32;
        len -= 32;
    }
#endif
}
)";
    runClangAnalysis(case1_code, "case1_string.cpp", "SVE");
}

// ========================================================
// 案例 4 实现: 标量循环向量化 (Scalar -> NEON)
// ========================================================
void AODSolveDemo::runScalarLoopVectorizationDemo() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "   Case 4: Scalar Loop Vectorization (Scalar -> NEON)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::string case4_code = R"(
#include <stddef.h>

void Test(float volatile* xNorms, int i, float volatile* yNorms,
          float volatile* ipLine, size_t ny) {
    for (size_t j = 0; j < ny; j++) {
        float ip = *ipLine;
        float dis = xNorms[i] + yNorms[j] - 2 * ip;
        if (dis < 0) {
            dis = 0;
        }
        *ipLine = dis;
        ipLine++;
    }
}
)";
    runClangAnalysis(case4_code, "case4_scalar.cpp", "NEON");
}

// ========================================================
// 案例 5 实现: 跨函数向量化 (Scalar -> NEON)
// ========================================================
void AODSolveDemo::runCrossFunctionVectorizationDemo() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "   Case 5: Cross-Function Vectorization (Scalar -> NEON)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::string case5_code = R"(
#include <stddef.h>

// 被调用的辅助函数
float cal_call(float volatile* xNorms, int i, int j,
               float volatile* yNorms, float ip) {
    return xNorms[i] + yNorms[j] - 2 * ip;
}

// 主循环函数 - 包含函数调用
void Test_call(float volatile* xNorms, int i, float volatile* yNorms,
               float volatile* ipLine, size_t ny) {
    for (size_t j = 0; j < ny; j++) {
        float ip = *ipLine;
        float dis = cal_call(xNorms, i, j, yNorms, ip);
        if (dis < 0) {
            dis = 0;
        }
        *ipLine = dis;
        ipLine++;
    }
}
)";
    runClangAnalysis(case5_code, "case5_cross_func.cpp", "NEON");
}

// ========================================================
// 核心分析执行逻辑
// ========================================================
void AODSolveDemo::runClangAnalysis(const std::string& code, const std::string& filename, const std::string& target_arch) {
    try {
        std::string temp_file = "/tmp/" + filename;
        saveToFile(code, temp_file);

        // 准备编译参数，模拟 AVX2 环境以支持 SIMD 内置函数解析
        std::vector<std::string> args = {
            "-xc++",
            "--target=x86_64-pc-linux-gnu",
            "-mavx2",
            "-D__AVX2__",
            "-std=c++17",
            "-I/usr/include",
            "-I/usr/local/include",
            "-I/usr/lib/gcc/x86_64-linux-gnu/9/include" // 可能需要根据环境调整
        };

        // 构建 AST
        auto owner = clang::tooling::buildASTFromCodeWithArgs(code, args, temp_file);
        if (!owner) {
            std::cerr << "Error: Failed to build AST for " << filename << std::endl;
            return;
        }

        auto& ast_context = owner->getASTContext();
        auto& source_manager = ast_context.getSourceManager();

        std::cout << "Analysis target file: " << filename << std::endl;

        // 查找主文件中的函数定义
        std::vector<const clang::FunctionDecl*> targetFuncs;
        auto* tu = ast_context.getTranslationUnitDecl();

        for (auto* decl : tu->decls()) {
            if (auto* func = clang::dyn_cast<clang::FunctionDecl>(decl)) {
                if (source_manager.isInMainFile(func->getLocation()) && func->hasBody()) {
                    targetFuncs.push_back(func);
                }
            }
        }

        if (targetFuncs.empty()) {
            std::cout << "Warning: No function definition found in main file." << std::endl;
            return;
        }

        // 创建分析器并执行
        AODSolveMainAnalyzer analyzer(ast_context);
        analyzer.setTargetArchitecture(target_arch);

        // 分析找到的最后一个函数（通常是主入口或外层函数）
        // 对于 Case 5，我们需要分析 Test_call，它会触发对 cal_call 的跨过程分析
        const clang::FunctionDecl* mainFunc = targetFuncs.back();

        // 如果有多个函数，优先找特定的入口名
        for (auto* f : targetFuncs) {
            std::string name = f->getNameAsString();
            if (name == "Test_call" || name == "lower_case_avx2" || name == "Test") {
                mainFunc = f;
                break;
            }
        }

        analyzer.analyzeFunction(mainFunc);

    } catch (const std::exception& e) {
        std::cerr << "Exception during analysis: " << e.what() << std::endl;
    }
}

void AODSolveDemo::saveToFile(const std::string& content, const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
    } else {
        std::cerr << "Error: Could not save to file " << filename << std::endl;
    }
}

// ========================================================
// 主函数
// ========================================================
int main(int argc, char* argv[]) {
    std::cout << "=== AODSOLVE Vectorization Optimization Demo ===" << std::endl;
    std::cout << "Demonstrating Rule-Based SIMD Conversion and Scalar Vectorization\n" << std::endl;

    AODSolveDemo demo;

    if (argc > 1) {
        std::string command = argv[1];
        if (command == "case1" || command == "string") {
            demo.runStringProcessingDemo();
        } else if (command == "case4" || command == "scalar") {
            demo.runScalarLoopVectorizationDemo();
        } else if (command == "case5" || command == "crossfunc") {
            demo.runCrossFunctionVectorizationDemo();
        } else if (command == "all") {
            demo.runStringProcessingDemo();
            demo.runScalarLoopVectorizationDemo();
            demo.runCrossFunctionVectorizationDemo();
        } else {
            std::cout << "Unknown command. Usage: ./vectorization_demo [case1|case4|case5|all]" << std::endl;
        }
    } else {
        // 默认运行所有案例
        demo.runStringProcessingDemo();
        demo.runScalarLoopVectorizationDemo();
        demo.runCrossFunctionVectorizationDemo();
    }

    std::cout << "\nDemo completed successfully." << std::endl;
    return 0;
}
