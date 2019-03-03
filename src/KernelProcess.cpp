#include "KernelProcess.h"
#include "KernelSystem.h"
#include "MemoryException.h"
#include "SpaceAllocator.h"
#include "SharedSegment.h"
#include "DummyMutex.h"
#include "Process.h"
#include "PMT.h"
#include <cstdlib>
#include <iostream>
#include <mutex>

KernelProcess::KernelProcess(ProcessId pid, Process* myProcess) 
	:pid(pid), myProcess(myProcess), pmtHead(nullptr) {

}

KernelProcess::~KernelProcess() {
	KernelSystem::kernelSystem->deleteProcess(this->pid); //Brise se proces iz mape procesa
	
	//Dealociranje segmenata koje je proces koristio.
	for (int i = 0; (i < PMT1_SIZE) && (pmtHead != nullptr) && (pmtHead->entriesUsed > 0); i++) {
		if (pmtHead->level2entry[i] != nullptr) {

			PMT2* pmt2 = pmtHead->level2entry[i];

			for (int j = 0; (j < PMT2_SIZE) && (pmt2->entriesUsed > 0); j++) {
				if ((pmt2->entry[j].frameAndFlags & L_MASK) && (pmt2->entry[j].ordinal == 0)) { //Stranica je ucitana ako je setovan LOAD bit
					VirtualAddress adr = (VirtualAddress)i << PMT1_OFFSET;
					adr |= (VirtualAddress)j << PMT2_OFFSET;
					this->deleteSegment(adr);

					if ((pmtHead == nullptr) || (pmtHead->level2entry[i] == nullptr)) break;
					
				}
			}
		}
	}
	pmtHead = nullptr;
}

ProcessId KernelProcess::getProcessId() const {
	return this->pid;
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {

	this->checkSegment(startAddress, segmentSize);

	for (PageNum i = 0; i < segmentSize; i++) {

		PhysicalAddress frameAddr = nullptr;
		try { //Dohvatanje jedne stranice
			frameAddr = KernelSystem::kernelSystem->allocatePage();
		}

		catch (MemoryException e) { //Ako je bilo greske pri dohvatanju stranice, ispisuje se greska i vraca se TRAP.
			std::cout << e;
			return Status::TRAP;
		}

		if (!this->updatePMT(startAddress + i * PAGE_SIZE, frameAddr, i, flags, false)) { //Postavljanje odgovarajuceg deskriptora u tabeli stranica
			return Status::TRAP; //Nije bilo moguce apdejtovati PMT
		}
	}

	return Status::OK;
}

Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content) {
	

	this->checkSegment(startAddress, segmentSize);

	for (PageNum i = 0; i < segmentSize; i++) {

		PhysicalAddress frameAddr = nullptr;
		try { //Dohvatanje jedne stranice
			frameAddr = KernelSystem::kernelSystem->allocatePage();
		}

		catch (MemoryException e) { //Ako je bilo greske pri dohvatanju stranice, ispisuje se greska i vraca se TRAP.
			std::cout << e;
			return Status::TRAP;
		}

		if (!this->updatePMT(startAddress + i * PAGE_SIZE, frameAddr, i, flags, true)) { //Postavljanje odgovarajuceg deskriptora u tabeli stranica
			return Status::TRAP; //Nije bilo moguce apdejtovanje PMTa.
		}
		
		//Inicijalizacija stranice prosledjenim sadrzajem
		char* initBuffer = (char*)content  + i * PAGE_SIZE;
		char* frameBuffer = (char*)frameAddr;

		for (int i = 0; i < PAGE_SIZE; i++) {
			frameBuffer[i] = initBuffer[i];
		}
	}

	return Status::OK;
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress) {

	if (startAddress & WORD_MASK) { //Provera da li je adresa poravnata na pocetak stranice
		std::cout << "GRESKA: metoda deleteSegment | Pocetna adresa nije poravnata na pocetak segmenta.\n";
		return Status::TRAP;
	}

	if (!this->checkAllocated(startAddress)) {
		std::cout << "GRESKA: metoda deleteSegment | Prosledjena adresa nije pocetak segmenta..\n";
		return Status::TRAP;
	}

	PMT2* pmt2 = pmtHead->level2entry[(startAddress >> PMT1_OFFSET) & PMT_ENTRY_MASK];
	
	Descriptor* desc = &pmt2->entry[(startAddress >> PMT2_OFFSET) & PMT_ENTRY_MASK];

	if (desc->ordinal != 0) {
		std::cout << "GRESKA: metoda deleteSegment | Prosledjena adresa nije adresa prve stranice u segmentu.\n";
		return Status::TRAP;
	}

	if (desc->frameAndFlags & SH_MASK) {
		std::cout << "GRESKA: metoda deleteSegment | Pokusaj brisanja deljenog segmenta.\n";
		return Status::TRAP;
	}

	bool first = true;
	PageNum i = 0;
	while ((desc->ordinal != 0 && (desc->frameAndFlags & L_MASK) && (pmt2 != nullptr)) || first) {
		if (first) {
			first = false;
		}
	
		PhysicalAddress frameAddress = (PhysicalAddress)((desc->frameAndFlags & FRAME_MASK) << ADR_WORD);

		KernelSystem::kernelSystem->deallocatePage(frameAddress); //Dealociranje jedne stranice

		desc->frameAndFlags = 0;

		if (desc->frameAndFlags & S_MASK) { //Ako je bio swapowan, postavlja se da je klaster slobodan
			KernelSystem::kernelSystem->setClusterFree(desc->disk);
		}

		if (--pmt2->entriesUsed == 0) { //Brisanje tabele drugog nivoa ako se vise ne koristi ni jedan ulaz
			KernelSystem::kernelSystem->deallocatePMT(pmt2, PMTType::LEVEL2_PMT);
			pmtHead->level2entry[((startAddress + i * PAGE_SIZE) >> PMT1_OFFSET) & PMT_ENTRY_MASK] = nullptr;
			--pmtHead->entriesUsed;
		}

		++i;
		pmt2 = pmtHead->level2entry[((startAddress + i * PAGE_SIZE)>> PMT1_OFFSET) & PMT_ENTRY_MASK]; 
		if (pmt2 != nullptr) {
			desc = &pmt2->entry[((startAddress + i * PAGE_SIZE) >> PMT2_OFFSET) & PMT_ENTRY_MASK];
		}
	}

	if (pmtHead->entriesUsed == 0) {
		KernelSystem::kernelSystem->deallocatePMT(pmtHead, PMTType::LEVEL1_PMT);
		pmtHead = nullptr;
	}
	return Status::OK;
}

