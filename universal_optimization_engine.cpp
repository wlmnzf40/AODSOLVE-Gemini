#include "aod/optimization_rule_system.h"
#include "analysis/loop_vectorization_analyzer.h"
#include "analysis/integrated_cpg_analyzer.h"
#include "aod/function_inline_rules.h"
#include "aod/simd_instruction_rules.h"

#include "tools/aodsolve_main_analyzer.h"
#include "conversion/enhanced_cpg_to_aod_converter.h"

#include <clang/AST/RecursiveASTVisitor.h>
#include <sstream>
#include <algorithm>

namespace aodsolve {

class UniversalOptimizationEngine {
public:
    UniversalOptimizationEngine() : pipeline(&rule_db) { initializeRules(); }
    std::string optimize(void* cpg_graph, void* aod_graph, const std::string& target_arch);
    std::string optimizeWithCategories(void* cpg_graph, void* aod_graph, const std::string& target_arch, const std::vector<std::string>& categories);

private:
    RuleDatabase rule_db;
    OptimizationPipeline pipeline;
    void initializeRules();
};

void UniversalOptimizationEngine::initializeRules() {
    // Rule initialization logic...
}

std::string UniversalOptimizationEngine::optimize(void* cpg_graph, void* aod_graph, const std::string& target_arch) {
    return optimizeWithCategories(cpg_graph, aod_graph, target_arch, {"all"});
}

std::string UniversalOptimizationEngine::optimizeWithCategories(void* cpg_graph, void* , const std::string& target_arch, const std::vector<std::string>& categories) {
    std::stringstream optimized_code;
    std::string result = pipeline.runOptimization(cpg_graph, target_arch, categories);
    optimized_code << result;
    return optimized_code.str();
}

// ============================================================================
// CPGBasedVectorizationOptimizer
// ============================================================================

class CPGBasedVectorizationOptimizer {
public:
    // 修复：移除了未使用的 ast_context 参数名，消除警告
    CPGBasedVectorizationOptimizer(clang::ASTContext* , IntegratedCPGAnalyzer* cpg_analyzer)
        : cpg_analyzer_(cpg_analyzer) {}

    std::string optimizeFunction(const clang::FunctionDecl* func, const std::string& target_arch);
    void* buildCPGForFunction(const clang::FunctionDecl* func);
    void* buildAODForFunction(const clang::FunctionDecl* func);

private:
    IntegratedCPGAnalyzer* cpg_analyzer_;
    // 修复：移除了未使用的 ast_context_ 成员变量
};

std::string CPGBasedVectorizationOptimizer::optimizeFunction(const clang::FunctionDecl* func, const std::string& target_arch) {
    if (!func || !func->hasBody()) return "// Error: Invalid function\n";
    std::stringstream code;
    code << "// Optimized version for " << target_arch << "\n";
    void* cpg = buildCPGForFunction(func);
    (void)cpg;
    void* aod = buildAODForFunction(func);
    (void)aod;

    UniversalOptimizationEngine engine;
    return code.str();
}

void* CPGBasedVectorizationOptimizer::buildCPGForFunction(const clang::FunctionDecl*) {
    if (cpg_analyzer_) return const_cast<cpg::CPGContext*>(&cpg_analyzer_->getCPGContext());
    return nullptr;
}

void* CPGBasedVectorizationOptimizer::buildAODForFunction(const clang::FunctionDecl*) {
    return nullptr;
}

} // namespace aodsolve
