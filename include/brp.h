// The BRidge Preprocessor
#ifndef _BRP_
#define _BRP_

#include "sbuf.h"
#include "datasets.h"
#include "errno.h"
#include "assert.h"

#ifdef _WIN32_
#define PATHSEP NT_PATHSEP
#define NEWLINE NT_NEWLINE
#else
#define PATHSEP POSIX_PATHSEP
#define NEWLINE POSIX_NEWLINE
#endif

#define DQUOTE fromcstr("\"")
#define NT_PATHSEP fromcstr("\\")
#define POSIX_PATHSEP fromcstr("/")
#define NT_NEWLINE fromcstr("\r\n")
#define POSIX_NEWLINE fromcstr("\n")
#define SPACE fromcstr(" ")
#define TAB fromcstr("\t")

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

typedef struct token {
	int8_t type;
	TokenLoc loc;
	union {
		char* word; // for TOKEN_WORD or TOKEN_STRING
		int64_t value; // for TOKEN_INT
		int64_t keyword_id; // for TOKEN_KEYWORD
		int64_t symbol_id; // for TOKEN_SYMBOL
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
	N_PARSER_ERRORS
} BRPError;

declArray(sbuf);
typedef struct input_ctx {
	struct input_ctx* prev;
	sbuf buffer;
	char* orig_data;
	TokenLoc cur_loc;
} InputCtx;

declQueue(Token);
typedef struct brp {
	sbuf* keywords;
	sbuf* symbols;
	sbuf* hidden_symbols;
	BRPError error_code;
	TokenLoc error_loc;
	union {
		sbuf error_symbol;
		char* error_filename;
		int32_t sys_errno;
	};
	void (*handler)(struct brp*);
	InputCtx* cur_input;
	TokenQueue pending;
	TokenLoc last_loc;
} BRP;
#define BRP_KEYWORD(spec) fromcstr(spec)
#define BRP_SYMBOL(spec) fromcstr(spec)
#define BRP_HIDDEN_SYMBOL(spec) ((sbuf){ .data = spec"\n", .length = sizeof(spec) - 1 })
// hidden symbols are delimiters that are not returned as generated tokens

#define BRPempty(prep) ((prep)->cur_input == NULL)
#define getTokenKeywordId(token) ( (token).type == TOKEN_KEYWORD ? (token).keyword_id : -1 )
#define getTokenSymbolId(token) ( (token).type == TOKEN_SYMBOL ? (token).symbol_id : -1 )
#define isWordToken(token) ( (token).type == TOKEN_WORD || (token).type == TOKEN_KEYWORD )
#define isSymbolSpecHidden(spec) (spec.data[spec.length] > 0)

typedef void (*BRPErrorHandler) (BRP*);

char* getNormPath(char* src);
BRP* initBRP(BRP* obj, BRPErrorHandler handler);
void delBRP(BRP* obj);

bool _setKeywords(BRP* obj, ...);
bool _setSymbols(BRP* obj, ...);
#define setKeywords(obj, ...) _setKeywords(obj, __VA_ARGS__, (sbuf){0})
#define setSymbols(obj, ...) _setSymbols(obj, __VA_ARGS__, (sbuf){0})
bool appendInputFrom(BRP *obj, char *path, FILE* input_fd, TokenLoc include_loc);
#define appendInput(obj, path, input_fd) appendInputFrom(obj, path, input_fd, (TokenLoc){0})
Token fetchToken(BRP* obj);
Token peekToken(BRP* obj);
bool unfetchToken(BRP* obj, Token token);

int fprintTokenLoc(FILE* fd, TokenLoc loc);
#define printTokenLoc(loc) fprintTokenLoc(stdout, loc)

int fprintTokenStr(FILE* fd, Token token, BRP* obj);
#define printTokenStr(token, parser) fprintTokenStr(stdout, token, parser)

int fprintToken(FILE* fd, Token token, BRP* obj);
#define printToken(token, parser) fprintToken(stdout, token, parser)

char* getTokenTypeName(TokenType type);
char* getTokenWord(BRP* obj, Token token);
void printBRPErrorStr(FILE* fd, BRP* obj);
void printBRPError(FILE* fd, BRP* obj);

#endif // _BRP_

#ifdef BRP_IMPLEMENTATION
#undef BRP_IMPLEMENTATION

defArray(sbuf);
defQueue(Token);

char* getNormPath(char* src)
{
	sbuf input = fromstr(src);

	sbuf res = smalloc(input.length + 1);
	if (!res.data) return NULL;
	memset(res.data, 0, res.length);

	sbufArray components = sbufArray_new(sbufcount(input, PATHSEP) * -1 - 1);
	sbuf new;
	
	while (input.length) {
		sbufsplit(&input, &new, PATHSEP);
		if (sbufeq(new, fromcstr(".."))) {
			sbufArray_pop(&components, -1);
		} else if ((new.length || !components.length) && !sbufeq(new, fromcstr("."))) {
			sbufArray_append(&components, new);
		}
	}

	res.length = 0;
	array_foreach(sbuf, component, components,
	 	if (res.length) {
			memcpy(res.data + res.length, PATHSEP.data, PATHSEP.length);
			res.length += PATHSEP.length;
		}
		memcpy(res.data + res.length, component.data, component.length);
		res.length += component.length;
	);
	free(components.data);
	return res.data;
}

bool pathEquals(char path1[], char path2[])
{
	char path1_r[256];
	char path2_r[256];
	realpath(path1, path1_r);
	realpath(path2, path2_r);
	return streq(path1_r, path2_r);
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
	obj->symbols = malloc(n_symbols * sizeof(sbuf));
	obj->hidden_symbols = malloc(n_hidden_symbols * sizeof(sbuf));
	if (!obj->symbols || !obj->hidden_symbols) return false;


	n_hidden_symbols = 0;
	for (int i = 0; i < n_symbols; i++) {
		obj->symbols[i] = va_arg(args, sbuf);
		if (obj->symbols[i].data ? isSymbolSpecHidden(obj->symbols[i]) : true) obj->hidden_symbols[n_hidden_symbols++] = obj->symbols[i];
	}
	va_end(args);

	for (int i = 0; i < n_symbols; i++) {
		for (int i1 = 0; i1 < i; i1++) {
			if (sbufstartswith(obj->symbols[i], obj->symbols[i1])) {
				obj->error_code = BRP_ERR_SYMBOL_CONFLICT;
				obj->error_symbol = obj->symbols[i];
				if (obj->handler) obj->handler(obj);
				return false;
			}
		}
	}

	return true;
}

bool appendInputFrom(BRP *obj, char* path, FILE* input_fd, TokenLoc include_loc)
{
	path = getNormPath(path);
	if (!input_fd) {
		obj->error_code = BRP_ERR_FILE_NOT_FOUND;
		obj->error_loc = include_loc;
		obj->error_symbol = fromstr(path);
		if (obj->handler) obj->handler(obj);
		return false;
	}
	InputCtx* init_ctx = malloc(sizeof(InputCtx));
	*init_ctx = (InputCtx){
		.cur_loc = {
			.src_name = path,
			.lineno = 1,
			.colno = 1,
			.included_from = NULL
		},
		.buffer = filecontent(input_fd),
		.prev = obj->cur_input
	};
	if (!init_ctx->buffer.data) {
		obj->error_code = BRP_ERR_NO_MEMORY;
		obj->error_loc = include_loc;
		if (obj->handler) obj->handler(obj);
		fclose(input_fd);
		return false;
	}
	if (include_loc.src_name) {
		init_ctx->cur_loc.included_from = malloc(sizeof(TokenLoc));
		*init_ctx->cur_loc.included_from = include_loc;
	}

	sbufstriprv(&init_ctx->buffer, obj->hidden_symbols);
	obj->cur_input = init_ctx;
	fclose(input_fd);
	return true;
}

void delInput(BRP* obj)
{
	free(obj->cur_input->orig_data);
	InputCtx* to_free = obj->cur_input;
	obj->cur_input = to_free->prev;
	free(to_free);
}

static sbuf nl_arg[2] = { NEWLINE };

void cleanInput(BRP* obj, InputCtx* ctx)
{
	sbuf stripped = sbufstriplv(&ctx->buffer, obj->hidden_symbols);
	int nl_count = sbufcount_v(stripped, nl_arg);
	if (nl_count) {
		ctx->cur_loc.lineno += nl_count;
		sbuf last_stripped_line;
		sbufsplitrv(&stripped, &last_stripped_line, nl_arg);
		ctx->cur_loc.colno = stripped.length + 1;
	} else ctx->cur_loc.colno += stripped.length;
}

#define BRP_INCLUDE fromcstr("include")
#define BRP_DEFINE fromcstr("define")

void preprocessInput(BRP* obj)
{
	if (!obj->cur_input) return;
	if (!obj->cur_input->buffer.length) {
		delInput(obj);
		return;
	}
// stripping unwanted symbols from the start of the buffer
	cleanInput(obj, obj->cur_input);
// processing directives
	while (sbufcutc(&obj->cur_input->buffer, fromcstr("#"))) {
		obj->cur_input->cur_loc.colno++;
		if (sbufcutc(&obj->cur_input->buffer, fromcstr("!"))) {
			sbuf stub;
			sbufsplitv(&obj->cur_input->buffer, &stub, nl_arg);
			obj->cur_input->cur_loc.lineno++;
			obj->cur_input->cur_loc.colno = 1;
		} else if (sbufcut(&obj->cur_input->buffer, BRP_INCLUDE).data) {
			obj->cur_input->cur_loc.colno += BRP_INCLUDE.length + sbufstripl(&obj->cur_input->buffer, SPACE, TAB).length;
			char path_start = sbufcutc(&obj->cur_input->buffer, fromcstr("<\""));
			if (!path_start) {
				obj->error_code = BRP_ERR_INVALID_CMD_SYNTAX;
				obj->error_loc = obj->cur_input->cur_loc;
				if (obj->handler) obj->handler(obj);
				return;
			}
			sbuf path_spec;
			sbufsplit(&obj->cur_input->buffer, &path_spec, path_start == '"' ? DQUOTE : fromchar('>'));
			obj->cur_input->cur_loc.colno += path_spec.length + 2;
			char path_spec_c[path_spec.length + 1];
			memcpy(path_spec_c, path_spec.data, path_spec.length);
			path_spec_c[path_spec.length] = '\0';

			for (InputCtx* ctx = obj->cur_input; ctx; ctx = ctx->prev) {
				if (pathEquals(path_spec_c, ctx->cur_loc.src_name)) path_spec.length = 0;
			}

			if (path_spec.length) {
				obj->error_loc = obj->cur_input->cur_loc;
				cleanInput(obj, obj->cur_input);
				appendInputFrom(obj, path_spec_c, fopen(path_spec_c, "r"), obj->error_loc);
			}
		} else {
			obj->error_code = BRP_ERR_UNKNOWN_CMD;
			sbufsplitv(&obj->cur_input->buffer, &obj->error_symbol, nl_arg);
			return;
		}
		cleanInput(obj, obj->cur_input);
	}

	if (!obj->cur_input->buffer.length) delInput(obj);
}

Token fetchToken(BRP* obj)
{
	Token res = {0};
	TokenQueue_fetch(&obj->pending, &res);
	static_assert(N_TOKEN_TYPES == 6, "not all token types are handled");
	switch (res.type) {
		case TOKEN_NONE: break;
		case TOKEN_SYMBOL: if (isSymbolSpecHidden(obj->symbols[res.symbol_id])) break;
			return res;
		case TOKEN_STRING:
		case TOKEN_WORD: if (sbufspace(fromstr(res.word))) break;
		case TOKEN_KEYWORD:
		case TOKEN_INT:
			return res;
	}

	preprocessInput(obj);
	if (obj->cur_input == NULL) return (Token){ .type = TOKEN_NONE, .loc = obj->last_loc };

	if (sbufcut(&obj->cur_input->buffer, DQUOTE).data) {
		sbuf new;
		sbuf delim = sbufsplitesc(&obj->cur_input->buffer, &new, DQUOTE);
		if (!sbufeq(delim, DQUOTE)) {
			obj->error_code = BRP_ERR_UNCLOSED_STR;
			obj->error_loc = obj->cur_input->cur_loc;
			if (obj->handler) obj->handler(obj);
			return (Token){ .type = TOKEN_NONE, .loc = obj->last_loc };
		}

		res.type = TOKEN_STRING;
		res.loc = obj->cur_input->cur_loc;
		res.word = tostr(new);

		obj->cur_input->cur_loc.colno += sbufutf8len(new) + 2;
		obj->last_loc = res.loc;
		return res;
	} else {
		res.type = TOKEN_WORD;
		sbuf new;
		int delim_id;
		if (obj->cur_input->buffer.data[0] == '-' && (obj->cur_input->buffer.length > 1 ? (obj->cur_input->buffer.data[1] >= '0' && obj->cur_input->buffer.data[1] <= '9') : false)) {
			sbufshift(obj->cur_input->buffer, 1);
			delim_id = sbufsplitv(&obj->cur_input->buffer, &new, obj->symbols);
			sbufshift(new, -1);
		} else {
			delim_id = sbufsplitv(&obj->cur_input->buffer, &new, obj->symbols);
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

			res.loc = obj->cur_input->cur_loc;
			obj->cur_input->cur_loc.colno += new.length;

            if (delim_id >= 0 ? !isSymbolSpecHidden(obj->symbols[delim_id]) : false) {
                TokenQueue_add(
                    &obj->pending,
                    (Token){
                        .type = TOKEN_SYMBOL,
                        .loc = obj->cur_input->cur_loc,
                        .symbol_id = delim_id
                    }
                );
            } 
		} else {
			res.type = delim_id == -1 ? TOKEN_NONE : TOKEN_SYMBOL;
			res.loc = obj->cur_input->cur_loc;
			res.symbol_id = delim_id;
		}

		if (obj->symbols[delim_id].data[0] == '\n') {
			obj->cur_input->cur_loc.lineno++;
			obj->cur_input->cur_loc.colno = 1;
		} else {
			obj->cur_input->cur_loc.colno += obj->symbols[delim_id].length;
		}

		obj->last_loc = res.loc;
		return res;
	}
}

Token peekToken(BRP* obj)
{
	Token res = {0};
	if (obj->pending.length) {
		TokenQueue_peek(&obj->pending, &res);
	} else {
		res = fetchToken(obj);
		if (obj->pending.length) {
			Token swapped;
			TokenQueue_fetch(&obj->pending, &swapped);
			TokenQueue_add(&obj->pending, res);
			TokenQueue_add(&obj->pending, swapped);
		} else {
			TokenQueue_add(&obj->pending, res);
		}
	}
	return res;
}

bool unfetchToken(BRP* obj, Token token)
{
	return TokenQueue_unfetch(&obj->pending, token);
}

int fprintTokenLoc(FILE* fd, TokenLoc loc)
{
	if (loc.src_name) {
		if (loc.included_from) {
			fprintTokenLoc(fd, *loc.included_from);
			fputs("-> ", fd);
		}
		return fprintf(fd, "[%s:%d:%d] ", loc.src_name, loc.lineno, loc.colno);
	} else { return fprintf(fd, " "); }
}

int fprintTokenStr(FILE* fd, Token token, BRP* obj)
{
	switch (token.type) {
		case TOKEN_WORD: return fprintf(fd, "word `%s`", token.word);
		case TOKEN_INT: return fprintf(fd, "integer %lld", token.value);
		case TOKEN_STRING: 
			return fprintf(fd, "string \"") + fputsbufesc(fd, fromstr(token.word), BYTEFMT_HEX) + fputc('"', fd);
		case TOKEN_KEYWORD: return fprintf(fd, "keyword `"sbuf_format"`", unpack(obj->keywords[token.keyword_id]));
		case TOKEN_SYMBOL: return fprintf(fd, "symbol `"sbuf_format"`", unpack(obj->symbols[token.symbol_id]));
		default:
			return fprintf(fd, "nothing");
	}
}

int fprintToken(FILE* fd, Token token, BRP* obj)
{
	int res = fprintTokenLoc(fd, token.loc);
	res += fprintTokenStr(fd, token, obj);
	res += fputc('\n', fd);
	return res;
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
	switch (obj->error_code) {
		case BRP_ERR_OK: break;
		case BRP_ERR_UNCLOSED_STR: 
			fprintf(fd, "unclosed string literal ");
			break;
		case BRP_ERR_NO_MEMORY: 
			fprintf(fd, "memory allocation failure ");
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
		default: fprintf(fd, "unreachable");
	}
}

void printBRPError(FILE* fd, BRP* obj)
{
	if (obj->error_code) {
		fprintTokenLoc(fd, obj->error_loc);
		printBRPErrorStr(fd, obj);
		fputc('\n', fd);
	}
}

BRP* initBRP(BRP* obj, BRPErrorHandler handler)
{
	*obj = (BRP){0};
	obj->pending = TokenQueue_new(0);
	obj->error_loc = (TokenLoc){0};
	obj->handler = handler;
	obj->cur_input = NULL;

	return obj;
}

void delBRP(BRP* obj)
{
	free(obj->keywords);
	free(obj->symbols);
	free(obj->hidden_symbols);
	TokenQueue_delete(&obj->pending);

	InputCtx* ctx = obj->cur_input;
	while (ctx) {
		InputCtx* prev = ctx->prev;
		free(ctx);
		ctx = prev;
	}
}

#endif // _BRP_