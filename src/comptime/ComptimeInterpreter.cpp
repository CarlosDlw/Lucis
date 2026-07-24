#include "comptime/ComptimeInterpreter.h"
#include "generated/LucisParser.h"
#include "generated/LucisLexer.h"

ComptimeInterpreter::ComptimeInterpreter(
    const TypeRegistry& types, ComptimeRegistry& registry,
    const ComptimeBuiltinRegistry* builtins,
    ConstEvalFn constEval)
    : types_(types), registry_(registry), builtins_(builtins), constEval_(constEval) {}

ComptimeValue ComptimeInterpreter::evaluate(
    void* funcDecl, const std::vector<ComptimeValue>& args) {
    auto* func = static_cast<LucisParser::FunctionDeclContext*>(funcDecl);
    Frame frame;
    auto* params = func->paramList();
    if (params) {
        size_t i = 0;
        for (auto* p : params->param()) {
            if (i < args.size()) {
                if (auto* id = p->IDENTIFIER())
                    frame.locals[id->getText()] = args[i];
            }
            i++;
        }
    }
    frames_.push_back(std::move(frame));
    auto result = evalBlock(func->block());
    frames_.pop_back();
    return result;
}

ComptimeValue ComptimeInterpreter::evalBlock(void* blockCtx) {
    auto* block = static_cast<LucisParser::BlockContext*>(blockCtx);
    for (auto* stmt : block->statement()) {
        auto result = evalStmt(stmt);
        if (result.kind() != ComptimeValue::Kind::Void)
            return result;
    }
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalStmt(void* stmtCtx) {
    auto* stmt = static_cast<LucisParser::StatementContext*>(stmtCtx);

    if (auto* ret = stmt->returnStmt()) return evalReturn(ret);
    if (auto* ifs = stmt->ifStmt()) return evalIf(ifs);
    if (auto* decl = stmt->varDeclStmt()) return evalVarDecl(decl);
    if (auto* assign = stmt->assignStmt()) return evalAssign(assign);
    if (auto* compound = stmt->compoundAssignStmt()) return evalCompoundAssign(compound);
    if (auto* ws = stmt->whileStmt()) return evalWhile(ws);
    if (auto* forStmt = stmt->forStmt()) {
        if (auto* forIn = dynamic_cast<LucisParser::ForInStmtContext*>(forStmt))
            return evalForIn(forIn);
        return ComptimeValue::voidVal();
    }

    // Expr statement — look for fn call or match
    if (auto* es = stmt->exprStmt()) {
        auto* expr = es->expression();
        if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr))
            return evalFnCall(call);
        if (auto* match = dynamic_cast<LucisParser::MatchExprContext*>(expr))
            return evalMatch(match);
    }

    // Simple callStmt: name(args)
    if (auto* cs = stmt->callStmt()) {
        auto* id = cs->IDENTIFIER();
        if (registry_.isComptime(id->getText())) {
            std::vector<ComptimeValue> args;
            if (auto* argList = cs->argList()) {
                for (auto* arg : argList->expression())
                    args.push_back(evalExpr(arg));
            }
            auto* decl = registry_.lookup(id->getText());
            if (decl) return evaluate(decl, args);
        }
    }

    if (auto* naked = stmt->nakedBlockStmt())
        return evalBlock(naked);

    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalReturn(void* retCtx) {
    auto* ret = static_cast<LucisParser::ReturnStmtContext*>(retCtx);
    if (!ret->expression()) return ComptimeValue::voidVal();
    return evalExpr(ret->expression());
}

