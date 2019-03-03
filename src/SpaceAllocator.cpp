#include "SpaceAllocator.h"
#include "KernelSystem.h"
#include "PMT.h"
#include "FreeSpaceDescriptor.h"
#include <iostream>
#include "DummyMutex.h"
#include <mutex>
size_t SpaceAllocator::pmt1Size = sizeof(PMT1);
size_t SpaceAllocator::pmt2Size = sizeof(PMT2);
size_t SpaceAllocator::descSize = sizeof(FreeSpaceDescriptor);

SpaceAllocator::SpaceAllocator(KernelSystem* system, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, PhysicalAddress  processVMspace, PageNum processVMSpaceSize, ClusterNo numberOfClusters, Partition * partition)
	:mySystem(system), kernelSpaceSize(pmtSpaceSize), pageSpaceSize(processVMSpaceSize), numberOfClusters(numberOfClusters), partition(partition)
{
	this->kernelHead = (FreeSpaceDescriptor*)pmtSpace;
	this->kernelHead->size = pmtSpaceSize * PAGE_SIZE;
	this->kernelHead->next = nullptr;

	this->pageHead = (FreeSpaceDescriptor*)processVMspace;
	this->pageHead->size = processVMSpaceSize;
	this->pageHead->next = nullptr;

	this->pageTail = this->pageHead;
	
	this->processVMFreeSpace.push_front(FreeSpaceDescriptor(processVMspace, processVMSpaceSize));
	this->pmtFreeSpace.push_front(FreeSpaceDescriptor(pmtSpace, pmtSpaceSize * PAGE_SIZE));

	this->memoryMutex = new std::mutex();

}

SpaceAllocator::~SpaceAllocator() {

}

PhysicalAddress SpaceAllocator::allocatePMT(PMTType type, PageNum segmentSize) {

	size_t size;

	if (type == PMTType::LEVEL1_PMT) size = SpaceAllocator::pmt1Size;
	else if (type == PMTType::LEVEL2_PMT) size = SpaceAllocator::pmt2Size;
	else size = segmentSize * sizeof(Descriptor);

	if (pmtFreeSpace.empty()) return nullptr;

	std::list<FreeSpaceDescriptor>::iterator it;

	for (it = pmtFreeSpace.begin(); (it != pmtFreeSpace.end()) && (it->size < size); ++it);

	if (it == pmtFreeSpace.end()) return nullptr;

	size_t newSize = it->size - size;
	PhysicalAddress pmtAdr = it->space;

	if (newSize > 0) {

		it->space = (char*)pmtAdr + size;
		it->size = newSize;

	}
	else {
		pmtFreeSpace.remove(*it);
	}
	return pmtAdr;
}

void SpaceAllocator::deallocatePMT(PhysicalAddress adr, PMTType type, PageNum segmentSize) {
	size_t size;

	if (type == PMTType::LEVEL1_PMT) size = SpaceAllocator::pmt1Size;
	else if (type == PMTType::LEVEL2_PMT) size = SpaceAllocator::pmt2Size;
	else size = segmentSize * sizeof(Descriptor);

	FreeSpaceDescriptor newDesc(adr, size);

	if (this->pmtFreeSpace.empty()) {
		this->pmtFreeSpace.push_front(newDesc);
	}

	else {
		std::list<FreeSpaceDescriptor>::iterator it, begin, end;

		begin = pmtFreeSpace.begin();
		end = pmtFreeSpace.end();

		for (it = begin; (it != end); ++it) {
			if ((it->space > adr)) break;
		}

		if (it == pmtFreeSpace.begin() && it->space < pmtFreeSpace.begin()->space) this->pmtFreeSpace.push_front(newDesc);

		else if (it == pmtFreeSpace.end()) {
			auto it2 = --it;
			if (adr > it2->space)
				this->pmtFreeSpace.push_back(newDesc);
		}

		else {
			if (it != pmtFreeSpace.begin()) --it;
			this->pmtFreeSpace.insert(it, newDesc);
		}
	}
}

PhysicalAddress SpaceAllocator::allocatePage() throw(MemoryException) {
	//DummyMutex dummy(this->memoryMutex);

	if (this->processVMFreeSpace.empty()) {
		PhysicalAddress adr = this->mySystem->swapPage();

		this->processVMFreeSpace.push_front(FreeSpaceDescriptor(adr, 1));
	}

	FreeSpaceDescriptor& desc = this->processVMFreeSpace.front(); //Uzimanje reference na prvi element u listi

	PhysicalAddress pageAdr = desc.space;

	if (desc.size > 1) {
		desc.space = (PhysicalAddress)((char*)desc.space + PAGE_SIZE);
		--desc.size;
	}

	else {
		this->processVMFreeSpace.pop_front();
	}
	
	return pageAdr;
}

void SpaceAllocator::deallocatePage(PhysicalAddress page) {
	//DummyMutex dummy(this->memoryMutex);

	FreeSpaceDescriptor newDesc(page, 1);

	if (this->processVMFreeSpace.empty()) {
		this->processVMFreeSpace.push_front(newDesc);
	}

	else {
		std::list<FreeSpaceDescriptor>::iterator it, begin, end;

		begin = processVMFreeSpace.begin();
		end = processVMFreeSpace.end();

		for (it = begin; (it != end); ++it) { 
			if ((it->space > page)) break;
		}

		if (it == processVMFreeSpace.begin() && it->space < processVMFreeSpace.begin()->space) this->processVMFreeSpace.push_front(newDesc);

		else if (it == processVMFreeSpace.end()) {
			auto it2 = --it;
			if (page > it2->space)
				this->processVMFreeSpace.push_back(newDesc);
		}

		else {
			if (it != processVMFreeSpace.begin()) --it;
			this->processVMFreeSpace.insert(it, newDesc);
		}
	}
}
