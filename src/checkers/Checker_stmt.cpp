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



bool Checker::blockAlwaysReturns(LucisParser::BlockContext* block) {
    std::function<bool(LucisParser::IfBodyContext*)> ifBodyAlwaysReturns =
    [&](LucisParser::IfBodyContext* body) -> bool {
        if (!body) return false;
        if (auto* b = body->block())
            return blockAlwaysReturns(b);
        if (auto* s = body->statement()) {
            if (s->returnStmt() || s->throwStmt() || s->bailStmt())
                return true;
            if (auto* ls = s->loopStmt())
                return blockAlwaysReturns(ls->block());
            if (auto* nestedIf = s->ifStmt()) {
                if (!nestedIf->elseClause()) return false;
                if (!ifBodyAlwaysReturns(nestedIf->ifBody())) return false;
                for (auto* eif : nestedIf->elseIfClause()) {
                    if (!ifBodyAlwaysReturns(eif->ifBody()))
                        return false;
                }
                return ifBodyAlwaysReturns(nestedIf->elseClause()->ifBody());
            }
        }
        return false;
    };

    auto stmts = block->statement();
    for (auto* stmt : stmts) {
        if (stmt->returnStmt()) {
            return true;
        }

        // throw / bail always exit the current function
        if (stmt->throwStmt() || stmt->bailStmt()) {
            return true;
        }

        // Detect noreturn function calls: panic(), exit(), unreachable()
        if (auto* cs = stmt->callStmt()) {
            auto name = cs->IDENTIFIER()->getText();
            if (name == "panic" || name == "exit" || name == "unreachable")
                return true;
        }
        if (auto* es = stmt->exprStmt()) {
            if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(es->expression())) {
                if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
                    auto name = ident->IDENTIFIER()->getText();
                    if (name == "panic" || name == "exit" || name == "unreachable")
                        return true;
                }
            }
            // lucis::core::trap() is a noreturn intrinsic
            if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(es->expression())) {
                auto ids = smc->IDENTIFIER();
                std::vector<std::string> idTexts;
                for (auto* id : ids) idTexts.push_back(id->getText());
                std::string ns, funcName;
                if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                    if (ns == "core" && funcName == "trap")
                        return true;
                }
            }
        }

        // loop { body } — if the body always returns, the loop always returns
        if (auto* ls = stmt->loopStmt()) {
            if (blockAlwaysReturns(ls->block()))
                return true;
        }

        if (auto* ifS = stmt->ifStmt()) {
            // if/else-if/else chain: all branches must return
            if (!ifS->elseClause()) continue;

            bool ifReturns = ifBodyAlwaysReturns(ifS->ifBody());
            if (!ifReturns) continue;

            bool allElseIfsReturn = true;
            for (auto* elseIf : ifS->elseIfClause()) {
                if (!ifBodyAlwaysReturns(elseIf->ifBody())) {
                    allElseIfsReturn = false;
                    break;
                }
            }
            if (!allElseIfsReturn) continue;

            if (ifBodyAlwaysReturns(ifS->elseClause()->ifBody()))
                return true;
        }

        if (auto* sw = stmt->switchStmt()) {
            // switch with default: all cases + default must return
            if (!sw->defaultClause()) continue;

            bool allReturn = true;
            for (auto* cc : sw->caseClause()) {
                if (!blockAlwaysReturns(cc->block())) {
                    allReturn = false;
                    break;
                }
            }
            if (allReturn && blockAlwaysReturns(sw->defaultClause()->block()))
                return true;
        }

        // try/catch: if all catch blocks + try block return, the whole thing returns
        if (auto* tc = stmt->tryCatchStmt()) {
            bool tryReturns = blockAlwaysReturns(tc->block());
            if (tryReturns) {
                bool allCatchReturn = true;
                for (auto* cc : tc->catchClause()) {
                    if (!blockAlwaysReturns(cc->block())) {
                        allCatchReturn = false;
                        break;
                    }
                }
                if (allCatchReturn) {
                    // finally doesn't affect return — it always runs regardless
                    return true;
                }
            }
        }
    }
    return false;
}



bool Checker::isTerminatorStmt(LucisParser::StatementContext* stmt) {
    if (stmt->returnStmt()) return true;
    if (stmt->throwStmt())  return true;
    if (stmt->breakStmt())  return true;
    if (stmt->continueStmt()) return true;

    // Noreturn function calls: panic(), exit(), unreachable()
    if (auto* cs = stmt->callStmt()) {
        auto name = cs->IDENTIFIER()->getText();
        if (name == "panic" || name == "exit" || name == "unreachable")
            return true;
    }
    if (auto* es = stmt->exprStmt()) {
        if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(es->expression())) {
            if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
                auto name = ident->IDENTIFIER()->getText();
                if (name == "panic" || name == "exit" || name == "unreachable")
                    return true;
            }
        }
        // lucis::core::trap() is a noreturn intrinsic
        if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(es->expression())) {
            auto ids = smc->IDENTIFIER();
            std::vector<std::string> idTexts;
            for (auto* id : ids) idTexts.push_back(id->getText());
            std::string ns, funcName;
            if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                if (ns == "core" && funcName == "trap")
                    return true;
            }
        }
    }
    return false;
}



void Checker::checkBlock(LucisParser::BlockContext* block,
                         const TypeInfo* retType,
                         std::unordered_set<std::string>* initCapture) {
    auto savedLocals = locals_;
    auto savedEnumImports = enumVariantImports_;
    ++scopeDepth_;
    bool terminated = false;
    for (auto* stmt : block->statement()) {
        if (terminated) {
            warning(stmt, "unreachable code");
            break;
        }
        checkStmt(stmt, retType, terminated);
    }
    --scopeDepth_;
    // Propagate 'used' flags to outer-scope variables before restoring.
    // Also capture newly-initialized outer-scope variables if requested.
    for (auto& [name, info] : savedLocals) {
        auto it = locals_.find(name);
        if (it != locals_.end()) {
            info.used = info.used || it->second.used;
            if (initCapture && !info.initialized && it->second.initialized)
                initCapture->insert(name);
        }
    }
    // Full restore: removes inner-scope variables and restores outer values.
    locals_ = savedLocals;
    enumVariantImports_ = savedEnumImports;
}



