// implementation of the BRB Assembler
#include <brb.h>
#define BRP_IMPLEMENTATION
#define _SBUF_IMPL_LOCK
#define _BR_UTILS_IMPL_LOCK
#include <brp.h>

defArray(BRB_Type);

typedef enum {
	BRB_KW_NOP,
	BRB_KW_END,
	BRB_KW_I8,
	BRB_KW_I16,
	BRB_KW_I32,
	BRB_KW_PTR,
	BRB_KW_I64,
	BRB_KW_ADDR,
	BRB_KW_DBADDR,
	BRB_KW_LD,
	BRB_KW_STR,
	BRB_KW_SYS,
	BRB_KW_BUILTIN,
	BRB_KW_ADD,
	BRB_KW_ADDI,
	BRB_KW_ADDIAT8,
	BRB_KW_ADDIAT16,
	BRB_KW_ADDIAT32,
	BRB_KW_ADDIATP,
	BRB_KW_ADDIAT64,
	BRB_KW_SUB,
	BRB_KW_SUBI,
	BRB_KW_SUBIAT8,
	BRB_KW_SUBIAT16,
	BRB_KW_SUBIAT32,
	BRB_KW_SUBIATP,
	BRB_KW_SUBIAT64,
	BRB_KW_MUL,
	BRB_KW_MULI,
	BRB_KW_MULIAT8,
	BRB_KW_MULIAT16,
	BRB_KW_MULIAT32,
	BRB_KW_MULIATP,
	BRB_KW_MULIAT64,
	BRB_KW_DIV,
	BRB_KW_DIVI,
	BRB_KW_DIVIAT8,
	BRB_KW_DIVIAT16,
	BRB_KW_DIVIAT32,
	BRB_KW_DIVIATP,
	BRB_KW_DIVIAT64,
	BRB_KW_DIVS,
	BRB_KW_DIVSI,
	BRB_KW_DIVSIAT8,
	BRB_KW_DIVSIAT16,
	BRB_KW_DIVSIAT32,
	BRB_KW_DIVSIATP,
	BRB_KW_DIVSIAT64,
	BRB_KW_MOD,
	BRB_KW_MODI,
	BRB_KW_MODIAT8,
	BRB_KW_MODIAT16,
	BRB_KW_MODIAT32,
	BRB_KW_MODIATP,
	BRB_KW_MODIAT64,
	BRB_KW_MODS,
	BRB_KW_MODSI,
	BRB_KW_MODSIAT8,
	BRB_KW_MODSIAT16,
	BRB_KW_MODSIAT32,
	BRB_KW_MODSIATP,
	BRB_KW_MODSIAT64,
	BRB_KW_DROP,
	BRB_KW_DATA,
	BRB_KW_BYTES,
	BRB_KW_TEXT,
	BRB_KW_ZERO,
	BRB_KW_VOID,
	BRB_KW_ENTRY,
	BRB_N_KWS
} BRB_AsmKw;
static sbuf asm_kws[] = {
	[BRB_KW_NOP]       = fromcstr("nop"),
	[BRB_KW_END]       = fromcstr("end"),
	[BRB_KW_I8]        = fromcstr("i8"),
	[BRB_KW_I16]       = fromcstr("i16"),
	[BRB_KW_I32]       = fromcstr("i32"),
	[BRB_KW_PTR]       = fromcstr("ptr"),
	[BRB_KW_I64]       = fromcstr("i64"),
	[BRB_KW_ADDR]      = fromcstr("addr"),
	[BRB_KW_DBADDR]    = fromcstr("dbaddr"),
	[BRB_KW_LD]        = fromcstr("ld"),
	[BRB_KW_STR]       = fromcstr("str"),
	[BRB_KW_SYS]       = fromcstr("sys"),
	[BRB_KW_ADD]       = fromcstr("add"),
	[BRB_KW_ADDI]      = fromcstr("add-i"),
	[BRB_KW_ADDIAT8]   = fromcstr("add-i@8"),
	[BRB_KW_ADDIAT16]  = fromcstr("add-i@16"),
	[BRB_KW_ADDIAT32]  = fromcstr("add-i@32"),
	[BRB_KW_ADDIATP]   = fromcstr("add-i@p"),
	[BRB_KW_ADDIAT64]  = fromcstr("add-i@64"),
	[BRB_KW_SUB]       = fromcstr("sub"),
	[BRB_KW_SUBI]      = fromcstr("sub-i"),
	[BRB_KW_SUBIAT8]   = fromcstr("sub-i@8"),
	[BRB_KW_SUBIAT16]  = fromcstr("sub-i@16"),
	[BRB_KW_SUBIAT32]  = fromcstr("sub-i@32"),
	[BRB_KW_SUBIATP]   = fromcstr("sub-i@p"),
	[BRB_KW_SUBIAT64]  = fromcstr("sub-i@64"),
	[BRB_KW_MUL]       = fromcstr("mul"),
	[BRB_KW_MULI]      = fromcstr("mul-i"),
	[BRB_KW_MULIAT8]   = fromcstr("mul-i@8"),
	[BRB_KW_MULIAT16]  = fromcstr("mul-i@16"),
	[BRB_KW_MULIAT32]  = fromcstr("mul-i@32"),
	[BRB_KW_MULIATP]   = fromcstr("mul-i@p"),
	[BRB_KW_MULIAT64]  = fromcstr("mul-i@64"),
	[BRB_KW_DIV]       = fromcstr("div"),
	[BRB_KW_DIVI]      = fromcstr("div-i"),
	[BRB_KW_DIVIAT8]   = fromcstr("div-i@8"),
	[BRB_KW_DIVIAT16]  = fromcstr("div-i@16"),
	[BRB_KW_DIVIAT32]  = fromcstr("div-i@32"),
	[BRB_KW_DIVIATP]   = fromcstr("div-i@p"),
	[BRB_KW_DIVIAT64]  = fromcstr("div-i@64"),
	[BRB_KW_DIVS]      = fromcstr("divs"),
	[BRB_KW_DIVSI]     = fromcstr("divs-i"),
	[BRB_KW_DIVSIAT8]  = fromcstr("divs-i@8"),
	[BRB_KW_DIVSIAT16] = fromcstr("divs-i@16"),
	[BRB_KW_DIVSIAT32] = fromcstr("divs-i@32"),
	[BRB_KW_DIVSIATP]  = fromcstr("divs-i@p"),
	[BRB_KW_DIVSIAT64] = fromcstr("divs-i@64"),
	[BRB_KW_MOD]       = fromcstr("mod"),
	[BRB_KW_MODI]      = fromcstr("mod-i"),
	[BRB_KW_MODIAT8]   = fromcstr("mod-i@8"),
	[BRB_KW_MODIAT16]  = fromcstr("mod-i@16"),
	[BRB_KW_MODIAT32]  = fromcstr("mod-i@32"),
	[BRB_KW_MODIATP]   = fromcstr("mod-i@p"),
	[BRB_KW_MODIAT64]  = fromcstr("mod-i@64"),
	[BRB_KW_MODS]      = fromcstr("mods"),
	[BRB_KW_MODSI]     = fromcstr("mods-i"),
	[BRB_KW_MODSIAT8]  = fromcstr("mods-i@8"),
	[BRB_KW_MODSIAT16] = fromcstr("mods-i@16"),
	[BRB_KW_MODSIAT32] = fromcstr("mods-i@32"),
	[BRB_KW_MODSIATP]  = fromcstr("mods-i@p"),
	[BRB_KW_MODSIAT64] = fromcstr("mods-i@64"),
	[BRB_KW_DROP]      = fromcstr("drop"),
	[BRB_KW_BUILTIN]   = fromcstr("builtin"),
	[BRB_KW_DATA]      = fromcstr("data"),
	[BRB_KW_BYTES]     = fromcstr("bytes"),
	[BRB_KW_TEXT]      = fromcstr("text"),
	[BRB_KW_ZERO]      = fromcstr("zero"),
	[BRB_KW_VOID]      = fromcstr("void"),
	[BRB_KW_ENTRY]     = fromcstr("entry"),
	(sbuf){0}
};
static_assert(BRB_N_OPS == 63, "not all operations have their names defined in the assembler");
static_assert(BRB_N_DP_TYPES == 10, "not all data pieces have their names defined in the assemblr");

