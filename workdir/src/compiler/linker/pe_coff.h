#ifndef PE_COFF_LINKER_H_00
#define PE_COFF_LINKER_H_00

#include "linker.h"

Object* PeObject_read(const uint8_t* buf, uint64_t size);

bool PeLibrary_read(ObjectSet* dest, const uint8_t* buf, uint64_t size);

void PeExectutable_create(Object* obj, ByteBuffer* dest, symbol_ix entrypoint);

#endif