Status KernelProcess::deleteSegmentLock(VirtualAddress startAddress)
{
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);
	return this->deleteSegment(startAddress);
}

Status KernelProcess::pageFault(VirtualAddress address) {
	

	if (this->pmtHead == nullptr) {  //Nije alocirana ni jedna stranica
		std::cout << "Metoda pageFault | Trazena stranica nije bila ucitana metodom create ili load segment.\n";
		return Status::TRAP;
	}

	PMT2* pmt2;
	if ((pmt2 = pmtHead->level2entry[(address >> PMT1_OFFSET) & PMT_ENTRY_MASK]) == nullptr) { //U potrebnom ulazu nije alocirana tabela drugog nivoa
		std::cout << "Metoda pageFault | Trazena stranica nije bila ucitana metodom create ili load segment.\n";
		return Status::TRAP;
	}

	Descriptor& desc = pmt2->entry[(address >> PMT2_OFFSET) & PMT_ENTRY_MASK];
	if (!(desc.frameAndFlags & L_MASK)) { //Nije ucitana stranica
		std::cout << "Metoda pageFault | Trazena stranica nije bila ucitana metodom create ili load segment.\n";
		return Status::TRAP;
	}

	if (desc.frameAndFlags & SH_MASK) {
		desc = *desc.sharedDesc;
	}

	if (desc.frameAndFlags & V_MASK) { //Stranica je vec ucitana
		return Status::OK;
	}

	PhysicalAddress addr = nullptr;
	try {
		addr = KernelSystem::kernelSystem->allocatePage();
	}

	catch (MemoryException e) { //Ovaj exception se desava ako nema slobodnog prostora na klasteru ili je doslo do greske prilikom swapovanja stranice na particiju
		std::cout << e;
		return Status::TRAP;
	}

	//Stranica moze biti kreirana ali bez ikakvog upisa, tada se stranica ne swapuje na disk
	//ukoliko je bilo upisa, svapovace se. Ako nije svapovana, samo ce se ucitati nova stranica
	//i dodeliti procesu.
	if (desc.frameAndFlags & S_MASK) {
		char *buffer = (char*)addr;
#ifdef PRINT
		std::cout << "Metoda PageFault | Citanje stranice sa diska.\n";
#endif
		if (!KernelSystem::kernelSystem->partition->readCluster(desc.disk, buffer)) {
			return Status::TRAP;
		}
	}

	desc.frameAndFlags &= FRAME_MASK_DELETE;
	desc.frameAndFlags |= (((unsigned int)addr) >> ADR_WORD) & FRAME_MASK; //Upisivanje novog broja frejma

	desc.frameAndFlags |= SET_V; //Setovanje V bita
	desc.frameAndFlags &= RESET_D; //Resetovanje D bita
	
#ifdef PRINT
	std::cout << "Metoda PageFault | Vracena stranica sa diska | Virtuelna adresa = " << address << "\n";
#endif
	return Status::OK;
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address) {
	if (this->pmtHead == nullptr) {  //Nije alocirana ni jedna stranica
		std::cout << "Metoda GetPhysicalAddress | Nedozvoljeno preslikavanje\n";
		std::exit(1);
	}

	PMT2* pmt2;
	if ((pmt2 = pmtHead->level2entry[(address >> PMT1_OFFSET) & PMT_ENTRY_MASK]) == nullptr) { //U potrebnom ulazu nije alocirana tabela drugog nivoa
		std::cout << "Metoda GetPhysicalAddress | Nedozvoljeno preslikavanje\n";
		std::exit(1);
	}
	
	Descriptor& desc = pmt2->entry[(address >> PMT2_OFFSET) & PMT_ENTRY_MASK];
	if (!(desc.frameAndFlags & L_MASK)) { //Nije ucitana stranica
		std::cout << "Metoda GetPhysicalAddress | Nedozvoljeno preslikavanje\n";
		std::exit(1);
	}

	if (desc.frameAndFlags & SH_MASK) {
		desc = *desc.sharedDesc;
	}

	if (!(desc.frameAndFlags & V_MASK)) { //Stranica je bila ucitana ali je swapovana, generise se page fault da bi se prvo dovukla
		this->myProcess->pageFault(address);
	}

	unsigned int intAddr = (desc.frameAndFlags & FRAME_MASK) << ADR_WORD;
	intAddr |= address & WORD_MASK;

	PhysicalAddress addr = (PhysicalAddress)intAddr;

	return addr;
}


