// The BRidge Preprocessor
#ifndef _BRP_
#define _BRP_

#include <br_utils.h>
#include <sbuf.h>
#include <datasets.h>
#include <errno.h>
#include <sys/cdefs.h>


#define DQUOTE CSBUF('"')
#define QUOTE CSBUF('\'')
#define NT_PATHSEP CSBUF('\\')
#define POSIX_PATHSEP CSBUF('/')
#define NT_NEWLINE CSBUF("\r\n")
#define POSIX_NEWLINE CSBUF('\n')
#define SPACE CSBUF(' ')
#define TAB CSBUF('\t')

#ifdef _WIN32_
#define PATHSEP NT_PATHSEP
#define NEWLINE NT_NEWLINE
#else
#define PATHSEP POSIX_PATHSEP
#define NEWLINE POSIX_NEWLINE
#endif

typedef enum token_type {
	TOKEN_NONE,
	TOKEN_WORD,
	TOKEN_KEYWORD,
	TOKEN_SYMBOL,
	TOKEN_INT,
	TOKEN_STRING,
	N_TOKEN_TYPES
} TokenType;

typedef struct token_loc {
	int32_t lineno;
	int32_t colno;
	char* src_name;
	struct token_loc* included_from;
} TokenLoc;
declArray(TokenLoc);

typedef struct token {
	int8_t type;
	TokenLoc loc;
	union {
		char* word; // for TOKEN_WORD
		sbuf string; // for TOKEN_STRING
		int64_t value; // for TOKEN_INT
		uint32_t keyword_id; // for TOKEN_KEYWORD
		uint32_t symbol_id; // for TOKEN_SYMBOL
	};
} Token;
#define emptyToken ((Token){ .type = TOKEN_NONE })
#define wordToken(_word) ((Token){ .type = TOKEN_WORD, .word = _word })
#define keywordToken(kw_id) ((Token){ .type = TOKEN_KEYWORD, .keyword_id = kw_id })
#define symbolToken(_symbol_id) ((Token){ .type = TOKEN_SYMBOL, .symbol_id = _symbol_id })
#define intToken(_int) ((Token){ .type = TOKEN_INT, .value = _int })
#define stringToken(_word) ((Token){ .type = TOKEN_STRING, .word = _word })

typedef enum {
	BRP_ERR_OK,
	BRP_ERR_UNCLOSED_STR,
	BRP_ERR_NO_MEMORY,
	BRP_ERR_FILE_NOT_FOUND,
	BRP_ERR_SYMBOL_CONFLICT,
	BRP_ERR_INVALID_CMD_SYNTAX,
	BRP_ERR_UNKNOWN_CMD,
	BRP_ERR_UNCLOSED_MACRO,
	BRP_ERR_EXCESS_ENDIF,
	BRP_ERR_EXCESS_ENDMACRO,
	BRP_ERR_UNCLOSED_CONDITION,
	BRP_ERR_EXCESS_ELSE,
	BRP_ERR_UNCLOSED_CHAR,
	BRP_ERR_MACRO_ARG_COUNT_MISMATCH,
	N_BRP_ERRORS
} BRPError;

declArray(sbuf);
typedef struct macro_arg {
	char* name;
	TokenLoc def_loc;
} MacroArg;

declArray(MacroArg);
typedef struct macro {
	char* name;
	sbuf def;
	TokenLoc def_loc;
	MacroArgArray args;
} Macro;
declArray(Macro);

typedef struct input_ctx {
	struct input_ctx* prev;
	sbuf buffer;
	char* orig_data;
	TokenLoc cur_loc;
	MacroArray locals;
} InputCtx;

declArray(Token);
typedef struct brp {
	sbuf* keywords;
	sbuf* symbols;
	sbuf* hidden_symbols;
	short _dquote_symbol_id;
	short _quote_symbol_id;
	char flags;
	BRPError error_code;
	TokenLoc error_loc;
	union {
		sbuf error_symbol;
		struct {
			char* error_macro_name;
			int error_macro_n_args;
			int error_n_args;
			TokenLoc error_macro_def_loc;
		};
		char* error_filename;
		int32_t sys_errno;
	};
	void (*handler)(struct brp*);
	InputCtx cur_input;
	TokenArray pending;
	MacroArray macros;
	TokenLocArray conditional_blocks;
} BRP;
#define BRP_KEYWORD(spec) CSBUF(spec)
#define BRP_SYMBOL(spec) CSBUF(spec)
// hidden symbols are delimiters that are not returned as generated tokens
#define BRP_HIDDEN_SYMBOL(spec) ((sbuf){ .data = spec"\n", .length = sizeof(spec) - 1 })

#define BRPempty(prep) ((prep)->cur_input.buffer.length + (size_t)(prep)->cur_input.prev == 0)
#define isWordToken(token) ( (token).type == TOKEN_WORD || (token).type == TOKEN_KEYWORD )
#define isSymbolSpecHidden(spec) ((spec).data[(spec).length] > 0)

typedef void (*BRPErrorHandler) (BRP*);

char* getNormPath(char* src);
BRP* initBRP(BRP* obj, BRPErrorHandler handler, char flags);
// flags for initBRP function
#define BRP_ESC_STR_LITERALS 0x1
void delBRP(BRP* obj);

bool _setKeywords(BRP* obj, ...);
#define setKeywords(obj, ...) _setKeywords(obj, __VA_ARGS__, (sbuf){0})

