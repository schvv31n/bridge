#include "brb.h"
#include "stdio.h"
#include "errno.h"
#include "unistd.h"
#include "signal.h"
#include "stdlib.h"

defArray(Op);
defArray(DataBlock);
defArray(MemBlock);
defArray(DataSpec)
defArray(ProcFrame);

typedef char* str;
declArray(str);
defArray(str);
class {
	Program* src;
	FILE* dst;
	strArray consts;
} ProgramWriter;

typedef str* field;
declArray(field);
defArray(field);
class {
	Program* dst;
	FILE* src;
	fieldArray unresolved;
} ProgramLoader;

void writeInt(FILE* fd, int64_t x)
{
	if (x) {
		if (inRange(x, INT8_MIN, INT8_MAX)) {
			fputc(1, fd);
			int8_t x8 = (int8_t)x;
			fputc(x8, fd);
		} else if (inRange(x, INT16_MIN, INT16_MAX)) {
			fputc(2, fd);
			int16_t x16 = (int16_t)x;
			fwrite(BRByteOrder(&x16, 2), 2, 1, fd);
		} else if (inRange(x, INT32_MIN, INT32_MAX)) {
			fputc(4, fd);
			int32_t x32 = (int32_t)x;
			fwrite(BRByteOrder(&x32, 4), 4, 1, fd);
		} else {
			fputc(8, fd);
			fwrite(BRByteOrder(&x, 8), 8, 1, fd);
		}
	} else {
		fputc(0, fd);
	}
}

void writeName(ProgramWriter* writer, char* name)
{
	for (long i = 0; i < writer->consts.length; i++) {
		if (streq(name, writer->consts.data[i])) {
			writeInt(writer->dst, i);
			return;
		}
	}
	writeInt(writer->dst, writer->consts.length);
	strArray_append(&writer->consts, name);
}

void writeDataBlock(ProgramWriter* writer, char* name, sbuf obj)
{
	writeName(writer, name);
	writeInt(writer->dst, obj.length);
	fputsbuf(writer->dst, obj);
}

void writeMemoryBlock(ProgramWriter* writer, char* name, int32_t size)
{
	writeName(writer, name);
	writeInt(writer->dst, size);
}

typedef void (*OpWriter) (ProgramWriter*, Op);

void writeNoArgOp(ProgramWriter* writer, Op op) {}

void writeMarkOp(ProgramWriter* writer, Op op)
{
	writeName(writer, op.mark_name);
}

void writeRegImmOp(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	writeInt(writer->dst, op.value);
}

void write2RegOp(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	fputc(op.src_reg, writer->dst);
}

void writeRegSymbolIdOp(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	writeInt(writer->dst, op.symbol_id);
}

void writeOpSetd(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	writeName(writer, writer->src->datablocks.data[op.symbol_id].name);
}

void writeOpSetm(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	writeName(writer, writer->src->memblocks.data[op.symbol_id].name);
}

void writeOpSyscall(ProgramWriter* writer, Op op)
{
	fputc(op.syscall_id, writer->dst);
}

void writeOpGoto(ProgramWriter* writer, Op op)
{
	writeInt(writer->dst, op.symbol_id);
}

void writeOpCall(ProgramWriter* writer, Op op)
{
	writeName(writer, writer->src->execblock.data[op.symbol_id].mark_name);
}

void writeOpCmp(ProgramWriter* writer, Op op)
{
	fputc(op.src_reg, writer->dst);
	writeInt(writer->dst, op.value);
}

void writeOpCmpr(ProgramWriter* writer, Op op)
{
	fputc(op.src_reg, writer->dst);
	fputc(op.src2_reg, writer->dst);
}

void write2RegImmOp(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	fputc(op.src_reg, writer->dst);
	writeInt(writer->dst, op.value);
}

void write3RegOp(ProgramWriter* writer, Op op)
{
	fputc(op.dst_reg, writer->dst);
	fputc(op.src_reg, writer->dst);
	fputc(op.src2_reg, writer->dst);
}

void writeOpVar(ProgramWriter* writer, Op op)
{
	fputc(op.var_size, writer->dst);
}

const OpWriter op_writers[] = {
	[OP_NONE] = &writeNoArgOp,
	[OP_END] = &writeNoArgOp,
	[OP_MARK] = &writeMarkOp,
	[OP_SET] = &writeRegImmOp,
	[OP_SETR] = &write2RegOp,
	[OP_SETD] = &writeOpSetd,
	[OP_SETB] = &writeRegSymbolIdOp,
	[OP_SETM] = &writeOpSetm,
	[OP_ADD] = &write2RegImmOp,
	[OP_ADDR] = &write3RegOp,
	[OP_SUB] = &write2RegImmOp,
	[OP_SUBR] = &write3RegOp,
	[OP_SYS] = &writeOpSyscall,
	[OP_GOTO] = &writeOpGoto,
	[OP_CMP] = &writeOpCmp,
	[OP_CMPR] = &writeOpCmpr,
	[OP_AND] = &write2RegImmOp,
	[OP_ANDR] = &write3RegOp,
	[OP_OR] = &write2RegImmOp,
	[OP_ORR] = &write3RegOp,
	[OP_NOT] = &write2RegOp,
	[OP_XOR] = &write2RegImmOp,
	[OP_XORR] = &write3RegOp,
	[OP_SHL] = &write2RegImmOp,
	[OP_SHLR] = &write3RegOp,
	[OP_SHR] = &write2RegImmOp,
	[OP_SHRR] = &write3RegOp,
	[OP_SHRS] = &write2RegImmOp,
	[OP_SHRSR] = &write3RegOp,
	[OP_PROC] = &writeMarkOp,
	[OP_CALL] = &writeOpCall,
	[OP_RET] = &writeNoArgOp,
	[OP_ENDPROC] = &writeNoArgOp,
	[OP_LD64] = &write2RegOp,
	[OP_STR64] = &write2RegOp,
	[OP_LD32] = &write2RegOp,
	[OP_STR32] = &write2RegOp,
	[OP_LD16] = &write2RegOp,
	[OP_STR16] = &write2RegOp,
	[OP_LD8] = &write2RegOp,
	[OP_STR8] = &write2RegOp,
	[OP_VAR] = &writeOpVar,
	[OP_SETV] = &writeRegSymbolIdOp,
	[OP_MUL] = &write2RegImmOp,
	[OP_MULR] = &write3RegOp,
	[OP_DIV] = &write2RegImmOp,
	[OP_DIVR] = &write3RegOp,
	[OP_DIVS] = &write2RegImmOp,
	[OP_DIVSR] = &write3RegOp,
	[OP_EXTPROC] = &writeMarkOp
};
static_assert(N_OPS == sizeof(op_writers) / sizeof(op_writers[0]), "Some BRB operations have unmatched writers");

void writeOp(ProgramWriter* writer, Op op)
{
	if (op.cond_id) {
		fputc(~op.type, writer->dst);
		fputc(op.cond_id, writer->dst);
	} else {
		fputc(op.type, writer->dst);
	}
	op_writers[op.type](writer, op);
}

void writeProgram(Program* src, FILE* dst)
{
	ProgramWriter writer = {
		.consts = strArray_new(TEMP_CTX, 0),
		.dst = dst,
		.src = src
	};
// writing stack size
	writeInt(dst, src->stack_size == DEFAULT_STACK_SIZE ? 0 : src->stack_size); // dumping stack size
//  dumping data blocks
	writeInt(dst, src->datablocks.length); 
	array_foreach(DataBlock, block, src->datablocks,
		writeDataBlock(&writer, block.name, block.spec);
	);
//  dumping memory blocks
	writeInt(dst, src->memblocks.length);
	array_foreach(MemBlock, block, src->memblocks,
		writeMemoryBlock(&writer, block.name, block.size);
	);
//  dumping operations
	writeInt(dst, src->execblock.length + 1);
	array_foreach(Op, op, src->execblock,
		writeOp(&writer, op);
	);
	writeOp(&writer, (Op){ .type = OP_END });
//  dumping constants pool
	writeInt(dst, writer.consts.length);
	array_foreach(str, constant, writer.consts, 
		fputs(constant, dst);
		fputc('\n', dst);
	);

	strArray_clear(&writer.consts);
}

