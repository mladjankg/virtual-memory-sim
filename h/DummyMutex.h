#pragma once

#include <mutex>

class DummyMutex {
public:
	DummyMutex(std::mutex* globalMutex) : globalMutex(globalMutex) {
		this->globalMutex->lock();
	}

	DummyMutex(DummyMutex&) = delete;

	~DummyMutex() { this->globalMutex->unlock(); }
private:
	std::mutex* globalMutex;
};