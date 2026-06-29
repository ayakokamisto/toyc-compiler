package toyc.opt;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.BinaryOp;
import toyc.ir.inst.Branch;
import toyc.ir.inst.Call;
import toyc.ir.inst.Compare;
import toyc.ir.inst.CondBranch;
import toyc.ir.inst.GlobalAddr;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Load;
import toyc.ir.inst.Move;
import toyc.ir.inst.Return;
import toyc.ir.inst.Store;
import toyc.ir.inst.UnaryOp;
import toyc.ir.value.GlobalVar;
import toyc.ir.value.IRValue;
import toyc.ir.value.Temp;

final class GlobalLoopStorePromotion {
    private GlobalLoopStorePromotion() {
    }

    static boolean promote(Function function) {
        ControlFlowGraph cfg = ControlFlowGraph.build(function);
        boolean changed = false;
        for (BasicBlock cond : function.blocks()) {
            Loop loop = findSimpleLoop(cond, cfg);
            if (loop != null) {
                changed |= promoteLoop(loop);
            }
        }
        return changed;
    }

    private static Loop findSimpleLoop(BasicBlock cond, ControlFlowGraph cfg) {
        if (!(cond.terminator() instanceof CondBranch branch)) {
            return null;
        }
        BasicBlock trueTarget = blockWithLabel(cfg.successors(cond), branch.trueTarget());
        BasicBlock falseTarget = blockWithLabel(cfg.successors(cond), branch.falseTarget());
        if (trueTarget == null || falseTarget == null) {
            return null;
        }
        BasicBlock body = branchesTo(trueTarget, cond) ? trueTarget : branchesTo(falseTarget, cond) ? falseTarget : null;
        BasicBlock exit = body == trueTarget ? falseTarget : trueTarget;
        if (body == null || cfg.predecessors(cond).size() != 2 || cfg.predecessors(exit).size() != 1) {
            return null;
        }
        BasicBlock preheader = null;
        for (BasicBlock predecessor : cfg.predecessors(cond)) {
            if (predecessor != body) {
                preheader = predecessor;
            }
        }
        if (preheader == null || !(preheader.terminator() instanceof Branch preBranch) || preBranch.target() != cond.label()) {
            return null;
        }
        return new Loop(preheader, cond, body, exit);
    }

    private static boolean promoteLoop(Loop loop) {
        for (Instruction instruction : loop.body().instructions()) {
            if (instruction instanceof Call || instruction instanceof Store store && !(store.address() instanceof Temp)) {
                return false;
            }
        }

        Set<Temp> candidateAddresses = java.util.Collections.newSetFromMap(new IdentityHashMap<>());
        for (Instruction preInstruction : loop.preheader().instructions()) {
            if (preInstruction instanceof GlobalAddr globalAddr) {
                candidateAddresses.add(globalAddr.result());
            }
        }
        for (Instruction preInstruction : loop.preheader().instructions()) {
            if (!(preInstruction instanceof GlobalAddr globalAddr)) {
                continue;
            }
            Temp address = globalAddr.result();
            Candidate candidate = findCandidate(loop.body(), address, candidateAddresses);
            if (candidate != null) {
                applyPromotion(loop, candidate, address, globalAddr.global());
                return true;
            }
        }
        return false;
    }

    private static Candidate findCandidate(BasicBlock body, Temp address, Set<Temp> candidateAddresses) {
        Load load = null;
        Store store = null;
        int loadIndex = -1;
        int storeIndex = -1;
        List<Instruction> instructions = body.instructions();
        for (int i = 0; i < instructions.size(); i++) {
            Instruction instruction = instructions.get(i);
            if (instruction instanceof Load currentLoad && currentLoad.address() == address) {
                if (load != null) {
                    return null;
                }
                load = currentLoad;
                loadIndex = i;
            } else if (instruction instanceof Store currentStore) {
                if (currentStore.address() == address) {
                    if (store != null) {
                        return null;
                    }
                    store = currentStore;
                    storeIndex = i;
                } else if (!candidateAddresses.contains(currentStore.address())) {
                    return null;
                }
            }
        }
        if (load == null || store == null || loadIndex >= storeIndex || !(load.result() instanceof Temp accumulator)) {
            return null;
        }
        return new Candidate(load, store, accumulator);
    }