int8_t loadInt8(FILE* fd, long* n_fetched)
{
	if (feof(fd) || ferror(fd)) { *n_fetched = 0; return 0; }
	*n_fetched = 1;
	return fgetc(fd);
}

int16_t loadInt16(FILE* fd, long* n_fetched)
{
	char res[2];
	if (fread(&res, 2, 1, fd) != 2) { *n_fetched = 0; return 0; };
	*n_fetched = 2;
	return *(int16_t*)BRByteOrder(&res, 2); 
}

int32_t loadInt32(FILE* fd, long* n_fetched)
{
	char res[4];
	if (fread(&res, 4, 1, fd) != 4) { *n_fetched = 0; return 0; };
	*n_fetched = 4;
	return *(int32_t*)BRByteOrder(&res, 4);
}

int64_t loadInt64(FILE* fd, long* n_fetched)
{
	char res[8];
	if (fread(&res, 8, 1, fd) != 8) { *n_fetched = 0; return 0; }
	*n_fetched = 8;
	return *(int64_t*)BRByteOrder(&res, 8);
}

int64_t loadInt(FILE* fd, long* n_fetched)
{
	if (feof(fd) || ferror(fd)) { *n_fetched = 0; return 0; }
	int8_t size = fgetc(fd);
	int64_t res;

	switch (size) {
		case 0: res = 0; break;
		case 1: res = loadInt8(fd, n_fetched); break;
		case 2: res = loadInt16(fd, n_fetched); break;
		case 4: res = loadInt32(fd, n_fetched); break;
		case 8: res = loadInt64(fd, n_fetched); break;
	}
	if (n_fetched) { *n_fetched = *n_fetched + 1; }
	return res;
}

bool loadName(ProgramLoader* loader, char** dst)
{
	long n_fetched = 0;
	int64_t index = loadInt(loader->src, &n_fetched);
	if (!n_fetched) return false;

	*dst = (char*)index;
	fieldArray_append(&loader->unresolved, dst);
	return true;
}

typedef BRBLoadError (*OpLoader) (ProgramLoader*, Op*);
 
void printLoadError(BRBLoadError err)
{
	static_assert(N_BRB_ERRORS == 18, "not all BRB errors are handled\n");
	switch (err.code) {
		case BRB_ERR_OK: break;
		case BRB_ERR_NO_MEMORY:
			eprintf("BRB loading error: memory allocation failure\n");
			break;
		case BRB_ERR_NO_BLOCK_NAME:
			eprintf("BRB loading error: block name not found\n");
			break;
		case BRB_ERR_NO_BLOCK_SIZE:
			eprintf("BRB loading error: block size not found\n");
			break;
		case BRB_ERR_NO_BLOCK_SPEC:
			eprintf("BRB loading error: data block specifier not found\n");
			break;
		case BRB_ERR_NO_OPCODE:
			eprintf("BRB loading error: operation code not found\n");
			break;
		case BRB_ERR_NO_MARK_NAME:
			eprintf("BRB loading error: mark name not found\n");
			break;
		case BRB_ERR_NO_OP_ARG:
			eprintf(
				"BRB loading error: argument for operation `"sbuf_format"` not found\n", 
				unpack(opNames[err.opcode])
			);
			break;
		case BRB_ERR_INVALID_OPCODE:
			eprintf("BRB loading error: invalid operation code %d found\n", err.opcode);
			break;
		case BRB_ERR_INVALID_COND_ID:
			eprintf("BRB loading error: invalid condition code %hhu found\n", err.cond_id);
			break;
		case BRB_ERR_NO_STACK_SIZE:
			eprintf("BRB loading error: found stack size segment identifier, but no stack size was provided\n");
			break;
		case BRB_ERR_NO_ENTRY:
			eprintf("BRB loading error: no `main` procedure found in the program\n");
			break;
		case BRB_ERR_NO_DATA_SEGMENT:
			eprintf("BRB loading error: no data segment found\n");
			break;
		case BRB_ERR_NO_MEMORY_SEGMENT:
			eprintf("BRB loading error: no memory segment found\n");
			break;
		case BRB_ERR_NO_EXEC_SEGMENT:
			eprintf("BRB loading error: no exec segment found\n");
			break;
		case BRB_ERR_NO_NAME_SEGMENT:
			eprintf("BRB loading error: no name segment found\n");
			break;
		case BRB_ERR_NO_NAME_SPEC:
			eprintf("BRB loading error: no name specifier found\n");
			break;
		case BRB_ERR_NO_COND_ID:
			eprintf("BRB loading error: no condition code found\n");
			break;
		case N_BRB_ERRORS:
			eprintf("unreachable\n");
			break;
	}
}

BRBLoadError loadNoArgOp(ProgramLoader* loader, Op* dst)
{
	return (BRBLoadError){0};
}

BRBLoadError loadMarkOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	if (!loadName(loader, &dst->mark_name)) {
		return (BRBLoadError){.code = BRB_ERR_NO_MARK_NAME};
	}
	return (BRBLoadError){0};
}

