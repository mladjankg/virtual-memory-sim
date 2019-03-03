#pragma once
#include "vm_declarations.h"
#include "ConstantsAndMasks.h"
#include "part.h"


class Descriptor {
public:
	union {
		ClusterNo disk;
		Descriptor* sharedDesc;
	};
	//ClusterNo disk;
	unsigned short ordinal;
	unsigned int frameAndFlags;
};

class PMT2 {
public:
	char entriesUsed;
	Descriptor entry[PMT2_SIZE];
};

class PMT1 {
public:
	char entriesUsed;
	PMT2* level2entry[PMT1_SIZE];
};

class SharedSegmentPMT {
public:
	Descriptor* entry;
};