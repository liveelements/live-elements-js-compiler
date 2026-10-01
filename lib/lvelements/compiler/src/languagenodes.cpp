/****************************************************************************
**
** Copyright (C) 2022 Dinu SV.
** This file is part of Livekeys Application.
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
****************************************************************************/

#include "languagenodes_p.h"
#include "propertybindingcontainer_p.h"
#include "live/visuallog.h"
#include "live/stacktrace.h"

#include <map>
#include <vector>
#include <algorithm>
#include <set>
#include <functional>
#include <string.h>

namespace lv{ namespace el{

namespace {

class DeclarationLookup{

public:
    DeclarationLookup(
        const Occurrence& declaration = Occurrence(nullptr, 0),
        JsBlockNode* moduleBlock = nullptr,
        JsImportNode* jsImportSite = nullptr,
        const Occurrence& spaceMismatch = Occurrence(nullptr, 0))
        : m_declaration(declaration)
        , m_moduleBlock(moduleBlock)
        , m_jsImportSite(jsImportSite)
        , m_spaceMismatch(spaceMismatch)
    {}

    const Occurrence& declaration() const{ return m_declaration; }
    IdentifierNode* identifier() const{ return m_declaration.identifier(); }
    JsBlockNode* moduleBlock() const{ return m_moduleBlock; }
    JsImportNode* jsImportSite() const{ return m_jsImportSite; }
    bool isModuleLevel() const{ return m_moduleBlock != nullptr; }

    const Occurrence& spaceMismatch() const{ return m_spaceMismatch; }

    static DeclarationLookup find(
        const std::string& source,
        BaseNode* node,
        const Occurrence& reference,
        BaseNode* boundary = nullptr);

private:
    Occurrence     m_declaration;
    JsBlockNode*   m_moduleBlock;
    JsImportNode*  m_jsImportSite;
    Occurrence     m_spaceMismatch;
};

DeclarationLookup DeclarationLookup::find(const std::string& source, BaseNode* node, const Occurrence& reference, BaseNode* boundary){
    const std::string id = BaseNode::slice(source, reference.identifier());
    // a meta reference is satisfied by a declaration in either space
    const Occurrence::Space requiredSpace = ( reference.space() & Occurrence::SpaceMeta )
        ? ( reference.space() & ~Occurrence::SpaceMeta ) | Occurrence::SpaceValue
        : reference.space();

    Occurrence spaceMismatch;
    while ( node && node != boundary ){
        if ( node->canCast<JsBlockNode>() ){
            JsBlockNode* block = node->as<JsBlockNode>();
            for ( const Occurrence& declaration : block->declarations() ){
                if ( BaseNode::slice(source, declaration.identifier()) != id ){
                    continue;
                }
                if ( requiredSpace != 0 && (declaration.space() & requiredSpace) == 0 ){
                    if ( !spaceMismatch.identifier() ){
                        spaceMismatch = declaration;
                    }
                    continue;
                }

                if ( !block->isNodeType<ProgramNode>() ){
                    return DeclarationLookup(declaration);
                }

                BaseNode* site = declaration.identifier()->parent();
                if ( site && site->isNodeType<JsImportNode>() ){
                    return DeclarationLookup(declaration, block, site->as<JsImportNode>());
                }

                while ( site && !site->isNodeType<ProgramNode>() ){
                    if ( site->isNodeType<TypedFunctionDeclarationNode>() || site->isNodeType<FunctionDeclarationNode>() || site->isNodeType<FunctionNode>() ){
                        return DeclarationLookup(declaration, block);
                    }
                    if ( site->isNodeType<VariableDeclaratorNode>() ){
                        return DeclarationLookup(declaration);
                    }
                    site = site->parent();
                }

                return DeclarationLookup(declaration, block);
            }
        }
        node = node->parent();
    }
    return DeclarationLookup(Occurrence(nullptr, 0), nullptr, nullptr, spaceMismatch);
}

} // namespace

std::string BaseNode::ConversionContext::baseComponentName(BaseNode::ConversionContext *ctx){
    return ctx && !ctx->baseComponent.empty() ? ctx->baseComponent : "Element";
}

std::string BaseNode::ConversionContext::baseComponentImport(BaseNode::ConversionContext *ctx){
    return ctx ? ctx->baseComponentImportUri : "";
}

bool BaseNode::ConversionContext::isImplicitType(BaseNode::ConversionContext *ctx, const std::string &type){
    if ( !ctx ){
        if ( type == "Element" )
            return true;
        return false;
    }

    if ( !ctx->baseComponent.empty() && ctx->baseComponent == type ){
        return true;
    } else if ( ctx->baseComponent.empty() ){
        if ( type == "Element" )
            return true;
    }

    for ( auto it = ctx->implicitTypes.begin(); it != ctx->implicitTypes.end(); ++it ){
        if ( *it == type )
            return true;
    }

    return false;
}

std::string FlowValueNode::flowCallRoleName(BaseNode::ConversionContext::FlowCall role){
    switch ( role ){
    case BaseNode::ConversionContext::FlowCall::Create: return "create";
    case BaseNode::ConversionContext::FlowCall::Call:   return "call";
    case BaseNode::ConversionContext::FlowCall::Return: return "return";
    case BaseNode::ConversionContext::FlowCall::Ref:    return "ref";
    case BaseNode::ConversionContext::FlowCall::Member: return "member";
    case BaseNode::ConversionContext::FlowCall::Function: return "function";
    }
    return "";
}

std::string BaseNode::ConversionContext::flowCall(BaseNode::ConversionContext *ctx, FlowCall role){
    if ( !ctx ){
        THROW_EXCEPTION(lv::Exception, "Flow literal found but flow configuration (flowCalls/flowModuleName) is not set.", lv::Exception::toCode("~Flow"));
    }

    auto it = ctx->flowCalls.find(role);
    if ( it == ctx->flowCalls.end() || it->second.empty() ){
        THROW_EXCEPTION(lv::Exception, Utf8("Missing flow call role '%' in flow configuration.").format(FlowValueNode::flowCallRoleName(role)), lv::Exception::toCode("~Flow"));
    }
    return it->second;
}

bool BaseNode::ConversionContext::flowConfigured(BaseNode::ConversionContext *ctx){
    return ctx && !ctx->flowCalls.empty() && !ctx->flowModuleName.empty();
}

BaseNode::BaseNode(const TSNode &node, const LanguageNodeInfo::ConstPtr &ni)
    : m_parent(nullptr)
    , m_node(node)
    , m_nodeInfo(ni)
{
}

std::vector<CommentNode*> BaseNode::extractPrecedingComments(const TSNode& tsnode) {
    std::vector<CommentNode*> result;
    if (ts_node_is_null(tsnode)) return result;
    TSNode prev = ts_node_prev_sibling(tsnode);
    std::vector<TSNode> commentNodes;
    while (!ts_node_is_null(prev) && std::string(ts_node_type(prev)) == "comment") {
        commentNodes.push_back(prev);
        prev = ts_node_prev_sibling(prev);
    }
    for (auto it = commentNodes.rbegin(); it != commentNodes.rend(); ++it) {
        result.push_back(new CommentNode(*it));
    }
    return result;
}

BaseNode::~BaseNode(){
    for ( auto it = m_children.begin(); it != m_children.end(); ++it ){
        delete *it;
    }
}

BaseNode *BaseNode::visit(const std::string &filePath, const std::string &fileName, LanguageParser::AST *ast){
    TSTree* tree = reinterpret_cast<TSTree*>(ast);
    TSNode root_node = ts_tree_root_node(tree);

    ProgramNode* node = new ProgramNode(root_node);
    node->setFileName(fileName);
    node->setFilePath(filePath);

    uint32_t count = ts_node_child_count(root_node);

    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(root_node, i);
        visit(node, child);
    }

    return node;
}

const std::vector<BaseNode *>& BaseNode::children() const{
    return m_children;
}

int BaseNode::startByte() const{
    return ts_node_start_byte(m_node);
}

int BaseNode::endByte() const{
    return ts_node_end_byte(m_node);
}

SourcePoint BaseNode::startPoint() const{
    TSPoint tpnt = ts_node_start_point(m_node);
    return SourcePoint(tpnt.row + 1, tpnt.column, ts_node_start_byte(m_node));
}

SourcePoint BaseNode::endPoint() const{
    TSPoint tpnt = ts_node_end_point(m_node);
    return SourcePoint(tpnt.row + 1, tpnt.column, ts_node_end_byte(m_node));
}

std::string BaseNode::rangeString() const{
    return std::string("[") + std::to_string(startByte()) + ", " + std::to_string(endByte()) + "]";
}

std::string BaseNode::pointRangeString() const{
    auto sp = startPoint();
    auto ep = endPoint();
    return std::string("[") + std::to_string(sp.line()) + ":" + std::to_string(sp.column()) + ", " + std::to_string(ep.line()) + ":" + std::to_string(ep.column()) + "]";
}

int BaseNode::nodeType() const{
    return m_nodeInfo->type();
}

const std::string &BaseNode::nodeName() const{
    return m_nodeInfo->name();
}

void BaseNode::collectImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext *ctx){
    for ( BaseNode* node : m_children ){
        node->collectImports(source, references, ctx);
    }
}

void FlowExpressionNode::collectImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext *ctx){
    std::vector<TypeNode*> signatureTypes;
    if ( m_parameters ){
        for ( ParameterNode* parameter : m_parameters->parameters() ){
            if ( parameter->type() ){
                signatureTypes.push_back(parameter->type());
            }
        }
    }
    if ( m_returnType ){
        signatureTypes.push_back(m_returnType);
    }

    for ( TypeNode* type : signatureTypes ){
        for ( const Occurrence& declaration : type->declaredIdentifiers() ){
            IdentifierNode* name = declaration.identifier();
            DeclarationLookup lookup = DeclarationLookup::find(source, this->parent(), Occurrence(name, Occurrence::SpaceType));
            IdentifierNode* found = lookup.identifier();
            bool isTypeParameter = found && found->parent() && found->parent()->isNodeType<TypeParameterListNode>();
            if ( !found || lookup.jsImportSite() || isTypeParameter ){
                continue;
            }

            throwError(
                name,
                name->current(),
                Utf8("'%' is declared in this file; a flow can only use imported types")
                    .format(slice(source, name)).data()
            );
        }
    }


    if ( m_parameters ){
        m_parameters->collectImports(source, references, ctx);
    }
    if ( m_returnType ){
        m_returnType->collectImports(source, references, ctx);
    }
    if ( m_flowBody ){
        m_flowBody->collectImports(source, references, ctx);
    }
}

std::vector<FlowExpressionNode*> FlowExpressionNode::collectFlowExpressions(BaseNode* node){
    std::vector<FlowExpressionNode*> flows;
    if ( !node ){
        return flows;
    }

    if ( node->isNodeType<FlowExpressionNode>() ){
        flows.push_back(node->as<FlowExpressionNode>());
    }

    for ( BaseNode* child : node->children() ){
        std::vector<FlowExpressionNode*> childFlows = FlowExpressionNode::collectFlowExpressions(child);
        flows.insert(flows.end(), childFlows.begin(), childFlows.end());
    }
    return flows;
}

void FlowBodyNode::collectImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext *ctx){
    BaseNode::collectImports(source, references, ctx);

    std::set<std::string> seenDeclarations;
    for ( const Occurrence& declaration : declarations() ){
        std::string name = slice(source, declaration.identifier());
        if ( seenDeclarations.find(name) != seenDeclarations.end() ){
            throwError(
                declaration.identifier(),
                declaration.identifier()->current(),
                Utf8("Duplicate flow step label '%'").format(name).data()
            );
        }
        seenDeclarations.insert(name);
    }

    for ( const Occurrence& reference : this->references() ){
        IdentifierNode* use = reference.identifier();
        const std::string name = slice(source, use);
        const bool isCallee = use->parent() && use->parent()->isNodeType<FlowCallNode>();

        if ( isCallee ){
            DeclarationLookup callee = DeclarationLookup::find(source, this->parent(), reference);
            if ( callee.identifier() ){
                if ( callee.isModuleLevel() ){
                    continue;
                }

                throwError(
                    use,
                    use->current(),
                    Utf8("'%' is declared outside the flow; a flow can only use its own parameters and taps, imported names, or top-level functions")
                        .format(name)
                        .data()
                );
            }

            DeclarationLookup flowLocal = DeclarationLookup::find(source, this, reference);
            if ( flowLocal.identifier() && declaresIdentifier(flowLocal.identifier()) ){
                throwError(
                    use,
                    use->current(),
                    Utf8("Cannot call '%' as a function; flow parameters and step results are values")
                        .format(name)
                        .data()
                );
            }

            if ( ConversionContext::isImplicitType(ctx, name) ){
                throwError(
                    use,
                    use->current(),
                    Utf8("'%' is a global; a flow can only call imported names or functions declared in this module")
                        .format(name)
                        .data()
                );
            }

            references.push_back(reference);
            continue;
        }

        DeclarationLookup lookup = DeclarationLookup::find(source, this, reference);
        if ( !lookup.identifier() ){
            references.push_back(reference);
            continue;
        }

        if ( declaresIdentifier(lookup.identifier()) ){
            continue;
        }

        if ( lookup.isModuleLevel() ){
            throwError(
                use,
                use->current(),
                Utf8("'%' is not a flow value; only parameters and step results can be referenced")
                    .format(name)
                    .data()
            );
        }

        throwError(
            use,
            use->current(),
            Utf8("'%' is declared outside the flow; a flow can only use its own parameters and taps, imported names, or top-level functions")
                .format(name)
                .data()
        );
    }
}

void FlowCallNode::collectImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext *ctx){
    for ( FlowValueNode* argument : m_arguments ){
        argument->collectImports(source, references, ctx);
    }
}

bool JsBlockNode::declaresIdentifier(IdentifierNode* declaration) const{
    for ( const Occurrence& ownDeclaration : m_declarations ){
        if ( ownDeclaration.identifier() == declaration ){
            return true;
        }
    }
    return false;
}

std::string BaseNode::slice(const std::string &source, uint32_t start, uint32_t end){
    return source.substr(start, end - start);
}

std::string BaseNode::slice(const std::string &source, BaseNode *node){
    return slice(source, node->startByte(), node->endByte());
}

std::string BaseNode::slice(const std::string &source, const TSNode &node){
    return slice(source, ts_node_start_byte(node), ts_node_end_byte(node));
}

JsBlockNode* BaseNode::addToDeclarations(BaseNode *parent, IdentifierNode *idNode, Occurrence::Space space){
    BaseNode* p = parent;
    while (p){
        JsBlockNode* jsbn = dynamic_cast<JsBlockNode*>(p);
        if (jsbn){
            jsbn->m_declarations.push_back(Occurrence(idNode, space));
            return jsbn;
        }
        p = p->parent();
    }
    return nullptr;
}

JsBlockNode *BaseNode::addUsedIdentifier(BaseNode *parent, const Occurrence& reference){
    BaseNode* p = parent;
    while (p){
        JsBlockNode* jsbn = dynamic_cast<JsBlockNode*>(p);
        if (jsbn){
            jsbn->m_references.push_back(reference);
            return jsbn;
        }
        p = p->parent();
    }

    return nullptr;
}

std::string BaseNode::nodeSourceFilePath(BaseNode *p){
    while (p && !p->isNodeType<ProgramNode>() )
        p = p->parent();
    return p && p->isNodeType<ProgramNode>() ? static_cast<ProgramNode*>(p)->m_filePath : "";
}


SourceRangeLocation BaseNode::nodeSourceLocation(BaseNode *p, const TSNode& node){
    std::string fileName = BaseNode::nodeSourceFilePath(p);
    if ( !ts_node_is_null(node) ){
        SourcePoint startPoint = SourcePoint(
            ts_node_start_point(node).row + 1,
            ts_node_start_point(node).column + 1,
            ts_node_start_byte(node)
        );
        SourcePoint endPoint = SourcePoint(
            ts_node_end_point(node).row + 1,
            ts_node_end_point(node).column + 1,
            ts_node_end_byte(node)
        );
        return SourceRangeLocation(SourceRange(startPoint, endPoint), fileName);
    }
    return SourceRangeLocation(SourceRange(), fileName);
}

void BaseNode::assertValid(BaseNode *from, const TSNode &node, const std::string& message){
    if ( ts_node_is_null(node) ){
        SyntaxException se = SyntaxException(
            "Syntax error: " + message,
            Exception::toCode("~Language"),
            nodeSourceLocation(from, from ? from->current() : node),
            SOURCE_TRACE()
        );
        throw se;
    }
    assertError(from, node, message);
}

void BaseNode::assertError(BaseNode *from, const TSNode &node, const std::string &message){
    if ( strcmp(ts_node_type(node), "ERROR") == 0 ){
        SyntaxException se = SyntaxException(
            "Syntax error: " + message,
            Exception::toCode("~Language"),
            nodeSourceLocation(from, node),
            SOURCE_TRACE()
        );
        throw se;
    }
}

void BaseNode::throwError(BaseNode *from, const TSNode &node, const std::string &message){
    SyntaxException se = SyntaxException(
        "Syntax error: " + message,
        Exception::toCode("~Language"),
        nodeSourceLocation(from, node),
        SOURCE_TRACE()
    );
    throw se;
}

TSNode BaseNode::nodeChildByFieldName(const TSNode &node, const std::string &name){
    return ts_node_child_by_field_name(node, name.c_str(), static_cast<uint32_t>(name.length()));
}

