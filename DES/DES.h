#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>

#include "InitialPermutation.h"
#include "FeistelRound.h"
#include "FinalPermutation.h"
#include "KeySchedule.h"

#define BUFSIZE 1024
#include "DES_client_server/WireProtocol.h"

/*
typedef struct
{
    int len;         // 데이터 길이
    int blockNum;    // 블록 번호
    char data[1024]; // 데이터
    } Packet;
    */

void DES_Encryption(Packet &packet, const char *key);
void DES_Decryption(Packet &packet, const char *key);

uint64_t *StringToBlocks(const char *input, int &blockNum, int &len);
const char *BlocksToString(uint64_t *blocks, int blockNum, int len);
