#pragma once

struct FreeSpaceDescriptor {

	FreeSpaceDescriptor(){}

	FreeSpaceDescriptor(PhysicalAddress adr, size_t size, FreeSpaceDescriptor* next = nullptr) : space(adr), next(next), size(size) {}

	bool operator==(const FreeSpaceDescriptor& desc) const {
		return (this->size == desc.size) && (this->space == desc.space);
	}

	PhysicalAddress space;
	FreeSpaceDescriptor* next;
	size_t size;
};

struct ClustersFree {
	ClustersFree(ClusterNo first, ClusterNo num) : first(first), num(num) {}
	ClusterNo first, num;
};