ComptimeValue ComptimeInterpreter::evalIf(void* ifCtx) {
    auto* ifStmt = static_cast<LucisParser::IfStmtContext*>(ifCtx);
    auto cond = evalExpr(ifStmt->expression());
    bool isTrue = cond.kind() == ComptimeValue::Kind::Bool ? cond.asBool()
                : cond.kind() == ComptimeValue::Kind::Int ? cond.asInt() != 0 : false;
    if (isTrue) {
        if (auto* b = ifStmt->ifBody()->block()) return evalBlock(b);
        return ComptimeValue::voidVal();
    }
    for (auto* eif : ifStmt->elseIfClause()) {
        auto ec = evalExpr(eif->expression());
        bool et = ec.kind() == ComptimeValue::Kind::Bool ? ec.asBool()
                : ec.kind() == ComptimeValue::Kind::Int ? ec.asInt() != 0 : false;
        if (et) {
            if (auto* b = eif->ifBody()->block()) return evalBlock(b);
            return ComptimeValue::voidVal();
        }
    }
    if (auto* ec = ifStmt->elseClause()) {
        if (auto* b = ec->ifBody()->block()) return evalBlock(b);
    }
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalVarDecl(void* declCtx) {
    auto* decl = static_cast<LucisParser::VarDeclStmtContext*>(declCtx);
    if (decl->varDeclarator().empty()) return ComptimeValue::voidVal();
    auto* d = decl->varDeclarator(0);
    if (!d->expression()) return ComptimeValue::voidVal();
    auto val = evalExpr(d->expression());
    if (auto* id = d->IDENTIFIER())
        frames_.back().locals[id->getText()] = val;
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalAssign(void* assignCtx) {
    auto* assign = static_cast<LucisParser::AssignStmtContext*>(assignCtx);
    if (!assign->IDENTIFIER()) return ComptimeValue::voidVal();
    auto val = evalExpr(assign->expression(0));
    frames_.back().locals[assign->IDENTIFIER()->getText()] = val;
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalCompoundAssign(void* compoundCtx) {
    auto* ca = static_cast<LucisParser::CompoundAssignStmtContext*>(compoundCtx);
    if (!ca->IDENTIFIER()) return ComptimeValue::voidVal();
    auto name = ca->IDENTIFIER()->getText();
    auto* it = [&]() -> std::unordered_map<std::string, ComptimeValue>* {
        for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
            if (it->locals.count(name)) return &it->locals;
        }
        return nullptr;
    }();
    if (!it) return ComptimeValue::voidVal();

    auto& var = (*it)[name];
    auto rhs = evalExpr(ca->expression());
    if (var.kind() == ComptimeValue::Kind::Int && rhs.kind() == ComptimeValue::Kind::Int) {
        if (ca->PLUS_ASSIGN())   var = ComptimeValue::intVal(var.asInt() + rhs.asInt());
        else if (ca->MINUS_ASSIGN())  var = ComptimeValue::intVal(var.asInt() - rhs.asInt());
        else if (ca->STAR_ASSIGN())   var = ComptimeValue::intVal(var.asInt() * rhs.asInt());
        else if (ca->SLASH_ASSIGN())  var = rhs.asInt() != 0 ? ComptimeValue::intVal(var.asInt() / rhs.asInt()) : ComptimeValue::intVal(0);
        else if (ca->PERCENT_ASSIGN()) var = rhs.asInt() != 0 ? ComptimeValue::intVal(var.asInt() % rhs.asInt()) : ComptimeValue::intVal(0);
        else if (ca->AMP_ASSIGN())    var = ComptimeValue::intVal(var.asInt() & rhs.asInt());
        else if (ca->PIPE_ASSIGN())   var = ComptimeValue::intVal(var.asInt() | rhs.asInt());
    }
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalForIn(void* forCtx) {
    auto* forStmt = static_cast<LucisParser::ForInStmtContext*>(forCtx);
    auto* id = forStmt->IDENTIFIER();
    if (!id) return ComptimeValue::voidVal();
    auto varName = id->getText();

    // Evaluate the iterable
    auto* iterExpr = forStmt->expression();

    // Try range evaluation: 0..n or 0..=n
    if (auto* range = dynamic_cast<LucisParser::RangeExprContext*>(iterExpr)) {
        auto start = evalExpr(range->expression(0));
        auto end   = evalExpr(range->expression(1));
        if (start.kind() == ComptimeValue::Kind::Int && end.kind() == ComptimeValue::Kind::Int) {
            for (int64_t i = start.asInt(); i < end.asInt(); i++) {
                frames_.back().locals[varName] = ComptimeValue::intVal(i);
                auto result = evalBlock(forStmt->block());
                if (result.kind() != ComptimeValue::Kind::Void) return result;
            }
        }
        return ComptimeValue::voidVal();
    }
    if (auto* rangeIncl = dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr)) {
        auto start = evalExpr(rangeIncl->expression(0));
        auto end   = evalExpr(rangeIncl->expression(1));
        if (start.kind() == ComptimeValue::Kind::Int && end.kind() == ComptimeValue::Kind::Int) {
            for (int64_t i = start.asInt(); i <= end.asInt(); i++) {
                frames_.back().locals[varName] = ComptimeValue::intVal(i);
                auto result = evalBlock(forStmt->block());
                if (result.kind() != ComptimeValue::Kind::Void) return result;
            }
        }
        return ComptimeValue::voidVal();
    }

    // Generic list iteration
    auto iterable = evalExpr(iterExpr);
    if (iterable.kind() == ComptimeValue::Kind::List) {
        for (auto& elem : iterable.asList()) {
            frames_.back().locals[varName] = elem;
            auto result = evalBlock(forStmt->block());
            if (result.kind() != ComptimeValue::Kind::Void) return result;
        }
    }
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalWhile(void* whileCtx) {
    auto* ws = static_cast<LucisParser::WhileStmtContext*>(whileCtx);
    while (true) {
        auto cond = evalExpr(ws->expression());
        bool isTrue = cond.kind() == ComptimeValue::Kind::Bool ? cond.asBool()
                    : cond.kind() == ComptimeValue::Kind::Int ? cond.asInt() != 0 : false;
        if (!isTrue) break;
        auto r = evalBlock(ws->block());
        if (r.kind() != ComptimeValue::Kind::Void) return r;
    }
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalExpr(void* exprCtx) {
    auto* expr = static_cast<LucisParser::ExpressionContext*>(exprCtx);

    // Literals
    if (auto* i = dynamic_cast<LucisParser::IntLitExprContext*>(expr))
        return ComptimeValue::intVal(std::stoll(i->INT_LIT()->getText()));
    if (auto* f = dynamic_cast<LucisParser::FloatLitExprContext*>(expr))
        return ComptimeValue::floatVal(std::stod(f->FLOAT_LIT()->getText()));
    if (auto* b = dynamic_cast<LucisParser::BoolLitExprContext*>(expr))
        return ComptimeValue::boolVal(b->BOOL_LIT()->getText() == "true");

    // String literal
    if (auto* sl = dynamic_cast<LucisParser::StrLitExprContext*>(expr)) {
        auto raw = sl->STR_LIT()->getText();
        if (raw.size() >= 2) raw = raw.substr(1, raw.size() - 2);
        return ComptimeValue::stringVal(raw);
    }

    // Identifier lookup
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
            auto f = it->locals.find(name);
            if (f != it->locals.end()) return f->second;
        }
        return ComptimeValue::voidVal();
    }

    // Index access: list[idx]
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        auto base = evalExpr(idx->expression(0));
        auto index = evalExpr(idx->expression(1));
        if (base.kind() == ComptimeValue::Kind::List &&
            index.kind() == ComptimeValue::Kind::Int) {
            auto& list = base.asList();
            size_t i = static_cast<size_t>(index.asInt());
            if (i < list.size()) return list[i];
        }
        return ComptimeValue::voidVal();
    }

    // Function call & match
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr))
        return evalFnCall(call);
    if (auto* match = dynamic_cast<LucisParser::MatchExprContext*>(expr))
        return evalMatch(match);

    // Unary operators
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        auto v = evalExpr(neg->expression());
        if (v.kind() == ComptimeValue::Kind::Int) return ComptimeValue::intVal(-v.asInt());
        if (v.kind() == ComptimeValue::Kind::Float) return ComptimeValue::floatVal(-v.asFloat());
        return v;
    }

    // Relational: < > <= >=
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        auto l = evalExpr(rel->expression(0));
        auto r = evalExpr(rel->expression(1));
        if (l.kind() == ComptimeValue::Kind::Int && r.kind() == ComptimeValue::Kind::Int) {
            auto op = rel->op->getType();
            if (op == LucisLexer::LT)  return ComptimeValue::boolVal(l.asInt() < r.asInt());
            if (op == LucisLexer::GT)  return ComptimeValue::boolVal(l.asInt() > r.asInt());
            if (op == LucisLexer::LTE) return ComptimeValue::boolVal(l.asInt() <= r.asInt());
            if (op == LucisLexer::GTE) return ComptimeValue::boolVal(l.asInt() >= r.asInt());
        }
        return ComptimeValue::voidVal();
    }

    // Equality: == !=
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        auto l = evalExpr(eq->expression(0));
        auto r = evalExpr(eq->expression(1));
        if (l.kind() == ComptimeValue::Kind::Int && r.kind() == ComptimeValue::Kind::Int) {
            if (eq->EQ()) return ComptimeValue::boolVal(l.asInt() == r.asInt());
            if (eq->NEQ()) return ComptimeValue::boolVal(l.asInt() != r.asInt());
        }
        return ComptimeValue::voidVal();
    }

    // Add/Sub
    if (auto* as = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        auto l = evalExpr(as->expression(0));
        auto r = evalExpr(as->expression(1));
        if (l.kind() == ComptimeValue::Kind::Int && r.kind() == ComptimeValue::Kind::Int) {
            if (as->PLUS()) return ComptimeValue::intVal(l.asInt() + r.asInt());
            if (as->MINUS()) return ComptimeValue::intVal(l.asInt() - r.asInt());
        }
        if (l.kind() == ComptimeValue::Kind::Float && r.kind() == ComptimeValue::Kind::Float) {
            if (as->PLUS()) return ComptimeValue::floatVal(l.asFloat() + r.asFloat());
            if (as->MINUS()) return ComptimeValue::floatVal(l.asFloat() - r.asFloat());
        }
        return l;
    }

    // Mul/Div/Mod
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        auto l = evalExpr(mul->expression(0));
        auto r = evalExpr(mul->expression(1));
        if (l.kind() == ComptimeValue::Kind::Int && r.kind() == ComptimeValue::Kind::Int) {
            if (mul->STAR()) return ComptimeValue::intVal(l.asInt() * r.asInt());
            if (mul->SLASH()) return r.asInt() != 0 ? ComptimeValue::intVal(l.asInt() / r.asInt()) : ComptimeValue::intVal(0);
            if (mul->PERCENT()) return r.asInt() != 0 ? ComptimeValue::intVal(l.asInt() % r.asInt()) : ComptimeValue::intVal(0);
        }
        return l;
    }

    // Bitwise AND
    if (auto* band = dynamic_cast<LucisParser::BitAndExprContext*>(expr)) {
        auto l = evalExpr(band->expression(0));
        auto r = evalExpr(band->expression(1));
        if (l.kind() == ComptimeValue::Kind::Int && r.kind() == ComptimeValue::Kind::Int)
            return ComptimeValue::intVal(l.asInt() & r.asInt());
        return l;
    }

    // Logical NOT
    if (auto* notE = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr)) {
        auto v = evalExpr(notE->expression());
        if (v.kind() == ComptimeValue::Kind::Bool) return ComptimeValue::boolVal(!v.asBool());
        if (v.kind() == ComptimeValue::Kind::Int) return ComptimeValue::intVal(~v.asInt());
        return v;
    }

    // Parenthesized expression
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return evalExpr(paren->expression());

    // CompoundAssign in while body: x += 1, etc.
    // These are handled as statements, not expressions.

    if (constEval_) {
        auto r = constEval_(expr);
        if (r.has_value()) return *r;
    }
    return ComptimeValue::voidVal();
}

