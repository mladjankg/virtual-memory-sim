#include "System.h"
#include "DummyMutex.h"
#include "KernelSystem.h"
#include <mutex>

System::System(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition * partition) {
	this->pSystem = new KernelSystem(processVMSpace, processVMSpaceSize, pmtSpace, pmtSpaceSize, partition, this);
}

System::~System() {
	delete this->pSystem;
}

Process* System::createProcess() {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	Process* proc = this->pSystem->createProcess();
	return proc;
}

Time System::periodicJob() {
	return this->pSystem->periodicJob();
}

Status System::access(ProcessId pid, VirtualAddress address, AccessType type) {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	return this->pSystem->access(pid, address, type);
}

Process * System::cloneProcess(ProcessId pid) {
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);

	return this->pSystem->cloneProcess(pid);
}