void Checker::checkStmt(LucisParser::StatementContext* stmt,
                        const TypeInfo* retType,
                        bool& terminated) {
    if (auto* ud = stmt->useDecl()) {
        auto* attrs = [&]() -> LucisParser::AttributeListContext* {
            if (auto* r = dynamic_cast<LucisParser::UseRootContext*>(ud)) return r->attributeList();
            if (auto* i = dynamic_cast<LucisParser::UseItemContext*>(ud)) return i->attributeList();
            if (auto* g = dynamic_cast<LucisParser::UseGroupContext*>(ud)) return g->attributeList();
            if (auto* e = dynamic_cast<LucisParser::UseEnumWildcardContext*>(ud)) return e->attributeList();
            return nullptr;
        }();
        if (!isDeclActive(attrs)) return;
        if (auto* ew = dynamic_cast<LucisParser::UseEnumWildcardContext*>(ud)) {
            unsigned arrayDims = 0;
            auto* enumType = resolveTypeSpec(ew->typeSpec(), arrayDims);
            if (!enumType) {
                error(ew, "unknown type in 'use' wildcard");
            } else if (enumType->kind != TypeKind::Enum) {
                error(ew, "type '" + enumType->name + "' is not an enum");
            } else if (arrayDims > 0) {
                error(ew, "cannot use array type in 'use' wildcard");
            } else {
                for (const auto& vi : enumType->enumVariantInfos) {
                    auto [it, inserted] = enumVariantImports_.try_emplace(
                        vi.name,
                        InjectedVariant{ enumType, &vi });
                    if (!inserted) {
                        error(ew, "ambiguous variant name '" + vi.name +
                                   "': already imported from '" +
                                   it->second.enumType->name + "'");
                    }
                }
            }
        }
        return;
    }

    if (auto* constDecl = stmt->constDeclStmt()) {
        checkConstDeclStmt(constDecl);
    } else if (auto* varDecl = stmt->varDeclStmt()) {
        checkVarDeclStmt(varDecl);
    } else if (auto* assign = stmt->assignStmt()) {
        checkAssignStmt(assign);
    } else if (auto* compound = stmt->compoundAssignStmt()) {
        checkCompoundAssignStmt(compound);
    } else if (auto* fieldCompound = stmt->fieldCompoundAssignStmt()) {
        checkFieldCompoundAssignStmt(fieldCompound);
    } else if (auto* fieldAssign = stmt->fieldAssignStmt()) {
        checkFieldAssignStmt(fieldAssign);
    } else if (auto* arrowAssign = stmt->arrowAssignStmt()) {
        checkArrowAssignStmt(arrowAssign);
    } else if (auto* arrowCompound = stmt->arrowCompoundAssignStmt()) {
        checkArrowCompoundAssignStmt(arrowCompound);
    } else if (auto* derefAssign = stmt->derefAssignStmt()) {
        checkDerefAssignStmt(derefAssign);
    } else if (auto* derefCompound = stmt->derefCompoundAssignStmt()) {
        checkDerefCompoundAssignStmt(derefCompound);
    } else if (auto* call = stmt->callStmt()) {
        checkCallStmt(call);
    } else if (auto* asmS = stmt->asmStmt()) {
        checkAsmStmt(asmS);
    } else if (auto* label = stmt->labelDef()) {
        auto name = label->IDENTIFIER()->getText();
        if (currentFunctionLabels_.count(name))
            error(label, "duplicate label '" + name + "'");
        else
            currentFunctionLabels_.insert(name);
        checkStmt(label->statement(), retType, terminated);
    } else if (auto* exprS = stmt->exprStmt()) {
        checkExprStmt(exprS);
    } else if (auto* ret = stmt->returnStmt()) {
        checkReturnStmt(ret, retType);
    } else if (auto* ifS = stmt->ifStmt()) {
        checkIfStmt(ifS, retType);
    } else if (auto* forS = stmt->forStmt()) {
        if (auto* forIn = dynamic_cast<LucisParser::ForInStmtContext*>(forS))
            checkForInStmt(forIn, retType);
        else if (auto* forC = dynamic_cast<LucisParser::ForClassicStmtContext*>(forS))
            checkForClassicStmt(forC, retType);
    } else if (stmt->loopStmt()) {
        loopDepth_++;
        checkBlock(stmt->loopStmt()->block(), retType);
        loopDepth_--;
    } else if (auto* ws = stmt->whileStmt()) {
        auto* wCondType = resolveExprType(ws->expression());
        if (wCondType && !isConditionType(wCondType))
            error(ws, "condition has type '" + wCondType->name +
                      "', expected 'bool' or numeric type");
        loopDepth_++;
        checkBlock(ws->block(), retType);
        loopDepth_--;
    } else if (auto* dw = stmt->doWhileStmt()) {
        loopDepth_++;
        checkBlock(dw->block(), retType);
        loopDepth_--;
        auto* dwCondType = resolveExprType(dw->expression());
        if (dwCondType && !isConditionType(dwCondType))
            error(dw, "condition has type '" + dwCondType->name +
                      "', expected 'bool' or numeric type");
    } else if (stmt->breakStmt()) {
        if (loopDepth_ == 0)
            error(stmt, "'break' used outside of a loop");
    } else if (stmt->continueStmt()) {
        if (loopDepth_ == 0)
            error(stmt, "'continue' used outside of a loop");
    } else if (auto* sw = stmt->switchStmt()) {
        checkSwitchStmt(sw, retType);
    } else if (auto* lk = stmt->lockStmt()) {
        resolveExprType(lk->expression());
        checkBlock(lk->block(), retType);
    } else if (auto* tc = stmt->tryCatchStmt()) {
        checkBlock(tc->block(), retType);
        for (auto* cc : tc->catchClause()) {
            auto catchVarName = cc->IDENTIFIER()->getText();
            unsigned catchDims = 0;
            auto* catchType = resolveTypeSpec(cc->typeSpec(), catchDims);
            auto prev = locals_.find(catchVarName);
            bool hadPrev = prev != locals_.end();
            VarInfo prevInfo;
            if (hadPrev) prevInfo = prev->second;
            if (catchType)
                locals_[catchVarName] = {catchType, catchDims, {}, true, true, nullptr};
            checkBlock(cc->block(), retType);
            if (hadPrev)
                locals_[catchVarName] = prevInfo;
            else
                locals_.erase(catchVarName);
        }
        if (auto* fin = tc->finallyClause()) {
            checkBlock(fin->block(), retType);
        }
    } else if (stmt->throwStmt()) {
        resolveExprType(stmt->throwStmt()->expression());
    } else if (stmt->bailStmt()) {
        checkBailStmt(stmt->bailStmt());
    } else if (auto* ifa = stmt->indexFieldAssignStmt()) {
        for (auto* e : ifa->expression()) {
            resolveExprType(e);
        }
    } else if (auto* fia = stmt->fieldIndexAssignStmt()) {
        for (auto* e : fia->expression()) {
            resolveExprType(e);
        }
    } else if (auto* def = stmt->deferStmt()) {
        if (def->callStmt())
            checkCallStmt(def->callStmt());
        else if (def->exprStmt())
            checkExprStmt(def->exprStmt());

    // ── Structural Blocks ──────────────────────────────────────────────

    } else if (auto* nb = stmt->nakedBlockStmt()) {
        // {} — lexical scope: variables declared inside do NOT leak out
        auto savedLocals = locals_;
        auto savedEnumImports = enumVariantImports_;
        ++scopeDepth_;
        bool innerTerminated = false;
        for (auto* inner : nb->statement()) {
            if (innerTerminated) {
                warning(inner, "unreachable code");
                break;
            }
            checkStmt(inner, retType, innerTerminated);
        }
        --scopeDepth_;
        for (auto& [name, info] : savedLocals) {
            auto it = locals_.find(name);
            if (it != locals_.end()) info.used = info.used || it->second.used;
        }
        locals_ = savedLocals;
        enumVariantImports_ = savedEnumImports;

    } else if (auto* ib = stmt->inlineBlockStmt()) {
        // #inline {} — variables are injected into parent scope
        for (auto* inner : ib->statement()) {
            if (terminated) {
                warning(inner, "unreachable code");
                break;
            }
            checkStmt(inner, retType, terminated);
        }

    } else if (auto* sb = stmt->scopeBlockStmt()) {
        // #scope (callbacks) {} — callbacks execute at scope exit, so they can
        // reference variables declared inside the body. Process body first, then
        // validate callbacks with body locals visible.
        auto savedLocals = locals_;
        auto savedEnumImports = enumVariantImports_;
        ++scopeDepth_;
        bool innerTerminated = false;
        for (auto* inner : sb->statement()) {
            if (innerTerminated) {
                warning(inner, "unreachable code");
                break;
            }
            checkStmt(inner, retType, innerTerminated);
        }
        // Validate callbacks with body locals in scope
        if (auto* cbs = sb->scopeCallbackList()) {
            for (auto* cb : cbs->scopeCallback()) {
                if (cb->DOT()) {
                    // dot-access: varName.methodName(args)
                    auto varName    = cb->IDENTIFIER().size() > 0 ? cb->IDENTIFIER(0)->getText() : "";
                    auto methodName = cb->IDENTIFIER().size() > 1 ? cb->IDENTIFIER(1)->getText() : "";
                    if (locals_.find(varName) == locals_.end())
                        warning(cb, "unknown variable '" + varName + "' in #scope callback");
                } else {
                    // plain call: funcName(args)
                    auto funcName = cb->IDENTIFIER(0)->getText();
                    if (!isKnownFunction(funcName) && !(cBindings_ && cBindings_->findFunction(funcName)))
                        warning(cb, "unknown function '" + funcName + "' in #scope callback");
                }
                if (cb->argList())
                    for (auto* arg : cb->argList()->expression())
                        resolveExprType(arg);
            }
        }
        --scopeDepth_;
        for (auto& [name, info] : savedLocals) {
            auto it = locals_.find(name);
            if (it != locals_.end()) info.used = info.used || it->second.used;
        }
        locals_ = savedLocals;
        enumVariantImports_ = savedEnumImports;

    } else if (auto* cm = stmt->cMacroBlock()) {
        checkCMacroBlock(cm);
    }

    if (isTerminatorStmt(stmt))
        terminated = true;
}



