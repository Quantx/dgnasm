// Pretend this is a 16-bit unix 6 system
#define int short
#define NULL 0

#define DBUG_TOK 1
#define DBUG_SYM 1

// How many symbols should we allocate for
#define MAX_SYMS 1024
#define MAX_LINE 256
#define MAX_TOKN 8    // The Unix6 'a.out' executable standard limits us to 8 characters
#define PAGESIZE 1023 // 2 KB (1 KW) of memory (1 mmu page)

// *** DGNova Instruction Parameter Formats ***
#define DGN_IONO  8 // I/O No accumulator
#define DGN_IO    9 // I/O
#define DGN_IOSK 10 // I/O Skip
#define DGN_FLOW 11 // Flow control
#define DGN_LOAD 12 // Memory access
#define DGN_MATH 13 // Arithmetic, Logic
// *** DGNova Extened Instruction Parameter Formats ***
#define DGN_TRAP 14 // Arithmetic no-op (No load, no skip)
#define DGN_CT   15 // Control, no accumulator or flags
#define DGN_CTA  16 // Control, acumulator
#define DGN_CTF  17 // Control, flag
#define DGN_CTAF 18 // Control, acumulator and flag
#define DGN_CTAA 19 // Control, two accumulators
// *** Assembler directive ***
#define ASM_TEXT 20 // .text   directive
#define ASM_DATA 21 // .data   directive
#define ASM_BSS  22 // .bss    directive
#define ASM_ZERO 23 // .zero   directive
#define ASM_GLOB 24 // .glob   directive
#define ASM_DEFN 25 // .define directive
// *** DGNova Constants ***
#define DGN_SKPC 26 // Arithmetic & Logic skip condition
#define DGN_HWID 27 // Nova Hardware ID
// *** Token types ***
#define TOK_NAME 28 // Named symbol (user label)
#define TOK_NUM  29 // Numberical constant
#define TOK_INDR 30 // Indirect bit token '@'
#define TOK_LABL 31 // Label token ':'
#define TOK_ARG  32 // Argument seperator token ','
#define TOK_BYLO 33 // Low  byte indicator '>'
#define TOK_BYHI 34 // High byte indicator '<'
#define TOK_EOL  35 // End of line token

// *** Symbols Type ***
#define SYM_BYTE  1 // Byte flag

#define SYM_ABS   0 // Constant value
#define SYM_TEXT  1 // Text pointer
#define SYM_DATA  2 // Data pointer
#define SYM_BSS   3 // Bss  pointer
#define SYM_ZERO  4 // Zero-page pointer
#define SYM_DEF   5 // Undefined symbol
#define SYM_ZDEF  6 // Undefined zero page symbol
#define SYM_FILE  7 // File seperator symbol

// *** Boolean Flags ***
#define FLG_GLOB 0b0000000000000001 // Force all undefined symbols to be global
#define FLG_PASS 0b0000000000000010 // Which pass are we on: pass 2 == Flag Set

struct symbol
{
    char name[MAX_TOKN]; // Symbol name
    unsigned char type; // Symbol type
    unsigned char file; // What file this symbol is local to (0 = global)
    unsigned int val; // Opcode, Address, Value, etc.
};

// Block of memory, used to hold newly defined blocks
struct memblock
{
    unsigned int data[PAGESIZE]; // Data
    struct memblock * next; // Next memblock in list
};

// Store a data segment
struct segment
{
    unsigned int pos; // Current position in the segment
    unsigned int max; // Highest position reached in the segment

    struct memblock * head; // Actuall data stored in segment
    struct memblock * rdir; // Redirection bits for the data

    unsigned char sym; // The symbol type
};

// Assembly fail function
void asmfail(char * msg);

// Unix system calls
void exit(int status);
int  open(const char * pathname, int flags);
int close(int fd);
int  read(int fd, void * buf, unsigned int count);
int write(int fd, void * buf, unsigned int count);
void * sbrk(int inc);
