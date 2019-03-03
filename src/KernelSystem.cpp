//vm_declarations.h included in KernelSystem.h
#include "KernelSystem.h"
#include "Process.h"
#include "KernelProcess.h"
#include "PMT.h"
#include "SpaceAllocator.h"
#include "DummyMutex.h"
#include "FreeSpaceDescriptor.h"
#include "MemoryException.h"
#include <iostream>
#include <unordered_map>
#include <mutex>


ProcessId KernelSystem::nextPid = 0;
KernelSystem* KernelSystem::kernelSystem = nullptr;

KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition, System* mySystem) 
		: processVMSpace(processVMSpace), processVMSpaceSize(processVMSpaceSize), pmtSpace(pmtSpace),
			pmtSpaceSize(pmtSpaceSize), partition(partition), mySystem(mySystem) {
	
	KernelSystem::kernelSystem = this;

	this->numberOfClusters = this->partition->getNumOfClusters();
	
	this->referenceBits = new unsigned char[(processVMSpaceSize / REF_BITS_HOLDER_SIZE) + (processVMSpaceSize % REF_BITS_HOLDER_SIZE == 0 ? 0 : 1)]{ 0 };

	spaceAllocator = new SpaceAllocator(this, pmtSpace, pmtSpaceSize, processVMSpace, processVMSpaceSize, numberOfClusters, partition);

	freeClusters.push_front(ClustersFree(0, this->numberOfClusters));
	this->globalMutex = new std::mutex();
}

KernelSystem::~KernelSystem() {
	std::unordered_map<ProcessId, Process*> map(processMap);
	for (auto it: map) {
		if (it.second->pProcess != nullptr)
			delete it.second->pProcess;
		it.second->pProcess = nullptr;
	}

	delete spaceAllocator;
	delete[] referenceBits;
	processMap.clear();
	freeClusters.clear();
	delete globalMutex;

	KernelSystem::kernelSystem = nullptr;
}

Time KernelSystem::periodicJob() {
	
	return 0;

	/*std::list<FreeSpaceDescriptor>::iterator prev, curr;
	
	bool first = true;

	while (true) {
		if (!first) {
		}
	}
	
	return 0;*/
}

Process* KernelSystem::createProcess() {

	Process* pcb = new Process(++KernelSystem::nextPid);

	const ProcessId pid = pcb->getProcessId();
	this->processMap.insert({pcb->getProcessId(), pcb});
	
	return pcb;
}

void KernelSystem::deleteProcess(ProcessId pid) {
	this->processMap.erase(pid);
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type) {

	auto pcbIterator = processMap.find(pid);

	Process *pcb = nullptr;

	if (pcbIterator != processMap.end()) {
		pcb = pcbIterator->second;
	}

	//Ako je pcb jednak nullptr-u znaci da proces sa unetim id-jem ne postoji.
	if (pcb == nullptr) {
		std::cout << "Metoda Access | Status = TRAP | pcb == nullptr\n";
		return Status::TRAP;
	}

	PMT1 *pmtHead = pcb->pProcess->pmtHead; //Uzmi pokazivac na PMT 1. nivoa

	if (pmtHead == nullptr) {
		std::cout << "Metoda Access | Status = TRAP | pmtHead == nullptr\n";
		return Status::TRAP; //Ako PMT 1. nivoa nije alociran, stranica nije ucitana
	}

	PMT2 *pmt2 = pmtHead->level2entry[(address >> PMT1_OFFSET) & PMT_ENTRY_MASK]; //Dohvati PMT 2. nivoa

	if (pmt2 == nullptr) {
		std::cout << "Metoda Access | Status = TRAP | pmt2 == nullptr\n";
		return Status::PAGE_FAULT; //Ako PMT 2. nivoa nije alocirana, stranica nije ucitana
	}

	Descriptor& desc = pmt2->entry[(address >> PMT2_OFFSET) & PMT_ENTRY_MASK]; //Dohvati referencu na deskriptor

	if (!(desc.frameAndFlags & L_MASK)) { //Ako je false, stranica nije dodeljena procesu.
		std::cout << "Metoda Access | Status = TRAP | Trazena stranica nije u memoriji.\n";
	}

	if (desc.frameAndFlags & SH_MASK) {
		desc = *desc.sharedDesc; //Ako je deljeni segment dohvati stvarni deskriptor segmenta
	}

	if (desc.frameAndFlags & V_MASK) { //Ako je setovan V bit, stranica je u memoriji
		char rights = (desc.frameAndFlags & ACCESS_BITS_MASK) >> ACCESS_BITS_SHIFT; //Dohvati bite za prava
		if ((rights == type) ||
			((rights == AccessType::READ_WRITE) && ((type == AccessType::READ) || (type == AccessType::WRITE)))) {

			//std::cout << "Metoda Access | Status = OK | Virtuelna Adresa = " << address << "\t\tTip = " << ((type == AccessType::READ) ? "READ" : (type == AccessType::WRITE) ? "WRITE" : "EXECUTE") << "\n";

			if (type == AccessType::WRITE) { //Ako proces upisuje u stranicu potrebno je setovati dirty bit
				desc.frameAndFlags |= SET_D; 
			}
			
			return Status::OK; //Ako su prava pristupa jednaka trazenim pravima, vrati OK
		}

		else {
			std::cout << "Metoda Access | Status = TRAP | Nema trazeno pravo pristupa\n";
			return Status::TRAP; //U suprotnom se vraca TRAP.
		}
	}
	else { //Ako V bit nije setovan, stranica nije u memoriji.
#ifdef PRINT
		std::cout << "Metoda Access | Status = PAGE_FAULT | Virtuelna adresa = " << address << "\n";
#endif
		return Status::PAGE_FAULT;
	}
}

