#pragma once

#include "optimization_rule_system.h"

namespace aodsolve {

class SIMDInstructionRuleBuilder {
    RuleDatabase* rule_db;
public:
    SIMDInstructionRuleBuilder(RuleDatabase* db) : rule_db(db) {}

    void buildAllRules() {
        // ==========================================
        // AVX2 -> SVE Rules
        // ==========================================
        {
            OptimizationRule rule;
            rule.rule_id = "avx2_set1_epi8";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"_mm256_set1_epi8"};

            TransformTemplate sve;
            sve.target_architecture = "SVE";
            sve.code_template = "svdup_s8({{input_0}})";
            sve.performance_hints["return_type"] = "svint8_t";
            rule.target_templates["SVE"] = sve;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "avx2_loadu_si256";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"_mm256_loadu_si256"};

            TransformTemplate sve;
            sve.target_architecture = "SVE";
            sve.code_template = "svld1_s8(pg, (const int8_t*){{input_0}})";
            sve.performance_hints["return_type"] = "svint8_t";
            rule.target_templates["SVE"] = sve;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "avx2_storeu_si256";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"_mm256_storeu_si256"};

            TransformTemplate sve;
            sve.target_architecture = "SVE";
            sve.code_template = "svst1_s8(pg, (int8_t*){{input_0}}, {{input_1}})";
            sve.performance_hints["return_type"] = "void";
            rule.target_templates["SVE"] = sve;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "avx2_cmpgt_epi8";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"_mm256_cmpgt_epi8"};

            TransformTemplate sve;
            sve.target_architecture = "SVE";
            sve.code_template = "svcmpgt_s8(pg, {{input_0}}, {{input_1}})";
            sve.performance_hints["return_type"] = "svbool_t";
            rule.target_templates["SVE"] = sve;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "avx2_and_si256";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"_mm256_and_si256"};

            TransformTemplate sve;
            sve.target_architecture = "SVE";
            sve.code_template = "svand_s8_z(pg, {{input_0}}, {{input_1}})";
            sve.performance_hints["return_type"] = "svint8_t";
            rule.target_templates["SVE"] = sve;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "avx2_add_epi8";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"_mm256_add_epi8"};

            TransformTemplate sve;
            sve.target_architecture = "SVE";
            sve.code_template = "svadd_s8_z(pg, {{input_0}}, {{input_1}})";
            sve.performance_hints["return_type"] = "svint8_t";
            rule.target_templates["SVE"] = sve;
            rule_db->addRule(rule);
        }

        // ==========================================
        // Scalar -> NEON Rules (Auto-Vectorization Ops)
        // ==========================================
        {
            OptimizationRule rule;
            rule.rule_id = "scalar_add_float";
            rule.category = "simd_instruction"; // 统一 Category 方便查找
            rule.source_pattern.required_operations = {"+"};

            TransformTemplate neon;
            neon.target_architecture = "NEON";
            neon.code_template = "vaddq_f32({{input_0}}, {{input_1}})";
            neon.performance_hints["return_type"] = "float32x4_t";
            rule.target_templates["NEON"] = neon;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "scalar_sub_float";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"-"};

            TransformTemplate neon;
            neon.target_architecture = "NEON";
            neon.code_template = "vsubq_f32({{input_0}}, {{input_1}})";
            neon.performance_hints["return_type"] = "float32x4_t";
            rule.target_templates["NEON"] = neon;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "scalar_load_float";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"load_float"};

            TransformTemplate neon;
            neon.target_architecture = "NEON";
            neon.code_template = "vld1q_f32((const float*){{input_0}})";
            neon.performance_hints["return_type"] = "float32x4_t";
            rule.target_templates["NEON"] = neon;
            rule_db->addRule(rule);
        }
        {
            OptimizationRule rule;
            rule.rule_id = "scalar_store_float";
            rule.category = "simd_instruction";
            rule.source_pattern.required_operations = {"store_float"};

            TransformTemplate neon;
            neon.target_architecture = "NEON";
            neon.code_template = "vst1q_f32((float*){{input_0}}, {{input_1}})";
            neon.performance_hints["return_type"] = "void";
            rule.target_templates["NEON"] = neon;
            rule_db->addRule(rule);
        }
    }
};

} // namespace aodsolve
