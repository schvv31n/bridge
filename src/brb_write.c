// implementation for writing BRB modules into `.brb` files
#include <brb.h>

defArray(BRB_Op);
defArray(BRB_DataPiece);
defArray(BRB_DataBlock);

typedef const char* str;
declArray(str);
defArray(str);
typedef struct {
	BRB_Module* src;
	FILE* dst;
	strArray names;
} BRB_ModuleWriter;

static inline int writeInt8(FILE* fd, uint8_t x)
{
	return fwrite(&x, 1, 1, fd);
}

static inline int writeInt16(FILE* fd, uint16_t x)
{
	return fwrite(BRByteOrder(&x, 2), 1, 2, fd);
}

static inline int writeInt32(FILE* fd, uint32_t x)
{
	return fwrite(BRByteOrder(&x, 4), 1, 4, fd);
}

static inline int writeInt64(FILE* fd, uint64_t x)
{
	return fwrite(BRByteOrder(&x, 8), 1, 8, fd);
}

static long writeInt(FILE* fd, uint64_t x, uint8_t hb)
{
	if (x < 8) {
		return writeInt8(fd, x | hb << 4);
	} else if (FITS_IN_8BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 12 : 8) | hb << 4)
			+ writeInt8(fd, absInt(x));
	} else if (FITS_IN_16BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9) | hb << 4)
			+ writeInt16(fd, absInt(x));
	} else if (FITS_IN_32BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10) | hb << 4)
			+ writeInt32(fd, absInt(x));
	} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11) | hb << 4)
			+ writeInt64(fd, absInt(x));
}

static long writeIntOnly(FILE* fd, uint64_t x)
{
	if (FITS_IN_8BITS(x)) {
		return inRange(x, 8, 16)
			? writeInt8(fd, SIGN_BIT_SET(x) ? 12 : 8)
				+ writeInt8(fd, absInt(x))
			: writeInt8(fd, x);
	} else if (FITS_IN_16BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9))
			+ writeInt16(fd, absInt(x));
	} else if (FITS_IN_32BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10))
			+ writeInt32(fd, absInt(x));
	} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11))
			+ writeInt64(fd, absInt(x));
}

static long writeIntSwapped(FILE* fd, uint64_t x, uint8_t hb)
{
	if (FITS_IN_8BITS(x)) {
		return x < 8
			? writeInt8(fd, hb | x << 4)
			: writeInt8(fd, (SIGN_BIT_SET(x) ? 12 : 8) << 4 | hb)
				+ writeInt8(fd, absInt(x));
	} else if (FITS_IN_16BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9) << 4 | hb)
			+ writeInt16(fd, absInt(x));
	} else if (FITS_IN_32BITS(x)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10) << 4 | hb)
			+ writeInt32(fd, absInt(x));
	} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11) << 4 | hb)
			+ writeInt64(fd, absInt(x));
}

static long write2Ints(FILE* fd, uint64_t x, uint64_t y)
{
	if (x < 8) {
		if (y < 8) {
			return writeInt8(fd, x << 4 | y);
		} else return writeInt(fd, y, x);
	} else if (y < 8) {
		return writeIntSwapped(fd, x, y);
	} else if (FITS_IN_8BITS(x)) {
		if (FITS_IN_8BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 12 : 8) << 4 | (SIGN_BIT_SET(y) ? 12 : 8))
				+ writeInt8(fd, absInt(x))
				+ writeInt8(fd, absInt(y));
		} else if (FITS_IN_16BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 12 : 8) << 4 | (SIGN_BIT_SET(y) ? 13 : 9))
				+ writeInt8(fd, absInt(x))
				+ writeInt16(fd, absInt(y));
		} else if (FITS_IN_32BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 12 : 8) << 4 | (SIGN_BIT_SET(y) ? 14 : 10))
				+ writeInt8(fd, absInt(x))
				+ writeInt32(fd, absInt(y));
		} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 12 : 8) << 4 | (SIGN_BIT_SET(y) ? 15 : 11))
				+ writeInt8(fd, absInt(x))
				+ writeInt64(fd, absInt(y));
	} else if (FITS_IN_16BITS(x)) {
		if (FITS_IN_8BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9) << 4 | (SIGN_BIT_SET(y) ? 12 : 8))
				+ writeInt16(fd, absInt(x))
				+ writeInt8(fd, absInt(y));
		} else if (FITS_IN_16BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9) << 4 | (SIGN_BIT_SET(y) ? 13 : 9))
				+ writeInt16(fd, absInt(x))
				+ writeInt16(fd, absInt(y));
		} else if (FITS_IN_32BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9) << 4 | (SIGN_BIT_SET(y) ? 14 : 10))
				+ writeInt16(fd, absInt(x))
				+ writeInt32(fd, absInt(y));
		} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 13 : 9) << 4 | (SIGN_BIT_SET(y) ? 15 : 11))
				+ writeInt16(fd, absInt(x))
				+ writeInt64(fd, absInt(y));
	} else if (FITS_IN_32BITS(x)) {
		if (FITS_IN_8BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10) << 4 | (SIGN_BIT_SET(y) ? 12 : 8))
				+ writeInt32(fd, absInt(x))
				+ writeInt8(fd, absInt(y));
		} else if (FITS_IN_16BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10) << 4 | (SIGN_BIT_SET(y) ? 13 : 9))
				+ writeInt32(fd, absInt(x))
				+ writeInt16(fd, absInt(y));
		} else if (FITS_IN_32BITS(y)) {
			return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10) << 4 | (SIGN_BIT_SET(y) ? 14 : 10))
				+ writeInt32(fd, absInt(x))
				+ writeInt32(fd, absInt(y));
		} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 14 : 10) << 4 | (SIGN_BIT_SET(y) ? 15 : 11))
				+ writeInt32(fd, absInt(x))
				+ writeInt64(fd, absInt(y));
	} else if (FITS_IN_8BITS(y)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11) << 4 | (SIGN_BIT_SET(y) ? 12 : 8))
			+ writeInt64(fd, absInt(x))
			+ writeInt8(fd, absInt(y));
	} else if (FITS_IN_16BITS(y)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11) << 4 | (SIGN_BIT_SET(y) ? 13 : 9))
			+ writeInt64(fd, absInt(x))
			+ writeInt16(fd, absInt(y));
	} else if (FITS_IN_32BITS(y)) {
		return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11) << 4 | (SIGN_BIT_SET(y) ? 14 : 10))
			+ writeInt64(fd, absInt(x))
			+ writeInt32(fd, absInt(y));
	} else return writeInt8(fd, (SIGN_BIT_SET(x) ? 15 : 11) << 4 | (SIGN_BIT_SET(y) ? 15 : 11))
			+ writeInt64(fd, absInt(x))
			+ writeInt64(fd, absInt(y));
}

