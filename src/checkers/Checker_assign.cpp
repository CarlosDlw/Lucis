#include "checkers/Checker.h"
#include "checkers/Checker_helpers.h"

#include "generated/LucisLexer.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"
#include "attributes/CfgPredicate.h"
#include "attributes/TargetInfo.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>



void Checker::checkAssignStmt(LucisParser::AssignStmtContext* stmt) {
    auto name = stmt->IDENTIFIER()->getText();
    auto it = locals_.find(name);

    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + name + "'");
        return;
    }

    if (it->second.isConst) {
        error(stmt, "cannot assign to constant '" + name + "'");
        return;
    }

    // Mark variable as initialized on assignment
    it->second.initialized = true;

    // Validate index expressions and RHS
    auto indexExprs = stmt->expression();

    auto* varType = it->second.type;

    // User-defined opIndex on struct: obj[i] = val
    if (indexExprs.size() >= 2 && varType && varType->kind == TypeKind::Struct) {
        const OperatorInfo* setter = nullptr;
        for (auto* t = varType; t; t = t->parentType) {
            auto it = operatorMethods_.find(t->name);
            if (it == operatorMethods_.end()) continue;
            for (auto& o : it->second) {
                if (o.internalName == "opIndex" && o.paramTypes.size() == 2)
                    setter = &o;
            }
        }
        if (setter) {
            auto* idxType = resolveExprType(indexExprs[0]);
            if (idxType && !isAssignable(setter->paramTypes[0], idxType))
                error(stmt, "index type mismatch: expected '" +
                             setter->paramTypes[0]->name + "', got '" +
                             idxType->name + "'");
            auto* rhsType = resolveExprType(indexExprs.back());
            if (rhsType && !isAssignable(setter->paramTypes[1], rhsType))
                error(stmt, "value type mismatch: expected '" +
                             setter->paramTypes[1]->name + "', got '" +
                             rhsType->name + "'");
            return;
        }
        auto* getter = findOperatorInChain(varType, "opIndex");
        if (getter) {
            error(stmt, "operator '[]' for type '" + varType->name +
                         "' requires a setter overload with value parameter");
            return;
        }
    }

    // set<T> has no subscript assignment (its API is add/contains).
    if (varType && varType->kind == TypeKind::Extended &&
        varType->extendedKind == "Set" && !indexExprs.empty()) {
        error(stmt, "set<" + (varType->elementType ? varType->elementType->name : "?") +
                     "> does not support indexing");
        return;
    }

    // For Map<K,V>, subscript assignment: m[key] = val
    if (indexExprs.size() == 2 && varType &&
        varType->kind == TypeKind::Extended && varType->keyType) {
        auto* keyType = resolveExprType(indexExprs[0]);
        auto* rhsType = resolveExprType(indexExprs[1]);

        if (keyType && !isAssignable(varType->keyType, keyType))
            error(stmt, "map key type mismatch: expected '" +
                         varType->keyType->name + "', got '" + keyType->name + "'");
        if (rhsType && varType->valueType && !isAssignable(varType->valueType, rhsType))
            error(stmt, "map value type mismatch: expected '" +
                         varType->valueType->name + "', got '" + rhsType->name + "'");
        return;
    }

    for (size_t i = 0; i + 1 < indexExprs.size(); i++) {
        auto* idxType = resolveExprType(indexExprs[i]);
        if (idxType && !isInteger(idxType))
            error(stmt, "index must be integer, got '" +
                             idxType->name + "'");
    }

    if (!indexExprs.empty()) {
        auto* rhsType = resolveExprType(indexExprs.back());
        // Resolve expected assignment type after each index operation.
        auto* expectedType = varType;
        for (size_t i = 0; i + 1 < indexExprs.size(); i++) {
            if (!expectedType) break;

            // Auto-dereference pointers before resolving element type
            if (expectedType->kind == TypeKind::Pointer && expectedType->pointeeType)
                expectedType = expectedType->pointeeType;

            // m[key] = value
            if (expectedType->kind == TypeKind::Extended && expectedType->keyType) {
                expectedType = expectedType->valueType;
                continue;
            }

            // vec[i] = value
            if (expectedType->kind == TypeKind::Extended && expectedType->elementType) {
                expectedType = expectedType->elementType;
                continue;
            }
        }
        if (rhsType && expectedType && !isAssignable(expectedType, rhsType)) {
            error(stmt, "type mismatch: cannot assign '" + rhsType->name +
                             "' to variable '" + name + "' of type '" +
                             expectedType->name + "'");
        } else if (rhsType && expectedType && indexExprs.size() == 1) {
            // Whole-variable assignment: check array dimension mismatch
            unsigned varDims = it->second.arrayDims;
            bool isArrayLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(indexExprs.back()) != nullptr;
            if (varDims == 0 && isArrayLit) {
                error(stmt, "type mismatch: cannot assign array literal to non-array variable '" +
                                 name + "'");
            } else if (varDims > 0 && !isArrayLit) {
                bool isScalarLit =
                    dynamic_cast<LucisParser::IntLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::HexLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::OctLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::BinLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::FloatLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::BoolLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::CharLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::StrLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::CStrLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(indexExprs.back());
                if (isScalarLit) {
                    error(stmt, "type mismatch: cannot assign scalar to array variable '" +
                                     name + "'");
                }
            }
        }
        // Check: assigning a negative constant to an unsigned integer type
        checkNegativeToUnsigned(expectedType, indexExprs.back(), stmt);

        // Whole-variable assignment (x = expr) can change tracked buffer state.
        if (indexExprs.size() == 1) {
            if (isDropTrackedType(it->second.type, it->second.arrayDims)) {
                updateOwnershipOnInitialization(it->second, indexExprs.back());
            }
            markExprAsMoved(indexExprs.back(), stmt);
            trackVarBufferFromExpr(name, indexExprs.back(), varType);
            trackVarNumericRangeFromExpr(name, indexExprs.back(), varType);
        }
    }
}



