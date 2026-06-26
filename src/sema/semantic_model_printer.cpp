/// SemanticModel printer — --dump-sema output.

#include "toyc/frontend/ast.h"
#include "toyc/sema/semantic_model.h"

#include <ostream>

namespace toyc {

static const char* symbolKindName(SymbolKind k) {
  switch (k) {
    case SymbolKind::GlobalVariable: return "GlobalVar";
    case SymbolKind::GlobalConstant: return "GlobalConst";
    case SymbolKind::LocalVariable:  return "LocalVar";
    case SymbolKind::LocalConstant:  return "LocalConst";
    case SymbolKind::Parameter:      return "Param";
    case SymbolKind::Function:       return "Function";
  }
  return "Unknown";
}

// typeKindName is declared in ast.h

void dumpSema(const SemanticModel& model, const CompUnit& unit,
              std::ostream& out) {
  out << "SemanticModel\n";

  // Print global symbols in source order.
  out << "  GlobalSymbols\n";
  for (const auto& item : unit.items()) {
    if (item->kind() == ASTKind::GlobalDecl) {
      const auto& gl = static_cast<const GlobalDecl&>(*item);
      const Decl* d = gl.declaration();
      if (!d) continue;

      auto symId = model.resolvedSymbol(*d);
      if (!symId.has_value()) continue;
      const auto& sym = model.symbol(symId.value());

      out << "    " << symbolKindName(sym.kind)
          << " name=" << sym.name
          << " type=" << typeKindName(sym.type);

      if (sym.isConstant() && sym.constantValue.has_value()) {
        out << " value=" << sym.constantValue.value();
      }

      auto gii = model.globalInitInfo(*d);
      if (gii.has_value()) {
        out << " init="
            << (gii->kind == GlobalInitKind::StaticConstant ? "static" : "runtime");
      }

      out << "\n";
    } else if (item->kind() == ASTKind::FuncDef) {
      const auto& func = static_cast<const FuncDef&>(*item);
      auto symId = model.resolvedSymbol(func);
      if (!symId.has_value()) continue;
      const auto& sym = model.symbol(symId.value());

      out << "    Function name=" << sym.name
          << " return=" << typeKindName(sym.type)
          << " params=(";
      for (std::size_t i = 0; i < sym.parameterNames.size(); ++i) {
        if (i > 0) out << ", ";
        out << typeKindName(sym.parameterTypes[i])
            << " " << sym.parameterNames[i];
      }
      out << ")\n";
    }
  }
}

} // namespace toyc