typedef enum {
	BRB_SYM_BRACKET_L,
	BRB_SYM_BRACKET_R,
	BRB_SYM_SQBRACKET_L,
	BRB_SYM_SQBRACKET_R,
	BRB_SYM_CBRACKET_L,
	BRB_SYM_CBRACKET_R,
	BRB_SYM_COMMA,
	BRB_SYM_PLUS,
	BRB_N_SYMS
} BRB_AsmSymbol;

static sbuf asm_symbols[] = {
	[BRB_SYM_BRACKET_L] = fromcstr("("),
	[BRB_SYM_BRACKET_R] = fromcstr(")"),
	[BRB_SYM_SQBRACKET_L] = fromcstr("["),
	[BRB_SYM_SQBRACKET_R] = fromcstr("]"),
	[BRB_SYM_CBRACKET_L] = fromcstr("{"),
	[BRB_SYM_CBRACKET_R] = fromcstr("}"),
	[BRB_SYM_COMMA] = fromcstr(","),
	[BRB_SYM_PLUS] = fromcstr("+"),
	BRP_HIDDEN_SYMBOL(" "),
	BRP_HIDDEN_SYMBOL("\t"),
	BRP_HIDDEN_SYMBOL("\n"),
	(sbuf){0}
};

static BRB_TypeKind kw_to_typekind[] = {
	[BRB_KW_I8] = BRB_TYPE_I8,
	[BRB_KW_I16] = BRB_TYPE_I16,
	[BRB_KW_I32] = BRB_TYPE_I32,
	[BRB_KW_PTR] = BRB_TYPE_PTR,
	[BRB_KW_I64] = BRB_TYPE_I64,
	[BRB_KW_VOID] = BRB_TYPE_VOID
};

