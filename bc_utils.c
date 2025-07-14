#define _CRT_SECURE_NO_WARNINGS
#include "bc_utils.h"

extern DWORD viewerLimit;
extern PBYTE viewerContent;

//static void bufferAppend(PBYTE buffer, LPSTR text, PDWORD bufferLimit) {
//	DWORD textLen = strlen(text);
//	DWORD bufferLen = strlen(viewerContent);
//
//	while ((bufferLen + textLen) >= viewerLimit) {
//		viewerLimit *= 2;
//		viewerContent = realloc(viewerContent, viewerLimit);
//	}
//
//	memcpy(viewerContent + bufferLen, text, textLen);
//	viewerContent[bufferLen + textLen] = 0;
//}
//
//void printFormat(const char* fText, ...) {
//	va_list args;
//	PBYTE buffer[1024];
//
//	va_start(args, fText);
//
//	vsprintf(buffer, fText, args);
//
//	va_end(args);
//
//	bufferAppend(viewerContent, buffer, &viewerLimit);
//}

DWORD getFileSize(FILE* fStream) {
	DWORD size;
	DWORD currentOffset = ftell(fStream);

	fseek(fStream, 0, SEEK_END);
	size = ftell(fStream);
	fseek(fStream, currentOffset, SEEK_SET);

	return size;
}

PBYTE dumpFile(FILE *fStream) {
	DWORD fileSize = getFileSize(fStream);
	PBYTE buffer = (PBYTE)malloc(sizeof(BYTE) * fileSize);

	DWORD bytesRead = fread(buffer, sizeof(BYTE), fileSize, fStream);

	if (bytesRead != fileSize) {
		//printf("Read not all the file! %s\n", __func__);
	}

	return buffer;
}

reader* readerInit(LPSTR fileName) {
	reader* pr = (reader *)malloc(sizeof(reader));
	if (!pr) {
		//printf("Couldn't init reader\n");
		exit(1);
	}

	FILE* fStream = fopen(fileName, "rb");
	
	pr->pdata = pr->current = dumpFile(fStream);
	pr->size = getFileSize(fStream);
	pr->pc = 1;

	fclose(fStream);

	return pr;
}

void readerFree(reader* pReader) {
	if (pReader) {
		free(pReader);
	}
}

BYTE readByte(reader* pReader) {
	if (pReader->current > (pReader->pdata + pReader->size)) {

		//printf("Got the end of reader data!\n");
		//exit(1);
	}
	return *pReader->current++;
}

SIZE_T readVariant(reader* pReader, SIZE_T limit) {
	SIZE_T x = 0;
	int b;
	limit >>= 7;
	do {
		b = readByte(pReader);
		/*if (x > limit) {
			printf("Integer overflow!\n");
			//exit(1);
		}*/
		x = (x << 7) | (b & 0x7f);
	} while ((b & 0x80) != 0);
	return x;
}

////Dword in hex editor of luac is reversed in memory
//DWORD readDword(reader* pReader) {
//	/*DWORD res = 0;
//
//	for (DWORD i = 0; i < 4; i++) {
//		BYTE b = readByte(pReader);
//		res |= (b << (8 * i));
//	}*/
//
//	return readVariant(pReader, ~sizeof(DWORD));
//}
//
//WORD readWord(reader* pReader) {
//	/*WORD res = 0;
//
//	res |= readByte(pReader);
//	res |= readByte(pReader) << 8;
//	*/
//	return readVariant(pReader, ~sizeof(WORD));
//}

#define MAX_SIZET	((SIZE_T)(~(SIZE_T)0))

SIZE_T readSize(reader* pReader) {
	SIZE_T res = 0;

	int b;
	SIZE_T limit = MAX_SIZET;
	limit >>= 7;
	do {
		b = readByte(pReader);
		if (res > limit)
			//printf("integer overflow");
		res = (res << 7) | (b & 0x7f);
	} while ((b & 0x80) != 0);

	return res;
}

//void readString(reader* pReader, string_t *string) {
//	SIZE_T stringSize = readSize(pReader);
//	string->size = 0;
//	string->str = (PCHAR)malloc(sizeof(CHAR) * stringSize);
//
//	if (!string->str) {
//		//printf("Couldn't read string\n");
//		//exit(1);
//	}
//
//	for (SIZE_T i = 0; i < stringSize; i++) {
//		string->str[string->size++] = readByte(pReader);
//	}
//}

