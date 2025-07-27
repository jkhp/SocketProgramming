#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

uint64_t *StringTo64BitBlocks(const char *Data, int &blockNum);
uint64_t *GenerateRoundKeys(uint64_t streamkey);
uint64_t *FeistelRounds(uint64_t *streamText, uint64_t *roundKeys);
void InitialPermutation(uint64_t *block, int blockNum);
void Finalpermutation(uint64_t *block, int blockNum);

// const char* 형태로 받아서 암호화된 uint64_t를 반환하는 함수
uint64_t *DES_Encryption(const char *input)
{
    int blockNum = 0;
    // uint64_t *result = new uint64_t[blockNum * 16 + 1]; // 암호문을 저장할 문자열

    uint64_t *inputBlock = StringTo64BitBlocks(input, blockNum);
    uint64_t streamKey = inputBlock[0];                // inputBlock의 첫 번째 64비트를 키로 사용
    uint64_t *roundKey = GenerateRoundKeys(streamKey); // 키 생성 함수, KeySchedule.cpp에서 정의되어 있다고 가정

    InitialPermutation(inputBlock, blockNum);               // +++ uint64_t 블록을 처리하기 위해 blockNum을 사용 (sizeof, strlen 등 사용불가) +++
    uint64_t *result = FeistelRounds(inputBlock, roundKey); // Feistel 함수, Feistel.cpp에서 정의되어 있다고 가정
    Finalpermutation(result, blockNum);                     // 최종 순열 적용

    delete[] inputBlock;
    delete[] roundKey;

    return result; // encrytion된 문자가 사용이 끝나면, 호출한 곳에서 result를 처리(반환)해야함
}

uint64_t *StringTo64BitBlocks(const char *Data, int &blockNum)
{
    int len = strlen(Data);   // 문자열 길이
    blockNum = (len + 7) / 8; // 나머지를 보정하기 위한 + 7

    uint64_t *blocks = new uint64_t[blockNum];

    for (int i = 0; i < blockNum; ++i)
    {
        uint64_t block_temp = 0;
        for (int j = 0; j < 8; ++j)
        {
            int index = i * 8 + j;
            uint8_t byte = (index < len) ? Data[index] : 0x00; // 문자열 길이를 초과하면 0으로 채움(패딩)
            block_temp = (block_temp << 8) | byte;             // 왼쪽으로 시프트하고 바이트를 추가, big endian 방식
        }
        blocks[i] = block_temp; // 64bit x i 블록에 저장
    }

    return blocks; // 64비트 블록 배열 반환
}