std::vector<IdentifierNode *> BaseNode::fromNestedIdentifier(BaseNode *parent, const TSNode &node){
    std::vector<IdentifierNode *> result;
    if ( strcmp(ts_node_type(node), "identifier") == 0 ){
        result.push_back(IdentifierNode::createNonEmpty(node, parent));
    } else if ( strcmp(ts_node_type(node), "nested_identifier") == 0 || strcmp(ts_node_type(node), "member_expression") == 0 ){
        uint32_t count = ts_node_child_count(node);
        for ( uint32_t i = 0; i < count; ++i ){
            TSNode child = ts_node_child(node, i);
            assertError(parent, child, "Expected identifier");
            if ( strcmp(ts_node_type(child), "identifier") == 0 ){
                result.push_back(IdentifierNode::createNonEmpty(child, parent));
            } else if ( strcmp(ts_node_type(child), "property_identifier") == 0 ){
                result.push_back(IdentifierNode::createNonEmpty(child, parent));
            } else if ( strcmp(ts_node_type(child), "nested_identifier") == 0 || strcmp(ts_node_type(child), "member_expression") == 0 ){
                auto nestedResult = fromNestedIdentifier(parent, child);
                result.insert( result.end(), nestedResult.begin(), nestedResult.end() );
            } else if ( (strcmp(ts_node_type(child), ".") != 0) ){
                throwError(parent, child, "Expected identifier");
            }
        }
    }

    return result;
}

ParameterListNode *BaseNode::scanFormalParameters(BaseNode *parent, const TSNode& formalParameters){
    ParameterListNode* result = parent->addCreatedChild(new ParameterListNode(formalParameters));

    uint32_t paramterCount = ts_node_child_count(formalParameters);

    for (uint32_t pc = 0; pc < paramterCount; ++pc){
        TSNode ftpc = ts_node_child(formalParameters, pc);
        assertError(parent, ftpc, "Function declaration not supported.");
        if (strcmp(ts_node_type(ftpc), "identifier") == 0){
            auto paramNode = result->addCreatedChild(new ParameterNode(ftpc, IdentifierNode::createNonEmpty(ftpc, result)));
            if ( paramNode->identifier() && !paramNode->isDestructuringPattern() ){
                addToDeclarations(result, paramNode->identifier(), Occurrence::SpaceValue);
            }
            result->m_parameters.push_back(paramNode);
        } else if ( strcmp(ts_node_type(ftpc), "required_parameter") == 0 ){
            auto name = BaseNode::nodeChildByFieldName(ftpc, "pattern");
            auto paramNode = result->addCreatedChild(new ParameterNode(ftpc, IdentifierNode::createNonEmpty(name, result)));

            auto type = BaseNode::nodeChildByFieldName(ftpc, "type");
            if (!ts_node_is_null(type)) {
                paramNode->m_type = paramNode->addCreatedChild(TypeNode::create(type, paramNode));
            }
            if ( paramNode->identifier() && !paramNode->isDestructuringPattern() ){
                addToDeclarations(result, paramNode->identifier(), Occurrence::SpaceValue);
            }
            result->m_parameters.push_back(paramNode);
        } else if ( strcmp(ts_node_type(ftpc), "optional_parameter") == 0 ){
            auto name = BaseNode::nodeChildByFieldName(ftpc, "pattern");
            auto paramNode = result->addCreatedChild(new ParameterNode(ftpc, IdentifierNode::createNonEmpty(name, result)));
            paramNode->m_isOptional = true;

            auto type = BaseNode::nodeChildByFieldName(ftpc, "type");
            if (!ts_node_is_null(type)) {
                paramNode->m_type = paramNode->addCreatedChild(TypeNode::create(type, paramNode));
            }
            if ( paramNode->identifier() && !paramNode->isDestructuringPattern() ){
                addToDeclarations(result, paramNode->identifier(), Occurrence::SpaceValue);
            }
            result->m_parameters.push_back(paramNode);
        }
    }
    return result;
}

ParameterListNode *BaseNode::scanFormalTypeParameters(BaseNode *parent, const TSNode &parameters, bool isMeta){
    auto result = parent->addCreatedChild(new ParameterListNode(parameters));
    uint32_t parameterCount = ts_node_child_count(parameters);

    for (uint32_t pc = 0; pc < parameterCount; ++pc){
        TSNode ftpc = ts_node_child(parameters, pc);
        assertError(parent, ftpc, "Function parameter not declared property.");
        if (strcmp(ts_node_type(ftpc), "formal_type_parameter") == 0){

            if ( ts_node_child_count(ftpc) > 0 ){
                TSNode typeParameter = ts_node_child(ftpc, 0);

                TSNode parameterName = BaseNode::nodeChildByFieldName(typeParameter, "name");
                if (ts_node_is_null(parameterName)) {
                    parameterName = BaseNode::nodeChildByFieldName(typeParameter, "pattern");
                }
                assertValid(parent, parameterName, "Parameter name is null.");
                TSNode parameterType = BaseNode::nodeChildByFieldName(typeParameter, "type");
                assertValid(parent, parameterType, "Parameter type is null.");

                ParameterNode* paramNode = nullptr;
                if ( strcmp(ts_node_type(typeParameter), "required_type_parameter") == 0 ){
                    paramNode = result->addCreatedChild(
                        new ParameterNode(typeParameter, IdentifierNode::createNonEmpty(parameterName, result), false)
                    );
                } else if ( strcmp(ts_node_type(typeParameter), "optional_type_parameter") == 0){
                    paramNode = result->addCreatedChild(
                        new ParameterNode(typeParameter, IdentifierNode::createNonEmpty(parameterName, result), true)
                    );
                }

                if ( paramNode ){
                    paramNode->m_type = paramNode->addCreatedChild(TypeNode::create(parameterType, paramNode, isMeta));
                    TSNode parameterValue = BaseNode::nodeChildByFieldName(typeParameter, "value");
                    if ( !ts_node_is_null(parameterValue) ){
                        paramNode->m_defaultValue = paramNode->addCreatedChild(new ExpressionNode(parameterValue));
                        visit(paramNode->m_defaultValue, parameterValue);
                    }
                    if ( paramNode->identifier() && !paramNode->isDestructuringPattern() ){
                        addToDeclarations(result, paramNode->identifier(), Occurrence::SpaceValue);
                    }
                    result->m_parameters.push_back(paramNode);
                }
            }
        }
    }
    return result;
}

ParameterListNode *BaseNode::scanFlowParameters(BaseNode *parent, const TSNode &parameters){
    auto result = parent->addCreatedChild(new ParameterListNode(parameters));
    uint32_t parameterCount = ts_node_child_count(parameters);

    for ( uint32_t pc = 0; pc < parameterCount; ++pc ){
        TSNode parameter = ts_node_child(parameters, pc);
        assertError(parent, parameter, "Flow parameter is invalid.");
        if ( strcmp(ts_node_type(parameter), "flow_parameter") != 0 ){
            continue;
        }

        TSNode parameterName = BaseNode::nodeChildByFieldName(parameter, "name");
        assertValid(parent, parameterName, "Flow parameter name is null.");
        TSNode parameterType = BaseNode::nodeChildByFieldName(parameter, "type");
        assertValid(parent, parameterType, "Flow parameter type is null.");

        auto paramNode = result->addCreatedChild(
            new ParameterNode(parameter, IdentifierNode::createNonEmpty(parameterName, result), false)
        );
        paramNode->m_type = paramNode->addCreatedChild(TypeNode::create(parameterType, paramNode, true));
        result->m_parameters.push_back(paramNode);
    }

    return result;
}

void BaseNode::addChild(BaseNode *child){
    m_children.push_back(child);
    child->setParent(this);
}

std::string BaseNode::astString() const{
    char* str = ts_node_string(m_node);
    std::string result(str);
    free(str);
    return result;
}

std::string BaseNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += nodeName() + " [" +
            std::to_string(ts_node_start_byte(m_node)) + ", " +
            std::to_string(ts_node_end_byte(m_node)) + "]\n";

    for (BaseNode* child : m_children){
        result += child->toString(indent < 0 ? indent : indent + 1);
    }

    return result;
}

std::string CommentNode::text(const std::string& source) const {
    return slice(source, startByte(), endByte());
}

BaseNode *BaseNode::visitRecognized(BaseNode *parent, const TSNode &node){
    if ( strcmp(ts_node_type(node), "type_identifier") == 0 ){
        return TypeNode::visitTypeIdentifier(parent, node);
    } else if ( strcmp(ts_node_type(node), "nested_type_identifier") == 0 ){
        return TypeNode::visitNestedTypeIdentifier(parent, node);
    } else if ( strcmp(ts_node_type(node), "type_query") == 0 ){
        return TypeNode::visitTypeQuery(parent, node);
    } else if ( parent->isNodeType<TypeNode>() && strcmp(ts_node_type(node), "ERROR") != 0 ){
        return nullptr;
    } else if ( strcmp(ts_node_type(node), "import_statement") == 0 ){
        return visitImport(parent, node);
    } else if ( strcmp(ts_node_type(node), "js_import_statement") == 0 ){
        return visitJsImport(parent, node);
    } else if ( strcmp(ts_node_type(node), "identifier") == 0 ){
        return visitIdentifier(parent, node);
    } else if ( strcmp(ts_node_type(node), "constructor_definition") == 0 ){
        return visitConstructorDefinition(parent, node);
    } else if ( strcmp(ts_node_type(node), "property_identifier") == 0 ){
        return visitPropertyIdentifier(parent, node);
    } else if ( strcmp(ts_node_type(node), "this") == 0 ){
        return visitIdentifier(parent, node);
    } else if ( strcmp(ts_node_type(node), "import_path") == 0 ){
        return visitImportPath(parent, node);
    } else if ( strcmp(ts_node_type(node), "component") == 0 ){
        return visitComponentDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "component_declaration") == 0 ){
        return visitComponentDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "component_instance_statement") == 0 ){
        return visitComponentInstanceStatement(parent, node);
    } else if ( strcmp(ts_node_type(node), "new_component_expression") == 0 ){
        return visitNewComponentExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "nested_new_component_expression") == 0 ){
        return visitNewComponentExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "constructor_initializer") == 0 ){
        return visitConstructorInitializer(parent, node);
    } else if ( strcmp(ts_node_type(node), "arrow_function") == 0 ){
        return visitArrowFunction(parent, node);
    } else if ( strcmp(ts_node_type(node), "component_body") == 0 ){
        return visitComponentBody(parent, node);
    } else if ( strcmp(ts_node_type(node), "new_component_body") == 0 ){
        return visitComponentBody(parent, node);
    } else if ( strcmp(ts_node_type(node), "class_declaration") == 0 ){
        return visitClassDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "property_declaration") == 0 ){
        return visitPropertyDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "static_property_declaration") == 0 ){
        return visitStaticPropertyDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "member_expression") == 0 ){
        return visitMemberExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "subscript_expression") == 0 ){
        return visitSubscriptExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "identifier_property_assignment") == 0 ){
        return visitIdentifierAssignment(parent, node);
    } else if ( strcmp(ts_node_type(node), "property_assignment") == 0 ){
        return visitPropertyAssignment(parent, node);
    } else if ( strcmp(ts_node_type(node), "event_declaration") == 0 ){
        return visitEventDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "listener_declaration") == 0 ){
        return visitListenerDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "method_definition") == 0 ){
        return visitMethodDefinition(parent, node);
    } else if ( strcmp(ts_node_type(node), "typed_method_declaration") == 0 ){
        return visitTypedMethodDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "typed_function_declaration") == 0 ||
                strcmp(ts_node_type(node), "declared_typed_function_declaration") == 0 ){
        return visitTypedFunctionDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "flow_function_declaration") == 0 ){
        return visitFlowFunctionDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "property_accessor_declaration") == 0 ){
        return visitPropertyAccessorDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "function_declaration") == 0 ){
        return visitFunctionDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "function_expression") == 0 ){
        return visitFunction(parent, node);
    } else if ( strcmp(ts_node_type(node), "number") == 0 ){
        return visitNumber(parent, node);
    } else if ( strcmp(ts_node_type(node), "expression_statement") == 0 ){
        return visitExpressionStatement(parent, node);
    } else if ( strcmp(ts_node_type(node), "assignment_expression") == 0 ){
        return visitAssignmentExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "call_expression") == 0 ){
        return visitCallExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "new_tagged_component_expression") == 0 ){
        return visitNewTaggedComponentExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "tagged_type_string") == 0 ){
        return visitTaggedString(parent, node);
    } else if ( strcmp(ts_node_type(node), "new_tripple_tagged_component_expression") == 0 ){
        return visitNewTrippleTaggedComponentExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "tripple_tagged_type_string") == 0 ){
        return visitTrippleTaggedString(parent, node);
    } else if ( strcmp(ts_node_type(node), "variable_declaration") == 0 ){
        return visitVariableDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "lexical_declaration") == 0 ){
        return visitLexicalDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "array_pattern") == 0 ){
        return visitDestructuringPattern(parent, node);
    } else if ( strcmp(ts_node_type(node), "object_pattern") == 0 ){
        return visitDestructuringPattern(parent, node);
    } else if ( strcmp(ts_node_type(node), "new_expression") == 0 ){
        return visitNewExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "return_statement") == 0 ){
        return visitReturnStatement(parent, node);
    } else if ( strcmp(ts_node_type(node), "object") == 0 ){
        return visitObject(parent, node);
    } else if ( strcmp(ts_node_type(node), "try_statement") == 0 ){
        return visitTryCatchBlock(parent, node);
    } else if ( strcmp(ts_node_type(node), "for_in_statement") == 0 ){
        return visitForInStatement(parent, node);
    } else if ( strcmp(ts_node_type(node), "type_alias_declaration") == 0 ){
        return visitTypeAliasDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "interface_declaration") == 0 ){
        return visitInterfaceDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "enum_declaration") == 0 ){
        return visitEnumDeclaration(parent, node);
    } else if ( strcmp(ts_node_type(node), "flow_expression") == 0 ){
        return visitFlowExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "ERROR") == 0 ){
        SyntaxException se = SyntaxException(
            "Syntax error",
            Exception::toCode("~Language"),
            nodeSourceLocation(parent, node),
            SOURCE_TRACE()
        );
        throw se;
    }
    return nullptr;
}

namespace {

ProgramNode* flowProgramNode(BaseNode* node){
    while ( node && !node->isNodeType<ProgramNode>() ){
        node = node->parent();
    }
    return node ? node->as<ProgramNode>() : nullptr;
}

// Flow validation, build pass (AST-only): structural rules that do not need the
// source text. Each flow expression is checked in isolation; nested flow
// expressions are reached by their own visitFlowExpression call, so the value
// walk below treats them as a no-op.
void checkFlowValueShape(FlowValueNode* value, bool allowCall, bool allowPrevious){
    if ( value->isNodeType<FlowCallNode>() ){
        if ( !allowCall ){
            BaseNode::throwError(value, value->current(), "Nested flow calls are not allowed in argument position.");
        }
        for ( FlowValueNode* argument : value->as<FlowCallNode>()->arguments() ){
            checkFlowValueShape(argument, false, allowPrevious);
        }
        return;
    }

    if ( value->isNodeType<FlowReferenceNode>() ){
        FlowReferenceNode* reference = value->as<FlowReferenceNode>();
        if ( reference->isPrevious() && !allowPrevious ){
            BaseNode::throwError(value, reference->current(), "The '%' reference can only be used on the first line when the flow has exactly one parameter, or on later lines.");
        }
        return;
    }

    if ( value->isNodeType<FlowMemberNode>() ){
        checkFlowValueShape(value->as<FlowMemberNode>()->base(), false, allowPrevious);
        return;
    }

    if ( value->isNodeType<FlowLiteralNode>() ){
        FlowLiteralNode* literal = value->as<FlowLiteralNode>();
        for ( FlowValueNode* item : literal->items() ){
            checkFlowValueShape(item, false, allowPrevious);
        }
        for ( FlowObjectPairNode* pair : literal->objectPairs() ){
            checkFlowValueShape(pair->value(), false, allowPrevious);
        }
        return;
    }
}

void checkFlowExpressionShape(FlowExpressionNode* node){
    const int parameterCount = node->parameters() ? static_cast<int>(node->parameters()->parameters().size()) : 0;

    if ( node->isExpressionBody() ){
        checkFlowValueShape(node->body()->as<FlowValueNode>(), true, parameterCount == 1);
        return;
    }

    for ( size_t index = 0; index < node->statements().size(); ++index ){
        FlowStatementNode* statement = node->statements()[index];
        bool allowPrevious = index == 0 ? parameterCount == 1 : true;

        if ( statement->isNodeType<FlowReturnNode>() ){
            checkFlowValueShape(statement->as<FlowReturnNode>()->value(), true, allowPrevious);
            continue;
        }

        FlowStepNode* step = statement->as<FlowStepNode>();
        if ( !step->expression()->isNodeType<FlowCallNode>() ){
            BaseNode::throwError(step, step->expression()->current(), "Flow steps must be function calls.");
        }
        checkFlowValueShape(step->expression(), true, allowPrevious);
    }
}

} // namespace