//Maybe data block should be reversed do to little big endian or smth like that
void readBlock(reader* pReader, void* pBuffer, SIZE_T elementSize, SIZE_T bufferSize) {
	if ((pReader->current + bufferSize) >= (pReader->pdata + pReader->size)) {
		//printf("Got the end of file!\n");
		//exit(1);
	}
	memcpy(pBuffer, pReader->current, bufferSize);
	pReader->current += bufferSize;
}

DWORD readUleb128(reader* pReader) {
	int v = readByte(pReader);
	if (v >= 0x80) {
		v &= 0x7f;
		int off = 1;
		do {
			v |= ((readByte(pReader) & 0x7f) << (off * 7));
			off++;
		} while (*(pReader->current-1) >= 0x80);
	}
	return v;
}

DWORD readUleb128_33(reader* pReader) {
	int v = readByte(pReader) >> 1;
	if (v >= 0x40) {
		v &= 0x3f;
		int off = -1;
		do {
			v |= ((readByte(pReader) & 0x7f) << (off += 7));
		} while (*(pReader->current-1) >= 0x80);
	}
	return v;
}

BOOL checkHeader(reader* pReader) {
	BOOL r = FALSE;
	if (pReader->pdata[0] == 0x1B &&
		pReader->pdata[1] == 0x4C &&
		pReader->pdata[2] == 0x4A) {
		r = TRUE;
	}
	pReader->current += 3;
	return r;
}

void readKtabk(reader* pReader) {
	DWORD tp = readUleb128(pReader);
	if (tp >= BCDUMP_KTAB_STR) {
		DWORD len = tp - BCDUMP_KTAB_STR;
		DWORD stringOffset = pReader->current - pReader->pdata;
		PBYTE pBuffer = (PBYTE)malloc(len);
		readBlock(pReader, pBuffer, 1, len);
		//printf("(Offset: 0x%X) KTAB_STR: \"%.*s\"\n", stringOffset, len, pBuffer);
		free(pBuffer);
	}
	else if (tp == BCDUMP_KTAB_INT) {
		//printf("KTAB_INT\n");
		readUleb128(pReader);
	}
	else if (tp == BCDUMP_KTAB_NUM) {
		//printf("KTAB_NUM\n");
		readUleb128(pReader);
		readUleb128(pReader);
	}
}