//=============================SHARING SEGMENTS METHODS===================================================//


Status KernelProcess::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * name, AccessType flags)
{
	

	this->checkSegment(startAddress, segmentSize); //Provera virtuelne adrese i broja stranica

	auto seg = KernelSystem::kernelSystem->sharedSegments.find(name); //Dohvatanje segmenta sa prosledjenim imenom, ako je prethodno kreiran, ako nije kreirace se
	
	if (seg == KernelSystem::kernelSystem->sharedSegments.end()) {
		
		SharedSegment* shared = new SharedSegment(name, startAddress, segmentSize, flags); //Kreiranje deskriptora deljenog segmenta

		shared->pmt.entry = (Descriptor*)KernelSystem::kernelSystem->spaceAllocator->allocatePMT(PMTType::SHARED_SEG_PMT, segmentSize);

		if (shared->pmt.entry == nullptr) { //Nije uspelo alociranje memorije za smestanje PMTa segmenta
			return Status::TRAP;
		}

		for (PageNum i = 0; i < segmentSize; i++) {

			PhysicalAddress frameAddr = nullptr;
			try { //Dohvatanje jedne stranice
				frameAddr = KernelSystem::kernelSystem->allocatePage();
			}

			catch (MemoryException e) { //Ako je bilo greske pri dohvatanju stranice, ispisuje se greska i vraca se TRAP.
				std::cout << e;
				return Status::TRAP;
			}

			shared->pmt.entry[i].disk = 0;
			shared->pmt.entry[i].ordinal = i;
			shared->pmt.entry[i].frameAndFlags = 0;
			shared->pmt.entry[i].frameAndFlags |= (SET_V | SET_L); //Postavljanje valid i loaded bita
			shared->pmt.entry[i].frameAndFlags |= (unsigned int)flags << ACCESS_BITS_SHIFT; //Postavljanje prava pristupa
			shared->pmt.entry[i].frameAndFlags |= (unsigned int)frameAddr >> ADR_WORD; //Postavljanje broja frejma u RAM memoriji

			if (!this->updatePMT(startAddress + i * PAGE_SIZE, frameAddr, i, flags, false, true, &shared->pmt.entry[i])) { //Postavljanje odgovarajuceg deskriptora u tabeli stranica
				return Status::TRAP; //Nije bilo moguce apdejtovati PMT
			}
		}
		
		shared->processesUsing++; //Uvecavanje broja procesa koji koriste deljeni segment.
		shared->processes.insert({ this->pid, startAddress }); //Ubacivanje procesa koji koristi deljeni segment i pocetne adrese segmenta u njegovom virtuelnom adresnom prostoru u mapu procesa koji koriste deljeni segment

		KernelSystem::kernelSystem->sharedSegments.insert({ name, shared }); //Ubacivanje deskriptora deljenog segmenta u mapu deljenih segmenata

		return Status::OK;
	}
	else {
		SharedSegment* shared = seg->second;

		if (shared->getSegmentSize() != segmentSize) {
			std::cout << "Greska: Metoda createSharedSegment | Pokusaj da se doda vec kreiran segment u memorijski prostor, velicine segmenata nekompatibilne.\n";
			return Status::TRAP;
		}

		if (!((shared->getAccess() == flags) || ((shared->getAccess() == AccessType::READ_WRITE) && (shared->getAccess() > flags)))) { //Provera da li proces zeli da koristi segment sa pravima koja nisu u skladu sa onima vec dodeljenim segmentu
			std::cout << "Greska: Metoda createSharedSegment | Pokusaj da se doda vec kreiran segment u memorijski prostor, prava pristupa nekompatibilna.\n";
			return Status::TRAP;
		}
		else {

			int i = 0;

			for (PageNum i = 0; i < shared->getSegmentSize(); i++) {
				unsigned int intAddr = (shared->pmt.entry[i].frameAndFlags & FRAME_MASK) << ADR_WORD;
				PhysicalAddress frameAddr = (PhysicalAddress)intAddr;

				if (!this->updatePMT(startAddress + i * PAGE_SIZE, frameAddr, i, flags, false, true, &(shared->pmt.entry[i]))) {
					return Status::TRAP; //Nije bilo moguce apdejtovati PMT
				}
			}

			shared->processesUsing++;
			shared->processes.insert({ this->pid, startAddress }); //Dodavanje procesa u mapu procesa koji koriste segment
		}
	}

	return Status::OK;
}

