#include <brb.h>
#include <errno.h>
#include <math.h>

defArray(Op);

#define STACK_ALIGNMENT 16
#define DEFAULT_ENTRY_NAME ".entry"

char* conditionNames_arm64[] = {
	[COND_NON] = "",
	[COND_EQU] = "eq",
	[COND_NEQ] = "ne",
	[COND_LTU] = "lo",
	[COND_GTU] = "hs",
	[COND_LEU] = "ls",
	[COND_GEU] = "hi",
	[COND_LTS] = "lt",
	[COND_GTS] = "gt",
	[COND_LES] = "le",
	[COND_GES] = "ge"
};
#define oppositeConditionName(cond_id) conditionNames_arm64[opposite_conditions[cond_id]]

static const char* regNames64[N_REGS] = {
	[0] = "x0",
	[1] = "x1",
	[2] = "x2",
	[3] = "x3",
	[4] = "x4",
	[5] = "x5",
	[6] = "x6",
	[7] = "x7",
	[ZEROREG_ID] = "xzr"
};

static const char* regNames32[N_REGS] = {
	[0] = "w0",
	[1] = "w1",
	[2] = "w2",
	[3] = "w3",
	[4] = "w4",
	[5] = "w5",
	[6] = "w6",
	[7] = "w7",
	[ZEROREG_ID] = "wzr"
};

static int cond_counter = 0;
void compileCondition(FILE* dst, uint8_t cond_id, uint8_t n_ops)
{
	if (cond_id == COND_NON) return;
	assert(cond_id < N_CONDS, "invalid condition ID was provided");
	fprintf(dst, "\tb%s . + %d\n", oppositeConditionName(cond_id), (n_ops + 1) * 4);
}

int startConditionalOp(FILE* dst, uint8_t cond_id)
{
	assert(cond_id < N_CONDS, "invalid condition ID was provided");
	if (!cond_id) return -1;
	fprintf(dst, "\tb%s .co_%d\n", oppositeConditionName(cond_id), cond_counter++);
	return cond_counter;
}

void endConditionalOp(FILE* dst, int cond_ctx)
{
	if (cond_ctx < 0) return;
	fprintf(dst, ".co_%d:\n", cond_ctx);
}

static char literal_buf[32];

static int bitOffset(int64_t value) {
	for (int i = 0; i < 64; ++i) {
		if (value & (1 << i)) return i;
	}
	return 64;
}

#define INTL_REG_ONLY 0x1
#define INTL_ADDR_OFFSET 0x2
#define INTL_REG_ARG 0x4

const char* intLiteral(FILE* dst, int8_t reg_id, int64_t value, int flag)
{
	if (flag == INTL_ADDR_OFFSET && inRange(value, -256, 256) ? true : !flag && inRange(value, -4096, 4096)) {
		snprintf(literal_buf, 32, "%lld", value);
		return literal_buf;
	}

	bool inverted = false;
	if (value < 0) {
		value *= -1;
		inverted = true;
	}

	if (value == 0 && flag != INTL_REG_ONLY) return "xzr";

	if (value < 65536) {
		fprintf(dst, "\tmov x%hhd, %lld\n", reg_id, inverted ? value *= -1 : value);
		inverted = false;
	} else if (!(value >> 32)) {
		fprintf(
			dst,
			"\tmov x%1$hhd, %2$lld\n"
			"\tmovk x%1$hhd, %3$lld, lsl 16\n",
			reg_id, value & 0xFFFF, value >> 16
		);
	} else if (!(value >> 48)) {
		fprintf(
			dst,
			"\tmov x%1$hhd, %2$lld\n"
			"\tmovk x%1$hhd, %3$lld, lsl 16\n"
			"\tmovk x%1$hhd, %4$lld, lsl 32\n",
			reg_id, value & 0xFFFF, (value >> 16) & 0xFFFF, value >> 32
		);
	} else {
		fprintf(
			dst,
			"\tmov x%1$hhd, %2$lld\n"
			"\tmovk x%1$hhd, %3$lld, lsl 16\n"
			"\tmovk x%1$hhd, %4$lld, lsl 32\n"
			"\tmovk x%1$hhd, %5$lld, lsl 48\n",
			reg_id, value & 0xFFFF, (value >> 16) & 0xFFFF, (value >> 32) & 0xFFFF, value >> 48
		);
	}
	if (inverted) fprintf(dst, "\tmvn x%1$hhd, x%1$hhd\n", reg_id);
	snprintf(literal_buf, 32, "x%hhd", reg_id);
	return literal_buf;
}

