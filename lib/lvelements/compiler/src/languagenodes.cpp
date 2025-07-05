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
#include <string.h>

namespace lv{ namespace el{

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

BaseNode::BaseNode(const TSNode &node, const LanguageNodeInfo::ConstPtr &ni)
    : m_parent(nullptr)
    , m_node(node)
    , m_nodeInfo(ni)
{
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

bool BaseNode::checkIdentifierDeclared(const std::string& source, BaseNode *node, std::string id, ConversionContext *ctx){
    if (id == "this" || id == "parent" || id == "import" )
        return true;
    if ( ConversionContext::isImplicitType(ctx, id) )
        return true;

    BaseNode* prog = nullptr;
    while (node){
        JsBlockNode* hasIds = dynamic_cast<JsBlockNode*>(node);

        if (!node->parent()){
            prog = node;
        }

        if (hasIds){
            for (auto it = hasIds->m_declarations.begin(); it != hasIds->m_declarations.end(); ++it){
                std::string declaredId = slice(source, *it);
                if (declaredId == id){
                    return true;
                }
            }
        }

        node = node->parent();
    }

    return false;
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

void BaseNode::collectImports(const std::string &source, std::vector<IdentifierNode*> &imp, ConversionContext *ctx){
    for ( BaseNode* node : m_children ){
        node->collectImports(source, imp, ctx);
    }
}

std::string BaseNode::slice(const std::string &source, uint32_t start, uint32_t end){
    return source.substr(start, end - start);
}

std::string BaseNode::slice(const std::string &source, BaseNode *node){
    return slice(source, node->startByte(), node->endByte());
}

JsBlockNode* BaseNode::addToDeclarations(BaseNode *parent, IdentifierNode *idNode){
    BaseNode* p = parent;
    while (p){
        JsBlockNode* jsbn = dynamic_cast<JsBlockNode*>(p);
        if (jsbn){
            jsbn->m_declarations.push_back(idNode);
            return jsbn;
            break;
        }
        p = p->parent();
    }
    return nullptr;
}

JsBlockNode *BaseNode::addUsedIdentifier(BaseNode *parent, IdentifierNode *idNode){
    BaseNode* p = parent;
    while (p){
        JsBlockNode* jsbn = dynamic_cast<JsBlockNode*>(p);
        if (jsbn){
            jsbn->m_usedIdentifiers.push_back(idNode);
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
            ts_node_start_point(node).column,
            ts_node_start_byte(node)
        );
        SourcePoint endPoint = SourcePoint(
            ts_node_end_point(node).row + 1,
            ts_node_end_point(node).column,
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
        result.push_back(new IdentifierNode(node));
    } else if ( strcmp(ts_node_type(node), "nested_identifier") == 0 || strcmp(ts_node_type(node), "member_expression") == 0 ){
        uint32_t count = ts_node_child_count(node);
        for ( uint32_t i = 0; i < count; ++i ){
            TSNode child = ts_node_child(node, i);
            assertError(parent, child, "Expected identifier.");
            if ( strcmp(ts_node_type(child), "identifier") == 0 ){
                result.push_back(new IdentifierNode(child));
            } else if ( strcmp(ts_node_type(child), "property_identifier") == 0 ){
                result.push_back(new IdentifierNode(child));
            } else if ( strcmp(ts_node_type(child), "nested_identifier") == 0 || strcmp(ts_node_type(child), "member_expression") == 0 ){
                auto nestedResult = fromNestedIdentifier(parent, child);
                result.insert( result.end(), nestedResult.begin(), nestedResult.end() );
            } else if ( (strcmp(ts_node_type(child), ".") != 0) ){
                throwError(parent, child, "Expected identifier.");
            }
        }
    }

    return result;
}

ParameterListNode *BaseNode::scanFormalParameters(BaseNode *parent, const TSNode& formalParameters){
    ParameterListNode* result = new ParameterListNode(formalParameters);

    uint32_t paramterCount = ts_node_child_count(formalParameters);

    for (uint32_t pc = 0; pc < paramterCount; ++pc){
        TSNode ftpc = ts_node_child(formalParameters, pc);
        assertError(parent, ftpc, "Function declaration not supported.");
        if (strcmp(ts_node_type(ftpc), "identifier") == 0){
            auto paramNode = result->addCreatedChild(new ParameterNode(ftpc, new IdentifierNode(ftpc)));
            result->m_parameters.push_back(paramNode);
        } else if ( strcmp(ts_node_type(ftpc), "required_parameter") == 0 ){
            auto name = BaseNode::nodeChildByFieldName(ftpc, "pattern");
            auto paramNode = result->addCreatedChild(new ParameterNode(ftpc, new IdentifierNode(name)));
            
            auto type = BaseNode::nodeChildByFieldName(ftpc, "type");
            if (!ts_node_is_null(type)) {
                paramNode->m_type = paramNode->addCreatedChild(new TypeNode(type));
            }
            result->m_parameters.push_back(paramNode);
        } else if ( strcmp(ts_node_type(ftpc), "optional_parameter") == 0 ){
            auto name = BaseNode::nodeChildByFieldName(ftpc, "pattern");
            auto paramNode = result->addCreatedChild(new ParameterNode(ftpc, new IdentifierNode(name)));
            paramNode->m_isOptional = true;
            
            auto type = BaseNode::nodeChildByFieldName(ftpc, "type");
            if (!ts_node_is_null(type)) {
                paramNode->m_type = paramNode->addCreatedChild(new TypeNode(type));
            }
            result->m_parameters.push_back(paramNode);
        }
    }
    return result;
}

ParameterListNode *BaseNode::scanFormalTypeParameters(BaseNode *parent, const TSNode &parameters){
    auto result = new ParameterListNode(parameters);
    uint32_t parameterCount = ts_node_child_count(parameters);

    for (uint32_t pc = 0; pc < parameterCount; ++pc){
        TSNode ftpc = ts_node_child(parameters, pc);
        assertError(parent, ftpc, "Function parameter not declared property.");
        if (strcmp(ts_node_type(ftpc), "formal_type_parameter") == 0){

            if ( ts_node_child_count(ftpc) > 0 ){
                TSNode typeParameter = ts_node_child(ftpc, 0);

                TSNode parameterName = BaseNode::nodeChildByFieldName(typeParameter, "name");
                assertValid(parent, parameterName, "Parameter name is null.");
                TSNode parameterType = BaseNode::nodeChildByFieldName(typeParameter, "type");
                assertValid(parent, parameterType, "Parameter type is null.");

                if ( strcmp(ts_node_type(typeParameter), "required_type_parameter") == 0 ){
                    result->m_parameters.push_back(
                        result->addCreatedChild(new ParameterNode(typeParameter, new IdentifierNode(parameterName), new TypeNode(parameterType), false))
                    );
                } else if ( strcmp(ts_node_type(typeParameter), "optional_type_parameter") == 0){
                    result->m_parameters.push_back(
                        result->addCreatedChild(new ParameterNode(typeParameter, new IdentifierNode(parameterName), new TypeNode(parameterType), true))
                    );
                }
            }
        }
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

BaseNode *BaseNode::visitRecognized(BaseNode *parent, const TSNode &node){
    if ( strcmp(ts_node_type(node), "import_statement") == 0 ){
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
        visitDestructuringPattern(parent, node);
        return nullptr;
    } else if ( strcmp(ts_node_type(node), "object_pattern") == 0 ){
        visitDestructuringPattern(parent, node);
        return nullptr;
    } else if ( strcmp(ts_node_type(node), "new_expression") == 0 ){
        return visitNewExpression(parent, node);
    } else if ( strcmp(ts_node_type(node), "return_statement") == 0 ){
        return visitReturnStatement(parent, node);
    } else if ( strcmp(ts_node_type(node), "object") == 0 ){
        return visitObject(parent, node);
    } else if ( strcmp(ts_node_type(node), "try_statement") == 0 ){
        return visitTryCatchBlock(parent, node);
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
            IdentifierNode* in = importNode->addCreatedChild(new IdentifierNode(aliasChild));
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
            IdentifierNode* name = importNode->addCreatedChild(new IdentifierNode(child));
            importNode->m_importNames.push_back(name);
            addToDeclarations(parent, name);
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
    IdentifierNode* identifierNode = parent->addCreatedChild(new IdentifierNode(node));
    addUsedIdentifier(parent, identifierNode);
    visitChildren(identifierNode, node);
    return identifierNode;
}

IdentifierNode* BaseNode::visitPropertyIdentifier(BaseNode *parent, const TSNode &node){
    IdentifierNode* identifierNode = parent->addCreatedChild(new IdentifierNode(node));
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

    TSNode name = nodeChildByFieldName(node, "name");
    if ( !ts_node_is_null(name) ){
        enode->m_name = enode->addCreatedChild(new IdentifierNode(name));
        addToDeclarations(parent, enode->m_name);
    }

    TSNode heritage = nodeChildByFieldName(node, "heritage");
    if ( !ts_node_is_null(heritage) ){
        if ( strcmp(ts_node_type(heritage), "component_heritage") == 0 ){
            uint32_t heritageCount = ts_node_child_count(heritage);
            for ( uint32_t j = 0; j < heritageCount; ++j ){
                TSNode heritageSegment = ts_node_child(heritage, j);
                if ( strcmp(ts_node_type(heritageSegment), "identifier" ) == 0 ){
                    IdentifierNode* heritageSegmentNode = enode->addCreatedChild(new IdentifierNode(heritageSegment));
                    enode->m_heritage.push_back(heritageSegmentNode);
                }
            }
        } else if ( strcmp(ts_node_type(heritage), "component_short_heritage") == 0 ){
            uint32_t heritageCount = ts_node_child_count(heritage);
            for ( uint32_t j = 0; j < heritageCount; ++j ){
                TSNode heritageSegment = ts_node_child(heritage, j);
                if ( strcmp(ts_node_type(heritageSegment), "identifier" ) == 0 ){
                    IdentifierNode* heritageSegmentNode = enode->addCreatedChild(new IdentifierNode(heritageSegment));
                    enode->m_heritage.push_back(heritageSegmentNode);
                }
            }
        }

        if ( enode->m_heritage.size() > 0 ){
            addUsedIdentifier(enode, enode->m_heritage[0]);
        }
    }


    TSNode identifier = nodeChildByFieldName(node, "id");
    if ( !ts_node_is_null(identifier) ){
        if ( ts_node_named_child_count(identifier) > 0 ){
            TSNode idChild = ts_node_child(identifier, 1);
            enode->m_id = enode->addCreatedChild(new IdentifierNode(idChild));
            addToDeclarations(parent, enode->m_id);
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

    if (parent->isNodeType<ComponentInstanceStatementNode>()
        && parent->parent()
        && parent->parent()->isNodeType<ProgramNode>() )
    {
        enode = parent->addCreatedChild(new RootNewComponentExpressionNode(node));
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
            auto iden = enode->addCreatedChild(new IdentifierNode(child));
            enode->m_name.push_back(iden);
            if ( enode->m_name.size() == 1 )
                addUsedIdentifier(enode, iden);
        } else if ( strcmp(ts_node_type(child), "nested_identifier") == 0 ){
            enode->m_name = BaseNode::fromNestedIdentifier(enode, child);
            for( IdentifierNode* name : enode->m_name )
                enode->addChild(name);
            if ( enode->m_name.size() > 0 ){
                addUsedIdentifier(enode, enode->m_name.front());
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
            enode->m_id = enode->addCreatedChild(new IdentifierNode(id));
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
                    addToDeclarations(body, enode->m_id);
                    break;
                }
            } else if (p->isNodeType<RootNewComponentExpressionNode>() ){
                auto rnce = p->as<RootNewComponentExpressionNode>();
                rnce->m_idComponents.push_back(enode);
                addToDeclarations(rnce, enode->m_id);
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
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        assertError(parent, child, "Unexpected token.");
        if ( strcmp(ts_node_type(child), "component_instance") == 0 ){
            TSNode id = ts_node_child(child, 1);
            if (strcmp(ts_node_type(id), "identifier") != 0)
                continue;
            enode->m_name = enode->addCreatedChild(new IdentifierNode(id));
            addToDeclarations(parent, enode->m_name);
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
            assignment->m_name = assignment->addCreatedChild(new IdentifierNode(assignmentName));
            assignment->m_expression = assignment->addCreatedChild(new BindableExpressionNode(assignmentExpression));
            enode->m_assignments.push_back(assignment);

            visit(assignment->m_expression, assignmentExpression);
        }
    }

    return enode;
}

PropertyDeclarationNode* BaseNode::visitPropertyDeclaration(BaseNode *parent, const TSNode &node){
    PropertyDeclarationNode* enode = parent->addCreatedChild(new PropertyDeclarationNode(node));

    TSNode propName = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, propName, "Property name is null.");
    enode->m_name = enode->addCreatedChild(new IdentifierNode(propName));
    TSNode propType = BaseNode::nodeChildByFieldName(node, "type");
    if ( !ts_node_is_null(propType) ){
        enode->m_type = enode->addCreatedChild(new TypeNode(propType));
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_assignment_expression") == 0 ){
            enode->m_expression = enode->addCreatedChild(new BindableExpressionNode(child));
            visitChildren(enode->m_expression, child);
        } else if (strcmp(ts_node_type(child), "statement_block") == 0) {
            enode->m_statementBlock = enode->addCreatedChild(new JsBlockNode(child));
            visitChildren(enode->m_statementBlock, child);
        } else if ( strcmp(ts_node_type(child), "=") == 0 ){
            enode->m_isBindingAssignment = false;
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->pushToProperties(enode);
    }

    if (parent->parent() && (parent->parent()->isNodeType<NewComponentExpressionNode>() ||
        parent->parent()->isNodeType<RootNewComponentExpressionNode>() ))
    {
        parent->parent()->as<NewComponentExpressionNode>()->pushToProperties(enode);
    }
    return enode;
}

StaticPropertyDeclarationNode* BaseNode::visitStaticPropertyDeclaration(BaseNode *parent, const TSNode &node){
    StaticPropertyDeclarationNode* enode = parent->addCreatedChild(new StaticPropertyDeclarationNode(node));

    TSNode propName = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, propName, "Property name is null.");
    enode->m_name = enode->addCreatedChild(new IdentifierNode(propName));
    TSNode propType = BaseNode::nodeChildByFieldName(node, "type");
    if ( !ts_node_is_null(propType) ){
        enode->m_type = enode->addCreatedChild(new TypeNode(propType));
    }

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_expression_initializer" ) == 0 ){
            uint32_t initCount = ts_node_child_count(child);
            if ( initCount == 2 ){
                TSNode initChild = ts_node_child(child, 1);
                enode->m_expression = enode->addCreatedChild(new BindableExpressionNode(initChild));
                visitChildren(enode->m_expression, initChild);
            }
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        auto componentDeclaration = parent->parent()->as<ComponentDeclarationNode>();
        if ( componentDeclaration->isAnonymous() ){
            throwError(enode, node, "Cannot declare static members for an anonymous component.");
        }

        componentDeclaration->pushToStaticProperties(enode);
    }

    if (parent->parent() && (parent->parent()->isNodeType<NewComponentExpressionNode>() ||
        parent->parent()->isNodeType<RootNewComponentExpressionNode>() ) )
    {
        throwError(enode, node, "Cannot declare static members on a new expression.");
    }
    return enode;
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
            enode->m_children.insert(enode->m_children.begin(), new IdentifierNode(child));
        }
    }

    if ( parent->isNodeType<CallExpressionNode>() ){
        if ( enode->children().size() > 0 && enode->children()[0]->isNodeType<MemberExpressionNode>() ){
            enode = enode->children()[0]->as<MemberExpressionNode>();
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
                auto child = enode->children()[0];
                if (!child->isNodeType<MemberExpressionNode>()  && !child->isNodeType<IdentifierNode>() )
                    break;
                p->as<PropertyDeclarationNode>()->pushToBindings(enode);
                break;
            }
            if (p->isNodeType<PropertyAssignmentNode>() ){
                auto child = enode->children()[0];
                if ( !child->isNodeType<MemberExpressionNode>() && !child->isNodeType<IdentifierNode>() )
                    break;
                p->as<PropertyAssignmentNode>()->pushToBindings(enode);
                break;
            }
            prevP = p;
            p = p->parent();
        }
    }
    return enode;
}

SubscriptExpressionNode* BaseNode::visitSubscriptExpression(BaseNode *parent, const TSNode &node){
    SubscriptExpressionNode* enode = parent->addCreatedChild(new SubscriptExpressionNode(node));
    visitChildren(enode, node);
    return enode;
}

PropertyAssignmentNode* BaseNode::visitPropertyAssignment(BaseNode *parent, const TSNode &node){
    PropertyAssignmentNode* enode = parent->addCreatedChild(new PropertyAssignmentNode(node));
    uint32_t count = ts_node_child_count(node);

    TSNode propertyName = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, propertyName, "Failed to find property name.");

    auto propertyIdentifiers = fromNestedIdentifier(enode, propertyName);
    for ( IdentifierNode* inode : propertyIdentifiers ){
        enode->addChild(inode);
    }
    enode->m_property = propertyIdentifiers;

    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_assignment_expression") == 0 ){
            enode->m_expression = enode->addCreatedChild(new BindableExpressionNode(child));
            visitChildren(enode->m_expression, child);
        } else if (strcmp(ts_node_type(child),"statement_block") == 0) {
            enode->m_statementBlock = enode->addCreatedChild(new JsBlockNode(child));
            visitChildren(enode->m_statementBlock, child);
        } else if ( strcmp(ts_node_type(child), "=") == 0 ){
            enode->m_isBindingAssignment = false;
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->pushToAssignments(enode);
    }

    if (parent->parent() && (parent->parent()->isNodeType<NewComponentExpressionNode>() ||
                             parent->parent()->isNodeType<RootNewComponentExpressionNode>() ))
    {
        parent->parent()->as<NewComponentExpressionNode>()->pushToAssignments(enode);
    }
    return enode;
}

IdentifierNode* BaseNode::visitIdentifierAssignment(BaseNode *parent, const TSNode &node){
    BaseNode* componentParent = parent ? parent->parent() : nullptr;
    if ( !componentParent )
        return nullptr;

    TSNode idChild = ts_node_child(node, 2);

    if ( componentParent->isNodeType<ComponentDeclarationNode>() ){
        ComponentDeclarationNode* cdn = componentParent->as<ComponentDeclarationNode>();
        cdn->m_id = parent->addCreatedChild(new IdentifierNode(idChild));
        // provide id to outside scope
        addToDeclarations(componentParent, cdn->m_id);
        return cdn->m_id;
    } else if ( componentParent->isNodeType<NewComponentExpressionNode>() ||
                componentParent->isNodeType<RootNewComponentExpressionNode>() )
    {
        NewComponentExpressionNode* ncen = componentParent->as<NewComponentExpressionNode>();
        ncen->m_id = parent->addCreatedChild(new IdentifierNode(idChild));
        // provide id to outside scope
        addToDeclarations(componentParent, ncen->m_id);
        return ncen->m_id;
    }

    return nullptr;
}

EventDeclarationNode* BaseNode::visitEventDeclaration(BaseNode *parent, const TSNode &node){
    EventDeclarationNode* enode = parent->addCreatedChild(new EventDeclarationNode(node));

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Event name is null.");
    enode->m_name = parent->addCreatedChild(new IdentifierNode(name));

    TSNode parameters = nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Event parameters are null.");

    enode->m_parameters = BaseNode::scanFormalTypeParameters(parent, parameters);
    enode->addChild(enode->m_parameters);

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
            enode->m_name = enode->addCreatedChild(new IdentifierNode(child));
        } else if ( strcmp(ts_node_type(child), "async") == 0 ){
            enode->m_async = true;
        } else if ( strcmp(ts_node_type(child), "formal_parameters") == 0 ){
            enode->m_parameters = scanFormalParameters(parent, child);
            enode->addChild(enode->m_parameters);
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
                addToDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]->identifier());
            }
        } else {
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addToDeclarations(enode, enode->m_parameters->m_parameters[i]->identifier());
            }
        }
    }
    return enode;
}