FlowObjectPairNode* BaseNode::buildFlowObjectPair(BaseNode* parent, const TSNode& node){
    FlowObjectPairNode* pairNode = parent->addCreatedChild(new FlowObjectPairNode(node));
    TSNode key = BaseNode::nodeChildByFieldName(node, "key");
    BaseNode::assertValid(parent, key, "Failed to find flow object key.");
    pairNode->m_key = pairNode->addCreatedChild(new FlowObjectPairKeyNode(key));

    TSNode value = BaseNode::nodeChildByFieldName(node, "value");
    BaseNode::assertValid(parent, value, "Failed to find flow object value.");
    pairNode->m_value = BaseNode::buildFlowValue(pairNode, value);
    return pairNode;
}

FlowLiteralNode* BaseNode::buildFlowLiteral(BaseNode* parent, const TSNode& node){
    FlowLiteralNode* literalNode = parent->addCreatedChild(new FlowLiteralNode(node));
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        BaseNode::assertError(parent, child, "Invalid flow literal.");
        const char* type = ts_node_type(child);
        if ( strcmp(type, "flow_array") == 0 ){
            literalNode->m_kind = FlowLiteralNode::Array;
            uint32_t itemCount = ts_node_child_count(child);
            for ( uint32_t itemIndex = 0; itemIndex < itemCount; ++itemIndex ){
                TSNode item = ts_node_child(child, itemIndex);
                if ( ts_node_is_named(item) ){
                    literalNode->m_items.push_back(BaseNode::buildFlowValue(literalNode, item));
                }
            }
        } else if ( strcmp(type, "flow_object") == 0 ){
            literalNode->m_kind = FlowLiteralNode::Object;
            uint32_t pairCount = ts_node_child_count(child);
            for ( uint32_t pairIndex = 0; pairIndex < pairCount; ++pairIndex ){
                TSNode pair = ts_node_child(child, pairIndex);
                if ( strcmp(ts_node_type(pair), "flow_object_pair") == 0 ){
                    literalNode->m_objectPairs.push_back(buildFlowObjectPair(literalNode, pair));
                }
            }
        }
    }
    return literalNode;
}

FlowValueNode* BaseNode::buildFlowValue(BaseNode* parent, const TSNode& node){
    BaseNode::assertError(parent, node, "Invalid flow value.");
    const char* type = ts_node_type(node);

    if ( strcmp(type, "flow_parenthesized_value") == 0 ){
        uint32_t count = ts_node_child_count(node);
        for ( uint32_t i = 0; i < count; ++i ){
            TSNode child = ts_node_child(node, i);
            if ( ts_node_is_named(child) ){
                FlowValueNode* inner = buildFlowValue(parent, child);
                if ( inner->isNodeType<FlowExpressionNode>() ){
                    inner->as<FlowExpressionNode>()->setParenthesized(true);
                }
                return inner;
            }
        }
        BaseNode::throwError(parent, node, "Failed to unwrap parenthesized flow value.");
    }

    if ( strcmp(type, "flow_expression") == 0 ){
        return BaseNode::visitFlowExpression(parent, node);
    }
    if ( strcmp(type, "flow_call") == 0 ){
        FlowCallNode* callNode = parent->addCreatedChild(new FlowCallNode(node));
        TSNode function = BaseNode::nodeChildByFieldName(node, "function");
        BaseNode::assertValid(parent, function, "Failed to find flow callee.");
        callNode->m_callee = BaseNode::fromNestedIdentifier(callNode, function);
        for ( IdentifierNode* identifier : callNode->m_callee ){
            callNode->addChild(identifier);
        }
        if ( !callNode->m_callee.empty() ){
            addUsedIdentifier(callNode, callNode->m_callee.front());
        }

        TSNode arguments = BaseNode::nodeChildByFieldName(node, "arguments");
        BaseNode::assertValid(parent, arguments, "Failed to find flow arguments.");
        uint32_t argCount = ts_node_child_count(arguments);
        for ( uint32_t i = 0; i < argCount; ++i ){
            TSNode argument = ts_node_child(arguments, i);
            if ( ts_node_is_named(argument) ){
                callNode->m_arguments.push_back(buildFlowValue(callNode, argument));
            }
        }
        return callNode;
    }
    if ( strcmp(type, "flow_member") == 0 ){
        FlowMemberNode* memberNode = parent->addCreatedChild(new FlowMemberNode(node));
        TSNode base = BaseNode::nodeChildByFieldName(node, "base");
        BaseNode::assertValid(parent, base, "Failed to find flow member base.");
        FlowValueNode* baseValue = buildFlowValue(memberNode, base);
        if ( !baseValue->isNodeType<FlowReferenceNode>() ){
            BaseNode::throwError(parent, base, "Flow member base must be a reference.");
        }
        memberNode->m_base = baseValue->as<FlowReferenceNode>();

        uint32_t count = ts_node_child_count(node);
        for ( uint32_t i = 0; i < count; ++i ){
            TSNode child = ts_node_child(node, i);
            if ( strcmp(ts_node_type(child), "identifier") == 0 ){
                memberNode->m_properties.push_back(IdentifierNode::createNonEmpty(child, memberNode));
            }
        }
        return memberNode;
    }
    if ( strcmp(type, "flow_reference") == 0 ){
        FlowReferenceNode* referenceNode = parent->addCreatedChild(new FlowReferenceNode(node));
        TSNode name = BaseNode::nodeChildByFieldName(node, "name");
        TSNode previous = BaseNode::nodeChildByFieldName(node, "previous");
        if ( !ts_node_is_null(name) ){
            referenceNode->m_name = IdentifierNode::createNonEmpty(name, referenceNode);
            addUsedIdentifier(referenceNode, referenceNode->m_name);
        }
        if ( !ts_node_is_null(previous) ){
            referenceNode->m_isPrevious = true;
        }
        return referenceNode;
    }
    if ( strcmp(type, "flow_literal") == 0 ){
        return buildFlowLiteral(parent, node);
    }

    BaseNode::throwError(parent, node, Utf8("Unsupported flow value '%'").format(type).data());
    return nullptr;
}

FlowStatementNode* BaseNode::buildFlowStatement(BaseNode* parent, const TSNode& node){
    BaseNode::assertError(parent, node, "Invalid flow statement.");
    const char* type = ts_node_type(node);

    if ( strcmp(type, "flow_statement") == 0 ){
        uint32_t count = ts_node_child_count(node);
        for ( uint32_t i = 0; i < count; ++i ){
            TSNode child = ts_node_child(node, i);
            if ( ts_node_is_named(child) ){
                return buildFlowStatement(parent, child);
            }
        }
        BaseNode::throwError(parent, node, "Empty flow statement.");
    }

    if ( strcmp(type, "flow_return") == 0 ){
        FlowReturnNode* returnNode = parent->addCreatedChild(new FlowReturnNode(node));
        TSNode value = BaseNode::nodeChildByFieldName(node, "value");
        BaseNode::assertValid(parent, value, "Flow return value is missing.");
        returnNode->m_value = buildFlowValue(returnNode, value);
        return returnNode;
    }

    if ( strcmp(type, "flow_step") == 0 ){
        FlowStepNode* stepNode = parent->addCreatedChild(new FlowStepNode(node));
        TSNode expression = BaseNode::nodeChildByFieldName(node, "expression");
        BaseNode::assertValid(parent, expression, "Flow step expression is missing.");
        stepNode->m_expression = buildFlowValue(stepNode, expression);

        TSNode label = BaseNode::nodeChildByFieldName(node, "label");
        if ( !ts_node_is_null(label) ){
            TSNode labelName = BaseNode::nodeChildByFieldName(label, "name");
            BaseNode::assertValid(parent, labelName, "Flow step label name is missing.");
            stepNode->m_label = IdentifierNode::createNonEmpty(labelName, stepNode);
            addToDeclarations(parent, stepNode->m_label, Occurrence::SpaceValue);
        }

        uint32_t count = ts_node_child_count(node);
        for ( uint32_t i = 0; i < count; ++i ){
            TSNode child = ts_node_child(node, i);
            if ( strcmp(ts_node_type(child), "flow_order") == 0 ){
                TSNode orderName = BaseNode::nodeChildByFieldName(child, "name");
                BaseNode::assertValid(parent, orderName, "Flow order name is missing.");
                stepNode->m_order.push_back(IdentifierNode::createNonEmpty(orderName, stepNode));
            }
        }
        return stepNode;
    }

    BaseNode::throwError(parent, node, Utf8("Unsupported flow statement '%'").format(type).data());
    return nullptr;
}

FlowExpressionNode* BaseNode::visitFlowExpression(BaseNode* parent, const TSNode& node){
    FlowExpressionNode* expressionNode = parent->addCreatedChild(new FlowExpressionNode(node));

    TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");
    BaseNode::assertValid(parent, parameters, "Flow parameters are missing.");

    expressionNode->m_parameters = BaseNode::scanFlowParameters(expressionNode, parameters);

    TSNode returnType = BaseNode::nodeChildByFieldName(parameters, "return");
    if ( ts_node_is_null(returnType) ){
        returnType = BaseNode::nodeChildByFieldName(node, "return");
    }
    if ( !ts_node_is_null(returnType) ){
        expressionNode->m_returnType = expressionNode->addCreatedChild(TypeNode::create(returnType, expressionNode, true));
    }

    TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    BaseNode::assertValid(parent, body, "Flow body is missing.");
    expressionNode->m_flowBody = expressionNode->addCreatedChild(new FlowBodyNode(body));

    if ( expressionNode->m_parameters ){
        for ( ParameterNode* parameter : expressionNode->m_parameters->parameters() ){
            addToDeclarations(expressionNode->m_flowBody, parameter->identifier(), Occurrence::SpaceValue);
        }
    }

    if ( strcmp(ts_node_type(body), "flow_body") == 0 ){
        expressionNode->m_flowBody->m_expressionBody = false;
        uint32_t statementCount = ts_node_child_count(body);
        for ( uint32_t i = 0; i < statementCount; ++i ){
            TSNode child = ts_node_child(body, i);
            if ( ts_node_is_named(child) ){
                expressionNode->m_flowBody->m_statements.push_back(buildFlowStatement(expressionNode->m_flowBody, child));
            }
        }
    } else {
        expressionNode->m_flowBody->m_expressionBody = true;
        expressionNode->m_flowBody->m_body = buildFlowValue(expressionNode->m_flowBody, body);
    }

    ProgramNode* program = flowProgramNode(parent);
    if ( program ){
        program->setHasFlows(true);
    }

    checkFlowExpressionShape(expressionNode);

    return expressionNode;
}

FlowFunctionDeclarationNode* BaseNode::visitFlowFunctionDeclaration(BaseNode* parent, const TSNode& node){
    FlowFunctionDeclarationNode* fnode = parent->addCreatedChild(new FlowFunctionDeclarationNode(node));
    for ( auto c : extractPrecedingComments(node) ){
        fnode->addChild(c);
        fnode->m_precedingComments.push_back(c);
    }

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Flow function name is null.");
    fnode->m_name = fnode->addCreatedChild(IdentifierNode::createNonEmpty(name, fnode));
    addToDeclarations(parent, fnode->m_name, Occurrence::SpaceValue);

    fnode->m_flow = visitFlowExpression(fnode, node);
    return fnode;
}

BaseNode* BaseNode::visit(BaseNode *parent, const TSNode &node){
    BaseNode* recognized = visitRecognized(parent, node);
    if ( recognized ){
        return recognized;
    } else {
        visitChildren(parent, node);
    }
    return nullptr;
}

void BaseNode::visitChildren(BaseNode *parent, const TSNode &node){
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        visit(parent, child);
    }
}

ImportNode* BaseNode::visitImport(BaseNode *parent, const TSNode &node){
    ImportNode* importNode = parent->addCreatedChild(new ImportNode(node));
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        assertError(importNode, child, "Import segment error.");
        if ( strcmp(ts_node_type(child), "import_as" ) == 0 ){
            TSNode aliasChild = ts_node_child(child, 1);
            IdentifierNode* in = importNode->addCreatedChild(IdentifierNode::createNonEmpty(aliasChild, importNode));
            importNode->m_importAs = in;
        } else if ( strcmp(ts_node_type(child), "import_path") == 0 ){
            visitImportPath(importNode, child);
        }
    }
    return importNode;
}

JsImportNode* BaseNode::visitJsImport(BaseNode *parent, const TSNode &node){
    JsImportNode* importNode = parent->addCreatedChild(new JsImportNode(node));
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "identifier" ) == 0 ){
            IdentifierNode* name = importNode->addCreatedChild(IdentifierNode::createNonEmpty(child, importNode));
            importNode->m_importNames.push_back(JsImportName(name));
            addToDeclarations(parent, name, Occurrence::SpaceValue | Occurrence::SpaceType);
        } else if ( strcmp(ts_node_type(child), "string" ) == 0 ){
            StringNode* path = importNode->addCreatedChild(new StringNode(child));
            importNode->m_importPath = path;
        } else if ( strcmp(ts_node_type(child), "{") == 0 ){
            importNode->setIsObjectImport(true);
        } else if ( strcmp(ts_node_type(child), "}") == 0 ){
            importNode->setIsObjectImport(true);
        }
    }
    return importNode;
}

IdentifierNode* BaseNode::visitIdentifier(BaseNode *parent, const TSNode &node){
    IdentifierNode* identifierNode = parent->addCreatedChild(IdentifierNode::createNonEmpty(node, parent));
    addUsedIdentifier(parent, identifierNode);
    visitChildren(identifierNode, node);
    return identifierNode;
}

IdentifierNode* BaseNode::visitPropertyIdentifier(BaseNode *parent, const TSNode &node){
    IdentifierNode* identifierNode = parent->addCreatedChild(IdentifierNode::createNonEmpty(node, parent));
    visitChildren(identifierNode, node);
    return identifierNode;
}

ImportPathNode* BaseNode::visitImportPath(BaseNode *parent, const TSNode &node){
    ImportPathNode* ipnode = parent->addCreatedChild(new ImportPathNode(node));

    uint32_t count = ts_node_child_count(node);

    TSNode n = ts_node_child_by_field_name(node, "relative", 8);
    if ( !ts_node_is_null(n) )
        ipnode->m_isRelative = true;

    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "import_path_segment") == 0 ){
            ImportPathSegmentNode* in = ipnode->addCreatedChild(new ImportPathSegmentNode(child));
            ipnode->m_segments.push_back(in);
        } else if ( strcmp(ts_node_type(child), "import_path_scope_segment") == 0 ){
            ipnode->m_hasScope = true;
            ImportPathSegmentNode* in = ipnode->addCreatedChild(new ImportPathSegmentNode(child));
            ipnode->m_segments.push_back(in);
        }
    }
    return ipnode;
}

ComponentDeclarationNode* BaseNode::visitComponentDeclaration(BaseNode *parent, const TSNode &node){
    ComponentDeclarationNode* enode = parent->addCreatedChild(new ComponentDeclarationNode(node));
    for (auto c : extractPrecedingComments(node)) {
        enode->addChild(c);
        enode->m_precedingComments.push_back(c);
    }

    TSNode name = nodeChildByFieldName(node, "name");
    if ( !ts_node_is_null(name) ){
        enode->m_name = enode->addCreatedChild(IdentifierNode::createNonEmpty(name, enode));
        addToDeclarations(parent, enode->m_name, Occurrence::SpaceValue | Occurrence::SpaceType);
    }

    TSNode heritage = nodeChildByFieldName(node, "heritage");
    if ( !ts_node_is_null(heritage) ){
        if ( strcmp(ts_node_type(heritage), "component_heritage") == 0 ){
            uint32_t heritageCount = ts_node_child_count(heritage);
            for ( uint32_t j = 0; j < heritageCount; ++j ){
                TSNode heritageSegment = ts_node_child(heritage, j);
                if ( strcmp(ts_node_type(heritageSegment), "identifier" ) == 0 ){
                    IdentifierNode* heritageSegmentNode = enode->addCreatedChild(IdentifierNode::createNonEmpty(heritageSegment, enode));
                    enode->m_heritage.push_back(heritageSegmentNode);
                }
            }
        } else if ( strcmp(ts_node_type(heritage), "component_short_heritage") == 0 ){
            uint32_t heritageCount = ts_node_child_count(heritage);
            for ( uint32_t j = 0; j < heritageCount; ++j ){
                TSNode heritageSegment = ts_node_child(heritage, j);
                if ( strcmp(ts_node_type(heritageSegment), "identifier" ) == 0 ){
                    IdentifierNode* heritageSegmentNode = enode->addCreatedChild(IdentifierNode::createNonEmpty(heritageSegment, enode));
                    enode->m_heritage.push_back(heritageSegmentNode);
                }
            }
        }

        if ( enode->m_heritage.size() > 0 ){
            addUsedIdentifier(enode, Occurrence(enode->m_heritage[0], Occurrence::SpaceValue | Occurrence::SpaceType));
        }
    }


    TSNode identifier = nodeChildByFieldName(node, "id");
    if ( !ts_node_is_null(identifier) ){
        if ( ts_node_named_child_count(identifier) > 0 ){
            TSNode idChild = ts_node_child(identifier, 1);
            enode->m_id = enode->addCreatedChild(IdentifierNode::createNonEmpty(idChild, enode));
            addToDeclarations(parent, enode->m_id, Occurrence::SpaceValue);
        }
    }

    TSNode body = nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Component declaration body is null.");
    enode->m_body = enode->addCreatedChild(new ComponentBodyNode(body));
    visitChildren(enode->m_body, body);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "ERROR") == 0 ){
            assertError(enode, child, "Unexpected component syntax.");
        }
    }

    return enode;
}

ComponentBodyNode* BaseNode::visitComponentBody(BaseNode *parent, const TSNode &node){
    ComponentBodyNode* enode = parent->addCreatedChild(new ComponentBodyNode(node));
    visitChildren(enode, node);
    return enode;
}

