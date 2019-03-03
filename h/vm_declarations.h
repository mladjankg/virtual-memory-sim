#pragma once
// File: vm_declarations.h


typedef unsigned long PageNum;
typedef unsigned long VirtualAddress;
typedef void* PhysicalAddress;
typedef unsigned long Time;

enum Status { OK, PAGE_FAULT, TRAP };

enum AccessType { READ, WRITE, READ_WRITE, EXECUTE };

enum PMTType { LEVEL1_PMT, LEVEL2_PMT, SHARED_SEG_PMT };

typedef unsigned ProcessId;

#define PAGE_SIZE 1024