PhysicalAddress KernelSystem::swapPage() throw(MemoryException) {
	//std::cout << "Swapping page.\n";
	while (true) { //Trazenje stranice za izbacivanje po Second chance algoritmu
		unsigned long byte = this->clockHand / REF_BITS_HOLDER_SIZE;
		char bit = this->clockHand % REF_BITS_HOLDER_SIZE;

		if (referenceBits[byte] & (1 << bit)) {
			referenceBits[byte] &= ~(1 << bit);
			this->clockHand = (this->clockHand + 1) % this->processVMSpaceSize;
		}
		else {
			break;
		}
	}

	unsigned int pageAdr = (unsigned int)processVMSpace + this->clockHand * PAGE_SIZE;
	unsigned int frame = pageAdr >> 10;
	
	PageNum swappedPage = this->clockHand;
	this->clockHand = (this->clockHand + 1) % this->processVMSpaceSize;

	for (auto it = processMap.begin(); it != processMap.end(); ++it) {
		Descriptor* desc = this->checkTables(it->second->pProcess->pmtHead, frame); //Provera da li je trenutni proces alocirao trazenu stranicu
		
		if (desc != nullptr) { //Ako je desc razlicito od nullptr onda je proces ukazan sa it alocirao trazenu stranicu
			
			if (desc->frameAndFlags & SH_MASK) {
				desc = desc->sharedDesc;
				//frameAndFlags = desc->frameAndFlags;
			}
			unsigned int frameAndFlags = desc->frameAndFlags;
			
			if (frameAndFlags & V_MASK) {
				if (frameAndFlags & D_MASK) { //Ako je stranica modifikovana, swapuj je na disk
					const char* buffer = (const char*)((frameAndFlags & FRAME_MASK) << ADR_WORD); //Adresa pocetka stranice

					if (!(frameAndFlags & S_MASK)) { //Ako je bila swapovana, vec joj je dodeljen broj klastera, i nalazi se u njenom deskriptoru, u suprotnom dohvati slobodan klaster
						ClusterNo cluster = this->getFreeCluster();
						desc->disk = cluster;
						frameAndFlags |= SET_S;
					}

					if (!this->partition->writeCluster(desc->disk, buffer)) { //upisi na klaster
						throw MemoryException("Greska pri upisu stranice na klaster");
					}
				}

				frameAndFlags &= RESET_V;
				frameAndFlags &= RESET_D;

				desc->frameAndFlags = frameAndFlags;

			}
			break;
		}
			
	}

	return (PhysicalAddress)pageAdr;
}

