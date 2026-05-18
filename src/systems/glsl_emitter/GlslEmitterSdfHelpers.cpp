#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterSdfHelperContext.h"

#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace sdf3d {
namespace {

constexpr uint64_t kGeneratedHelperIdBase = 1000000;

void emitSdfHelperPostorder(
    const SdfNodePtr& node,
    SdfCompileResult& result,
    GlslSdfHelperBlock& block,
    glsl_emitter::SdfHelperEmitContext& context,
    std::unordered_set<uint64_t>& emittedIds,
    std::unordered_set<const SdfNode*>& visiting)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node.");
        return;
    }

    if (visiting.find(node.get()) != visiting.end()) {
        result.errors.push_back("Cycle detected while emitting SDF helpers.");
        return;
    }

    const uint64_t id = glsl_emitter::helperIdFor(node, context);
    if (emittedIds.find(id) != emittedIds.end()) {
        return;
    }

    visiting.insert(node.get());
    for (const SdfNodePtr& child : node->children) {
        emitSdfHelperPostorder(child, result, block, context, emittedIds, visiting);
    }
    visiting.erase(node.get());

    const std::string functionName = "sdf_node_" + std::to_string(id);
    const std::string expression = glsl_emitter::emitGeometryExpression(node, "p", result, context);

    std::ostringstream glsl;
    glsl << "float " << functionName << "(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return " << expression << ";\n";
    glsl << "}\n";

    block.helpers.push_back({id, functionName, glsl.str()});
    block.functionNameByNode.emplace(node.get(), functionName);
    emittedIds.insert(id);
}

} // namespace

GlslSdfHelperBlock GlslEmitter::emitSdfHelpers(const SdfNodePtr& root, SdfCompileResult& result) const
{
    GlslSdfHelperBlock block;
    if (!root) {
        result.errors.push_back("Cannot emit SDF helpers for an empty tree.");
        return block;
    }

    std::unordered_map<const SdfNode*, uint64_t> generatedIds;
    std::unordered_set<uint64_t> emittedIds;
    std::unordered_set<const SdfNode*> visiting;
    uint64_t nextGeneratedId = kGeneratedHelperIdBase;
    glsl_emitter::SdfHelperEmitContext context{generatedIds, nextGeneratedId};

    emitSdfHelperPostorder(root, result, block, context, emittedIds, visiting);
    block.rootFunctionName = glsl_emitter::helperNameFor(root, context);
    return block;
}

} // namespace sdf3d