ComptimeValue ComptimeInterpreter::evalFnCall(void* callCtx) {
    auto* call = static_cast<LucisParser::FnCallExprContext*>(callCtx);
    std::string name;
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(call->expression()))
        name = id->IDENTIFIER()->getText();
    else return ComptimeValue::voidVal();

    // Check builtin registry first
    if (builtins_) {
        if (auto* builtin = builtins_->lookup(name)) {
            std::vector<ComptimeValue> args;
            if (auto* al = call->argList()) {
                for (auto* a : al->expression()) args.push_back(evalExpr(a));
            }
            return builtin->fn(args);
        }
    }

    auto* decl = registry_.lookup(name);
    if (!decl) return ComptimeValue::voidVal();

    auto* callee = static_cast<LucisParser::FunctionDeclContext*>(decl);
    std::vector<ComptimeValue> args;
    if (auto* al = call->argList()) {
        for (auto* a : al->expression()) args.push_back(evalExpr(a));
    }

    Frame frame;
    if (auto* pl = callee->paramList()) {
        size_t i = 0;
        for (auto* p : pl->param()) {
            if (i < args.size()) {
                if (auto* id = p->IDENTIFIER())
                    frame.locals[id->getText()] = args[i];
            }
            i++;
        }
    }
    frames_.push_back(std::move(frame));
    auto r = evalBlock(callee->block());
    frames_.pop_back();
    return r;
}

