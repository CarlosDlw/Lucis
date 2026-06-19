#pragma once

#include <string>
#include <vector>

#include "generated/LucisParser.h"

class ProjectContext;

// Shared type inference utilities for LSP providers.
struct TypeInferrer {

    // Find a struct declaration by name, checking same-file then cross-file.
    static LucisParser::StructDeclContext* findStruct(
        const std::string& name,
        LucisParser::ProgramContext* tree,
        const ProjectContext* project);

    // Given a struct and a field name, return the field's type spec,
    // or nullptr if not found.
    static LucisParser::TypeSpecContext* findFieldTypeSpec(
        LucisParser::StructDeclContext* sd,
        const std::string& fieldName);

    // If typeSpec is a function type (fn(...) -> R), return the return
    // type as text. Otherwise return empty string.
    static std::string functionReturnType(LucisParser::TypeSpecContext* ts);

    // Resolve the return type of calling `methodName` on a receiver of
    // type `receiverType`, checking struct function-pointer fields.
    // Returns "" if no match found.
    static std::string resolveMethodReturnTypeViaStructField(
        const std::string& receiverType,
        const std::string& methodName,
        LucisParser::ProgramContext* tree,
        const ProjectContext* project);
};