void readKgci(proto* prot, reader *pReader, const int i) {
	DWORD tp = readUleb128(pReader);
	DWORD constantOffset = pReader->current - pReader->pdata;

	if (tp >= BCDUMP_KGC_STR) {
		DWORD len = tp - BCDUMP_KGC_STR;
		PBYTE buffer = (PBYTE)malloc(len + 1);

		readBlock(pReader, buffer, 1, len);

		buffer[len] = 0;
		
		prot->kgc[i].stringLen = len;
		prot->kgc[i].str = buffer;
		prot->kgc[i].fileOffset = constantOffset;
		prot->kgc[i].type = BCDUMP_KGC_STR;
		prot->kgc[i].constantStr = (PBYTE)malloc(64);
		
		sprintf(prot->kgc[i].constantStr, "STRING");
	}
	else if (tp == BCDUMP_KGC_TAB) {
		prot->kgc[i].constantStr = (PBYTE)malloc(64);
		prot->kgc[i].fileOffset = constantOffset;
		prot->kgc[i].type = BCDUMP_KGC_TAB;
		sprintf(prot->kgc[i].constantStr, "TABLE");

		DWORD karray_len = readUleb128(pReader);
		DWORD khash_len = readUleb128(pReader);

		for (int karr_i = 0; karr_i < karray_len; karr_i++) {
			readKtabk(pReader);
		}
		for (int khash_i = 0; khash_i < khash_len; khash_i++) {
			readKtabk(pReader);
			readKtabk(pReader);
		}
	}
	else if (tp == BCDUMP_KGC_CHILD) {
		prot->kgc[i].constantStr = (PBYTE)malloc(64);
		prot->kgc[i].fileOffset = constantOffset;
		prot->kgc[i].type = BCDUMP_KGC_CHILD;
		sprintf(prot->kgc[i].constantStr, "CHILD");
	}
	else {
		prot->kgc[i].constantStr = (PBYTE)malloc(128);
		prot->kgc[i].fileOffset = constantOffset;
		prot->kgc[i].type = BCDUMP_KGC_I64;

		int low = readUleb128(pReader), high = readUleb128(pReader);
		long long v = ((long long)(low) << 32) | ((long long)(high));

		if (tp == BCDUMP_KGC_COMPLEX) {
			int ilow = readUleb128(pReader), ihigh = readUleb128(pReader);
			long long vh = ((long long)(ilow) << 32) | ((long long)(ihigh));

			prot->kgc[i].type = BCDUMP_KGC_COMPLEX;
			prot->kgc[i].value = vh;
			sprintf(prot->kgc[i].constantStr, "INT128");
		}
		else {
			prot->kgc[i].value = v;
			sprintf(prot->kgc[i].constantStr, "INT64");
		}
	}
}
//const char* BC_NAMES = "ISLT  ISGE  ISLE  ISGT  ISEQV ISNEV ISEQS ISNES ISEQN ISNEN ISEQP ISNEP ISTC  ISFC  IST   ISF   MOV   NOT   UNM   LEN   ADDVN SUBVN MULVN DIVVN MODVN ADDNV SUBNV MULNV DIVNV MODNV ADDVV SUBVV MULVV DIVVV MODVV POW   CAT   KSTR  KCDATAKSHORTKNUM  KPRI  KNIL  UGET  USETV USETS USETN USETP UCLO  FNEW  TNEW  TDUP  GGET  GSET  TGETV TGETS TGETB TSETV TSETS TSETB TSETM CALLM CALL  CALLMTCALLT ITERC ITERN VARG  ISNEXTRETM  RET   RET0  RET1  FORI  JFORI FORL  IFORL JFORL ITERL IITERLJITERLLOOP  ILOOP JLOOP JMP   FUNCF IFUNCFJFUNCFFUNCV IFUNCVJFUNCVFUNCC FUNCCW";
const char* opcodeNames[] = {
	"ISLT", "ISGE", "ISLE", "ISGT",
	"ISEQV", "ISNEV", "ISEQS", "ISNES",
	"ISEQN", "ISNEN", "ISEQP", "ISNEP",
	"ISTC", "ISFC", "IST", "ISF",
	"ISTYPE", "ISNUM", "MOV", "NOT",
	"UNM", "LEN", "ADDVN", "SUBVN",
	"MULVN", "DIVVN", "MODVN", "ADDNV",
	"SUBNV", "MULNV", "DIVNV", "MODNV",
	"ADDVV", "SUBVV", "MULVV", "DIVVV",
	"MODVV", "POW", "CAT", "KSTR",
	"KCDATA", "KSHORT", "KNUM", "KPRI",
	"KNIL", "UGET", "USETV", "USETS",
	"USETN", "USETP", "UCLO", "FNEW",
	"TNEW", "TDUP", "GGET", "GSET",
	"TGETV", "TGETS", "TGETB", "TGETR",
	"TSETV", "TSETS", "TSETB", "TSETM",
	"TSETR", "CALLM", "CALL", "CALLMT",
	"CALLT", "ITERC", "ITERN", "VARG",
	"ISNEXT", "RETM", "RET", "RET0",
	"RET1", "FORI", "JFORI", "FORL",
	"IFORL", "JFORL", "ITERL", "IITERL",
	"JITERL", "LOOP", "ILOOP", "JLOOP",
	"JMP", "FUNCF", "IFUNCF", "JFUNCF",
	"FUNCV", "IFUNCV", "JFUNCV", "FUNCC",
	"FUNCCW"
};

#define COLOR_JMP "\033[1;36m"      // Cyan for JMP-type instructions
#define COLOR_CALL "\033[1;35m"     // Magenta for CALL-type instructions
#define COLOR_RESET "\033[0m"       // Reset to default color