void Checker::checkSwitchStmt(LucisParser::SwitchStmtContext* stmt,
                              const TypeInfo* retType) {
    auto* exprType = resolveExprType(stmt->expression());
    if (exprType && !isInteger(exprType) && exprType->name != "char" &&
        exprType->kind != TypeKind::Enum) {
        error(stmt, "switch expression must be an integer, char or enum type, got '" +
              exprType->name + "'");
    }
    for (auto* cc : stmt->caseClause()) {
        for (auto* caseExpr : cc->expression()) {
            resolveExprType(caseExpr);
        }
        checkBlock(cc->block(), retType);
    }
    if (auto* dc = stmt->defaultClause()) {
        checkBlock(dc->block(), retType);
    }

    // Enum exhaustiveness check: warn if switch on enum has no default
    // and doesn't cover all variants
    if (exprType && exprType->kind == TypeKind::Enum && !stmt->defaultClause()) {
        std::unordered_set<std::string> coveredVariants;
        for (auto* cc : stmt->caseClause()) {
            for (auto* caseExpr : cc->expression()) {
                // Enum access: Enum::Variant
                if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(caseExpr)) {
                    auto ids = ea->IDENTIFIER();
                    if (ids.size() >= 2)
                        coveredVariants.insert(ids[1]->getText());
                }
            }
        }

        std::vector<std::string> missing;
        for (auto& variant : exprType->enumVariants) {
            if (!coveredVariants.count(variant))
                missing.push_back(variant);
        }

        if (!missing.empty()) {
            std::string msg = "switch on enum '" + exprType->name +
                "' is not exhaustive. Missing: ";
            for (size_t i = 0; i < missing.size(); i++) {
                if (i > 0) msg += ", ";
                msg += "'" + exprType->name + "::" + missing[i] + "'";
            }
            msg += ". Add a 'default' clause or handle all variants";
            warning(stmt, msg);
        }
    }
}



void Checker::checkIfStmt(LucisParser::IfStmtContext* stmt,
                           const TypeInfo* retType) {
    // Run a branch body and capture which outer-scope variables become initialized.
    auto checkIfBodyTracked = [&](LucisParser::IfBodyContext* body,
                                   std::unordered_set<std::string>& inited) {
        if (!body) return;
        if (auto* b = body->block()) {
            checkBlock(b, retType, &inited);
            return;
        }
        if (auto* s = body->statement()) {
            auto savedLocals = locals_;
            ++scopeDepth_;
            bool branchTerminated = false;
            checkStmt(s, retType, branchTerminated);
            --scopeDepth_;
            for (auto& [name, info] : savedLocals) {
                auto it = locals_.find(name);
                if (it != locals_.end()) {
                    info.used = info.used || it->second.used;
                    if (!info.initialized && it->second.initialized)
                        inited.insert(name);
                }
            }
            locals_ = savedLocals;
        }
    };

    // Extract is-binding from an expression (if it's `expr is Type::Variant(name)`).
    // Returns {bindingName, payloadTypeInfo} or {"", nullptr}.
    auto extractIsBinding = [&](LucisParser::ExpressionContext* condExpr)
            -> std::pair<std::string, const TypeInfo*> {
        auto* isE = dynamic_cast<LucisParser::IsExprContext*>(condExpr);
        if (!isE || !isE->SCOPE() || !isE->LPAREN() || !isE->IDENTIFIER(1))
            return {"", nullptr};
        auto variantName = isE->IDENTIFIER(0)->getText();
        unsigned dims = 0;
        auto* rhsType = resolveTypeSpec(isE->typeSpec(), dims);
        if (!rhsType || rhsType->kind != TypeKind::Enum) return {"", nullptr};
        auto* variantInfo = findEnumVariantInfo(rhsType, variantName);
        if (!variantInfo || variantInfo->payloadFields.empty()) return {"", nullptr};
        // Single-field tuple payload
        if (variantInfo->payloadKind == EnumPayloadKind::Tuple &&
            variantInfo->payloadFields.size() == 1) {
            return {isE->IDENTIFIER(1)->getText(),
                    variantInfo->payloadFields[0].typeInfo};
        }
        return {"", nullptr};
    };

    // Check the if condition
    auto* condType = resolveExprType(stmt->expression());
    if (condType && !isConditionType(condType))
        error(stmt, "condition has type '" + condType->name +
                    "', expected 'bool' or numeric type");

    // ── Dead branch elimination for compile-time constant conditions ──
    auto constCond = evaluateConstExpr(stmt->expression());
    bool isConstTrue = false;
    bool isConstFalse = false;
    if (constCond && constCond->kind() == ComptimeValue::Kind::Bool) {
        isConstTrue = constCond->asBool();
        isConstFalse = !constCond->asBool();
    }

    // Collect init sets from each branch for flow-sensitive init analysis.
    // Only propagate if ALL branches (including else) initialize a variable.
    std::vector<std::unordered_set<std::string>> branchInits;

    // Check if-body
    auto [ifBindName, ifBindTI] = extractIsBinding(stmt->expression());
    if (!ifBindName.empty() && ifBindTI)
        locals_[ifBindName] = {ifBindTI, 0, {}, true, true, nullptr};
    branchInits.emplace_back();
    if (!isConstFalse)
        checkIfBodyTracked(stmt->ifBody(), branchInits.back());
    if (!ifBindName.empty()) locals_.erase(ifBindName);

    // Check else-if clauses
    for (auto* elseIf : stmt->elseIfClause()) {
        if (isConstTrue) break; // dead branch
        auto* eifCondType = resolveExprType(elseIf->expression());
        if (eifCondType && !isConditionType(eifCondType))
            error(elseIf, "condition has type '" + eifCondType->name +
                          "', expected 'bool' or numeric type");
        auto eifConstCond = evaluateConstExpr(elseIf->expression());
        bool eifConstFalse = eifConstCond && eifConstCond->kind() == ComptimeValue::Kind::Bool
                             && !eifConstCond->asBool();
        auto [eifBindName, eifBindTI] = extractIsBinding(elseIf->expression());
        if (!eifBindName.empty() && eifBindTI)
            locals_[eifBindName] = {eifBindTI, 0, {}, true, true, nullptr};
        branchInits.emplace_back();
        if (!eifConstFalse)
            checkIfBodyTracked(elseIf->ifBody(), branchInits.back());
        if (!eifBindName.empty()) locals_.erase(eifBindName);
        if (eifConstCond && eifConstCond->kind() == ComptimeValue::Kind::Bool && eifConstCond->asBool())
            isConstTrue = true; // subsequent else-ifs and else are dead
    }

    // Check else clause
    bool hasElse = (stmt->elseClause() != nullptr);
    if (hasElse && !isConstTrue) {
        branchInits.emplace_back();
        checkIfBodyTracked(stmt->elseClause()->ifBody(), branchInits.back());
    }

    // Flow-sensitive init propagation: if all branches (with else = full coverage)
    // initialize a variable, mark it initialized in the outer scope.
    if (hasElse && !branchInits.empty()) {
        // Start with the first branch's init set, intersect with all others.
        std::unordered_set<std::string> intersection = branchInits[0];
        for (size_t bi = 1; bi < branchInits.size(); ++bi) {
            for (auto it = intersection.begin(); it != intersection.end(); ) {
                if (!branchInits[bi].count(*it))
                    it = intersection.erase(it);
                else
                    ++it;
            }
        }
        for (const auto& name : intersection) {
            auto lit = locals_.find(name);
            if (lit != locals_.end())
                lit->second.initialized = true;
        }
    }
}