Status KernelProcess::disconnectSharedSegmentLock(const char * name)
{
	DummyMutex dummy(KernelSystem::kernelSystem->globalMutex);

	return this->disconnectSharedSegment(name);
}

Status KernelProcess::disconnectSharedSegment(const char * name) {

	auto segments = KernelSystem::kernelSystem->sharedSegments; //Mapa svih deljenih segmenata

	auto segmentPtr = segments.find(name); //Segment koji se trazi

	if (segmentPtr == segments.end()) {
		std::cout << "GRESKA: metoda disconnectSharedSegment | Deljeni segment sa zadatim imenom ne postoji.\n";
		return Status::TRAP;
	}
	auto segment = segmentPtr->second;

	auto startAddressPtr = segment->processes.find(this->pid);

	if (startAddressPtr == segment->processes.end()) {
		std::cout << "GRESKA: metoda disconnectSharedSegment | Proces ne koristi segment sa zadatim imenom.\n";
		return Status::TRAP;
	}

	VirtualAddress startAddress = startAddressPtr->second; //Adresa pocetka segmenta u virtuelnom adresnom prostoru procesa
	PageNum size = segment->getSegmentSize(); //Velicina segmenta

	for (PageNum i = 0; i < size; i++) {
		if (!this->checkAllocated(startAddress + i * PAGE_SIZE)) {
			std::cout << "GRESKA: metoda disconnectSharedSegment | Greska u PM tabeli procesa.\n";
			return Status::TRAP;
		}

		unsigned char entry1 = (startAddress >> PMT1_OFFSET) & PMT_ENTRY_MASK;
		unsigned char entry2 = (startAddress >> PMT2_OFFSET) & PMT_ENTRY_MASK;

		pmtHead->level2entry[entry1]->entry[entry2].frameAndFlags = 0;

		if (--pmtHead->level2entry[entry1]->entriesUsed == 0) { //Oslobadjanje pm tabele ako su svi ulazi slobodni
			KernelSystem::kernelSystem->deallocatePMT(pmtHead->level2entry[entry1], PMTType::LEVEL2_PMT);
			pmtHead->level2entry[entry1] = nullptr;
			--pmtHead->entriesUsed;
		}
	}

	if (pmtHead->entriesUsed == 0) { //Oslobadjanje pm tabele ako su svi ulazi slobodni
		KernelSystem::kernelSystem->deallocatePMT(pmtHead, PMTType::LEVEL1_PMT);
		pmtHead = nullptr;
	}

	--segment->processesUsing;
	segment->processes.erase(this->pid);

	return Status::OK;
}