bool _setSymbols(BRP* obj, ...);
#define setSymbols(obj, ...) _setSymbols(obj, __VA_ARGS__, (sbuf){0})

bool appendInput(BRP* obj, InputCtx* input, FILE* input_fd, TokenLoc initial_loc, TokenLoc include_loc);
#define setInput(obj, path, input_fd) appendInput(obj, &(obj)->cur_input, input_fd, (TokenLoc){ .src_name = path, .colno = 1, .lineno = 1, .included_from = NULL }, (TokenLoc){0})

Token _fetchToken(BRP* obj, InputCtx* input, TokenArray* queue);
#define fetchToken(obj) _fetchToken(obj, &(obj)->cur_input, &(obj)->pending)

Token peekToken(BRP* obj);
bool unfetchToken(BRP* obj, Token token);

void fprintTokenLoc(FILE* fd, TokenLoc loc);
#define printTokenLoc(loc) fprintTokenLoc(stdout, loc)

void fprintTokenStr(FILE* fd, Token token, BRP* obj);
#define printTokenStr(token, parser) fprintTokenStr(stdout, token, parser)

void fprintToken(FILE* fd, Token token, BRP* obj);
#define printToken(token, parser) fprintToken(stdout, token, parser)

int getTokenSymbolId(Token token);
int getTokenKeywordId(Token token);
char* getTokenTypeName(TokenType type);
char* getTokenWord(BRP* obj, Token token);
void printBRPErrorStr(FILE* fd, BRP* obj);
void printBRPError(BRP* obj);

#endif // _BRP_

#if defined(BRP_IMPLEMENTATION) && !defined(_BRP_IMPL_LOCK)
#define _BRP_IMPL_LOCK

#define BR_BYTEORDER_IMPLEMENTATION
#include <br_utils.h>
#define SBUF_IMPLEMENTATION
#include <sbuf.h>

defArray(TokenLoc);
defArray(Macro);
defArray(sbuf);
defArray(Token);
defArray(MacroArg);

char* getNormPath(char* src)
{
	sbuf input = SBUF(src);

	sbuf res = smalloc(input.length + 1);
	if (!res.data) return NULL;
	memset(res.data, 0, res.length);

	sbufArray components = sbufArray_new(sbufcount(input, PATHSEP) * -1 - 1);
	sbuf new;
	
	while (input.length) {
		sbufsplit(&input, &new, PATHSEP);
		if (sbufeq(new, "..")) {
			sbufArray_pop(&components, -1);
		} else if ((new.length || !components.length) && !sbufeq(new, ".")) {
			sbufArray_append(&components, new);
		}
	}

	res.length = 0;
	arrayForeach(sbuf, component, components) {
	 	if (res.length) {
			memcpy(res.data + res.length, PATHSEP.data, PATHSEP.length);
			res.length += PATHSEP.length;
		}
		memcpy(res.data + res.length, component->data, component->length);
		res.length += component->length;
	}
	free(components.data);
	return res.data;
}

bool pathEquals(char path1[], char path2[])
{
	char path1_r[256];
	char path2_r[256];
	realpath(path1, path1_r);
	realpath(path2, path2_r);
	return sbufeq(path1_r, path2_r);
}

bool _setKeywords(BRP* obj, ...)
{
	va_list args;
	va_start(args, obj);
	sbuf kw;
	int n_kws = 1;
// calculating amount of keywords
	while ((kw = va_arg(args, sbuf)).data) { n_kws++; }
	va_end(args);
// copying the keywords array
	va_start(args, obj);
	obj->keywords = malloc(n_kws * sizeof(sbuf));
	if (!obj->keywords) return false;
	for (int i = 0; i < n_kws; i++) {
		obj->keywords[i] = va_arg(args, sbuf);
	}

	return true;
}

bool _setSymbols(BRP* obj, ...)
{
	va_list args;
	va_start(args, obj);
	sbuf symbol;
	int n_symbols = 1;
	int n_hidden_symbols = 1;
// calculating amount of symbols
	while ((symbol = va_arg(args, sbuf)).data) {
		n_symbols++;
		if (isSymbolSpecHidden(symbol)) n_hidden_symbols++;
	}
	va_end(args);
// copying the symbols array
	va_start(args, obj);
	obj->symbols = malloc((n_symbols + 2) * sizeof(sbuf));
	obj->hidden_symbols = malloc(n_hidden_symbols * sizeof(sbuf));
	if (!obj->symbols || !obj->hidden_symbols) return false;

	n_hidden_symbols = 0;
	for (int i = 0; i < n_symbols - 1; i++) {
		obj->symbols[i] = va_arg(args, sbuf);
		if (isSymbolSpecHidden(obj->symbols[i])) obj->hidden_symbols[n_hidden_symbols++] = obj->symbols[i];
	}
	va_end(args);

	obj->_dquote_symbol_id = n_symbols - 1;
	obj->symbols[n_symbols - 1] = DQUOTE;
	obj->_quote_symbol_id = n_symbols;
	obj->symbols[n_symbols] = QUOTE;
	obj->symbols[n_symbols + 1] = obj->hidden_symbols[n_hidden_symbols] = (sbuf){0};

	for (int i = 0; i < n_symbols; i++) {
		for (int i1 = 0; i1 < i; i1++) {
			if (sbufstartswith(obj->symbols[i], obj->symbols[i1])) {
				obj->error_code = BRP_ERR_SYMBOL_CONFLICT;
				obj->error_symbol = obj->symbols[i];
				obj->handler(obj);
				return false;
			}
		}
	}

	return true;
}

