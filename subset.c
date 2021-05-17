/* behavioral simulation of a subset of SPARC-like instructions
 * CPSC 3300, Fall 2020
 *
 * PROGRAM 1 - this is the simulator with sections missing for students to
 *   provide; the I/O is kept to allow for proper formats for autograding
 *
 * if you see something that you think is an error, please let me know;
 *   note that the use of global variables is intentional, even though it
 *   is a design style that is frowned upon
 *
 * subset features
 *   32 32-bit registers, r0 always 0
 *   no register windows
 *   4-bit condition code, NZVC (negative, zero, overflow, carry)
 *   32-bit word addressing rather than byte addressing
 *   no delayed branches
 *   displacements added to updated program counter
 *   shift count is least significant five bits in register
 *     or immediate value
 *   program starts execution at address zero
 *   ten instructions with all others interpreted as a halt
 *
 * simulator features
 *   command line file name for program in hex
 *   contents of memory echoed as they are read in
 *   final contents of registers and memory are printed on halt
 *   execution statistics are also printed on halt
 *
 * subset of SPARC instruction formats
 *
 *  format 2:
 *                    +--+-+---+---+------------------------+
 *    branch is       |00|a|cnd|op2|..22-bit displacement...|
 *                    +--+-+---+---+------------------------+
 *    sethi is        |00|rdest|100|..22-bit displacement...|
 *                    +--+-----+---+------------------------+
 *  format 3:
 *                    +--+-----+------+-----+-+--------+-----+
 *    3-register is   |1x|rdest|.op3..|rsrc|0|asi/fpop|rsrc2|
 *                    +--+-----+------+-----+-+--------+-----+
 *    immediate is    |1x|rdest|.op3..|rsrc|1|signed 13-bit |
 *                    +--+-----+------+-----+-+--------------+
 *
 * ten SPARC-like instructions, with some having 3-register as well
 *   as register-immediate forms
 *
 *   aa..a is 22-bit signed displacement or sethi constant
 *   ddddd is 5-bit rdest register specifier
 *   sssss is 5-bit rsrc register specifier
 *   ttttt is 5-bit rsrc2 register specifier
 *   ii.ii is 13-bit signed immediate value
 *
 *   00 _____ ___ aaaaaaaaaaaaaaaaaaaaaa   branch/sethi format
 *      01000 010                          ba, branch always
 *      01011 010                          bge, branch greater than or equal
 *      ddddd 100                          sethi    (also set)
 *
 *   1_ ddddd ______ sssss 0 00000000 ttttt   reg-reg format
 *   1_ ddddd ______ sssss 1 iiiiiiiiiiiii    reg-imm format
 *    0       000000                          add   (also inc)
 *    0       000010                          or    (also set, clr)
 *    0       000100                          sub   (also dec)
 *    0       010100                          subcc (also cmp)
 *    0       100101                          sll
 *    1       000000                          load
 *    1       000100                          store
 *
 */

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

char     inst_name[12][12],  /* instruction names for display */
         inst_flags[12][4] = /* flags to control what info is displayed; */
                             /*   fourth flag reserved for future use */
         { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {1,0,0,3},
           {1,1,1,3}, {1,1,1,3}, {1,1,1,3}, {1,1,1,2},
           {1,1,1,3}, {1,1,1,2}, {1,1,1,2}, {0,0,0,3} };

unsigned inst_number,          /* index into instruction info arrays */
         inst_fetches = 0,     /* execution statistics */
         memory_reads = 0,
         memory_writes = 0,
         inst_count[12] = {0};


/* initialization routine to set up instruction names */

void init_inst_names(){
  strcpy( inst_name[0], "ba taken" );
  strcpy( inst_name[1], "bge untaken" );
  strcpy( inst_name[2], "bge taken" );
  strcpy( inst_name[3], "sethi" );
  strcpy( inst_name[4], "add" );
  strcpy( inst_name[5], "or" );
  strcpy( inst_name[6], "sub" );
  strcpy( inst_name[7], "subcc" );
  strcpy( inst_name[8], "sll" );
  strcpy( inst_name[9], "load" );
  strcpy( inst_name[10], "store" );
  strcpy( inst_name[11], "halt" );
}

