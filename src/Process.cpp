#include "KernelProcess.h"
#include "KernelSystem.h"
#include "DummyMutex.h"
#include "Process.h"
#include <iostream>
#include <cassert>
#include <mutex>

Process::Process(ProcessId pid) {
	this->pProcess = new KernelProcess(pid, this);
}

Process::~Process() {
	//std::cout << "PROCESS " << this->getProcessId() << " FINISHED \n";

	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	if (this->pProcess != nullptr)
		delete this->pProcess;

}

ProcessId Process::getProcessId() const {
	return this->pProcess->getProcessId();
}

Status Process::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {

	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	assert(this->pProcess != nullptr);
	Status status = this->pProcess->createSegment(startAddress, segmentSize, flags);		
	return status;
}

Status Process::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content) {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	assert(this->pProcess != nullptr);
	Status status = this->pProcess->loadSegment(startAddress, segmentSize, flags, content);
	return status;
}

Status Process::deleteSegment(VirtualAddress startAddress) {

	assert(this->pProcess != nullptr);
	Status status = this->pProcess->deleteSegmentLock(startAddress);
	return status;
}

Status Process::pageFault(VirtualAddress address) {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	assert(this->pProcess != nullptr);
	Status status = this->pProcess->pageFault(address);	
	return status;
}

PhysicalAddress Process::getPhysicalAddress(VirtualAddress address) {
	assert(this->pProcess != nullptr);
	PhysicalAddress addr = this->pProcess->getPhysicalAddress(address);
	return addr;
}

Status Process::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * name, AccessType flags) {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	assert(this->pProcess != nullptr);
	return this->pProcess->createSharedSegment(startAddress, segmentSize, name, flags);
}

Status Process::disconnectSharedSegment(const char * name) {
	assert(this->pProcess != nullptr);
	return this->pProcess->disconnectSharedSegmentLock(name);
}

Status Process::deleteSharedSegment(const char * name) {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	assert(this->pProcess != nullptr);
	return this->pProcess->deleteSharedSegment(name);
}