static uint32_t getNameId(BRB_ModuleWriter* writer, const char* name)
{
	arrayForeach (str, iter, writer->names) {
		if (streq(name, *iter)) return iter - writer->names.data;
	}
	strArray_append(&writer->names, name);
	return writer->names.length - 1;
}

static long writeDataPiece(BRB_ModuleWriter* writer, BRB_DataPiece piece)
{
// writing the type
	long acc = writeInt8(writer->dst, piece.type);
// writing the operand
	switch (piece.type) {
		case BRB_DP_BYTES:
			return acc
				+ writeIntOnly(writer->dst, piece.data.length)
				+ fputsbuf(writer->dst, piece.data);
		case BRB_DP_TEXT:
			return acc
				+ fputsbuf(writer->dst, piece.data)
				+ writeInt8(writer->dst, '\0');
		case BRB_DP_I16:
		case BRB_DP_I32:
		case BRB_DP_PTR:
		case BRB_DP_I64:
		case BRB_DP_ZERO:
		case BRB_DP_BUILTIN:
		case BRB_DP_DBADDR:
			return acc + writeIntOnly(writer->dst, piece.operand_u);
		case BRB_DP_NONE:
		case BRB_N_DP_TYPES:
		default:
			assert(false, "invalid data piece type");
	}
}

static long writeDataBlockDecl(BRB_ModuleWriter* writer, BRB_DataBlock block)
{
// writing the flags and the name ID
	return writeInt(writer->dst, getNameId(writer, block.name), block.is_mutable)
// writing the body size
		+ writeIntOnly(writer->dst, block.pieces.length);
}

static long writeOp(BRB_ModuleWriter* writer, BRB_Op op)
{
// writing the type
	return writeInt8(writer->dst, op.type)
// writing the operand, if needed
		+ (BRB_opFlags[op.type] & BRB_OPF_HAS_OPERAND
			? writeIntOnly(writer->dst, op.operand_u)
			: 0);
}

static long writeProcDecl(BRB_ModuleWriter* writer, BRB_Proc proc)
{
// writing the return type
	long acc = writeIntOnly(writer->dst, proc.ret_type)
// writing the name ID and amount of arguments
		+ write2Ints(writer->dst, getNameId(writer, proc.name), proc.args.length);
// writing the arguments
	arrayForeach (BRB_Size, arg, proc.args) {
		acc += writeIntOnly(writer->dst, *arg);
	}
// writing the body size
	return acc + writeIntOnly(writer->dst, proc.body.length);
}

long BRB_writeModule(BRB_Module src, FILE* dst)
{
	BRB_ModuleWriter writer = {
		.src = &src,
		.dst = dst
	};
// writing the header
	long acc = fputsbuf(dst, BRB_V1_HEADER)
// writing the amount of data blocks
		+ writeIntOnly(dst, src.seg_data.length);
// writing the data block declarations
	arrayForeach (BRB_DataBlock, block, src.seg_data) {
		acc += writeDataBlockDecl(&writer, *block);
	}
// writing the amount of procedures
	acc += writeIntOnly(dst, src.seg_exec.length);
// writing the procedure declarations
	arrayForeach (BRB_Proc, proc, src.seg_exec) {
		acc += writeProcDecl(&writer, *proc);
	}
// writing the execution entry point
	acc += writeIntOnly(dst, src.exec_entry_point);
// writing the data pieces
	arrayForeach (BRB_DataBlock, block, src.seg_data) {
		arrayForeach (BRB_DataPiece, piece, block->pieces) {
			acc += writeDataPiece(&writer, *piece);
		}
	}
// writing the operations
	arrayForeach (BRB_Proc, proc, src.seg_exec) {
		arrayForeach (BRB_Op, op, proc->body) {
			acc += writeOp(&writer, *op);
		}
	}
// writing names
	fieldArray names = BRB_getNameFields(&src);
	arrayForeach (field, name, names) {
		acc += fwrite(**name, 1, strlen(**name) + 1, dst);
	}
	fieldArray_clear(&names);
	return acc;
}
