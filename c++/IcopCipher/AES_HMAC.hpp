#pragma once
#include <vector>
#include <string>

// ETM(Encrypt-then-MAC) 방식의 AES-256-CBC + HMAC-SHA256 구현
class EtmCipher
{
public:
    static constexpr int SALT_LEN = 16; // class 내 상수 선언 ex) EtmCipher::SALT_LEN
    static constexpr int IV_LEN = 16;
    static constexpr int TAG_LEN = 32;
    static constexpr int AES_KEY_LEN = 32;

    // AES-256-CBC + HMAC-SHA256
    // plaintext => std::String(사용자 입력) -> std::vector<unsigned char>(내부처리, 문자 아닌 바이트데이터)
    static std::vector<unsigned char> Encrypt(const std::vector<unsigned char> &plaintext, const std::string &password, int iterations); // iterations는 100k ~ 300k

    static std::vector<unsigned char> Decrypt(const std::vector<unsigned char> &packet, const std::string &password, int iterations);

private:
    // PKCS5_PBKDF2_HMAC, 키 유도 함수(반복 hash) -> mac_key, enc_key
    static void DeriveKeys(const std::string &password, const unsigned char *salt, int salt_len, int iterations,
                           unsigned char enc_key[AES_KEY_LEN], unsigned char mac_key[AES_KEY_LEN]); // enc(32) + mac(32) = 64byte

    // AES-CBC는 무결성 검증 x, hmac 검증으로 'tag 값'과 '직접 계산한 tag 값' 일치 확인 -> hash는 단방향이니까.
    static void HmacSha256(const unsigned char *key, size_t key_len, const std::vector<unsigned char> &salt,
                           const std::vector<unsigned char> &iv, const std::vector<unsigned char> &ciphertext,
                           unsigned char out_tag[TAG_LEN]);
};
