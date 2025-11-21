#pragma once

#include "analysis/integrated_cpg_analyzer.h"
#include "aod/enhanced_aod_node.h"
#include "aod/enhanced_aod_graph.h"
#include "aod/optimization_rule_system.h"

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <string>

namespace aodsolve {

// 跨过程数据流分析结果
struct InterproceduralDataFlow {
    std::map<const clang::CallExpr*, std::map<std::string, std::string>> argument_flows;
    std::map<const clang::CallExpr*, std::string> return_value_flows;
    std::map<const clang::FunctionDecl*, std::set<std::string>> side_effects;
    std::map<const clang::CallExpr*, std::vector<std::string>> affected_variables;
};

// 转换结果结构体
struct ConversionResult {
    bool successful = false;
    std::string error_message;
    std::vector<std::string> warnings;
    std::vector<std::string> info_messages;

    std::shared_ptr<AODGraph> aod_graph;
    std::map<const clang::Stmt*, int> stmt_to_node_map;

    int converted_node_count = 0;
    int data_flow_edges = 0;
};

class EnhancedCPGToAODConverter {
private:
    clang::SourceManager& source_manager;
    IntegratedCPGAnalyzer* analyzer;

    // 转换状态
    std::map<const clang::Stmt*, std::shared_ptr<AODNode>> stmt_to_node_map;

public:
    explicit EnhancedCPGToAODConverter(clang::ASTContext& ctx, IntegratedCPGAnalyzer& a);

    // 核心转换接口
    ConversionResult convertWithOperators(
        const clang::FunctionDecl* func,
        const std::string& source_arch,
        const std::string& target_arch);

    // 构建全量AOD图
    void buildFullAODGraph(const clang::FunctionDecl* func, AODGraph& graph);

    const IntegratedCPGAnalyzer& getAnalyzer() const { return *analyzer; }

private:
    // 递归遍历 AST
    // is_top_level: 标记当前遍历的节点是否应视为独立语句
    void traverseAndBuild(const clang::Stmt* stmt, AODGraph& graph, bool is_top_level = true);

    // 新增：遍历表达式树（用于构建非语句节点）
    void traverseExpressionTree(const clang::Stmt* expr, AODGraph& graph);

    // 节点创建辅助
    std::shared_ptr<AODNode> createAODNodeFromStmt(const clang::Stmt* stmt, bool is_stmt);

    // SIMD 辅助函数声明
    std::shared_ptr<AODNode> createSIMDNode(const clang::Stmt* stmt);
    bool isSIMDIntrinsic(const clang::Stmt* stmt);

    // 连接数据流
    void connectDataFlow(const clang::FunctionDecl* func, AODGraph& graph);

    // AST 类型映射
    AODNodeType mapStmtToNodeType(const clang::Stmt* stmt);
};

} // namespace aodsolve
