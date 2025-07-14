#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

#define BCDUMP_F_BE     0x01
#define BCDUMP_F_STRIP  0x02
#define BCDUMP_F_FFI    0x04

#define BCDUMP_F_KNOWN  (BCDUMP_F_FFI*2-1)

#define mask_op (0xFF)
#define mask_a (0xFF<<8)
#define mask_c (0xFF<<16)
#define mask_b (0xFF<<24)
#define mask_d (0xFFFF<<16)

#define get_op(v) (BCOp)(v&mask_op)
#define get_a(v) (BYTE)((v&mask_a)>>8)
#define get_c(v) (BYTE)((v&mask_c)>>16)
#define get_b(v) (BYTE)((v&mask_b)>>24)
#define get_d(v) (WORD)((v&mask_d)>>16)

#define ClearMemory(p,s) memset(p,0,s)
#define BoolToStr(b) (b ? "true" : "false")

#define BGFG_RESET "\x1b[0m"

#define BG_BLACK "\x1b[40m"
#define BG_RED "\x1b[41m"
#define BG_WHITE "\x1b[47m"
#define BG_YELLOW "\x1b[43m"
#define BG_CYAN "\x1b[46m"
#define BG_MAGENTA "\x1b[45m"

#define FG_RED "\x1b[31m"
#define FG_WHITE "\x1b[37m"
#define FG_YELLOW "\x1b[33m"
#define FG_CYAN "\x1b[36m"
#define FG_MAGENTA "\x1b[35m"

typedef enum {
	BC_ISLT, BC_ISGE, BC_ISLE, BC_ISGT, BC_ISEQV, BC_ISNEV, BC_ISEQS, BC_ISNES, BC_ISEQN, BC_ISNEN,
	BC_ISEQP, BC_ISNEP, BC_ISTC, BC_ISFC, BC_IST, BC_ISF, BC_MOV, BC_NOT, BC_UNM, BC_LEN, BC_ADDVN,
	BC_SUBVN, BC_MULVN, BC_DIVVN, BC_MODVN, BC_ADDNV, BC_SUBNV, BC_MULNV, BC_DIVNV, BC_MODNV, BC_ADDVV,
	BC_SUBVV, BC_MULVV, BC_DIVVV, BC_MODVV, BC_POW, BC_CAT, BC_KSTR, BC_KCDATA, BC_KSHORT, BC_KNUM,
	BC_KPRI, BC_KNIL, BC_UGET, BC_USETV, BC_USETS, BC_USETN, BC_USETP, BC_UCLO, BC_FNEW, BC_TNEW,
	BC_TDUP, BC_GGET, BC_GSET, BC_TGETV, BC_TGETS, BC_TGETB, BC_TSETV, BC_TSETS, BC_TSETB, BC_TSETM,
	BC_CALLM, BC_CALL, BC_CALLMT, BC_CALLT, BC_ITERC, BC_ITERN, BC_VARG, BC_ISNEXT, BC_RETM, BC_RET,
	BC_RET0, BC_RET1, BC_FORI, BC_JFORI, BC_FORL, BC_IFORL, BC_JFORL, BC_ITERL, BC_IITERL, BC_JITERL,
	BC_LOOP, BC_ILOOP, BC_JLOOP, BC_JMP, BC_FUNCF, BC_IFUNCF, BC_JFUNCF, BC_FUNCV, BC_IFUNCV, BC_JFUNCV,
	BC_FUNCC, BC_FUNCCW,
	BC__MAX
} BCOp;
enum {
	BCDUMP_KGC_CHILD, BCDUMP_KGC_TAB, BCDUMP_KGC_I64, BCDUMP_KGC_U64,
	BCDUMP_KGC_COMPLEX, BCDUMP_KGC_STR
};
enum {
	BCDUMP_KTAB_NIL, BCDUMP_KTAB_FALSE, BCDUMP_KTAB_TRUE,
	BCDUMP_KTAB_INT, BCDUMP_KTAB_NUM, BCDUMP_KTAB_STR
};

typedef struct jmp_arrow_ {
	float xStart, yStart;
	float xEnd, yEnd;
	DWORD from, to;
	DWORD padding;
} jmp_arrow;

typedef struct reader_ {
	BYTE* pdata;
	BYTE* current;
	DWORD size;
	DWORD bcFlags;
	DWORD pc;
} reader;

typedef struct string_ {
	PCHAR str;
	DWORD size;
} string_t;

typedef struct opcode_ {
	DWORD r, g, blue, alpha;
	DWORD a, b, c, d;
	BYTE *opcodeStr;
	BYTE *opName;
	DWORD opcodeHex;
	DWORD opFileOffsets;
	DWORD opPc;
	signed int jmpAddr;
	DWORD fileOffset;
	DWORD protoOffset;
	WORD isTarget;
} opcode;

typedef struct constant_ {
	BYTE* constantStr; //constant itself representated as string
	DWORD fileOffset;
	DWORD stringLen;
	PBYTE str; // if type == KGC_STR then str != NULl
	DWORD value; // if type != KGC_STR
	SHORT type;
	DWORD id;
}constant;

typedef struct proto_ {
	DWORD fileOffset;
	DWORD protoLen;
	DWORD id;
	DWORD flags;
	DWORD paramsCount;
	DWORD frameSize;
	DWORD sizeUV;
	DWORD sizeKGC;
	DWORD sizeKN;
	DWORD sizeBC;
	opcode *bc;
	WORD* upvalues;
	constant* kn;
	constant* kgc;
	DWORD constantsCount;
	struct proto_* next;
} proto;

typedef struct lua_bytecode_ {
	BYTE bytecodeVersion;
	DWORD flags;
	DWORD protosCount;
	proto* protos; //list on top element
	reader* pReader;
	//BYTE* chunkName;
	//hex_view hexView; //TODO
} lua_bytecode;

DWORD getFileSize(FILE* fStream);
PBYTE dumpFile(FILE* fStream);
reader* readerInit(LPSTR fileName);
void readerFree(reader* pReader);
BYTE readByte(reader* pReader);
//DWORD readDword(reader* pReader);
SIZE_T readSize(reader* pReader);
//void readString(reader* pReader, string_t* string);
void readBlock(reader* pReader, void* pBuffer, SIZE_T elementSize, SIZE_T bufferSize);
DWORD readUleb128(reader* pReader);
DWORD readUleb128_33(reader* pReader);
BOOL checkHeader(reader* pReader);
void readProto(lua_bytecode* proto, int protoId);