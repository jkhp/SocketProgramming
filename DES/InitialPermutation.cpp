#include "InitialPermutation.h"

void InitialPermutation(uint64_t *block, int blockNum)
{
    for (int i = 0; i < blockNum; ++i)
    {
        uint64_t original = block[i];
        uint64_t permute = 0;

        for (int j = 0; j < 64; ++j)
        {
            int fromBit = 64 - IP_Table[j]; // 1 based -> 0 based
            permute <<= 1;
            permute |= (original >> fromBit) & 0x01;
        }

        block[i] = permute;
    }
}
