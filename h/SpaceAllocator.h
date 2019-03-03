#pragma once

#include "PMT.h"
#include <list>
#include <mutex>
#include "vm_declarations.h"
#include "MemoryException.h"

class FreeSpaceDescriptor;
class KernelSystem;

class SpaceAllocator {
public:

	SpaceAllocator(KernelSystem* system, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, PhysicalAddress processVMspace, PageNum processVMSpaceSize, ClusterNo numberOfClusters, Partition* partition);

	~SpaceAllocator();

	PhysicalAddress allocatePMT(PMTType type, PageNum size = 0);

	void deallocatePMT(PhysicalAddress adr, PMTType type, PageNum size = 0);

	void deallocatePage(PhysicalAddress page);

	PhysicalAddress allocatePage() throw(MemoryException);

	static size_t pmt1Size, pmt2Size, descSize;
	
private:
	friend class KernelSystem;

	KernelSystem* mySystem;

	FreeSpaceDescriptor *kernelHead, *pageHead, *pageTail;

	std::list<FreeSpaceDescriptor> processVMFreeSpace, pmtFreeSpace;

	PageNum kernelSpaceSize, pageSpaceSize;

	ClusterNo numberOfClusters;
	
	Partition* partition;

	std::mutex* memoryMutex;
};