NewComponentExpressionNode* BaseNode::visitNewComponentExpression(BaseNode *parent, const TSNode &node){
    NewComponentExpressionNode* enode = nullptr;

    int nameSpace = Occurrence::SpaceValue;

    if (parent->isNodeType<ComponentInstanceStatementNode>()
        && parent->parent()
        && parent->parent()->isNodeType<ProgramNode>() )
    {
        enode = parent->addCreatedChild(new RootNewComponentExpressionNode(node));
        nameSpace = Occurrence::SpaceValue | Occurrence::SpaceType;
    } else {
        BaseNode* p = parent;
        while (p && dynamic_cast<JsBlockNode*>(p) == nullptr){
            p = p->parent();
        }
        if (p && p->isNodeType<JsBlockNode>() ){
            enode = parent->addCreatedChild(new RootNewComponentExpressionNode(node));
        } else {
            enode = parent->addCreatedChild(new NewComponentExpressionNode(node));
        }
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "identifier") == 0 ){
            auto iden = enode->addCreatedChild(IdentifierNode::createNonEmpty(child, enode));
            enode->m_name.push_back(iden);
            if ( enode->m_name.size() == 1 )
                addUsedIdentifier(enode, Occurrence(iden, nameSpace));
        } else if ( strcmp(ts_node_type(child), "nested_identifier") == 0 ){
            enode->m_name = BaseNode::fromNestedIdentifier(enode, child);
            for( IdentifierNode* name : enode->m_name )
                enode->addChild(name);
            if ( enode->m_name.size() > 0 ){
                addUsedIdentifier(enode, Occurrence(enode->m_name.front(), nameSpace));
            }
        } else if ( strcmp(ts_node_type(child), "new_component_body") == 0 ){
            enode->m_body = enode->addCreatedChild(new ComponentBodyNode(child));
            visitChildren(enode->m_body, child);
        } else if ( strcmp(ts_node_type(child), "arguments") == 0 ){
            enode->m_arguments = enode->addCreatedChild(new ArgumentsNode(child));
            visitChildren(enode->m_arguments, child);
            for ( auto child : enode->m_arguments->children() ){
                if (child->isNodeType<IdentifierNode>() ){
                    addUsedIdentifier(enode->m_arguments, child->as<IdentifierNode>());
                }
            }

        } else if ( strcmp(ts_node_type(child), "component_identifier") == 0 ){
            TSNode id = ts_node_child(child, 1);
            if (strcmp(ts_node_type(id), "identifier") != 0)
                continue;
            enode->m_id = enode->addCreatedChild(IdentifierNode::createNonEmpty(id, enode));
        }
    }

    // for id components
    BaseNode* p = parent;

    if ( enode->m_id ){
        while (p && !enode->isNodeType<RootNewComponentExpressionNode>()){
            if (p->isNodeType<ComponentBodyNode>() ){
                auto body = p->as<ComponentBodyNode>();
                if (body->parent()->isNodeType<ComponentDeclarationNode>() )
                {
                    auto decl = body->parent()->as<ComponentDeclarationNode>();
                    decl->pushToIdComponents(enode);
                    addToDeclarations(body, enode->m_id, Occurrence::SpaceValue);
                    break;
                }
            } else if (p->isNodeType<RootNewComponentExpressionNode>() ){
                auto rnce = p->as<RootNewComponentExpressionNode>();
                rnce->m_idComponents.push_back(enode);
                addToDeclarations(rnce, enode->m_id, Occurrence::SpaceValue);
                break;
            }

            p = p->parent();
        }
    }


    // for default property components
    p = parent;
    if (p->isNodeType<ComponentBodyNode>() )
    {
        auto body = p->as<ComponentBodyNode>();
        if (body->parent()->isNodeType<ComponentDeclarationNode>() )
        {
            auto decl = body->parent()->as<ComponentDeclarationNode>();
            decl->pushToDefault(enode);

        }
        else if (body->parent()->isNodeType<NewComponentExpressionNode>()  || body->parent()->isNodeType<RootNewComponentExpressionNode>() )
        {
            auto expr = body->parent()->as<NewComponentExpressionNode>();
            expr->pushToDefault(enode);
        }

    }

    return enode;

}

ComponentInstanceStatementNode* BaseNode::visitComponentInstanceStatement(BaseNode *parent, const TSNode &node){
    ComponentInstanceStatementNode* enode = parent->addCreatedChild(new ComponentInstanceStatementNode(node));
    for (auto c : extractPrecedingComments(node)) {
        enode->addChild(c);
        enode->m_precedingComments.push_back(c);
    }
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        assertError(parent, child, "Unexpected token.");
        if ( strcmp(ts_node_type(child), "component_instance") == 0 ){
            TSNode id = ts_node_child(child, 1);
            if (strcmp(ts_node_type(id), "identifier") != 0)
                continue;
            enode->m_name = enode->addCreatedChild(IdentifierNode::createNonEmpty(id, enode));
            addToDeclarations(parent, enode->m_name, Occurrence::SpaceValue);
        } else if ( strcmp(ts_node_type(child), "new_component_expression") == 0 ){
            visitNewComponentExpression(enode, child);
        }
    }

    return enode;
}

ConstructorInitializerAssignmentNode::ConstructorInitializerAssignmentNode(const TSNode& node)
    : BaseNode(node, ConstructorInitializerAssignmentNode::nodeInfo())
    , m_name(nullptr)
    , m_expression(nullptr)
{}

ConstructorInitializerNode* BaseNode::visitConstructorInitializer(BaseNode* parent, const TSNode& node){
    ConstructorInitializerNode* enode = parent->addCreatedChild(new ConstructorInitializerNode(node));

    BaseNode* p = parent;
    ConstructorDefinitionNode* constructorDefinition = nullptr;
    while ( p ){
        if (
            p->isNodeType<ExpressionStatementNode>() ||
            p->isNodeType<JsBlockNode>()
        ){
            p = p->parent();
        } else if ( p->isNodeType<ConstructorDefinitionNode>() ){
            constructorDefinition = p->as<ConstructorDefinitionNode>();
            break;
        } else {
            throwError(parent, node, "Constructor initializer must be called directly from the constructor body.");
        }
    }

    constructorDefinition->m_initializer = enode;

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        assertError(parent, child, "Constructor initializer not declared properly.");
        if ( strcmp(ts_node_type(child), "constructor_initializer_assignment") == 0 ){
            TSNode assignmentName = BaseNode::nodeChildByFieldName(child, "name");
            BaseNode::assertValid(enode, assignmentName, "Constructor initializer assignment name missing.");
            TSNode assignmentExpression = BaseNode::nodeChildByFieldName(child, "value");
            BaseNode::assertValid(enode, assignmentExpression, "Constructor initializer assignment value not set.");

            auto assignment = enode->addCreatedChild(new ConstructorInitializerAssignmentNode(child));
            assignment->m_name = assignment->addCreatedChild(IdentifierNode::createNonEmpty(assignmentName, assignment));
            assignment->m_expression = assignment->addCreatedChild(new BindableExpressionNode(assignmentExpression));
            enode->m_assignments.push_back(assignment);

            visit(assignment->m_expression, assignmentExpression);
        }
    }

    return enode;
}

PropertyDeclarationNode* BaseNode::visitPropertyDeclaration(BaseNode *parent, const TSNode &node){
    PropertyDeclarationNode* pnode = parent->addCreatedChild(new PropertyDeclarationNode(node));
    for (auto c : extractPrecedingComments(node)) {
        pnode->addChild(c);
        pnode->m_precedingComments.push_back(c);
    }

    TSNode propName = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, propName, "Property name is null.");
    pnode->m_name = pnode->addCreatedChild(IdentifierNode::createNonEmpty(propName, pnode));
    TSNode propType = BaseNode::nodeChildByFieldName(node, "type");
    if ( !ts_node_is_null(propType) ){
        pnode->m_type = pnode->addCreatedChild(TypeNode::create(propType, pnode));
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_assignment_expression") == 0 ){
            pnode->m_expression = pnode->addCreatedChild(new BindableExpressionNode(child));
            visitChildren(pnode->m_expression, child);
        } else if (strcmp(ts_node_type(child), "statement_block") == 0) {
            pnode->m_statementBlock = pnode->addCreatedChild(new JsBlockNode(child));
            visitChildren(pnode->m_statementBlock, child);
        } else if ( strcmp(ts_node_type(child), "=") == 0 ){
            pnode->m_isBindingAssignment = false;
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->pushToProperties(pnode);
    }

    if (parent->parent() && (parent->parent()->isNodeType<NewComponentExpressionNode>() ||
        parent->parent()->isNodeType<RootNewComponentExpressionNode>() ))
    {
        parent->parent()->as<NewComponentExpressionNode>()->pushToProperties(pnode);
    }
    return pnode;
}

StaticPropertyDeclarationNode* BaseNode::visitStaticPropertyDeclaration(BaseNode *parent, const TSNode &node){
    StaticPropertyDeclarationNode* pnode = parent->addCreatedChild(new StaticPropertyDeclarationNode(node));
    for (auto c : extractPrecedingComments(node)) {
        pnode->addChild(c);
        pnode->m_precedingComments.push_back(c);
    }

    TSNode propName = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, propName, "Property name is null.");
    pnode->m_name = pnode->addCreatedChild(IdentifierNode::createNonEmpty(propName, pnode));
    TSNode propType = BaseNode::nodeChildByFieldName(node, "type");
    if ( !ts_node_is_null(propType) ){
        pnode->m_type = pnode->addCreatedChild(TypeNode::create(propType, pnode));
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_expression_initializer" ) == 0 ){
            uint32_t initCount = ts_node_child_count(child);
            if ( initCount == 2 ){
                TSNode initChild = ts_node_child(child, 1);
                pnode->m_expression = pnode->addCreatedChild(new BindableExpressionNode(initChild));
                visitChildren(pnode->m_expression, initChild);
            }
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        auto componentDeclaration = parent->parent()->as<ComponentDeclarationNode>();
        if ( componentDeclaration->isAnonymous() ){
            throwError(pnode, node, "Cannot declare static members for an anonymous component.");
        }

        componentDeclaration->pushToStaticProperties(pnode);
    }

    if (parent->parent() && (parent->parent()->isNodeType<NewComponentExpressionNode>() ||
        parent->parent()->isNodeType<RootNewComponentExpressionNode>() ) )
    {
        throwError(pnode, node, "Cannot declare static members on a new expression.");
    }
    return pnode;
}

BindableExpressionNode* BaseNode::visitBindableExpression(BaseNode *parent, const TSNode &node){
    BindableExpressionNode* enode = parent->addCreatedChild(new BindableExpressionNode(node));
    visitChildren(enode, node);
    return enode;
}

MemberExpressionNode* BaseNode::visitMemberExpression(BaseNode *parent, const TSNode &node){
    MemberExpressionNode* enode = parent->addCreatedChild(new MemberExpressionNode(node));
    visitChildren(enode, node);

    // import keyword fix
    uint32_t count = ts_node_child_count(node);
    if ( count > 0 ){
        TSNode child = ts_node_child(node, 0);
        if ( strcmp(ts_node_type(child), "import") == 0 ){
            enode->m_children.insert(enode->m_children.begin(), IdentifierNode::createNonEmpty(child, enode));
        }
    }

    if ( parent->isNodeType<CallExpressionNode>() ){
        
        MemberExpressionNode* head = enode;
        while ( !head->children().empty() &&
                head->children()[0]->isNodeType<CallExpressionNode>() )
        {
            BaseNode* innerCall = head->children()[0];
            if ( innerCall->children().empty() ||
                 !innerCall->children()[0]->isNodeType<MemberExpressionNode>() )
                return enode;
            head = innerCall->children()[0]->as<MemberExpressionNode>();
        }
        if ( !head->children().empty() &&
             head->children()[0]->isNodeType<MemberExpressionNode>() ){
            enode = head->children()[0]->as<MemberExpressionNode>();
        } else {
            return enode;
        }
    }

    if ( enode->children().size() > 0 ){
        BaseNode* p = parent;
        BaseNode* prevP = enode;

        while (p){
            if (p->isNodeType<MemberExpressionNode>() ) break;
            if (p->isNodeType<FunctionDeclarationNode>() ) break;
            if (p->isNodeType<ClassDeclarationNode>() ) break;
            if (p->isNodeType<ArrowFunctionNode>() ) break;
            if (p->isNodeType<NewComponentExpressionNode>() ) break;
            if (p->isNodeType<ComponentDeclarationNode>() ) break;
            if (p->isNodeType<ListenerDeclarationNode>() ) break;

            if (p->isNodeType<AssignmentExpressionNode>()  &&
                p->as<AssignmentExpressionNode>()->left() == prevP)
            {
                break;
            }

            if (p->isNodeType<PropertyDeclarationNode>() ){
                BaseNode* binding = resolveBindingBase(enode);
                if ( !binding )
                    break;
                p->as<PropertyDeclarationNode>()->pushToBindings(binding);
                break;
            }
            if (p->isNodeType<PropertyAssignmentNode>() ){
                BaseNode* binding = resolveBindingBase(enode);
                if ( !binding )
                    break;
                p->as<PropertyAssignmentNode>()->pushToBindings(binding);
                break;
            }
            prevP = p;
            p = p->parent();
        }
    }
    return enode;
}

BaseNode* BaseNode::resolveBindingBase(BaseNode* node){
    if ( !node->isNodeType<MemberExpressionNode>() || node->children().empty() )
        return nullptr;
    BaseNode* child = node->children()[0];
    if ( child->isNodeType<MemberExpressionNode>() || child->isNodeType<IdentifierNode>() )
        return node;
    // For chains broken by function calls (e.g. this.m.a('x').b.c) drill down
    // through call/member pairs to find the leading member chain (this.m).
    MemberExpressionNode* head = node->as<MemberExpressionNode>();
    while ( !head->children().empty() &&
            head->children()[0]->isNodeType<CallExpressionNode>() )
    {
        BaseNode* innerCall = head->children()[0];
        if ( innerCall->children().empty() ||
             !innerCall->children()[0]->isNodeType<MemberExpressionNode>() )
            return nullptr;
        head = innerCall->children()[0]->as<MemberExpressionNode>();
    }
    if ( !head->children().empty() &&
         head->children()[0]->isNodeType<MemberExpressionNode>() )
        return head->children()[0];
    return nullptr;
}

SubscriptExpressionNode* BaseNode::visitSubscriptExpression(BaseNode *parent, const TSNode &node){
    SubscriptExpressionNode* enode = parent->addCreatedChild(new SubscriptExpressionNode(node));
    visitChildren(enode, node);
    return enode;
}

PropertyAssignmentNode* BaseNode::visitPropertyAssignment(BaseNode *parent, const TSNode &node){
    PropertyAssignmentNode* pnode = parent->addCreatedChild(new PropertyAssignmentNode(node));
    for (auto c : extractPrecedingComments(node)) {
        pnode->addChild(c);
        pnode->m_precedingComments.push_back(c);
    }
    uint32_t count = ts_node_child_count(node);

    TSNode propertyName = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, propertyName, "Failed to find property name.");

    auto propertyIdentifiers = fromNestedIdentifier(pnode, propertyName);
    for ( IdentifierNode* inode : propertyIdentifiers ){
        pnode->addChild(inode);
    }
    pnode->m_property = propertyIdentifiers;

    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_assignment_expression") == 0 ){
            pnode->m_expression = pnode->addCreatedChild(new BindableExpressionNode(child));
            visitChildren(pnode->m_expression, child);
        } else if (strcmp(ts_node_type(child),"statement_block") == 0) {
            pnode->m_statementBlock = pnode->addCreatedChild(new JsBlockNode(child));
            visitChildren(pnode->m_statementBlock, child);
        } else if ( strcmp(ts_node_type(child), "=") == 0 ){
            pnode->m_isBindingAssignment = false;
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->pushToAssignments(pnode);
    }

    if (parent->parent() && (parent->parent()->isNodeType<NewComponentExpressionNode>() ||
                             parent->parent()->isNodeType<RootNewComponentExpressionNode>() ))
    {
        parent->parent()->as<NewComponentExpressionNode>()->pushToAssignments(pnode);
    }
    return pnode;
}

IdentifierNode* BaseNode::visitIdentifierAssignment(BaseNode *parent, const TSNode &node){
    BaseNode* componentParent = parent ? parent->parent() : nullptr;
    if ( !componentParent )
        return nullptr;

    TSNode idChild = ts_node_child(node, 2);

    if ( componentParent->isNodeType<ComponentDeclarationNode>() ){
        ComponentDeclarationNode* cdn = componentParent->as<ComponentDeclarationNode>();
        cdn->m_id = parent->addCreatedChild(IdentifierNode::createNonEmpty(idChild, parent));
        // provide id to outside scope
        addToDeclarations(componentParent, cdn->m_id, Occurrence::SpaceValue);
        return cdn->m_id;
    } else if ( componentParent->isNodeType<NewComponentExpressionNode>() ||
                componentParent->isNodeType<RootNewComponentExpressionNode>() )
    {
        NewComponentExpressionNode* ncen = componentParent->as<NewComponentExpressionNode>();
        ncen->m_id = parent->addCreatedChild(IdentifierNode::createNonEmpty(idChild, parent));
        // provide id to outside scope
        addToDeclarations(componentParent, ncen->m_id, Occurrence::SpaceValue);
        return ncen->m_id;
    }

    return nullptr;
}

