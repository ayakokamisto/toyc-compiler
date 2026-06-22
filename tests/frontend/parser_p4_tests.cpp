#include "ast/ast.h"
#include "common/token_stream.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "parser P4 test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

struct ParseResult {
    toyc::TokenStream tokens;
    toyc::parser::Parser parser;
    std::unique_ptr<toyc::ast::CompUnit> unit;

    explicit ParseResult(std::string_view source)
        : tokens(toyc::lex(source)), parser(tokens), unit(parser.parseCompUnit()) {}
};

const toyc::ast::FuncDef& functionAt(const ParseResult& result, std::size_t index) {
    require(index < result.unit->declarations.size(), "function index");
    const auto* function =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[index].get());
    require(function != nullptr && function->body != nullptr, "function definition");
    return *function;
}

void testIfWithoutElse() {
    ParseResult result("int main(){if(a<b)return 1;return 0;}");
    require(!result.parser.hasError(), "if without else parses");
    const auto& body = *functionAt(result, 0).body;
    require(body.statements.size() == 2, "if and following return");
    const auto* statement = dynamic_cast<const toyc::ast::IfStmt*>(body.statements[0].get());
    require(statement != nullptr && statement->elseBranch == nullptr, "optional else absent");
    const auto* condition =
        dynamic_cast<const toyc::ast::BinaryExpr*>(statement->condition.get());
    require(condition != nullptr && condition->op == toyc::TokenKind::Less,
            "if relational condition");
    require(dynamic_cast<const toyc::ast::ReturnStmt*>(statement->thenBranch.get()) != nullptr,
            "if then return");
    require(dynamic_cast<const toyc::ast::ReturnStmt*>(body.statements[1].get()) != nullptr,
            "statement after if remains");
}

void testIfElseWithBlocks() {
    ParseResult result("int main(){if(a){x=1;}else{x=2;}}");
    require(!result.parser.hasError(), "if else blocks parse");
    const auto* statement = dynamic_cast<const toyc::ast::IfStmt*>(
        functionAt(result, 0).body->statements[0].get());
    require(statement != nullptr && statement->elseBranch != nullptr, "if has else");
    const auto* thenBlock =
        dynamic_cast<const toyc::ast::BlockStmt*>(statement->thenBranch.get());
    const auto* elseBlock =
        dynamic_cast<const toyc::ast::BlockStmt*>(statement->elseBranch.get());
    require(thenBlock != nullptr && elseBlock != nullptr, "both branches are blocks");
    require(dynamic_cast<const toyc::ast::AssignStmt*>(thenBlock->statements[0].get()) !=
                nullptr,
            "then block assignment");
    require(dynamic_cast<const toyc::ast::AssignStmt*>(elseBlock->statements[0].get()) !=
                nullptr,
            "else block assignment");
    require(statement->range.end.line == elseBlock->range.end.line &&
                statement->range.end.column == elseBlock->range.end.column,
            "if range ends with else block");
}

void testDanglingElseBindsNearestIf() {
    ParseResult result("int main(){if(a)if(b)x=1;else x=2;}");
    require(!result.parser.hasError(), "dangling else input parses");
    const auto* outer = dynamic_cast<const toyc::ast::IfStmt*>(
        functionAt(result, 0).body->statements[0].get());
    require(outer != nullptr && outer->elseBranch == nullptr, "outer if has no else");
    const auto* inner = dynamic_cast<const toyc::ast::IfStmt*>(outer->thenBranch.get());
    require(inner != nullptr && inner->elseBranch != nullptr, "inner if owns else");
}

