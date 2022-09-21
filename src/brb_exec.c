// implementation for execution of BRB modules
#include <brb.h>
#include <unistd.h>

defArray(sbuf);

static sbuf allocDataBlock(BRB_DataBlock block)
{
	sbuf_size_t res = 0;
	arrayForeach (BRB_DataPiece, piece, block.pieces) {
		switch (piece->type) {
			case BRB_DP_BYTES:
			case BRB_DP_TEXT:
				res += piece->data.length;
				break;
			case BRB_DP_I16:
				res += 2;
				break;
			case BRB_DP_I32:
				res += 4;
				break;
			case BRB_DP_PTR:
			case BRB_DP_BUILTIN:
			case BRB_DP_DBADDR:
				res += sizeof(void*);
				break;
			case BRB_DP_I64:
				res += 8;
				break;
			case BRB_DP_ZERO:
				res += BRB_getTypeRTSize(piece->content_type);
				break;
			case BRB_DP_NONE:
			case BRB_N_DP_TYPES:
			default:
				assert(false, "unknown data piece type %u\n", piece->type);
		}
	}
	return smalloc(res);
}

static void assembleDataBlock(BRB_ExecEnv* env, BRB_DataBlock block, sbuf dst)
{
	sbuf_size_t offset = 0;
	arrayForeach (BRB_DataPiece, piece, block.pieces) {
		switch (piece->type) {
			case BRB_DP_BYTES:
			case BRB_DP_TEXT:
				memcpy(dst.data + offset, piece->data.data, piece->data.length);
				offset += piece->data.length;
				break;
			case BRB_DP_I16:
				*(uint16_t*)(dst.data + offset) = piece->content_u;
				offset += 2;
				break;
			case BRB_DP_I32:
				*(uint32_t*)(dst.data + offset) = piece->content_u;
				offset += 4;
				break;
			case BRB_DP_PTR:
				*(uintptr_t*)(dst.data + offset) = piece->content_u;
				offset += sizeof(void*);
				break;
			case BRB_DP_I64:
				*(uint64_t*)(dst.data + offset) = piece->content_u;
				offset += 8;
				break;
			case BRB_DP_DBADDR:
				*(char**)(dst.data + offset) = env->seg_data.data[piece->content_u].data;
				offset += sizeof(void*);
				break;
			case BRB_DP_ZERO: {
				size_t n_zeroes = BRB_getTypeRTSize(piece->content_type);
				memset(dst.data + offset, 0, n_zeroes);
				offset += n_zeroes;
			} break;
			case BRB_DP_BUILTIN:
				*(uintptr_t*)(dst.data + offset) = BRB_builtinValues[piece->content_u];
				offset += sizeof(void*);
				break;
			case BRB_DP_NONE:
			case BRB_N_DP_TYPES:
			default:
				assert(false, "unknown data piece type %u\n", piece->type);
		}
	}
}