// Array of opcode names with color formatting
//const char* opcodeNames[] = {
//	"ISLT", "ISGE", "ISLE", "ISGT",
//	"ISEQV", "ISNEV", "ISEQS", "ISNES",
//	"ISEQN", "ISNEN", "ISEQP", "ISNEP",
//	"ISTC", "ISFC", "IST", "ISF",
//	"ISTYPE", "ISNUM", "MOV", "NOT",
//	"UNM", "LEN", "ADDVN", "SUBVN",
//	"MULVN", "DIVVN", "MODVN", "ADDNV",
//	"SUBNV", "MULNV", "DIVNV", "MODNV",
//	"ADDVV", "SUBVV", "MULVV", "DIVVV",
//	"MODVV", "POW", "CAT", "KSTR",
//	"KCDATA", "KSHORT", "KNUM", "KPRI",
//	"KNIL", "UGET", "USETV", "USETS",
//	"USETN", "USETP", "UCLO", "FNEW",
//	"TNEW", "TDUP", "GGET", "GSET",
//	"TGETV", "TGETS", "TGETB", "TGETR",
//	"TSETV", "TSETS", "TSETB", "TSETM",
//	"TSETR",
//	COLOR_CALL "CALLM" COLOR_RESET, COLOR_CALL "CALL" COLOR_RESET, COLOR_CALL "CALLMT" COLOR_RESET,
//	COLOR_CALL "CALLT" COLOR_RESET, "ITERC", "ITERN", "VARG",
//	"ISNEXT", "RETM", "RET", "RET0",
//	"RET1", "FORI", "JFORI", "FORL",
//	"IFORL", "JFORL", "ITERL", "IITERL",
//	"JITERL",
//	COLOR_JMP "LOOP" COLOR_RESET, COLOR_JMP "ILOOP" COLOR_RESET, COLOR_JMP "JLOOP" COLOR_RESET,
//	COLOR_JMP "JMP" COLOR_RESET, "FUNCF", "IFUNCF", "JFUNCF",
//	"FUNCV", "IFUNCV", "JFUNCV", "FUNCC",
//	"FUNCCW"
//};

const char* get_opcode_name(BCOp opcode) {
	if (opcode >= 97) {
		return "UNKNOWN";
	}
	else {
		return opcodeNames[opcode];
	}
	/*if (opcode >= BC__MAX) {
		char* buff = (char*)malloc(14);
		sprintf(buff, "UNKNOWN<%d>\0", opcode);
		return buff;
	}
	const char* const_op_name = BC_NAMES + (opcode * 6);
	char* op_name = (char*)malloc(7);
	memcpy(op_name, const_op_name, 7);
	op_name[6] = '\0';
	return op_name;*/
}