BRBLoadError loadRegImmOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->dst_reg = loadInt8(loader->src, &status);
	dst->value = loadInt(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError load2RegOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->dst_reg = loadInt8(loader->src, &status);
	dst->src_reg = loadInt8(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError loadRegSymbolIdOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->dst_reg = loadInt8(loader->src, &status);
	dst->symbol_id = loadInt(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError loadRegNameOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->dst_reg = loadInt8(loader->src, &status);
	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }

	if (!loadName(loader, &dst->mark_name)) return (BRBLoadError){.code = BRB_ERR_NO_OP_ARG};
	return (BRBLoadError){0};
}

BRBLoadError loadOpSyscall(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->syscall_id = loadInt8(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError loadOpGoto(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->symbol_id = loadInt(loader->src, &status);

	if (!status) { return (BRBLoadError){.code = BRB_ERR_NO_OP_ARG}; }
	return (BRBLoadError){0};
}

BRBLoadError loadOpCall(ProgramLoader* loader, Op* dst)
{
	if (!loadName(loader, &dst->mark_name)) {
		return (BRBLoadError){.code = BRB_ERR_NO_OP_ARG};
	}

	return (BRBLoadError){0};
}

BRBLoadError loadOpCmp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->src_reg = loadInt8(loader->src, &status);
	dst->value = loadInt(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError loadOpCmpr(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->src_reg = loadInt8(loader->src, &status);
	dst->src2_reg = loadInt8(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError load2RegImmOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->dst_reg = loadInt8(loader->src, &status);
	dst->src_reg = loadInt8(loader->src, &status);
	dst->value = loadInt(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError load3RegOp(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->dst_reg = loadInt8(loader->src, &status);
	dst->src_reg = loadInt8(loader->src, &status);
	dst->src2_reg = loadInt8(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

BRBLoadError loadOpVar(ProgramLoader* loader, Op* dst)
{
	long status = 0;
	dst->var_size = loadInt8(loader->src, &status);

	if (!status) { return (BRBLoadError){ .code = BRB_ERR_NO_OP_ARG }; }
	return (BRBLoadError){0};
}

OpLoader op_loaders[] = {
	[OP_NONE] = &loadNoArgOp,
	[OP_END] = &loadNoArgOp,
	[OP_MARK] = &loadMarkOp,
	[OP_SET] = &loadRegImmOp,
	[OP_SETR] = &load2RegOp,
	[OP_SETD] = &loadRegNameOp,
	[OP_SETB] = &loadRegSymbolIdOp,
	[OP_SETM] = &loadRegNameOp,
	[OP_ADD] = &load2RegImmOp,
	[OP_ADDR] = &load3RegOp,
	[OP_SUB] = &load2RegImmOp,
	[OP_SUBR] = &load3RegOp,
	[OP_SYS] = &loadOpSyscall,
	[OP_GOTO] = &loadOpGoto,
	[OP_CMP] = &loadOpCmp,
	[OP_CMPR] = &loadOpCmpr,
	[OP_AND] = &load2RegImmOp,
	[OP_ANDR] = &load3RegOp,
	[OP_OR] = &load2RegImmOp,
	[OP_ORR] = &load3RegOp,
	[OP_NOT] = &load2RegOp,
	[OP_XOR] = &load2RegImmOp,
	[OP_XORR] = &load3RegOp,
	[OP_SHL] = &load2RegImmOp,
	[OP_SHLR] = &load3RegOp,
	[OP_SHR] = &load2RegImmOp,
	[OP_SHRR] = &load3RegOp,
	[OP_SHRS] = &load2RegImmOp,
	[OP_SHRSR] = &load3RegOp,
	[OP_PROC] = &loadMarkOp,
	[OP_CALL] = &loadOpCall,
	[OP_RET] = &loadNoArgOp,
	[OP_ENDPROC] = &loadNoArgOp,
	[OP_LD64] = &load2RegOp,
	[OP_STR64] = &load2RegOp,
	[OP_LD32] = &load2RegOp,
	[OP_STR32] = &load2RegOp,
	[OP_LD16] = &load2RegOp,
	[OP_STR16] = &load2RegOp,
	[OP_LD8] = &load2RegOp,
	[OP_STR8] = &load2RegOp,
	[OP_VAR] = &loadOpVar,
	[OP_SETV] = &loadRegSymbolIdOp, 
	[OP_MUL] = &load2RegImmOp,
	[OP_MULR] = &load3RegOp,
	[OP_DIV] = &load2RegImmOp,
	[OP_DIVR] = &load3RegOp,
	[OP_DIVS] = &load2RegImmOp,
	[OP_DIVSR] = &load3RegOp,
	[OP_EXTPROC] = &loadMarkOp
};
static_assert(N_OPS == sizeof(op_loaders) / sizeof(op_loaders[0]), "Some BRB operations have unmatched loaders");

BRBLoadError loadProgram(FILE* src, Program* dst, heapctx_t ctx)
{
	ProgramLoader loader = {
		.dst = dst,
		.src = src,
		.unresolved = fieldArray_new(ctx, 0)
	};

	long status = 0;
// loading stack size
	dst->stack_size = loadInt(src, &status);
	if (!dst->stack_size) {
		if (!status) return (BRBLoadError){
			.code = BRB_ERR_NO_STACK_SIZE
		};
		dst->stack_size = DEFAULT_STACK_SIZE;
	}
// loading data blocks
	int64_t n = loadInt(src, &status);
	if (!status) return (BRBLoadError){
		.code = BRB_ERR_NO_DATA_SEGMENT
	};
	dst->datablocks = DataBlockArray_new(ctx, absNegInt(n));
	dst->datablocks.length = n;
	for (int64_t i = 0; i < n; i++) {
		if (!loadName(&loader, &dst->datablocks.data[i].name)) return (BRBLoadError){
			.code = BRB_ERR_NO_BLOCK_NAME
		};
		dst->datablocks.data[i].spec.length = loadInt(src, &status);
		if (!status) return (BRBLoadError){
			.code = BRB_ERR_NO_BLOCK_SIZE
		};
		dst->datablocks.data[i].spec.data = ctxalloc_new(dst->datablocks.data[i].spec.length, ctx);
		if (!fread(
			dst->datablocks.data[i].spec.data, 
			dst->datablocks.data[i].spec.length,
			1,
			src
		)) return (BRBLoadError){
			.code = BRB_ERR_NO_BLOCK_SPEC
		};
	}
//  loading memory blocks
	n = loadInt(src, &status);
	if (!status) return (BRBLoadError){
		.code = BRB_ERR_NO_MEMORY_SEGMENT
	};
	dst->memblocks = MemBlockArray_new(ctx, absNegInt(n));
	dst->memblocks.length = n;
	for (int64_t i = 0; i < n; i++) {
		if (!loadName(&loader, &dst->memblocks.data[i].name)) return (BRBLoadError){
			.code = BRB_ERR_NO_BLOCK_NAME
		};
		
		dst->memblocks.data[i].size = loadInt(src, &status);
		if (!status) return (BRBLoadError){
			.code = BRB_ERR_NO_BLOCK_SIZE
		};
	}
//  loading operations
	n = loadInt(src, &status);
	if (!status) return (BRBLoadError){
		.code = BRB_ERR_NO_EXEC_SEGMENT
	};
	dst->execblock = OpArray_new(ctx, absNegInt(n));
	dst->execblock.length = n;
	for (int64_t i = 0; i < n; i++) {
		Op* op = dst->execblock.data + i;

		op->type = loadInt8(src, &status);
		if (!status) return (BRBLoadError){
			.code = BRB_ERR_NO_OPCODE
		};
		if (op->type < 0) {
			op->type = ~op->type;
			op->cond_id = loadInt8(src, &status);
			if (!status) return (BRBLoadError){
				.code = BRB_ERR_NO_COND_ID
			};
			if (!inRange(op->cond_id, 0, N_CONDS)) return (BRBLoadError){
				.code = BRB_ERR_INVALID_COND_ID,
				.cond_id = op->cond_id
			};
		}
		if (!inRange(op->type, 0, N_OPS)) return (BRBLoadError){
			.code = BRB_ERR_INVALID_OPCODE,
			.opcode = op->type
		};

		BRBLoadError err = op_loaders[op->type](&loader, op);
		if (err.code) return err;
	}
//  resolving symbol names
	n = loadInt(src, &status);
	if (!status) return (BRBLoadError){
		.code = BRB_ERR_NO_NAME_SEGMENT
	};
	for (int64_t i = 0; i < n; i++) {
		sbuf name;

		name.data = fgetln(src, (size_t*)&name.length);
		if (!name.length || !name.data) return (BRBLoadError){
			.code = BRB_ERR_NO_NAME_SPEC
		};
		name.length--;

		char* name_c = tostr(ctx, name);
		for (int64_t i1 = 0; i1 < loader.unresolved.length; i1++) {
			if (*loader.unresolved.data[i1] == (char*)i) {
				*loader.unresolved.data[i1] = name_c;
			}
		}
	}
// resolving references
	for (int64_t i = 0; i < dst->execblock.length; i++) {
		Op* op = dst->execblock.data + i;
		
		switch (op->type) {
			case OP_SETD:
				for (int64_t db_i = 0; db_i < dst->datablocks.length; db_i++) {
					if (streq(op->mark_name, dst->datablocks.data[db_i].name)) {
						op->symbol_id = db_i;
						break;
					}
				}
				break;
			case OP_SETM:
				for (int64_t mb_i = 0; mb_i < dst->memblocks.length; mb_i++) {
					if (streq(op->mark_name, dst->memblocks.data[mb_i].name)) {
						op->symbol_id = mb_i;
						break;
					}
				}
				break;
			case OP_CALL:
				for (int64_t proc_index = 0; proc_index < dst->execblock.length; proc_index++) {
					Op* proc = dst->execblock.data + proc_index;

					if (proc->type == OP_PROC || proc->type == OP_EXTPROC) {
						if (streq(proc->mark_name, op->mark_name)) {
							proc->symbol_id = proc_index;
							break;
						}
					}
				}
				break;
			default:
				break;
		}
	}
//  resolving entry
	dst->entry_opid = -1;
	for (int64_t i = 0; i < dst->execblock.length; i++) {
		Op* op = dst->execblock.data + i;
		if (op->type == OP_PROC || op->type == OP_EXTPROC) {
			dst->entry_opid = i;
			break;
		}
	}

	fieldArray_clear(&loader.unresolved);

	return (BRBLoadError){0};
}

ExecEnv initExecEnv(Program* program, int8_t flags, char** args)
{
	heapctx_t memctx = ctxalloc_newctx(0);
	ExecEnv res = {
		.heap = sctxalloc_new(0, memctx),
		.stack_brk = ctxalloc_new(program->stack_size, memctx),
		.exitcode = 0,
		.memblocks = sbufArray_new(memctx, program->memblocks.length * -1),
		.op_id = program->entry_opid,
		.registers = ctxalloc_new(sizeof(int64_t) * N_REGS, memctx),
		.flags = flags,
		.call_count = 0
	};
	res.prev_stack_head = res.stack_head = res.stack_brk + program->stack_size;
	memset(res.registers, 0, sizeof(int64_t) * N_REGS);

	sbuf* newblock;
	array_foreach(MemBlock, block, program->memblocks,
		newblock = sbufArray_append(&res.memblocks, sctxalloc_new(block.size, memctx));
		memset(newblock->data, 0, newblock->length);
	);

	if (flags & BRBX_TRACING) {
		res.regs_trace = ctxalloc_new(sizeof(DataSpec) * N_REGS, memctx);
		for (int i = 0; i < N_REGS; i++) {
			res.regs_trace[i].type = DS_VOID;
		}
		res.vars = ProcFrameArray_new(
			memctx, 1, 
			(ProcFrame){
				.call_id = 0,
				.prev_opid = 0,
				.vars = DataSpecArray_new(memctx, 0) 
			}
		);
	}

	res.exec_argc = 0;
	while (args[++res.exec_argc]) {}
	res.exec_argv = ctxalloc_new(res.exec_argc * sizeof(sbuf), memctx);
	for (int i = 0; i < res.exec_argc; i++) {
		res.exec_argv[i] = fromstr(args[i]);
		res.exec_argv[i].length++;
	}

	return res;
}

BufferRef classifyPtr(ExecEnv* env, Program* program, void* ptr)
{
	static_assert(N_BUF_TYPES == 5, "not all buffer types are handled");

	if (inRange(ptr, env->stack_head, env->stack_brk + program->stack_size - env->stack_head)) {
		return (BufferRef){ .type = BUF_VAR };
	}

	array_foreach(DataBlock, block, program->datablocks,
		if (inRange(ptr, block.spec.data, block.spec.data + block.spec.length)) {
			return ((BufferRef){ .type = BUF_DATA, .id = _block });
		}
	);

	array_foreach(sbuf, block, env->memblocks, 
		if (inRange(ptr, block.data, block.data + block.length)) {
			return ((BufferRef){ .type = BUF_MEMORY, .id = _block });
		}
	);

	for (int i = 0; i < env->exec_argc; i++) {
		if (inRange(ptr, env->exec_argv[i].data, env->exec_argv[i].data + env->exec_argv[i].length)) {
			return (BufferRef){ .type = BUF_ARGV, .id = i };
		}
	}
	return (BufferRef){0};
}

sbuf getBufferByRef(ExecEnv* env, Program* program, BufferRef ref)
{
	static_assert(N_BUF_TYPES == 5, "not all buffer types are handled");
	switch (ref.type) {
		case BUF_DATA: return program->datablocks.data[ref.id].spec;
		case BUF_MEMORY: return env->memblocks.data[ref.id];
		case BUF_VAR: return (sbuf){ .data = env->stack_head, .length = env->stack_brk + program->stack_size - env->stack_head };
		case BUF_ARGV: return env->exec_argv[ref.id];
		default: return (sbuf){0};
	}
}

void printDataSpec(FILE* fd, Program* program, ExecEnv* env, DataSpec spec, int64_t value)
{
	static_assert(N_DS_TYPES == 8, "not all data types are handled");
	static_assert(N_BUF_TYPES == 5, "not all buffer types are handled");
	switch (spec.type) {
		case DS_VOID: {
			fprintf(fd, "(void)\n");
			break;
		} case DS_BOOL: {
			fprintf(fd, "(bool)%s\n", value ? "true" : "false");
			break;
		} case DS_INT64: {
			fprintf(fd, "(int64_t)%lld\n", value);
			break;
		} case DS_INT32: {
			fprintf(fd, "(int32_t)%lld\n", value);
			break;
		} case DS_INT16: {
			fprintf(fd, "(int16_t)%lld\n", value);
			break;
		} case DS_INT8: {
			fprintf(fd, "(char)'");
			fputcesc(fd, value, BYTEFMT_HEX);
			fprintf(fd, "' // %lld\n", value);
			break;
		} case DS_PTR: {
			if (!spec.ref.type) {
				spec.ref = classifyPtr(env, program, (void*)value);
			}
			sbuf buf = getBufferByRef(env, program, spec.ref);
			switch (spec.ref.type) {
				case BUF_DATA:
					fprintf(fd, "(void*)%s ", program->datablocks.data[spec.ref.id].name);
					break;
				case BUF_MEMORY:
					fprintf(fd, "(void*)%s ", program->memblocks.data[spec.ref.id].name);
					break;
				case BUF_VAR:
					fprintf(fd, "(stackptr_t)sp ");
					break;
				case BUF_ARGV:
					fprintf(fd, "(char*)argv[%d] ", spec.ref.id);
					break;
				default:
					fprintf(fd, "(void*)%p\n", (void*)value);
					return;
			}

			if ((uint64_t)value < (uint64_t)buf.data) {
				fprintf(fd, "- %llu ", (uint64_t)buf.data - (uint64_t)value);
			} else if ((uint64_t)value > (uint64_t)buf.data) {
				fprintf(fd, "+ %llu ", (uint64_t)value - (uint64_t)buf.data);
			}
			fprintf(fd, "// %p\n", (void*)value);
			break;
		} case DS_CONST: {
			fprintf(fd, "(const int)%s // %lld\n", builtins[spec.symbol_id].name, value);
			break;
		}
	}
}

void printExecState(FILE* fd, ExecEnv* env, Program* program)
{
	if (env->flags & BRBX_TRACING) {
		fprintf(fd, "local stack frame:\n");
		void* cur_stack_pos = env->prev_stack_head;
		static_assert(N_DS_TYPES == 8, "not all tracer types are handled");

		array_foreach(DataSpec, spec, arrayhead(env->vars)->vars,
			int64_t input = 0;
			switch (spec.type) {
				case DS_VOID:
					cur_stack_pos -= spec.size;
					break;
				case DS_INT64:
				case DS_PTR:
					input = *(int64_t*)(cur_stack_pos -= 8);
					break;
				case DS_INT32:
				case DS_CONST:
					input = (int64_t)*(int32_t*)(cur_stack_pos -= 4);
					break;
				case DS_INT16:
					cur_stack_pos -= 2;
					input = (int64_t)*(int16_t*)(cur_stack_pos -= 2);
					break;
				case DS_BOOL:
				case DS_INT8:
					input = (int64_t)*(int8_t*)(--cur_stack_pos);
					break;
			}
			fprintf(fd, "\t[sp + %ld] ", cur_stack_pos - env->stack_head);
			printDataSpec(fd, program, env, spec, input);
		);
		printf("\t[end]\n");
		fprintf(
			fd, 
			"total stack usage: %.3f/%.3f Kb (%.3f%%)\n",
			(float)(env->stack_brk + program->stack_size - env->stack_head) / 1024.0f,
			DEFAULT_STACK_SIZE / 1024.0f,
			(float)(env->stack_brk + program->stack_size - env->stack_head)	/ (float)DEFAULT_STACK_SIZE * 100.0f
		);

		fprintf(fd, "registers:\n");
		for (char i = 0; i < N_USER_REGS; i++) {
			fprintf(fd, "\t[%hhd]\t", i);
			printDataSpec(fd, program, env, env->regs_trace[i], env->registers[i]);
		}

		if (env->flags & BRBX_PRINT_MEMBLOCKS) {
			printf("memory blocks:\n");
			array_foreach(sbuf, block, env->memblocks, 
				printf("\t%s: \"", program->memblocks.data[_block].name);
				putsbufesc(block, BYTEFMT_HEX | BYTEFMT_ESC_DQUOTE);
				printf("\"\n");
			);
		}
        printf("internal error code: %d (%s)\n", errno, errno ? strerror(errno) : "OK");
	}
}

BufferRef validateMemoryAccess(ExecEnv* env, Program* program, DataSpec spec, void* ptr, int64_t size)
{
	static_assert(N_BUF_TYPES == 5, "not all buffer types are handled");

	if (size < 0) {
		env->exitcode = EC_NEGATIVE_SIZE_ACCESS;
		env->err_ptr = ptr,
		env->err_access_length = size;
		return (BufferRef){0};
	}

	BufferRef ref;
	if (spec.type == DS_PTR) {
		ref = spec.ref;
	} else {
		ref = classifyPtr(env, program, ptr);
	}

	if (!ref.type) {
		env->exitcode = EC_ACCESS_FAILURE;
		env->err_access_length = size;
		env->err_ptr = ptr;
		return (BufferRef){0};
	}

	if (ref.type == BUF_VAR && ref.id != env->call_count) {
		env->exitcode = EC_OUTDATED_LOCALPTR;
		env->err_ptr = ptr;
		return (BufferRef){0};
	}

	sbuf buf = getBufferByRef(env, program, ref);
	if (!isSlice(ptr, size, buf.data, buf.length)) {
		env->exitcode = EC_ACCESS_MISALIGNMENT;
		env->err_buf_ref = ref;
		env->err_access_length = size;
		env->err_ptr = ptr;
		return (BufferRef){0};
	}

	return ref;
}

DataSpec getStackSpec(ExecEnv* env, void* ptr, int size)
{
	void* cur_stack_pos = env->stack_head;
	array_rev_foreach(DataSpec, spec, arrayhead(env->vars)->vars,
		int spec_size = dataSpecSize(spec);
		if (inRange(ptr, cur_stack_pos, cur_stack_pos + spec_size)) {
			if (ptr == cur_stack_pos && size <= spec_size) { return spec; }
			env->exitcode = EC_ACCESS_FAILURE;
			env->err_ptr = ptr;
			env->err_access_length = size;
			return (DataSpec){.type = DS_VOID};
		}
		cur_stack_pos += spec_size;
	);
	env->exitcode = EC_ACCESS_FAILURE;
	env->err_ptr = ptr;
	env->err_access_length = size;
	return (DataSpec){.type = DS_VOID};
}

bool setStackSpec(ExecEnv* env, void* ptr, DataSpec spec)
{
	void* cur_stack_pos = env->stack_head;
	int spec_size = dataSpecSize(spec);
	array_rev_foreach(DataSpec, cur_spec, arrayhead(env->vars)->vars,
		int cur_spec_size = dataSpecSize(cur_spec);
		if (inRange(ptr, cur_stack_pos, cur_stack_pos + cur_spec_size)) {
			if (ptr == cur_stack_pos) {
				if (spec_size == cur_spec_size) {
					arrayhead(env->vars)->vars.data[_cur_spec] = spec;
					return true;
				} else if (spec_size < cur_spec_size) {
					arrayhead(env->vars)->vars.data[_cur_spec] = intSpecFromSize(cur_spec_size);
					return true;
				}
			}
			return false;
		}
		cur_stack_pos += cur_spec_size;
	);
	return false;
}

bool handleInvalidSyscall(ExecEnv* env, Program* program)
{
	env->exitcode = EC_UNKNOWN_SYSCALL;
	return true;
}

bool handleExitSyscall(ExecEnv* env, Program* program)
{
	env->exitcode = env->registers[0];
	return true;
}

bool handleWriteSyscall(ExecEnv* env, Program* program)
{
	if (env->flags & BRBX_TRACING) {
		env->regs_trace[0].type = DS_INT64;
	}

	env->registers[0] = write(env->registers[0], (char*)env->registers[1], env->registers[2]);

	env->op_id++;
	return false;
}

bool handleArgcSyscall(ExecEnv* env, Program* program)
{
	if (env->flags & BRBX_TRACING) {
		env->regs_trace[0].type = DS_INT32;
	}

	env->registers[0] = env->exec_argc;
	env->op_id++;
	return false;
}

bool handleArgvSyscall(ExecEnv* env, Program* program)
{
	if (env->flags & BRBX_TRACING) {
		env->regs_trace[0] = (DataSpec){ 
			.type = DS_PTR,
			.ref = (BufferRef){
				.type = BUF_ARGV,
				.id = env->registers[0]
			} 
		};
	}

	env->registers[0] = inRange(env->registers[0], 0, env->exec_argc) ? (uint64_t)env->exec_argv[env->registers[0]].data : 0;
	env->op_id++;
	return false;
}

bool handleReadSyscall(ExecEnv* env, Program* program)
{
	if (env->flags & BRBX_TRACING) {
		env->regs_trace[0].type = DS_INT64;
	}

    env->registers[0] = read(env->registers[0], (void*)env->registers[1], env->registers[2]);

    env->op_id++;
    return false;
}

bool handleGetErrnoSyscall(ExecEnv* env, Program* program)
{
    if (env->flags & BRBX_TRACING) {
        env->regs_trace[0].type = DS_INT32;
    }

    env->registers[0] = errno;

    env->op_id++;
    return false;
}

bool handleSetErrnoSyscall(ExecEnv* env, Program* program)
{
    errno = env->registers[0];
    env->op_id++;
    return false;
}

typedef bool (*ExecHandler) (ExecEnv*, Program*);

ExecHandler syscall_handlers[] = {
	[SYS_OP_INVALID] = &handleInvalidSyscall,
	[SYS_OP_EXIT] = &handleExitSyscall,
	[SYS_OP_WRITE] = &handleWriteSyscall,
	[SYS_OP_ARGC] = &handleArgcSyscall,
	[SYS_OP_ARGV] = &handleArgvSyscall,
    [SYS_OP_READ] = &handleReadSyscall,
    [SYS_OP_GET_ERRNO] = &handleGetErrnoSyscall,
    [SYS_OP_SET_ERRNO] = &handleSetErrnoSyscall
};
static_assert(N_SYS_OPS == sizeof(syscall_handlers) / sizeof(syscall_handlers[0]), "not all system calls have matching handlers");

bool handleCondition(ExecEnv* env, ConditionCode cond_id) {
	switch (cond_id) {
		case COND_NON: return true;
		case COND_EQU: return env->registers[CONDREG1_ID] == env->registers[CONDREG2_ID];
		case COND_NEQ: return env->registers[CONDREG1_ID] != env->registers[CONDREG2_ID];
		case COND_LTU: return env->registers[CONDREG1_ID] <  env->registers[CONDREG2_ID];
		case COND_GTU: return env->registers[CONDREG1_ID] >  env->registers[CONDREG2_ID];
		case COND_LEU: return env->registers[CONDREG1_ID] <= env->registers[CONDREG2_ID];
		case COND_GEU: return env->registers[CONDREG1_ID] >= env->registers[CONDREG2_ID];
		case COND_LTS: return (int64_t)env->registers[CONDREG1_ID] <  (int64_t)env->registers[CONDREG2_ID];
		case COND_GTS: return (int64_t)env->registers[CONDREG1_ID] >  (int64_t)env->registers[CONDREG2_ID];
		case COND_LES: return (int64_t)env->registers[CONDREG1_ID] <= (int64_t)env->registers[CONDREG2_ID];
		case COND_GES: return (int64_t)env->registers[CONDREG1_ID] >= (int64_t)env->registers[CONDREG2_ID];
		case N_CONDS: return false;
	}
}

bool handleNop(ExecEnv* env, Program* program)
{
	env->op_id++;
	return false;
}

bool handleOpEnd(ExecEnv* env, Program* program)
{
	env->exitcode = EC_OK;
	return true;
}

bool handleOpSet(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = op.value;

	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg].type = DS_INT64;
	}

	env->op_id++;
	return false;
}

bool handleOpSetr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg];

	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
	}

	env->op_id++;
	return false;
}

bool handleOpSetd(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = (int64_t)program->datablocks.data[op.symbol_id].spec.data;

	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg] = (DataSpec){ 
			.type = DS_PTR,
			.ref = (BufferRef){ 
				.type = BUF_DATA,
				.id = op.symbol_id
			}
		};
	}

	env->op_id++;
	return false;
}

bool handleOpSetb(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = builtins[op.symbol_id].value;

	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg] = (DataSpec){ .type = DS_CONST, .symbol_id = op.symbol_id };
	}

	env->op_id++;
	return false;
}