Status KernelProcess::deleteSharedSegment(const char * name) {

	auto segments = KernelSystem::kernelSystem->sharedSegments; //Mapa svih deljenih segmenata

	auto segmentPtr = segments.find(name); //Segment koji se trazi

	if (segmentPtr == segments.end()) {
		std::cout << "GRESKA: metoda disconnectSharedSegment | Deljeni segment sa zadatim imenom ne postoji.\n";
		return Status::TRAP;
	}
	auto segment = segmentPtr->second; //Dohvatanje pokazivaca na deskriptor deljenog segmenta

	auto processes = segment->processes; //Dohvatanje mape procesa koji koriste deljeni segment

	for (auto it : processes) {
		auto process = KernelSystem::kernelSystem->processMap.find(it.first); //Dohvatanje PCBa procesa koji koristi deljeni segment

		if (process == KernelSystem::kernelSystem->processMap.end()) continue; //swallow

		process->second->disconnectSharedSegment(name); //Odvezivanje deljenog segmenta iz procesa koji ga koristi
	}

	KernelSystem::kernelSystem->spaceAllocator->deallocatePMT(segment->pmt.entry, PMTType::SHARED_SEG_PMT, segment->getSegmentSize()); //Dealociranje tabele deljenog segmenta

	segments.erase(name); //Brisanje segmenta iz mape segmenata

	delete segment; //Brisanje deskriptora

	return Status::OK;
}

//============================PRIVATE METHODS===========================//

Status KernelProcess::checkSegment(VirtualAddress startAddress, PageNum segmentSize) {
	if (startAddress & WORD_MASK) { //Provera da li je adresa poravnata na pocetak stranice
		std::cout << "GRESKA: metoda createSharedSegment | Pocetna adresa nije poravnata na pocetak segmenta.\n";
		return Status::TRAP;
	}

	if (startAddress + segmentSize * PAGE_SIZE > VIRTUAL_MEMORY_LAST_ADDRESS) { //Provera da li je doslo do prekoracenja segmenta
		std::cout << "GRESKA: metoda createSharedSegment | Segment izvan granica virtuelne memorije.\n";
		return Status::TRAP;
	}

	if (segmentSize == 0) {
		std::cout << "GRESKA: metoda createSharedSegment | Segment ne moze biti velicine 0.\n";
		return Status::TRAP;
	}

	for (PageNum i = 0; i < segmentSize && pmtHead != nullptr; i++) { //Provera da li se zeljeni segment preklapa sa vec dodeljenim.
		if (this->checkAllocated(startAddress + i * PAGE_SIZE)) {
			std::cout << "GRESKA: metoda createSharedSegment | Segment se preklapa sa vec alociranim segmentom.\n";
			return Status::TRAP;
		}
	}

	return Status::OK;
}

