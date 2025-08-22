#include <cstring>
#include <cstdint>

#include "InitialPermutation.h"
#include "FeistelRound.h"
#include "FinalPermutation.h"
#include "KeySchedule.h"

uint64_t streamKey; // 전역으로 사용하면 멀티스레딩, 재사용시 문제 발생 가능성 -> 전역변수 사용 안하는 것으로, 호출자 측에서 관리 (구조체 또는 클래스)
uint64_t *roundKey;

// const char* 형태로 받아서 암호화된 uint64_t를 반환하는 함수 ---> encryption, decryption 매개변수와 반환형 변경 필요 ()
uint64_t *DES_Encryption(const char *input)
{
    int blockNum = 0;

    uint64_t *inputBlock = StringTo64BitBlocks(input, blockNum);
    streamKey = inputBlock[0];               // inputBlock의 첫 번째 64비트를 키로 사용
    roundKey = GenerateRoundKeys(streamKey); // 키 생성 함수, KeySchedule.cpp에서 정의되어 있다고 가정

    InitialPermutation(inputBlock, blockNum);                         // +++ uint64_t 블록을 처리하기 위해 blockNum을 매개변수로 사용 (sizeof, strlen 등 사용불가) +++
    uint64_t *result = FeistelRounds(inputBlock, roundKey, blockNum); // Feistel 함수, Feistel.cpp에서 정의되어 있다고 가정
    FinalPermutation(result, blockNum);                               // 최종 순열 적용

    delete[] inputBlock;
    // delete[] roundKey;

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

const char *DES_Decryption(uint64_t *encryptedBlocks, int blockNum)
{
    uint64_t reverseKey[16];
    for (int i = 0; i < 16; ++i)
        reverseKey[i] = roundKey[15 - i]; // 역순으로 키를 저장

    InitialPermutation(encryptedBlocks, blockNum);                                     // 초기 순열 적용
    uint64_t *decryptionBlocks = FeistelRounds(encryptedBlocks, reverseKey, blockNum); // 역순으로 라운드 키를 사용하여 Feistel 함수 적용
    FinalPermutation(decryptionBlocks, blockNum);                                      // 최종 순열 적용

    const char *result = BlocksToString(decryptionBlocks, blockNum);

    delete[] encryptedBlocks;
    delete[] roundKey;

    return result; // 복호화된 결과 반환, 사용이 끝나면 호출한 곳에서 result를 처리(반환)해야함
}

const char *BlocksToString(uint64_t *Data, int blockNum)
{
    char *result = new char[blockNum * 8 + 1];

    for (int i = 0; i < blockNum; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            result[i * 8 + (7 - j)] = (Data[i] >> (j * 8)) & 0xFF; // big endian 방식으로 변환?
        }
    }

    result[blockNum * 8] = '\0'; // 문자열 끝에 널 문자 추가
    return result;
}
