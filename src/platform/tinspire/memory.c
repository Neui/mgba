#include <mgba-util/memory.h>

void* anonymousMemoryMap(size_t size) {
	return malloc(size);
}

void mappedMemoryFree(void* memory, size_t size) {
	UNUSED(size);
	free(memory);
}