typedef struct comp_ctx {
	FILE* dst;
	int cur_frame_size;
	char* src_path;
	int src_line;
} CompCtx;

uint64_t nativeStackOffset(CompCtx* ctx, uint64_t offset)
{
	return alignby(ctx->cur_frame_size, STACK_ALIGNMENT) - ctx->cur_frame_size + offset;
}

typedef void (*OpNativeCompiler) (Module*, int, CompCtx*);

void compileSysNoneNative(Module* module, int index, CompCtx* ctx) {}

void compileSysExitNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 2);
	fprintf(ctx->dst, "\tmov x16, 1\n\tsvc 0\n");
}

void compileSysWriteNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 5);
	fprintf(ctx->dst,
		"\tmov x16, 4\n"
		"\tsvc 0\n"
		"\tbcc . + 12\n"
		"\tmov x26, x0\n"
		"\tmvn x0, xzr\n"
	);
}

void compileSysArgcNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 1);
	fprintf(ctx->dst, "\tmov x0, x28\n");
}

void compileSysArgvNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 3);
	fprintf(ctx->dst,
		"\tmadd x0, x0, 8, x27\n"
		"\tldr x0, x0\n"
	);
}

void compileSysReadNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 5);
	fprintf(ctx->dst,
		"\tmov x16, 3\n"
		"\tsvc 0\n"
		"\tbcc . + 12\n"
		"\tmov x26, x0\n"
		"\tmvn x0, xzr\n"
	);
}

void compileSysGetErrnoNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 1);
	fprintf(ctx->dst, "\tmov x0, x26\n");
}

void compileSysSetErrnoNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 1);
	fprintf(ctx->dst, "\tmov x26, x0\n");
}

OpNativeCompiler native_syscall_compilers[] = {
	[SYS_OP_INVALID] = &compileSysNoneNative,
	[SYS_OP_EXIT] = &compileSysExitNative,
	[SYS_OP_WRITE] = &compileSysWriteNative,
	[SYS_OP_ARGC] = &compileSysArgcNative,
	[SYS_OP_ARGV] = &compileSysArgvNative,
	[SYS_OP_READ] = &compileSysReadNative,
	[SYS_OP_GET_ERRNO] = &compileSysGetErrnoNative,
	[SYS_OP_SET_ERRNO] = &compileSysSetErrnoNative
};
static_assert(
	N_SYS_OPS == sizeof(native_syscall_compilers) / sizeof(native_syscall_compilers[0]),
	"not all syscalls have matching native compilers"
);

void compileNopNative(Module* module, int index, CompCtx* ctx)
{
	fprintf(ctx->dst, "\tnop\n");
}

void compileOpEndNative(Module* module, int index, CompCtx* ctx)
{
	if (index == module->seg_exec.length - 1) return;
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 3);
	fprintf(
		ctx->dst,
		"\tmov x16, 1\n"
		"\tmov x0, 0\n"
		"\tsvc 0\n"
	);
}

void compileOpMarkNative(Module* module, int index, CompCtx* ctx)
{
	fprintf(ctx->dst, ".m%d:\n", index);
}

void compileOpSetNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	intLiteral(ctx->dst, op.dst_reg, op.value, INTL_REG_ONLY);
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpSetrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tmov %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpSetdNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	DataBlock* block = module->seg_data.data + op.symbol_id;
	compileCondition(ctx->dst, op.cond_id, 2);
	if (block->is_mutable) {
		fprintf(
			ctx->dst,
			"\tadrp %1$s, \"%2$s::%3$s\"@PAGE\n"
			"\tadd %1$s, %1$s, \"%2$s::%3$s\"@PAGEOFF\n",
			regNames64[op.dst_reg], getDataBlockSubmodule(module, block)->name, block->name
		   );
	} else {
		fprintf(
			ctx->dst,
			"\tadr %s, \"%s::%s\"\n",
			regNames64[op.dst_reg], getDataBlockSubmodule(module, block)->name, block->name
		   );
	}
}

void compileOpSetbNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	intLiteral(
		ctx->dst,
		op.dst_reg,
		builtins[op.symbol_id].value,
		INTL_REG_ONLY
	);
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpAddNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	fprintf(ctx->dst, "\tadd %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, 0));
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpAddrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(
		ctx->dst,
		"\tadd %s, %s, %s\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		regNames64[op.src2_reg]
	);
}

void compileOpSubNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	fprintf(ctx->dst, "\tsub %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, 0));
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpSubrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(
		ctx->dst,
		"\tsub %s, %s, %s\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		regNames64[op.src2_reg]
	);
}

void compileOpSyscallNative(Module* module, int index, CompCtx* ctx)
{
	native_syscall_compilers[module->seg_exec.data[index].symbol_id](module, index, ctx);
}

void compileOpGotoNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	fprintf(ctx->dst, "\tb%s .m%lld\n", conditionNames_arm64[op.cond_id], index + op.op_offset);
}

void compileOpCmpNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	if (op.cond_id) {
		int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
		fprintf(
			ctx->dst,
			"\tccmp %s, %s, %s\n",
			regNames64[op.src_reg],
			intLiteral(ctx->dst, 8, op.value, 0),
			conditionNames_arm64[op.cond_id]
		);
		endConditionalOp(ctx->dst, cond_ctx);
	} else {
		fprintf(ctx->dst, "\tcmp %s, %s\n", regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, 0));
	}
}

void compileOpCmprNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	if (op.cond_id) {
		fprintf(
			ctx->dst,
			"\tccmp %s, %s, %s\n",
			regNames64[op.src_reg],
			regNames64[op.src2_reg],
			conditionNames_arm64[op.cond_id]
		);
	} else {
		fprintf(ctx->dst, "\tcmp %s, %s\n", regNames64[op.src_reg], regNames64[op.src2_reg]);
	}
}

void compileOpAndNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	fprintf(ctx->dst, "\tand %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, 0));
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpAndrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(
		ctx->dst,
		"\tand %s, %s, %s\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		regNames64[op.src2_reg]
	);
}

void compileOpOrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	fprintf(ctx->dst, "\torr %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, 0));
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpOrrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(
		ctx->dst,
		"\torr %s, %s, %s\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		regNames64[op.src2_reg]
	);
}

void compileOpNotNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tmvn %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpXorNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	fprintf(ctx->dst, "\teor %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, 0));
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpXorrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(
		ctx->dst,
		"\teor %s, %s, %s\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		regNames64[op.src2_reg]
	);
}

void compileOpShlNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst,
		"\tlsl %s, %s, %llu\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		op.value
	);
}

void compileOpShlrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tlsl %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]);
}

void compileOpShrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst,
		"\tlsr %s, %s, %llu\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		op.value
	);
}

void compileOpShrrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tlsr %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]);
}

void compileOpShrsNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst,
		"\tasr %s, %s, %llu\n",
		regNames64[op.dst_reg],
		regNames64[op.src_reg],
		op.value
	);
}

void compileOpShrsrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tasr %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]);
}

void compileOpProcNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	fprintf(
		ctx->dst,
		"\"%s::%s\":\n"
		"\tstp fp, lr, [sp, -16]!\n"
		"\tmov fp, sp\n",
		getOpSubmodule(module, module->seg_exec.data + index)->name, op.mark_name
	);
}

void compileOpCallNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	Op* proc = module->seg_exec.data + op.symbol_id;
	compileCondition(ctx->dst, op.cond_id, 1);
	if (proc->type == OP_PROC) {
		fprintf(ctx->dst, "\tbl \"%s::%s\"\n", getOpSubmodule(module, proc)->name, proc->mark_name);
	} else {
		fprintf(ctx->dst, "\tbl \"%s\"\n", proc->mark_name);
	}
}

void compileOpRetNative(Module* module, int index, CompCtx* ctx)
{
	compileCondition(ctx->dst, module->seg_exec.data[index].cond_id, 3);
	fprintf(ctx->dst,
		"\tmov sp, fp\n"
		"\tldp fp, lr, [sp], 16\n"
		"\tret\n"
	);
}

void compileOpEndprocNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	ctx->cur_frame_size = 0;
	if (module->seg_exec.data[index - 1].type == OP_RET) return;
	fputs(
		"\tmov sp, fp\n"
		"\tldp fp, lr, [sp], 16\n"
		"\tret\n",
		ctx->dst
	);
}

void compileOpLd64Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldr %s, [%s]\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpStr64Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tstr %s, [%s]\n", regNames64[op.src_reg], regNames64[op.dst_reg]);
}

