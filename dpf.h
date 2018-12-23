#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "aes.h"
#include "block.h"

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

/*
 * Not clear we need this
//code from
//https://locklessinc.com/articles/256bit_arithmetic/
typedef unsigned long long u64b;
typedef unsigned __int128 u128b;
typedef struct u256b u256b;
struct u256b
{
	u64b lo;
	u64b mid;
	u128b hi;
};

u256b add256b(u256b *x, u256b *y);

u256b mul256b(u256b *x, u256b *y);
*/

//DPF functions, PRG, GEN, and EVAL from libdpf

void dpfPRG(AES_KEY *key, block input, block* output1, block* output2, int* bit1, int* bit2);

void genDPF(AES_KEY *key, int domainSize, uint128_t index, int dataSize, uint8_t* data, unsigned char** k0, unsigned char **k1);

block evalDPF(AES_KEY *key, unsigned char* k, uint128_t x, int dataSize, uint8_t** dataShare);

//DPF checking functions
//written assuming 128 bit dpf domain

int getSeed(block* seed);

void PRF(AES_KEY *key, block seed, int layer, int count, block* output);

//client check inputs
void clientVerify(AES_KEY *key, block seed, int index, block aShare, block bShare, int dbLayers, uint8_t* bits, block* nonZeroVectors);

//server check inputs
void serverVerify(AES_KEY *key, block seed, int dbLayers, int dbSize, block* vectors, block* outVectors);

//auditor functionality
int auditorVerify(int dbLayers, uint8_t* bits, block* nonZeroVectors, block* outVectorsA, block* outVectorsB);
