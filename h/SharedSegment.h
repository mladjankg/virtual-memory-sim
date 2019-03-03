#pragma once
#include <list>
#include <unordered_map>
#include "vm_declarations.h"
#include "PMT.h"

class SharedSegment {
public:

	struct ProcessInfo {
		ProcessInfo(ProcessId pid, VirtualAddress segmentAddress) : pid(pid), segmentAddress(segmentAddress) {}
		ProcessId pid;
		VirtualAddress segmentAddress;
	};

	SharedSegment(const char* name, VirtualAddress startAddress, PageNum size, AccessType access) : name(name), startAddress(startAddress), size(size), access(access), processesUsing(0) {
		pmt.entry = nullptr;
	};

	//std::list<ProcessInfo> processes;
	std::unordered_map<ProcessId, VirtualAddress> processes;
	//std::list<PhysicalAddress> frames;

	SharedSegmentPMT pmt;

	AccessType getAccess() const { return access; }

	PageNum getSegmentSize() const { return size; }

	bool operator==(const SharedSegment& segment) const {
		return (name == segment.name) && (startAddress == startAddress) && (size == size);
	}


	ProcessId processesUsing;

private:
	std::string name;

	AccessType access;

	VirtualAddress startAddress;

	PageNum size;
};