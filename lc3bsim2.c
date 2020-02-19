/*
    Name 1: Sai Susheel Koukuntla
    UTEID 1: sk46579
*/

/***************************************************************/
/*                                                             */
/*   LC-3b Instruction Level Simulator                         */
/*                                                             */
/*   EE 460N                                                   */
/*   The University of Texas at Austin                         */
/*                                                             */
/***************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//__________________________________ D E F I N I T I O N S ________________________________________
#define FALSE 0
#define TRUE  1

#define Low16bits(x)  ((x) & 0xFFFF)

#define get_DR(x)     (((x) & 0x0E00) >> 9)
#define get_SR1(x)    (((x) & 0x01C0) >> 6)
#define get_BaseR(x)  (((x) & 0x01C0) >> 6)
#define get_SR2(x)    ((x) & 0x0007)

#define sext_imm5(x)  (((x) | ((x) >> 4) * 0xFFFFFFE0))
#define sext_off6(x)  (((x) | ((x) >> 5) * 0xFFFFFFC0))
#define sext_off9(x)  (((x) | ((x) >> 8) * 0xFFFFFE00)) 
#define sext_off11(x) (((x) | ((x) >> 9) * 0xFFFFF800)) 
#define sext_byte(x)  (((x) | ((x) >> 7) * 0xFF00)) //Automatically omits upper 16 bits

#define WORDS_IN_MEM    0x08000 
#define LC_3b_REGS 8
typedef struct System_Latches_Struct{
  int PC,		/* program counter */
    N,		/* n condition bit */
    Z,		/* z condition bit */
    P;		/* p condition bit */
  int REGS[LC_3b_REGS]; /* register file. */
} System_Latches;
System_Latches CURRENT_LATCHES, NEXT_LATCHES;

#define setCC(x){ \
      NEXT_LATCHES.N = 0;\
      NEXT_LATCHES.Z = 0;\
      NEXT_LATCHES.P = 0;\
      if(x > 0) NEXT_LATCHES.P = 1;\
      else if(x == 0) NEXT_LATCHES.Z = 1;\
      else if(x < 0) NEXT_LATCHES.N = 1;\
}

enum opcode {
add = 0x1, and = 0x5, br = 0x0, jmp = 0xC, jsr = 0x4, ldb = 0x2, ldw = 0x6, 
lea = 0xE, rti = 0x8, shf = 0xD, stb = 0x3, stw = 0x7, trap = 0xF, xor = 0x9
};

//_____________________________________ G L O B A L S ______________________________________________
int RUN_BIT;	/* run bit */

/* MEMORY[A][0] stores the least significant byte of word at word address A
   MEMORY[A][1] stores the most significant byte of word at word address A */
int MEMORY[WORDS_IN_MEM][2];
int INSTRUCTION_COUNT;

//__________________________________ P R O T O T Y P E S __________________________________________
void process_instruction();