bool appendInput(BRP *obj, InputCtx* const input, FILE* input_fd, TokenLoc initial_loc, TokenLoc include_loc)
{
	if (!input_fd) {
		obj->error_code = BRP_ERR_FILE_NOT_FOUND;
		obj->error_loc = include_loc;
		obj->error_symbol = SBUF(initial_loc.src_name);
		obj->handler(obj);
		return false;
	}

	InputCtx* prev_ctx = malloc(sizeof(InputCtx));
	*prev_ctx = *input;
	input->cur_loc = initial_loc,
	input->buffer = filecontent(input_fd),
	input->prev = prev_ctx;

	if (!input->buffer.data) {
		obj->error_code = BRP_ERR_NO_MEMORY;
		obj->error_loc = include_loc;
		obj->handler(obj);
		fclose(input_fd);
		return false;
	}

	input->orig_data = input->buffer.data;
	if (include_loc.src_name) {
		input->cur_loc.included_from = malloc(sizeof(TokenLoc));
		*input->cur_loc.included_from = include_loc;
	}

	sbufstriprv(&input->buffer, obj->hidden_symbols);
	fclose(input_fd);
	return true;
}

void delInput(BRP* obj, InputCtx* const input)
{
	free(input->orig_data);
	InputCtx* to_free = input->prev;
	if (to_free) {
		*input = *input->prev;
	} else *input = (InputCtx){0};
	free(to_free);

	if (!input && obj->conditional_blocks.length) {
		obj->error_code = BRP_ERR_UNCLOSED_CONDITION;
		obj->error_loc = TokenLocArray_pop(&obj->conditional_blocks, -1);
		obj->handler(obj);
	}
}

static sbuf nl_arg[2] = { NEWLINE };

void cleanInput(BRP* obj, InputCtx* const input)
{
	sbuf stripped = sbufstriplv(&input->buffer, obj->hidden_symbols);
	int nl_count = sbufcount_v(stripped, nl_arg);
	if (nl_count) {
		input->cur_loc.lineno += nl_count;
		sbuf last_stripped_line;
		sbufsplitrv(&stripped, &last_stripped_line, nl_arg);
		input->cur_loc.colno = stripped.length + 1;
	} else input->cur_loc.colno += stripped.length;
}

typedef enum {
	BRP_INCLUDE,
	BRP_DEFINE,
	BRP_MACRO,
	BRP_COMMENT,
	BRP_IFDEF,
	BRP_ENDIF,
	BRP_ENDMACRO,
	BRP_IFNDEF,
	BRP_ELSE,
	N_BRP_CMDS
} BRPDirectiveCode;

static const sbuf brp_directives[N_BRP_CMDS + 1] = {
	[BRP_INCLUDE] = CSBUF("include"),
	[BRP_DEFINE] = CSBUF("define"),
	[BRP_MACRO] = CSBUF("macro"),
	[BRP_COMMENT] = CSBUF('!'),
	[BRP_IFDEF] = CSBUF("ifdef"),
	[BRP_ENDIF] = CSBUF("endif"),
	[BRP_ENDMACRO] = CSBUF("endmacro"),
	[BRP_IFNDEF] = CSBUF("ifndef"),
	[BRP_ELSE] = CSBUF("else")
};

static const char oppositeBrackets[INT8_MAX] = {
	['('] = ')', [')'] = '(',
	['['] = ']', [']'] = '[',
	['{'] = '}', ['}'] = '{'
};

static inline Macro fetchMacroArg(BRP* const obj, InputCtx* const input, const MacroArg arg)
{
	Macro res = { .name = arg.name, .def_loc = arg.def_loc };
	bool isnt_last_arg = true;
	while (true) {
		sbuf delim = sbufsplit(&input->buffer, &res.def, SBUF(','), SBUF('('), SBUF('['), SBUF('{'), SBUF(')'), NEWLINE);
		if (delim.length ? sbufeq(delim, NEWLINE) : true) {
			obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
			obj->error_loc = input->cur_loc;
			obj->error_loc.colno += res.def.length;
			obj->handler(obj);
			return (Macro){0};
		}

		if (delim.data[0] == ',') break;
		if (delim.data[0] == ')') {
			sbufshift(input->buffer, -1);
			isnt_last_arg = false;
			break;
		}
		sbuf also_def;
		delim = sbufsplit(&input->buffer, &also_def, SBUF(oppositeBrackets[delim.data[0]]), NEWLINE);
		if (!delim.data ? sbufeq(delim, NEWLINE) : false) {
			obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
			obj->error_loc = input->cur_loc;
			obj->error_loc.colno += res.def.length;
			obj->handler(obj);
			return (Macro){0};
		}
		res.def.length += also_def.length + 2;
	}

	input->cur_loc.colno += res.def.length + isnt_last_arg;
	return res;
}

static void preprocessInput(BRP* obj, InputCtx* const input);