bool handleOpSetm(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = (int64_t)env->memblocks.data[op.symbol_id].data;

	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg] = (DataSpec){ 
			.type = DS_PTR,
			.ref = (BufferRef){
				.type = BUF_MEMORY,
				.id = op.symbol_id
			}
		};
	}

	env->op_id++;
	return false;
}

bool handleOpAdd(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] + op.value;

	if (env->flags &  BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
				break;
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpAddr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] + env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				if (isIntSpec(env->regs_trace[op.src2_reg])) {
					env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
					break;
				}
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpSub(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] - op.value;

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
				break;
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpSubr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] - env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				if (isIntSpec(env->regs_trace[op.src2_reg])) {
					env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
					break;
				}
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpSyscall(ExecEnv* env, Program* program)
{
	return syscall_handlers[program->execblock.data[env->op_id].syscall_id](env, program);
}

bool handleOpGoto(ExecEnv* env, Program* program)
{
	env->op_id = program->execblock.data[env->op_id].symbol_id;
	return false;
}

bool handleOpCmp(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[CONDREG1_ID] = env->registers[op.src_reg];
	env->registers[CONDREG2_ID] = op.value;
	env->op_id++;
	return false;
}

bool handleOpCmpr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[CONDREG1_ID] = env->registers[op.src_reg];
	env->registers[CONDREG2_ID] = env->registers[op.src2_reg];
	env->op_id++;
	return false;
}

