#include "KeySchedule.h"

uint64_t *GenerateRoundKeys(uint64_t streamKey)
{
    uint64_t *roundKeys = new uint64_t[16];

    // 1. PC-1: 64bit → 56bit
    uint64_t permutedKey = 0;
    for (int i = 0; i < 56; ++i)
    {
        int fromBit = 64 - PC1_Table[i];              // 인덱싱 방법이 반대로 있으므로 64에서 빼줌
        permutedKey <<= 1;                            // 값을 왼쪽으로 시프트
        permutedKey |= (streamKey >> fromBit) & 0x01; // 오른쪽으로 fromBit만큼 이동 -> 원하는 bit가 오른쪽 끝(LSB)에 오도록하고, 0x01로 AND 연산으로 비트 추출
    }

    // 2. Split into C and D (28-bit each)
    uint32_t C = (permutedKey >> 28) & 0x0FFFFFFF; // 상위 28비트(7X4bit) 추출
    uint32_t D = permutedKey & 0x0FFFFFFF;         // 하위 28비트

    // 3. 16 rounds
    for (int round = 0; round < 16; ++round)
    {
        // Left shift C and D
        int shift = LeftShifts[round];
        C = ((C << shift) | (C >> (28 - shift))) & 0x0FFFFFFF; // 순환 시프트를 구현, 28bit로 제한하기 위해 & 0x0FFFFFFF
        D = ((D << shift) | (D >> (28 - shift))) & 0x0FFFFFFF;

        // Combine C and D
        uint64_t CD = ((uint64_t)C << 28) | D;

        // 56bit → 48bit
        uint64_t roundKey = 0;
        for (int i = 0; i < 48; ++i)
        {
            int fromBit = 56 - PC2_Table[i];    // 인덱싱 방법이 반대로 있으므로 56에서 빼줌
            roundKey <<= 1;                     // 값을 왼쪽으로 시프트
            roundKey |= (CD >> fromBit) & 0x01; // 오른쪽으로 fromBit만큼 이동 -> 원하는 bit가 오른쪽 끝(LSB)에 오도록하고, 0x01로 AND 연산으로 비트 추출
        }

        roundKeys[round] = roundKey; // 48비트(uint64_t) 라운드 키 저장
    }

    return roundKeys;
}