void Checker::checkForInStmt(LucisParser::ForInStmtContext* stmt,
                              const TypeInfo* retType) {
    unsigned dims = 0;
    auto* iterType = [&]() -> const TypeInfo* {
        if (!activeTypeSubst_.empty())
            return resolveTypeSpecWithSubst(stmt->typeSpec(), activeTypeSubst_, dims);
        return resolveTypeSpec(stmt->typeSpec(), dims);
    }();
    auto iterName = stmt->IDENTIFIER()->getText();

    auto* iterableType = resolveExprType(stmt->expression());

    // Range expressions (start..end, start..=end) are always valid for iteration
    bool isRange = dynamic_cast<LucisParser::RangeExprContext*>(stmt->expression()) ||
                   dynamic_cast<LucisParser::RangeInclExprContext*>(stmt->expression());

    // Validate the iterable expression type
    if (!isRange && iterableType) {
        bool isValid = false;

        if (iterableType->kind == TypeKind::Extended) {
            // Collection iteration: Vec<T>, Map<K,V>, Set<T>
            isValid = true;
        } else if (iterableType->kind == TypeKind::Integer) {
            // Integer range iteration: for TYPE i in N
            if (iterType && iterType->kind != TypeKind::Integer) {
                error(stmt, "cannot iterate over integer '" + iterableType->name +
                      "' with loop variable of type '" + iterType->name +
                      "', expected an integer type");
            }
            isValid = true;
        }

        if (!isValid) {
            error(stmt, "cannot iterate over type '" + iterableType->name +
                  "', expected a Vec, Map, Set, or integer range");
        }
    }

    // Register the loop variable
    if (iterType)
        locals_[iterName] = {iterType, dims, {}, true, true, nullptr};

    loopDepth_++;
    checkBlock(stmt->block(), retType);
    loopDepth_--;

    locals_.erase(iterName);
}



void Checker::checkForClassicStmt(LucisParser::ForClassicStmtContext* stmt,
                                   const TypeInfo* retType) {
    unsigned dims = 0;
    auto* varType = [&]() -> const TypeInfo* {
        if (!activeTypeSubst_.empty())
            return resolveTypeSpecWithSubst(stmt->typeSpec(), activeTypeSubst_, dims);
        return resolveTypeSpec(stmt->typeSpec(), dims);
    }();
    auto varName = stmt->IDENTIFIER()->getText();

    // Check init expression
    auto exprs = stmt->expression();
    auto* initType = resolveExprType(exprs[0]);

    if (varType && initType && !isAssignable(varType, initType))
        error(stmt, "for init type mismatch: expected '" +
                         varType->name + "', got '" + initType->name + "'");

    // Register the loop variable
    if (varType)
        locals_[varName] = {varType, dims, {}, true, true, nullptr};

    // Check condition expression
    auto* condType = resolveExprType(exprs[1]);
    if (condType && !isConditionType(condType))
        error(stmt, "for condition has type '" + condType->name +
                    "', expected 'bool' or numeric type");

    // Check update expression
    resolveExprType(exprs[2]);

    loopDepth_++;
    checkBlock(stmt->block(), retType);
    loopDepth_--;

    locals_.erase(varName);
}

// ═══════════════════════════════════════════════════════════════════════
//  Statement checks
// ═══════════════════════════════════════════════════════════════════════

// ── Const expression evaluator ─────────────────────────────────────────────
// Walks the AST and computes the compile-time value of a const expression.
// Returns std::nullopt if the expression cannot be evaluated at compile time.


void Checker::validateAttributeList(AttributeListContext* attrs, const std::string& contextName) {
    if (!attrs) return;
    for (auto* a : attrs->attribute()) {
        auto attrName = a->IDENTIFIER()->getText();
        if (!attrRegistry_.isKnown(attrName)) {
            error(a, "unknown attribute '" + attrName + "'");
            continue;
        }
        auto* handler = attrRegistry_.lookup(attrName);
        if (handler && handler->validate) {
            // Build Attribute struct from AST node for validation
            Attribute attr;
            populateAttribute(a, attr);
            attr.contextName = contextName;
            std::vector<std::string> errors;
            if (!handler->validate(attr, nullptr, errors)) {
                for (auto& err : errors)
                    error(a, "invalid attribute '" + attrName + "': " + err);
                if (errors.empty())
                    error(a, "invalid attribute '" + attrName + "'");
            }
        }
    }
}



bool Checker::hasAttribute(AttributeListContext* attrs, const std::string& name) const {
    if (!attrs) return false;
    for (auto* a : attrs->attribute()) {
        if (a->IDENTIFIER()->getText() == name)
            return true;
    }
    return false;
}



void Checker::checkConstDeclStmt(LucisParser::ConstDeclStmtContext* stmt) {
    if (!isDeclActive(stmt->attributeList())) return;
    auto decls = stmt->constDeclarator();
    if (decls.empty()) return;

    for (auto* d : decls) {
        auto name = d->IDENTIFIER()->getText();
        auto it = locals_.find(name);

        // Function-scoped const: register if not already in locals_
        bool isFunctionScoped = (it == locals_.end());
        if (isFunctionScoped) {
            VarInfo vi{typeRegistry_.lookup("int32"), 0, {}, true, false, nullptr};
            vi.isConst = true;
            vi.scopeDepth = scopeDepth_;
            vi.declToken = d->IDENTIFIER()->getSymbol();
            locals_[name] = vi;
            it = locals_.find(name);
        }

        // Validate that the initializer is a constant expression
        if (d->expression() && !isValidConstExpr(d->expression())) {
            error(d->expression(), "constant '" + name + "' initializer must be a compile-time constant expression");
            continue;
        }

        if (d->COLON() && d->typeSpec()) {
            // Explicit type: const NAME: TYPE = VALUE;
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(d->typeSpec(), dims);
            if (!ti) continue;

            if (d->expression()) {
                auto* initTI = resolveExprType(d->expression());
                if (initTI) {
                    if (ti->kind == TypeKind::Integer && initTI->kind == TypeKind::Integer) {
                        // allow int-to-int coercions
                    } else if (ti->name != initTI->name) {
                        error(d, "type mismatch: cannot initialize '" + name +
                                 "' of type '" + ti->name + "' with expression of type '" +
                                 initTI->name + "'");
                        continue;
                    }
                }
            }
            it->second.type = ti;
            it->second.arrayDims = dims;
            it->second.initialized = (d->expression() != nullptr);
            it->second.declToken = d->IDENTIFIER()->getSymbol();
            if (!isFunctionScoped)
                globalVars_[name] = it->second;
        } else if (d->expression()) {
            // Auto type: const NAME = VALUE;
            auto* initTI = resolveExprType(d->expression());
            if (!initTI || initTI->kind == TypeKind::Void) {
                error(d, "cannot infer type for constant '" + name + "'");
                continue;
            }

            unsigned dims = 0;
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression())) {
                if (!arrLit->expression().empty()) dims = 1;
            } else {
                dims = resolveExprArrayDims(d->expression());
            }

            it->second.type = initTI;
            it->second.arrayDims = dims;
            it->second.initialized = true;
            it->second.declToken = d->IDENTIFIER()->getSymbol();
            if (!isFunctionScoped)
                globalVars_[name] = it->second;
        }

        // Store compile-time evaluated value for use in array dimensions and other const exprs
        if (d->expression()) {
            auto ev = evaluateConstExpr(d->expression());
            if (ev) {
                compileTimeValues_[name] = *ev;
            }
        }
    }
}



