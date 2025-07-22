#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

uint64_t *StringTo64BitBlocks(const char *Data, int &blockNum);
uint64_t StringTo64(const char *Data);
uint64_t *GenerateRoundKeys(uint64_t streamkey, uint64_t *roundKeys);
uint64_t FeistelRounds(uint64_t left, uint64_t right, uint64_t *roundKeys);
uint64_t InitialPermutation(uint64_t block);
uint64_t Finalpermutation(uint64_t block);

const char *DES_MAIN(const char *input, const char *key)
{
    int blockNum = 0;

    uint64_t *inputBlock = StringTo64BitBlocks(input, blockNum);
    uint64_t *keyBlock = StringTo64BitBlocks(key, blockNum);

    uint64_t streamKey = keyBlock[0]; // 키 블록에서 첫 번째 64비트 키 사용
    uint64_t roundKeys[16];
    GenerateRoundKeys(streamKey, roundKeys); // 키 생성 함수

    // 결과를 저장할 문자열
    char *result = new char[blockNum * 16 + 1];

    // 각 64비트 블록에 대해 DES 암호화 수행
    for (int i = 0; i < blockNum; ++i)
    {

        uint64_t ip = InitialPermutation(inputBlock[i]); // 초기 순열 적용

        uint32_t left = (ip >> 32) & 0xFFFFFFFF; // 왼쪽 32비트
        uint32_t right = ip & 0xFFFFFFFF;        // 오른쪽 32비트

        FeistelRounds(left, right, roundKeys); // 16회 라운드

        uint64_t merged = ((uint64_t)right << 32) | left; // 왼쪽과 오른쪽을 합침
        uint64_t fp = Finalpermutation(merged);           // 최종 순열 적용

        printf("%c ", fp); // 결과 출력
    }

    // result = (const char *)keyBlock;

    delete[] inputBlock, keyBlock; // 동적 할당된 메모리 해제

    return result;
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