/* initialization routine to read in memory contents */

void load_mem( char *filename ){
  int i = 0;
  FILE *fp;
  
  if( ( fp = fopen( filename, "r" ) ) == NULL ){
    printf( "error in opening memory file %s\n", filename );
    exit( -1 );
  }
  printf( "contents of memory\n" );
  printf( "addr  value\n" );
  while( fscanf( fp, "%x", &mem[i] ) != EOF ){
    if( i >= MEM_SIZE ){
      printf( "program file overflows available memory\n" );
      exit( -1 );
    }
    printf( "%4x: %08x\n", i, mem[i] );
    i++;
  }
  word_count = i;
  for( ; i < MEM_SIZE; i++ ){
    mem[i] = 0;
  }
  printf( "\n" );
  fclose( fp );
}


/* instruction fetch routine */

void fetch(){
  mar = pc;
  mdr = mem[ mar ];
  inst_fetches++;
  ir = mdr;
  pc++;
}

/* set of instruction execution routines */

void ba(){
  inst_number = BA_TAKEN;
  inst_count[inst_number]++;
  pc = pc + signed_displacement;
}

void bge(){
  /* branch condition is (N xor V) == 0 */
  if( ((( cc >> 3 ) & 1 ) ^ (( cc >> 1 ) & 1 )) == 0 ){
    inst_number = BGE_TAKEN;
    pc = pc + signed_displacement;
  }else{
    inst_number = BGE_UNTAKEN;
  }
  inst_count[inst_number]++;
}

// PROGRAM 1 - fill in those missing

void subcc(){
  long long int a,b,c;
  inst_number = SUBCC;
  inst_count[inst_number]++;

  unsigned int var = (ir>>13) & 0x1;

  if(var==1){
    rsrc = (ir>>14) & 0x1f;
    src2_value = ir & 0x1fff;
    src1_value = reg[rsrc];
    imm_flag = 1;
  }
  if(var==0){
    rsrc = (ir>>14) & 0x1f;
    rsrc2 = ir & 0x1f;
    src2_value = reg[rsrc2];
    src1_value = reg[rsrc];
    imm_flag = 0;
  }
  reg[rdest] = src1_value - src2_value;

  /* set cc by using 64-bit arithmetic */
  cc = 0;
  a = (long long int) src1_value;
  b = (long long int) src2_value;
  c = a - b;
  /* N */ if( c < 0 ) cc = cc | 8;
  /* Z */ if( c == 0 ) cc = cc | 4;
  /* V */ if( ( a > 0 ) && ( b < 0 ) && ( c < 0 ) ) cc = cc | 2;
          if( ( a < 0 ) && ( b > 0 ) && ( c > 0 ) ) cc = cc | 2;
  /* C */ if( ( c >> 32 ) & 1 ) cc = cc | 1;
}

// PROGRAM 1 - fill in those missing
void store(){
  inst_number = STORE;
  inst_count[inst_number]++;
  unsigned int var = (ir>>13) & 0x1;
  if(var==1){
    rsrc = (ir>>14) & 0x1f;
    src2_value = ir & 0x1fff;
    src1_value = reg[rsrc];
    imm_flag = 1;
  }
  if(var==0){
    rsrc = (ir>>14) & 0x1f;
    rsrc2 = ir & 0x1f;
    src2_value = reg[rsrc2];
    src1_value = reg[rsrc];
    imm_flag = 0;
  }
  mar = src1_value + src2_value;
  mdr = reg[rdest];
  mem[mar] = mdr;
  memory_writes++;
}
void load(){
  inst_number = LOAD;
  inst_count[inst_number]++;
  unsigned int var = (ir>>13) & 0x1;
  if(var==1){
    rsrc = (ir>>14) & 0x1f;
    src2_value = ir & 0x1fff;
    src1_value = reg[rsrc];
    reg[rdest] = src1_value + src2_value;
    imm_flag = 1;
  }
  if(var==0){
    rsrc = (ir>>14) & 0x1f;
    rsrc2 = ir & 0x1f;
    src2_value = reg[rsrc2];
    src1_value = reg[rsrc];
    reg[rdest] = src1_value + src2_value;
    imm_flag = 0;
  }
  mar = rdest;
  mdr = reg[rdest];
  memory_reads++;
}
void sethi(){
  inst_number = SETHI;
  inst_count[inst_number]++;
  sethi_value = ir & 0x3ffff;
  reg[rdest] = sethi_value << 10;
}
 
