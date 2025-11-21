#include "tools/aodsolve_main_analyzer.h"
#include "aod/optimization_rule_system.h"
#include "aod/simd_instruction_rules.h"
#include "aod/function_inline_rules.h"
#include <iostream>

namespace aodsolve {

std::string generateFuncSignature(const clang::FunctionDecl* func, const std::string& suffix) {
    std::string sig = "void " + func->getNameAsString() + "_" + suffix + "(";
    bool first = true;
    for (auto param : func->parameters()) {
        if (!first) sig += ", ";
        std::string type = param->getType().getAsString();
        // 类型映射
        if (suffix == "SVE" && type.find("__m256i") != std::string::npos) type = "int8_t*"; // 简化假设
        // Scalar -> NEON 中，float* 保持 float* (vld1 接受 float*)

        sig += type + " " + param->getNameAsString();
        first = false;
    }
    sig += ") {\n";
    if (suffix == "SVE") sig += "    svbool_t pg = svptrue_b8();\n";
    return sig;
}

AODSolveMainAnalyzer::AODSolveMainAnalyzer(clang::ASTContext& ctx)
    : ast_context(ctx), source_manager(ctx.getSourceManager()) {
    target_architecture = "SVE";
    initializeComponents();
}

void AODSolveMainAnalyzer::initializeComponents() {
    cpg_analyzer = std::make_unique<IntegratedCPGAnalyzer>(ast_context);
    converter = std::make_unique<EnhancedCPGToAODConverter>(ast_context, *cpg_analyzer);
    code_generator = std::make_unique<EnhancedCodeGenerator>(ast_context);

    static RuleDatabase global_rule_db;
    static bool rules_loaded = false;
    if (!rules_loaded) {
        SIMDInstructionRuleBuilder simd_builder(&global_rule_db);
        simd_builder.buildAllRules();
        rules_loaded = true;
    }
    code_generator->setRuleDatabase(&global_rule_db);
}

ComprehensiveAnalysisResult AODSolveMainAnalyzer::analyzeFunction(const clang::FunctionDecl* func) {
    ComprehensiveAnalysisResult result;
    if (!source_manager.isInMainFile(func->getLocation())) return result;

    std::cout << "\n=== AODSOLVE Analysis: " << func->getNameAsString() << " ===" << std::endl;

    try {
        cpg_analyzer->analyzeFunctionWithCPG(func);

        auto conversion_res = converter->convertWithOperators(func, "AVX2", target_architecture);

        code_generator->setTargetArchitecture(target_architecture);
        auto gen_res = code_generator->generateCodeFromGraph(conversion_res.aod_graph);

        std::cout << "\n// Generated " << target_architecture << " Code:\n";
        std::cout << generateFuncSignature(func, target_architecture);
        std::cout << gen_res.generated_code;
        std::cout << "}\n";

        result.successful = true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        result.successful = false;
    }
    return result;
}

// Empty Stubs
ComprehensiveAnalysisResult AODSolveMainAnalyzer::analyzeTranslationUnit() { return {}; }
ComprehensiveAnalysisResult AODSolveMainAnalyzer::analyzeFile(const std::string&) { return {}; }
std::string AODSolveMainAnalyzer::generateComprehensiveReport(const ComprehensiveAnalysisResult&) { return ""; }
std::string AODSolveMainAnalyzer::generatePerformanceReport(const ComprehensiveAnalysisResult&) { return ""; }

} // namespace aodsolve
