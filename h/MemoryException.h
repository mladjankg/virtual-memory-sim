#pragma once
#include <exception>
#include <string>

class MemoryException : std::exception {
public:
	MemoryException(std::string msg):message(msg) {}

	friend std::ostream& operator<<(std::ostream& os, MemoryException& e) {
		return os << e.message << std::endl;
	}

private:
	std::string message;
};