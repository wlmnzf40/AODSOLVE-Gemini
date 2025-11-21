#pragma once
#include "aod/enhanced_aod_graph.h"
#include "aod/optimization_rule_system.h"
#include "clang/AST/ASTContext.h"

#include <memory>
#include <map>
#include <vector>
#include <string>

namespace aodsolve {

    struct CodeGenerationResult {
        bool successful = false;
        std::string generated_code;
        double estimated_speedup = 0.0;
        int simd_intrinsics = 0;
        std::vector<std::string> info_messages;
        std::string target_architecture;
    };

    class EnhancedCodeGenerator {
    private:
        clang::ASTContext& ast_context;
        std::string target_architecture;
        RuleDatabase* rule_db = nullptr;

    public:
        explicit EnhancedCodeGenerator(clang::ASTContext& ctx);
        ~EnhancedCodeGenerator() = default;

        void setTargetArchitecture(const std::string& arch) { target_architecture = arch; }
        void setRuleDatabase(RuleDatabase* db) { rule_db = db; }

        CodeGenerationResult generateCodeFromGraph(const AODGraphPtr& graph);

        // 兼容旧接口
        std::string generateLoopFromTemplate(const std::map<std::string, std::string>&, const std::string&) { return ""; }

    private:
        std::string tryApplyRules(const std::shared_ptr<AODNode>& node, const AODGraphPtr& graph);
        std::string generateFallbackCode(const clang::Stmt* stmt);
        std::string generateOutputVar(const std::shared_ptr<AODNode>& node);

        // 新增声明: 修复编译错误
        std::string generateDefineNode(const std::shared_ptr<AODNode>& node, const AODGraphPtr& graph);
    };

} // namespace aodsolve