Descriptor* KernelSystem::checkTables(PMT1 * pmt1, unsigned int frame) {
	if (pmt1 == nullptr) return nullptr;

	for (int i = 0; i < PMT1_SIZE; i++) {
		if (pmt1->level2entry[i] == nullptr) continue;
		
		PMT2* pmt2 = pmt1->level2entry[i];

		for (int j = 0; j < PMT2_SIZE; j++) {
			unsigned int frameAndFlags = pmt2->entry[j].frameAndFlags;

			if ((frameAndFlags & L_MASK) && (frameAndFlags && V_MASK) 
				&& ((frameAndFlags & FRAME_MASK) == frame)) { //Ako je dodeljena stranica procesu, ako je u memoriji i ako se broj frejma poklapa sa trazenim frejmom
				
				return &pmt2->entry[j];
			}
		} //Kraj iteriranja kroz stranicu drugog nivoa
	}

	return nullptr;
}

ClusterNo KernelSystem::getFreeCluster() throw(MemoryException) {
	if (freeClusters.empty()) throw MemoryException("Nema slobodnih klastera na disku");

	std::list<ClustersFree>::iterator begin = freeClusters.begin();

	ClusterNo cluster = (*begin).first;
	
	if (--(*begin).num == 0) {
		freeClusters.pop_front();
	}
	else {
		++(*begin).first;
	}

	return cluster;
}

void KernelSystem::setClusterFree(ClusterNo cluster) {
	this->freeClusters.push_front(ClustersFree(cluster, 1));
}

PhysicalAddress KernelSystem::allocatePMT(PMTType type) {
	return this->spaceAllocator->allocatePMT(type);
}

void KernelSystem::deallocatePMT(PhysicalAddress adr, PMTType type) {
	this->spaceAllocator->deallocatePMT(adr, type);
}

void KernelSystem::deallocatePage(PhysicalAddress page) {
	this->spaceAllocator->deallocatePage(page);
}

PhysicalAddress KernelSystem::allocatePage() throw(MemoryException) {
	return this->spaceAllocator->allocatePage();
}

Process* KernelSystem::cloneProcess(ProcessId pid) {
	
	Process* newPcb = new Process(++KernelSystem::nextPid); //PCB novog procesa

	this->processMap.insert({ newPcb->getProcessId(), newPcb }); //Ubacivanje PCB-a novog procesa u mapu svih procesa

	auto it = this->processMap.find(pid); //Pronalazenje pokazivaca na pcb procesa koji se kopira

	if (it == this->processMap.end()) {
		std::cout << "GRESKA: metoda cloneProcess | Ne postoji proces sa prosledjenim ID-jem\n";
		return nullptr;
	}

	KernelProcess *oldKP = it->second->pProcess, *newKP = newPcb->pProcess; //PCB procesa koji se kopira

	if (oldKP->pmtHead == nullptr) {
		return newPcb;
	}

	bool shared; //Indikator da li je trenutni segment deljen
	unsigned int i = 0, j = 0;
	PageNum segmentSize = 0; //Velicina trenutnog segmenta
	VirtualAddress startAddress; //Pocetna adresa segmenta koji se kopira
	AccessType flags; //Flegovi trenutnog segmenta

	while (i < PMT1_SIZE) {
		
		j = 0; //Brojac stranica pmt-a drugog nivoa

		if (oldKP->pmtHead->level2entry[i] == nullptr) { //Provera da li je alocirana stranica drugog nivoa

			if (segmentSize != 0) { //Ako nije proverava se da li je bio pronadjen neki od segmenata, ako jeste kopira se u klonirani proces
				this->initSegment(startAddress, newKP, oldKP, segmentSize, pid, flags, shared);
				segmentSize = 0;
			}
			i++;
			continue;
		}

		while (j < PMT2_SIZE) { //Itearacija kroz PMT 2. nivoa
			PMT2* oldPmt2 = oldKP->pmtHead->level2entry[i];
			if ((oldPmt2->entry[j].frameAndFlags & L_MASK) && (segmentSize == oldPmt2->entry[j].ordinal)) { //Ako je L bit setovan i velicina segmenta je jednaka rednom broju stranice u segmentu, ili smo nasli nov segment ili smo jos uvek u vec pronadjenom
				if (segmentSize == 0) { //Pronadjen je novi segment, iniciranje potrebnih promenljivih
					startAddress = 0;
					startAddress |= (unsigned long)i << PMT1_OFFSET;
					startAddress |= (unsigned long)j << PMT2_OFFSET;
					
					if (oldPmt2->entry[j].frameAndFlags & SH_MASK) shared = true;
					else shared = false;
				
					flags = (AccessType)((oldPmt2->entry[j].frameAndFlags & ACCESS_BITS_MASK) >> ACCESS_BITS_SHIFT);
				}
				++segmentSize; //Uvecavanje broja stranica u pronadjenom segmentu
			}

			else if (segmentSize != 0) { //U suprotnom se proverava da li je prethodno bio pronadjen neki segment, ako jeste kopira se u kloniran proces
				this->initSegment(startAddress, newKP, oldKP, segmentSize, pid, flags, shared);
				segmentSize = 0;
				continue; //Preskacce se j++
			}
			j++;
		}
		i++;
	}

	if (segmentSize != 0) {
		this->initSegment(startAddress, newKP, oldKP, segmentSize, pid, flags, shared);
	}

	return newPcb;
}

