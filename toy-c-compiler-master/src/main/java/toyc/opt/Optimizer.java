package toyc.opt;

import toyc.ir.IRProgram;

public final class Optimizer {
    private Optimizer() {
    }

    public static IRProgram optimize(IRProgram program) {
        for (var function : program.module().functions()) {
            ControlFlowSimplifier.simplify(function);
            Mem2Reg.promote(function);
        }
        SmallFunctionInliner.inline(program.module());
        FunctionEffects effects = FunctionEffects.analyze(program.module());
        for (var function : program.module().functions()) {
            boolean changed;
            PhiLowerer.lower(function);
            TailRecursionEliminator.eliminate(function);
            GlobalScalarReplacement.replace(function, effects);
            do {
                changed = false;
                changed |= LocalValueOptimizer.optimize(function);
                changed |= DeadCodeEliminator.eliminate(function, effects);
                changed |= LinearExpressionOptimizer.optimize(function);
                changed |= LocalCse.eliminate(function);
                changed |= LoopInvariantCodeMotion.hoist(function);
                changed |= GlobalLoopStorePromotion.promote(function);
                changed |= ControlFlowSimplifier.simplify(function);
                changed |= DeadStoreEliminator.eliminate(function);
                changed |= DeadGlobalStoreEliminator.eliminate(program.module());
            } while (changed);
        }
        DeadFunctionEliminator.eliminate(program.module());
        return program;
    }
}
