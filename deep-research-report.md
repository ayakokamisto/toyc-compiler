# ToyC 编译器从零重建的参考架构与实现路线

## 任务约束决定了这次重建应该追求什么

这个项目的外部契约非常清晰：编译器从 `stdin` 读入 ToyC 源码，向 `stdout` 输出 RISC-V32 汇编；评测系统会自动构建并在线测试；功能分和性能分共同计入总评，性能测试单点时限 20 秒，性能基准是 `gcc -O2` 生成代码的运行时间；编译器本身的编译时间限制宽松，且编译器执行效率本身不计分；性能测试时还会传入 `-opt` 参数。这个约束直接说明，你的设计目标应该是“生成代码质量优先、实现复杂度可控、优化收益稳定”，而不是把大量精力放在前端吞吐或构建速度上。 fileciteturn0file0

ToyC 语言本身也非常适合做“强中端、轻后端”的编译器：只有 `int`/`void`、语句块、`if/else`、`while`、`break`/`continue`、函数调用、全局变量和全局常量；没有数组、指针、I/O、多文件编译；逻辑与/或要求短路求值；变量和常量都要求先声明后使用；函数调用要求写在被调函数定义之后，支持函数内自递归；测试用例都保证语法正确、满足语义约束、且不包含未定义行为。这个子集意味着你完全可以把主要工程量集中在 SSA、CFG 规范化、全局优化、寄存器分配和 RV32 代码生成上。 fileciteturn0file0

这份任务说明还有两个实现层面的隐蔽坑。一个是 `NUMBER` 的正则写成了 `-?(0|[1-9][0-9]*)`，同时语法里又包含一元 `-`；为了让 `a-1`、`a - 1`、`(-1)` 的分词和解析都稳定，工程上更稳妥的办法是把负号留给语法层的一元表达式，只把非负十进制串当作词法数字。另一个是全局变量初始化：文法允许 `int x = Expr;` 出现在全局作用域，语义段只明确要求“常量初始化式必须是编译期可确定”，但任务又声明 ToyC 源文件可直接被 C 编译器编译并与等价 C 代码行为一致。为了同时兼容这两种表述，建议你的实现支持两条路径：编译期可求值的全局初始化直接进数据段，无法在编译期求值的全局初始化则降成程序启动时执行的隐藏初始化例程，并在 `main` 开始前调用。这样对隐藏测试最稳。 fileciteturn0file0

## 应该借鉴的是 LLVM 与 GCC 的核心思想

LLVM 值得借鉴的核心不是它那套庞大的目标无关代码生成器，而是几个非常稳定的中端原则。第一，值是 SSA 值，优化围绕显式的 def-use / use-def 关系展开。LLVM 的 `Value`/`User`/`Use` 体系把“谁用了我、我用了谁”做成了一等公民，`Value` 提供 use-list 迭代和 `replaceAllUsesWith` 这类批量重写接口，而 `User` 直接持有操作数列表；LLVM 明确说明，这种直接连接成立的原因就是 IR 采用了 SSA 形式。 citeturn11view2turn11view3

第二，变量提升到 SSA 不是可选增强，而是整个优化管线的起点。LLVM 的 `mem2reg` 会把只通过 `load/store` 使用的 `alloca` 提升成寄存器引用，具体做法是用 dominance frontier 放置 φ 节点，再沿深度优先顺序改写 load/store；官方文档直接指出，这就是标准 SSA construction algorithm，用来构造 “pruned SSA form”。GCC 的 Tree-SSA 也把大多数树优化建立在 SSA 之上，并明确引用了 Cytron 等人的 1991 年 SSA 论文作为实现依据。 citeturn4view4turn21view0

第三，CFG 规范化与 SSA 规范化是优化放大的前提。LLVM 的 `simplifycfg` 会移除无前驱块、合并唯一前驱/唯一后继的直线块、消除单前驱块上的 φ、删除只含无条件跳转的空块；`loop-simplify` 会给自然循环补 preheader、规范 exit block，并保证恰好一个 backedge；LICM 依赖这种循环规范形态，把循环不变量提升到 preheader。GCC 也把 dominator-based copy/constant propagation、expression simplification、jump threading、PHI optimization 作为 Tree-SSA 中反复执行的核心优化。 citeturn13view0turn27view0turn21view1

