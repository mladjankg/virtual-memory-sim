#pragma once

#include "vm_declarations.h"
#include "ConstantsAndMasks.h"
#include "SharedSegment.h"
#include "part.h"
#include <list>
#include <mutex>
#include <unordered_map>
#include <map>

class Partition;
class Descriptor;
class Process;
class System;
class PMT1;
class KernelProcess;
class MemoryException;
struct ClustersFree;

class KernelSystem {
private:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
		PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
		Partition* partition, System* mySystem);

	~KernelSystem();

	Process* createProcess();

	void deleteProcess(ProcessId pid);

	Time periodicJob();

	Status access(ProcessId pid, VirtualAddress address, AccessType type);

	PhysicalAddress swapPage() throw(MemoryException);
	
	Descriptor* checkTables(PMT1* pmt1, unsigned int frame);

	//Metode za upravljanje memorijom

	ClusterNo getFreeCluster() throw(MemoryException);

	void setClusterFree(ClusterNo cluster);

	PhysicalAddress allocatePMT(PMTType type);

	void deallocatePMT(PhysicalAddress adr, PMTType type);

	void deallocatePage(PhysicalAddress page);

	PhysicalAddress allocatePage() throw(MemoryException);

	Process* cloneProcess(ProcessId pid);

	void copyContent(const char* src, char* dst);

	void initSegment(VirtualAddress startAddress, KernelProcess* newKP, KernelProcess* oldKP, PageNum segmentSize, ProcessId pid, AccessType flags, bool shared);

	System* mySystem;

	unsigned char* referenceBits;

	std::unordered_map<ProcessId, Process*> processMap;
	
	std::map<std::string, SharedSegment*> sharedSegments;

	std::list<ClustersFree> freeClusters;

	PhysicalAddress processVMSpace;
	PageNum processVMSpaceSize;
	
	PhysicalAddress pmtSpace;
	PageNum pmtSpaceSize;

	Partition* partition;
	ClusterNo numberOfClusters;

	PageNum clockHand; //Pokazivac na sledecu stranicu za zamenu (Second chance algoritam)

	static ProcessId nextPid; //Promenljiva koja sluzi da se pri kreiranju procesa procesu dodeli jedinstveni ID

	friend class System;

	friend class Process;

	friend class KernelProcess;

	friend class SpaceAllocator;

	static KernelSystem* kernelSystem;

	SpaceAllocator* spaceAllocator;

	std::mutex *globalMutex;
};