static inline InputCtx* expandMacro(BRP* const obj, InputCtx* const input, const Macro macro)
{
	InputCtx* prev_ctx = malloc(sizeof(InputCtx));
	*prev_ctx = *input;
	input->buffer = macro.def;
	input->orig_data = NULL;
	input->cur_loc = macro.def_loc;
	input->prev = prev_ctx;
	input->locals = MacroArray_new(-macro.args.length);
	input->locals.length = macro.args.length;
	input->cur_loc.included_from = malloc(sizeof(TokenLoc));
	*input->cur_loc.included_from = prev_ctx->cur_loc;
	int name_length = strlen(macro.name);
	sbufshift(prev_ctx->buffer, name_length);
	prev_ctx->cur_loc.colno += name_length;

	if (sbufcutc(&prev_ctx->buffer, '(')) {
		prev_ctx->cur_loc.colno++;
		for (int i = 0; i < macro.args.length; i++) {
			if (!prev_ctx->buffer.length) {
				obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
				obj->error_loc = prev_ctx->cur_loc;
				obj->handler(obj);
				return NULL;
			}
			if (prev_ctx->buffer.data[0] == ')') {
				obj->error_code = BRP_ERR_MACRO_ARG_COUNT_MISMATCH;
				obj->error_loc = prev_ctx->cur_loc;
				obj->error_n_args = input->locals.length;
				obj->error_macro_def_loc = macro.def_loc;
				obj->error_macro_name = macro.name;
				obj->error_macro_n_args = macro.args.length;
				obj->handler(obj);
				return NULL;
			}

			input->locals.data[i] = fetchMacroArg(obj, prev_ctx, macro.args.data[i]);
		}
		if (prev_ctx->buffer.length ? prev_ctx->buffer.data[0] != ')' : true) {
			obj->error_code = BRP_ERR_MACRO_ARG_COUNT_MISMATCH;
			obj->error_loc = prev_ctx->cur_loc;
			obj->error_n_args = -1;
			obj->error_macro_def_loc = macro.def_loc;
			obj->error_macro_name = macro.name;
			obj->error_macro_n_args = macro.args.length;
			obj->handler(obj);
			return NULL;
		}
		sbufshift(prev_ctx->buffer, 1);
		++prev_ctx->cur_loc.colno;
	} else if (macro.args.length > 0) {
		obj->error_code = BRP_ERR_MACRO_ARG_COUNT_MISMATCH;
		obj->error_loc = prev_ctx->cur_loc;
		obj->error_n_args = 0;
		obj->error_macro_def_loc = macro.def_loc;
		obj->error_macro_name = macro.name;
		obj->error_macro_n_args = macro.args.length;
		obj->handler(obj);
		return NULL;
	}

	preprocessInput(obj, input);
	return prev_ctx;
}