MethodDefinitionNode* BaseNode::visitMethodDefinition(BaseNode *parent, const TSNode &node){
    MethodDefinitionNode* enode = parent->addCreatedChild(new MethodDefinitionNode(node));

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "property_identifier") == 0 ){
            enode->m_name = enode->addCreatedChild(new IdentifierNode(child));
        } else if ( strcmp(ts_node_type(child), "formal_parameters") == 0 ){
            enode->m_parameters = BaseNode::scanFormalParameters(parent, child);
            enode->addChild(enode->m_parameters);
        } else if ( strcmp(ts_node_type(child), "statement_block") == 0 ){
            enode->m_body = enode->addCreatedChild(new JsBlockNode(child));
            visitChildren(enode->m_body, child);
        }
    }
    return enode;
}

TypedMethodDeclarationNode* BaseNode::visitTypedMethodDeclaration(BaseNode *parent, const TSNode &node){
    TypedMethodDeclarationNode* enode = parent->addCreatedChild(new TypedMethodDeclarationNode(node));

    TSNode name = nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Function name is null.");
    enode->m_name = enode->addCreatedChild(new IdentifierNode(name));

    TSNode parameters = nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Function parameters are null.");

    enode->m_parameters = BaseNode::scanFormalTypeParameters(parent, parameters);
    enode->addChild(enode->m_parameters);

    TSNode body = nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Function body is null.");

    enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
    visitChildren(enode->m_body, body);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){

        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "static") == 0 ){
            enode->setStatic(true);
        } else if ( strcmp(ts_node_type(child), "async") == 0 ){
            enode->setAsync(true);
        }
    }

    if (parent->parent() && parent->parent()->isNodeType<ComponentDeclarationNode>() ){
        parent->parent()->as<ComponentDeclarationNode>()->m_methods.push_back(enode);
    }
    if (parent->parent() && parent->parent()->canCast<NewComponentExpressionNode>()){
        parent->parent()->as<NewComponentExpressionNode>()->m_methods.push_back(enode);
    }

    if (enode->m_body && enode->m_parameters){
        for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
            addToDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]->identifier());
        }
    }
    return enode;
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
    enode->m_name = enode->addCreatedChild(new IdentifierNode(name));

    if ( enode->m_access == PropertyAccessorDeclarationNode::Setter ){
        TSNode parameters = nodeChildByFieldName(node, "parameters");
        assertValid(parent, parameters, "Set parameters are null.");
        enode->m_parameters = scanFormalTypeParameters(enode, parameters);
        enode->addChild(enode->m_parameters);
    }

    TSNode body = nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Accessor body is null.");

    enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
    visitChildren(enode->m_body, body);

    if (enode->m_body && enode->m_parameters ){
        for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
            auto param = enode->m_parameters->m_parameters[i];
            addToDeclarations(enode->m_body, param->identifier());
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
    ConstructorDefinitionNode* cdnode = parent->addCreatedChild(new ConstructorDefinitionNode(node));
    // visitChildren(cdnode, node);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "statement_block") == 0 ){
            cdnode->m_body = cdnode->addCreatedChild(new JsBlockNode(child));
            visitChildren(cdnode->m_body, child);
        } else if ( strcmp(ts_node_type(child), "formal_type_parameters") == 0 ){
            TSNode& parameters = child;

            cdnode->m_parameters = cdnode->addCreatedChild(new ParameterListNode(parameters));
            uint32_t parameterCount = ts_node_child_count(parameters);

            for (uint32_t pc = 0; pc < parameterCount; ++pc){
                TSNode ftpc = ts_node_child(parameters, pc);
                assertError(parent, ftpc, "Constructor parameter not declared properly.");
                if (strcmp(ts_node_type(ftpc), "formal_type_parameter") == 0){

                    if ( ts_node_child_count(ftpc) > 0 ){
                        TSNode typeParameter = ts_node_child(ftpc, 0);

                        TSNode parameterName = BaseNode::nodeChildByFieldName(typeParameter, "name");
                        assertValid(cdnode, parameterName, "Parameter name is null.");
                        TSNode parameterType = BaseNode::nodeChildByFieldName(typeParameter, "type");
                        assertValid(cdnode, parameterType, "Parameter type is null.");

                        if ( strcmp(ts_node_type(typeParameter), "required_type_parameter") == 0 ){
                            cdnode->m_parameters->m_parameters.push_back(
                                cdnode->addCreatedChild(new ParameterNode(typeParameter, new IdentifierNode(parameterName), new TypeNode(parameterType), false))
                            );
                        } else if ( strcmp(ts_node_type(typeParameter), "optional_type_parameter") == 0){
                            cdnode->m_parameters->m_parameters.push_back(
                                cdnode->addCreatedChild(new ParameterNode(typeParameter, new IdentifierNode(parameterName), new TypeNode(parameterType), true))
                            );
                        }
                    }
                }
            }
        } else
            visit(cdnode, child);
    }

    if (parent->isNodeType<ComponentBodyNode>() ){
        parent->as<ComponentBodyNode>()->setConstructor(cdnode);
    }

    if (cdnode->m_body){
        for (size_t i = 0; i != cdnode->m_parameters->parameters().size(); ++i){
            addToDeclarations(cdnode->m_body, cdnode->m_parameters->m_parameters[i]->identifier());
        }
    }
    return cdnode;
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

    // Function parameters
    const TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");

    assertValid(parent, parameters, "Function parameters are null.");
    enode->m_parameters = BaseNode::scanFormalParameters(parent, parameters);
    enode->addChild(enode->m_parameters);

    // Function return type
    const TSNode returnType = BaseNode::nodeChildByFieldName(node, "return_type");
    if (!ts_node_is_null(returnType)) {
        enode->m_returnType = enode->addCreatedChild(new TypeNode(returnType));
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
                addToDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]->identifier());
            }
        }
    }
    return enode;
}