bool handleOpAnd(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] & op.value;

	if (env->flags & BRBX_TRACING) {
		if (isIntSpec(env->regs_trace[op.src_reg])) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpAndr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] & env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		bool left_reg_int = isIntSpec(env->regs_trace[op.src_reg]), right_reg_int = isIntSpec(env->regs_trace[op.src2_reg]);
		if (left_reg_int && right_reg_int) { // TODO: proper estimation of data size
			if (env->regs_trace[op.src_reg].type > env->regs_trace[op.src2_reg].type) {
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src2_reg];
			} else {
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
			}
		} else if (left_reg_int) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
		} else if (right_reg_int) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src2_reg];
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpOr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] | op.value;

	if (env->flags & BRBX_TRACING) {
		if (isIntSpec(env->regs_trace[op.src_reg])) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpOrr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] | env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		bool left_reg_int = isIntSpec(env->regs_trace[op.src_reg]), right_reg_int = isIntSpec(env->regs_trace[op.src2_reg]);
		if (left_reg_int && right_reg_int) {
			if (env->regs_trace[op.src_reg].type < env->regs_trace[op.src2_reg].type) {
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src2_reg];
			} else {
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
			}
		} else if (left_reg_int) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
		} else if (right_reg_int) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src2_reg];
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpNot(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = ~env->registers[op.src_reg];

	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg].type = DS_INT64;
	}

	env->op_id++;
	return false;
}