void Checker::checkCompoundAssignStmt(LucisParser::CompoundAssignStmtContext* stmt) {
    auto name = stmt->IDENTIFIER()->getText();
    auto it = locals_.find(name);

    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + name + "'");
        return;
    }

    auto* varType = it->second.type;
    auto* rhsType = resolveExprType(stmt->expression());
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: ptr ??= default
    if (opText == "??=") {
        if (varType && varType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer variable, got '" +
                             varType->name + "'");
        if (rhsType && varType && !isAssignable(varType, rhsType))
            error(stmt, "'??=' type mismatch: variable type '" +
                             varType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    // string += string → string concatenation
    if (opText == "+=" && varType && varType->kind == TypeKind::String) {
        if (rhsType && rhsType->kind != TypeKind::String)
            error(stmt, "cannot concatenate 'string' with '" + rhsType->name + "' using '+='");
        return;
    }

    bool isPtrArith = varType && varType->kind == TypeKind::Pointer &&
                      (opText == "+=" || opText == "-=");

    bool needsNumeric = !isPtrArith && (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (isPtrArith) {
        if (rhsType && !isInteger(rhsType))
            error(stmt, "pointer arithmetic '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
    } else {
        if (needsNumeric && varType && !isNumeric(varType))
            error(stmt, "operator '" + opText +
                             "' requires numeric variable, got '" + varType->name + "'");
        if (needsInteger && varType && !isIntegerOrPointer(varType))
            error(stmt, "operator '" + opText +
                             "' requires integer variable, got '" + varType->name + "'");
        if (needsNumeric && rhsType && !isNumeric(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires numeric operand, got '" + rhsType->name + "'");
        if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
        // Type compatibility between variable and RHS
        if (rhsType && varType && !isAssignable(varType, rhsType))
            error(stmt, "type mismatch in compound assignment: variable type '" +
                        varType->name + "', cannot use '" + rhsType->name + "'");
    }

    // Compile-time division by zero check
    if (opText == "/=" || opText == "%=") {
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(stmt->expression())) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}



void Checker::checkFieldAssignStmt(LucisParser::FieldAssignStmtContext* stmt) {
    auto identifiers = stmt->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    if (it->second.isConst) {
        error(stmt, "cannot assign to constant '" + varName + "'");
        return;
    }

    // Walk the field chain: p.x.y = val
    auto* currentType = it->second.type;
    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();

        if (currentType && currentType->kind == TypeKind::Pointer && currentType->pointeeType)
            currentType = currentType->pointeeType;

        if (!currentType || (currentType->kind != TypeKind::Struct &&
                             currentType->kind != TypeKind::Union &&
                             currentType->kind != TypeKind::BitStruct)) {
            error(stmt, "'" +
                             (i == 1 ? varName : identifiers[i-1]->getText()) +
                             "' is not a struct or union type");
            return;
        }

        if (currentType->kind == TypeKind::BitStruct) {
            bool found = false;
            for (auto& bf : currentType->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) { found = true; break; }
            }
            if (!found) {
                error(stmt, "'" + currentType->name + "' has no field '" + fieldName + "'");
                return;
            }
            auto* rhsType = resolveExprType(stmt->expression());
            if (rhsType && rhsType->kind != TypeKind::Integer)
                error(stmt, "bitstruct field '" + fieldName + "' requires an integer value");
            return;
        }

        bool found = false;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                currentType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }
    }

    auto* rhsType = resolveExprType(stmt->expression());
    if (rhsType && currentType && !isAssignable(currentType, rhsType)) {
        error(stmt, "type mismatch in field assignment: expected '" +
                         currentType->name + "', got '" + rhsType->name + "'");
    }
}



void Checker::checkFieldCompoundAssignStmt(LucisParser::FieldCompoundAssignStmtContext* stmt) {
    auto identifiers = stmt->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    // Walk the field chain: p.x.y += val
    auto* currentType = it->second.type;
    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();

        if (currentType && currentType->kind == TypeKind::Pointer && currentType->pointeeType)
            currentType = currentType->pointeeType;

        if (!currentType || (currentType->kind != TypeKind::Struct &&
                             currentType->kind != TypeKind::Union &&
                             currentType->kind != TypeKind::BitStruct)) {
            error(stmt, "'" +
                             (i == 1 ? varName : identifiers[i-1]->getText()) +
                             "' is not a struct or union type");
            return;
        }

        if (currentType->kind == TypeKind::BitStruct) {
            bool found = false;
            for (auto& bf : currentType->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) { found = true; break; }
            }
            if (!found) {
                error(stmt, "'" + currentType->name + "' has no field '" + fieldName + "'");
                return;
            }
            auto* rhsType = resolveExprType(stmt->expression());
            if (rhsType && rhsType->kind != TypeKind::Integer)
                error(stmt, "bitstruct field '" + fieldName + "' requires an integer value");
            return;
        }

        bool found = false;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                currentType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }
    }

    auto* rhsType = resolveExprType(stmt->expression());
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: obj.field ??= default
    if (opText == "??=") {
        if (currentType && currentType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer field, got '" +
                             currentType->name + "'");
        if (rhsType && currentType && !isAssignable(currentType, rhsType))
            error(stmt, "'??=' type mismatch: field type '" +
                             currentType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool isPtrArith = currentType && currentType->kind == TypeKind::Pointer &&
                      (opText == "+=" || opText == "-=");

    bool needsNumeric = !isPtrArith && (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (isPtrArith) {
        if (rhsType && !isInteger(rhsType))
            error(stmt, "pointer arithmetic '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
    } else {
        if (needsNumeric && currentType && !isNumeric(currentType))
            error(stmt, "operator '" + opText +
                             "' requires numeric field, got '" + currentType->name + "'");
        if (needsInteger && currentType && !isIntegerOrPointer(currentType))
            error(stmt, "operator '" + opText +
                             "' requires integer field, got '" + currentType->name + "'");
        if (needsNumeric && rhsType && !isNumeric(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires numeric operand, got '" + rhsType->name + "'");
        if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
        // Type compatibility between field and RHS
        if (rhsType && currentType && !isAssignable(currentType, rhsType))
            error(stmt, "type mismatch in field compound assignment: field type '" +
                        currentType->name + "', cannot use '" + rhsType->name + "'");
    }

    // Compile-time division by zero check
    if (opText == "/=" || opText == "%=") {
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(stmt->expression())) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}



void Checker::checkArrowAssignStmt(LucisParser::ArrowAssignStmtContext* stmt) {
    auto ids = stmt->IDENTIFIER();
    if (ids.size() < 2) {
        error(stmt, "malformed '->' assignment");
        return;
    }

    auto varName = ids[0]->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    const TypeInfo* currentType = it->second.type;
    bool pointerBase = false;

    if (currentType && currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
        currentType = currentType->pointeeType;
        pointerBase = true;
    }

    // Resolve dotted base before arrow: a.b.c->x
    for (size_t i = 1; i + 1 < ids.size(); i++) {
        auto fieldName = ids[i]->getText();

        if (!currentType ||
            (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
            error(stmt, "cannot access field '" + fieldName +
                             "' on non-struct type");
            return;
        }

        bool found = false;
        const TypeInfo* nextType = nullptr;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                nextType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found || !nextType) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }

        currentType = nextType;
        if (currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
            currentType = currentType->pointeeType;
            pointerBase = true;
        }
    }

    if (!pointerBase || !currentType ||
        (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
        error(stmt, "'->' requires pointer to struct or union");
        return;
    }

    auto arrowField = ids.back()->getText();
    const TypeInfo* fieldType = nullptr;
    for (auto& field : currentType->fields) {
        if (field.name == arrowField) {
            fieldType = field.typeInfo;
            break;
        }
    }
    if (!fieldType) {
        error(stmt, "struct '" + currentType->name +
                         "' has no field '" + arrowField + "'");
        return;
    }

    auto* rhsType = resolveExprType(stmt->expression());
    if (rhsType && fieldType && !isAssignable(fieldType, rhsType)) {
        error(stmt, "type mismatch in arrow assignment: expected '" +
                         fieldType->name + "', got '" + rhsType->name + "'");
    }
}



void Checker::checkArrowCompoundAssignStmt(LucisParser::ArrowCompoundAssignStmtContext* stmt) {
    auto ids = stmt->IDENTIFIER();
    if (ids.size() < 2) {
        error(stmt, "malformed '->' compound assignment");
        return;
    }

    auto varName = ids[0]->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    const TypeInfo* currentType = it->second.type;
    bool pointerBase = false;

    if (currentType && currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
        currentType = currentType->pointeeType;
        pointerBase = true;
    }

    for (size_t i = 1; i + 1 < ids.size(); i++) {
        auto fieldName = ids[i]->getText();

        if (!currentType ||
            (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
            error(stmt, "cannot access field '" + fieldName +
                             "' on non-struct type");
            return;
        }

        bool found = false;
        const TypeInfo* nextType = nullptr;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                nextType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found || !nextType) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }

        currentType = nextType;
        if (currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
            currentType = currentType->pointeeType;
            pointerBase = true;
        }
    }

    if (!pointerBase || !currentType ||
        (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
        error(stmt, "'->' requires pointer to struct or union");
        return;
    }

    auto arrowField = ids.back()->getText();
    const TypeInfo* fieldType = nullptr;
    for (auto& field : currentType->fields) {
        if (field.name == arrowField) {
            fieldType = field.typeInfo;
            break;
        }
    }
    if (!fieldType) {
        error(stmt, "struct '" + currentType->name +
                         "' has no field '" + arrowField + "'");
        return;
    }

    auto* rhsType = resolveExprType(stmt->expression());
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: ptr->field ??= default
    if (opText == "??=") {
        if (fieldType && fieldType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer field, got '" +
                             fieldType->name + "'");
        if (rhsType && fieldType && !isAssignable(fieldType, rhsType))
            error(stmt, "'??=' type mismatch: field type '" +
                             fieldType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool isPtrArith = fieldType && fieldType->kind == TypeKind::Pointer &&
                      (opText == "+=" || opText == "-=");

    bool needsNumeric = !isPtrArith && (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (isPtrArith) {
        if (rhsType && !isInteger(rhsType))
            error(stmt, "pointer arithmetic '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
    } else {
        if (needsNumeric && fieldType && !isNumeric(fieldType))
            error(stmt, "operator '" + opText +
                             "' requires numeric field, got '" + fieldType->name + "'");
        if (needsInteger && fieldType && !isInteger(fieldType))
            error(stmt, "operator '" + opText +
                             "' requires integer field, got '" + fieldType->name + "'");
        if (needsNumeric && rhsType && !isNumeric(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires numeric operand, got '" + rhsType->name + "'");
        if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
        // Type compatibility between field and RHS
        if (rhsType && fieldType && !isAssignable(fieldType, rhsType))
            error(stmt, "type mismatch in arrow compound assignment: field type '" +
                        fieldType->name + "', cannot use '" + rhsType->name + "'");
    }

    if (opText == "/=" || opText == "%=") {
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(stmt->expression())) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}



void Checker::checkDerefAssignStmt(LucisParser::DerefAssignStmtContext* stmt) {
    if (stmt->IDENTIFIER()) {
        // *ptr = value;
        auto varName = stmt->IDENTIFIER()->getText();
        auto it = locals_.find(varName);

        if (it == locals_.end()) {
            error(stmt, "undefined variable '" + varName + "'");
            return;
        }

        if (it->second.type && it->second.type->kind != TypeKind::Pointer) {
            error(stmt, "cannot dereference non-pointer variable '" +
                             varName + "' of type '" + it->second.type->name + "'");
        }

        resolveExprType(stmt->expression(0));
    } else {
        // *(expr) = value;
        auto* ptrType = resolveExprType(stmt->expression(0));
        if (ptrType && ptrType->kind != TypeKind::Pointer) {
            error(stmt, "cannot dereference non-pointer expression of type '" +
                             ptrType->name + "'");
        }
        resolveExprType(stmt->expression(1));
    }
}



void Checker::checkDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext* stmt) {
    const TypeInfo* targetType = nullptr;

    if (stmt->IDENTIFIER()) {
        // *ptr op= value;
        auto varName = stmt->IDENTIFIER()->getText();
        auto it = locals_.find(varName);

        if (it == locals_.end()) {
            error(stmt, "undefined variable '" + varName + "'");
            return;
        }

        if (!it->second.type || it->second.type->kind != TypeKind::Pointer ||
            !it->second.type->pointeeType) {
            error(stmt, "cannot dereference non-pointer variable '" +
                             varName + "' of type '" + it->second.type->name + "'");
            return;
        }

        targetType = it->second.type->pointeeType;
    } else {
        // *(expr) op= value;
        auto* ptrType = resolveExprType(stmt->expression(0));
        if (!ptrType || ptrType->kind != TypeKind::Pointer || !ptrType->pointeeType) {
            error(stmt, "cannot dereference non-pointer expression");
            return;
        }
        targetType = ptrType->pointeeType;
    }

    auto* rhsType = resolveExprType(stmt->expression(stmt->IDENTIFIER() ? 0 : 1));
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: *ptr ??= default
    if (opText == "??=") {
        if (targetType && targetType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer target (deref of pointer-to-pointer), got '" +
                             targetType->name + "'");
        if (rhsType && targetType && !isAssignable(targetType, rhsType))
            error(stmt, "'??=' type mismatch: target type '" +
                             targetType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool needsNumeric = (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (needsNumeric && targetType && !isNumeric(targetType))
        error(stmt, "operator '" + opText +
                         "' requires numeric pointer target, got '" + targetType->name + "'");
    if (needsInteger && targetType && !isInteger(targetType))
        error(stmt, "operator '" + opText +
                         "' requires integer pointer target, got '" + targetType->name + "'");
    if (needsNumeric && rhsType && !isNumeric(rhsType))
        error(stmt, "operator '" + opText +
                         "' requires numeric operand, got '" + rhsType->name + "'");
    if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
        error(stmt, "operator '" + opText +
                         "' requires integer operand, got '" + rhsType->name + "'");

    // Type compatibility between deref target and RHS
    if (rhsType && targetType && !isAssignable(targetType, rhsType))
        error(stmt, "type mismatch in deref compound assignment: target type '" +
                    targetType->name + "', cannot use '" + rhsType->name + "'");

    if (opText == "/=" || opText == "%=") {
        auto* rhsExpr = stmt->expression(stmt->IDENTIFIER() ? 0 : 1);
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(rhsExpr)) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Global builtins — always available without `use`
// ═══════════════════════════════════════════════════════════════════════