static BRB_Error addLoc(BRB_Error err, BRP_Token token)
{
	err.loc = objdup(BRP_TokenLoc, token.loc);
	return err;
}

static BRB_Error getName(BRP* obj, char** name_p)
{
	BRP_Token res = BRP_fetchToken(obj);
	if (res.type != TOKEN_STRING)
		return addLoc((BRB_Error){.type = BRB_ERR_INVALID_NAME}, res);
	if (memchr(res.string.data, '\0', res.string.length))
		return addLoc((BRB_Error){.type = BRB_ERR_INVALID_NAME}, res);
	*name_p = tostr(res.string);
	return (BRB_Error){0};
}

static BRB_Error getType(BRP* obj, BRB_Type* res_p)
{
	BRP_Token token = BRP_fetchToken(obj);
	if (token.type != TOKEN_KEYWORD)
		return addLoc((BRB_Error){.type = BRB_ERR_TYPE_EXPECTED}, token);
	res_p->kind = kw_to_typekind[token.keyword_id];
	if (BRP_getTokenSymbolId(BRP_peekToken(obj)) == BRB_SYM_SQBRACKET_L) {
		BRP_fetchToken(obj);
		token = BRP_fetchToken(obj);
		if (token.type != TOKEN_INT)
			return addLoc((BRB_Error){.type = BRB_ERR_INVALID_ARRAY_SIZE_SPEC}, token);
		res_p->n_items = token.value;
		if (BRP_getTokenSymbolId(token = BRP_fetchToken(obj)) != BRB_SYM_SQBRACKET_R)
			return addLoc((BRB_Error){.type = BRB_ERR_INVALID_ARRAY_SIZE_SPEC}, token);
	} else res_p->n_items = 1;
	return (BRB_Error){0};
}

