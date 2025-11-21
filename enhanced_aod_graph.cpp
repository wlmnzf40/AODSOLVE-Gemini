#include "aod/enhanced_aod_graph.h"
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <stack>
#include <sstream>
#include <iostream>

namespace aodsolve {

// Edge Implementation
AODEdge::AODEdge(std::shared_ptr<AODNode> src, std::shared_ptr<AODNode> tgt, AODEdgeType edge_type)
    : source(src), target(tgt), type(edge_type) {
    if (!src || !tgt) throw std::invalid_argument("Source or target node cannot be null");
}

std::string AODEdge::toString() const { return "Edge"; }
std::string AODEdge::getDOTLabel() const { return ""; }

// Graph Implementation
AODGraph::AODGraph(const std::string& graph_name) : name(graph_name) {}

void AODGraph::addNode(std::shared_ptr<AODNode> node) {
    if (!node) return;
    nodes.push_back(node);
    node_map[node->getId()] = node;
    nodes_by_name[node->getName()] = node;
}

bool AODGraph::removeNode(int /*node_id*/) { return false; }

std::shared_ptr<AODNode> AODGraph::getNode(int node_id) const {
    auto it = node_map.find(node_id);
    return (it != node_map.end()) ? it->second : nullptr;
}

std::shared_ptr<AODNode> AODGraph::getNodeByName(const std::string& name) const {
    auto it = nodes_by_name.find(name);
    return (it != nodes_by_name.end()) ? it->second : nullptr;
}

void AODGraph::addEdge(std::shared_ptr<AODNode> source, std::shared_ptr<AODNode> target, AODEdgeType type, const std::string& variable) {
    if (!source || !target) return;
    auto edge = std::make_shared<AODEdge>(source, target, type);
    edge->setVariableName(variable);
    edges.push_back(edge);
}

void AODGraph::addEdge(std::shared_ptr<AODNode> source, std::shared_ptr<AODNode> target, AODEdgeType type) {
    addEdge(source, target, type, "");
}

bool AODGraph::removeEdge(int /*source_id*/, int /*target_id*/) { return false; }

std::vector<std::shared_ptr<AODEdge>> AODGraph::getEdgesFrom(int node_id) const {
    std::vector<std::shared_ptr<AODEdge>> result;
    for (const auto& edge : edges) {
        if (edge->getSource()->getId() == node_id) result.push_back(edge);
    }
    return result;
}

std::vector<std::shared_ptr<AODEdge>> AODGraph::getEdgesTo(int node_id) const {
    std::vector<std::shared_ptr<AODEdge>> result;
    for (const auto& edge : edges) {
        if (edge->getTarget()->getId() == node_id) result.push_back(edge);
    }
    return result;
}

// Analysis Methods
void AODGraph::computeVariableDefinitions() {
    variable_defs_map.clear();
    for (const auto& node : nodes) {
        auto defs = node->getDefinedVariables();
        for (const auto& var : defs) variable_defs_map[var].insert(node);
    }
}

void AODGraph::computeVariableUses() {
    variable_uses_map.clear();
    for (const auto& node : nodes) {
        auto uses = node->getUsedVariables();
        for (const auto& var : uses) variable_uses_map[var].insert(node);
    }
}

std::set<std::string> AODGraph::getVariablesAtNode(int /*node_id*/) const { return {}; }
std::set<std::shared_ptr<AODNode>> AODGraph::getDefinitionsOf(const std::string& /*variable*/) const { return {}; }
std::set<std::shared_ptr<AODNode>> AODGraph::getUsesOf(const std::string& /*variable*/) const { return {}; }

void AODGraph::computeDominators() {}
std::vector<int> AODGraph::getImmediateDominators() const { return {}; }
std::set<int> AODGraph::getDominators(int /*node_id*/) const { return {}; }
bool AODGraph::isDominatedBy(int /*dominated*/, int /*dominator*/) const { return false; }
bool AODGraph::isCyclic() const { return false; }

std::vector<int> AODGraph::getEntryNodes() const { return {}; }
std::vector<int> AODGraph::getExitNodes() const { return {}; }

void AODGraph::eliminateDeadCode() {}
void AODGraph::constantPropagation() {}
void AODGraph::commonSubexpressionElimination() {}

bool AODGraph::isValid() const { return true; }
std::vector<std::string> AODGraph::getValidationErrors() const { return {}; }
void AODGraph::validateCycles() const {}
void AODGraph::validateNoOrphanedNodes() const {}

std::string AODGraph::toDOT() const {
    std::ostringstream oss;
    oss << "digraph " << name << " {\n";
    for (const auto& node : nodes) {
        oss << "  " << node->getId() << " [label=\"" << node->getDOTLabel() << "\", " << node->getDOTStyle() << "];\n";
    }
    for (const auto& edge : edges) {
        oss << "  " << edge->getSource()->getId() << " -> " << edge->getTarget()->getId() << ";\n";
    }
    oss << "}\n";
    return oss.str();
}

AODGraph::GraphStatistics AODGraph::getStatistics() const {
    GraphStatistics stats;
    stats.node_count = nodes.size();
    stats.edge_count = edges.size();
    for (const auto& node : nodes) {
        if (node->isSIMDNode()) stats.simd_nodes++;
        else if (node->isControlNode()) stats.control_nodes++;
        else if (node->isDataNode()) stats.data_nodes++;
        else if (node->isCallNode()) stats.call_nodes++;
        stats.complexity_score += node->getComplexity();
    }
    return stats;
}

void AODGraph::printStatistics() const {
    auto stats = getStatistics();
    std::cout << "Graph Stats: " << stats.node_count << " nodes, " << stats.edge_count << " edges." << std::endl;
}

// 缺少的接口空实现以通过链接
void AODGraph::topologicalSort() const {}
std::vector<int> AODGraph::getPath(int, int) const { return {}; }
bool AODGraph::hasPath(int, int) const { return false; }

} // namespace aodsolve