static void preprocessInput(BRP* obj, InputCtx* const input)
{
	while (!input->buffer.length) {
		if (!input->prev) return;
		delInput(obj, input);
	}
// stripping unwanted symbols from the start of the buffer
	cleanInput(obj, input);
// processing directives
	while (sbufcutc(&input->buffer, '#')) {
		input->cur_loc.colno += sbufstripl(&input->buffer, SPACE, TAB).length + 1;
		BRPDirectiveCode directive_id = sbufcutv(&input->buffer, brp_directives);

		static_assert(N_BRP_CMDS == 9, "not all BRP directives are handled in preprocessInput");
		switch (directive_id) {
			case BRP_COMMENT: {
				sbuf stub;
				sbufsplitv(&input->buffer, &stub, nl_arg);
				input->cur_loc.lineno++;
				input->cur_loc.colno = 1;
				break;
			}
			case BRP_INCLUDE: {
				input->cur_loc.colno += brp_directives[BRP_INCLUDE].length + sbufstripl(&input->buffer, SPACE, TAB).length;
				char path_start = sbufcutc(&input->buffer, CSBUF("<\""));
				if (!path_start) {
					obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
					obj->error_loc = input->cur_loc;
					obj->handler(obj);
					return;
				}
				sbuf path_spec;
				sbufsplit(&input->buffer, &path_spec, path_start == '"' ? DQUOTE : CSBUF('>'));
				input->cur_loc.colno += path_spec.length + 2;
				char path_spec_c[path_spec.length + 1];
				memcpy(path_spec_c, path_spec.data, path_spec.length);
				path_spec_c[path_spec.length] = '\0';

				for (InputCtx* ctx = input; ctx; ctx = ctx->prev) {
					if (pathEquals(path_spec_c, ctx->cur_loc.src_name)) path_spec.length = 0;
				}

				if (path_spec.length) {
					appendInput(
						obj,
						input,
						fopen(path_spec_c, "r"),
						(TokenLoc){ .src_name = tostr(path_spec), .lineno = 1, .colno = 1, .included_from = NULL },
						input->cur_loc
					);
					cleanInput(obj, input->prev);
					preprocessInput(obj, input);
				}
				break;
			}
			case BRP_MACRO:
			case BRP_DEFINE: {
				input->cur_loc.colno += brp_directives[directive_id].length + sbufstriplc(&input->buffer, " \t").length;
				Macro macro = {0};
				sbuf macro_name;
				sbuf delim = sbufsplit(&input->buffer, &macro_name, SPACE, TAB, NEWLINE, CSBUF('('));
				
				input->cur_loc.colno += delim.length + macro_name.length;
				macro.name = tostr(macro_name);
// parsing macro arguments declaration
				if (sbufeq(delim, '(')) {
					while (!sbufcutc(&input->buffer, ')')) {
						input->cur_loc.colno += sbufstriplc(&input->buffer, " \t").length;
						sbuf arg_name;
						MacroArg arg = { .def_loc = input->cur_loc };

						sbuf arg_name_delim = sbufsplit(&input->buffer, &arg_name, SPACE, TAB, NEWLINE, CSBUF(','), CSBUF(')'));
						input->cur_loc.colno += arg_name.length + arg_name_delim.length;
						arg.name = tostr(arg_name);
						if (sbufeq(arg_name_delim, NEWLINE)) {
							obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
							obj->error_loc = input->cur_loc;
							obj->handler(obj);
						} else if (sbufeq(arg_name_delim, ')')) {
							sbufshift(input->buffer, -1);
						} else if (!sbufeq(arg_name_delim, ',')) {
							sbuf leftovers;
							sbuf comma_or_bracket = sbufsplit(&input->buffer, &leftovers, NEWLINE, CSBUF(','), CSBUF(')'));
							if (!sbufspace(leftovers)) {
								obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
								obj->error_loc = input->cur_loc;
								obj->handler(obj);
								return;
							}

							input->cur_loc.colno += leftovers.length + comma_or_bracket.length;
							if (sbufeq(comma_or_bracket, ')')) {
								sbufshift(input->buffer, -1);
							} else if (!sbufeq(comma_or_bracket, ',')) {
								obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
								obj->error_loc = input->cur_loc;
								obj->handler(obj);
								return;
							}
						}
						MacroArgArray_append(&macro.args, arg);
					}
				}
				macro.def_loc = input->cur_loc;
// parsing macro definition
				if (directive_id == BRP_DEFINE) {
					if (!sbufeq(delim, NEWLINE)) {
						if (sbufsplitescv(&input->buffer, &macro.def, nl_arg) >= 0) input->cur_loc.lineno++;
						macro.def_loc.colno += sbufstriplv(&macro.def, obj->hidden_symbols).length;
						sbufstriprv(&macro.def, obj->hidden_symbols);

						sbuf macro_def = macro.def;
						sbufsub(macro_def, &macro.def, "\\\n", NEWLINE);
					}
					input->cur_loc.colno = 1;
					input->cur_loc.lineno += sbufcount_v(macro.def, nl_arg);
				} else {
					if (!sbufsplit(&input->buffer, &macro.def, CSBUF("\n#endmacro")).data) {
						obj->error_code = BRP_ERR_UNCLOSED_MACRO;
						obj->error_loc = input->cur_loc;
						obj->handler(obj);
						free(macro.name);
						return;
					}
					macro.def = sbufcopy(macro.def);
					input->cur_loc.colno = brp_directives[BRP_ENDMACRO].length + 2;
					input->cur_loc.lineno += sbufcount_v(macro.def, nl_arg) + 1;
				}

				arrayForeach(Macro, prev_macro, obj->macros) {
					if (sbufeq(macro.name, prev_macro->name)) {
						free(macro.name);
						macro.name = NULL;
						free(prev_macro->def.data);
						prev_macro->def = macro.def;
					}
				}
				if (macro.name) MacroArray_append(&obj->macros, macro);
				break;
			}
			case BRP_IFNDEF:
			case BRP_IFDEF: {
				input->cur_loc.colno += brp_directives[directive_id].length + sbufstripl(&input->buffer, SPACE, TAB).length;
				sbuf macro_name;
				sbufsplitv(&input->buffer, &macro_name, nl_arg);
				sbufstripl(&macro_name, SPACE, TAB);
				if (!macro_name.length) {
					obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
					obj->error_loc = input->cur_loc;
					obj->handler(obj);
					return;
				}

				bool defined = false;
				for (int i = 0; i < obj->macros.length; i++) {
					if (sbufeq(macro_name, obj->macros.data[i].name)) {
						defined = true;
						break;
					}
				}
				if (directive_id == BRP_IFNDEF) defined = !defined;

				if (defined) {
					TokenLocArray_append(&obj->conditional_blocks, input->cur_loc);
					input->cur_loc.lineno++;
					input->cur_loc.colno = 1;
				} else {
					TokenLoc cond_loc = input->cur_loc;
					input->cur_loc.lineno++;
					input->cur_loc.colno = 1;
					int block_level = 0;
					while (true) {
						sbuf line;
						if (sbufsplitv(&input->buffer, &line, nl_arg) < 0) {
							obj->error_code = BRP_ERR_UNCLOSED_CONDITION;
							obj->error_loc = cond_loc;
							obj->handler(obj);
							return;
						}

						input->cur_loc.lineno++;
						sbufshift(line, 1);
						if (sbufstartswith(line, brp_directives[BRP_IFDEF])) {
							block_level++;
						} else if (sbufstartswith(line, brp_directives[BRP_ENDIF])) {
							if (block_level == 0) break;
							block_level--;
						} else if (!block_level ? sbufstartswith(line, brp_directives[BRP_ELSE]) : false) {
							TokenLocArray_append(&obj->conditional_blocks, input->cur_loc);
							break;
						}
					}
				}
				break;
			}
			case BRP_ENDIF: {
				if (obj->conditional_blocks.length == 0) {
					obj->error_code = BRP_ERR_EXCESS_ENDIF;
					obj->error_loc = input->cur_loc;
					obj->handler(obj);
					return;
				}
				TokenLocArray_pop(&obj->conditional_blocks, -1);
				sbuf stub;
				sbufsplitv(&input->buffer, &stub, nl_arg);
				input->cur_loc.lineno++;
				break;
			}
			case BRP_ENDMACRO:
				obj->error_code = BRP_ERR_EXCESS_ENDMACRO;
				obj->error_loc = input->cur_loc;
				obj->handler(obj);
				return;
			case BRP_ELSE:
				if (obj->conditional_blocks.length == 0) {
					obj->error_code = BRP_ERR_EXCESS_ELSE;
					obj->error_loc = input->cur_loc;
					obj->handler(obj);
					return;
				}
				TokenLocArray_pop(&obj->conditional_blocks, -1);
				TokenLoc cond_loc = input->cur_loc;
				input->cur_loc.colno = 1;
				int block_level = 0;
				while (true) {
					sbuf line;
					if (sbufsplitv(&input->buffer, &line, nl_arg) < 0) {
						obj->error_code = BRP_ERR_UNCLOSED_CONDITION;
						obj->error_loc = cond_loc;
						obj->handler(obj);
						return;
					}

					input->cur_loc.lineno++;
					sbufshift(line, 1);
					if (sbufstartswith(line, brp_directives[BRP_IFDEF])) {
						block_level++;
					} else if (sbufstartswith(line, brp_directives[BRP_ENDIF])) {
						if (block_level == 0) break;
						block_level--;
					} else if (!block_level ? sbufstartswith(line, brp_directives[BRP_ELSE]) : false) {
						obj->error_code = BRP_ERR_EXCESS_ELSE;
						obj->error_loc = input->cur_loc;
						obj->handler(obj);
						return;
					}
				}
				break;
			default:
				obj->error_code = BRP_ERR_UNKNOWN_CMD;
				obj->error_loc = input->cur_loc;
				sbufsplitv(&input->buffer, &obj->error_symbol, nl_arg);
				obj->handler(obj);
				return;
		}
		cleanInput(obj, input);
	}
// replacing a macro name with its definition if one is found
	sbuf macro_name, buffer_view = input->buffer;
	sbuf delim = obj->symbols[sbufsplitv(&buffer_view, &macro_name, obj->symbols)];
	
	InputCtx* prev_ctx = NULL;
	for (int i = 0; i < input->locals.length; i++) {
		Macro macro = input->locals.data[i];
		if (sbufeq(macro_name, macro.name)) {
			prev_ctx = expandMacro(obj, input, macro);
			break;
		}
	}
	if (!prev_ctx) {
		for (int i = 0; i < obj->macros.length; i++) {
			Macro macro = obj->macros.data[i];
			if (sbufeq(macro_name, macro.name)) {
				prev_ctx = expandMacro(obj, input, macro);
				break;
			}
		}
	}
	if (!input->buffer.length) delInput(obj, input);
}