第四，LLVM 后端的复杂度并不适合课程项目整体照搬。LLVM 的目标无关代码生成分成 instruction selection、调度、machine SSA 优化、寄存器分配、prolog/epilog 插入、late machine optimization、code emission 等多个阶段；机器虚拟寄存器在寄存器分配前仍保持 SSA，随后进入 SSA deconstruction，把 PHI 变成拷贝。这个体系很强，但你的课程项目同时要求“禁止抄袭代码或套用 Clang、LLVM 等现成后端框架”，所以最有价值的参考方式是借用 SSA、中端 pass 组织方式、后端分层思想，而把实际 RISC-V32 后端写成手工、轻量、可验证的版本。 citeturn19view0turn20view0turn20view2 fileciteturn0file0

## 最值得采用的总体架构

我建议你把项目重建成“五层流水线”：

```text
ToyC Source
  → Lexer / Parser / AST
  → 语义分析与常量求值
  → Canonical Slot IR
  → Mem2Reg
  → Optimizing SSA IR
  → Out-of-SSA + RV32 MIR
  → 寄存器分配 / 栈帧生成
  → RISC-V32 Assembly
```

这条路线最接近 Clang/LLVM 的“前端先产出易构造 IR，再通过 Mem2Reg 进入优化友好的 SSA IR”的思路，同时比“直接从 AST 生成 RV32 汇编”更容易做全局优化，也比“直接从 AST 生成 SSA”更容易保证作用域、赋值、短路求值和全局初始化的正确性。LLVM `mem2reg` 的文档本身就说明了：先把局部可提升对象建成内存形式，再统一进入 pruned SSA，是一条成熟且高收益的路径。 citeturn4view4

其中最关键的设计点是把“源码变量名”和“SSA 值编号”彻底分层。你当前 IR 的问题之一是把 `%x` 这类名字同时当成源级变量与 IR vreg 的身份标识，导致跨块多重定义、copy 赋值、缺失 def-use 链等问题一起出现。重建后，前端保留 `SymbolId` 处理作用域与遮蔽，Slot IR 用 `SlotId` 表示“可写存储对象”，SSA IR 用 `ValueId(uint32_t)` 表示“唯一赋值的 SSA 值”；人类可读名字只用于 dump 与调试打印。这样一来，支配树、φ、GVN、DCE、LICM、寄存器分配都会自然落位，字符串比较和哈希开销也会大幅下降。这个建议直接对应 LLVM 把 `Value` 和 use-list 做成核心对象的设计。 citeturn11view2turn11view3

推荐的 Slot IR 可以非常小，只需要显式表达“局部 slot / 全局符号 / 纯算术 / 控制流 / 调用 / 返回”即可：

```text
Module
  GlobalObject(name, kind=var|const, init)
  Function
    BasicBlock
      Inst:
        slot.alloc / slot.load / slot.store
        global.load / global.store
        const / copy / unary / binary / cmp / call
      Terminator:
        br / cbr / ret
```

ToyC 没有指针与地址传递，所以局部变量和形参几乎都能经过 `mem2reg` 完整提升；全局变量保留成显式内存对象即可。全局常量优先做编译期替换，其次落到只读数据段。这个结构既保留了 Clang 风格的“先有 memory form，再进入 SSA”，又把内存复杂度压到 ToyC 子集真正需要的范围内。 citeturn4view4 fileciteturn0file0

表达式构建器建议从一开始就提供两套入口：`emitValue(expr)` 和 `emitCond(expr, trueBB, falseBB)`。这是处理 ToyC 短路语义的关键。因为 `&&` 和 `||` 必须短路，条件上下文最自然的 lowering 方式就是直接生成 CFG；在值上下文里，再通过分支和 φ 物化出 0/1 整数结果。LLVM 的 Kaleidoscope 教程在 `if/then/else` 和循环章节中演示了这种基于控制流与 PHI 的构建方式，并特别强调 PHI 需要按前驱块建立 block/value 对。这个套路在 ToyC 上非常适合。 citeturn17view2turn17view3 fileciteturn0file0

## SSA、Def-Use 与核心优化的实现顺序

重建阶段最先要补齐的是 CFG 分析基础设施的“最后三块拼图”：`idom`、支配树孩子表、dominance frontier。你已经有 `buildCFG()`、支配关系求解、自然循环检测和 preheader 创建，这意味着你离标准的 Mem2Reg 只差最后一层结构化信息。GCC 文档明确把“进入 SSA 形式”放在大多数树优化之前，LLVM 的 `mem2reg` 也明确依赖 dominator frontier 与 DFS rewrite。你现在最合适的顺序是：先收敛 CFG 与 `idom`/DF，再做 Phi 插入与 rename。 citeturn21view1turn4view4