static BRB_Error getOp(BRP* obj, BRB_ModuleBuilder* builder, uint32_t proc_id, BRB_Op* op)
{
	BRB_Error err;
	BRP_Token token = BRP_fetchToken(obj);
	if (token.type != TOKEN_KEYWORD || token.keyword_id >= BRB_N_OPS)
		return addLoc((BRB_Error){.type = BRB_ERR_OP_NAME_EXPECTED}, token);

	op->type = (BRB_OpType)token.keyword_id;
	switch (BRB_GET_OPERAND_TYPE(op->type)) {
		case BRB_OPERAND_NONE:
			break;
		case BRB_OPERAND_SYSCALL_NAME: {
			op->type = BRB_OP_SYS;
			if ((token = BRP_fetchToken(obj)).type != TOKEN_WORD)
				return addLoc((BRB_Error){.type = BRB_ERR_SYSCALL_NAME_EXPECTED}, token);
			op->operand_u = 0;
			sbuf name = fromstr(token.word);
			repeat (BRB_N_SYSCALLS) {
				if (sbufeq(name, BRB_syscallNames[op->operand_u])) {
					name.data = NULL;
					break;
				}
				++op->operand_u;
			}
			if (name.data) return addLoc((BRB_Error){.type = BRB_ERR_SYSCALL_NAME_EXPECTED}, token);
		} break;
		case BRB_OPERAND_INT8:
		case BRB_OPERAND_INT:
			if ((token = BRP_fetchToken(obj)).type != TOKEN_INT)
				return addLoc((BRB_Error){.type = BRB_ERR_INT_OPERAND_EXPECTED}, token);
			op->operand_u = token.value;
			break;
		case BRB_OPERAND_VAR_NAME:
			if ((token = BRP_fetchToken(obj)).type != TOKEN_INT)
				assert(false, "named stack items are not implemented yet");
			op->operand_u = token.value;
			break;
		case BRB_OPERAND_DB_NAME:
			token = BRP_fetchToken(obj);
			switch (token.type) {
				case TOKEN_INT:
					op->operand_u = token.value;
					break;
				case TOKEN_STRING: {
					char* name;
					BRP_unfetchToken(obj, token);
					if ((err = getName(obj, &name)).type) return err;
					op->operand_u = BRB_getDataBlockIdByName(&builder->module, name);
					if (op->operand_u == SIZE_MAX)
						return addLoc((BRB_Error){.type = BRB_ERR_INT_OR_NAME_OPERAND_EXPECTED}, token);
				} break;
				case TOKEN_SYMBOL:
				case TOKEN_KEYWORD:
				case TOKEN_NONE:
				default:
					return addLoc((BRB_Error){.type = BRB_ERR_INT_OR_NAME_OPERAND_EXPECTED}, token);
			}
			break;
		case BRB_OPERAND_TYPE:
			if ((err = getType(obj, &op->operand_type)).type) return err;
			break;
		case BRB_OPERAND_BUILTIN: {
			if ((token = BRP_fetchToken(obj)).type != TOKEN_WORD)
				return addLoc((BRB_Error){.type = BRB_ERR_BUILTIN_OPERAND_EXPECTED}, token);
			op->operand_u = 0;
			sbuf name = fromstr(token.word);
			repeat (BRB_N_BUILTINS) {
				if (sbufeq(name, BRB_builtinNames[op->operand_u])) {
					name.data = NULL;
					break;
				}
				++op->operand_u;
			}
			if (name.data) return addLoc((BRB_Error){.type = BRB_ERR_BUILTIN_OPERAND_EXPECTED}, token);
		} break;
		default:
			return addLoc((BRB_Error){.type = BRB_ERR_OP_NAME_EXPECTED}, token);
	}
	return (BRB_Error){0};
}

static BRB_DataPieceType kw_to_dp[] = {
	[BRB_KW_NOP] = BRB_DP_NONE,
	[BRB_KW_BYTES] = BRB_DP_BYTES,
	[BRB_KW_I16] = BRB_DP_I16,
	[BRB_KW_I32] = BRB_DP_I32,
	[BRB_KW_PTR] = BRB_DP_PTR,
	[BRB_KW_I64] = BRB_DP_I64,
	[BRB_KW_TEXT] = BRB_DP_TEXT,
	[BRB_KW_DBADDR] = BRB_DP_DBADDR,
	[BRB_KW_ZERO] = BRB_DP_ZERO,
	[BRB_KW_BUILTIN] = BRB_DP_BUILTIN,
};