Token _fetchToken(BRP* const obj, InputCtx* const input, TokenArray* const pending)
{
	Token res = {0};
	res = TokenArray_pop(&obj->pending, 0);
	static_assert(N_TOKEN_TYPES == 6, "not all token types are handled");
	switch (res.type) {
		case TOKEN_NONE: break;
		case TOKEN_SYMBOL: if (isSymbolSpecHidden(obj->symbols[res.symbol_id])) break;
			return res;
		case TOKEN_STRING: if (res.string.length == 0) break;
			return res;
		case TOKEN_WORD: if (sbufspace(res.word)) break;
		case TOKEN_KEYWORD:
		case TOKEN_INT:
			return res;
	}

	preprocessInput(obj, input);
	if (!input->buffer.length) return (Token){ .type = TOKEN_NONE };

	res.type = TOKEN_WORD;
	sbuf new;
	int delim_id;
	if (input->buffer.data[0] == '-' && (input->buffer.length > 1 ? (input->buffer.data[1] >= '0' && input->buffer.data[1] <= '9') : false)) {
		sbufshift(input->buffer, 1);
		delim_id = sbufsplitv(&input->buffer, &new, obj->symbols);
		sbufshift(new, -1);
	} else {
		delim_id = sbufsplitv(&input->buffer, &new, obj->symbols);
	}

	if (new.length) {
		for (int i = 0; obj->keywords[i].data; i++) {
			if (sbufeq(obj->keywords[i], new)) {
				res.type = TOKEN_KEYWORD;
				res.keyword_id = i;
				break;
			}
		}
		if (res.type == TOKEN_WORD) {
			if (sbufint(new)) {
				res.type = TOKEN_INT;
				res.value = sbuftoint(new);
			}
		}
		if (res.type == TOKEN_WORD) res.word = tostr(new);

		res.loc = input->cur_loc;
		input->cur_loc.colno += new.length;

		if (delim_id >= 0) {
			if (delim_id == obj->_dquote_symbol_id) {
				sbuf str_literal;
				sbuf delim = sbufsplitesc(&input->buffer, &str_literal, DQUOTE, NEWLINE);
				if (sbufeq(delim, NEWLINE)) {
					obj->error_code = BRP_ERR_UNCLOSED_STR;
					obj->error_loc = input->cur_loc;
					obj->handler(obj);
					return (Token){0};
				}

				sbuf str_literal_res = str_literal;
				if (obj->flags & BRP_ESC_STR_LITERALS) str_literal_res = sbufunesc(str_literal, NULL);

				TokenArray_append(
					&obj->pending,
					(Token){
						.type = TOKEN_STRING,
						.loc = input->cur_loc,
						.string = str_literal_res
					}
				);
				input->cur_loc.colno += sbufutf8len(str_literal) + DQUOTE.length;
			} else if (delim_id == obj->_quote_symbol_id) {
				sbuf char_literal;
				sbuf delim = sbufsplitesc(&input->buffer, &char_literal, QUOTE, NEWLINE);
				if (sbufeq(delim, NEWLINE)) {
					obj->error_code = BRP_ERR_UNCLOSED_CHAR;
					obj->error_loc = input->cur_loc;
					obj->handler(obj);
					return (Token){0};
				}
				char unesc_buffer[sizeof(res.value)] = {0};
				sbuf unesc = { .data = unesc_buffer, .length = sizeof(unesc_buffer) };
				sbufunesc(char_literal, &unesc);
				if (IS_BIG_ENDIAN) reverseByteOrder(unesc.data, unesc.length);

				TokenArray_append(
					&obj->pending,
					(Token){
						.type = TOKEN_INT,
						.loc = input->cur_loc,
						.value = *(int64_t*)unesc_buffer
					}
				);
				input->cur_loc.colno += sbufutf8len(char_literal) + QUOTE.length;
			} else if (!isSymbolSpecHidden(obj->symbols[delim_id])) {
				TokenArray_append(
					&obj->pending,
					(Token){
						.type = TOKEN_SYMBOL,
						.loc = input->cur_loc,
						.symbol_id = delim_id
					}
				);
			} 
		}
	} else if (delim_id >= 0) {
		if (delim_id == obj->_dquote_symbol_id) {
			sbuf str_literal;
			sbuf delim = sbufsplitesc(&input->buffer, &str_literal, DQUOTE, NEWLINE);
			if (sbufeq(delim, NEWLINE)) {
				obj->error_code = BRP_ERR_UNCLOSED_STR;
				obj->error_loc = input->cur_loc;
				obj->handler(obj);
				return (Token){0};
			}

			sbuf str_literal_res = str_literal;
			if (obj->flags & BRP_ESC_STR_LITERALS) str_literal_res = sbufunesc(str_literal, NULL);


			res.type = TOKEN_STRING;
			res.loc = input->cur_loc;
			res.string = str_literal_res;
			input->cur_loc.colno += sbufutf8len(str_literal) + DQUOTE.length;
		} else if (delim_id == obj->_quote_symbol_id) {
			sbuf char_literal;
			sbuf delim = sbufsplitesc(&input->buffer, &char_literal, QUOTE, NEWLINE);
			if (sbufeq(delim, NEWLINE)) {
				obj->error_code = BRP_ERR_UNCLOSED_CHAR;
				obj->error_loc = input->cur_loc;
				obj->handler(obj);
				return (Token){0};
			}
			char unesc_buffer[sizeof(res.value)] = {0};
			sbuf unesc = { .data = unesc_buffer, .length = sizeof(unesc_buffer) };
			sbufunesc(char_literal, &unesc);
			if (IS_BIG_ENDIAN) reverseByteOrder(unesc.data, unesc.length);

			res.type = TOKEN_INT;
			res.loc = input->cur_loc;
			res.value = *(int64_t*)unesc_buffer;
			input->cur_loc.colno += sbufutf8len(char_literal) + QUOTE.length;
		} else {
			res.type = TOKEN_SYMBOL;
			res.symbol_id = delim_id;
			res.loc = input->cur_loc;
		}
	} else return (Token){ .type = TOKEN_NONE, .loc = input->cur_loc };

	if (delim_id >= 0) {
		if (obj->symbols[delim_id].data[0] == '\n') {
			input->cur_loc.lineno++;
			input->cur_loc.colno = 1;
		} else {
			input->cur_loc.colno += obj->symbols[delim_id].length;
		}
	}

	return res;
}

