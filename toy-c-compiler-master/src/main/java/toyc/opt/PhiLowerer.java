package toyc.opt;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import toyc.ir.block.BasicBlock;
import toyc.ir.block.Function;
import toyc.ir.inst.Instruction;
import toyc.ir.inst.Move;
import toyc.ir.inst.Phi;
import toyc.ir.value.Label;

final class PhiLowerer {
    private PhiLowerer() {
    }

    static boolean lower(Function function) {
        Map<Label, BasicBlock> blockByLabel = new IdentityHashMap<>();
        for (BasicBlock block : function.blocks()) {
            blockByLabel.put(block.label(), block);
        }

        boolean changed = false;
        for (BasicBlock block : function.blocks()) {
            if (block.phis().isEmpty()) {
                continue;
            }
            for (Phi phi : block.phis()) {
                for (Phi.Incoming incoming : phi.incoming()) {
                    BasicBlock predecessor = blockByLabel.get(incoming.predecessor());
                    if (predecessor == null) {
                        throw new IllegalStateException("phi predecessor does not exist: " + incoming.predecessor().name());
                    }
                    List<Instruction> body = new ArrayList<>(predecessor.instructions());
                    body.add(new Move(phi.result(), incoming.value()));
                    predecessor.replaceBody(body);
                }
            }
            block.clearPhis();
            changed = true;
        }
        return changed;
    }
}
