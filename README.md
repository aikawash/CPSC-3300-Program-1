 behavioral simulation of a subset of SPARC-like instructions
 CPSC 3300, Fall 2020
 
 PROGRAM 1 - this is the simulator with sections missing for students to
    provide; the I/O is kept to allow for proper formats for autograding
 
  if you see something that you think is an error, please let me know;
    note that the use of global variables is intentional, even though it
    is a design style that is frowned upon
 
  subset features
    32 32-bit registers, r0 always 0
    no register windows
    4-bit condition code, NZVC (negative, zero, overflow, carry)
    32-bit word addressing rather than byte addressing
    no delayed branches
    displacements added to updated program counter
    shift count is least significant five bits in register
      or immediate value
    program starts execution at address zero
    ten instructions with all others interpreted as a halt
 
  simulator features
    command line file name for program in hex
    contents of memory echoed as they are read in
    final contents of registers and memory are printed on halt
    execution statistics are also printed on halt
 
  subset of SPARC instruction formats
 
   format 2:
                     +--+-+---+---+------------------------+
     branch is       |00|a|cnd|op2|..22-bit displacement...|
                     +--+-+---+---+------------------------+
     sethi is        |00|rdest|100|..22-bit displacement...|
                     +--+-----+---+------------------------+
   format 3:
                     +--+-----+------+-----+-+--------+-----+
     3-register is   |1x|rdest|.op3..|rsrc|0|asi/fpop|rsrc2|
                     +--+-----+------+-----+-+--------+-----+
     immediate is    |1x|rdest|.op3..|rsrc|1|signed 13-bit |
                     +--+-----+------+-----+-+--------------+
 
  ten SPARC-like instructions, with some having 3-register as well
    as register-immediate forms
 
    aa..a is 22-bit signed displacement or sethi constant
    ddddd is 5-bit rdest register specifier
    sssss is 5-bit rsrc register specifier
    ttttt is 5-bit rsrc2 register specifier
    ii.ii is 13-bit signed immediate value
 
    00 _____ ___ aaaaaaaaaaaaaaaaaaaaaa   branch/sethi format
       01000 010                          ba, branch always
       01011 010                          bge, branch greater than or equal
       ddddd 100                          sethi    (also set)
 
    1_ ddddd ______ sssss 0 00000000 ttttt   reg-reg format
    1_ ddddd ______ sssss 1 iiiiiiiiiiiii    reg-imm format
     0       000000                          add   (also inc)
     0       000010                          or    (also set, clr)
     0       000100                          sub   (also dec)
     0       010100                          subcc (also cmp)
     0       100101                          sll
     1       000000                          load
     1       000100                          store
 
 

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define MEM_SIZE 4096

/* memory */
unsigned mem[MEM_SIZE], /* main memory to hold instructions and data */
         word_count;    /* how many memory words to display at end */

/* registers */
unsigned halt     = 0, /* halt flag to halt the simulation */
	 pc       = 0, /* program counter register */
	 mar      = 0, /* memory address register */
	 mdr      = 0, /* memory data register */
   reg[32] = {0},/* general registers */
   cc       = 0, /* condition code */
   ir       = 0; /* instruction register */

/* function pointer to the instruction to execute */
void ( *inst )();

/* decoding variables */
unsigned rdest,       /* register identifier for destination register */
         rsrc,       /* register identifier for first source register */
         rsrc2,       /* register identifier for second source register */
         sethi_value, /* left-shifted value of 22-bit field */
         src1_value,  /* value from rsrc register */
         src2_value,  /* value from rsrc2 or sign-extended immediate */
         imm_flag;    /* flag to indicate immediate value */

int      signed_displacement, /* sign-extended displacement value */
         signed_value;        /* sign-extended immediate value */

/* instruction index values */
#define BA_TAKEN 0
#define BGE_UNTAKEN 1
#define BGE_TAKEN 2
#define SETHI 3
#define ADD 4
#define OR 5
#define SUB 6
#define SUBCC 7
#define SLL 8
#define LOAD 9
#define STORE 10
#define HALT 11