void compileOpLd32Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldr %s, [%s]\n", regNames32[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpStr32Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tstr %s, [%s]\n", regNames32[op.src_reg], regNames64[op.dst_reg]);
}

void compileOpLd16Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldrh %s, [%s]\n", regNames32[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpStr16Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tstrh %s, [%s]\n", regNames32[op.src_reg], regNames64[op.dst_reg]);
}

void compileOpLd8Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldrb %s, [%s]\n", regNames32[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpStr8Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tstrb %s, [%s]\n", regNames32[op.src_reg], regNames64[op.dst_reg]);
}

void compileOpVarNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	uint64_t offset = alignby(ctx->cur_frame_size, STACK_ALIGNMENT);
	ctx->cur_frame_size += op.new_var_size;
	offset = alignby(ctx->cur_frame_size, STACK_ALIGNMENT) - offset;

	if (offset) fprintf(ctx->dst, "\tsub sp, sp, %s\n", intLiteral(ctx->dst, 8, offset, 0));
}

void compileOpSetvNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tsub %s, fp, %d\n", regNames64[op.dst_reg], op.symbol_id);
}

void compileOpMulNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	if (op.value == -1) {
		compileCondition(ctx->dst, op.cond_id, 1);
		fprintf(ctx->dst, "\tneg %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
	} else {
		int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
		fprintf(ctx->dst, "\tmul %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG));
		endConditionalOp(ctx->dst, cond_ctx);
	}
}

void compileOpMulrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tmul %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]);
}

void compileOpDivNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	if (op.value == -1) {
		compileCondition(ctx->dst, op.cond_id, 1);
		fprintf(ctx->dst, "\tneg %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
	} else {
		int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
		fprintf(ctx->dst, "\tudiv %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG));
		endConditionalOp(ctx->dst, cond_ctx);
	}
}

void compileOpDivrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tudiv %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]);
}

void compileOpDivsNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	if (op.value == -1) {
		compileCondition(ctx->dst, op.cond_id, 1);
		fprintf(ctx->dst, "\tneg %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
	} else {
		int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
		fprintf(ctx->dst, "\tsdiv %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG));
		endConditionalOp(ctx->dst, cond_ctx);
	}
}

void compileOpDivsrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tsdiv %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]);
}

void compileOpExtprocNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	Submodule* submodule = getOpSubmodule(module, module->seg_exec.data + index);
	char proc_name[strlen(submodule->name) + strlen(op.mark_name) + 3];
	if (strcmp(submodule->name, ".") == 0) {
		snprintf(proc_name, sizeof(proc_name), "%s", op.mark_name);
	} else {
		snprintf(proc_name, sizeof(proc_name), "%s::%s", submodule->name, op.mark_name);
	}
	fprintf(
		ctx->dst,
		".global \"%1$s\"\n"
		"\"%1$s\":\n"
		"\tstp fp, lr, [sp, -16]!\n"
		"\tmov fp, sp\n",
		proc_name
	);
}

void compileOpLdvNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	const char* literal = intLiteral(ctx->dst, 8, -op.symbol_id, INTL_ADDR_OFFSET);
	switch (op.var_size) {
		case 1:
			fprintf(ctx->dst, "\tldrb %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		case 2:
			fprintf(ctx->dst, "\tldrh %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		case 4:
			fprintf(ctx->dst, "\tldrw %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		case 8:
			fprintf(ctx->dst, "\tldr %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		default:
			eprintf("internal compiler bug in compileOpLdvNative: unexpected variable size %hhd\n", op.var_size);
			abort();
	}
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpStrvNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	const char* literal = intLiteral(ctx->dst, 8, -op.symbol_id, INTL_ADDR_OFFSET);
	switch (op.var_size) {
		case 1:
			fprintf(ctx->dst, "\tstrb %s, [fp, %s]\n", regNames32[op.src_reg], literal);
			break;
		case 2:
			fprintf(ctx->dst, "\tstrh %s, [fp, %s]\n", regNames32[op.src_reg], literal);
			break;
		case 4:
			fprintf(ctx->dst, "\tstr %s, [fp, %s]\n", regNames32[op.src_reg], literal);
			break;
		case 8:
			fprintf(ctx->dst, "\tstr %s, [fp, %s]\n", regNames64[op.src_reg], literal);
			break;
		default:
			eprintf("internal compiler bug in compileOpStrvNative: unexpected variable size %hhd\n", op.var_size);
			abort();
	}
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpPopvNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	uint64_t offset = nativeStackOffset(ctx, 0);

	switch (op.var_size) {
		case 1:
			fprintf(ctx->dst, "\tldrsb %s, [sp, %llu]\n", regNames64[op.dst_reg], offset);
			break;
		case 2:
			fprintf(ctx->dst, "\tldrsh %s, [sp, %llu]\n", regNames64[op.dst_reg], offset);
			break;
		case 4:
			fprintf(ctx->dst, "\tldrsw %s, [sp, %llu]\n", regNames64[op.dst_reg], offset);
			break;
		case 8:
			fprintf(ctx->dst, "\tldr %s, [sp, %llu]\n", regNames64[op.dst_reg], offset);
			break;
		default:
			eprintf("internal compiler bug in compileOpPopvNative: unexpected variable size %hhd\n", op.var_size);
	}

	if (offset + op.var_size >= STACK_ALIGNMENT) {
		fprintf(ctx->dst, "\tadd sp, sp, "_s2(STACK_ALIGNMENT)"\n");
	}
	ctx->cur_frame_size -= op.var_size;
}

void compileOpPushvNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	if (alignby(ctx->cur_frame_size, STACK_ALIGNMENT) < alignby(ctx->cur_frame_size + op.var_size, STACK_ALIGNMENT)) {
		fprintf(ctx->dst, "\tsub sp, sp, "_s2(STACK_ALIGNMENT)"\n");
	}
	ctx->cur_frame_size += op.var_size;
	uint64_t offset = nativeStackOffset(ctx, 0);

	switch (op.var_size) {
		case 1:
			fprintf(ctx->dst, "\tstrb %s, [sp, %llu]\n", regNames32[op.src_reg], offset);
			break;
		case 2:
			fprintf(ctx->dst, "\tstrh %s, [sp, %llu]\n", regNames32[op.src_reg], offset);
			break;
		case 4:
			fprintf(ctx->dst, "\tstr %s, [sp, %llu]\n", regNames32[op.src_reg], offset);
			break;
		case 8:
			fprintf(ctx->dst, "\tstr %s, [sp, %llu]\n", regNames64[op.src_reg], offset);
			break;
		default:
			eprintf("internal compiler bug in compileOpPushvNative: unexpected variable size %hhd\n", op.var_size);
			abort();
	}
}

void compileOpAtfNative(Module* module, int index, CompCtx* ctx)
{
	ctx->src_path = module->seg_exec.data[index].mark_name;
	ctx->src_line = 0;
	fprintf(ctx->dst, "// %s:%d\n", ctx->src_path, ctx->src_line);
}

void compileOpAtlNative(Module* module, int index, CompCtx* ctx)
{
	ctx->src_line = module->seg_exec.data[index].symbol_id;
	fprintf(ctx->dst, "// %s:%d\n", ctx->src_path, ctx->src_line);
}

void compileOpSetcNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tcset %s, %s\n", regNames64[op.dst_reg], conditionNames_arm64[op.cond_arg]);
}

void compileOpDelnvNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int64_t aligned_offset = alignby(ctx->cur_frame_size, STACK_ALIGNMENT);
	ctx->cur_frame_size -= op.symbol_id;
	aligned_offset -= alignby(ctx->cur_frame_size, STACK_ALIGNMENT);

	if (aligned_offset) {
		int cond_id = startConditionalOp(ctx->dst, op.cond_id);
		fprintf(ctx->dst, "\tadd sp, sp, %s\n", intLiteral(ctx->dst, 8, aligned_offset, 0));
		endConditionalOp(ctx->dst, cond_id);
	}
}

void compileOpLd64SNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldr %s, [%s]\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpLd32SNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldrsw %s, [%s]\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpLd16SNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldrsh %s, [%s]\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpLd8SNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	compileCondition(ctx->dst, op.cond_id, 1);
	fprintf(ctx->dst, "\tldrsb %s, [%s]\n", regNames64[op.dst_reg], regNames64[op.src_reg]);
}

void compileOpLdvsNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	int cond_ctx = startConditionalOp(ctx->dst, op.cond_id);
	const char* literal = intLiteral(ctx->dst, 8, -op.symbol_id, INTL_ADDR_OFFSET);
	switch (op.var_size) {
		case 1:
			fprintf(ctx->dst, "\tldrsb %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		case 2:
			fprintf(ctx->dst, "\tldrsh %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		case 4:
			fprintf(ctx->dst, "\tldrsw %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		case 8:
			fprintf(ctx->dst, "\tldr %s, [fp, %s]\n", regNames64[op.dst_reg], literal);
			break;
		default:
			eprintf("internal compiler bug in compileOpLdvNative: unexpected variable size %hhd\n", op.var_size);
			abort();
	}
	endConditionalOp(ctx->dst, cond_ctx);
}

void compileOpSx32Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	fprintf(ctx->dst, "\tsxtw %s, %s\n", regNames64[op.dst_reg], regNames32[op.src_reg]);
}

void compileOpSx16Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	fprintf(ctx->dst, "\tsxth %s, %s\n", regNames64[op.dst_reg], regNames32[op.src_reg]);
}

void compileOpSx8Native(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	fprintf(ctx->dst, "\tsxtb %s, %s\n", regNames64[op.dst_reg], regNames32[op.src_reg]);
}

void compileOpModNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];

	if ((op.value & (op.value - 1)) == 0) {
		fprintf(ctx->dst, "\tand %s, %s, %s\n", regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value - 1, 0));
	} else if (op.dst_reg == op.src_reg) {
		fprintf(
			ctx->dst,
			"\tmov x9, %1$s\n"
			"\tudiv %1$s, %1$s, %2$s\n"
			"\tmul %1$s, %1$s, %2$s\n"
			"\tsub %1$s, x9, %1$s\n",
			regNames64[op.dst_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG)
		);
	} else {
		fprintf(
			ctx->dst,
			"\tudiv %1$s, %1$s, %3$s\n"
			"\tmul %1$s, %1$s, %3$s\n"
			"\tsub %1$s, %2$s, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG)
		);
	}
}

void compileOpModsNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	if (op.dst_reg == op.src_reg) {
		fprintf(
			ctx->dst,
			"\tmov x9, %1$s\n"
			"\tsdiv %1$s, %1$s, %2$s\n"
			"\tmul %1$s, %1$s, %2$s\n"
			"\tsub %1$s, x9, %1$s\n",
			regNames64[op.dst_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG)
		);
	} else {
		fprintf(
			ctx->dst,
			"\tsdiv %1$s, %1$s, %3$s\n"
			"\tmul %1$s, %1$s, %3$s\n"
			"\tsub %1$s, %2$s, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src_reg], intLiteral(ctx->dst, 8, op.value, INTL_REG_ARG)
		);
	}
}

void compileOpModrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	if (op.dst_reg == op.src_reg) {
		fprintf(
			ctx->dst,
			"\tmov x8, %1$s\n"
			"\tudiv %1$s, %1$s, %2$s\n"
			"\tmul %1$s, %1$s, %2$s\n"
			"\tsub %1$s, x8, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src2_reg]
		);
	} else if (op.dst_reg == op.src2_reg) {
		fprintf(
			ctx->dst,
			"\tmov x8, %1$s\n"
			"\tudiv %1$s, %2$s, x8\n"
			"\tmul %1$s, %1$s, x8\n"
			"\tsub %1$s, %2$s, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src_reg]
		);
	} else {
		fprintf(
			ctx->dst,
			"\tudiv %1$s, %2$s, %3$s\n"
			"\tmul %1$s, %1$s, %3$s\n"
			"\tsub %1$s, %2$s, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]
		);
	}
}

void compileOpModsrNative(Module* module, int index, CompCtx* ctx)
{
	Op op = module->seg_exec.data[index];
	if (op.dst_reg == op.src_reg) {
		fprintf(
			ctx->dst,
			"\tmov x8, %1$s\n"
			"\tsdiv %1$s, %1$s, %2$s\n"
			"\tmul %1$s, %1$s, %2$s\n"
			"\tsub %1$s, x8, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src2_reg]
		);
	} else if (op.dst_reg == op.src2_reg) {
		fprintf(
			ctx->dst,
			"\tmov x8, %1$s\n"
			"\tsdiv %1$s, %2$s, x8\n"
			"\tmul %1$s, %1$s, x8\n"
			"\tsub %1$s, %2$s, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src_reg]
		);
	} else {
		fprintf(
			ctx->dst,
			"\tsdiv %1$s, %2$s, %3$s\n"
			"\tmul %1$s, %1$s, %3$s\n"
			"\tsub %1$s, %2$s, %1$s\n",
			regNames64[op.dst_reg], regNames64[op.src_reg], regNames64[op.src2_reg]
		);
	}
}