SSA IR 的块内布局应该固定成 `[Phi*][Inst*]Terminator`。LLVM 的 verifier 规则要求 PHI 必须位于基本块最前面、连续分组出现，而且每个 PHI 必须为当前块的每个前驱提供一项输入，不能多也不能少。把这些规则内建成你自己的 IR verifier，会极大降低后续优化 debug 成本。你现在这种“Instruction 与 Terminator 混合、CopyInst 表示赋值”的风格，可以直接升级成 `PhiInst` + 真正的单定义值模型。 citeturn13view0turn4view1

Def-Use 链的实现建议完全仿照 LLVM 思路：每个 `Instruction` 同时是 `Value` 与 `User`；每个 operand 是一个 `Use { Value* def; User* user; uint16_t opIndex; }`；`Value` 维护 intrusive 或 vector-based 的 use-list；所有替换走统一的 `replaceAllUsesWith` 风格接口。这样做以后，CopyProp、CSE、DCE 的很多复杂度会直接改变。当前 DCE 之所以退化成全量扫描，是因为“死”要通过全图搜索得知；有了 use-count 与 use-list 以后，DCE 可以变成标准 worklist：当一个纯指令的 use-list 变空，就删除它，并递减其操作数的 use-count，继续向前传播。LLVM 文档把 def-use 迭代和 `replaceAllUsesWith` 都当成最基础的 IR 变换接口，这正是你需要补上的核心能力。 citeturn11view2turn11view3

你的优化管线里，最先落地的高收益组合应该是这五个 pass：`InstCombine-lite`、`SimplifyCFG/CFS`、`GVN`、`DCE`、`LoopSimplify + LICM`。LLVM 文档说明 `instcombine` 是 worklist-driven 的代数与规范化化简；`gvn` 消除 fully / partially redundant instructions，并做 redundant load elimination；`simplifycfg` 负责死块删除、块合并、PHI 简化；`licm` 负责把循环不变量提升到 preheader，`loop-simplify` 则保证 LICM 所需的 preheader、exit block 和单一 backedge。你期望的 “CFS + GVN + 固定点迭代” 与这套组合几乎同构。 citeturn12view1turn12view0turn13view0turn27view0

对 ToyC 来说，推荐的 pass 顺序是：

- `Mem2Reg`
- `InstCombine-lite`
- `SCCP/CFS`
- `SimplifyCFG`
- `GVN`
- `DCE`
- `LoopSimplify`
- `LICM`
- `SimplifyCFG`
- `DCE`

然后在 `-opt` 模式下迭代到不动点，最多 10 轮。课程说明已经说明编译器本身的执行效率不影响评分，所以这种“多轮固定点”在这个项目里是合算的；LLVM 的 `sccp` 会把条件分支证明成无条件，从而制造新死代码，文档也明确建议在之后再跑 DCE；GCC 也把 DCE、dominator optimization、PHI optimization 设计成优化过程中多次穿插执行的 pass。 fileciteturn0file0 citeturn13view1turn21view1

实现 GVN 时，我建议你做一个“工程上足够强、语义上非常稳”的版本：以支配树为遍历顺序，在每个作用域维护表达式哈希表，key 由 `opcode + type + canonicalized operands + predicate + side-effect class` 组成；对交换律运算做 operand 排序；对全局 load 只在“同一全局符号、支配路径上无该符号 store、且中间无可能写该符号的 call”时做冗余消除；PHI 只做两类简化：所有输入相同直接折叠、单前驱块上的 PHI 由 `SimplifyCFG` 清掉。这样能吃到大部分收益，又能规避早期 GVN 最容易踩的循环与等价类收敛问题。LLVM 的 `gvn` 负责全局冗余消除，GCC 的 dominator optimization则负责跨块 copy propagation / constant propagation / simplification，你的版本做这两者的“中间态”就已经很有价值。 citeturn12view0turn21view1

LICM 可以比你现在的启发式版本精确得多。因为经过 `Mem2Reg` 之后，绝大多数局部标量已经不在内存里，循环中剩下的真正副作用主要是全局读写和调用。再加上 ToyC 没有指针与数组，你对全局符号的 alias 关系几乎可以做到“按名字精确区分”。配合轻量的函数摘要 `readsGlobals / writesGlobals / callsImpure`，就能安全地把许多循环内的常量、只读全局加载甚至纯函数调用提出循环。LLVM 官方文档明确说明 LICM 会把不变量提升到 preheader，并依赖 loop-simplify 形成的 preheader 和 exit-block 性质。 citeturn27view2turn27view0turn28view0 fileciteturn0file0

## RISC-V32 后端应该怎样做才稳而快