static void prepareOpForExec(BRB_ModuleBuilder* builder, sbufArray seg_data, BRB_ProcArray seg_exec, uint32_t proc_id, uint32_t op_id)
{
	BRB_Op *const op = &seg_exec.data[proc_id].body.data[op_id];
	switch (op->type) {
			case BRB_OP_NOP:
			case BRB_OP_END:
			case BRB_OP_I8:
			case BRB_OP_I16:
			case BRB_OP_I32:
			case BRB_OP_I64:
			case BRB_OP_PTR:
			case BRB_OP_SYS:
			case BRB_OP_ADDIAT8:
			case BRB_OP_ADDIAT16:
			case BRB_OP_ADDIAT32:
			case BRB_OP_ADDIATP:
			case BRB_OP_ADDIAT64:
			case BRB_OP_SUBIAT8:
			case BRB_OP_SUBIAT16:
			case BRB_OP_SUBIAT32:
			case BRB_OP_SUBIATP:
			case BRB_OP_SUBIAT64:
			case BRB_OP_MULIAT8:
			case BRB_OP_MULIAT16:
			case BRB_OP_MULIAT32:
			case BRB_OP_MULIATP:
			case BRB_OP_MULIAT64:
			case BRB_OP_DIVIAT8:
			case BRB_OP_DIVIAT16:
			case BRB_OP_DIVIAT32:
			case BRB_OP_DIVIATP:
			case BRB_OP_DIVIAT64:
			case BRB_OP_DIVSIAT8:
			case BRB_OP_DIVSIAT16:
			case BRB_OP_DIVSIAT32:
			case BRB_OP_DIVSIATP:
			case BRB_OP_DIVSIAT64:
			case BRB_OP_MODIAT8:
			case BRB_OP_MODIAT16:
			case BRB_OP_MODIAT32:
			case BRB_OP_MODIATP:
			case BRB_OP_MODIAT64:
			case BRB_OP_MODSIAT8:
			case BRB_OP_MODSIAT16:
			case BRB_OP_MODSIAT32:
			case BRB_OP_MODSIATP:
			case BRB_OP_MODSIAT64:
			case BRB_OP_ANDIAT8:
			case BRB_OP_ANDIAT16:
			case BRB_OP_ANDIAT32:
			case BRB_OP_ANDIATP:
			case BRB_OP_ANDIAT64:
			case BRB_OP_ORIAT8:
			case BRB_OP_ORIAT16:
			case BRB_OP_ORIAT32:
			case BRB_OP_ORIATP:
			case BRB_OP_ORIAT64:
			case BRB_OP_XORIAT8:
			case BRB_OP_XORIAT16:
			case BRB_OP_XORIAT32:
			case BRB_OP_XORIATP:
			case BRB_OP_XORIAT64:
			case BRB_OP_SHLIAT8:
			case BRB_OP_SHLIAT16:
			case BRB_OP_SHLIAT32:
			case BRB_OP_SHLIATP:
			case BRB_OP_SHLIAT64:
			case BRB_OP_SHRIAT8:
			case BRB_OP_SHRIAT16:
			case BRB_OP_SHRIAT32:
			case BRB_OP_SHRIATP:
			case BRB_OP_SHRIAT64:
			case BRB_OP_SHRSIAT8:
			case BRB_OP_SHRSIAT16:
			case BRB_OP_SHRSIAT32:
			case BRB_OP_SHRSIATP:
			case BRB_OP_SHRSIAT64:
			case BRB_OP_NOTAT8:
			case BRB_OP_NOTAT16:
			case BRB_OP_NOTAT32:
			case BRB_OP_NOTATP:
			case BRB_OP_NOTAT64:
				break;
			case BRB_OP_ADDR:
				op->operand_u = BRB_getStackItemRTOffset(builder, proc_id, op_id, op->operand_u);
				break;
			case BRB_OP_DBADDR:
				op->operand_u = (uintptr_t)seg_data.data[op->operand_u].data;
				break;
			case BRB_OP_LD:
				op->operand_u = BRB_getTypeRTSize(op->operand_type) - sizeof(void*);
				break;
			case BRB_OP_STR:
				op->operand_u = BRB_getStackItemRTSize(builder, proc_id, op_id - 1, 1);
				break;
			case BRB_OP_BUILTIN:
				op->operand_u = BRB_builtinValues[op->operand_u];
				break;
			case BRB_OP_ADD:
			case BRB_OP_SUB:
			case BRB_OP_MUL:
			case BRB_OP_DIV:
			case BRB_OP_DIVS:
			case BRB_OP_MOD:
			case BRB_OP_MODS:
			case BRB_OP_AND:
			case BRB_OP_OR:
			case BRB_OP_XOR:
			case BRB_OP_SHL:
			case BRB_OP_SHR:
			case BRB_OP_SHRS:
				op->x_op2_size = BRB_getStackItemRTSize(builder, proc_id, op_id - 1, 1);
			case BRB_OP_ADDI:
			case BRB_OP_SUBI:
			case BRB_OP_MULI:
			case BRB_OP_DIVI:
			case BRB_OP_DIVSI:
			case BRB_OP_MODI:
			case BRB_OP_MODSI:
			case BRB_OP_ANDI:
			case BRB_OP_ORI:
			case BRB_OP_XORI:
			case BRB_OP_SHLI:
			case BRB_OP_SHRI:
			case BRB_OP_SHRSI:
			case BRB_OP_NOT:
			case BRB_OP_DROP:
				op->x_op1_size = BRB_getStackItemRTSize(builder, proc_id, op_id - 1, 0);
				break;
			case BRB_N_OPS:
			default:
				assert(false, "unknown operation type %u\n", op->type);
	}
}

