package toyc.ir.block;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import toyc.common.Type;
import toyc.ir.IRVisitor;
import toyc.ir.value.Label;
import toyc.ir.value.LocalVar;

public final class Function {
    private final String name;
    private final Type returnType;
    private final List<LocalVar> parameters;
    private final List<BasicBlock> blocks = new ArrayList<>();
    private final BasicBlock entryBlock;
    private final Map<String, LocalVar> locals = new LinkedHashMap<>();

    public Function(String name, Type returnType, List<LocalVar> parameters, Label entryLabel) {
        this.name = name;
        this.returnType = returnType;
        this.parameters = List.copyOf(parameters);
        this.entryBlock = new BasicBlock(entryLabel);
        this.blocks.add(entryBlock);
        for (LocalVar parameter : parameters) {
            locals.put(parameter.name(), parameter);
        }
    }

    public String name() {
        return name;
    }

    public Type returnType() {
        return returnType;
    }

    public List<LocalVar> parameters() {
        return parameters;
    }

    public List<BasicBlock> blocks() {
        return Collections.unmodifiableList(blocks);
    }

    public BasicBlock entryBlock() {
        return entryBlock;
    }

    public Map<String, LocalVar> locals() {
        return Collections.unmodifiableMap(locals);
    }

    public void addBlock(BasicBlock block) {
        blocks.add(block);
    }

    public void replaceBlocks(List<BasicBlock> newBlocks) {
        if (newBlocks.isEmpty() || newBlocks.get(0) != entryBlock) {
            throw new IllegalArgumentException("function blocks must keep the entry block first");
        }
        blocks.clear();
        blocks.addAll(newBlocks);
    }

    public void addLocal(LocalVar local) {
        locals.put(local.name(), local);
    }

    public <R, C> R accept(IRVisitor<R, C> visitor, C context) {
        return visitor.visitFunction(this, context);
    }
}