void testNestedIfWithOuterElse() {
    ParseResult result("int main(){if(a){if(b)x=1;else x=2;}else{x=3;}}");
    require(!result.parser.hasError(), "nested if with outer else parses");
    const auto* outer = dynamic_cast<const toyc::ast::IfStmt*>(
        functionAt(result, 0).body->statements[0].get());
    require(outer != nullptr && outer->elseBranch != nullptr, "outer if owns else");
    const auto* thenBlock =
        dynamic_cast<const toyc::ast::BlockStmt*>(outer->thenBranch.get());
    require(thenBlock != nullptr && thenBlock->statements.size() == 1, "outer then block");
    const auto* inner =
        dynamic_cast<const toyc::ast::IfStmt*>(thenBlock->statements[0].get());
    require(inner != nullptr && inner->elseBranch != nullptr, "inner if owns its else");
    require(dynamic_cast<const toyc::ast::BlockStmt*>(outer->elseBranch.get()) != nullptr,
            "outer else is separate block");
}

void testWhileWithSingleStatement() {
    ParseResult result("int main(){while(i<10)i=i+1;}");
    require(!result.parser.hasError(), "single-statement while parses");
    const auto* statement = dynamic_cast<const toyc::ast::WhileStmt*>(
        functionAt(result, 0).body->statements[0].get());
    require(statement != nullptr, "while node");
    const auto* condition =
        dynamic_cast<const toyc::ast::BinaryExpr*>(statement->condition.get());
    require(condition != nullptr && condition->op == toyc::TokenKind::Less,
            "while relational condition");
    require(dynamic_cast<const toyc::ast::AssignStmt*>(statement->body.get()) != nullptr,
            "while assignment body");
}

void testWhileWithBlockAndLoopControls() {
    ParseResult result(R"(
int main() {
    while (i < 10) {
        if (i == 5)
            break;
        if (i % 2 == 0) {
            i = i + 1;
            continue;
        }
        i = i + 1;
    }
}
)");
    require(!result.parser.hasError(), "while controls parse");
    const auto* loop = dynamic_cast<const toyc::ast::WhileStmt*>(
        functionAt(result, 0).body->statements[0].get());
    require(loop != nullptr, "while node with block");
    const auto* block = dynamic_cast<const toyc::ast::BlockStmt*>(loop->body.get());
    require(block != nullptr && block->statements.size() == 3, "while block structure");
    const auto* firstIf =
        dynamic_cast<const toyc::ast::IfStmt*>(block->statements[0].get());
    require(firstIf != nullptr &&
                dynamic_cast<const toyc::ast::BreakStmt*>(firstIf->thenBranch.get()) != nullptr,
            "break in first if");
    const auto* secondIf =
        dynamic_cast<const toyc::ast::IfStmt*>(block->statements[1].get());
    const auto* secondBlock = secondIf == nullptr
                                  ? nullptr
                                  : dynamic_cast<const toyc::ast::BlockStmt*>(
                                        secondIf->thenBranch.get());
    require(secondBlock != nullptr && secondBlock->statements.size() == 2,
            "second if block");
    require(dynamic_cast<const toyc::ast::ContinueStmt*>(
                secondBlock->statements[1].get()) != nullptr,
            "continue in second if");
    require(dynamic_cast<const toyc::ast::AssignStmt*>(block->statements[2].get()) != nullptr,
            "assignment after if statements");
}

void testBreakAndContinueOutsideLoopAreSyntacticallyAccepted() {
    ParseResult result("int main(){break;continue;return 0;}");
    require(!result.parser.hasError(), "loop controls accepted syntactically");
    const auto& statements = functionAt(result, 0).body->statements;
    require(statements.size() == 3, "outside-loop statement count");
    require(dynamic_cast<const toyc::ast::BreakStmt*>(statements[0].get()) != nullptr,
            "outside-loop break node");
    require(dynamic_cast<const toyc::ast::ContinueStmt*>(statements[1].get()) != nullptr,
            "outside-loop continue node");
    require(dynamic_cast<const toyc::ast::ReturnStmt*>(statements[2].get()) != nullptr,
            "return after loop controls");
    // Sema owns loop-context legality.
}