BRB_Error BRB_initExecEnv(BRB_ExecEnv* env, BRB_Module module, size_t stack_size)
{
	*env = (BRB_ExecEnv){0};
	if (module.exec_entry_point >= module.seg_exec.length) return (BRB_Error){.type = BRB_ERR_INVALID_ENTRY};
	const BRB_Proc* const proc = &module.seg_exec.data[module.exec_entry_point];
	if (proc->ret_type.kind != BRB_TYPE_VOID || proc->args.length) return (BRB_Error){.type = BRB_ERR_INVALID_ENTRY_PROTOTYPE};
	if (!(env->stack = smalloc(stack_size)).data) return (BRB_Error){.type = BRB_ERR_NO_MEMORY};
	env->stack_head = env->stack.data + env->stack.length;

	if (!sbufArray_incrlen(&env->seg_data, module.seg_data.length) && module.seg_data.length) return (BRB_Error){.type = BRB_ERR_NO_MEMORY};
	arrayForeach (BRB_DataBlock, block, module.seg_data) {
		env->seg_data.data[block - module.seg_data.data] = allocDataBlock(*block);
		if (!env->seg_data.data[block - module.seg_data.data].data) return (BRB_Error){.type = BRB_ERR_NO_MEMORY};
	}
	arrayForeach (BRB_DataBlock, block, module.seg_data) {
		assembleDataBlock(env, *block, env->seg_data.data[block - module.seg_data.data]);
	}

	BRB_ModuleBuilder builder;
	BRB_Error err = BRB_initModuleBuilder(&builder);
	if (err.type) return err;
	builder.module.seg_data = module.seg_data;
	uint32_t _;
	arrayForeach (BRB_Proc, proc, module.seg_exec) {
		BRB_addProc(&builder, &_, proc->name, proc->args.length, proc->args.data, proc->ret_type, proc->body.length);
	}

	arrayForeach (BRB_Proc, proc, module.seg_exec) {
		arrayForeach (BRB_Op, op, proc->body) {
			BRB_addOp(&builder, proc - module.seg_exec.data, *op);
			prepareOpForExec(&builder, env->seg_data, builder.module.seg_exec, proc - module.seg_exec.data, op - proc->body.data);
		}
	}
	BRB_addOp(&builder, module.exec_entry_point, (BRB_Op){.type = BRB_OP_END});
	BRB_setEntryPoint(&builder, module.exec_entry_point);

	if ((err = BRB_extractModule(builder, &module)).type) return err;
	env->seg_exec = module.seg_exec;
	env->entry_point = module.exec_entry_point;

	return (BRB_Error){0};
}
/*
void getSrcLoc(ExecEnv* env, const char** filename_p, size_t* line_p)
{
	*filename_p = NULL;
	*line_p = 0;
	for (int i = env->exec_index; i >= 0; i--) {
		Op op = module->seg_exec.data[i];
		if (op.type == OP_ATF) {
			*filename_p = op.mark_name;
			return;
		} else if (op.type == OP_ATL && !*line_p) *line_p = op.symbol_id;
	}
}
*/

typedef bool (*BRB_syscall)(BRB_ExecEnv* env);

bool BRB_sysExit(BRB_ExecEnv* env)
{
	env->exec_status.type = BRB_EXC_EXIT;
	env->exec_status.exit_code = *(uintptr_t*)env->stack_head;
	env->stack_head += sizeof(uintptr_t);
	++env->exec_index;
	return true;
}

bool BRB_sysWrite(BRB_ExecEnv* env)
{
	register const intptr_t res = write(((intptr_t*)env->stack_head)[0], ((void**)env->stack_head)[1], ((uintptr_t*)env->stack_head)[2]);
	((intptr_t*)env->stack_head)[2] = res;
	env->stack_head += 2 * sizeof(void*);
	++env->exec_index;
	return false;
}

bool BRB_sysRead(BRB_ExecEnv* env)
{
	register const intptr_t res = read(((intptr_t*)env->stack_head)[0], ((void**)env->stack_head)[1], ((uintptr_t*)env->stack_head)[2]);
	((intptr_t*)env->stack_head)[2] = res;
	env->stack_head += 2 * sizeof(void*);
	++env->exec_index;
	return false;
}

static const BRB_syscall BRB_syscalls[] = {
	[BRB_SYS_EXIT] = BRB_sysExit,
	[BRB_SYS_WRITE] = BRB_sysWrite,
	[BRB_SYS_READ] = BRB_sysRead
};
static_assert(sizeof(BRB_syscalls) / sizeof(BRB_syscall) == BRB_N_SYSCALLS, "not all syscalls have their implementations defined");

