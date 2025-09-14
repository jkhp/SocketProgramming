#include "DES.h"

const char *DES_Encryption(const char *input, const char *key, DesContext &context)
{
    uint64_t *inputBlock = StringToBlocks(input, context.blockNum, context.len); // context에 blockNum, len 저장
    int blockNum = context.blockNum;

    uint64_t streamKey = 0;
    for (int i = 0; i < 8; ++i)
    {
        streamKey = (streamKey << 8) | (uint8_t)key[i];
    }
    uint64_t *roundKey = GenerateRoundKeys(streamKey); // *uint64_t OR uint64_t[16]

    InitialPermutation(inputBlock, blockNum);                          // +++ uint64_t 블록을 처리하기 위해 blockNum을 매개변수로 사용 (sizeof, strlen 등 사용불가) +++
    uint64_t *Feistel = FeistelRounds(inputBlock, roundKey, blockNum); // Feistel 함수, Feistel.cpp에서 정의되어 있다고 가정
    FinalPermutation(Feistel, blockNum);                               // 최종 순열 적용

    const char *result = BlocksToString(Feistel, blockNum, blockNum * 8); // blockNum * 8 = 원래 문자열 길이(패딩 포함), 복호화 시 원래 길이 사용(len)

    delete[] inputBlock;
    delete[] roundKey;
    delete[] Feistel;

    return result; // encrytion된 문자가 사용이 끝나면, 호출한 곳에서 result를 처리(반환)해야함
}

const char *DES_Decryption(const char *cipher, const char *key, DesContext &context)
{
    int blockNum = context.blockNum;
    int len = context.len;

    // 암호문 -> uint64_t 블록 배열로 변환
    uint64_t *blocks = new uint64_t[blockNum];
    for (int i = 0; i < blockNum; ++i)
    {
        uint64_t block = 0;
        for (int j = 0; j < 8; ++j)
        {
            block = (block << 8) | (uint8_t)cipher[i * 8 + j];
        }
        blocks[i] = block;
    }

    uint64_t streamKey = 0;
    for (int i = 0; i < 8; ++i)
    {
        streamKey = (streamKey << 8) | (uint8_t)key[i];
    }

    // 라운드 키 생성 및 역순 배열
    uint64_t *roundKey = GenerateRoundKeys(streamKey);
    uint64_t reversedKey[16];
    for (int i = 0; i < 16; ++i)
    {
        reversedKey[i] = roundKey[15 - i];
    }

    // 복호화 수행
    InitialPermutation(blocks, blockNum);
    uint64_t *Feistel = FeistelRounds(blocks, reversedKey, blockNum);
    FinalPermutation(Feistel, blockNum);

    // 결과 문자열로 변환 (패딩 제거 포함)
    const char *result = BlocksToString(Feistel, blockNum, len);

    delete[] blocks;
    delete[] roundKey;
    delete[] Feistel;

    return result; // 호출자가 delete[] 해줘야 함
}

uint64_t *StringToBlocks(const char *input, int &blockNum, int &len)
{
    len = strlen(input);      // 문자열 길이
    blockNum = (len + 7) / 8; // 나머지를 보정하기 위한 + 7

    uint64_t *blocks = new uint64_t[blockNum];

    for (int i = 0; i < blockNum; ++i)
    {
        uint64_t block_temp = 0;
        for (int j = 0; j < 8; ++j)
        {
            int index = i * 8 + j;
            uint8_t byte = (index < len) ? input[index] : 0x00; // 문자열 길이를 초과하면 0으로 채움(패딩)
            block_temp = (block_temp << 8) | byte;              // 왼쪽으로 시프트하고 바이트를 추가, big endian 방식
        }
        blocks[i] = block_temp; // 64bit x i 블록에 저장
    }

    return blocks; // 64비트 블록 배열 반환
}

const char *BlocksToString(uint64_t *blocks, int blockNum, int len)
{
    char *result = new char[len + 1];

    int origin = 0;
    for (int i = 0; i < blockNum && origin < len; ++i)
    {
        for (int j = 0; j < 8 && origin < len; ++j)
        {
            result[origin++] = (blocks[i] >> (8 * (7 - j))) & 0xFF;
        }
    }

    result[len] = '\0'; // 문자열 끝에 널 문자 추가
    return result;
}