static BRB_Error getDataPiece(BRP* obj, BRB_ModuleBuilder* builder, uint32_t db_id, BRB_DataPiece* piece)
{
	BRB_Error err;
	BRP_Token token = BRP_fetchToken(obj);
	if (token.type != TOKEN_KEYWORD)
		return addLoc((BRB_Error){.type = BRB_ERR_DP_NAME_EXPECTED}, token);

	switch (token.keyword_id) {
		case BRB_KW_I16:
		case BRB_KW_I32:
		case BRB_KW_I64:
		case BRB_KW_PTR:
			piece->type = kw_to_dp[token.keyword_id];
			if ((token = BRP_fetchToken(obj)).type != TOKEN_INT)
				return addLoc((BRB_Error){.type = BRB_ERR_INT_OPERAND_EXPECTED}, token);
			piece->content_u = token.value;
			break;
		case BRB_KW_ZERO: {
			piece->type = BRB_DP_ZERO;
			if ((err = getType(obj, &piece->content_type)).type) return err;
		} break;
		case BRB_KW_DBADDR:
			piece->type = BRB_DP_DBADDR;
			token = BRP_fetchToken(obj);
			switch (token.type) {
				case TOKEN_INT:
					piece->content_u = token.value;
					break;
				case TOKEN_STRING: {
					char* name;
					BRP_unfetchToken(obj, token);
					if ((err = getName(obj, &name)).type) return err;
					piece->content_u = BRB_getDataBlockIdByName(&builder->module, name);
					if (piece->content_u == SIZE_MAX)
						return addLoc((BRB_Error){.type = BRB_ERR_INT_OR_NAME_OPERAND_EXPECTED}, token);
				} break;
				case TOKEN_SYMBOL:
				case TOKEN_KEYWORD:
				case TOKEN_NONE:
				default:
					return addLoc((BRB_Error){.type = BRB_ERR_INT_OR_NAME_OPERAND_EXPECTED}, token);
			} break;
		case BRB_KW_BUILTIN: {
			piece->type = BRB_DP_BUILTIN;
			if ((token = BRP_fetchToken(obj)).type != TOKEN_WORD)
				return addLoc((BRB_Error){.type = BRB_ERR_BUILTIN_OPERAND_EXPECTED}, token);
			piece->content_u = 0;
			sbuf name = fromstr(token.word);
			repeat (BRB_N_BUILTINS) {
				if (sbufeq(name, BRB_builtinNames[piece->content_u])) {
					name.data = NULL;
					break;
				}
				++piece->content_u;
			}
			if (name.data) return addLoc((BRB_Error){.type = BRB_ERR_BUILTIN_OPERAND_EXPECTED}, token);
		} break;
		case BRB_KW_BYTES:
		case BRB_KW_TEXT:
			piece->type = kw_to_dp[token.keyword_id];
			if ((token = BRP_fetchToken(obj)).type != TOKEN_STRING)
				return addLoc((BRB_Error){.type = BRB_ERR_TEXT_OPERAND_EXPECTED}, token);
			if (piece->type == BRB_DP_TEXT ? memchr(token.string.data, '\0', token.string.length) : false)
				return addLoc((BRB_Error){.type = BRB_ERR_INVALID_TEXT_OPERAND}, token);
			piece->data = token.string;
			break;
		case BRB_KW_VOID:
		case BRB_KW_ADDR:
		case BRB_KW_LD:
		case BRB_KW_STR:
		case BRB_KW_SYS:
		case BRB_KW_DATA:
		case BRB_KW_NOP:
		case BRB_KW_END:
		case BRB_KW_I8:
		case BRB_N_KWS:
		default:
			return addLoc((BRB_Error){.type = BRB_ERR_DP_NAME_EXPECTED}, token);
	}
	return (BRB_Error){0};
}

