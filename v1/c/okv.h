#ifndef _OKV
#define _OKV

#include "dpf.h"

typedef struct{
	uint128_t rowID; //128 bit virtual address
    //uint128_t rowKey; //key used to encrypt for rerandomizing
    EVP_CIPHER_CTX *rowKey;
    int dataSize; //size of the data stored here
    uint8_t* mask; //current mask resulting from rerandomization
    uint8_t* data; //the actual data
} vatRow;

int initializeServer();

int processnewEntry(int dataSize, uint8_t *rowKey);

uint128_t registerQuery(unsigned char* dpfKey, int dataSize, int dataTransferSize);

void processQuery(void);

int getEntrySize(uint8_t *id, int index);

int readEntry(uint8_t *id, int index, uint8_t *data, uint8_t *seed);

#endif