EventDeclarationNode* BaseNode::visitEventDeclaration(BaseNode *parent, const TSNode &node){
    EventDeclarationNode* enode = parent->addCreatedChild(new EventDeclarationNode(node));

    for (auto c : extractPrecedingComments(node)) {
        enode->addChild(c);
        enode->m_precedingComments.push_back(c);
    }

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Event name is null.");
    enode->m_name = parent->addCreatedChild(IdentifierNode::createNonEmpty(name, parent));

    TSNode parameters = nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Event parameters are null.");

    enode->m_parameters = BaseNode::scanFormalTypeParameters(enode, parameters);

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->m_events.push_back(enode);
    }

    if (parent->parent() && parent->parent()->canCast<NewComponentExpressionNode>() ){
        parent->parent()->as<NewComponentExpressionNode>()->m_events.push_back(enode);
    }
    return enode;
}

ListenerDeclarationNode* BaseNode::visitListenerDeclaration(BaseNode *parent, const TSNode &node){
    ListenerDeclarationNode* enode = parent->addCreatedChild(new ListenerDeclarationNode(node));
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_identifier") == 0 ){
            enode->m_name = enode->addCreatedChild(IdentifierNode::createNonEmpty(child, enode));
        } else if ( strcmp(ts_node_type(child), "async") == 0 ){
            enode->m_async = true;
        } else if ( strcmp(ts_node_type(child), "formal_parameters") == 0 ){
            enode->m_parameters = BaseNode::scanFormalParameters(enode, child);
        }
    }

    TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Failed to find listener body.");

    if ( strcmp(ts_node_type(body), "statement_block") == 0 ){
        enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
        visitChildren(enode->m_body, body);
    } else {
        enode->m_bodyExpression = enode->addCreatedChild(new ExpressionNode(body));
        visitChildren(enode->m_bodyExpression, body);
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->m_listeners.push_back(enode);
    }
    if (parent->parent() && parent->parent()->canCast<NewComponentExpressionNode>()){
        parent->parent()->as<NewComponentExpressionNode>()->m_listeners.push_back(enode);
    }

    if ( enode->m_parameters ){
        if (enode->m_body ){
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addParameterDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]);
            }
        } else {
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addParameterDeclarations(enode, enode->m_parameters->m_parameters[i]);
            }
        }
    }
    return enode;
}

MethodDefinitionNode* BaseNode::visitMethodDefinition(BaseNode *parent, const TSNode &node){
    MethodDefinitionNode* cnode = parent->addCreatedChild(new MethodDefinitionNode(node));
    for (auto c : extractPrecedingComments(node)) {
        cnode->addChild(c);
        cnode->m_precedingComments.push_back(c);
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_identifier") == 0 ){
            cnode->m_name = cnode->addCreatedChild(IdentifierNode::createNonEmpty(child, cnode));
        } else if ( strcmp(ts_node_type(child), "formal_parameters") == 0 ){
            cnode->m_parameters = BaseNode::scanFormalParameters(cnode, child);
        } else if ( strcmp(ts_node_type(child), "statement_block") == 0 ){
            cnode->m_body = cnode->addCreatedChild(new JsBlockNode(child));
            visitChildren(cnode->m_body, child);
        }
    }
    return cnode;
}

TypedMethodDeclarationNode* BaseNode::visitTypedMethodDeclaration(BaseNode *parent, const TSNode &node){
    TypedMethodDeclarationNode* cnode = parent->addCreatedChild(new TypedMethodDeclarationNode(node));
    for (auto c : extractPrecedingComments(node)) {
        cnode->addChild(c);
        cnode->m_precedingComments.push_back(c);
    }

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Function name is null.");
    cnode->m_name = cnode->addCreatedChild(IdentifierNode::createNonEmpty(name, cnode));

    TSNode parameters = nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Function parameters are null.");

    cnode->m_parameters = BaseNode::scanFormalTypeParameters(cnode, parameters);

    TSNode returnType = nodeChildByFieldName(node, "return");
    if ( !ts_node_is_null(returnType) ){
        cnode->m_returnType = cnode->addCreatedChild(TypeNode::create(returnType, cnode));
    }

    TSNode body = nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Function body is null.");

    cnode->m_body = cnode->addCreatedChild(new JsBlockNode(body));
    visitChildren(cnode->m_body, body);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){

        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "static") == 0 ){
            cnode->setStatic(true);
        } else if ( strcmp(ts_node_type(child), "async") == 0 ){
            cnode->setAsync(true);
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->m_methods.push_back(cnode);
    }
    if (parent->parent() && parent->parent()->canCast<NewComponentExpressionNode>()){
        parent->parent()->as<NewComponentExpressionNode>()->m_methods.push_back(cnode);
    }

    if (cnode->m_body && cnode->m_parameters){
        for (size_t i = 0; i != cnode->m_parameters->m_parameters.size(); ++i){
            addParameterDeclarations(cnode->m_body, cnode->m_parameters->m_parameters[i]);
        }
    }
    return cnode;
}

TypedFunctionDeclarationNode* BaseNode::visitTypedFunctionDeclaration(BaseNode *parent, const TSNode &node){
    TypedFunctionDeclarationNode* cnode = parent->addCreatedChild(new TypedFunctionDeclarationNode(node));
    if ( strcmp(ts_node_type(node), "declared_typed_function_declaration") == 0 ){
        cnode->setDeclared(true);
    }
    for (auto c : extractPrecedingComments(node)) {
        cnode->addChild(c);
        cnode->m_precedingComments.push_back(c);
    }

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Function name is null.");
    cnode->m_name = cnode->addCreatedChild(IdentifierNode::createNonEmpty(name, cnode));
    addToDeclarations(parent, cnode->m_name, Occurrence::SpaceValue);

    TSNode typeParameters = nodeChildByFieldName(node, "type_parameters");
    if ( !ts_node_is_null(typeParameters) ){
        cnode->m_typeParameters = cnode->addCreatedChild(TypeParameterListNode::create(typeParameters, cnode));

        if ( cnode->m_typeParameters->names().empty() ){
            BaseNode::throwError(cnode, typeParameters, "Type parameter list cannot be empty.");
        }
    }

    TSNode meta = nodeChildByFieldName(node, "meta");
    if ( !ts_node_is_null(meta) ){
        cnode->setMeta(true);
    }

    TSNode parameters = nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Function parameters are null.");

    cnode->m_parameters = BaseNode::scanFormalTypeParameters(cnode, parameters, cnode->isMeta());

    TSNode returnType = nodeChildByFieldName(node, "return");
    if ( !ts_node_is_null(returnType) ){
        cnode->m_returnType = cnode->addCreatedChild(TypeNode::create(returnType, cnode, cnode->isMeta()));
    }

    TSNode body = nodeChildByFieldName(node, "body");
    if ( !ts_node_is_null(body) ){
        cnode->m_body = cnode->addCreatedChild(new JsBlockNode(body));
        visitChildren(cnode->m_body, body);
    } else if ( !cnode->isDeclared() ){
        assertValid(parent, body, "Function body is null.");
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "async") == 0 ){
            cnode->setAsync(true);
        }
    }

    if (cnode->m_body && cnode->m_parameters){
        for (size_t i = 0; i != cnode->m_parameters->m_parameters.size(); ++i){
            addParameterDeclarations(cnode->m_body, cnode->m_parameters->m_parameters[i]);
        }
    }
    return cnode;
}

void TypedFunctionDeclarationNode::collectImports(const std::string &source, std::vector<Occurrence> &references, ConversionContext *ctx){
    if ( m_typeParameters ){
        std::set<std::string> seenNames;
        for ( IdentifierNode* typeParameterName : m_typeParameters->names() ){
            const std::string name = slice(source, typeParameterName);
            if ( !seenNames.insert(name).second ){
                throwError(
                    typeParameterName,
                    typeParameterName->current(),
                    Utf8("Duplicate type parameter '%'.").format(name).data()
                );
            }
        }
    }

    JsBlockNode::collectImports(source, references, ctx);
}

PropertyAccessorDeclarationNode* BaseNode::visitPropertyAccessorDeclaration(BaseNode *parent, const TSNode &node){
    PropertyAccessorDeclarationNode* enode = parent->addCreatedChild(new PropertyAccessorDeclarationNode(node));

    uint32_t nodeCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < nodeCount; ++i){
        if (strcmp(ts_node_type(ts_node_child(node, i)), "get") == 0){
            enode->m_access = PropertyAccessorDeclarationNode::Getter;
        } else if (strcmp(ts_node_type(ts_node_child(node, i)), "set") == 0){
            enode->m_access = PropertyAccessorDeclarationNode::Setter;
        }
    }

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Accessor name is null.");
    enode->m_name = enode->addCreatedChild(IdentifierNode::createNonEmpty(name, enode));

    if ( enode->m_access == PropertyAccessorDeclarationNode::Setter ){
        TSNode parameters = nodeChildByFieldName(node, "parameters");
        assertValid(parent, parameters, "Set parameters are null.");
        enode->m_parameters = BaseNode::scanFormalTypeParameters(enode, parameters);
    }

    TSNode body = nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Accessor body is null.");

    enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
    visitChildren(enode->m_body, body);

    if (enode->m_body && enode->m_parameters ){
        for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
            auto param = enode->m_parameters->m_parameters[i];
            addParameterDeclarations(enode->m_body, param);
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->pushToPropertyAccessors(enode);
    }
    return enode;
}

PublicFieldDeclarationNode* BaseNode::visitPublicFieldDeclaration(BaseNode *parent, const TSNode &node){
    PublicFieldDeclarationNode* enode = parent->addCreatedChild(new PublicFieldDeclarationNode(node));
    visitChildren(enode, node);
    return enode;
}

JsBlockNode* BaseNode::visitJsScope(BaseNode *parent, const TSNode &node){
    JsBlockNode* enode = parent->addCreatedChild(new JsBlockNode(node));
    visitChildren(enode, node);
    return enode;
}

NumberNode* BaseNode::visitNumber(BaseNode *parent, const TSNode &node)
{
    NumberNode* nnode = parent->addCreatedChild(new NumberNode(node));
    return nnode;
}

ConstructorDefinitionNode* BaseNode::visitConstructorDefinition(BaseNode *parent, const TSNode &node)
{
    ConstructorDefinitionNode* cnode = parent->addCreatedChild(new ConstructorDefinitionNode(node));
    for (auto c : extractPrecedingComments(node)) {
        cnode->addChild(c);
        cnode->m_precedingComments.push_back(c);
    }
    // visitChildren(cdnode, node);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "statement_block") == 0 ){
            cnode->m_body = cnode->addCreatedChild(new JsBlockNode(child));
            visitChildren(cnode->m_body, child);
        } else if ( strcmp(ts_node_type(child), "formal_type_parameters") == 0 ){
            TSNode& parameters = child;

            cnode->m_parameters = cnode->addCreatedChild(new ParameterListNode(parameters));
            uint32_t parameterCount = ts_node_child_count(parameters);

            for (uint32_t pc = 0; pc < parameterCount; ++pc){
                TSNode ftpc = ts_node_child(parameters, pc);
                assertError(parent, ftpc, "Constructor parameter not declared properly.");
                if (strcmp(ts_node_type(ftpc), "formal_type_parameter") == 0){

                    if ( ts_node_child_count(ftpc) > 0 ){
                        TSNode typeParameter = ts_node_child(ftpc, 0);

                        TSNode parameterName = BaseNode::nodeChildByFieldName(typeParameter, "name");
                        if (ts_node_is_null(parameterName)) {
                            parameterName = BaseNode::nodeChildByFieldName(typeParameter, "pattern");
                        }
                        assertValid(cnode, parameterName, "Parameter name is null.");
                        TSNode parameterType = BaseNode::nodeChildByFieldName(typeParameter, "type");
                        assertValid(cnode, parameterType, "Parameter type is null.");

                        ParameterNode* paramNode = nullptr;
                        if ( strcmp(ts_node_type(typeParameter), "required_type_parameter") == 0 ){
                            paramNode = cnode->addCreatedChild(new ParameterNode(typeParameter, IdentifierNode::createNonEmpty(parameterName, cnode), false));
                        } else if ( strcmp(ts_node_type(typeParameter), "optional_type_parameter") == 0){
                            paramNode = cnode->addCreatedChild(new ParameterNode(typeParameter, IdentifierNode::createNonEmpty(parameterName, cnode), true));
                        }

                        if ( paramNode ){
                            paramNode->m_type = paramNode->addCreatedChild(TypeNode::create(parameterType, paramNode));
                            TSNode parameterValue = BaseNode::nodeChildByFieldName(typeParameter, "value");
                            if ( !ts_node_is_null(parameterValue) ){
                                paramNode->m_defaultValue = paramNode->addCreatedChild(new ExpressionNode(parameterValue));
                                visit(paramNode->m_defaultValue, parameterValue);
                            }
                            cnode->m_parameters->m_parameters.push_back(paramNode);
                        }
                    }
                }
            }
        } else
            visit(cnode, child);
    }

    if (parent->isNodeType<ComponentBodyNode>() ){
        parent->as<ComponentBodyNode>()->setConstructor(cnode);
    }

    if (cnode->m_body){
        for (size_t i = 0; i != cnode->m_parameters->parameters().size(); ++i){
            addParameterDeclarations(cnode->m_body, cnode->m_parameters->m_parameters[i]);
        }
    }
    return cnode;
}

ExpressionStatementNode* BaseNode::visitExpressionStatement(BaseNode *parent, const TSNode &node)
{
    ExpressionStatementNode* esnode = parent->addCreatedChild(new ExpressionStatementNode(node));
    visitChildren(esnode, node);
    return esnode;
}

AssignmentExpressionNode* BaseNode::visitAssignmentExpression(BaseNode *parent, const TSNode &node){
    AssignmentExpressionNode* esnode = parent->addCreatedChild(new AssignmentExpressionNode(node));
    visitChildren(esnode, node);
    return esnode;
}

CallExpressionNode* BaseNode::visitCallExpression(BaseNode *parent, const TSNode &node)
{
    CallExpressionNode* enode = parent->addCreatedChild(new CallExpressionNode(node));

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "arguments") == 0 ){
            enode->m_arguments = enode->addCreatedChild(new ArgumentsNode(child));
            visitChildren(enode->m_arguments, child);

            for ( BaseNode* childNode : enode->m_arguments->children() ){
                if ( childNode->isNodeType<IdentifierNode>() ){
                    addUsedIdentifier(enode, childNode->as<IdentifierNode>());
                }
            }

        } else if ( strcmp(ts_node_type(child), "super") == 0 ) {
            enode->isSuper = true;
            if (parent->parent() && parent->parent()->parent()
                    && parent->parent()->parent()->isNodeType<ConstructorDefinitionNode>() )
            {
                parent->parent()->parent()->as<ConstructorDefinitionNode>()->setSuperCall(enode);
            }
        } else {
            visit(enode, child);
        }
    }
    return enode;
}

NewTaggedComponentExpressionNode* BaseNode::visitNewTaggedComponentExpression(BaseNode *parent, const TSNode &node)
{
    NewTaggedComponentExpressionNode* tagnode = parent->addCreatedChild(new NewTaggedComponentExpressionNode(node));
    visitChildren(tagnode, node);

    // for default property components
    BaseNode* p = parent;
    if (p->isNodeType<ComponentBodyNode>() )
    {
        auto body = p->as<ComponentBodyNode>();
        if (body->parent()->isNodeType<ComponentDeclarationNode>() )
        {
            auto decl = body->parent()->as<ComponentDeclarationNode>();
            decl->pushToDefault(tagnode);
        }
        else if (body->parent()->isNodeType<NewComponentExpressionNode>() ||
                 body->parent()->isNodeType<RootNewComponentExpressionNode>() )
        {
            auto expr = body->parent()->as<NewComponentExpressionNode>();
            expr->pushToDefault(tagnode);
        }
    }


    for (auto child: tagnode->children()){
        if (child->isNodeType<IdentifierNode>() ) {
            addUsedIdentifier(tagnode, child->as<IdentifierNode>());
        }
    }
    return tagnode;
}

NewTrippleTaggedComponentExpressionNode* BaseNode::visitNewTrippleTaggedComponentExpression(BaseNode *parent, const TSNode &node){
    NewTrippleTaggedComponentExpressionNode* tagnode = parent->addCreatedChild(new NewTrippleTaggedComponentExpressionNode(node));
    visitChildren(tagnode, node);

    // for default property components
    BaseNode* p = parent;
    if (p->isNodeType<ComponentBodyNode>() )
    {
        auto body = p->as<ComponentBodyNode>();
        if (body->parent()->isNodeType<ComponentDeclarationNode>() )
        {
            auto decl = body->parent()->as<ComponentDeclarationNode>();
            decl->pushToDefault(tagnode);
        }
        else if (body->parent()->isNodeType<NewComponentExpressionNode>() || body->parent()->isNodeType<RootNewComponentExpressionNode>() )
        {
            auto expr = body->parent()->as<NewComponentExpressionNode>();
            expr->pushToDefault(tagnode);
        }
    }


    for (auto child: tagnode->children()){
        if (child->isNodeType<IdentifierNode>() ) {
            addUsedIdentifier(tagnode, child->as<IdentifierNode>());
        }
    }
    return tagnode;
}

TaggedStringNode* BaseNode::visitTaggedString(BaseNode *parent, const TSNode &node){
    TaggedStringNode* tsnode = parent->addCreatedChild(new TaggedStringNode(node));
    return tsnode;
}