FunctionDeclarationNode* BaseNode::visitFunctionDeclaration(BaseNode *parent, const TSNode &node)
{
    FunctionDeclarationNode* enode = parent->addCreatedChild(new FunctionDeclarationNode(node));

    // Function name
    const TSNode name = BaseNode::nodeChildByFieldName(node, "name");
    assertValid(parent, name, "Function name is null.");
    enode->m_name = enode->addCreatedChild(new IdentifierNode(name));
    addToDeclarations(parent, enode->m_name);

    // Function parameters
    const TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");
    assertValid(parent, parameters, "Function parameters are null.");
    enode->m_parameters = BaseNode::scanFormalParameters(parent, parameters);
    enode->addChild(enode->m_parameters);

    // Function return type
    const TSNode returnType = BaseNode::nodeChildByFieldName(node, "return_type");
    if (!ts_node_is_null(returnType)) {
        enode->m_returnType = enode->addCreatedChild(new TypeNode(returnType));
    }

    // Function properties
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "async") == 0 ){
            enode->m_async = true;
        }
    }

    // Function body
    const TSNode body = BaseNode::nodeChildByFieldName(node, "body");
    assertValid(parent, body, "Function body is null.");
    enode->m_body = enode->addCreatedChild(new JsBlockNode(body));
    visitChildren(enode->m_body, body);

    if ( enode->m_parameters ){
        if (enode->m_body ){
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addToDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]->identifier());
            }
        }
    }
    return enode;
}