bool KernelProcess::checkAllocated(VirtualAddress startAddress) {

	unsigned char entry1 = (startAddress >> PMT1_OFFSET) & PMT_ENTRY_MASK;
	unsigned char entry2 = (startAddress >> PMT2_OFFSET) & PMT_ENTRY_MASK;

	if (this->pmtHead == nullptr) return false; //Ako nije alocirana tabela prvog nivoa, tada ni jedna stranica nije dodeljena procesu

	if (this->pmtHead->level2entry[entry1] == nullptr) return false; //Ako nije u odgovarajucem deskriptoru alocirana tabela drugog nivoa, tada nije ni trazena stranica dodeljena procesu

	if (this->pmtHead->level2entry[entry1]->entry[entry2].frameAndFlags & L_MASK) return true; //U odgovarajucem ulazu je alocirana trazena stranica

	else return false; //U odgovarajucem ulazu nije alocirana stranica

}

bool KernelProcess::updatePMT(VirtualAddress page, PhysicalAddress frame, PageNum ordinal, AccessType flags, bool setD, bool setSh, Descriptor* sharedDesc) {

	unsigned char entry1 = (page >> PMT1_OFFSET) & PMT_ENTRY_MASK;
	unsigned char entry2 = (page >> PMT2_OFFSET) & PMT_ENTRY_MASK;

	if (this->pmtHead == nullptr) { //Ako je true, nije alocirana tabela prvog nivoa
		PhysicalAddress adr = KernelSystem::kernelSystem->allocatePMT(PMTType::LEVEL1_PMT);

		if (adr == nullptr) { //Ako metoda allocatePmt vrati nullptr znaci da nema dovoljno prostora za PMT
			std::cout << "GRESKA: Metoda updatePMT | Nema dovoljno prostora za alociranje PMTa.\n";
			return false;
		}

		this->pmtHead = (PMT1*)adr;

		//Inicijalizacija PM tabele prvog nivoa
		for (int i = 0; i < PMT1_SIZE; i++)
			this->pmtHead->level2entry[i] = nullptr;
		
		this->pmtHead->entriesUsed = 0;
	}

	if (this->pmtHead->level2entry[entry1] == nullptr) { //Ako je true, znaci da u odgovarajucem ulazu tabele 1. nivoa nije alocirana tabela drugog nivoa
		PhysicalAddress adr = KernelSystem::kernelSystem->allocatePMT(PMTType::LEVEL2_PMT);

		if (adr == nullptr) { //Ako metoda allocatePmt vrati nullptr znaci da nema dovoljno prostora za PMT
			std::cout << "GRESKA: Metoda updatePMT | Nema dovoljno prostora za alociranje PMTa.\n";
			return false;
		}

		PMT2* pmt2 = (PMT2*)adr;

		//Inicijalizacija PM tabele drugog nivoa
		for (int i = 0; i < PMT2_SIZE; i++) {
			pmt2->entry[i].frameAndFlags = 0;
		}
		pmt2->entriesUsed = 0;
		
		++this->pmtHead->entriesUsed; //Povecavanje broja alociranih tabela drugog nivoa
		this->pmtHead->level2entry[entry1] = pmt2; //Postavljenje pokazivaca u tabeli prvog nivoa da pokazuje na alociranu tabelu drugog nivoa.
	}

	if (this->pmtHead->level2entry[entry1]->entry[entry2].frameAndFlags & L_MASK) { //Ako je true, odgovarajuci ulaz je zauzet.
		std::cout << "GRESKA: Metoda updatePMT | U odgovarajucem deskriptoru prosledjene adrese je vec alocirana stranica.\n";
		return false;
	}

	Descriptor& desc = this->pmtHead->level2entry[entry1]->entry[entry2];

	desc.frameAndFlags = 0; //Inicijalno stanje
	if (setSh) {
		desc.sharedDesc = sharedDesc;
		desc.frameAndFlags |= SET_SH;
	}
	else {
		desc.disk = 0; //Na pocetku stranica nije swapovana
	}

	desc.ordinal = ordinal; //Postavljanje rednog broja u segmentu
	desc.frameAndFlags |= (SET_V | SET_L); //Postavljanje valid i loaded bita
	desc.frameAndFlags |= setD ? SET_D : 0; //Postavljanje D bita
	desc.frameAndFlags |= (unsigned int)flags << ACCESS_BITS_SHIFT; //Postavljanje prava pristupa
	desc.frameAndFlags |= (unsigned int)frame >> ADR_WORD; //Postavljanje broja frejma u RAM memoriji
	
	++this->pmtHead->level2entry[entry1]->entriesUsed;
	
	return true;
}