bool handleOpXor(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] ^ op.value;

	if (env->flags & BRBX_TRACING) {
		if (isIntSpec(env->regs_trace[op.src_reg])) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpXorr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] ^ env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		bool left_reg_int = isIntSpec(env->regs_trace[op.src_reg]), right_reg_int = isIntSpec(env->regs_trace[op.src2_reg]);
		if (left_reg_int && right_reg_int) {
			if (env->regs_trace[op.src_reg].type < env->regs_trace[op.src2_reg].type) {
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src2_reg];
			} else {
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
			}
		} else if (left_reg_int) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
		} else if (right_reg_int) {
			env->regs_trace[op.dst_reg] = env->regs_trace[op.src2_reg];
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpShl(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] << op.value;

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
				break;
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpShlr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] << env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				if (isIntSpec(env->regs_trace[op.src2_reg])) {
					env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
					break;
				}
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpShr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] >> op.value;

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
				break;
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpShrr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = env->registers[op.src_reg] >> env->registers[op.src2_reg];

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				if (isIntSpec(env->regs_trace[op.src2_reg])) {
					env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
					break;
				}
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpShrs(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = (int64_t)env->registers[op.src_reg] >> op.value;

	if (env->flags & BRBX_TRACING) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
				break;
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpShrsr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	env->registers[op.dst_reg] = (int64_t)env->registers[op.src_reg] >> env->registers[op.src2_reg];

	if (env->flags & (BRBX_TRACING)) {
		switch (env->regs_trace[op.src_reg].type) {
			case DS_PTR:
				if (isIntSpec(env->regs_trace[op.src2_reg])) {
					env->regs_trace[op.dst_reg] = env->regs_trace[op.src_reg];
					break;
				}
			default:
				if (env->registers[op.dst_reg] >= (1L << 32)) {
					env->regs_trace[op.dst_reg].type = DS_INT64;
				} else if (env->registers[op.dst_reg] >= (1 << 16)) {
					env->regs_trace[op.dst_reg].type = DS_INT32;
				} else if (env->registers[op.dst_reg] >= (1 << 8)) {
					env->regs_trace[op.dst_reg].type = DS_INT16;
				} else {
					env->regs_trace[op.dst_reg].type = DS_INT8;
				}
				break;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpCall(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	if (env->flags & BRBX_TRACING) {
		if (env->stack_brk > env->stack_head - 16) {
			env->exitcode = EC_STACK_OVERFLOW;
			env->err_push_size = 16;
			return true;
		}

		ProcFrameArray_append(
			&env->vars,
			(ProcFrame){
				.call_id = ++env->call_count,
				.prev_opid = env->op_id,
				.vars = DataSpecArray_new(envctx(env), 0)
			}
		);
	}

	env->stack_head -= sizeof(env->op_id);
	*(int64_t*)env->stack_head = env->op_id + 1;

	env->stack_head -= sizeof(env->prev_stack_head);
	*(void**)env->stack_head = env->prev_stack_head;
	env->prev_stack_head = env->stack_head;

	env->op_id = op.symbol_id;
	return false;
}

bool handleOpRet(ExecEnv* env, Program* program)
{
	if (env->flags & BRBX_TRACING) {
		if (env->stack_head + 16 > env->stack_brk + program->stack_size) {
			env->exitcode = EC_STACK_UNDERFLOW;
			env->err_pop_size = 16;
			return true;
		}

		if (env->vars.length == 1) {
			env->exitcode = EC_NO_STACKFRAME;
			return true;
		}
		env->vars.length--;
	}

	env->stack_head = env->prev_stack_head;
	env->prev_stack_head = *(void**)env->stack_head;
	env->stack_head += sizeof(env->prev_stack_head);

	env->op_id = *(int64_t*)env->stack_head;
	env->stack_head += sizeof(env->op_id);

	return false;
}

bool handleOpLd64(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.src_reg], (void*)env->registers[op.src_reg], 8);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			env->regs_trace[op.dst_reg] = getStackSpec(env, (void*)env->registers[op.src_reg], 8);
			if (env->regs_trace[op.dst_reg].type == DS_VOID) {
				if (!env->exitcode) {
					env->exitcode = EC_UNDEFINED_STACK_LOAD;
					env->err_ptr = (void*)env->registers[op.src_reg];
				}
				return true;
			}
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		}
	}

	env->registers[op.dst_reg] = *(int64_t*)env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpStr64(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.dst_reg], (void*)env->registers[op.dst_reg], 8);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			if (!setStackSpec(
				env,
				(void*)env->registers[op.dst_reg],
				dataSpecSize(env->regs_trace[op.src_reg]) == 8 ? env->regs_trace[op.src_reg] : (DataSpec){.type = DS_INT64}
			)) {
				env->exitcode = EC_ACCESS_FAILURE;
				env->err_ptr = (void*)env->registers[op.dst_reg];
				env->err_access_length = 8;
				return true;
			}
		}
	}

	*(int64_t*)env->registers[op.dst_reg] = env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpLd32(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.src_reg], (void*)env->registers[op.src_reg], 4);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			env->regs_trace[op.dst_reg] = getStackSpec(env, (void*)env->registers[op.src_reg], 4);
			if (env->regs_trace[op.dst_reg].type == DS_VOID) {
				if (!env->exitcode) {
					env->exitcode = EC_UNDEFINED_STACK_LOAD;
					env->err_ptr = (void*)env->registers[op.src_reg];
				}
				return true;
			}
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		}
	}

	env->registers[op.dst_reg] = *(int32_t*)env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpStr32(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.dst_reg], (void*)env->registers[op.dst_reg], 4);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			if (!setStackSpec(
				env,
				(void*)env->registers[op.dst_reg], 
				dataSpecSize(env->regs_trace[op.src_reg]) == 4 ? env->regs_trace[op.src_reg] : (DataSpec){.type = DS_INT32}
			)) {
				env->exitcode = EC_ACCESS_FAILURE;
				env->err_ptr = (void*)env->registers[op.dst_reg];
				env->err_access_length = 4;
				return true;
			}
		}
	}

	*(int32_t*)env->registers[op.dst_reg] = env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpLd16(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.src_reg], (void*)env->registers[op.src_reg], 2);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			env->regs_trace[op.dst_reg] = getStackSpec(env, (void*)env->registers[op.src_reg], 2);
			if (env->regs_trace[op.dst_reg].type == DS_VOID) {
				if (!env->exitcode) {
					env->exitcode = EC_UNDEFINED_STACK_LOAD;
					env->err_ptr = (void*)env->registers[op.src_reg];
				}
				return true;
			}
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		}
	}

	env->registers[op.dst_reg] = *(int16_t*)env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpStr16(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.dst_reg], (void*)env->registers[op.dst_reg], 2);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			if (!setStackSpec(
				env,
				(void*)env->registers[op.dst_reg], 
				dataSpecSize(env->regs_trace[op.src_reg]) == 2 ? env->regs_trace[op.src_reg] : (DataSpec){.type = DS_INT16}
			)) {
				env->exitcode = EC_ACCESS_FAILURE;
				env->err_ptr = (void*)env->registers[op.dst_reg];
				env->err_access_length = 2;
				return true;
			}
		}
	}

	*(int16_t*)env->registers[op.dst_reg] = env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpLd8(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.src_reg], (void*)env->registers[op.src_reg], 1);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			env->regs_trace[op.dst_reg] = getStackSpec(env, (void*)env->registers[op.src_reg], 1);
			if (env->regs_trace[op.dst_reg].type == DS_VOID) {
				if (!env->exitcode) {
					env->exitcode = EC_UNDEFINED_STACK_LOAD;
					env->err_ptr = (void*)env->registers[op.src_reg];
				}
				return true;
			}
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->registers[op.dst_reg] = *(int8_t*)env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpStr8(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		BufferRef ref = validateMemoryAccess(env, program, env->regs_trace[op.dst_reg], (void*)env->registers[op.dst_reg], 1);
		if (!ref.type) {
			return true;
		} else if (ref.type == BUF_VAR) {
			if (!setStackSpec(
				env,
				(void*)env->registers[op.dst_reg], 
				dataSpecSize(env->regs_trace[op.src_reg]) == 1 ? env->regs_trace[op.src_reg] : (DataSpec){.type = DS_INT8}
			)) {
				env->exitcode = EC_ACCESS_FAILURE;
				env->err_ptr = (void*)env->registers[op.dst_reg];
				env->err_access_length = 1;
				return true;
			}
		}
	}

	*(int8_t*)env->registers[op.dst_reg] = env->registers[op.src_reg];
	env->op_id++;
	return false;
}