ClassDeclarationNode* BaseNode::visitClassDeclaration(BaseNode *parent, const TSNode &node)
{
    ClassDeclarationNode* fnode = parent->addCreatedChild(new ClassDeclarationNode(node));
    visitChildren(fnode, node);

    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    if ( !ts_node_is_null(nameNode) && strcmp(ts_node_type(nameNode), "identifier") != 0){
        fnode->addChild(new IdentifierNode(nameNode));
    }

    for (auto it = fnode->children().begin(); it != fnode->children().end(); ++it){
        if ((*it)->isNodeType<IdentifierNode>() )
        {
            // this must be the id!
            IdentifierNode* id = static_cast<IdentifierNode*>(*it);
            addToDeclarations(parent, id);
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
    VariableDeclarationNode* vdn = parent->addCreatedChild(new VariableDeclarationNode(node));
    vdn->m_declarationForm = static_cast<VariableDeclarationNode::DeclarationForm>(form);

    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "variable_declarator") == 0 ){
            VariableDeclaratorNode* declaratorNode = vdn->addCreatedChild(new VariableDeclaratorNode(child));
            vdn->m_declarators.push_back(declaratorNode);
            
            TSNode name = BaseNode::nodeChildByFieldName(child, "name");
            TSNode type = BaseNode::nodeChildByFieldName(child, "type");
            TSNode value = BaseNode::nodeChildByFieldName(child, "value");

            declaratorNode->m_name = declaratorNode->addCreatedChild(new IdentifierNode(name));
            addToDeclarations(declaratorNode, declaratorNode->m_name);
            if (!ts_node_is_null(type)) {
                declaratorNode->m_type = declaratorNode->addCreatedChild(new TypeNode(type));
            }
            if (!ts_node_is_null(value)) {
                declaratorNode->m_value = declaratorNode->addCreatedChild(new ExpressionNode(value));
                visit(declaratorNode->m_value, value);
            }
        } else if ( strcmp(ts_node_type(child), ";") == 0 ) {
            vdn->m_hasSemicolon = true;
        } else {
            visit(vdn, child);
        }
    }
    return vdn;
}