Token peekToken(BRP* obj)
{
	Token res = {0};
	if (obj->pending.length) {
		return obj->pending.data[0];
	} else {
		res = fetchToken(obj);
		if (obj->pending.length) {
			Token swapped = obj->pending.data[0];
			obj->pending.data[0] = res;
			TokenArray_append(&obj->pending, swapped);
		} else {
			TokenArray_append(&obj->pending, res);
		}
	}
	return res;
}

bool unfetchToken(BRP* obj, Token token)
{
	return TokenArray_insert(&obj->pending, (TokenArray){ .data = &token, .length = 1 }, 0);
}

void fprintTokenLoc(FILE* fd, TokenLoc loc)
{
	if (loc.src_name) {
		if (loc.included_from) {
			fprintTokenLoc(fd, *loc.included_from);
			fputs("-> ", fd);
		}
		fprintf(fd, "[%s:%d:%d] ", loc.src_name, loc.lineno, loc.colno);
	}
}

void fprintTokenStr(FILE* fd, Token token, BRP* obj)
{
	switch (token.type) {
		case TOKEN_WORD: fprintf(fd, "word `%s`", token.word); break;
		case TOKEN_INT: fprintf(fd, "integer %lld", token.value); break;
		case TOKEN_STRING:
			fprintf(fd, "string \"");
			fputsbufesc(fd, token.string, BYTEFMT_HEX);
			fputc('"', fd);
			break;
		case TOKEN_KEYWORD: fprintf(fd, "keyword `"sbuf_format"`", unpack(obj->keywords[token.keyword_id])); break;
		case TOKEN_SYMBOL: fprintf(fd, "symbol `"sbuf_format"`", unpack(obj->symbols[token.symbol_id])); break;
		default: fprintf(fd, "nothing");
	}
}

void fprintToken(FILE* fd, Token token, BRP* obj)
{
	fprintTokenLoc(fd, token.loc);
	fprintTokenStr(fd, token, obj);
	fputc('\n', fd);
}

int getTokenSymbolId(Token token)
{
	return token.type == TOKEN_SYMBOL ? token.symbol_id : -1;
}