BRB_Error BRB_assembleModule(FILE* input, const char* input_name, BRB_Module* dst)
{
	BRB_Error err;
	BRP prep = {0};
	BRP_initBRP(&prep, NULL, BRP_ESC_STR_LITERALS);
	BRP_setKeywords(&prep, asm_kws);
	BRP_setSymbols(&prep, asm_symbols);
	BRP_setInput(&prep, (char*)input_name, input);

	BRB_ModuleBuilder builder;
	if ((err = BRB_initModuleBuilder(&builder)).type) return err;

	while (true) {
		BRP_Token token = BRP_fetchToken(&prep);
		if (token.type == TOKEN_NONE) break;
		if (token.type != TOKEN_KEYWORD)
			return addLoc((BRB_Error){.type = BRB_ERR_INVALID_DECL}, token);

		switch (token.keyword_id) {
			case BRB_KW_I8:
			case BRB_KW_I16:
			case BRB_KW_I32:
			case BRB_KW_I64:
			case BRB_KW_PTR:
			case BRB_KW_VOID: {
				BRP_unfetchToken(&prep, token);
// getting return type of the procdure
				BRB_Type ret_type;
				if ((err = getType(&prep, &ret_type)).type) return err;
// getting procedure name
				char* proc_name;
				if ((err = getName(&prep, &proc_name)).type) return err;
// getting procedure arguments
				token = BRP_fetchToken(&prep);
				if (token.type != TOKEN_SYMBOL || token.symbol_id != BRB_SYM_BRACKET_L)
					return addLoc((BRB_Error){.type = BRB_ERR_ARGS_EXPECTED}, token);
				BRB_TypeArray args = {0};
				if (BRP_getTokenSymbolId(BRP_peekToken(&prep)) != BRB_SYM_BRACKET_R) {
					do {
						if (!BRB_TypeArray_incrlen(&args, 1)) return (BRB_Error){.type = BRB_ERR_NO_MEMORY};
						if ((err = getType(&prep, arrayhead(args))).type) return err;
					} while (BRP_getTokenSymbolId(BRP_fetchToken(&prep)) != BRB_SYM_BRACKET_R);
				} else BRP_fetchToken(&prep);
// adding the declaration
				uint32_t proc_id;
				size_t _proc_id = BRB_getProcIdByName(&builder.module, proc_name);
				if (_proc_id == SIZE_MAX) {
					if ((err = BRB_addProc(&builder, &proc_id, proc_name, args.length, args.data, ret_type, 0)).type) return addLoc(err, token);
				} else {
					if (memcmp(&ret_type, &builder.module.seg_exec.data[_proc_id].ret_type, sizeof(BRB_Type)))
						return addLoc((BRB_Error){.type = BRB_ERR_PROTOTYPE_MISMATCH, .name = proc_name}, token);
					if (args.length == builder.module.seg_exec.data[_proc_id].args.length
						? memcmp(args.data, builder.module.seg_exec.data[_proc_id].args.data, args.length) : true)
						return addLoc((BRB_Error){.type = BRB_ERR_PROTOTYPE_MISMATCH, .name = proc_name}, token);
					proc_id = _proc_id;
				}
				BRB_TypeArray_clear(&args);
// checking if the procedure is declared as an entry point
				if (BRP_getTokenKeywordId(BRP_peekToken(&prep)) == BRB_KW_ENTRY) {
					builder.module.exec_entry_point = proc_id;
					BRP_fetchToken(&prep);
				}
// optionally fetching the body
				if (BRP_getTokenSymbolId(BRP_peekToken(&prep)) != BRB_SYM_CBRACKET_L) break;
				BRP_fetchToken(&prep);
				BRB_Op op;
				while (BRP_getTokenSymbolId(token = BRP_peekToken(&prep)) != BRB_SYM_CBRACKET_R) {
					if ((err = getOp(&prep, &builder, proc_id, &op)).type) return err;
					if ((err = BRB_addOp(&builder, proc_id, op)).type) return addLoc(err, token);
				}
				BRP_fetchToken(&prep);
			} break;
			case BRB_KW_DATA: {
				bool is_mutable = false;
				char* db_name;
// fetching the `+` symbol if found after the `data` keyword
				if (BRP_getTokenSymbolId(BRP_peekToken(&prep)) == BRB_SYM_PLUS) {
					BRP_fetchToken(&prep);
					is_mutable = true;
				}
// fetching the declaration name
				token = BRP_peekToken(&prep);
				if ((err = getName(&prep, &db_name)).type) return err;
// adding the declaration
				uint32_t db_id;
				size_t _db_id = BRB_getDataBlockIdByName(&builder.module, db_name);
				if (_db_id == SIZE_MAX) {
					if ((err = BRB_addDataBlock(&builder, &db_id, db_name, is_mutable, 0)).type) return addLoc(err, token);
				} else db_id = _db_id;
// optionally fetching the body
				if (BRP_getTokenSymbolId(BRP_peekToken(&prep)) != BRB_SYM_CBRACKET_L) break;
				BRP_fetchToken(&prep);
				BRB_DataPiece piece;
				while (BRP_getTokenSymbolId(token = BRP_peekToken(&prep)) != BRB_SYM_CBRACKET_R) {
					if ((err = getDataPiece(&prep, &builder, db_id, &piece)).type) return err;
					if ((err = BRB_addDataPiece(&builder, db_id, piece)).type) return addLoc(err, token);
				}
				BRP_fetchToken(&prep);
			} break;
			case BRB_KW_NOP:
			case BRB_KW_END:
			case BRB_KW_ADDR:
			case BRB_KW_DBADDR:
			case BRB_KW_LD:
			case BRB_KW_STR:
			case BRB_KW_SYS:
			case BRB_KW_BUILTIN:
			case BRB_KW_BYTES:
			case BRB_KW_TEXT:
			case BRB_KW_ZERO:
			case BRB_KW_ENTRY:
				return addLoc((BRB_Error){.type = BRB_ERR_INVALID_DECL}, token);
		}
	}
	BRP_delBRP(&prep);
	return BRB_extractModule(builder, dst);
}