void testControlFlowSourceRanges() {
    ParseResult result(R"(int f() {
  if (a) {
    break;
  } else {
    continue;
  }
  while (b)
    break;
})");
    require(!result.parser.hasError(), "control-flow ranges parse");
    const auto& statements = functionAt(result, 0).body->statements;
    const auto* ifStatement = dynamic_cast<const toyc::ast::IfStmt*>(statements[0].get());
    require(ifStatement != nullptr && ifStatement->range.begin.line == 2 &&
                ifStatement->range.begin.column == 3 && ifStatement->range.end.line == 6 &&
                ifStatement->range.end.column == 4,
            "if source range");
    const auto* thenBlock =
        dynamic_cast<const toyc::ast::BlockStmt*>(ifStatement->thenBranch.get());
    const auto* breakStatement = thenBlock == nullptr
                                     ? nullptr
                                     : dynamic_cast<const toyc::ast::BreakStmt*>(
                                           thenBlock->statements[0].get());
    require(breakStatement != nullptr && breakStatement->range.begin.column == 5 &&
                breakStatement->range.end.column == 11,
            "break source range");
    const auto* elseBlock =
        dynamic_cast<const toyc::ast::BlockStmt*>(ifStatement->elseBranch.get());
    const auto* continueStatement = elseBlock == nullptr
                                        ? nullptr
                                        : dynamic_cast<const toyc::ast::ContinueStmt*>(
                                              elseBlock->statements[0].get());
    require(continueStatement != nullptr && continueStatement->range.begin.column == 5 &&
                continueStatement->range.end.column == 14,
            "continue source range");
    const auto* whileStatement =
        dynamic_cast<const toyc::ast::WhileStmt*>(statements[1].get());
    require(whileStatement != nullptr && whileStatement->range.begin.line == 7 &&
                whileStatement->range.begin.column == 3 && whileStatement->range.end.line == 8 &&
                whileStatement->range.end.column == 11,
            "while source range");
}

