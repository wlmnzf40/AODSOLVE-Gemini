#include "generation/enhanced_code_generator.h"
#include <sstream>
#include <iostream>
#include <clang/AST/Stmt.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Decl.h>

namespace aodsolve {

EnhancedCodeGenerator::EnhancedCodeGenerator(clang::ASTContext& ctx)
    : ast_context(ctx), target_architecture("SVE") {}

bool needsSemicolon(const clang::Stmt* stmt) {
    if (llvm::isa<clang::CompoundStmt>(stmt)) return false;
    if (llvm::isa<clang::IfStmt>(stmt)) return false;
    if (llvm::isa<clang::WhileStmt>(stmt)) return false;
    if (llvm::isa<clang::ForStmt>(stmt)) return false;
    if (llvm::isa<clang::DeclStmt>(stmt)) return true;
    return true;
}

CodeGenerationResult EnhancedCodeGenerator::generateCodeFromGraph(const AODGraphPtr& graph) {
    CodeGenerationResult result;
    std::stringstream code;

    for (const auto& node : graph->getNodes()) {
        // Block End
        if (node->getType() == AODNodeType::BlockEnd) {
            code << "    }\n";
            continue;
        }

        if (!node->isStatement()) continue;

        std::string line;

        // 1. 变量定义 (DeclStmt) - 包含 SIMD 或 普通定义
        if (node->getProperty("op_name") == "define") {
            line = generateDefineNode(node, graph);
        }
        // 2. 控制流头部
        else if (node->getType() == AODNodeType::Control) {
            // 这里我们只打印头部，不打印 Body
            if (auto* whileStmt = llvm::dyn_cast<clang::WhileStmt>(node->getAstStmt())) {
                std::string cond = generateFallbackCode(whileStmt->getCond());
                line = "while (" + cond + ") {";
            } else if (auto* forStmt = llvm::dyn_cast<clang::ForStmt>(node->getAstStmt())) {
                // 简单处理 For Loop Header
                std::string init = generateFallbackCode(forStmt->getInit());
                std::string cond = generateFallbackCode(forStmt->getCond());
                std::string inc = generateFallbackCode(forStmt->getInc());

                // 如果标记了 NEON 向量化
                if (target_architecture == "NEON" && node->getProperty("vectorize") == "true") {
                    line = "// Vector Loop (NEON)\n    for (" + init + " " + cond + "; " + inc + ") {";
                    size_t pos = line.find("++");
                    if (pos != std::string::npos) line.replace(pos, 2, " += 4");
                } else {
                    line = "for (" + init + " " + cond + "; " + inc + ") {";
                }
            } else if (auto* ifStmt = llvm::dyn_cast<clang::IfStmt>(node->getAstStmt())) {
                std::string cond = generateFallbackCode(ifStmt->getCond());
                line = "if (" + cond + ") {";
            }
        }
        // 3. 独立语句 (SIMD Call 或 Generic)
        else {
            // 先尝试应用规则 (可能是 SIMD Store 或 Scalar Calc)
            std::string rule_code = tryApplyRules(node, graph);

            // 如果规则应用成功，且不是空
            if (!rule_code.empty() && rule_code.find("Unknown") == std::string::npos) {
                line = rule_code;
            } else {
                // Fallback 到 AST 打印
                line = generateFallbackCode(node->getAstStmt());
            }
        }

        if (!line.empty()) {
            code << "    " << line;
            if (needsSemicolon(node->getAstStmt()) && line.back() != ';' && line.back() != '{' && line.back() != '}') {
                code << ";";
            }
            code << "\n";
        }
    }

    result.generated_code = code.str();
    result.successful = true;
    return result;
}

std::string EnhancedCodeGenerator::generateDefineNode(const std::shared_ptr<AODNode>& node, const AODGraphPtr& graph) {
    std::string var_name = node->getProperty("var_name");
    std::string rhs_code;
    std::string type = "auto";
    bool is_const = false;

    if (auto* declStmt = llvm::dyn_cast_or_null<clang::DeclStmt>(node->getAstStmt())) {
        if (auto* var = llvm::dyn_cast<clang::VarDecl>(declStmt->getSingleDecl())) {
            if (var->getType().isConstQualified()) is_const = true;
            type = var->getType().getAsString(); // Default type
        }
    }

    // 从 Init 边获取 RHS
    auto edges = graph->getIncomingEdges(node->getId());
    std::shared_ptr<AODNode> init_src = nullptr;
    for (auto& edge : edges) {
        if (edge->getProperties().variable_name == "init") {
            init_src = edge->getSource();
            break;
        }
    }

    if (init_src) {
        rhs_code = tryApplyRules(init_src, graph);

        // 类型推断逻辑
        if (rule_db) {
            std::string op = init_src->getProperty("op_name");
            auto rules = rule_db->queryRules("simd_instruction");
            // 尝试查询 scalar 规则，以防万一
            auto scalar_rules = rule_db->queryRules("scalar_vectorization");
            rules.insert(rules.end(), scalar_rules.begin(), scalar_rules.end());

            for (auto* r : rules) {
                for (auto& req : r->source_pattern.required_operations) {
                    if (req == op) {
                        if (r->target_templates.count(target_architecture)) {
                            auto& tmpl = r->target_templates.at(target_architecture);
                            if (tmpl.performance_hints.count("return_type")) {
                                type = tmpl.performance_hints.at("return_type");
                            }
                        }
                        goto found_type;
                    }
                }
            }
        }
        found_type:;

        // Heuristics (如果在规则中未找到)
        if (type == "auto" || type.find("__m256") != std::string::npos) {
            if (target_architecture == "SVE") {
                if (rhs_code.find("svbool") != std::string::npos || rhs_code.find("svptrue") != std::string::npos || rhs_code.find("svcmp") != std::string::npos)
                    type = "svbool_t";
                else
                    type = "svint8_t"; // Default SVE type
            } else if (target_architecture == "NEON") {
                if (rhs_code.find("vaddq") != std::string::npos) type = "float32x4_t";
            }
        }

    } else {
        // Fallback to original init expr
        if (auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(node->getAstStmt())) {
            if (auto* var = llvm::dyn_cast<clang::VarDecl>(declStmt->getSingleDecl())) {
                if (var->getInit()) rhs_code = generateFallbackCode(var->getInit());
            }
        }
    }

    if (rhs_code.empty()) return ""; // No definition body

    // 修复：仅当 type 中不包含 const 时才添加 const，避免 double const
    if (is_const && type.find("const") == std::string::npos) type = "const " + type;
    return type + " " + var_name + " = " + rhs_code;
}

std::string EnhancedCodeGenerator::tryApplyRules(const std::shared_ptr<AODNode>& node, const AODGraphPtr& graph) {
    if (!rule_db) return generateFallbackCode(node->getAstStmt());

    std::string op_name = node->getProperty("op_name");
    // 如果没有 op_name，说明不是识别出的算子，回退
    if (op_name.empty()) return generateFallbackCode(node->getAstStmt());

    // 查找规则
    std::vector<OptimizationRule*> rules;
    auto simd_rules = rule_db->queryRules("simd_instruction");
    auto scalar_rules = rule_db->queryRules("scalar_vectorization"); // 确保也能查到标量规则
    rules.insert(rules.end(), simd_rules.begin(), simd_rules.end());
    rules.insert(rules.end(), scalar_rules.begin(), scalar_rules.end());

    OptimizationRule* matched_rule = nullptr;
    for (auto* rule : rules) {
        for (const auto& req : rule->source_pattern.required_operations) {
            if (req == op_name) { matched_rule = rule; break; }
        }
        if (matched_rule) break;
    }

    // 无规则 -> 回退
    if (!matched_rule) return generateFallbackCode(node->getAstStmt());

    if (matched_rule->target_templates.find(target_architecture) == matched_rule->target_templates.end()) {
        return generateFallbackCode(node->getAstStmt());
    }

    const auto& tmpl = matched_rule->target_templates.at(target_architecture);
    std::string code = tmpl.code_template;
    std::map<std::string, std::string> bindings;

    // 处理参数
    auto edges = graph->getIncomingEdges(node->getId());

    // 预填充 (AST Fallback for args) - 修复：先转为 Expr* 再调用 IgnoreParenCasts
    const clang::Expr* expr_ptr = llvm::dyn_cast_or_null<clang::Expr>(node->getAstStmt());
    if (expr_ptr) {
        // 注意：IgnoreParenCasts 可能会剥离 CallExpr，所以我们要在转换后检查 CallExpr
        if (auto* call = llvm::dyn_cast<clang::CallExpr>(expr_ptr->IgnoreParenCasts())) {
            for (unsigned i = 0; i < call->getNumArgs(); ++i) {
                std::string s = generateFallbackCode(call->getArg(i));
                // Clean cast
                size_t pos;
                while ((pos = s.find("(__m256i *)")) != std::string::npos) s.replace(pos, 11, "(int8_t *)");
                bindings["{{input_" + std::to_string(i) + "}}"] = s;
            }
        } else if (auto* bo = llvm::dyn_cast<clang::BinaryOperator>(expr_ptr->IgnoreParenCasts())) {
            bindings["{{input_0}}"] = generateFallbackCode(bo->getLHS());
            bindings["{{input_1}}"] = generateFallbackCode(bo->getRHS());
        }
    }

    // 图数据流覆盖
    for (auto& edge : edges) {
        std::string var_name = edge->getProperties().variable_name;
        if (var_name.find("arg_") == 0) {
            int idx = std::stoi(var_name.substr(4));
            auto src = edge->getSource();
            std::string val;

            if (src->isStatement()) {
                val = src->getProperty("var_name");
            } else {
                val = tryApplyRules(src, graph);
            }

            // SVE 类型适配 (Predicate -> Data)
            if (target_architecture == "SVE" && op_name.find("and") != std::string::npos) {
                std::string src_op = src->getProperty("op_name");
                if (src_op.find("cmp") != std::string::npos) {
                    val = "svsel_s8(" + val + ", svdup_s8(0xFF), svdup_s8(0x00))";
                }
            }

            bindings["{{input_" + std::to_string(idx) + "}}"] = val;
        }
    }

    if (target_architecture == "SVE") bindings["{{predicate}}"] = "pg";

    // 模板替换
    for (const auto& [k, v] : bindings) {
        std::string ph = k;
        size_t pos = 0;
        while ((pos = code.find(ph, pos)) != std::string::npos) {
            code.replace(pos, ph.length(), v);
            pos += v.length();
        }
    }

    return code;
}

std::string EnhancedCodeGenerator::generateFallbackCode(const clang::Stmt* stmt) {
    if (!stmt) return "";
    std::string code;
    llvm::raw_string_ostream os(code);
    stmt->printPretty(os, nullptr, ast_context.getPrintingPolicy());
    std::string s = os.str();
    // 清除末尾分号
    while (!s.empty() && (s.back() == ';' || s.back() == '\n' || s.back() == ' ')) {
        s.pop_back();
    }
    return s;
}

std::string EnhancedCodeGenerator::generateOutputVar(const std::shared_ptr<AODNode>& node) {
    return "vec_" + std::to_string(node->getId());
}

} // namespace aodsolve