bool BRB_execOp(BRB_ExecEnv* env)
{
#define ALLOC_STACK_SPACE(incr) \
	if ((env->stack_head -= (incr)) < env->stack.data) return (env->exec_status.type = BRB_EXC_STACK_OVERFLOW);

	const BRB_Op op = env->cur_proc[env->exec_index];
	switch (op.type) {
		case BRB_OP_NOP:
			++env->exec_index;
			return false;
		case BRB_OP_END:
			env->exec_status.type = BRB_EXC_END;
			return true;
		case BRB_OP_I8:
			ALLOC_STACK_SPACE(1);
			*(uint8_t*)env->stack_head = op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_I16:
			ALLOC_STACK_SPACE(2);
			*(uint16_t*)env->stack_head = op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_I32:
			ALLOC_STACK_SPACE(4);
			*(uint32_t*)env->stack_head = op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_PTR:
		case BRB_OP_DBADDR:
		case BRB_OP_BUILTIN:
			ALLOC_STACK_SPACE(sizeof(intptr_t));
			*(uintptr_t*)env->stack_head = op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_I64:
			ALLOC_STACK_SPACE(8);
			*(uint64_t*)env->stack_head = op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_ADDR:
			ALLOC_STACK_SPACE(sizeof(void*));
			*(void**)env->stack_head = env->stack_head + op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_LD:
			ALLOC_STACK_SPACE(op.operand_u);
			memmove(env->stack_head, *(void**)(env->stack_head + op.operand_u), op.operand_u + sizeof(void*));
			++env->exec_index;
			return false;
		case BRB_OP_STR:
			memmove(*((void**)(env->stack_head)++), env->stack_head, op.operand_u);
			env->stack_head += op.operand_u;
			++env->exec_index;
			return false;
		case BRB_OP_SYS:
			return BRB_syscalls[op.operand_u](env);
		case BRB_OP_ADD: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head + op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head + op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head + op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head + op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_ADDI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head += op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head += op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head += op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head += op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_ADDIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp += op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ADDIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp += op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ADDIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp += op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ADDIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp += op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ADDIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp += op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SUB: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head - op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head - op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head - op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head - op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_SUBI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head -= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head -= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head -= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head -= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_SUBIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp -= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SUBIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp -= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SUBIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp -= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SUBIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp -= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SUBIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp -= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MUL: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head * op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head * op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head * op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head * op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_MULI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head *= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head *= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head *= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head *= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_MULIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp *= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MULIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp *= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MULIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp *= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MULIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp *= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MULIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp *= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIV: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head / op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head / op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head / op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head / op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head /= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head /= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head /= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head /= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_DIVIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp /= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp /= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp /= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp /= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp /= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVS: {
			int64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(int8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(int16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(int32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(int64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(int8_t *)(env->stack_head + op.x_op2_size) = *(int8_t *)env->stack_head / op2; break;
				case 2: *(int16_t*)(env->stack_head + op.x_op2_size) = *(int16_t*)env->stack_head / op2; break;
				case 4: *(int32_t*)(env->stack_head + op.x_op2_size) = *(int32_t*)env->stack_head / op2; break;
				case 8: *(int64_t*)(env->stack_head + op.x_op2_size) = *(int64_t*)env->stack_head / op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVSI:
			switch (op.x_op1_size) {
				case 1: *(int8_t *)env->stack_head /= op.operand_s; break;
				case 2: *(int16_t*)env->stack_head /= op.operand_s; break;
				case 4: *(int32_t*)env->stack_head /= op.operand_s; break;
				case 8: *(int64_t*)env->stack_head /= op.operand_s; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_DIVSIAT8: {
			register int8_t* temp = *(int8_t**)env->stack_head;
			*(int8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp /= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVSIAT16: {
			register int16_t* temp = *(int16_t**)env->stack_head;
			*(int16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp /= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVSIAT32: {
			register int32_t* temp = *(int32_t**)env->stack_head;
			*(int32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp /= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVSIATP: {
			register intptr_t* temp = *(intptr_t**)env->stack_head;
			*(intptr_t*)env->stack_head = (*temp /= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DIVSIAT64: {
			register int64_t* temp = *(int64_t**)env->stack_head;
			*(int64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp /= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MOD: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head % op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head % op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head % op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head % op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head %= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head %= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head %= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head %= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_MODIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp %= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp %= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp %= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp %= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp %= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODS: {
			int64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(int8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(int16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(int32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(int64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(int8_t *)(env->stack_head + op.x_op2_size) = *(int8_t *)env->stack_head % op2; break;
				case 2: *(int16_t*)(env->stack_head + op.x_op2_size) = *(int16_t*)env->stack_head % op2; break;
				case 4: *(int32_t*)(env->stack_head + op.x_op2_size) = *(int32_t*)env->stack_head % op2; break;
				case 8: *(int64_t*)(env->stack_head + op.x_op2_size) = *(int64_t*)env->stack_head % op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODSI:
			switch (op.x_op1_size) {
				case 1: *(int8_t *)env->stack_head %= op.operand_s; break;
				case 2: *(int16_t*)env->stack_head %= op.operand_s; break;
				case 4: *(int32_t*)env->stack_head %= op.operand_s; break;
				case 8: *(int64_t*)env->stack_head %= op.operand_s; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_MODSIAT8: {
			register int8_t* temp = *(int8_t**)env->stack_head;
			*(int8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp %= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODSIAT16: {
			register int16_t* temp = *(int16_t**)env->stack_head;
			*(int16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp %= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODSIAT32: {
			register int32_t* temp = *(int32_t**)env->stack_head;
			*(int32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp %= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODSIATP: {
			register intptr_t* temp = *(intptr_t**)env->stack_head;
			*(intptr_t*)env->stack_head = (*temp %= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_MODSIAT64: {
			register int64_t* temp = *(int64_t**)env->stack_head;
			*(int64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp %= op.operand_s);
			++env->exec_index;
			return false;
		}
		case BRB_OP_AND: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head & op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head & op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head & op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head & op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_ANDI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head &= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head &= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head &= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head &= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_ANDIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp &= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ANDIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp &= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ANDIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp &= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ANDIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp &= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ANDIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp &= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_OR: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head | op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head | op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head | op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head | op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_ORI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head |= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head |= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head |= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head |= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_ORIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp |= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ORIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp |= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ORIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp |= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ORIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp |= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_ORIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp |= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_XOR: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head ^ op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head ^ op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head ^ op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head ^ op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_XORI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head ^= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head ^= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head ^= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head ^= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_XORIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp ^= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_XORIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp ^= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_XORIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp ^= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_XORIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp ^= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_XORIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp ^= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHL: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head << op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head << op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head << op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head << op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHLI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head <<= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head <<= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head <<= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head <<= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_SHLIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp <<= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHLIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp <<= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHLIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp <<= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHLIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp <<= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHLIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp <<= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHR: {
			uint64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(uint8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(uint16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(uint32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(uint64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)(env->stack_head + op.x_op2_size) = *(uint8_t *)env->stack_head >> op2; break;
				case 2: *(uint16_t*)(env->stack_head + op.x_op2_size) = *(uint16_t*)env->stack_head >> op2; break;
				case 4: *(uint32_t*)(env->stack_head + op.x_op2_size) = *(uint32_t*)env->stack_head >> op2; break;
				case 8: *(uint64_t*)(env->stack_head + op.x_op2_size) = *(uint64_t*)env->stack_head >> op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRI:
			switch (op.x_op1_size) {
				case 1: *(uint8_t *)env->stack_head >>= op.operand_u; break;
				case 2: *(uint16_t*)env->stack_head >>= op.operand_u; break;
				case 4: *(uint32_t*)env->stack_head >>= op.operand_u; break;
				case 8: *(uint64_t*)env->stack_head >>= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_SHRIAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRIAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRIAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRIATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRIAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRS: {
			int64_t op2;
			switch (op.x_op2_size) {
				case 1:  op2 = *(int8_t *)(env->stack_head + op.x_op1_size); break;
				case 2:  op2 = *(int16_t*)(env->stack_head + op.x_op1_size); break;
				case 4:  op2 = *(int32_t*)(env->stack_head + op.x_op1_size); break;
				case 8:  op2 = *(int64_t*)(env->stack_head + op.x_op1_size); break;
				default: assert(false, "unknown argument size");
			}
			switch (op.x_op1_size) {
				case 1: *(int8_t *)(env->stack_head + op.x_op2_size) = *(int8_t *)env->stack_head >> op2; break;
				case 2: *(int16_t*)(env->stack_head + op.x_op2_size) = *(int16_t*)env->stack_head >> op2; break;
				case 4: *(int32_t*)(env->stack_head + op.x_op2_size) = *(int32_t*)env->stack_head >> op2; break;
				case 8: *(int64_t*)(env->stack_head + op.x_op2_size) = *(int64_t*)env->stack_head >> op2; break;
			}
			env->stack_head += op.x_op2_size;
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRSI:
			switch (op.x_op1_size) {
				case 1: *(int8_t *)env->stack_head >>= op.operand_u; break;
				case 2: *(int16_t*)env->stack_head >>= op.operand_u; break;
				case 4: *(int32_t*)env->stack_head >>= op.operand_u; break;
				case 8: *(int64_t*)env->stack_head >>= op.operand_u; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_SHRSIAT8: {
			register int8_t* temp = *(int8_t**)env->stack_head;
			*(int8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRSIAT16: {
			register int16_t* temp = *(int16_t**)env->stack_head;
			*(int16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRSIAT32: {
			register int32_t* temp = *(int32_t**)env->stack_head;
			*(int32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRSIATP: {
			register intptr_t* temp = *(intptr_t**)env->stack_head;
			*(intptr_t*)env->stack_head = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_SHRSIAT64: {
			register int64_t* temp = *(int64_t**)env->stack_head;
			*(int64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp >>= op.operand_u);
			++env->exec_index;
			return false;
		}
		case BRB_OP_NOT:
			switch (op.x_op1_size) {
				case 1: *(uint8_t*)env->stack_head = ~*(uint8_t*)env->stack_head; break;
				case 2: *(uint16_t*)env->stack_head = ~*(uint16_t*)env->stack_head; break;
				case 4: *(uint32_t*)env->stack_head = ~*(uint32_t*)env->stack_head; break;
				case 8: *(uint64_t*)env->stack_head = ~*(uint64_t*)env->stack_head; break;
			}
			++env->exec_index;
			return false;
		case BRB_OP_NOTAT8: {
			register uint8_t* temp = *(uint8_t**)env->stack_head;
			*(uint8_t*)(env->stack_head += sizeof(void*) - 1) = (*temp = ~*temp);
			++env->exec_index;
			return false;
		}
		case BRB_OP_NOTAT16: {
			register uint16_t* temp = *(uint16_t**)env->stack_head;
			*(uint16_t*)(env->stack_head += sizeof(void*) - 2) = (*temp = ~*temp);
			++env->exec_index;
			return false;
		}
		case BRB_OP_NOTAT32: {
			register uint32_t* temp = *(uint32_t**)env->stack_head;
			*(uint32_t*)(env->stack_head += sizeof(void*) - 4) = (*temp = ~*temp);
			++env->exec_index;
			return false;
		}
		case BRB_OP_NOTATP: {
			register uintptr_t* temp = *(uintptr_t**)env->stack_head;
			*(uintptr_t*)env->stack_head = (*temp = ~*temp);
			++env->exec_index;
			return false;
		}
		case BRB_OP_NOTAT64: {
			register uint64_t* temp = *(uint64_t**)env->stack_head;
			*(uint64_t*)(env->stack_head += sizeof(void*) - 8) = (*temp = ~*temp);
			++env->exec_index;
			return false;
		}
		case BRB_OP_DROP:
			env->stack_head += op.x_op1_size;
			++env->exec_index;
			return false;
		case BRB_N_OPS:
		default:
			env->exec_status.type = BRB_EXC_UNKNOWN_OP;
			return true;
	}
}

void BRB_execModule(BRB_ExecEnv* env, char** args, const volatile bool* interruptor)
{
	env->stack_head = env->stack.data + env->stack.length;
	env->exec_index = 0;
	env->cur_proc = env->seg_exec.data[env->entry_point].body.data;
	env->exec_status.type = BRB_EXC_CONTINUE;

	env->exec_argc = 0;
	if (*args) while (args[++env->exec_argc]);
	env->exec_argv = malloc(env->exec_argc * sizeof(sbuf));
	for (uint32_t i = 0; i < env->exec_argc; i += 1) {
		env->exec_argv[i] = fromstr((char*)args[i]);
		env->exec_argv[i].length += 1;
	}

	bool stub_interruptor = false;
	if (!interruptor) interruptor = &stub_interruptor;

	while (true) {
		if (*interruptor) {
			env->exec_status.type = BRB_EXC_INTERRUPT;
			break;
		}
		if (BRB_execOp(env)) break;
	}

	env->exec_argc = 0;
	free(env->exec_argv);
	env->exec_argv = NULL;
}