void BaseNode::visitDestructuringPattern(BaseNode *parent, const TSNode &node){
    uint32_t count = ts_node_child_count(node);
    for ( uint32_t i = 0; i < count; ++i ){
        TSNode child = ts_node_child(node, i);
        if ( strcmp(ts_node_type(child), "identifier") == 0 ){
            auto inode = parent->addCreatedChild(new IdentifierNode(child));
            addToDeclarations(parent, inode);
        } else if ( strcmp(ts_node_type(child), "shorthand_property_identifier") == 0 ){
            auto inode = parent->addCreatedChild(new IdentifierNode(child));
            addToDeclarations(parent, inode);
        } else {
            visitDestructuringPattern(parent, child);
        }
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
            auto inode = nenode->addCreatedChild(new IdentifierNode(child));
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

    // Function parameters
    const TSNode parameters = BaseNode::nodeChildByFieldName(node, "parameters");
    if (!ts_node_is_null(parameters)) {
        enode->m_parameters = BaseNode::scanFormalParameters(parent, parameters);
        enode->addChild(enode->m_parameters);
    } else {
        const TSNode parameter = BaseNode::nodeChildByFieldName(node, "parameter");
        if (!ts_node_is_null(parameter)) {
            enode->m_parameters = enode->addCreatedChild(new ParameterListNode(parameter));
            auto paramNode = enode->m_parameters->addCreatedChild(new ParameterNode(parameter, new IdentifierNode(parameter)));
            enode->m_parameters->m_parameters.push_back(paramNode);
        }
    }

    // Function return type
    const TSNode returnType = BaseNode::nodeChildByFieldName(node, "return_type");
    if (!ts_node_is_null(returnType)) {
        enode->m_returnType = enode->addCreatedChild(new TypeNode(returnType));
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
                addToDeclarations(enode->m_body, enode->m_parameters->m_parameters[i]->identifier());
            }
        } else {
            for (size_t i = 0; i != enode->m_parameters->m_parameters.size(); ++i){
                addToDeclarations(enode, enode->m_parameters->m_parameters[i]->identifier());
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
                auto typeNode = !ts_node_is_null(parameterTypeNode) ? new TypeNode(parameterTypeNode) : nullptr;
                enode->m_catchParameter = enode->addCreatedChild(
                    new ParameterNode(parameterNode, new IdentifierNode(parameterNode), typeNode)
                );
            }
            TSNode catchBody = BaseNode::nodeChildByFieldName(child, "body");
            if ( !ts_node_is_null(catchBody) ){
                enode->m_catchBody = enode->addCreatedChild(new JsBlockNode(catchBody));
                if ( enode->m_catchParameter ){
                    addToDeclarations(enode->m_catchBody, enode->m_catchParameter->identifier());
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

    std::vector<IdentifierNode*> identifiers;
    collectImports(source, identifiers, ctx);

    for ( auto identifier : identifiers ){

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
                            addImportType(impt);
                        }
                    } else if ( parent->isNodeType<MemberExpressionNode>() ){
                        auto parentCast = parent->as<MemberExpressionNode>();
                        if ( parentCast->children().size() > 1 && parentCast->children()[0] == identifier ){
                            ProgramNode::ImportType impt;
                            impt.importNamespace = idName;
                            impt.name = slice(source, parentCast->children()[1]);
                            impt.location = identifier->startPoint();
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
            addImportType(impt);
        }
    }

    m_importTypesCollected = true;
}

void ProgramNode::resolveImport(const std::string &as, const std::string &name, const std::string &path){
    auto asIt = m_importTypes.find(as);
    if ( asIt != m_importTypes.end() ){
        auto nameIt = asIt->second.find(name);
        if ( nameIt != asIt->second.end() ){
            nameIt->second.resolvedPath = path;
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

void ProgramNode::collectImports(const std::string &source, std::vector<IdentifierNode *> &identifiers, ConversionContext *ctx){
    for ( BaseNode* child: m_exports ){
        child->collectImports(source, identifiers, ctx);
    }
}

void ProgramNode::addImportType(const ProgramNode::ImportType &t){
    auto asIt = m_importTypes.find(t.importNamespace);
    if ( asIt == m_importTypes.end() ){
        std::map<std::string, ProgramNode::ImportType> imports;
        imports[t.name] = t;
        m_importTypes[t.importNamespace] = imports;
    } else {
        asIt->second[t.name] = t;
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
    m_bindingContainer->setDeclarationCheck([this](const std::string& source, const std::string& name, BaseNode* m){
        BaseNode* parent = m->parent();
        while ( parent ){
            if ( parent == this ){
                break;
            }
            JsBlockNode* hasIds = dynamic_cast<JsBlockNode*>(parent);
            if (hasIds){
                for (auto it = hasIds->identifiers().begin(); it != hasIds->identifiers().end(); ++it){
                    std::string declaredId = slice(source, *it);
                    if (declaredId == name){
                        return true;
                    }
                }
            }
            parent = parent->parent();
        }
        return false;
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
    m_bindingContainer->setDeclarationCheck([this](const std::string& source, const std::string& name, BaseNode* m){
        BaseNode* parent = m->parent();
        while ( parent ){
            if ( parent == this ){
                break;
            }
            JsBlockNode* hasIds = dynamic_cast<JsBlockNode*>(parent);
            if (hasIds){
                for (auto it = hasIds->identifiers().begin(); it != hasIds->identifiers().end(); ++it){
                    std::string declaredId = slice(source, *it);
                    if (declaredId == name){
                        return true;
                    }
                }
            }
            parent = parent->parent();
        }
        return false;
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

std::string ParameterListNode::toString(int indent) const{
    std::string result;
   if ( indent > 0 )
       result.assign(indent * 2, ' ');

    std::string parameters = "(no parameters: " + std::to_string(m_parameters.size()) + ")";

    result += "ParameterList " + rangeString() + parameters + "\n";
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
    : BaseNode(node, TypedMethodDeclarationNode::nodeInfo())
    , m_name(nullptr)
    , m_parameters(nullptr)
    , m_body(nullptr)
    , m_async(false)
    , m_static(false)
{
}

TypedMethodDeclarationNode::TypedMethodDeclarationNode(const TSNode &node, const LanguageNodeInfo::ConstPtr &ni)
    : BaseNode(node, ni)
    , m_name(nullptr)
    , m_parameters(nullptr)
    , m_body(nullptr)
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

void JsBlockNode::collectImports(const std::string &source, std::vector<IdentifierNode *> &identifiers, ConversionContext *ctx){
    BaseNode::collectImports(source, identifiers, ctx);
    collectBlockImports(source, identifiers, ctx);
}

void JsBlockNode::collectBlockImports(const std::string &source, std::vector<IdentifierNode *> &identifiers, ConversionContext* ctx){
    for ( auto identifier : m_usedIdentifiers ){
        if ( !checkIdentifierDeclared(source, this, slice(source, identifier), ctx) ){
            identifiers.push_back(identifier);
        }
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

}} // namespace lv, el