void testControlFlowRecovery() {
    ParseResult result(R"(
int main() {
    if (a return 1;
    int x = 0;
    while () { }
    return x;
}
)");
    require(result.parser.hasError(), "control-flow errors produce diagnostics");
    const auto& statements = functionAt(result, 0).body->statements;
    bool foundDeclaration = false;
    bool foundFinalReturn = false;
    for (const auto& statement : statements) {
        foundDeclaration = foundDeclaration ||
                           dynamic_cast<const toyc::ast::DeclStmt*>(statement.get()) != nullptr;
        const auto* returnStatement =
            dynamic_cast<const toyc::ast::ReturnStmt*>(statement.get());
        if (returnStatement != nullptr && returnStatement->value != nullptr) {
            const auto* reference =
                dynamic_cast<const toyc::ast::DeclRefExpr*>(returnStatement->value.get());
            foundFinalReturn = foundFinalReturn ||
                               (reference != nullptr && reference->name == "x");
        }
    }
    require(foundDeclaration, "recovery preserves following declaration");
    require(foundFinalReturn, "recovery preserves final return");

    ParseResult breakRecovery("int main(){break return 1;}");
    require(breakRecovery.parser.hasError(), "missing break semicolon diagnostic");
    require(functionAt(breakRecovery, 0).body->statements.size() == 1 &&
                dynamic_cast<const toyc::ast::ReturnStmt*>(
                    functionAt(breakRecovery, 0).body->statements[0].get()) != nullptr,
            "break recovery preserves return");

    ParseResult continueRecovery("int main(){continue return 2;}");
    require(continueRecovery.parser.hasError(), "missing continue semicolon diagnostic");
    require(functionAt(continueRecovery, 0).body->statements.size() == 1 &&
                dynamic_cast<const toyc::ast::ReturnStmt*>(
                    functionAt(continueRecovery, 0).body->statements[0].get()) != nullptr,
            "continue recovery preserves return");
}

void testMalformedControlFlowDiagnostics() {
    const std::string_view cases[] = {
        "int f(){if(a return 1;}", "int f(){if()return 1;}",
        "int f(){if(a)else return 1;}", "int f(){while(a return 1;}",
        "int f(){while(){}return 1;}", "int f(){break}", "int f(){continue}",
    };
    for (const std::string_view source : cases) {
        ParseResult result(source);
        require(result.parser.hasError(), "malformed control flow diagnostic");
        require(!result.parser.diagnostics().empty(), "control flow diagnostic collected");
    }
}

void testFullParserAcceptanceProgram() {
    ParseResult result(R"(
const int LIMIT = 10;

int sumEven(int n) {
    int i = 0;
    int sum = 0;

    while (i < n) {
        if (i % 2 != 0) {
            i = i + 1;
            continue;
        }

        sum = sum + i;

        if (sum > LIMIT * LIMIT) {
            break;
        }

        i = i + 1;
    }

    return sum;
}

int main() {
    return sumEven(LIMIT);
}
)");
    require(!result.parser.hasError(), "full parser acceptance program");
    require(result.unit->declarations.size() == 3, "acceptance top-level count");
    require(dynamic_cast<const toyc::ast::ConstDecl*>(result.unit->declarations[0].get()) !=
                nullptr,
            "acceptance global constant");
    const auto& sumEven = functionAt(result, 1);
    require(sumEven.name == "sumEven" && sumEven.body->statements.size() == 4,
            "sumEven body structure");
    require(dynamic_cast<const toyc::ast::DeclStmt*>(sumEven.body->statements[0].get()) !=
                nullptr &&
                dynamic_cast<const toyc::ast::DeclStmt*>(sumEven.body->statements[1].get()) !=
                    nullptr,
            "sumEven local declarations");
    const auto* loop =
        dynamic_cast<const toyc::ast::WhileStmt*>(sumEven.body->statements[2].get());
    require(loop != nullptr, "sumEven while loop");
    const auto* loopBody = dynamic_cast<const toyc::ast::BlockStmt*>(loop->body.get());
    require(loopBody != nullptr && loopBody->statements.size() == 4,
            "sumEven loop body structure");
    const auto* firstIf =
        dynamic_cast<const toyc::ast::IfStmt*>(loopBody->statements[0].get());
    const auto* firstBlock = firstIf == nullptr
                                 ? nullptr
                                 : dynamic_cast<const toyc::ast::BlockStmt*>(
                                       firstIf->thenBranch.get());
    require(firstBlock != nullptr &&
                dynamic_cast<const toyc::ast::ContinueStmt*>(
                    firstBlock->statements[1].get()) != nullptr,
            "acceptance continue");
    const auto* secondIf =
        dynamic_cast<const toyc::ast::IfStmt*>(loopBody->statements[2].get());
    const auto* secondBlock = secondIf == nullptr
                                  ? nullptr
                                  : dynamic_cast<const toyc::ast::BlockStmt*>(
                                        secondIf->thenBranch.get());
    require(secondBlock != nullptr &&
                dynamic_cast<const toyc::ast::BreakStmt*>(
                    secondBlock->statements[0].get()) != nullptr,
            "acceptance break");

    const auto& mainFunction = functionAt(result, 2);
    const auto* returnStatement = dynamic_cast<const toyc::ast::ReturnStmt*>(
        mainFunction.body->statements[0].get());
    require(returnStatement != nullptr &&
                dynamic_cast<const toyc::ast::CallExpr*>(returnStatement->value.get()) !=
                    nullptr,
            "main return call");
}

} // namespace

int main() {
    try {
        testIfWithoutElse();
        testIfElseWithBlocks();
        testDanglingElseBindsNearestIf();
        testNestedIfWithOuterElse();
        testWhileWithSingleStatement();
        testWhileWithBlockAndLoopControls();
        testBreakAndContinueOutsideLoopAreSyntacticallyAccepted();
        testControlFlowSourceRanges();
        testControlFlowRecovery();
        testMalformedControlFlowDiagnostics();
        testFullParserAcceptanceProgram();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