ABI 选择上，最稳妥的目标是 RV32 的 ILP32 整数调用约定。RISC-V psABI 明确给出：基础整数调用约定有 8 个参数寄存器 `a0-a7`，其中 `a0-a1` 也用于返回值；`ra` 是返回地址寄存器，`sp` 是栈指针；`s0-s11` 是跨调用保留寄存器；栈向低地址增长，并且过程入口时栈指针要求 128-bit 对齐，过程执行期间也应保持对齐。ToyC 只有 `int` 与 `void`，所以完全不需要引入浮点 ABI。 citeturn5view0turn5view1turn5view2turn5view3

ISA 子集上，我建议把“可提交版本”的基线设成 RV32I 兼容，再可选启用 `M` 扩展优化。理由很简单：官方 ISA 文档把 RV32I 基础整数指令集与 `M` 扩展分开列出，课程任务只写了 “RISC-V32 汇编”，并没有承诺评测环境一定带乘除扩展。于是最稳妥的工程策略是：`+ -`、比较、位逻辑、移位全部用 RV32I；`* / %` 分三类处理，常量幂次尽量在中端化简成移位与掩码，可确定为小常量的乘法可做 strength reduction，其余退化为软件 helper 或手写例程；如果你后面确认评测环境支持 `M`，再把 helper 替换成原生 `mul/div/rem`。这样既不赌环境，又保留提速空间。 citeturn14view0turn12view1

后端分层上，ToyC 项目最划算的做法是“一层优化 SSA IR + 一层非 SSA 的 RV32 MIR”。LLVM 在机器层继续维持 SSA，直到寄存器分配前再做 SSA deconstruction；你可以借鉴它的原则，但不必把机器 SSA 也完整复刻。更合适的流程是：优化完成后，先把 IR PHI 降成 predecessor edge 上的 parallel copies；对 critical edge 先切边；随后进入 RV32 MIR，做 liveness、copy coalescing、寄存器分配、spill、帧生成。LLVM 文档明确说明传统 ISA 不直接实现 PHI，因此最终必须把 PHI 替换为保持语义的拷贝；`break-crit-edges` 也是很多 pass 的前置规范化步骤。 citeturn20view2turn28view0

寄存器分配上，我建议你采用“保守但效果好的 greedy allocator”。一个非常实用的可分配集合是：固定保留 `x0/ra/sp/gp/tp/s0`，把 `s1-s11` 与 `t0-t6` 作为主分配池，共 18 个通用整数寄存器；`a0-a7` 主要留给参数传递、返回值和少量临时指令选择。这样做的好处是跨调用活跃值天然偏向 `s` 寄存器，调用序列生成简单；局部短命值则多用 `t` 寄存器。spill cost 乘以 loop depth 权重，通常就足够把热点循环的关键值留在寄存器里。配合你前面的 GVN、LICM、InstCombine-lite，代码质量会明显高于“所有局部都常驻栈槽”的方案。寄存器角色与保存规则都直接来自 psABI。 citeturn4view10turn4view11

控制流与块布局上，可以顺手借助 RISC-V 的分支特性做一些免费的质量优化。RISC-V 条件分支直接比较两个寄存器，支持 `BEQ/BNE/BLT/BLTU/BGE/BGEU`，分支目标范围是 ±4 KiB；无条件跳转推荐使用 `JAL x0, label` 这种 jump，而不是构造一个恒真的条件分支。ISA 手册还建议让顺序执行路径成为更常见路径，并假设后向分支偏向 taken、前向分支偏向 not taken。对 ToyC 来说，这意味着 while 循环应优先布局成“条件检查 → 循环体 → 回跳 header → 退出块前向跳出”，`if` 则尽量让更常见的路径成为 fallthrough。这个优化实现成本很低，运行时收益很实在。 citeturn15view0turn15view1turn15view3

## 在拿不到隐藏测试时，验证体系应该怎样搭

你其实已经拥有一个非常强的参考 oracle：任务说明明确说 ToyC 源文件可以直接被 C 编译器编译成可执行文件，其运行结果与等价 C 代码一致；评测又只看 `main` 的退出码。基于这个契约，最有价值的自测方式就是 differential harness：把同一份合法 ToyC 程序分别交给 `gcc` 和你的编译器，运行后比较退出码，并把 `-opt` 与非 `-opt` 都纳入测试矩阵。因为没有 I/O，这个 harness 会非常简洁，却能覆盖绝大多数功能与优化正确性问题。 fileciteturn0file0