void hlt(){
  inst_number = HALT;
  inst_count[inst_number]++;
  halt = 1;
}

void sub(){
  inst_number = SUB;
  inst_count[inst_number]++;
  unsigned int var = (ir>>13) & 0x1;
  if(var==1){
    rsrc = (ir>>14) & 0x1f;
    src2_value = ir & 0x1fff;
    src1_value = reg[rsrc];
    reg[rdest] = src1_value - src2_value;
    imm_flag = 1;
  }
  if(var==0){
    rsrc = (ir>>14) & 0x1f;
    rsrc2 = ir & 0x1f;
    src2_value = reg[rsrc2];
    src1_value = reg[rsrc];
    reg[rdest] = src1_value - src2_value;
    imm_flag = 0;
  }
}

void add(){
  inst_number = ADD;
  inst_count[inst_number]++;
  unsigned int var = (ir>>13) & 0x1;
  if(var==1){
    rsrc = (ir >> 14) & 0x1f;
    src2_value = ir & 0x1fff;
    src1_value = reg[rsrc];
    reg[rdest] = src1_value + src2_value;
    imm_flag = 1;
  }
  if(var==0){
    rsrc = (ir >> 14) & 0x1f;
    rsrc2 = ir & 0x1f;
    src2_value = reg[rsrc2];
    src1_value = reg[rsrc];
    reg[rdest] = src1_value + src2_value;
    imm_flag = 0;
  }
}

void Or(){
  inst_number = OR;
  inst_count[inst_number]++;
  unsigned int var = (ir>>13) & 0x1;
  if(var==1){
    rsrc = (ir >> 14) & 0x1f;
    src2_value = ir & 0x1fff;
    src1_value = reg[rsrc];
    reg[rdest] = src1_value | src2_value;
    imm_flag = 1;
  }
  if(var==0){
    rsrc = (ir >> 14) & 0x1f;
    rsrc2 = ir & 0x1f;
    src2_value = reg[rsrc2];
    src1_value = reg[rsrc];
    reg[rdest] = src1_value | src2_value;
    imm_flag = 0;
  }
}
void sll(){
  inst_number = SLL;
  inst_count[inst_number]++;
  unsigned int var = (ir>>13) & 0x1;
  if(var==1){
    src2_value = ir & 0x1f;
    rsrc = (ir >> 14) & 0x1f;
    int data = reg[rsrc];
    data = data << src2_value;
    reg[rdest] = data;
    imm_flag = 1;
  }
}

void decode(){

  rdest = (ir >> 25) & 0x1f;
  unsigned int lead1 = (ir >> 30) & 0x3;
  int  sethi2 = (ir >> 22) & 0x7;

  /* decoding tree */

  if( (ir >> 22) == 0x42 ){
    inst = ba;
    int num = (ir & 0x3ffff) << 20;
    num = num >> 20;
    signed_displacement = num;
  }else if( (ir >> 22) == 0x5a ){
    int num = (ir & 0x3ffff) << 20;
    num = num >> 20;
    signed_displacement = num;
    inst = bge;

  }else if((lead1 == 0) && sethi2 == 4){
    if( ((ir >> 22) & 0x4) == 4){
      inst = sethi;
    }
  }else if(lead1==2){
    unsigned int index1 = (ir >> 19) & 0x3f;
    if(index1 == 37){
      inst=sll;
    }else if (index1 == 0){
      inst = add;
    }else if (index1 == 2){
      inst = Or;
    }else if (index1 == 20){
      inst = subcc;
    }
  }else if (lead1 == 3){
    unsigned int index1 = (ir >> 19) & 0x3f;
    if(index1==0){
      inst = load;
    }
    if(index1==4){
      inst = store;
    }



// PROGRAM 1 - fill in the rest of the decoding tree

  }else{
    inst = hlt;
  }
}