TrippleTaggedStringNode* BaseNode::visitTrippleTaggedString(BaseNode *parent, const TSNode &node){
    TrippleTaggedStringNode* tsnode = parent->addCreatedChild(new TrippleTaggedStringNode(node));
    return tsnode;
}

FunctionNode* BaseNode::visitFunction(BaseNode *parent, const TSNode &node){

    FunctionNode* enode = parent->addCreatedChild(new FunctionNode(node));
    for (auto c : extractPrecedingComments(node)) {
        enode->addChild(c);
        enode->m_precedingComments.push_back(c);
    }

    // Function parameters
    const TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");

    assertValid(parent, parameters, "Function parameters are null.");
    enode->m_parameters = BaseNode::scanFormalParameters(enode, parameters);

    // Function return type
    const TSNode returnType = BaseNode::nodeChildByFieldName(node, "return_type");
    if (!ts_node_is_null(returnType)) {
        enode->m_returnType = enode->addCreatedChild(TypeNode::create(returnType, enode));
    }

    // Function body
    const TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Function body is null.");
    enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
    visitChildren(enode->m_body, body);
    
    // Function properties
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "async") == 0 ){
            enode->m_async = true;
        }
    }

    if ( enode->m_parameters ){
        if (enode->m_body ){
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addParameterDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]);
            }
        }
    }
    return enode;
}

FunctionDeclarationNode* BaseNode::visitFunctionDeclaration(BaseNode *parent, const TSNode &node)
{
    FunctionDeclarationNode* fnode = parent->addCreatedChild(new FunctionDeclarationNode(node));
    for (auto c : extractPrecedingComments(node)) {
        fnode->addChild(c);
        fnode->m_precedingComments.push_back(c);
    }

    // Function name
    const TSNode name = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Function name is null.");
    fnode->m_name = fnode->addCreatedChild(IdentifierNode::createNonEmpty(name, fnode));
    addToDeclarations(parent, fnode->m_name, Occurrence::SpaceValue);

    // Function parameters
    const TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Function parameters are null.");
    fnode->m_parameters = BaseNode::scanFormalParameters(fnode, parameters);

    // Function return type
    const TSNode returnType = BaseNode::nodeChildByFieldName(node, "return_type");
    if (!ts_node_is_null(returnType)) {
        fnode->m_returnType = fnode->addCreatedChild(TypeNode::create(returnType, fnode));
    }

    // Function properties
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "async") == 0 ){
            fnode->m_async = true;
        }
    }

    // Function body
    const TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Function body is null.");
    fnode->m_body = fnode->addCreatedChild(new JsBlockNode(body));
    visitChildren(fnode->m_body, body);

    if ( fnode->m_parameters ){
        if (fnode->m_body ){
            for (size_t i = 0; i != fnode->m_parameters->m_parameters.size(); ++i){
                addParameterDeclarations(fnode->m_body, fnode->m_parameters->m_parameters[i]);
            }
        }
    }
    return fnode;
}

ClassDeclarationNode* BaseNode::visitClassDeclaration(BaseNode *parent, const TSNode &node)
{
    ClassDeclarationNode* fnode = parent->addCreatedChild(new ClassDeclarationNode(node));
    visitChildren(fnode, node);

    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    if ( !ts_node_is_null(nameNode) && strcmp(ts_node_type(nameNode), "identifier") != 0){
        fnode->addChild(IdentifierNode::createNonEmpty(nameNode, fnode));
    }

    for (auto it = fnode->children().begin(); it != fnode->children().end(); ++it){
        if ((*it)->isNodeType<IdentifierNode>() )
        {
            // this must be the id!
            IdentifierNode* id = static_cast<IdentifierNode*>(*it);
            addToDeclarations(parent, id, Occurrence::SpaceValue | Occurrence::SpaceType);
        }
    }
    return fnode;
}

VariableDeclarationNode* BaseNode::visitVariableDeclaration(BaseNode *parent, const TSNode &node){
    return visitDeclarationForm(parent, node, VariableDeclarationNode::Var);
}

VariableDeclarationNode* BaseNode::visitLexicalDeclaration(BaseNode *parent, const TSNode &node){
    TSNode kind = BaseNode::nodeChildByFieldName(node, "kind");
    VariableDeclarationNode::DeclarationForm mode = VariableDeclarationNode::Let;
    if (!ts_node_is_null(kind) && strcmp(ts_node_type(kind), "const") == 0) {
        mode = VariableDeclarationNode::Const;
    }

    return visitDeclarationForm(parent, node, mode);
}


VariableDeclarationNode* BaseNode::visitDeclarationForm(BaseNode * parent, const TSNode & node, int form){
    VariableDeclarationNode* dnode = parent->addCreatedChild(new VariableDeclarationNode(node));
    for (auto c : extractPrecedingComments(node)) {
        dnode->addChild(c);
        dnode->m_precedingComments.push_back(c);
    }
    dnode->m_declarationForm = static_cast<VariableDeclarationNode::DeclarationForm>(form);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "variable_declarator") == 0 ){
            VariableDeclaratorNode* declaratorNode = dnode->addCreatedChild(new VariableDeclaratorNode(child));
            dnode->m_declarators.push_back(declaratorNode);
            
            TSNode name = BaseNode::nodeChildByFieldName(child, "name");
            TSNode type = BaseNode::nodeChildByFieldName(child, "type");
            TSNode value = BaseNode::nodeChildByFieldName(child, "value");

            declaratorNode->m_name = declaratorNode->addCreatedChild(IdentifierNode::createNonEmpty(name, declaratorNode));
            if (strcmp(ts_node_type(name), "object_pattern") == 0 || strcmp(ts_node_type(name), "array_pattern") == 0) {
                visitDestructuringPattern(declaratorNode, name);
            } else {
                addToDeclarations(declaratorNode, declaratorNode->m_name, Occurrence::SpaceValue);
            }
            if (!ts_node_is_null(type)) {
                declaratorNode->m_type = declaratorNode->addCreatedChild(TypeNode::create(type, declaratorNode));
            }
            if (!ts_node_is_null(value)) {
                declaratorNode->m_value = declaratorNode->addCreatedChild(new ExpressionNode(value));
                visit(declaratorNode->m_value, value);
            }
        } else if ( strcmp(ts_node_type(child), ";") == 0 ) {
            dnode->m_hasSemicolon = true;
        } else {
            visit(dnode, child);
        }
    }
    return dnode;
}


BaseNode* BaseNode::visitDestructuringPattern(BaseNode *parent, const TSNode &node){
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        const char* type = ts_node_type(child);
        if ( strcmp(type, "identifier") == 0 ){
            auto inode = parent->addCreatedChild(IdentifierNode::createNonEmpty(child, parent));
            addToDeclarations(parent, inode, Occurrence::SpaceValue);
        } else if ( strcmp(type, "shorthand_property_identifier") == 0 || strcmp(type, "shorthand_property_identifier_pattern") == 0 ){
            auto inode = parent->addCreatedChild(IdentifierNode::createNonEmpty(child, parent));
            addToDeclarations(parent, inode, Occurrence::SpaceValue);
        } else if (strcmp(type, "array_pattern") == 0 || strcmp(type, "object_pattern") == 0){
            visitDestructuringPattern(parent, child);
        } else if (strcmp(type, "rest_pattern") == 0){
            visitDestructuringPattern(parent, child);
        } else if (strcmp(type, "pair_pattern") == 0){
            TSNode value = nodeChildByFieldName(child, "value");
            if (!ts_node_is_null(value))
                visitDestructuringPattern(parent, value);
        } else if (strcmp(type, "assignment_pattern") == 0 || strcmp(type, "object_assignment_pattern") == 0){
            TSNode left = nodeChildByFieldName(child, "left");
            if (!ts_node_is_null(left))
                visitDestructuringPattern(parent, left);
        }
    }
    return nullptr;
}

void BaseNode::addParameterDeclarations(BaseNode *parent, ParameterNode *param){
    if (!parent || !param || !param->identifier())
        return;

    if ( param->isDestructuringPattern() ){
        visitDestructuringPattern(parent, param->identifier()->current());
    } else {
        addToDeclarations(parent, param->identifier(), Occurrence::SpaceValue);
    }
}

NewExpressionNode* BaseNode::visitNewExpression(BaseNode *parent, const TSNode &node)
{
    NewExpressionNode* nenode = parent->addCreatedChild(new NewExpressionNode(node));
    nenode->setParent(parent);
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "identifier") == 0 ){
            auto inode = nenode->addCreatedChild(IdentifierNode::createNonEmpty(child, nenode));
            addUsedIdentifier(nenode, inode);
        }
    }
    visitChildren(nenode, node);
    return nenode;
}

ReturnStatementNode* BaseNode::visitReturnStatement(BaseNode *parent, const TSNode &node)
{
    ReturnStatementNode* rsnode = parent->addCreatedChild(new ReturnStatementNode(node));
    visitChildren(rsnode, node);
    return rsnode;
}

ArrowFunctionNode* BaseNode::visitArrowFunction(BaseNode *parent, const TSNode &node){
    ArrowFunctionNode* enode = parent->addCreatedChild(new ArrowFunctionNode(node));
    for (auto c : extractPrecedingComments(node)) {
        enode->addChild(c);
        enode->m_precedingComments.push_back(c);
    }

    // Function parameters
    const TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");
    if (!ts_node_is_null(parameters)) {
        enode->m_parameters = BaseNode::scanFormalParameters(enode, parameters);
    } else {
        const TSNode parameter = BaseNode::nodeChildByFieldName(node, "parameter");
        if (!ts_node_is_null(parameter)) {
            enode->m_parameters = enode->addCreatedChild(new ParameterListNode(parameter));
            auto paramNode = enode->m_parameters->addCreatedChild(new ParameterNode(parameter, IdentifierNode::createNonEmpty(parameter, enode->m_parameters)));
            enode->m_parameters->m_parameters.push_back(paramNode);
        }
    }

    // Function return type
    const TSNode returnType = BaseNode::nodeChildByFieldName(node, "return_type");
    if (!ts_node_is_null(returnType)) {
        enode->m_returnType = enode->addCreatedChild(TypeNode::create(returnType, enode));
    }

    // Function properties
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "async") == 0 ){
            enode->m_async = true;
        }
    }


    if ( enode->m_parameters ){
        if (enode->m_body ){
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addParameterDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]);
            }
        } else {
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addParameterDeclarations(enode, enode->m_parameters->m_parameters[i]);
            }
        }
    }
    
    // Function body
    const TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    if (!ts_node_is_null(body)) {
        if ( strcmp(ts_node_type(body), "statement_block") == 0 ){
            enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
            visitChildren(enode->m_body, body);
        } else {
            enode->m_expression = visitRecognized(enode, body);
            if ( !enode->m_expression ){
                enode->m_expression = enode->addCreatedChild(new ExpressionNode(body));
                visitChildren(enode->m_expression, body);
            }
        }
    }

    return enode;
}

ObjectNode* BaseNode::visitObject(BaseNode *parent, const TSNode &node)
{
    ObjectNode* onode = parent->addCreatedChild(new ObjectNode(node));
    visitChildren(onode, node);
    return onode;
}

TryCatchBlockNode* BaseNode::visitTryCatchBlock(BaseNode *parent, const TSNode &node){
    TryCatchBlockNode* enode = parent->addCreatedChild(new TryCatchBlockNode(node));

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "statement_block") == 0 ){
            enode->m_tryBody = enode->addCreatedChild(new JsBlockNode(child));
            visitChildren(enode->m_tryBody, child);
        } else if ( strcmp(ts_node_type(child), "catch_clause") == 0 ){
            TSNode parameterNode = BaseNode::nodeChildByFieldName(child, "parameter");
            TSNode parameterTypeNode = BaseNode::nodeChildByFieldName(child, "type");
            if ( !ts_node_is_null(parameterNode) && strcmp(ts_node_type(parameterNode), "identifier") == 0 ){
                enode->m_catchParameter = enode->addCreatedChild(
                    new ParameterNode(parameterNode, IdentifierNode::createNonEmpty(parameterNode, enode))
                );
                if ( !ts_node_is_null(parameterTypeNode) ){
                    ParameterNode* catchParameter = enode->m_catchParameter;
                    catchParameter->m_type = catchParameter->addCreatedChild(TypeNode::create(parameterTypeNode, catchParameter));
                }
            }
            TSNode catchBody = BaseNode::nodeChildByFieldName(child, "body");
            if ( !ts_node_is_null(catchBody) ){
                enode->m_catchBody = enode->addCreatedChild(new JsBlockNode(catchBody));
                if ( enode->m_catchParameter ){
                    addToDeclarations(enode->m_catchBody, enode->m_catchParameter->identifier(), Occurrence::SpaceValue);
                } else if (!ts_node_is_null(parameterNode) &&
                           (strcmp(ts_node_type(parameterNode), "object_pattern") == 0 ||
                            strcmp(ts_node_type(parameterNode), "array_pattern") == 0)) {
                    visitDestructuringPattern(enode->m_catchBody, parameterNode);
                }
                visitChildren(enode->m_catchBody, catchBody);
            }
        } else if ( strcmp(ts_node_type(child), "finally_clause") == 0 ){
            enode->m_finalizerBody = enode->addCreatedChild(new JsBlockNode(child));
            visitChildren(enode->m_finalizerBody, child);
        }
    }

    return enode;
}

BaseNode* BaseNode::visitForInStatement(BaseNode *parent, const TSNode &node){
    JsBlockNode* enode = parent->addCreatedChild(new JsBlockNode(node));
    uint32_t count = ts_node_child_count(node);

    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(node, i);
        if (strcmp(ts_node_type(child), "identifier") == 0) {
            auto inode = enode->addCreatedChild(IdentifierNode::createNonEmpty(child, enode));
            addToDeclarations(enode, inode, Occurrence::SpaceValue);
        } else if (
            strcmp(ts_node_type(child), "statement_block") == 0 || 
            strcmp(ts_node_type(child), "expression_statement") == 0)
        {
            visitChildren(enode, child);
        } else if (strcmp(ts_node_type(child), "array_pattern") == 0 || strcmp(ts_node_type(child), "object_pattern") == 0) {
            visitDestructuringPattern(enode, child);
        }
    }

    return enode;
}

TypeAliasDeclarationNode* BaseNode::visitTypeAliasDeclaration(BaseNode* parent, const TSNode& node){
    TypeAliasDeclarationNode* declarationNode = parent->addCreatedChild(new TypeAliasDeclarationNode(node));
    TSNode name = BaseNode::nodeChildByFieldName(node, "name");
    BaseNode::assertValid(parent, name, "Type alias name is missing.");
    declarationNode->m_name = declarationNode->addCreatedChild(IdentifierNode::createNonEmpty(name, declarationNode));
    addToDeclarations(parent, declarationNode->m_name, Occurrence::SpaceType);

    TSNode typeParameters = BaseNode::nodeChildByFieldName(node, "type_parameters");
    if ( !ts_node_is_null(typeParameters) ){
        declarationNode->m_typeParameters = declarationNode->addCreatedChild(TypeParameterListNode::create(typeParameters, declarationNode));
    }

    TSNode value = BaseNode::nodeChildByFieldName(node, "value");
    if ( !ts_node_is_null(value) ){
        declarationNode->addCreatedChild(TypeNode::create(value, declarationNode));
    }
    return declarationNode;
}

InterfaceDeclarationNode* BaseNode::visitInterfaceDeclaration(BaseNode* parent, const TSNode& node){
    InterfaceDeclarationNode* declarationNode = parent->addCreatedChild(new InterfaceDeclarationNode(node));
    TSNode name = BaseNode::nodeChildByFieldName(node, "name");
    BaseNode::assertValid(parent, name, "Interface name is missing.");
    declarationNode->m_name = declarationNode->addCreatedChild(IdentifierNode::createNonEmpty(name, declarationNode));
    addToDeclarations(parent, declarationNode->m_name, Occurrence::SpaceType);

    TSNode typeParameters = BaseNode::nodeChildByFieldName(node, "type_parameters");
    if ( !ts_node_is_null(typeParameters) ){
        declarationNode->m_typeParameters = declarationNode->addCreatedChild(TypeParameterListNode::create(typeParameters, declarationNode));
    }

    TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    if ( !ts_node_is_null(body) ){
        declarationNode->addCreatedChild(TypeNode::create(body, declarationNode));
    }
    for ( uint32_t i = 0; i < ts_node_child_count(node); ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "extends_type_clause") == 0 ){
            declarationNode->addCreatedChild(TypeNode::create(child, declarationNode));
        }
    }
    return declarationNode;
}

EnumDeclarationNode* BaseNode::visitEnumDeclaration(BaseNode* parent, const TSNode& node){
    EnumDeclarationNode* declarationNode = parent->addCreatedChild(new EnumDeclarationNode(node));
    TSNode name = BaseNode::nodeChildByFieldName(node, "name");
    BaseNode::assertValid(parent, name, "Enum name is missing.");
    declarationNode->m_name = declarationNode->addCreatedChild(IdentifierNode::createNonEmpty(name, declarationNode));
    addToDeclarations(parent, declarationNode->m_name, Occurrence::SpaceValue | Occurrence::SpaceType);

    TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    if ( !ts_node_is_null(body) ){
        declarationNode->addCreatedChild(TypeNode::create(body, declarationNode));
    }
    return declarationNode;
}

void JsImportNode::markUsage(const std::string& source, const std::string& id, Occurrence::Space space){
    for ( JsImportName& importName : m_importNames ){
        if ( importName.identifier() && BaseNode::slice(source, importName.identifier()) == id ){
            importName.markUsage(space);
        }
    }
}