bool handleOpVar(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	if (env->flags & BRBX_TRACING) {
		DataSpecArray_append(
			&arrayhead(env->vars)->vars, 
			(DataSpec){
				.type = DS_VOID,
				.size = op.var_size
			}
		);
	}

	env->stack_head -= op.var_size;
	env->op_id++;
	return false;
}

bool handleOpSetv(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];
	
	if (env->flags & BRBX_TRACING) {
		env->regs_trace[op.dst_reg] = (DataSpec){
			.type = DS_PTR,
			.ref = (BufferRef){
				.type = BUF_VAR,
				.id = arrayhead(env->vars)->call_id
			}
		};
	}

	env->registers[op.dst_reg] = (int64_t)env->stack_head + op.symbol_id;
	env->op_id++;
	return false;
}

bool handleOpMul(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[op.dst_reg] = env->registers[op.src_reg] * (uint64_t)op.value;
	
	if (env->flags & BRBX_TRACING) {
		if (env->registers[op.dst_reg] >= (1L << 32)) {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		} else if (env->registers[op.dst_reg] >= (1 << 16)) {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		} else if (env->registers[op.dst_reg] >= (1 << 8)) {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpMulr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[op.dst_reg] = env->registers[op.src_reg] * env->registers[op.src2_reg];
	
	if (env->flags & BRBX_TRACING) {
		if (env->registers[op.dst_reg] >= (1L << 32)) {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		} else if (env->registers[op.dst_reg] >= (1 << 16)) {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		} else if (env->registers[op.dst_reg] >= (1 << 8)) {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpDiv(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[op.dst_reg] = env->registers[op.src_reg] / (uint64_t)op.value;
	
	if (env->flags & BRBX_TRACING) {
		if (!op.value) {
			env->exitcode = EC_ZERO_DIVISION;
			return true;
		}

		if (env->registers[op.dst_reg] >= (1L << 32)) {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		} else if (env->registers[op.dst_reg] >= (1 << 16)) {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		} else if (env->registers[op.dst_reg] >= (1 << 8)) {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpDivr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[op.dst_reg] = env->registers[op.src_reg] / env->registers[op.src2_reg];
	
	if (env->flags & BRBX_TRACING) {
		if (!env->registers[op.src2_reg]) {
			env->exitcode = EC_ZERO_DIVISION;
			return true;
		}

		if (env->registers[op.dst_reg] >= (1L << 32)) {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		} else if (env->registers[op.dst_reg] >= (1 << 16)) {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		} else if (env->registers[op.dst_reg] >= (1 << 8)) {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpDivs(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[op.dst_reg] = (int64_t)env->registers[op.src_reg] / op.value;
	
	if (env->flags & BRBX_TRACING) {
		if (!op.value) {
			env->exitcode = EC_ZERO_DIVISION;
			return true;
		}

		if (env->registers[op.dst_reg] >= (1L << 32)) {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		} else if (env->registers[op.dst_reg] >= (1 << 16)) {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		} else if (env->registers[op.dst_reg] >= (1 << 8)) {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->op_id++;
	return false;
}

bool handleOpDivsr(ExecEnv* env, Program* program)
{
	Op op = program->execblock.data[env->op_id];

	env->registers[op.dst_reg] = (int64_t)env->registers[op.src_reg] / (int64_t)env->registers[op.src2_reg];
	
	if (env->flags & BRBX_TRACING) {
		if (!env->registers[op.src2_reg]) {
			env->exitcode = EC_ZERO_DIVISION;
			return true;
		}

		if (env->registers[op.dst_reg] >= (1L << 32)) {
			env->regs_trace[op.dst_reg].type = DS_INT64;
		} else if (env->registers[op.dst_reg] >= (1 << 16)) {
			env->regs_trace[op.dst_reg].type = DS_INT32;
		} else if (env->registers[op.dst_reg] >= (1 << 8)) {
			env->regs_trace[op.dst_reg].type = DS_INT16;
		} else {
			env->regs_trace[op.dst_reg].type = DS_INT8;
		}
	}

	env->op_id++;
	return false;
}

ExecHandler op_handlers[] = {
	[OP_NONE] = &handleNop,
	[OP_END] = &handleOpEnd,
	[OP_MARK] = &handleNop,
	[OP_SET] = &handleOpSet,
	[OP_SETR] = &handleOpSetr,
	[OP_SETD] = &handleOpSetd,
	[OP_SETB] = &handleOpSetb,
	[OP_SETM] = &handleOpSetm,
	[OP_ADD] = &handleOpAdd,
	[OP_ADDR] = &handleOpAddr,
	[OP_SUB] = &handleOpSub,
	[OP_SUBR] = &handleOpSubr,
	[OP_SYS] = &handleOpSyscall,
	[OP_GOTO] = &handleOpGoto,
	[OP_CMP] = &handleOpCmp,
	[OP_CMPR] = &handleOpCmpr,
	[OP_AND] = &handleOpAnd,
	[OP_ANDR] = &handleOpAndr,
	[OP_OR] = &handleOpOr,
	[OP_ORR] = &handleOpOrr,
	[OP_NOT] = &handleOpNot,
	[OP_XOR] = &handleOpXor,
	[OP_XORR] = &handleOpXorr,
	[OP_SHL] = &handleOpShl,
	[OP_SHLR] = &handleOpShlr,
	[OP_SHR] = &handleOpShr,
	[OP_SHRR] = &handleOpShrr,
	[OP_SHRS] = &handleOpShrs,
	[OP_SHRSR] = &handleOpShrsr,
	[OP_PROC] = &handleNop,
	[OP_CALL] = &handleOpCall,
	[OP_RET] = &handleOpRet,
	[OP_ENDPROC] = &handleNop,
	[OP_LD64] = &handleOpLd64,
	[OP_STR64] = &handleOpStr64,
	[OP_LD32] = &handleOpLd32,
	[OP_STR32] = &handleOpStr32,
	[OP_LD16] = &handleOpLd16,
	[OP_STR16] = &handleOpStr16,
	[OP_LD8] = &handleOpLd8,
	[OP_STR8] = &handleOpStr8,
	[OP_VAR] = &handleOpVar,
	[OP_SETV] = &handleOpSetv,
	[OP_MUL] = &handleOpMul,
	[OP_MULR] = &handleOpMulr,
	[OP_DIV] = &handleOpDiv,
	[OP_DIVR] = &handleOpDivr,
	[OP_DIVS] = &handleOpDivs,
	[OP_DIVSR] = &handleOpDivsr,
	[OP_EXTPROC] = &handleNop
};
static_assert(N_OPS == sizeof(op_handlers) / sizeof(op_handlers[0]), "Some BRB operations have unmatched execution handlers");

void printRuntimeError(FILE* fd, ExecEnv* env, Program* program)
{
	switch (env->exitcode) {
		case EC_NO_STACKFRAME:
			fprintf(fd,"%llx:\n\truntime error: attempted to return from a global stack frame\n", env->op_id);
			break;
		case EC_ZERO_DIVISION:
			fprintf(fd, "%llx:\n\truntime error: attempted to divide by zero\n", env->op_id);
			break;
		case EC_OUTDATED_LOCALPTR:
			fprintf(fd, "%llx:\n\truntime error: attempted to use an outdated stack pointer\n", env->op_id);
			fprintf(fd, "\tpointer %p references already deallocated stack frame\n", env->err_ptr);
			break;
		case EC_UNDEFINED_STACK_LOAD:
			fprintf(fd, "%llx:\n\truntime error: attempted to read from an unused stack variable\n", env->op_id);
			fprintf(fd, "\tstack variable with undefined value is at %p\n", env->err_ptr);
			break;
		case EC_ACCESS_FAILURE:
			fprintf(fd,"%llx:\n\truntime error: memory access failure\n", env->op_id);
			fprintf(fd,
				"\tattempted to access %lld bytes from %p, which is not in bounds of the stack, the heap or any of the buffers\n",
				env->err_access_length,
				env->err_ptr
			);
			break;
		case EC_NEGATIVE_SIZE_ACCESS:
			fprintf(fd,"%llx:\n\truntime error: negative sized memory access\n", env->op_id);
			fprintf(fd,
				"\tattempted to access %lld bytes at %p; accessing memory by negative size is not allowed\n",
				env->err_access_length, 
				env->err_ptr
			);
			break;
		case EC_STACK_OVERFLOW:
			fprintf(fd,"%llx:\n\truntime error: stack overflow\n", env->op_id);
			fprintf(fd,"\tattempted to expand the stack by %hhd bytes, overflowing the stack\n", env->err_push_size);
			break;
		case EC_STACK_UNDERFLOW:
			fprintf(fd,"%llx:\n\truntime error: stack underflow\n", env->op_id);
			fprintf(fd,"\tattempted to decrease the stack by %hhd bytes, underflowing the stack\n", env->err_pop_size);
			break;
		case EC_UNKNOWN_SYSCALL:
			fprintf(fd,"%llx:\n\truntime error: invalid system call\n", env->op_id - 1);
			break;
		case EC_ACCESS_MISALIGNMENT:
			fprintf(fd,"%llx:\n\truntime error: misaligned memory access\n", env->op_id);
			sbuf err_buf = getBufferByRef(env, program, env->err_buf_ref);
			static_assert(N_BUF_TYPES == 5, "not all buffer types are handled");
			switch (env->err_buf_ref.type) {
				case BUF_DATA:
					fprintf(fd,
						"\tdata block `%s` is at %p\n\tblock size: %ld bytes\n",
						program->datablocks.data[env->err_buf_ref.id].name,
						(void*)err_buf.data,
						err_buf.length
					);
					break;
				case BUF_MEMORY:
					fprintf(fd,
						"\tmemory block `%s` is at %p\n\tblock size: %ld bytes\n",
						program->memblocks.data[env->err_buf_ref.id].name,
						(void*)err_buf.data,
						err_buf.length
					);
					break;
				case BUF_VAR:
					fprintf(fd,"\tstack head is at %p\n\tstack size: %ld bytes\n", err_buf.data, err_buf.length);
					break;
				case BUF_ARGV:
					fprintf(fd,"\targument #%d is at %p\n\targument length: %ld bytes\n", env->err_buf_ref.id, err_buf.data, err_buf.length);
					break;
				default: break;
			}
			fprintf(fd,"\tattempted to access %lld bytes from %p\n", env->err_access_length, env->err_ptr);
			if (env->err_ptr < (void*)err_buf.data) {
				fprintf(fd,"\tthe accessed field is %lld bytes before the start of the buffer\n", (int64_t)(err_buf.data - (int64_t)env->err_ptr));
			} else if ((void*)(env->err_ptr + env->err_access_length) > (void*)(err_buf.data + err_buf.length)) {
				fprintf(fd,
					"\tthe accessed field exceeds the buffer by %lld bytes\n", 
					(int64_t)(env->err_ptr + env->err_access_length - ((int64_t)err_buf.data + err_buf.length))
				);
			}
			break;
	}
}

ExecEnv execProgram(Program* program, int8_t flags, char** args, volatile bool* interruptor)
{
	ExecEnv env = initExecEnv(program, flags, args);
	while (true) {
		if (program->execblock.data[env.op_id].cond_id) {
			if (!handleCondition(&env, program->execblock.data[env.op_id].cond_id)) {
				env.op_id++;
				continue;
			}
		}
		if (interruptor ? *interruptor : false) break;
		if (op_handlers[program->execblock.data[env.op_id].type](&env, program)) break;
	}
	return env;
}
