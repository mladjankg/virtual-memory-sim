#pragma once

//#define PRINT

#define PMT_ENTRY_MASK 0x7F

#define FRAME_MASK 0x0003FFFFF
#define FRAME_MASK_DELETE 0xFFFC00000

#define PMT1_SIZE 128
#define PMT1_OFFSET 17

#define PMT2_SIZE 128
#define PMT2_OFFSET 10

#define SET_V 0x01000000
#define RESET_V 0xFEFFFFFF
#define V_MASK 0x01000000

#define SET_D 0x02000000
#define RESET_D 0xFDFFFFFF
#define D_MASK 0x02000000

#define S_MASK 0x04000000 //Swap bit, oznacava da li je stranica ikada sacuvana na disk ili ne
#define SET_S 0x04000000
#define RESET_S 0xFBFFFFFF

#define L_MASK 0x08000000 //L bit oznacava da li je stranica dodeljena procesu pozivom metoda CreateSegment ili LoadSegment
#define SET_L 0x08000000
#define RESET_L 0xF7FFFFFF

#define SH_MASK 0x10000000
#define SET_SH 0x10000000
#define RESET_SH 0xEFFFFFFF

#define ACCESS_BITS_MASK 0x0C00000
#define ACCESS_BITS_SHIFT 22

#define PCB_HASH_SIZE 128

#define PAGE_SIZE 1024

#define REF_BITS_HOLDER_SIZE 8

#define ADR_WORD 10 //Duzina word polja u adresi
#define WORD_MASK 0x3FF

#define VIRTUAL_MEMORY_LAST_ADDRESS 0xFFFFFF