void KernelSystem::copyContent(const char * src, char * dst) {
	for (unsigned long i = 0; i < PAGE_SIZE; i++) {
		dst[i] = src[i];
	}
}

void KernelSystem::initSegment(VirtualAddress startAddress, KernelProcess* newKP, KernelProcess* oldKP, PageNum segmentSize, ProcessId pid, AccessType flags, bool shared) {

	if (shared) {
		for (auto it : KernelSystem::kernelSystem->sharedSegments) { //Iteracija kroz mapu segmenata
			SharedSegment* sh = it.second;

			auto segPtr = sh->processes.find(pid); //Deskriptor deljenog segmenta procesa koji se kopira

			if (segPtr == sh->processes.end()) { //Ako proces nije pronadjen predji na sledeci deljeni segment
				continue;
			}

			if (segPtr->second != startAddress) { //Nije u pitanju segment koji nam je potreban 
				continue;
			}

			newKP->createSharedSegment(startAddress, segmentSize, it.first.data(), flags);
		}
	}

	else {
		newKP->createSegment(startAddress, segmentSize, flags);
		for (PageNum k = 0; k < segmentSize; k++) {
			VirtualAddress page = startAddress + k * PAGE_SIZE; //Dohvatanje pocetne adrese stranica

			unsigned char entry1 = (page >> PMT1_OFFSET) & PMT_ENTRY_MASK; //Ulaz u tabeli prvog nivoa
			unsigned char entry2 = (page >> PMT2_OFFSET) & PMT_ENTRY_MASK; //Ulaz u tabeli drugog nivoa

			Descriptor& oldDesc = oldKP->pmtHead->level2entry[entry1]->entry[entry2];
			Descriptor& newDesc = newKP->pmtHead->level2entry[entry1]->entry[entry2];

			if (!(newDesc.frameAndFlags & V_MASK)) { //Ako je stranica u koju je potrebno iskopirati sadrzaj bila zamenjenea
				newKP->pageFault(page);
			}

			if ((oldDesc.frameAndFlags & S_MASK) && !(oldDesc.frameAndFlags & V_MASK)) {//Menjan je, svapovan je i nije u memoriji
				PhysicalAddress dst = newKP->getPhysicalAddress(page);

				this->partition->readCluster(oldDesc.disk, (char*)dst);

				newDesc.frameAndFlags |= SET_D;
			}

			else if ((oldDesc.frameAndFlags & V_MASK) && ((oldDesc.frameAndFlags & D_MASK) || (oldDesc.frameAndFlags & S_MASK))) { //U memoriji je ali je ili menjan ili je bio swapovan
				PhysicalAddress src = oldKP->getPhysicalAddress(page); //Fizicka adresa izvorista
				PhysicalAddress dst = newKP->getPhysicalAddress(page); //Fizicka adresa odredista

				this->copyContent((const char*)src, (char*)dst); //Kopiranje sadrzaja

				newDesc.frameAndFlags |= SET_D;
			}
		}
	}
}