unsigned prevInstruct = 0,
         prevWrite = 0,
         currWrite = 0,
         prevReadDest = 0,
         prevReadDest2 = 0,
         currReadDest = 0,
         currReadDest2 = 0,
         prevFlag = 0,
         currFlag = 0;   


/* main program */

int main( int argc, char **argv ){
  unsigned i,j;

  printf( "\nbehavioral simulation of SPARC subset from %s\n", argv[1] );
  printf( "  simulation values are in hexadecimal\n" );
  printf( "  execution statistics are in decimal\n\n" );

  init_inst_names();
  load_mem( argv[1] );

  printf( "register values after each instruction has been executed\n" );
  printf( "   instruction pc       mar      mdr      ir       cc " );
  printf( "rd rs1 rs2/imm\n" );

  while( !halt ){

    unsigned prevInstruct = inst_number;
    rdest = 0;
    rsrc = 0;
    rsrc2 = 0;
    src2_value = 0;

    fetch();

    decode();

    //define
    prevWrite = currWrite;
    prevReadDest = currReadDest;
    prevReadDest2 = currReadDest2;
    prevFlag = currFlag;

    ( *inst )(); /* execute */

    reg[0] = 0;  

   
    if(strcmp(inst_name[inst_number], "store") != 0){

          currWrite = rdest;
          currFlag = 0;
          if(rsrc2 == 0){
              currReadDest = rsrc;
              currReadDest2 = -1;
              }
          else{
              currReadDest = rsrc;
              currReadDest2 = rsrc2;
          }
      }
      else{
        
          currReadDest = rdest;
          currFlag = 0;
          currReadDest2 = 0;

          if(rsrc2 != 0){
              currWrite = reg[rsrc] + reg[rsrc2];
          }
          else{
              currWrite = reg[rsrc] + src2_value;
          }
    }


    if(inst_flags[prevInstruct][3] == 1 || prevInstruct == 12){
        printf("CH ");
      }else if((inst_flags[prevInstruct][3] == 2) && inst_flags[inst_number][3] != 3){
        printf("SH ");
      }else if(( prevWrite == currWrite) &&
              inst_flags[prevInstruct][0] !=0 &&
              inst_flags[inst_number][0] != 0 &&
              strcmp(inst_name[inst_number], "store") != 0
              ){
                printf("WH ");
      }else if( (( prevWrite == currReadDest) ||
               ( prevWrite == currReadDest2)) &&
               inst_flags[prevInstruct][0] != 0 &&
               inst_flags[inst_number][0] != 0
               ){
                 printf("DH ");
      }else{
        printf("2I ");
      }




    printf( "%11s %08x %08x %08x %08x %x",
      inst_name[inst_number], pc, mar, mdr, ir, cc );
    if( inst_flags[inst_number][0] ) printf( "  %2x", rdest );
    if( inst_flags[inst_number][1] ) printf( " %2x", rsrc );
    if( inst_flags[inst_number][2] ){
      if( imm_flag ) printf( "  %08x", src2_value );
      else printf( "  %2x", rsrc2 );
    }
    printf( "\n" );
  }

  printf( "\ncontents of registers\n" );
  printf( "  reg value     reg value     reg value     reg value\n" );
  for( i = 0; i < 8; i++ ){
    for( j = 0; j < 4; j++ ){
      printf( "  %2x: %08x", 8*j+i, reg[8*j+i] );
    }
    printf( "\n" );
  }

  printf( "\ncontents of memory\n" );
  printf( "addr  value\n" );
  for( i = 0; i < word_count; i++ ){
    printf( "%4x: %08x\n", i, mem[i] );
  }

  printf( "\ndynamic execution statistics\n" );
  printf( "  instruction fetches = %d\n", inst_fetches );
  printf( "  memory data reads   = %d\n", memory_reads );
  printf( "  memory data writes  = %d\n", memory_writes );
  for( i = 0; i < 12; i++ ){
    printf( "  %11s = %d\n", inst_name[i], inst_count[i] );
  }

  return 0;
}