void readProto(lua_bytecode* lbc, int protoId) {
	DWORD proto_len ;

	if (!(proto_len = readUleb128(lbc->pReader))) { //EOF
		return;
	}

	DWORD pflags			= readByte(lbc->pReader);
	DWORD numparams			= readByte(lbc->pReader);
	DWORD framesize			= readByte(lbc->pReader);
	DWORD sizeuv			= readByte(lbc->pReader);
	DWORD sizekgc			= readUleb128(lbc->pReader);
	DWORD sizekn			= readUleb128(lbc->pReader);
	DWORD sizebc			= readUleb128(lbc->pReader);
	DWORD protoStartOffset	= lbc->pReader->current - lbc->pReader->pdata;

	proto* prot = (proto*)malloc(sizeof(proto));
	
	prot->protoLen = proto_len;
	prot->flags = pflags;
	prot->paramsCount = numparams;
	prot->frameSize = framesize;
	prot->sizeUV = sizeuv;
	prot->sizeKGC = sizekgc;
	prot->sizeKN = sizekn;
	prot->sizeBC = sizebc;
	prot->id = protoId;
	prot->next = NULL;
	prot->fileOffset = protoStartOffset;

	int sizedbg = 0;
	if (!(lbc->pReader->bcFlags & BCDUMP_F_STRIP)) {
		sizedbg = readUleb128(lbc->pReader);
		if (sizedbg) {
			int firstline = readUleb128(lbc->pReader);
			int numline = readUleb128(lbc->pReader);
		}
	}

	prot->bc = (opcode*)malloc(sizeof(opcode) * sizebc);
	DWORD* code = (DWORD*)malloc(sizebc * sizeof(DWORD));
	readBlock(lbc->pReader, code, sizeof(DWORD), sizebc * sizeof(DWORD));

	prot->upvalues = (WORD*)malloc(sizeof(WORD) * sizeuv);
	readBlock(lbc->pReader, prot->upvalues, sizeof(WORD), sizeuv * sizeof(WORD));

	prot->constantsCount = sizekgc + sizekn;
	prot->kgc = (constant*)malloc(sizeof(constant) * prot->sizeKGC);
	prot->kn = (constant*)malloc(sizeof(constant) * prot->sizeKN);

	for (DWORD i = 0; i < sizekgc; i++) {
		readKgci(prot, lbc->pReader, i);
	}

	for (DWORD i = 0; i < sizekn; i++) {
		BYTE is64 = (*lbc->pReader->current) & 1;
		DWORD low = readUleb128_33(lbc->pReader);
		prot->kn[i].constantStr = (PBYTE)malloc(64);
		if (is64) {
			DWORD high = readUleb128(lbc->pReader);
			long long v = ((long long)(low) << 32) | high;
			sprintf(prot->kn[i].constantStr, "%d\tINT64<%ld>", i, v);
		}
		else {
			sprintf(prot->kn[i].constantStr, "%d\tINT32<%d>", i, low);
		}
	}

	if (!(lbc->pReader->bcFlags & BCDUMP_F_STRIP)) {
		PBYTE buffer_2 = (PBYTE)malloc(sizedbg);
		readBlock(lbc->pReader, buffer_2, 1, sizedbg);
		free(buffer_2);
	}

	/* Print bytecode */
	for (size_t i = 0; i < sizebc; i++) {
		DWORD inst = code[i];
		DWORD op = get_op(inst);
		BYTE a = get_a(inst);
		BYTE b = get_b(inst);
		BYTE c = get_c(inst);
		WORD d = get_d(inst); 
		
		prot->bc[i].a = a;
		prot->bc[i].b = b;
		prot->bc[i].c = c;
		prot->bc[i].d = d;

		const char* op_name = get_opcode_name(op);
		prot->bc[i].opName = op_name;
		prot->bc[i].fileOffset = protoStartOffset + i * 4;
		prot->bc[i].opcodeHex = op;
		prot->bc[i].opPc = lbc->pReader->pc;
		//prot->bc[i].opcodeStr = (PBYTE)malloc(128);

		if (prot->bc[i].opcodeHex == 0x58) {
			prot->bc[i].jmpAddr = ((inst >> 16) - 0x7fff);//(lbc->pReader->pc - 1 + (inst >> 16) - 0x7fff);
			/*sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X> => 0x%X\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\n",
				protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, (lbc->pReader->pc - 1 + (inst >> 16) - 0x7fff) * 4, a, b, c, d);*/
		}
		else if (!strcmp(op_name, opcodeNames[0x27]) && d < sizekgc) {
			/*const char* constItself = kgcArr[d].pBuffer;
			DWORD constLen = kgcArr[d].bufferSize;
			constLen = (constLen > 64) ? 64 : constLen;
			if(constItself)
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\t;%.*s\n",
				protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, b, c, d, (constLen > strlen(constItself)) ? strlen(constItself) : constLen, constItself);
			else
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, b, c, d);*/
		}
		else {
			/*if (op >= 0 && op <= 13) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 14 && op <= 15) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, d);
			}
			else if (op >= 16 && op <= 21) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 22 && op <= 38) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, b, c, d);
			}
			else if (op >= 39 && op <= 55) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 56 && op <= 62) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, b, c, d);
			}
			else if (op == 63) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 64 && op <= 66) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, b, c, d);
			}
			else if (op >= 67 && op <= 68) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 69 && op <= 71) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tB=0x%X\tC=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, b, c, d);
			}
			else if (op >= 72 && op <= 88) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 89 && op <= 90) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a);
			}
			else if (op == 91) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 92 && op <= 93) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a);
			}
			else if (op == 94) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\tD=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a, d);
			}
			else if (op >= 95 && op <= 96) {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\tA=0x%X\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op, a);
			}
			else {
				sprintf(prot->bc[i].opcodeStr, "0x%X\t%.4X\tPC:%u\t%s<0x%X>\n",
					protoStartOffset + i * 4, i * 4, lbc->pReader->pc, op_name, op);
			}*/
		}
		lbc->pReader->pc++;
	}

	free(code);

	proto* top = lbc->protos;
	if (!top) {
		lbc->protos = prot;
	}
	else {
		//Getting last element of list
		while (lbc->protos->next) {
			lbc->protos = lbc->protos->next;
		}
		lbc->protos->next = prot;
		lbc->protos = top;
	}
	lbc->protosCount++;
	lbc->pReader->pc = 1;
}