ComptimeValue ComptimeInterpreter::evalMatch(void* matchCtx) {
    auto* match = static_cast<LucisParser::MatchExprContext*>(matchCtx);
    auto val = evalExpr(match->expression());

    for (auto* arm : match->matchArm()) {
        auto patterns = arm->pattern();
        if (patterns.empty()) {
            // default arm
            if (arm->block()) return evalBlock(arm->block());
            if (arm->expression().size() > 0) return evalExpr(arm->expression(0));
            continue;
        }

        for (auto* pat : patterns) {
            auto ids = pat->IDENTIFIER();
            if (ids.empty()) {
                // wildcard _
                if (arm->block()) return evalBlock(arm->block());
                if (arm->expression().size() > 0) return evalExpr(arm->expression(0));
                continue;
            }

            // Enum variant pattern: VariantName(binding)
            if (ids.size() >= 2) {
                auto varName = ids[0]->getText();
                auto bindName = ids[1]->getText();
                if (val.kind() == ComptimeValue::Kind::Struct) {
                    auto& fields = val.asStruct();
                    auto it = fields.find(varName);
                    if (it != fields.end()) {
                        frames_.back().locals[bindName] = it->second;
                        if (arm->block()) return evalBlock(arm->block());
                        if (arm->expression().size() > 0) return evalExpr(arm->expression(0));
                        return it->second;
                    }
                }
                continue;
            }

            // Single identifier: binding or unit variant
            auto name = ids[0]->getText();
            if (name == "_") {
                if (arm->block()) return evalBlock(arm->block());
                if (arm->expression().size() > 0) return evalExpr(arm->expression(0));
                continue;
            }

            // Check if it's a unit variant in the value
            if (val.kind() == ComptimeValue::Kind::Struct && val.asStruct().count(name)) {
                if (arm->block()) return evalBlock(arm->block());
                if (arm->expression().size() > 0) return evalExpr(arm->expression(0));
                continue;
            }

            // Otherwise it's a binding — capture the value
            frames_.back().locals[name] = val;
            return val;
        }
    }

    return ComptimeValue::voidVal();
}