int getTokenKeywordId(Token token)
{
	return token.type == TOKEN_KEYWORD ? token.keyword_id : -1;
}

char* TokenTypeNames[N_TOKEN_TYPES] = {
	"nothing",
	"word",
	"keyword",
	"symbol",
	"integer",
	"string"
};

char* getTokenTypeName(TokenType type)
{
	return TokenTypeNames[type];
}

char* getTokenWord(BRP* obj, Token token)
{
	switch (token.type) {
		case TOKEN_WORD:
		case TOKEN_STRING:
			return token.word;
		case TOKEN_KEYWORD:
			return obj->keywords[token.keyword_id].data;
		case TOKEN_SYMBOL:
			return obj->symbols[token.symbol_id].data;
		default: return NULL;
	}
}

void printBRPErrorStr(FILE* fd, BRP* obj) {
	static_assert(N_BRP_ERRORS == 14, "not all BRP errors are handled in printBRPErrorStr");
	switch (obj->error_code) {
		case BRP_ERR_OK: break;
		case BRP_ERR_UNCLOSED_STR: 
			fprintf(fd, "unclosed string literal ");
			break;
		case BRP_ERR_NO_MEMORY: 
			fprintf(fd, "memory allocation failure (reason: %s) ", strerror(errno));
			break;
		case BRP_ERR_FILE_NOT_FOUND:
			fprintf(fd, "could not open file `%.*s` (reason: %s) ", unpack(obj->error_symbol), strerror(errno));
			break;
		case BRP_ERR_SYMBOL_CONFLICT:
			fprintf(
				fd,
				"symbol conflict: symbol `%.*s` will be incorrectly parsed; to solve this, place it before the symbol that has a common start ",
				unpack(obj->error_symbol)
			);
			break;
		case BRP_ERR_INVALID_CMD_SYNTAX:
			fprintf(fd, "invalid BRP directive syntax ");
			break;
		case BRP_ERR_UNKNOWN_CMD:
			fprintf(fd, "unknown BRP directive `%.*s` ", unpack(obj->error_symbol));
			break;
		case BRP_ERR_UNCLOSED_MACRO:
			fprintf(fd, "macros defined with `#macro` must be closed with the `#endmacro` directive ");
			break;
		case BRP_ERR_EXCESS_ENDIF:
			fprintf(fd, "excess `#endif` directive ");
			break;
		case BRP_ERR_EXCESS_ENDMACRO:
			fprintf(fd, "excess `#endmacro` directive ");
			break;
		case BRP_ERR_UNCLOSED_CONDITION:
			fprintf(fd, "unclosed conditional block; to close it, use the `#endif` dirrective ");
			break;
		case BRP_ERR_EXCESS_ELSE:
			fprintf(fd, "excess `#else` directive ");
			break;
		case BRP_ERR_UNCLOSED_CHAR:
			fprintf(fd, "unclosed character literal ");
			break;
		case BRP_ERR_MACRO_ARG_COUNT_MISMATCH:
			fprintf(fd, "macro `%s`, defined at ", obj->error_macro_name);
			fprintTokenLoc(fd, obj->error_macro_def_loc);
			fprintf(fd, ", expects %d arguments, instead got ", obj->error_macro_n_args);
			if (obj->error_n_args < 0) {
				fputs("more ", fd);
			} else {
				fprintf(fd, "%d ", obj->error_n_args);
			}
			break;
		default: fprintf(fd, "unreachable");
	}
}

void printBRPError(BRP* obj)
{
	if (obj->error_code) {
		fprintTokenLoc(stderr, obj->error_loc);
		fputs("preprocessing error: ", stderr);
		printBRPErrorStr(stderr, obj);
		fputc('\n', stderr);
		exit(1);
	}
}

BRP* initBRP(BRP* obj, BRPErrorHandler handler, char flags)
{
	*obj = (BRP){0};
	obj->handler = handler ? handler : printBRPError;
	obj->flags = flags;
	obj->macros = MacroArray_new(
		3,
		(Macro){ .name = "__BRC__", .def = {0}, .def_loc = {0} },
		(Macro){0}, // OS indicator macro placeholder
		(Macro){0} // platform (POSIX | NT) indicator macro placeholder
	);

#if defined(__APPLE__) || defined(__MACH__)
	obj->macros.data[1] = (Macro){ .name = "__APPLE__", .def = {0}, .def_loc = {0} };
	obj->macros.data[2] = (Macro){ .name = "__POSIX__", .def = {0}, .def_loc = {0} };
#elif defined(_WIN32)
	obj->macros.data[1] = (Macro){ .name = "_WIN32", .def = {0}, .def_loc = {0} };
	obj->macros.data[2] = (Macro){ .name = "__NT__", .def = {0}, .def_loc = {0} };
#elif defined(__linux__)
	obj->macros.data[1] = (Macro){ .name = "__linux__", .def = {0}, .def_loc = {0} };
	obj->macros.data[2] = (Macro){ .name = "__POSIX__", .def = {0}, .def_loc = {0} };
#endif

	return obj;
}

void delBRP(BRP* obj)
{
	free(obj->keywords);
	free(obj->symbols);
	free(obj->hidden_symbols);
	TokenArray_clear(&obj->pending);
	MacroArray_clear(&obj->macros);
	TokenLocArray_clear(&obj->conditional_blocks);

	while (obj->cur_input.prev) delInput(obj, &obj->cur_input);
}

#endif // _BRP_