void Checker::checkVarDeclStmt(LucisParser::VarDeclStmtContext* stmt) {
    if (!isDeclActive(stmt->attributeList())) return;
    // ── Tuple destructuring: auto (x, y) = expr; ─────────────────────
    if (stmt->LPAREN()) {
        auto ids = stmt->IDENTIFIER();
        if (!stmt->expression()) {
            error(stmt, "tuple destructuring requires an initializer");
            return;
        }
        auto* initType = resolveExprType(stmt->expression());
        if (!initType || initType->kind != TypeKind::Tuple) {
            error(stmt, "tuple destructuring requires a tuple expression");
            return;
        }
        if (ids.size() != initType->tupleElements.size()) {
            error(stmt, "tuple destructuring expects " +
                        std::to_string(initType->tupleElements.size()) +
                        " variables, got " + std::to_string(ids.size()));
            return;
        }
        for (size_t i = 0; i < ids.size(); i++) {
            auto varName = ids[i]->getText();
            auto it = locals_.find(varName);
            if (it != locals_.end() && it->second.scopeDepth == scopeDepth_) {
                error(stmt, "variable '" + varName + "' already declared in this scope");
                continue;
            }
            VarInfo vi{initType->tupleElements[i], 0, {}, true, false, nullptr};
            vi.declToken = ids[i]->getSymbol();
            vi.scopeDepth = scopeDepth_;
            updateOwnershipOnInitialization(vi, stmt->expression());
            locals_[varName] = vi;
        }
        markExprAsMoved(stmt->expression(), stmt);
        return;
    }

    // ── Detect module prefix (qualified type: LIB::Point) ────────
    bool hasNsPrefix = stmt->SCOPE() != nullptr;
    std::string nsPrefix = hasNsPrefix && stmt->IDENTIFIER().size() > 0
        ? stmt->IDENTIFIER(0)->getText() : "";

    // ── Detect auto type ─────────────────────────────────────────────
    bool isAutoType = stmt->typeSpec() && stmt->typeSpec()->AUTO() != nullptr;

    // ── Resolve type for explicit types ──────────────────────────────
    const TypeInfo* typeInfo = nullptr;
    unsigned arrayDims = 0;
    if (!isAutoType) {
        typeInfo = [&]() -> const TypeInfo* {
            if (!activeTypeSubst_.empty())
                return resolveTypeSpecWithSubst(stmt->typeSpec(), activeTypeSubst_, arrayDims);
            if (hasNsPrefix) {
                auto* ts = stmt->typeSpec();
                if (ts && ts->IDENTIFIER()) {
                    auto typeName = ts->IDENTIFIER()->getText();
                    if (moduleRegistry_) {
                        auto* sym = moduleRegistry_->findSymbol(nsPrefix, typeName);
                        if (!sym) {
                            error(stmt, "'" + nsPrefix + "::" + typeName + "' is not exported");
                            return nullptr;
                        }
                    }
                    auto* ti = typeRegistry_.lookup(typeName);
                    if (!ti) {
                        error(stmt, "type '" + typeName + "' from module '" + nsPrefix +
                                     "' is not imported; add 'use " + nsPrefix + "::" + typeName + ";'");
                        return nullptr;
                    }
                    return ti;
                }
            }
            return resolveTypeSpec(stmt->typeSpec(), arrayDims);
        }();
        if (!typeInfo) return;
    }

    // ── Iterate varDeclarators ──────────────────────────────────────
    auto decls = stmt->varDeclarator();
    if (decls.empty()) return;

    // Find the last declarator that has an initializer (propagation source)
    LucisParser::VarDeclaratorContext* lastInitDecl = nullptr;
    for (auto it = decls.rbegin(); it != decls.rend(); ++it) {
        if ((*it)->expression()) { lastInitDecl = *it; break; }
    }

    // For auto, infer type from the first declarator that has an init
    if (isAutoType && !typeInfo) {
        for (auto* d : decls) {
            if (d->expression()) {
                auto* initType = resolveExprType(d->expression());
                if (!initType || initType->kind == TypeKind::Void) {
                    error(d, "cannot infer auto type for variable '" +
                             d->IDENTIFIER()->getText() + "'");
                    return;
                }
                typeInfo = initType;
                arrayDims = resolveExprArrayDims(d->expression());
                break;
            }
        }
        if (!typeInfo) {
            error(stmt, "type 'auto' requires at least one initializer expression");
            return;
        }
    }

    for (auto* d : decls) {
        auto varName = d->IDENTIFIER()->getText();

        // Check redeclaration in same scope
        auto it = locals_.find(varName);
        if (it != locals_.end() && it->second.scopeDepth == scopeDepth_) {
            error(d, "variable '" + varName + "' already declared in this scope");
            continue;
        }

        if (isAutoType) {
            // auto: use own init, or propagate from last init
            if (!d->expression()) {
                if (!lastInitDecl) {
                    error(d, "type 'auto' requires an initializer for '" + varName + "'");
                    continue;
                }
std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());
            VarInfo vi{typeInfo, arrayDims, arraySizes, true, false, nullptr};
                vi.declToken = d->IDENTIFIER()->getSymbol();
                vi.scopeDepth = scopeDepth_;
                updateOwnershipOnInitialization(vi, lastInitDecl->expression());
                locals_[varName] = vi;
                continue;
            }
            auto* initType = resolveExprType(d->expression());
            if (dynamic_cast<LucisParser::RangeExprContext*>(d->expression()) ||
                dynamic_cast<LucisParser::RangeInclExprContext*>(d->expression())) {
                error(d, "cannot infer type for '" + varName +
                         "': range expression has no concrete type");
                continue;
            }
            if (!initType) {
                error(d, "cannot infer type for '" + varName + "'");
                continue;
            }
            if (initType->kind == TypeKind::Void) {
                error(d, "cannot infer type: initializer for '" + varName +
                         "' has type 'void'");
                continue;
            }
            std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());
            VarInfo vi{initType, arrayDims, arraySizes, true, false, nullptr};
            vi.declToken = d->IDENTIFIER()->getSymbol();
            vi.scopeDepth = scopeDepth_;
            updateOwnershipOnInitialization(vi, d->expression());
            locals_[varName] = vi;
            // Store compile-time value for use in array dimensions
            {
                auto cv = evaluateConstExpr(d->expression());
                if (cv) compileTimeValues_[varName] = *cv;
            }
            markExprAsMoved(d->expression(), stmt);
            trackVarBufferFromExpr(varName, d->expression(), initType);
            trackVarNumericRangeFromExpr(varName, d->expression(), initType);
            continue;
        }

        // Explicit type: without own initializer — may receive propagated value
        if (!d->expression()) {
            bool hasPropagated = (lastInitDecl != nullptr);
            bool autoInit = hasPropagated ||
                (typeInfo->kind == TypeKind::Extended ||
                 typeInfo->kind == TypeKind::Struct ||
                 arrayDims > 0);
            std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());
            VarInfo vi{typeInfo, arrayDims, arraySizes, autoInit, false, nullptr};
            vi.declToken = d->IDENTIFIER()->getSymbol();
            vi.scopeDepth = scopeDepth_;
            vi.ownership = autoInit && isDropTrackedType(typeInfo, arrayDims)
                ? VarInfo::OwnershipState::Owned
                : VarInfo::OwnershipState::BorrowedImm;
            locals_[varName] = vi;
            continue;
        }

        // Explicit type with initializer — validate
        auto* initType = resolveExprType(d->expression());
        if (dynamic_cast<LucisParser::RangeExprContext*>(d->expression()) ||
            dynamic_cast<LucisParser::RangeInclExprContext*>(d->expression())) {
            error(d, "type mismatch: cannot initialize variable '" + varName +
                     "' with a range expression");
            continue;
        }
        std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());

        if (typeInfo->kind == TypeKind::Extended &&
            dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression())) {
            if (typeInfo->extendedKind == "Map") {
                error(d, "Map cannot be initialized with a literal; "
                         "use method 'set' to add entries");
                continue;
            }
            auto* arrExpr = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression());
            for (auto* e : arrExpr->expression()) {
                auto* et = resolveExprType(e);
                if (et && typeInfo->elementType &&
                    !isAssignable(typeInfo->elementType, et)) {
                    error(e, "element type mismatch: expected '" +
                             typeInfo->elementType->name + "', got '" + et->name + "'");
                }
            }
        } else if (initType && !isAssignable(typeInfo, initType)) {
            // Special case: [N]uint8 = c"..." — allow if sizes match
            bool cstrToArrayOk = false;
            if (arrayDims > 0 && initType->kind == TypeKind::Pointer) {
                if (auto* cexpr = dynamic_cast<LucisParser::CStrLitExprContext*>(d->expression())) {
                    // typeInfo is the element type (e.g., uint8) when arrayDims > 0
                    if (typeInfo && (typeInfo->name == "uint8" || typeInfo->name == "char")) {
                        auto cstrLen = tryGetCStringLiteralLen(d->expression());
                        // c-string includes null terminator: content + 1
                        if (cstrLen) {
                            unsigned inferred = static_cast<unsigned>(*cstrLen + 1);
                            if (arraySizes.empty()) {
                                // []uint8 = c"..." → infer size
                                arraySizes.push_back(inferred);
                                cstrToArrayOk = true;
                            } else if (arraySizes[0] == inferred) {
                                cstrToArrayOk = true;
                            } else {
                                error(d, "c-string literal length (" + std::to_string(inferred) +
                                      ") does not match array size (" + std::to_string(arraySizes[0]) + ")");
                            }
                        }
                    }
                }
            }
            if (!cstrToArrayOk) {
                error(d, "type mismatch: cannot assign '" + initType->name +
                      "' to variable '" + varName + "' of type '" + typeInfo->name + "'");
            }
        } else if (initType) {
            bool isArrayLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression()) != nullptr;
            if (arrayDims == 0 && isArrayLit) {
                error(d, "type mismatch: cannot assign array literal to non-array variable '" +
                          varName + "'");
            } else if (arrayDims > 0 && !isArrayLit) {
                bool isScalarLit =
                    dynamic_cast<LucisParser::IntLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::HexLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::OctLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::BinLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::FloatLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::BoolLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::CharLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::StrLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::CStrLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(d->expression());
                if (isScalarLit) {
                    error(d, "type mismatch: cannot assign scalar to array variable '" +
                              varName + "'");
                }
            }
        }

        checkNegativeToUnsigned(typeInfo, d->expression(), d);

            VarInfo vi{typeInfo, arrayDims, arraySizes, true, false, nullptr};
        vi.declToken = d->IDENTIFIER()->getSymbol();
        vi.scopeDepth = scopeDepth_;
        updateOwnershipOnInitialization(vi, d->expression());
        locals_[varName] = vi;
        // Store compile-time value for use in array dimensions
        {
            auto cv = evaluateConstExpr(d->expression());
            if (cv) compileTimeValues_[varName] = *cv;
        }
        markExprAsMoved(d->expression(), stmt);
        trackVarBufferFromExpr(varName, d->expression(), typeInfo);
        trackVarNumericRangeFromExpr(varName, d->expression(), typeInfo);
    }
}