/***************************************************************/
/*                                                             */
/* Procedure : help                                            */
/*                                                             */
/* Purpose   : Print out a list of commands                    */
/*                                                             */
/***************************************************************/
void help() {                                                    
  printf("----------------LC-3b ISIM Help-----------------------\n");
  printf("go               -  run program to completion         \n");
  printf("run n            -  execute program for n instructions\n");
  printf("mdump low high   -  dump memory from low to high      \n");
  printf("rdump            -  dump the register & bus values    \n");
  printf("?                -  display this help menu            \n");
  printf("quit             -  exit the program                  \n\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : cycle                                           */
/*                                                             */
/* Purpose   : Execute a cycle                                 */
/*                                                             */
/***************************************************************/
void cycle() {                                                

  process_instruction();
  CURRENT_LATCHES = NEXT_LATCHES;
  INSTRUCTION_COUNT++;
}

/***************************************************************/
/*                                                             */
/* Procedure : run n                                           */
/*                                                             */
/* Purpose   : Simulate the LC-3b for n cycles                 */
/*                                                             */
/***************************************************************/
void run(int num_cycles) {                                      
  int i;

  if (RUN_BIT == FALSE) {
    printf("Can't simulate, Simulator is halted\n\n");
    return;
  }

  printf("Simulating for %d cycles...\n\n", num_cycles);
  for (i = 0; i < num_cycles; i++) {
    if (CURRENT_LATCHES.PC == 0x0000) {
	    RUN_BIT = FALSE;
	    printf("Simulator halted\n\n");
	    break;
    }
    cycle();
  }
}

/***************************************************************/
/*                                                             */
/* Procedure : go                                              */
/*                                                             */
/* Purpose   : Simulate the LC-3b until HALTed                 */
/*                                                             */
/***************************************************************/
void go() {                                                     
  if (RUN_BIT == FALSE) {
    printf("Can't simulate, Simulator is halted\n\n");
    return;
  }

  printf("Simulating...\n\n");
  while (CURRENT_LATCHES.PC != 0x0000)
    cycle();
  RUN_BIT = FALSE;
  printf("Simulator halted\n\n");
}

/***************************************************************/ 
/*                                                             */
/* Procedure : mdump                                           */
/*                                                             */
/* Purpose   : Dump a word-aligned region of memory to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void mdump(FILE * dumpsim_file, int start, int stop) {          
  int address; /* this is a byte address */

  printf("\nMemory content [0x%.4x..0x%.4x] :\n", start, stop);
  printf("-------------------------------------\n");
  for (address = (start >> 1); address <= (stop >> 1); address++)
    printf("  0x%.4x (%d) : 0x%.2x%.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
  printf("\n");

  /* dump the memory contents into the dumpsim file */
  fprintf(dumpsim_file, "\nMemory content [0x%.4x..0x%.4x] :\n", start, stop);
  fprintf(dumpsim_file, "-------------------------------------\n");
  for (address = (start >> 1); address <= (stop >> 1); address++)
    fprintf(dumpsim_file, " 0x%.4x (%d) : 0x%.2x%.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
  fprintf(dumpsim_file, "\n");
  fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : rdump                                           */
/*                                                             */
/* Purpose   : Dump current register and bus values to the     */   
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void rdump(FILE * dumpsim_file) {                               
  int k; 

  printf("\nCurrent register/bus values :\n");
  printf("-------------------------------------\n");
  printf("Instruction Count : %d\n", INSTRUCTION_COUNT);
  printf("PC                : 0x%.4x\n", CURRENT_LATCHES.PC);
  printf("CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
  printf("Registers:\n");
  for (k = 0; k < LC_3b_REGS; k++)
    printf("%d: 0x%.4x\n", k, CURRENT_LATCHES.REGS[k]);
  printf("\n");

  /* dump the state information into the dumpsim file */
  fprintf(dumpsim_file, "\nCurrent register/bus values :\n");
  fprintf(dumpsim_file, "-------------------------------------\n");
  fprintf(dumpsim_file, "Instruction Count : %d\n", INSTRUCTION_COUNT);
  fprintf(dumpsim_file, "PC                : 0x%.4x\n", CURRENT_LATCHES.PC);
  fprintf(dumpsim_file, "CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
  fprintf(dumpsim_file, "Registers:\n");
  for (k = 0; k < LC_3b_REGS; k++)
    fprintf(dumpsim_file, "%d: 0x%.4x\n", k, CURRENT_LATCHES.REGS[k]);
  fprintf(dumpsim_file, "\n");
  fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : get_command                                     */
/*                                                             */
/* Purpose   : Read a command from standard input.             */  
/*                                                             */
/***************************************************************/
void get_command(FILE * dumpsim_file) {                         
  char buffer[20];
  int start, stop, cycles;

  printf("LC-3b-SIM> ");

  scanf("%s", buffer);
  printf("\n");

  switch(buffer[0]) {
  case 'G':
  case 'g':
    go();
    break;

  case 'M':
  case 'm':
    scanf("%i %i", &start, &stop);
    mdump(dumpsim_file, start, stop);
    break;

  case '?':
    help();
    break;
  case 'Q':
  case 'q':
    printf("Bye.\n");
    exit(0);

  case 'R':
  case 'r':
    if (buffer[1] == 'd' || buffer[1] == 'D')
	    rdump(dumpsim_file);
    else {
	    scanf("%d", &cycles);
	    run(cycles);
    }
    break;

  default:
    printf("Invalid Command\n");
    break;
  }
}

/***************************************************************/
/*                                                             */
/* Procedure : init_memory                                     */
/*                                                             */
/* Purpose   : Zero out the memory array                       */
/*                                                             */
/***************************************************************/
void init_memory() {                                           
  int i;

  for (i=0; i < WORDS_IN_MEM; i++) {
    MEMORY[i][0] = 0;
    MEMORY[i][1] = 0;
  }
}

/**************************************************************/
/*                                                            */
/* Procedure : load_program                                   */
/*                                                            */
/* Purpose   : Load program and service routines into mem.    */
/*                                                            */
/**************************************************************/
void load_program(char *program_filename) {                   
  FILE * prog;
  int ii, word, program_base;

  /* Open program file. */
  prog = fopen(program_filename, "r");
  if (prog == NULL) {
    printf("Error: Can't open program file %s\n", program_filename);
    exit(-1);
  }

  /* Read in the program. */
  if (fscanf(prog, "%x\n", &word) != EOF)
    program_base = word >> 1;
  else {
    printf("Error: Program file is empty\n");
    exit(-1);
  }

  ii = 0;
  while (fscanf(prog, "%x\n", &word) != EOF) {
    /* Make sure it fits. */
    if (program_base + ii >= WORDS_IN_MEM) {
	    printf("Error: Program file %s is too long to fit in memory. %x\n",
             program_filename, ii);
	    exit(-1);
    }

    /* Write the word to memory array. */
    MEMORY[program_base + ii][0] = word & 0x00FF;
    MEMORY[program_base + ii][1] = (word >> 8) & 0x00FF;
    ii++;
  }

  if (CURRENT_LATCHES.PC == 0) CURRENT_LATCHES.PC = (program_base << 1);

  printf("Read %d words from program into memory.\n\n", ii);
}

/************************************************************/
/*                                                          */
/* Procedure : initialize                                   */
/*                                                          */
/* Purpose   : Load machine language program                */ 
/*             and set up initial state of the machine.     */
/*                                                          */
/************************************************************/
void initialize(char *program_filename, int num_prog_files) { 
  int i;

  init_memory();
  for ( i = 0; i < num_prog_files; i++ ) {
    load_program(program_filename);
    while(*program_filename++ != '\0');
  }
  CURRENT_LATCHES.Z = 1;  
  NEXT_LATCHES = CURRENT_LATCHES;
    
  RUN_BIT = TRUE;
}

/***************************************************************/
/*                                                             */
/* Procedure : main                                            */
/*                                                             */
/***************************************************************/
int main(int argc, char *argv[]) {                              
  FILE * dumpsim_file;

  /* Error Checking */
  if (argc < 2) {
    printf("Error: usage: %s <program_file_1> <program_file_2> ...\n",
           argv[0]);
    exit(1);
  }

  printf("LC-3b Simulator\n\n");

  initialize(argv[1], argc - 1);

  if ( (dumpsim_file = fopen( "dumpsim", "w" )) == NULL ) {
    printf("Error: Can't open dumpsim file\n");
    exit(-1);
  }

  while (1)
    get_command(dumpsim_file);
    
}

/***************************************************************/
/* Do not modify the above code.
   You are allowed to use the following global variables in your
   code. These are defined above.

   MEMORY

   CURRENT_LATCHES
   NEXT_LATCHES

   You may define your own local/global variables and functions.
   You may use the functions to get at the control bits defined
   above.

   Begin your code here 	  			       */

/***************************************************************/

/*  function: process_instruction
  *  
  *    Process one instruction at a time  
  *       -Fetch one instruction
  *       -Decode 
  *       -Execute
  *       -Update NEXT_LATCHES
  */     
void process_instruction(){
  //Initialize next latches
  NEXT_LATCHES = CURRENT_LATCHES;

  //Fetch
  int pc = CURRENT_LATCHES.PC;
  int instr = (MEMORY[pc >> 1][1] << 8) + MEMORY[pc >> 1][0];
  NEXT_LATCHES.PC += 2;

  //Decode
  int opcode = instr >> 12;

  //Execute/Update
  switch(opcode){
//______________________________________ A D D | A N D | X O R ___________________________________________
    case add:
    case and:
    case xor:;
      int result;
      int op1 = CURRENT_LATCHES.REGS[get_SR1(instr)]; //Get value of SR1
      int op2;

      if(instr & 0x0020){ //bit 5 = 1, so op2 = imm5
        op2 = instr & 0x001F;
        op2 = sext_imm5(op2); //sign extend
      }
      else{ //bit 5 = 0, op2 = sr2
        op2 = CURRENT_LATCHES.REGS[get_SR2(instr)]; //Get value of SR2
      }
      //Compute and store result
      if(opcode == add){
        result = op1 + op2;
      }
      else if(opcode == and){
        result = op1 & op2;
      }
      else{
        result = op1 ^ op2;
      }
      NEXT_LATCHES.REGS[get_DR(instr)] = Low16bits(result); //Store result

      setCC(result);
      break;

//__________________________________________________ B R __________________________________________________ 
    case br:;
      bool n = CURRENT_LATCHES.N && (instr & 0x0800);
      bool z = CURRENT_LATCHES.Z && (instr & 0x0400);
      bool p = CURRENT_LATCHES.P && (instr & 0x0200);

      if(n|z|p){
        int offset = sext_off9(instr & 0x01FF) << 1;
        NEXT_LATCHES.PC += Low16bits(offset);
      }
      break;

//_________________________________________________ J M P _________________________________________________
    case jmp:;
      NEXT_LATCHES.PC = CURRENT_LATCHES.REGS[get_BaseR(instr)];
      break;

//_________________________________________________ J S R ________________________________________________
    case jsr:;
      NEXT_LATCHES.REGS[7] = CURRENT_LATCHES.PC;
      if(instr & 0x0800){
        int offset = sext_off11(instr & 0x07FF) << 1;
        NEXT_LATCHES.PC += Low16bits(offset);
      }
      else{
        NEXT_LATCHES.PC = CURRENT_LATCHES.REGS[get_BaseR(instr)];
      }
      break;

//_________________________________________________ L D B ________________________________________________
    case ldb:;
      result = CURRENT_LATCHES.REGS[get_BaseR(instr)] + sext_off6(instr);
      result = Low16bits(result); //just to be safe
      result = MEMORY[result >> 1][result % 2];
      result = sext_byte(result);

      NEXT_LATCHES.REGS[get_DR(instr)] = result;
      setCC(result);
      break; 

//_________________________________________________ L D W ________________________________________________
    case ldw:;
      result = CURRENT_LATCHES.REGS[get_BaseR(instr)] + ((sext_off6(instr)) << 1);
      result = Low16bits(result); //just to be safe
      result = (MEMORY[result >> 1][1] << 8) + MEMORY[result >> 1][0];

      NEXT_LATCHES.REGS[get_DR(instr)] = result;
      setCC(result);
      break;

//_________________________________________________ L E A _______________________________________________
    case lea:;
      int offset = sext_off9(instr & 0x01FF) << 1;
      NEXT_LATCHES.PC += Low16bits(offset);
      break;

//________________________________________________ S H F ________________________________________________
    case shf:;
      result = CURRENT_LATCHES.REGS[get_SR1(instr)];
      if(instr & 0x0010){ //right shift
        if(instr & 0x0020){ //arithmetic
          int sign = result & 0x8000 >> 15;
          for(int i = 0; i < (instr & 0x000F); i++){
            result = (sign << 15) + (result >> 1);
          }
        }
        else{ //logical
          result = result >> (instr & 0x000F); 
        }
      }
      else{ //left shift
        result = result << (instr & 0x000F); 
      }

      NEXT_LATCHES.REGS[get_DR(instr)] = Low16bits(result);
      setCC(result);
      break;

//_______________________________________________ S T B _________________________________________________
    case stb:;
      int data = CURRENT_LATCHES.REGS[get_DR(instr)] & 0x000000FF;
      int address = CURRENT_LATCHES.REGS[get_BaseR(instr)] + sext_off6((instr & 0x003F));
      MEMORY[address >> 1][address % 2] = data;
      break;

 //______________________________________________ S T W _________________________________________________
    case stw:;
      data = CURRENT_LATCHES.REGS[get_DR(instr)];
      address = CURRENT_LATCHES.REGS[get_BaseR(instr)] + (sext_off6((instr & 0x003F)) << 1);
      MEMORY[address >> 1][1] = data & 0x0000FF00;
      MEMORY[address >> 1][0] = data & 0x000000FF;
      break;       

//______________________________________________ T R A P ________________________________________________
    case trap:;
      NEXT_LATCHES.REGS[7] = CURRENT_LATCHES.PC;
      int vect = instr & 0x000000FF;
      NEXT_LATCHES.PC = (MEMORY[vect << 1][1] << 8) + MEMORY[vect << 1][0];
      break;

    default: //RTI(1000), 1010, 1011
      break;
  }


}