    private static void applyPromotion(Loop loop, Candidate candidate, Temp address, GlobalVar global) {
        List<Instruction> preheader = new ArrayList<>(loop.preheader().instructions());
        preheader.add(new Load(candidate.accumulator(), address));
        loop.preheader().replaceBody(preheader);

        List<Instruction> body = new ArrayList<>();
        for (Instruction instruction : loop.body().instructions()) {
            if (instruction == candidate.load() || instruction == candidate.store()) {
                continue;
            }
            body.add(rewriteOperand(instruction, candidate.load().result(), candidate.accumulator()));
        }
        body.add(new Move(candidate.accumulator(), candidate.store().value()));
        loop.body().replaceBody(body);

        List<Instruction> exit = new ArrayList<>();
        Map<IRValue, IRValue> replacements = new IdentityHashMap<>();
        exit.add(new Store(candidate.accumulator(), address));
        for (Instruction instruction : loop.exit().instructions()) {
            if (instruction instanceof GlobalAddr globalAddr && globalAddr.global() == global) {
                replacements.put(globalAddr.result(), address);
                continue;
            }
            Instruction rewritten = rewriteOperands(instruction, replacements);
            if (rewritten instanceof Load load && load.address() == address) {
                replacements.put(load.result(), candidate.accumulator());
                continue;
            }
            exit.add(rewritten);
        }
        loop.exit().replaceBody(exit);
    }

    private static Instruction rewriteOperand(Instruction instruction, IRValue from, IRValue to) {
        if (instruction instanceof BinaryOp binary) {
            IRValue left = binary.left() == from ? to : binary.left();
            IRValue right = binary.right() == from ? to : binary.right();
            return new BinaryOp(binary.result(), binary.op(), left, right);
        }
        return instruction;
    }

    private static Instruction rewriteOperands(Instruction instruction, Map<IRValue, IRValue> replacements) {
        if (instruction instanceof BinaryOp binary) {
            return new BinaryOp(binary.result(), binary.op(), resolve(binary.left(), replacements), resolve(binary.right(), replacements));
        }
        if (instruction instanceof Compare compare) {
            return new Compare(compare.result(), compare.predicate(), resolve(compare.left(), replacements), resolve(compare.right(), replacements));
        }
        if (instruction instanceof UnaryOp unary) {
            return new UnaryOp(unary.result(), unary.op(), resolve(unary.value(), replacements));
        }
        if (instruction instanceof Move move) {
            return new Move(move.result(), resolve(move.value(), replacements));
        }
        if (instruction instanceof Load load) {
            return new Load(load.result(), resolve(load.address(), replacements));
        }
        if (instruction instanceof Store store) {
            return new Store(resolve(store.value(), replacements), resolve(store.address(), replacements));
        }
        if (instruction instanceof Return ret && ret.value() != null) {
            return new Return(resolve(ret.value(), replacements));
        }
        return instruction;
    }

    private static IRValue resolve(IRValue value, Map<IRValue, IRValue> replacements) {
        IRValue current = value;
        while (true) {
            IRValue next = replacements.get(current);
            if (next == null || next == current) {
                return current;
            }
            current = next;
        }
    }

    private static BasicBlock blockWithLabel(List<BasicBlock> blocks, toyc.ir.value.Label label) {
        for (BasicBlock block : blocks) {
            if (block.label() == label) {
                return block;
            }
        }
        return null;
    }

    private static boolean branchesTo(BasicBlock block, BasicBlock target) {
        return block.terminator() instanceof Branch branch && branch.target() == target.label();
    }

    private record Loop(BasicBlock preheader, BasicBlock cond, BasicBlock body, BasicBlock exit) {
    }

    private record Candidate(Load load, Store store, Temp accumulator) {
    }
}
