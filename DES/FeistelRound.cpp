#include "FeistelRound.h"

// Feistel 함수: 32bit 입력 R, 48bit 라운드 키
uint32_t FeistelFunction(uint32_t R, uint64_t roundKey)
{
    // 1. E 확장 (32bit → 48bit)
    uint64_t expandedR = 0;
    for (int i = 0; i < 48; ++i)
    {
        int fromBit = 32 - E_TABLE[i]; // 1 based -> 0 based
        expandedR <<= 1;
        expandedR |= (R >> fromBit) & 0x01;
    }

    // 2. XOR with round key (48bit)
    uint64_t xored = expandedR ^ roundKey;

    // 3. S-box (48bit → 32bit)
    uint32_t sboxOutput = 0;
    for (int i = 0; i < 8; ++i)
    {
        // 6비트 추출
        uint8_t block = (xored >> (42 - 6 * i)) & 0x3F; // & 0011 1111, 48비트에서 6비트씩 추출 (42~47, 36~41, ... ,0~5)

        // S-box row, column 계산
        int row = ((block & 0x20) >> 4) | (block & 0x01); // 1st & 6th bits (0010 0000, 0000 0001)
        int col = (block >> 1) & 0x0F;                    // middle 4 bits (0000 1111)

        uint8_t sVal = S_BOX[i][row][col]; // 값 추출 (row = 0 ~ 3, col = 0 ~ 15)

        sboxOutput <<= 4;   // S-box는 4비트 출력이므로 왼쪽으로 4비트 시프트
        sboxOutput |= sVal; // S-box 값을 추가
    }

    // 4. P-박스 순열 (32bit → 32bit)
    uint32_t result = 0;
    for (int i = 0; i < 32; ++i)
    {
        int fromBit = 32 - P_TABLE[i]; // 1 based -> 0 based
        result <<= 1;
        result |= (sboxOutput >> fromBit) & 0x01;
    }

    return result;
}

uint64_t *FeistelRounds(uint64_t *blocks, uint64_t *roundKeys, int blockNum)
{
    uint64_t *result = new uint64_t[blockNum];

    // start of block loop
    for (int i = 0; i < blockNum; ++i)
    {
        uint64_t dataBlock = blocks[i];

        uint32_t L = (uint32_t)(dataBlock >> 32);
        uint32_t R = (uint32_t)(dataBlock & 0xFFFFFFFF);

        // start of 16 rounds
        for (int round = 0; round < 16; ++round)
        {
            uint32_t nextL = R;
            uint32_t nextR = L ^ FeistelFunction(R, roundKeys[round]);

            L = nextL;
            R = nextR;
        } // end of 16 rounds

        result[i] = ((uint64_t)R << 32) | L; // 마지막에 (uint32_t)L, R을 뒤집어서 조합
    } // end of block loop

    return result;
}