std::string JsImportNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += std::string("JsImport (" + rangeString()+ ")") + "\n";

    return result;
}

std::string ImportNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    std::string importPath;
    if ( m_importPath ){
        for( ImportPathSegmentNode* node : m_importPath->segments() ){
            importPath += node->rangeString();
        }
    }

    std::string importAs = m_importAs ? "(as " + m_importAs->rangeString() + ")" : "";

    result += std::string("Import (" + importPath + ")") + importAs + rangeString() + "\n";

    return result;
}

std::string ImportNode::path(const std::string& source) const{
    std::string importSegments;

    if (isRelative())
        importSegments = ".";

    auto segments = m_importPath->segments();
    for (size_t i = 0; i < segments.size(); ++i){
        auto node = segments[i];
        if (i != 0)
            importSegments += ".";
        importSegments += slice(source, node);
    }

    return importSegments;
}

std::string ImportNode::as(const std::string &source) const{
    if ( m_importAs )
        return slice(source, m_importAs);
    return "";
}

void ImportNode::addChild(BaseNode *child)
{
    if (child->isNodeType<ImportPathNode>() ){
        m_importPath = child->as<ImportPathNode>();
    }
    BaseNode::addChild(child);
}

void ProgramNode::collectImportTypes(const std::string &source, ConversionContext *ctx){
    if ( m_importTypesCollected )
        return;

    std::vector<Occurrence> references;
    collectImports(source, references, ctx);

    for ( const Occurrence& reference : references ){
        IdentifierNode* identifier = reference.identifier();

        std::string idName = slice(source, identifier);

        bool isNamespaceIdentifier = false;

        // check whether identifier is part of an import namespace
        for ( ImportNode* in : m_imports ){
            if ( in->hasNamespace() ){
                if ( in->as(source) == idName ){
                    isNamespaceIdentifier = true;
                    BaseNode* parent = identifier->parent();
                    if ( parent->isNodeType<ComponentDeclarationNode>() ){
                        auto parentCast = parent->as<ComponentDeclarationNode>();
                        if ( parentCast->heritage().size() > 1 && parentCast->heritage()[0] == identifier ){
                            ProgramNode::ImportType impt;
                            impt.importNamespace = idName;
                            impt.name = slice(source, parentCast->heritage()[1]);
                            impt.location = identifier->startPoint();
                            impt.space = reference.space();
                            addImportType(impt);
                        }
                    } else if ( parent->isNodeType<MemberExpressionNode>() ){
                        auto parentCast = parent->as<MemberExpressionNode>();
                        if ( parentCast->children().size() > 1 && parentCast->children()[0] == identifier ){
                            ProgramNode::ImportType impt;
                            impt.importNamespace = idName;
                            impt.name = slice(source, parentCast->children()[1]);
                            impt.location = identifier->startPoint();
                            impt.space = reference.space();
                            addImportType(impt);
                        }
                    } else if ( parent->isNodeType<NewComponentExpressionNode>() ||
                                parent->isNodeType<RootNewComponentExpressionNode>() )
                    {
                        auto parentCast = parent->as<NewComponentExpressionNode>();
                        if ( parentCast->name().size() > 1 && parentCast->name()[0] == identifier ){
                            ProgramNode::ImportType impt;
                            impt.importNamespace = idName;
                            impt.name = slice(source, parentCast->name()[1]);
                            impt.location = identifier->startPoint();
                            impt.space = reference.space();
                            addImportType(impt);
                        }
                    } else if ( parent->isNodeType<FlowCallNode>() ){
                        auto parentCast = parent->as<FlowCallNode>();
                        if ( parentCast->callee().size() > 1 && parentCast->callee()[0] == identifier ){
                            ProgramNode::ImportType impt;
                            impt.importNamespace = idName;
                            impt.name = slice(source, parentCast->callee()[1]);
                            impt.location = identifier->startPoint();
                            impt.space = reference.space();
                            addImportType(impt);
                        }
                    } else if ( parent->isNodeType<TypeNode>() ){
                        if ( IdentifierNode* qualifiedTail = parent->as<TypeNode>()->qualifiedTail(identifier) ){
                            ProgramNode::ImportType impt;
                            impt.importNamespace = idName;
                            impt.name = slice(source, qualifiedTail);
                            impt.location = identifier->startPoint();
                            impt.space = reference.space();
                            addImportType(impt);
                        }
                    }
                }
            }
        }

        if ( !isNamespaceIdentifier ){
            ProgramNode::ImportType impt;
            impt.name = idName;
            impt.location = identifier->startPoint();
            impt.space = reference.space();
            addImportType(impt);
        }
    }

    m_importTypesCollected = true;
}

void ProgramNode::resolveImport(const std::string &as, const std::string &name, const std::string &path, Occurrence::Space exportSpace){
    auto asIt = m_importTypes.find(as);
    if ( asIt != m_importTypes.end() ){
        auto nameIt = asIt->second.find(name);
        if ( nameIt != asIt->second.end() ){
            nameIt->second.resolvedPath = path;
            if ( !( exportSpace & Occurrence::SpaceValue ) ){
                nameIt->second.space &= ~Occurrence::SpaceMeta;
            }
        }
    }
}

std::string ProgramNode::importTypesString() const{
    std::string result;
    for ( auto it = m_importTypes.begin(); it != m_importTypes.end(); ++it ){
        if ( !result.empty() )
            result += '\n';
        result += "Imports as '" + it->first + "':";

        for ( auto impIt = it->second.begin(); impIt != it->second.end(); ++ impIt ){
            result += " " + impIt->second.name;
        }
    }
    return result;
}

void ProgramNode::addChild(BaseNode *child){
    if ( child->isNodeType<ImportNode>() ){
        m_imports.push_back(child->as<ImportNode>());
        BaseNode::addChild(child);
    } else if ( child->isNodeType<JsImportNode>() ){
        m_jsImports.push_back(child->as<JsImportNode>());
        BaseNode::addChild(child);
    } else if ( child->isNodeType<ComponentDeclarationNode>() ){
        m_exports.push_back(child);
        BaseNode::addChild(child);
    } else if ( child->isNodeType<ComponentInstanceStatementNode>() ){
        m_exports.push_back(child);
        BaseNode::addChild(child);
    } else if ( child->isNodeType<TypedFunctionDeclarationNode>() ||
                child->isNodeType<FlowFunctionDeclarationNode>() ){
        m_exports.push_back(child);
        BaseNode::addChild(child);
    } else if ( child->isNodeType<TypeAliasDeclarationNode>() ||
                child->isNodeType<InterfaceDeclarationNode>() ||
                child->isNodeType<EnumDeclarationNode>() ){
        m_exports.push_back(child);
        BaseNode::addChild(child);
    } else {
        SyntaxException se = SyntaxException(
            Utf8("Unexpected expression in file root of type '%'").format(child->nodeName()),
            Exception::toCode("~Language"),
            nodeSourceLocation(child, child->current()),
            SOURCE_TRACE()
        );
        throw se;
    }
}

std::string ProgramNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += std::string("Program [") +
            std::to_string(ts_node_start_byte(current())) + ", " +
            std::to_string(ts_node_end_byte(current())) + "]\n";

    std::string indentStr;
    if ( indent > 0 )
        indentStr.assign(indent * 2, ' ');

    result += indentStr + " /Imports\n";
    for (BaseNode* child : m_imports){
        result += child->toString(indent < 0 ? indent : indent + 1);
    }

    result += indentStr + " /Component Exports\n";
    for (BaseNode* child : m_exports){
        result += child->toString(indent < 0 ? indent : indent + 1);
    }

    result += indentStr + " /Children\n";
    for (BaseNode* child : children()){
        result += child->toString(indent < 0 ? indent : indent + 1);
    }

    return result;
}

void ProgramNode::collectImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext *ctx){
    for ( BaseNode* child: m_exports ){
        child->collectImports(source, references, ctx);
    }
    collectBlockImports(source, references, ctx);
}

void ProgramNode::addImportType(const ProgramNode::ImportType &t){
    ProgramNode::ImportType& existing = m_importTypes[t.importNamespace][t.name];
    if ( existing.name.empty() ){
        existing = t;
        return;
    }

    existing.space |= t.space;
    if ( existing.resolvedPath.empty() ){
        existing.resolvedPath = t.resolvedPath;
    }
}

ComponentDeclarationNode::ComponentDeclarationNode(const TSNode &node)
    : JsBlockNode(node, ComponentDeclarationNode::nodeInfo())
    , m_name(nullptr)
    , m_id(nullptr)
    , m_body(nullptr)
{
}

std::string ComponentDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    std::string name = "(name " + (m_name ? m_name->rangeString() : "") + ")";

    std::string inheritance = "";
    if ( m_heritage.size() > 0 ){
        inheritance = "(inherits ";
        for( IdentifierNode* node :m_heritage ){
            inheritance += node->rangeString();
        }
        inheritance += ")";
    }

    std::string identifier = "";
    if ( m_id ){
        identifier = "(id " + m_id->rangeString() + ")";
    }

    result += "ComponentDeclaration " + name + identifier + inheritance + rangeString() + "\n";
    if ( m_body ){
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);
    }

    return result;
}

PropertyAccessorDeclarationNode::PropertyAccess ComponentDeclarationNode::propertyAccessors(const std::string &source, const std::string &propertyName){
    PropertyAccessorDeclarationNode::PropertyAccess result;

    for ( size_t j = 0; j < m_propertyAccesors.size(); ++j ){
        PropertyAccessorDeclarationNode* pa = m_propertyAccesors[j];
        if ( slice(source, pa->name()) == propertyName ){
            if ( pa->access() == PropertyAccessorDeclarationNode::Getter ){
                result.getter = pa;
                pa->setIsPropertyAttached(true);
            } else if ( pa->access() == PropertyAccessorDeclarationNode::Setter ){
                result.setter = pa;
                pa->setIsPropertyAttached(true);
            }
        }
    }

    return result;
}

std::string ComponentDeclarationNode::name(const std::string& source) const{
    if ( !m_name )
        return "";

    std::string name = slice(source, m_name);

    if (name == "default"){
        const BaseNode* p = this;
        while (p && !p->isNodeType<ProgramNode>() )
            p = p->parent();
        if (p) {
            const ProgramNode* program = dynamic_cast<const ProgramNode*>(p);
            if (program)
                name = program->fileName();
        }
    }

    return name;
}

bool ComponentDeclarationNode::isAnonymous() const{
    return !m_name;
}

NewComponentExpressionNode::NewComponentExpressionNode(const TSNode &node, const LanguageNodeInfo::ConstPtr &ni)
    : JsBlockNode(node, ni)
    , m_id(nullptr)
    , m_body(nullptr)
    , m_arguments(nullptr)
{

}

NewComponentExpressionNode::NewComponentExpressionNode(const TSNode &node):
    NewComponentExpressionNode(node, NewComponentExpressionNode::nodeInfo())
{

}


std::string NewComponentExpressionNode::initializerName(const std::string &source){
    std::string name;
    for ( auto nameIden : m_name ){
        if ( !name.empty() )
            name += ".";
        name += slice(source, nameIden);
    }
    return name;
}

std::string NewComponentExpressionNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    std::string name =
        "(name " +
        (!m_name.empty()
            ? std::string("[") + std::to_string(m_name.front()->startByte()) + ", " +
              std::to_string(m_name.back()->endByte()) + "]" : "") + ")";

    std::string identifier = "";
    if ( m_id )
        identifier = "(id " + m_id->rangeString() + ")";


    std::string args = "";
    if (m_arguments)
        args = "(args " + m_arguments->rangeString() + ")";

    result += nodeName() + " " + name + identifier +args + rangeString() + "\n";
    if ( m_body ){
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);
    }

    if (m_arguments)
        result += m_arguments->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string ComponentBodyNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "ComponentBody " + rangeString() + "\n";

    const std::vector<BaseNode*>& c = children();
    for ( BaseNode* node : c ){
        result += node->toString(indent >= 0 ? indent + 1 : indent);
    }

    return result;
}

PropertyDeclarationNode::PropertyDeclarationNode(const TSNode &node)
    : JsBlockNode(node, PropertyDeclarationNode::nodeInfo())
    , m_name(nullptr)
    , m_type(nullptr)
    , m_isBindingAssignment(true)
    , m_expression(nullptr)
    , m_statementBlock(nullptr)
    , m_bindingContainer(new PropertyBindingContainer)
{
    m_bindingContainer->setDeclarationCheck([this](const std::string& source, const std::string&, BaseNode* m){
        
        IdentifierNode* head = m && m->canCast<MemberExpressionNode>()
            ? m->as<MemberExpressionNode>()->identifierChainHead()
            : nullptr;
        return head && DeclarationLookup::find(
            source, m->parent(), Occurrence(head, Occurrence::SpaceValue), this).identifier() != nullptr;
    });
}

PropertyDeclarationNode::~PropertyDeclarationNode(){
    delete m_bindingContainer;
}

std::string PropertyDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";
    std::string type = "";
    if ( m_type )
        type = "(type " + m_type->rangeString() + ")";
    result += "PropertyDeclaration " + rangeString() + name + type + "\n";

    if ( m_expression )
        result += m_expression->toString(indent >= 0 ? indent + 1 : indent);

    if ( m_statementBlock )
        result += m_statementBlock->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

void PropertyDeclarationNode::pushToBindings(BaseNode *bn){
    m_bindingContainer->addBinding(bn);
}

std::string PropertyDeclarationNode::bindingIdentifiersToString(const std::string &source) const{
    return m_bindingContainer->bindingIdentifiersToString(source);
}

std::string PropertyDeclarationNode::bindingIdentifiersToJs(const std::string &source) const{
    return m_bindingContainer->bindingIdentifiersToJs(source);
}

StaticPropertyDeclarationNode::StaticPropertyDeclarationNode(const TSNode &node)
    : BaseNode(node, StaticPropertyDeclarationNode::nodeInfo())
    , m_name(nullptr)
    , m_type(nullptr)
    , m_expression(nullptr)
{
}

std::string StaticPropertyDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";
    std::string type = "";
    if ( m_type )
        type = "(type " + m_type->rangeString() + ")";
    result += "StaticPropertyDeclaration " + rangeString() + name + type + "\n";

    if ( m_expression )
        result += m_expression->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}


PropertyAssignmentNode::PropertyAssignmentNode(const TSNode &node)
    : JsBlockNode(node, PropertyAssignmentNode::nodeInfo())
    , m_isBindingAssignment(true)
    , m_expression(nullptr)
    , m_statementBlock(nullptr)
    , m_bindingContainer(new PropertyBindingContainer)
{
    m_bindingContainer->setDeclarationCheck([this](const std::string& source, const std::string&, BaseNode* m){
        IdentifierNode* head = m && m->canCast<MemberExpressionNode>()
            ? m->as<MemberExpressionNode>()->identifierChainHead()
            : nullptr;
        return head && DeclarationLookup::find(
            source, m->parent(), Occurrence(head, Occurrence::SpaceValue), this).identifier() != nullptr;
    });
}

PropertyAssignmentNode::~PropertyAssignmentNode(){
    delete m_bindingContainer;
}

std::string PropertyAssignmentNode::toString(int indent) const{
    std::string result;
   if ( indent > 0 )
       result.assign(indent * 2, ' ');
    std::string name = "";
    result += "PropertyAssignment " + rangeString() + name + "\n";
    if ( m_expression )
        result += m_expression->toString(indent >= 0 ? indent + 1 : indent);
    return result;
}

void PropertyAssignmentNode::pushToBindings(BaseNode *bn){
    m_bindingContainer->addBinding(bn);
}

std::string PropertyAssignmentNode::bindingIdentifiersToString(const std::string &source) const{
    return m_bindingContainer->bindingIdentifiersToString(source);
}

std::string PropertyAssignmentNode::bindingIdentifiersToJs(const std::string &source) const{
    return m_bindingContainer->bindingIdentifiersToJs(source);
}

EventDeclarationNode::EventDeclarationNode(const TSNode &node)
    : BaseNode(node, EventDeclarationNode::nodeInfo() )
    , m_name(nullptr)
    , m_parameters(nullptr)
{}

std::string EventDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');
    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";
    result += "EventDeclaration " + rangeString() + name + "\n";
    if ( m_parameters )
        result += m_parameters->toString(indent >= 0 ? indent + 1 : indent);
    return result;
}

std::vector<std::string> MemberExpressionNode::identifierChain(const std::string &source) const{
    std::vector<std::string> result;
    for ( auto child : children() ){
        if ( child->isNodeType<IdentifierNode>() ){
            result.push_back(BaseNode::slice(source, child));
        } else if ( child->canCast<MemberExpressionNode>() ){
            auto nestedChain = child->as<MemberExpressionNode>()->identifierChain(source);
            result.insert( result.end(), nestedChain.begin(), nestedChain.end() );
        }
    }
    return result;
}

IdentifierNode* MemberExpressionNode::identifierChainHead() const{
    for ( auto child : children() ){
        if ( child->isNodeType<IdentifierNode>() ){
            return child->as<IdentifierNode>();
        } else if ( child->canCast<MemberExpressionNode>() ){
            IdentifierNode* nested = child->as<MemberExpressionNode>()->identifierChainHead();
            if ( nested ){
                return nested;
            }
        }
    }
    return nullptr;
}

