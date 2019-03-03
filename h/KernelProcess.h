#pragma once
#include "vm_declarations.h"

class Descriptor;
class Process;
class PMT1;

class KernelProcess {
public:
	KernelProcess(ProcessId pid, Process* myProcess);

	~KernelProcess();

	ProcessId getProcessId() const;

	Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);

	Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content);

	Status deleteSegment(VirtualAddress startAddress);

	Status deleteSegmentLock(VirtualAddress startAddress);

	Status pageFault(VirtualAddress address);

	PhysicalAddress getPhysicalAddress(VirtualAddress address);

	Process* clone(ProcessId pid);

	Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char* name, AccessType flags);

	Status disconnectSharedSegmentLock(const char* name);

	Status disconnectSharedSegment(const char* name);

	Status deleteSharedSegment(const char* name);

private:

	Status checkSegment(VirtualAddress startAddress, PageNum segmentSize);

	bool checkAllocated(VirtualAddress startAddress);

	bool updatePMT(VirtualAddress page, PhysicalAddress frame, PageNum ordinal, AccessType flags, bool setD = false, bool setSh = false, Descriptor* sharedDesc = nullptr);

	Process* myProcess;

	PMT1* pmtHead;

	ProcessId pid;

	friend class KernelSystem;
};