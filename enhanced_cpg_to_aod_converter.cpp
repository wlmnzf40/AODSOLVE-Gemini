#include "conversion/enhanced_cpg_to_aod_converter.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Expr.h>

namespace aodsolve {

EnhancedCPGToAODConverter::EnhancedCPGToAODConverter(clang::ASTContext& ctx, IntegratedCPGAnalyzer& a)
    : source_manager(ctx.getSourceManager()), analyzer(&a) {
    (void)ctx;
}

ConversionResult EnhancedCPGToAODConverter::convertWithOperators(
    const clang::FunctionDecl* func,
    const std::string&, const std::string& target_arch) {

    ConversionResult result;
    result.aod_graph = std::make_shared<AODGraph>(func->getNameAsString());
    stmt_to_node_map.clear();

    // 简单的上下文标记：是否在向量化模式
    bool enable_autovec = (target_arch == "NEON"); // 简单开关

    try {
        buildFullAODGraph(func, *result.aod_graph);

        // 对图进行简单的向量化标记 (Demo用途)
        if (enable_autovec) {
            for (auto& node : result.aod_graph->getNodes()) {
                if (node->getType() == AODNodeType::Control && node->getName().find("ForStmt") != std::string::npos) {
                    node->setProperty("vectorize", "true");
                }
                // 如果是在 Loop 内的算术操作，标记为需要向量化
                // (这里简化为所有算术操作，实际需要 Loop Analysis)
                if (node->getType() == AODNodeType::GenericStmt && node->getName().find("BinaryOperator") != std::string::npos) {
                    node->setProperty("vectorize", "true");
                }
            }
        }

        connectDataFlow(func, *result.aod_graph);
        result.successful = true;
        result.converted_node_count = result.aod_graph->getNodeCount();
    } catch (const std::exception& e) {
        result.successful = false;
        result.error_message = std::string("Conversion Error: ") + e.what();
    }

    return result;
}

void EnhancedCPGToAODConverter::buildFullAODGraph(const clang::FunctionDecl* func, AODGraph& graph) {
    if (!func || !func->hasBody()) return;
    traverseAndBuild(func->getBody(), graph, true);
}

void EnhancedCPGToAODConverter::traverseExpressionTree(const clang::Stmt* stmt, AODGraph& graph) {
    if (!stmt) return;

    const clang::Expr* expr = llvm::dyn_cast<clang::Expr>(stmt);
    const clang::Expr* expr_clean = expr ? expr->IgnoreParenCasts() : nullptr;

    bool is_simd = isSIMDIntrinsic(stmt);
    // Case 4/5: 处理标量算术表达式
    bool is_scalar_op = false;
    if (auto* bo = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
        if (bo->getType()->isFloatingType()) is_scalar_op = true;
    }

    if (is_simd || is_scalar_op) {
        std::shared_ptr<AODNode> node = nullptr;
        if (is_simd) {
            node = createSIMDNode(stmt);
        } else if (is_scalar_op) {
            auto bo = llvm::cast<clang::BinaryOperator>(stmt);
            node = std::make_shared<AODNode>(AODNodeType::GenericStmt, "ScalarOp");
            node->setProperty("op_name", bo->getOpcodeStr().str());
            node->setAstStmt(stmt);
            node->setIsStatement(false);
        }

        if (node) {
            graph.addNode(node);
            // 映射所有相关的 Expr 指针
            if (expr) stmt_to_node_map[expr] = node;
            if (expr_clean) stmt_to_node_map[expr_clean] = node;
            stmt_to_node_map[stmt] = node;
        }
    }

    for (const auto* child : stmt->children()) {
        if (child) traverseExpressionTree(child, graph);
    }
}

void EnhancedCPGToAODConverter::traverseAndBuild(const clang::Stmt* stmt, AODGraph& graph, bool is_top_level) {
    if (!stmt) return;

    bool is_simd = isSIMDIntrinsic(stmt);
    bool is_container = llvm::isa<clang::CompoundStmt>(stmt);
    bool is_control = llvm::isa<clang::IfStmt>(stmt) || llvm::isa<clang::WhileStmt>(stmt) || llvm::isa<clang::ForStmt>(stmt);

    std::shared_ptr<AODNode> node;

    if (is_container) {
        for (const auto* child : stmt->children()) {
            if (child) traverseAndBuild(child, graph, true);
        }
        auto end = std::make_shared<AODNode>(AODNodeType::BlockEnd, "}");
        graph.addNode(end);
        return;
    }

    if (is_control) {
        node = std::make_shared<AODNode>(AODNodeType::Control, stmt->getStmtClassName());
        node->setAstStmt(stmt);
        node->setIsStatement(is_top_level);
        graph.addNode(node);
        stmt_to_node_map[stmt] = node;

        if (auto* whileStmt = llvm::dyn_cast<clang::WhileStmt>(stmt)) {
            traverseExpressionTree(whileStmt->getCond(), graph);
            traverseAndBuild(whileStmt->getBody(), graph, true);
        } else if (auto* forStmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
            traverseExpressionTree(forStmt->getInit(), graph);
            traverseExpressionTree(forStmt->getCond(), graph);
            traverseAndBuild(forStmt->getBody(), graph, true);
        } else if (auto* ifStmt = llvm::dyn_cast<clang::IfStmt>(stmt)) {
            traverseExpressionTree(ifStmt->getCond(), graph);
            traverseAndBuild(ifStmt->getThen(), graph, true);
            if (ifStmt->getElse()) traverseAndBuild(ifStmt->getElse(), graph, true);
        }
        return;
    }

    if (auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
        if (declStmt->isSingleDecl()) {
            if (auto* var = llvm::dyn_cast<clang::VarDecl>(declStmt->getSingleDecl())) {
                const clang::Expr* init = var->getInit();

                // 即使是普通 Decl，我们也创建节点，以便 generateDefineNode 处理
                node = std::make_shared<AODNode>(AODNodeType::GenericStmt, "DeclStmt");
                // 标记为 define 算子，生成器会特殊处理
                node->setProperty("op_name", "define");
                node->setProperty("var_name", var->getNameAsString());
                node->setAstStmt(stmt);
                node->setIsStatement(is_top_level);

                if (init && isSIMDIntrinsic(init->IgnoreParenCasts())) {
                    // 标记为 SIMD 相关的定义
                    node = createSIMDNode(stmt);
                }

                graph.addNode(node);
                stmt_to_node_map[stmt] = node;

                if (init) {
                    traverseExpressionTree(init, graph);
                }
                return;
            }
        }
    }

    if (is_simd) {
        node = createSIMDNode(stmt);
        node->setIsStatement(is_top_level);
    } else {
        node = std::make_shared<AODNode>(AODNodeType::GenericStmt, stmt->getStmtClassName());
        node->setAstStmt(stmt);
        node->setIsStatement(is_top_level);
    }
    graph.addNode(node);
    stmt_to_node_map[stmt] = node;

    for (const auto* child : stmt->children()) {
        if (child) traverseExpressionTree(child, graph);
    }
}

bool EnhancedCPGToAODConverter::isSIMDIntrinsic(const clang::Stmt* stmt) {
    if (!stmt) return false;
    const clang::Expr* expr = llvm::dyn_cast<clang::Expr>(stmt);
    if (!expr) return false;
    expr = expr->IgnoreParenCasts();

    if (auto* call = llvm::dyn_cast<clang::CallExpr>(expr)) {
        if (auto* func = call->getDirectCallee()) {
            return func->getNameAsString().find("_mm") != std::string::npos;
        }
    }
    return false;
}

std::shared_ptr<AODNode> EnhancedCPGToAODConverter::createSIMDNode(const clang::Stmt* stmt) {
    auto node = std::make_shared<AODNode>(AODNodeType::SIMD_Intrinsic, "SIMD_Op");
    node->setAstStmt(stmt);

    const clang::Expr* expr = llvm::dyn_cast<clang::Expr>(stmt);
    if (expr) expr = expr->IgnoreParenCasts();

    if (expr) {
        if (auto* call = llvm::dyn_cast<clang::CallExpr>(expr)) {
            if (auto* func = call->getDirectCallee()) {
                node->setProperty("op_name", func->getNameAsString());
                node->setIsStatement(false);
            }
        }
    } else if (auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
        if (auto* var = llvm::dyn_cast<clang::VarDecl>(declStmt->getSingleDecl())) {
            node->setProperty("op_name", "define");
            node->setProperty("var_name", var->getNameAsString());
            node->setIsStatement(true);
        }
    }
    return node;
}

std::shared_ptr<AODNode> EnhancedCPGToAODConverter::createAODNodeFromStmt(const clang::Stmt* stmt, bool is_stmt) {
    std::string name = stmt->getStmtClassName();
    auto node = std::make_shared<AODNode>(AODNodeType::Unknown, name);
    node->setAstStmt(stmt);
    node->setIsStatement(is_stmt);
    return node;
}

AODNodeType EnhancedCPGToAODConverter::mapStmtToNodeType(const clang::Stmt* stmt) {
    if (isSIMDIntrinsic(stmt)) return AODNodeType::SIMD_Intrinsic;
    if (llvm::isa<clang::CompoundStmt>(stmt)) return AODNodeType::Control;
    return AODNodeType::GenericStmt;
}

void EnhancedCPGToAODConverter::connectDataFlow(const clang::FunctionDecl*, AODGraph& graph) {
    cpg::CPGContext& cpg_ctx = const_cast<cpg::CPGContext&>(analyzer->getCPGContext());

    for (auto& node : graph.getNodes()) {
        const clang::Stmt* stmt = node->getAstStmt();
        if (!stmt) continue;

        // 1. DeclStmt -> Init
        if (node->getProperty("op_name") == "define") {
            if (auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
                if (auto* var = llvm::dyn_cast<clang::VarDecl>(declStmt->getSingleDecl())) {
                    if (auto* init = var->getInit()) {
                        auto init_clean = init->IgnoreParenCasts();
                        if (stmt_to_node_map.count(init_clean)) {
                            auto src = stmt_to_node_map[init_clean];
                            if (src != node) {
                                try { graph.addEdge(src, node, AODEdgeType::Data, "init"); } catch(...) {}
                            }
                        }
                    }
                }
            }
        }

        // 2. CallExpr/BinaryOp -> Children (Operands)
        const clang::Expr* expr = llvm::dyn_cast<clang::Expr>(stmt);
        const clang::Expr* expr_clean = expr ? expr->IgnoreParenCasts() : nullptr;

        if (!expr_clean) continue; // Skip non-expr statements here

        // Handle CallExpr args
        if (auto* call = llvm::dyn_cast<clang::CallExpr>(expr_clean)) {
            int arg_idx = 0;
            for (const auto* arg : call->arguments()) {
                auto arg_clean = arg->IgnoreParenCasts();

                if (stmt_to_node_map.count(arg_clean)) {
                    auto src = stmt_to_node_map[arg_clean];
                    try { graph.addEdge(src, node, AODEdgeType::Data, "arg_" + std::to_string(arg_idx)); } catch(...) {}
                } else if (auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(arg_clean)) {
                    // Find definition
                    std::string var_name = dre->getDecl()->getNameAsString();
                    for (auto& potential_src : graph.getNodes()) {
                        if (potential_src->getProperty("op_name") == "define" &&
                            potential_src->getProperty("var_name") == var_name) {
                            try { graph.addEdge(potential_src, node, AODEdgeType::Data, "arg_" + std::to_string(arg_idx)); } catch(...) {}
                            break;
                        }
                    }
                }
                arg_idx++;
            }
        }
        // Handle BinaryOperator operands (for Scalar Vectorization)
        else if (auto* bo = llvm::dyn_cast<clang::BinaryOperator>(expr_clean)) {
            clang::Expr* lhs = bo->getLHS()->IgnoreParenCasts();
            clang::Expr* rhs = bo->getRHS()->IgnoreParenCasts();

            auto linkOperand = [&](clang::Expr* op, int idx) {
                if (stmt_to_node_map.count(op)) {
                    try { graph.addEdge(stmt_to_node_map[op], node, AODEdgeType::Data, "arg_" + std::to_string(idx)); } catch(...) {}
                } else if (auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(op)) {
                     std::string var_name = dre->getDecl()->getNameAsString();
                     for (auto& src : graph.getNodes()) {
                         if (src->getProperty("op_name") == "define" && src->getProperty("var_name") == var_name) {
                             try { graph.addEdge(src, node, AODEdgeType::Data, "arg_" + std::to_string(idx)); } catch(...) {}
                             break;
                         }
                     }
                }
            };

            linkOperand(lhs, 0);
            linkOperand(rhs, 1);
        }

        // 3. CPG Data Deps
        auto deps = cpg_ctx.getDataDependencies(stmt);
        for (const auto& dep : deps) {
            if (stmt_to_node_map.count(dep.sourceStmt)) {
                auto source_node = stmt_to_node_map[dep.sourceStmt];
                if (source_node == node || source_node->getId() == node->getId()) continue;
                try {
                    graph.addEdge(source_node, node, AODEdgeType::Data, dep.varName);
                } catch (...) {}
            }
        }
    }
}

} // namespace aodsolve
