#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>

#include "InitialPermutation.h"
#include "FeistelRound.h"
#include "FinalPermutation.h"
#include "KeySchedule.h"

#define BUFSIZE 1024

typedef struct
{
    int len;
    int blcokNum;
    char data[BUFSIZE];
} Packet;

void DES_Encrtption(Packet &packet, const char *key);
void DES_Decryption(Packet &packet, const char *key);

uint64_t *StringToBlocks(const char *input, int &blockNum, int &len);
const char *BlocksToString(uint64_t *blocks, int blockNum, int len);