OpNativeCompiler native_op_compilers[] = {
	[OP_NONE] = &compileNopNative,
	[OP_END] = &compileOpEndNative,
	[OP_MARK] = &compileOpMarkNative,
	[OP_SET] = &compileOpSetNative,
	[OP_SETR] = &compileOpSetrNative,
	[OP_SETD] = &compileOpSetdNative,
	[OP_SETB] = &compileOpSetbNative,
	[OP_ADD] = &compileOpAddNative,
	[OP_ADDR] = &compileOpAddrNative,
	[OP_SUB] = &compileOpSubNative,
	[OP_SUBR] = &compileOpSubrNative,
	[OP_SYS] = &compileOpSyscallNative,
	[OP_GOTO] = &compileOpGotoNative,
	[OP_CMP] = &compileOpCmpNative,
	[OP_CMPR] = &compileOpCmprNative,
	[OP_AND] = &compileOpAndNative,
	[OP_ANDR] = &compileOpAndrNative,
	[OP_OR] = &compileOpOrNative,
	[OP_ORR] = &compileOpOrrNative,
	[OP_NOT] = &compileOpNotNative,
	[OP_XOR] = &compileOpXorNative,
	[OP_XORR] = &compileOpXorrNative,
	[OP_SHL] = &compileOpShlNative,
	[OP_SHLR] = &compileOpShlrNative,
	[OP_SHR] = &compileOpShrNative,
	[OP_SHRR] = &compileOpShrrNative,
	[OP_SHRS] = &compileOpShrsNative,
	[OP_SHRSR] = &compileOpShrsrNative,
	[OP_PROC] = &compileOpProcNative,
	[OP_CALL] = &compileOpCallNative,
	[OP_RET] = &compileOpRetNative,
	[OP_ENDPROC] = &compileOpEndprocNative,
	[OP_LD64] = &compileOpLd64Native,
	[OP_STR64] = &compileOpStr64Native,
	[OP_LD32] = &compileOpLd32Native,
	[OP_STR32] = &compileOpStr32Native,
	[OP_LD16] = &compileOpLd16Native,
	[OP_STR16] = &compileOpStr16Native,
	[OP_LD8] = &compileOpLd8Native,
	[OP_STR8] = &compileOpStr8Native,
	[OP_VAR] = &compileOpVarNative,
	[OP_SETV] = &compileOpSetvNative,
	[OP_MUL] = &compileOpMulNative,
	[OP_MULR] = &compileOpMulrNative,
	[OP_DIV] = &compileOpDivNative,
	[OP_DIVR] = &compileOpDivrNative,
	[OP_DIVS] = &compileOpDivsNative,
	[OP_DIVSR] = &compileOpDivsrNative,
	[OP_EXTPROC] = &compileOpExtprocNative,
	[OP_LDV] = &compileOpLdvNative,
	[OP_STRV] = &compileOpStrvNative,
	[OP_POPV] = &compileOpPopvNative,
	[OP_PUSHV] = &compileOpPushvNative,
	[OP_ATF] = &compileOpAtfNative,
	[OP_ATL] = &compileOpAtlNative,
	[OP_SETC] = &compileOpSetcNative,
	[OP_DELNV] = &compileOpDelnvNative,
	[OP_LD64S] = &compileOpLd64SNative,
	[OP_LD32S] = &compileOpLd32SNative,
	[OP_LD16S] = &compileOpLd16SNative,
	[OP_LD8S] = &compileOpLd8SNative,
	[OP_LDVS] = &compileOpLdvsNative,
	[OP_SX32] = &compileOpSx32Native,
	[OP_SX16] = &compileOpSx16Native,
	[OP_SX8] = &compileOpSx8Native,
	[OP_MOD] = &compileOpModNative,
	[OP_MODS] = &compileOpModsNative,
	[OP_MODR] = &compileOpModrNative,
	[OP_MODSR] = &compileOpModsrNative
};
static_assert(
	N_OPS == sizeof(native_op_compilers) / sizeof(native_op_compilers[0]),
	"not all operations have matching native compilers"
);

bool isForDataSegment(DataBlock* block)
{
	arrayForeach (DataPiece, piece, block->pieces) {
		if (piece->type == PIECE_DB_ADDR) return true;
	}
	return block->is_mutable;
}