void Checker::checkCallStmt(LucisParser::CallStmtContext* stmt) {
    auto name = stmt->IDENTIFIER()->getText();

    // Check if name is a local variable with a function/closure type
    auto lit = locals_.find(name);
    if (lit != locals_.end()) {
        auto* localType = lit->second.type;
        if (localType && (localType->kind == TypeKind::Function ||
                          localType->kind == TypeKind::Closure)) {
            // Resolve all argument types
            std::vector<const TypeInfo*> argTypes;
            std::vector<LucisParser::ExpressionContext*> argExprs;
            if (auto* argList = stmt->argList()) {
                for (auto* argExpr : argList->expression()) {
                    argExprs.push_back(argExpr);
                    argTypes.push_back(resolveExprType(argExpr));
                }
            }

            // A value argument cannot have type void.
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* ti = argTypes[i];
                if (ti && ti->kind == TypeKind::Void) {
                    error(argExprs[i],
                          "argument " + std::to_string(i + 1) +
                          " has type 'void'; functions returning void cannot be used as values");
                    return;
                }
            }

            size_t argCount = argTypes.size();
            size_t paramCount = localType->paramTypes.size();

            if (argCount != paramCount) {
                error(stmt, "function '" + name + "' expects " +
                                 std::to_string(paramCount) +
                                 " arguments " + formatParamTypes(localType->paramTypes) +
                                 ", got " + std::to_string(argCount));
                return;
            }
            for (size_t i = 0; i < argCount; i++) {
                if (argTypes[i] && localType->paramTypes[i] &&
                    !isAssignable(localType->paramTypes[i], argTypes[i])) {
                    error(stmt,
                        "argument " + std::to_string(i + 1) +
                        " type mismatch in '" + name + "': expected '" +
                        localType->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
            applyCallOwnershipEffects(name, argExprs, stmt);
            return;
        }
    }

    // Check for function-like macro calls (e.g. SQUARE(3))
    if (cFunctionLikeMacros_.count(name)) {
        auto* flm = cFunctionLikeMacros_[name];
        size_t argCount = stmt->argList() ? stmt->argList()->expression().size() : 0;
        if (argCount != flm->paramNames.size()) {
            error(stmt, "function-like macro '" + name + "' expects " +
                  std::to_string(flm->paramNames.size()) + " argument(s), got " +
                  std::to_string(argCount));
        }
        return;
    }

    if (!isKnownFunction(name)) {
        std::string hint = ImportResolver::suggestImport(name);
        std::string msg = "call to undeclared function '" + name + "'";
        if (!hint.empty())
            msg += ". Did you forget '" + hint + "'?";
        error(stmt, msg);
        return;
    }

    // Warn if calling a deprecated function
    {
        auto declIt = functionDecls_.find(name);
        if (declIt != functionDecls_.end() &&
            declIt->second->attributeList()) {
            if (hasAttribute(declIt->second->attributeList(), "deprecated"))
                warning(stmt, "call to deprecated function '" + name + "'");
            if (hasAttribute(declIt->second->attributeList(), "must_use"))
                warning(stmt, "unused return value: '" + name +
                        "' is marked #[must_use]");
        }
    }

    // Resolve all argument types
    std::vector<const TypeInfo*> argTypes;
    std::vector<LucisParser::ExpressionContext*> argExprs;
    if (auto* argList = stmt->argList()) {
        for (auto* argExpr : argList->expression()) {
            argExprs.push_back(argExpr);
            argTypes.push_back(resolveExprType(argExpr));
        }
    }

    // A value argument cannot have type void.
    for (size_t i = 0; i < argTypes.size(); i++) {
        auto* ti = argTypes[i];
        if (ti && ti->kind == TypeKind::Void) {
            error(argExprs[i],
                  "argument " + std::to_string(i + 1) +
                  " has type 'void'; functions returning void cannot be used as values");
            return;
        }
    }

    // Guard against invalid ABI lowering for array values in std::log print calls.
    // Arrays do not map to scalar print builtins; require explicit conversion.
    if ((name == "print" || name == "println" || name == "eprint" || name == "eprintln") &&
        (imports_.isImported(name) || functions_.count(name))) {
        for (size_t i = 0; i < argExprs.size(); i++) {
            if (resolveExprArrayDims(argExprs[i]) > 0) {
                error(stmt, "function '" + name + "' does not accept array arguments directly; "
                            "use '.toString()' (or '.join(...)' for strings) before printing");
                return;
            }
        }
    }
    // Lucis std::log::sprintf expects Lucis `string` values, not `*char`.
    // Passing `*char` reaches invalid IR lowering; fail early in checker.
    if (name == "sprintf" && imports_.isImported("sprintf")) {
        for (size_t i = 0; i < argTypes.size(); i++) {
            auto* ti = argTypes[i];
            if (!ti) continue;
            if (ti->kind == TypeKind::Pointer &&
                ti->pointeeType &&
                ti->pointeeType->kind == TypeKind::Char) {
                error(argExprs[i],
                      "'sprintf' from Lucis does not accept '*char'; "
                      "pass 'string' values (use 'fromCStr(...)' if needed)");
            }
        }
    }

    // Generic function call with inferred type arguments: foo(10);
    auto gfit = genericFuncTemplates_.find(name);
    if (gfit != genericFuncTemplates_.end()) {
        std::vector<LucisParser::ParamContext*> formalParams;
        if (auto* paramList = gfit->second.decl->paramList())
            formalParams = paramList->param();

        // Check for typed variadic last param: T ...args
        bool hasTypedVariadic = false;
        if (!formalParams.empty()) {
            auto* last = formalParams.back();
            hasTypedVariadic = last && last->SPREAD() && last->typeSpec();
        }
        size_t fixedCount = hasTypedVariadic ? formalParams.size() - 1 : formalParams.size();

        if (!hasTypedVariadic && argTypes.size() != formalParams.size()) {
            error(stmt, "generic function '" + name + "' expects " +
                         std::to_string(formalParams.size()) +
                         " argument(s), got " + std::to_string(argTypes.size()));
            return;
        }
        if (hasTypedVariadic && argTypes.size() < fixedCount) {
            error(stmt, "generic function '" + name + "' expects at least " +
                         std::to_string(fixedCount) +
                         " argument(s), got " + std::to_string(argTypes.size()));
            return;
        }

        auto inferred = inferGenericTypeArgs(
            name,
            gfit->second.typeParams,
            gfit->second.decl->typeParamList(),
            formalParams,
            argTypes,
            stmt);
        if (!inferred) return;

        instantiateGenericFunc(name, gfit->second, *inferred, stmt);
        applyCallOwnershipEffects(name, argExprs, stmt);
        return;
    }

    // Check user-defined function signatures
    auto fit = functions_.find(name);
    if (fit != functions_.end() && fit->second->kind == TypeKind::Function) {
        size_t argCount = argTypes.size();
        auto* fnType = fit->second;
        size_t paramCount = fnType->paramTypes.size();

        if (fnType->isVariadic) {
            bool isTyped = fnType->variadicElementType != nullptr;
            size_t fixedCount = isTyped && paramCount > 0 ? paramCount - 1 : paramCount;

            // Typed variadic: fixed params are paramCount-1 (last is element type)
            if (argCount < fixedCount) {
                error(stmt, "function '" + name + "' expects at least " +
                                 std::to_string(fixedCount) +
                                 " arguments " + formatParamTypes(fnType->paramTypes) +
                                 ", got " + std::to_string(argCount));
            }
            // Validate fixed parameter types
            for (size_t i = 0; i < std::min(argCount, fixedCount); i++) {
                if (argTypes[i] && fnType->paramTypes[i] &&
                    !isAssignable(fnType->paramTypes[i], argTypes[i])) {
                    error(stmt,
                        "argument " + std::to_string(i + 1) +
                        " type mismatch in '" + name + "': expected '" +
                        fnType->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
            // Validate variadic args against element type (typed variadic only)
            if (isTyped) {
                for (size_t i = fixedCount; i < argCount; i++) {
                    if (argTypes[i] && fnType->variadicElementType &&
                        !isAssignable(fnType->variadicElementType, argTypes[i])) {
                        error(stmt,
                            "argument " + std::to_string(i + 1) +
                            " type mismatch in '" + name + "': expected '" +
                            fnType->variadicElementType->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
            }
        } else {
            if (argCount != paramCount) {
                error(stmt, "function '" + name + "' expects " +
                                 std::to_string(paramCount) +
                                 " arguments " + formatParamTypes(fnType->paramTypes) +
                                 ", got " + std::to_string(argCount));
            } else {
                for (size_t i = 0; i < argCount; i++) {
                    if (argTypes[i] && fnType->paramTypes[i] &&
                        !isAssignable(fnType->paramTypes[i], argTypes[i])) {
                        error(stmt,
                            "argument " + std::to_string(i + 1) +
                            " type mismatch in '" + name + "': expected '" +
                            fnType->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
            }
        }
        analyzeUnsafeCBufferCall(name, stmt, argExprs);
        applyCallOwnershipEffects(name, argExprs, stmt);
        return;
    }

    // Check builtin function signatures
    auto* sig = builtinRegistry_.lookup(name);
    if (!sig) return; // unknown builtin, already reported

    size_t argCount = argTypes.size();
    if (sig->isVariadic) {
        if (argCount < sig->paramTypes.size()) {
            error(stmt, "'" + name + "' expects at least " +
                             std::to_string(sig->paramTypes.size()) +
                             " argument(s) " + formatParamTypes(sig->paramTypes) +
                             ", got " + std::to_string(argCount));
            return;
        }
    } else if (argCount != sig->paramTypes.size()) {
        error(stmt, "'" + name + "' expects " +
                         std::to_string(sig->paramTypes.size()) +
                         " argument(s) " + formatParamTypes(sig->paramTypes) +
                         ", got " + std::to_string(argCount));
        return;
    }

    // Validate argument types for non-polymorphic builtins
    if (!sig->isPolymorphic) {
        for (size_t i = 0; i < argCount; i++) {
            if (!argTypes[i]) continue;
            auto& expected = sig->paramTypes[i];
            auto* argTI = argTypes[i];

            if (expected == "_any" || expected == "_numeric" ||
                expected == "_integer" || expected == "_float")
                continue;

            auto* expectedTI = resolveBuiltinReturnType(expected);
            if (expectedTI && !isAssignable(expectedTI, argTI)) {
                error(stmt, "'" + name + "' argument " +
                    std::to_string(i + 1) + ": expected '" +
                    expected + "', got '" + argTI->name + "'");
            }
        }
    }

    // Cross-parameter validation for polymorphic builtins
    if (sig->isPolymorphic && argCount >= 2) {
        auto isNumeric = [](const TypeInfo* t) {
            return t && (t->kind == TypeKind::Integer ||
                         t->kind == TypeKind::Float);
        };
        auto isScalar = [](const TypeInfo* t) {
            return t && (t->kind == TypeKind::Integer ||
                         t->kind == TypeKind::Float ||
                         t->kind == TypeKind::Bool ||
                         t->kind == TypeKind::Char);
        };
        bool allAny = true, allNumeric = true;
        for (auto& p : sig->paramTypes) {
            if (p != "_any") allAny = false;
            if (p != "_numeric") allNumeric = false;
        }
        if (allAny) {
            auto* firstType = argTypes[0];
            for (size_t i = 1; i < argCount; i++) {
                if (!argTypes[i] || !firstType) continue;
                bool stringVsScalar =
                    (firstType->kind == TypeKind::String && isScalar(argTypes[i])) ||
                    (argTypes[i]->kind == TypeKind::String && isScalar(firstType));
                if (stringVsScalar) {
                    error(stmt, "'" + name + "' argument " +
                        std::to_string(i + 1) + ": type mismatch, expected '" +
                        firstType->name + "', got '" + argTypes[i]->name + "'");
                }
            }
        }
        if (allNumeric) {
            for (size_t i = 0; i < argCount; i++) {
                if (!argTypes[i]) continue;
                if (!isNumeric(argTypes[i])) {
                    error(stmt, "'" + name + "' argument " +
                        std::to_string(i + 1) + ": expected numeric type, got '" +
                        argTypes[i]->name + "'");
                }
            }
        }
    }

    analyzeUnsafeCBufferCall(name, stmt, argExprs);
    applyCallOwnershipEffects(name, argExprs, stmt);
}



void Checker::checkAsmStmt(LucisParser::AsmStmtContext* stmt) {
    bool hasOutput = stmt->asmOutputList() != nullptr;
    bool hasInput = stmt->asmInputList() != nullptr;
    bool isVolatile = stmt->VOLATILE() != nullptr;

    // Count outputs for matching-constraint validation
    size_t numOutputs = 0;
    if (hasOutput)
        numOutputs = stmt->asmOutputList()->asmOutput().size();

    // Suggest volatile if no outputs (side-effect only asm)
    if (!hasOutput && !isVolatile) {
        warning(stmt, "asm statement with no outputs should be 'asm volatile'");
    }

    // Validate output constraints and variables
    if (auto* outList = stmt->asmOutputList()) {
        for (auto* out : outList->asmOutput()) {
            auto raw = out->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);

            // Output constraints must start with '=', '+', or be a digit (matching)
            if (constraint.empty()) {
                error(out, "asm output constraint cannot be empty");
            } else if (constraint[0] != '=' && constraint[0] != '+'
                       && !std::isdigit(constraint[0])) {
                error(out, "asm output constraint must start with '=', '+', or a digit "
                      "(got '" + constraint + "')");
            }

            if (!out->IDENTIFIER()) continue; // unnamed output — nothing to validate
            auto varName = out->IDENTIFIER()->getText();
            auto it = locals_.find(varName);
            if (it == locals_.end()) {
                error(out, "undefined variable '" + varName + "' in asm output");
                continue;
            }

            // Mark output variable as used and initialized
            it->second.used = true;
        }
    }

    // Validate input constraints and expressions
    if (auto* inList = stmt->asmInputList()) {
        for (auto* operand : inList->asmOperand()) {
            auto raw = operand->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);

            if (constraint.empty()) {
                error(operand, "asm input constraint cannot be empty");
            } else if (constraint[0] == '=') {
                error(operand, "asm input constraint cannot use '=' (reserved for outputs)");
            } else if (constraint[0] == '+') {
                error(operand, "asm input constraint cannot use '+' (use in output with '+r' instead)");
            } else if (std::isdigit(constraint[0])) {
                // Matching constraint — validate index
                int matchIdx = std::stoi(constraint);
                if (matchIdx < 0 || static_cast<size_t>(matchIdx) >= numOutputs) {
                    error(operand, "asm matching constraint '" + constraint
                          + "' refers to output " + constraint + " but there "
                          + (numOutputs == 1 ? "is only 1 output"
                                             : "are only " + std::to_string(numOutputs) + " outputs"));
                }
            }

            resolveExprType(operand->expression());
        }
    }

    // Collect goto label references (validated at function end for forward refs)
    if (auto* labelList = stmt->asmGotoLabelList()) {
        for (auto* ident : labelList->IDENTIFIER())
            asmGotoLabelRefs_.insert(ident->getText());
    }
}



void Checker::checkExprStmt(LucisParser::ExprStmtContext* stmt) {
    resolveExprType(stmt->expression());
}



void Checker::checkBailStmt(LucisParser::BailStmtContext* stmt) {
    if (!currentReturnType_ || currentReturnType_->kind != TypeKind::Fallible) {
        error(stmt, "'bail' só pode ser usado dentro de função com retorno !T");
        return;
    }
    auto* exprType = resolveExprType(stmt->expression());
    if (!exprType) return;

    if (isAssignable(currentReturnType_->errorType, exprType))
        return;

    if (exprType->kind == TypeKind::String) {
        return;
    }

    error(stmt, "'bail' type mismatch: expected '" +
               currentReturnType_->errorType->name + "', got '" +
               exprType->name + "'");
}



void Checker::checkReturnStmt(LucisParser::ReturnStmtContext* stmt,
                               const TypeInfo* expectedType) {
    auto* expr = stmt->expression();

    if (!expr) {
        if (expectedType->kind != TypeKind::Void) {
            error(stmt,
                "function with return type '" + expectedType->name +
                "' must return a value");
        }
        return;
    }

    // void function should not return a value
    if (expectedType->kind == TypeKind::Void) {
        error(stmt, "void function should not return a value");
        return;
    }

    auto* exprType = resolveExprType(expr);
    if (exprType && expectedType && !isAssignable(expectedType, exprType)) {
        error(stmt, "return type mismatch: expected '" +
                         expectedType->name + "', got '" + exprType->name + "'");
    }
    markExprAsMoved(expr, stmt);
}



void Checker::warnUnusedLocals(LucisParser::FunctionDeclContext* func) {
    warnUnusedLocals(static_cast<antlr4::ParserRuleContext*>(func));
}

void Checker::warnUnusedLocals(antlr4::ParserRuleContext* ctx) {
    for (auto& [name, info] : locals_) {
        if (!info.used && name != "_") {
            // Skip warning for top-level constants (they may be used externally)
            if (info.isConst && info.scopeDepth == 0)
                continue;
            if (info.declToken) {
                warningToken(info.declToken, info.declToken,
                             "variable '" + name + "' is declared but never used");
            } else {
                warning(ctx, "variable '" + name + "' is declared but never used");
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  User-defined generics — monomorphization
// ═══════════════════════════════════════════════════════════════════════


