#pragma once
#include <cstdint>
#define BUFSIZE 512

// context -> 복호화를 위한 구조체, 직렬화 후 server을 통해 전달
typedef struct
{
    int len;
    int blockNum;
} DesContext;

// Input: clinet에서 입력, Chiper: 서버로 전달, Key: client 입력·오프라인에서 교환
const char *DES_Encryption(const char *input, const char *key, DesContext &context);
const char *DES_Decryption(const char *chiper, const char *key, DesContext &context);

uint64_t *StringToBlocks(const char *input, int &blockNum);
const char *BlocksToString(uint64_t *blocks, int &blockNum);