bool isForBSSSegment(DataBlock* block)
//	   ^
{
	arrayForeach(DataPiece, piece, block->pieces) {
		if (piece->type != PIECE_ZERO) return false;
	}
	return true;
}

void compileModule(Module* src, FILE* dst)
// notes: 
// 		x28 - argc
// 		x27 - argv
// 		x26 - errno
{
	CompCtx ctx = {.dst = dst};
#	define CUR_SEG_BSS 1
#	define CUR_SEG_DATA 2
#	define CUR_SEG_TEXT 3
	char cur_seg = 0;
	static_assert(N_PIECE_TYPES == 8, "not all data piece types are handled in `compileModule`");
	if (src->seg_data.length) {
		for (DataBlock* block = src->seg_data.data; block - src->seg_data.data < src->seg_data.length; ++block) {
			if (isForBSSSegment(block) && cur_seg != CUR_SEG_BSS) {
				fputs(".bss\n", dst);
				cur_seg = CUR_SEG_BSS;
			} else if (isForDataSegment(block) && cur_seg != CUR_SEG_DATA) {
				fputs(".data\n", dst);
				cur_seg = CUR_SEG_DATA;
			} else if (cur_seg != CUR_SEG_TEXT) {
				fputs(".text\n", dst);
				cur_seg = CUR_SEG_TEXT;
			}
			fprintf(dst, "\"%s::%s\":\n", getDataBlockSubmodule(src, block)->name, block->name);
			for (DataPiece* piece = block->pieces.data; piece - block->pieces.data < block->pieces.length; ++piece) {
				switch (piece->type) {
					case PIECE_BYTES:
					case PIECE_TEXT:
						fputs("\t.ascii \"", dst);
						fputsbufesc(dst, piece->data, BYTEFMT_ESC_DQUOTE | BYTEFMT_HEX);
						fputs("\"\n", dst);
						break;
					case PIECE_INT16:
					case PIECE_INT32:
					case PIECE_INT64:
						fprintf(dst, "\t.%dbyte %lld\n", 2 << (piece->type - PIECE_INT16), piece->integer);
						break;
					case PIECE_DB_ADDR:
						fprintf(
							dst,
							"\t.quad \"%s::%s\"\n",
							src->submodules.data[piece->module_id].name,
							src->seg_data.data[piece->symbol_id].name
						);
						break;
					case PIECE_ZERO:
						fprintf(dst, "\t.zero %lld\n", piece->n_bytes);
						break;
					case PIECE_NONE:
					case N_PIECE_TYPES:
					default:
						eprintf("internal compiler bug: invalid block piece type %d\n", piece->type);
						abort();
				}
			}
			fprintf(dst, ".align 4\n");
		}
		if (cur_seg != CUR_SEG_TEXT) fprintf(dst, ".text\n.align 4\n");
	} else fprintf(dst, ".text\n.align 4\n");


	fprintf(
		dst,
		".global "DEFAULT_ENTRY_NAME"\n"
		DEFAULT_ENTRY_NAME":\n"
		"\tmov x28, x0\n"
		"\tmov x27, x1\n"
		"\tmov x26, 0\n"
		"\tmov x0, 0\n"
		"\tmov x1, 0\n"
		"\tmov x2, 0\n"
		"\tmov x3, 0\n"
		"\tmov x4, 0\n"
		"\tmov x5, 0\n"
		"\tmov x6, 0\n"
		"\tmov x7, 0\n"
	);

// compile the code outside the procedures
	arrayForeach (Op, op, src->seg_exec) {
		while (op->type == OP_PROC || op->type == OP_EXTPROC)
			while ((op++)->type != OP_ENDPROC);
		native_op_compilers[op->type](src, op - src->seg_exec.data, &ctx);
	}

	fprintf(
		dst,
		"\tmov x0, 0\n"
		"\tmov x16, 1\n"
		"\tsvc 0\n"
	);
// compile the procedures
	int first_op = 0;
	arrayForeach(Op, op, src->seg_exec) {
		if (op->type == OP_PROC || op->type == OP_EXTPROC) break;
		first_op += 1;
	}
	arrayForeach (Op, op, OpArray_slice(src->seg_exec, first_op, -1)) {
		native_op_compilers[op->type](src, op - src->seg_exec.data, &ctx);
		if (op->type == OP_ENDPROC) {
			while (op->type != OP_END ? op->type != OP_PROC && op->type != OP_EXTPROC : false) {
				op += 1;
			}
			op -= 1;
		}
	}
}