ParameterNode::ParameterNode(const TSNode& node, IdentifierNode* identifier, bool isOptional)
    : BaseNode(node, ParameterNode::nodeInfo())
    , m_identifier(identifier)
    , m_type(nullptr)
    , m_isOptional(isOptional)
    , m_isDestructuringPattern(false)
    , m_defaultValue(nullptr)
{
    if ( m_identifier ){
        addChild(m_identifier);

        const char* nodeType = ts_node_type(m_identifier->current());
        m_isDestructuringPattern = strcmp(nodeType, "object_pattern") == 0 ||
                      strcmp(nodeType, "array_pattern") == 0 ||
                      strcmp(nodeType, "assignment_pattern") == 0 ||
                      strcmp(nodeType, "object_assignment_pattern") == 0 ||
                      strcmp(nodeType, "pair_pattern") == 0 ||
                      strcmp(nodeType, "rest_pattern") == 0;
    }
}

std::string ParameterListNode::toString(int indent) const{
    std::string result;
   if ( indent > 0 )
       result.assign(indent * 2, ' ');

    std::string parameters = "(no parameters: " + std::to_string(m_parameters.size()) + ")";

    result += "ParameterList " + rangeString() + parameters + "\n";
    return result;
}

TypeParameterListNode* TypeParameterListNode::create(const TSNode& typeParameters, BaseNode* parent){
    TypeParameterListNode* result = new TypeParameterListNode(typeParameters);
    result->setParent(parent);

    uint32_t parameterCount = ts_node_child_count(typeParameters);
    for ( uint32_t pc = 0; pc < parameterCount; ++pc ){
        TSNode typeParameter = ts_node_child(typeParameters, pc);
        assertError(parent, typeParameter, "Type parameter is invalid.");
        if ( strcmp(ts_node_type(typeParameter), "type_parameter") != 0 ){
            continue;
        }

        TSNode parameterName = BaseNode::nodeChildByFieldName(typeParameter, "name");
        assertValid(parent, parameterName, "Type parameter name is null.");
        result->m_names.push_back(result->addCreatedChild(IdentifierNode::createNonEmpty(parameterName, result)));
        addToDeclarations(parent, result->m_names.back(), Occurrence::SpaceType);
    }

    return result;
}

std::string TypeParameterListNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    std::string parameters = "(no type parameters: " + std::to_string(m_names.size()) + ")";

    result += "TypeParameterList " + rangeString() + parameters + "\n";
    return result;
}

ListenerDeclarationNode::ListenerDeclarationNode(const TSNode &node)
    : BaseNode(node, ListenerDeclarationNode::nodeInfo() )
    , m_name(nullptr)
    , m_parameters(nullptr)
    , m_body(nullptr)
    , m_bodyExpression(nullptr)
    , m_async(false)
{}

std::string ListenerDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');
    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";

    result += "ListenerDeclaration " + rangeString() + name + "\n";
    if ( m_parameters )
        result += m_parameters->toString(indent >= 0 ? indent + 1 : indent);
    if ( m_body )
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

MethodDefinitionNode::MethodDefinitionNode(const TSNode &node)
    : BaseNode(node, MethodDefinitionNode::nodeInfo() )
    , m_name(nullptr)
    , m_parameters(nullptr)
    , m_body(nullptr)
{
}


TryCatchBlockNode::TryCatchBlockNode(const TSNode &node)
    : BaseNode(node, TryCatchBlockNode::nodeInfo())
    , m_tryBody(nullptr)
    , m_catchBody(nullptr)
    , m_finalizerBody(nullptr)
    , m_catchParameter(nullptr)
{
}

std::string MethodDefinitionNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');
    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";

    result += "MethodDefinition " + rangeString() + name + "\n";
    if ( m_parameters )
        result += m_parameters->toString(indent >= 0 ? indent + 1 : indent);
    if ( m_body )
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}


ArrowFunctionNode::ArrowFunctionNode(const TSNode &node)
    : FunctionNode(node, ArrowFunctionNode::nodeInfo() )
    , m_expression(nullptr)
{
}

std::string ArrowFunctionNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "ArrowFunction " + rangeString();

    const std::size_t paramSize = m_parameters->parameters().size();
    if ( paramSize > 0 ){
        std::string paramsResult;
        if ( indent > 0 )
            paramsResult.assign(indent * 2, ' ');
        std::string parameters = "(no parameters: " + std::to_string(paramSize) + ")";
        result += paramsResult + "ParameterList " + rangeString() + parameters + "\n";
    }

    if ( m_body )
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

FunctionNode::FunctionNode(const TSNode &node)
    : BaseNode(node, FunctionNode::nodeInfo())
    , m_parameters(nullptr)
    , m_body(nullptr)
    , m_returnType(nullptr)
    , m_async(false)
{
}

FunctionNode::FunctionNode(const TSNode &node, const LanguageNodeInfo::ConstPtr &ni)
    : BaseNode(node, ni)
    , m_parameters(nullptr)
    , m_body(nullptr)
    , m_returnType(nullptr)
    , m_async(false)
{
}

std::string FunctionNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "Function " + rangeString() + "\n";

    if ( m_parameters->parameters().size() > 0 ){
        std::string paramsResult;
        if ( indent > 0 )
            paramsResult.assign(indent * 2, ' ');
        std::string parameters = "(no parameters: " + std::to_string(m_parameters->parameters().size()) + ")";
        result += paramsResult + "ParameterList " + rangeString() + parameters + "\n";
    }

    if ( m_body )
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}


FunctionDeclarationNode::FunctionDeclarationNode(const TSNode &node)
    : FunctionNode(node, FunctionDeclarationNode::nodeInfo())
    , m_name(nullptr)
{
}

std::string FunctionDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');
    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";

    result += "FunctionDeclaration " + rangeString() + name + "\n";

    if ( parameters() && parameters()->parameters().size() > 0 ){
        std::string paramsResult;
        if ( indent > 0 )
            paramsResult.assign(indent * 2, ' ');
        std::string params = "(no parameters: " + std::to_string(parameters()->parameters().size()) + ")";
        result += paramsResult + "ParameterList " + rangeString() + params + "\n";
    }

    if ( body() )
        result += body()->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}


TypedMethodDeclarationNode::TypedMethodDeclarationNode(const TSNode &node)
    : JsBlockNode(node, TypedMethodDeclarationNode::nodeInfo())
    , m_name(nullptr)
    , m_parameters(nullptr)
    , m_body(nullptr)
    , m_returnType(nullptr)
    , m_async(false)
    , m_static(false)
{
}

TypedMethodDeclarationNode::TypedMethodDeclarationNode(const TSNode &node, const LanguageNodeInfo::ConstPtr &ni)
    : JsBlockNode(node, ni)
    , m_name(nullptr)
    , m_parameters(nullptr)
    , m_body(nullptr)
    , m_returnType(nullptr)
    , m_async(false)
    , m_static(false)
{
}

std::string TypedMethodDeclarationNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');
    std::string name = "";
    if ( m_name )
        name = "(name " + m_name->rangeString() + ")";

    result += "TypedMethodDeclaration " + rangeString() + name + "\n";
    if ( m_parameters )
        result += m_parameters->toString(indent >= 0 ? indent + 1 : indent);
    if ( m_body )
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string ConstructorDefinitionNode::toString(int indent) const
{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "ConstructorDefinition " + rangeString() + "\n";

    if ( m_parameters && m_parameters->parameters().size() > 0 ){
        std::string paramsResult;
        if ( indent > 0 )
            paramsResult.assign(indent * 2, ' ');
        std::string params = "(no parameters: " + std::to_string(m_parameters->parameters().size()) + ")";
        result += paramsResult + "ParameterList " + rangeString() + params + "\n";
    }

    if (m_body)
        result += m_body->toString(indent >= 0 ? indent + 1 : indent);

    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string ExpressionStatementNode::toString(int indent) const
{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "ExpressionStatement " + rangeString() + "\n";
    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string AssignmentExpressionNode::toString(int indent) const{

    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "AssignmentExpression" + rangeString() + "\n";
    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

void AssignmentExpressionNode::addChild(BaseNode *child){
    TSNode s = current();
    TSNode left = ts_node_child_by_field_name(s, "left", 4);
    TSNode right = ts_node_child_by_field_name(s, "right", 5);
    if ( !ts_node_is_null(left) && !ts_node_is_null(right) ){
        TSNode childNode = child->current();
        if ( ts_node_eq(left, childNode) ){
            m_left = child;
        } else if ( ts_node_eq(right, childNode ) ){
            m_right = child;
        }
    }

    BaseNode::addChild(child);
}

std::string CallExpressionNode::toString(int indent) const
{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "CallExpression " + rangeString() + (isSuper ? " SUPER" : "") + "\n";
    if (m_arguments)
        result += m_arguments->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string NewTaggedComponentExpressionNode::toString(int indent) const
{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "NewTaggedComponentExpression " + rangeString() + "\n";

    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string NewTrippleTaggedComponentExpressionNode::toString(int indent) const{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "NewTrippleTaggedComponentExpression " + rangeString() + "\n";

    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}


std::string ArgumentsNode::toString(int indent) const
{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "Arguments " + rangeString() + "\n";

    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

std::string VariableDeclarationNode::toString(int indent) const
{
    std::string result;
    if ( indent > 0 )
        result.assign(indent * 2, ' ');

    result += "VariableDeclaration " + rangeString() + "\n";

    for (size_t i = 0; i < m_declarators.size(); i++)
    {
        result += m_declarators[i]->toString(indent >= 0 ? indent + 1 : indent);
    }

    for (auto it = children().begin(); it != children().end(); ++it)
        result += (*it)->toString(indent >= 0 ? indent + 1 : indent);

    return result;
}

void JsBlockNode::collectImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext *ctx){
    BaseNode::collectImports(source, references, ctx);
    collectBlockImports(source, references, ctx);
}

void JsBlockNode::collectBlockImports(const std::string &source, std::vector<Occurrence>& references, ConversionContext* ctx){
    for ( const Occurrence& reference : m_references ){
        IdentifierNode* identifier = reference.identifier();
        const std::string id = slice(source, identifier);
        if ( id == "this" || id == "parent" || id == "import" ){
            continue;
        }
        if ( ConversionContext::isImplicitType(ctx, id) ){
            continue;
        }
        if ( TypeNode::findTypeNode(identifier) && TypeNode::isReservedTypeName(id) ){
            continue;
        }

        DeclarationLookup lookup = DeclarationLookup::find(source, this, reference);
        if ( lookup.identifier() ){
            if ( lookup.jsImportSite() ){
                lookup.jsImportSite()->markUsage(source, id, reference.space());
            }
            continue;
        }

        if ( lookup.spaceMismatch().identifier() ){
            throwError(
                identifier,
                identifier->current(),
                ( reference.space() & Occurrence::SpaceType )
                    ? Utf8("'%' refers to a value, but is being used as a type.").format(id).data()
                    : Utf8("'%' refers to a type, but is being used as a value.").format(id).data()
            );
        }

        BaseNode* prog = this;
        while ( prog && prog->parent() ){
            prog = prog->parent();
        }
        if ( prog && prog->isNodeType<ProgramNode>() && ctx && !ctx->flowModuleName.empty() && id == ctx->flowModuleName && prog->as<ProgramNode>()->hasFlows() ){
            continue;
        }

        references.push_back(reference);
    }
}

std::string ComponentInstanceStatementNode::name(const std::string &source) const{
    if ( !m_name )
        return "";

    std::string instance_name = slice(source, m_name);

    if (instance_name == "default"){
        const BaseNode* p = this;
        while (p && !p->isNodeType<ProgramNode>() )
            p = p->parent();
        if (p) {
            const ProgramNode* program = dynamic_cast<const ProgramNode*>(p);
            if (program)
                instance_name = program->fileName();
        }
    }

    return instance_name;
}

IdentifierNode *PropertyAccessorDeclarationNode::firstParameterName() const{
    if ( parameters() ){
        ParameterListNode* pdn = parameters()->as<ParameterListNode>();
        if  ( pdn->parameters().size() > 0 ){
            return pdn->parameters()[0]->identifier();
        }
    }
    return nullptr;
}

IdentifierNode* IdentifierNode::createNonEmpty(const TSNode& node, BaseNode *parentTrace){
    int length = ts_node_end_byte(node) - ts_node_start_byte(node);
    if ( length == 0 ){
        throwError(parentTrace, node, "Expected identifier");
    }
    return new IdentifierNode(node);
}

TypeNode* TypeNode::create(const TSNode& node, BaseNode* parent, bool isMeta){
    TypeNode* result = new TypeNode(node, isMeta);
    result->setParent(parent);
    visit(result, node);
    return result;
}

TypeNode* TypeNode::findTypeNode(BaseNode* node){
    for ( BaseNode* current = node; current; current = current->parent() ){
        if ( current->isNodeType<TypeNode>() ){
            return current->as<TypeNode>();
        }
    }
    return nullptr;
}

IdentifierNode* TypeNode::visitTypeIdentifier(BaseNode* parent, const TSNode& node){
    IdentifierNode* identifier = parent->addCreatedChild(IdentifierNode::createNonEmpty(node, parent));

    TypeNode* type = findTypeNode(parent);
    Occurrence occurrence(identifier, type ? type->nameSpace() : Occurrence::SpaceType);
    if ( type ){
        type->m_declaredIdentifiers.push_back(occurrence);
    }
    addUsedIdentifier(parent, occurrence);
    return identifier;
}

IdentifierNode* TypeNode::visitNestedTypeIdentifier(BaseNode* parent, const TSNode& node){
    return visitQualifiedHead(parent, node);
}

IdentifierNode* TypeNode::visitTypeQuery(BaseNode* parent, const TSNode& node){
    for ( uint32_t i = 0; i < ts_node_child_count(node); ++i ){
        TSNode child = ts_node_child(node, i);
        if ( ts_node_is_named(child) ){
            return visitQualifiedHead(parent, child);
        }
    }
    return nullptr;
}

IdentifierNode* TypeNode::visitQualifiedHead(BaseNode* parent, const TSNode& node){
    if ( strcmp(ts_node_type(node), "identifier") == 0 ){
        IdentifierNode* identifier = parent->addCreatedChild(IdentifierNode::createNonEmpty(node, parent));

        TypeNode* type = findTypeNode(parent);
        Occurrence occurrence(identifier, type ? type->nameSpace() : Occurrence::SpaceType);
        if ( type ){
            type->m_declaredIdentifiers.push_back(occurrence);
        }
        addUsedIdentifier(parent, occurrence);
        return identifier;
    }

    TSNode operand = BaseNode::nodeChildByFieldName(node, "object");
    if ( ts_node_is_null(operand) ){
        operand = BaseNode::nodeChildByFieldName(node, "module");
    }
    if ( ts_node_is_null(operand) ){
        operand = BaseNode::nodeChildByFieldName(node, "function");
    }
    if ( !ts_node_is_null(operand) ){
        IdentifierNode* head = visitQualifiedHead(parent, operand);
        if ( head ){
            declareQualifiedTail(parent, head, node);
        }
        return head;
    }

    for ( uint32_t i = 0; i < ts_node_child_count(node); ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "identifier") == 0 ){
            IdentifierNode* identifier = parent->addCreatedChild(IdentifierNode::createNonEmpty(child, parent));

            TypeNode* type = findTypeNode(parent);
            Occurrence occurrence(identifier, type ? type->nameSpace() : Occurrence::SpaceType);
            if ( type ){
                type->m_declaredIdentifiers.push_back(occurrence);
            }
            addUsedIdentifier(parent, occurrence);
            return identifier;
        }
    }
    return nullptr;
}

IdentifierNode* TypeNode::qualifiedTail(IdentifierNode* head) const{
    for ( const std::pair<IdentifierNode*, IdentifierNode*>& qualifiedName : m_qualifiedNames ){
        if ( qualifiedName.first == head ){
            return qualifiedName.second;
        }
    }
    return nullptr;
}

void TypeNode::declareQualifiedTail(BaseNode* parent, IdentifierNode* head, const TSNode& node){
    TypeNode* type = findTypeNode(parent);
    if ( !type || type->qualifiedTail(head) ){
        return;
    }

    TSNode tail = BaseNode::nodeChildByFieldName(node, "name");
    if ( ts_node_is_null(tail) ){
        tail = BaseNode::nodeChildByFieldName(node, "property");
    }
    if ( ts_node_is_null(tail) || ts_node_end_byte(tail) == ts_node_start_byte(tail) ){
        return;
    }

    IdentifierNode* tailIdentifier = parent->addCreatedChild(IdentifierNode::createNonEmpty(tail, parent));
    type->m_qualifiedNames.push_back(std::make_pair(head, tailIdentifier));
}

std::string TypeNode::sliceWithoutAnnotation(const std::string& source, TypeNode *tn){
    if ( !tn ){
        return "";
    }

    if ( strcmp(ts_node_type(tn->current()), "type_annotation") == 0 ){
        auto count = ts_node_child_count(tn->current());
        if ( count > 1 ){
            uint32_t start = ts_node_start_byte(ts_node_child(tn->current(), 1));
            uint32_t end   = ts_node_end_byte(ts_node_child(tn->current(), count - 1));
            return slice(source, start, end);
        }
        return slice(source, tn);
    }
    return slice(source, tn);
}

bool TypeNode::isReservedTypeName(const std::string& name){
    return name == "var"      || name == "let"    || name == "const" ||
           name == "default"  || name == "static" || name == "component" ||
           name == "instance" || name == "Object" || name == "Function";
}

}} // namespace lv, el