隐藏测试环境下，fuzz 的方向也应该围绕语言子集定制。最值得自动生成的程序类型有四类：大量嵌套作用域与遮蔽、短路逻辑与副作用混合、深循环与 `break/continue` 组合、跨函数递归和全局变量读写。生成器只需要保证“语义合法、无 UB、返回值在 0~255”，就能直接送进 differential harness。再加上一套 reducer，把失败样例收缩成最小程序，调试效率会非常高。课程说明已经保证在线测试只提供合法程序，这让你可以把随机程序生成器的约束写得更激进、更贴近评测输入分布。 fileciteturn0file0

除了外部差分，内部 verifier 也要尽早上线。最少要有四类校验：CFG 完整性、SSA 完整性、类型一致性、后端栈帧与寄存器约定一致性。SSA verifier 至少检查三件事：单一 `ValueId` 仅定义一次；PHI 位于块开头并成组；PHI 的 incoming 数量与前驱数完全一致。LLVM 把这些都放在 verifier 规则里，这是你最应该早做的“自救工程”。很多优化 bug 在 verifier 层就能截断，而不会一直拖到汇编运行期才暴露。 citeturn13view0

优化正确性测试再加一层“自同构”约束会更稳：同一程序在 `-O0`、`-opt`、`-opt` 多轮固定点之间退出码一致；在“关闭 GVN / 关闭 LICM / 关闭 RA coalescing”的变体间退出码一致；在块布局重排前后退出码一致。你的很多后端 bug，尤其是 PHI lowering、critical-edge copy 插入、寄存器保存恢复，都会在这类测试里被很快抓住。这个方法不依赖最终评测数据，完全符合课程给出的执行接口与结果判定方式。 fileciteturn0file0

## 从零重建时，最划算的落地顺序

最优先交付物是“语义正确、无优化、栈版直译后端”的可运行编译器。它只需要把 ToyC 正确翻译成 RV32 汇编，遵守 `stdin`/`stdout` 接口、`main` 入口、调用约定与全局对象生成规则。这个版本的价值在于它给后续所有中端和后端重构提供一个稳定语义基线，也为 differential harness 提供对照面。课程评分首先看功能通过率，这个基线版本越早稳定，后面的优化越敢做。 fileciteturn0file0

第二阶段直接进入“Canonical Slot IR + Mem2Reg + SSA verifier”。这是整次重建的分水岭。只要这一步成型，你当前列出的 CopyProp 跨块不安全、DCE 全量扫描、LICM 启发式不可靠、GVN 难以落地等问题都会同时开始收敛，因为整张图已经从“变量名字流”转变成“SSA 值流”。LLVM 与 GCC 都把这个阶段放在大多数优化之前，你也应该这样做。 citeturn4view4turn21view1

第三阶段落地 `InstCombine-lite + CFS/SimplifyCFG + DCE`。这是最容易看到收益的一组 pass：常量表达式会被压缩，短路与分支会被折叠，死块和单前驱 PHI 会消失，IR 尺寸和 CFG 复杂度明显下降。LLVM 的 `instcombine`、`sccp`、`simplifycfg`、`dce` 正是围绕这个目标设计的，这一组完成后，很多本来要靠复杂后端弥补的问题会直接在中端被消掉。 citeturn12view1turn13view1turn13view0turn28view0

第四阶段落地 `GVN + LoopSimplify + LICM`。这是“高性能”真正开始出现的地方。ToyC 没有数组和指针，循环中的纯算术和只读全局访问非常适合被 GVN 与 LICM 利用；而 `loop-simplify` 提供的 preheader 与单 backedge 又正好解决你现有 LICM 风险高的问题。做到这一步，你生成代码的热点路径通常已经会出现比较明显的提速。 citeturn12view0turn27view0turn27view2

第五阶段做 “Out-of-SSA + RA + copy coalescing + 帧优化”。LLVM 的机器层做得更复杂，但你在课程项目里做到“critical edge splitting、parallel copy lowering、greedy allocation、callee-saved 只按需保存、leaf function 省略不必要保存”就已经足够有竞争力。psABI 对寄存器角色和栈对齐给出了明确规范，照着执行即可。 citeturn20view2turn28view0turn5view1turn5view3

最值得坚持的总路线是：**用 LLVM/GCC 的 SSA 中端思想重建你的核心 IR，用 ToyC 子集的简单语义把 alias、call side-effect、global access 精确化，再用一个克制而扎实的 RV32 后端把这些优化真正落到汇编上。** 这条路线既符合课程限制，也最有机会把“功能全达标但是性能很糟糕”的状态改造成“功能稳、优化强、后端可靠”的高质量 ToyC 编译器。