#ifndef VALVE_CRC32_H_INCLUDED
#define VALVE_CRC32_H_INCLUDED

typedef unsigned int CRC32_t;

CRC32_t CRC32_ProcessSingleBuffer(const void* p, int len);

#endif