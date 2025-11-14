#pragma once
#include "AES_HMAC.hpp"

#include <openssl/evp.h>  // EVP_Encrypt*/Decrypt*, PBKDF2 등, EVP_MAX_BLOCK_LENGTH = 32
#include <openssl/rand.h> // RAND_bytes()
#include <openssl/hmac.h> // HMAC_* (또는 OpenSSL 3.x의 EVP_MAC 대체 가능)
#include <openssl/sha.h>  // SHA256 (직접 쓸 일은 적지만 종종 포함)

#include <cstring> // std::memcpy
#include <stdexcept>

void EtmCipher::DeriveKeys(const std::string &password, const unsigned char *salt, int salt_len,
                           int iterations, unsigned char enc_key[AES_KEY_LEN], unsigned char mac_key[AES_KEY_LEN])
{
    unsigned char tmp[AES_KEY_LEN * 2]; // 32*2, 256bit
    if (PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                          salt, salt_len,
                          iterations, EVP_sha256(),
                          sizeof(tmp), tmp) != 1) // 성공:1 실패: 0
        throw std::runtime_error("PBKDF2 failed");

    std::memcpy(enc_key, tmp, AES_KEY_LEN);               // 앞 32byte
    std::memcpy(mac_key, tmp + AES_KEY_LEN, AES_KEY_LEN); // 뒤 32byte
    OPENSSL_cleanse(tmp, sizeof(tmp));
}

std::vector<unsigned char> EtmCipher::Encrypt(const std::vector<unsigned char> &plaintext, const std::string &password, int iterations)
{
    std::vector<unsigned char> salt(SALT_LEN), iv(IV_LEN); // salt -> key 유도, iv -> AES 초기화 벡터
    if (RAND_bytes(salt.data(), SALT_LEN) != 1)            // 0: 실패, 1: 성공
        throw std::runtime_error("RAND_bytes salt failed");
    if (RAND_bytes(iv.data(), IV_LEN) != 1)
        throw std::runtime_error("RAND_bytes iv failed");

    unsigned char enc_key[AES_KEY_LEN], mac_key[AES_KEY_LEN]; // enc -> AES key, mac -> HMAC key
    DeriveKeys(password, salt.data(), SALT_LEN, iterations, enc_key, mac_key);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::bad_alloc(); // 메모리 할당 실패

    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH); // 패딩 때문에 여유있게 설정, 추후 UPDATE, FINAL에서 크기 조정
    int outl = 0, total = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, enc_key, iv.data()) != 1) // 암호화 시작
        throw std::runtime_error("EncryptInit failed");

    EVP_EncryptUpdate(ctx, ciphertext.data(), &outl, plaintext.data(), (int)plaintext.size()); // 실제 암호화된 바이트 수를 반환해야해서 &outl 사용, 위 선언한 크기보다 작을 수 있어서
    total += outl;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + total, &outl);
    total += outl; // 패딩을 처리한 마지막 바이트 수
    ciphertext.resize(total);
    EVP_CIPHER_CTX_free(ctx);

    unsigned char tag[TAG_LEN];                                  // 변조 검사를 위한 HMAC 태그
    HmacSha256(mac_key, AES_KEY_LEN, salt, iv, ciphertext, tag); // OUT: tag

    // [salt | iv | ciphertext | tag] 형태의 패킷 생성
    std::vector<unsigned char> packet;
    packet.reserve(SALT_LEN + IV_LEN + ciphertext.size() + TAG_LEN); // reserve로 메모리 용량 미리 할당, 한 번에 확보해서 성능 향상
    packet.insert(packet.end(), salt.begin(), salt.end());
    packet.insert(packet.end(), iv.begin(), iv.end());
    packet.insert(packet.end(), ciphertext.begin(), ciphertext.end());
    packet.insert(packet.end(), tag, tag + TAG_LEN);

    OPENSSL_cleanse(enc_key, sizeof(enc_key));
    OPENSSL_cleanse(mac_key, sizeof(mac_key));
    return packet; // RVO(Return Value Optimization) & Move Semantics(복사 생성자와 이동 생성자, 메모리 복사가 아닌 포인터 이동으로 성능 향상)
}

std::vector<unsigned char> EtmCipher::Decrypt(const std::vector<unsigned char> &packet, const std::string &password, int iterations)
{
    if (packet.size() < SALT_LEN + IV_LEN + TAG_LEN) // packet 최소 크기 검사
        throw std::invalid_argument("invalid packet size");

    const unsigned char *salt = packet.data(); // 각 요소의 시작 위치 포인터 지정
    const unsigned char *iv = packet.data() + SALT_LEN;
    const unsigned char *ct = packet.data() + SALT_LEN + IV_LEN;
    size_t ct_len = packet.size() - (SALT_LEN + IV_LEN + TAG_LEN);
    const unsigned char *tag = ct + ct_len;

    unsigned char enc_key[AES_KEY_LEN], mac_key[AES_KEY_LEN];
    DeriveKeys(password, salt, SALT_LEN, iterations, enc_key, mac_key); // *salt부터 [SALT_LEN] 바이트 만큼 사용

    std::vector<unsigned char> saltv(salt, salt + SALT_LEN); // vector 생성자, 포인터 범위로 vector 초기화
    std::vector<unsigned char> ivv(iv, iv + IV_LEN);
    std::vector<unsigned char> ctv(ct, ct + ct_len);

    unsigned char calc[TAG_LEN];                             // 새롭게 계산할 hmac 태그
    HmacSha256(mac_key, AES_KEY_LEN, saltv, ivv, ctv, calc); // out: calc
    if (CRYPTO_memcmp(calc, tag, TAG_LEN) != 0)              // CRYPTO_memcmp() : 안전한 메모리 비교함수, memcpy와 달리 항상 끝까지 비교해서 역추적 방지
        throw std::runtime_error("auth failed");

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::bad_alloc();

    std::vector<unsigned char> plaintext(ct_len + EVP_MAX_BLOCK_LENGTH); // 논리적으로는 plaintext size <= ciphertext size이지만, 패딩처리 및 안전을 위해 여유있게
    int outl = 0, total = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, enc_key, iv) != 1)
        throw std::runtime_error("DecryptInit failed");

    EVP_DecryptUpdate(ctx, plaintext.data(), &outl, ct, (int)ct_len);
    total += outl;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &outl) != 1)
        throw std::runtime_error("DecryptFinal failed");
    total += outl;
    plaintext.resize(total);
    EVP_CIPHER_CTX_free(ctx);

    OPENSSL_cleanse(enc_key, sizeof(enc_key));
    OPENSSL_cleanse(mac_key, sizeof(mac_key));
    return plaintext;
}

void EtmCipher::HmacSha256(const unsigned char *key, size_t key_len, const std::vector<unsigned char> &salt, const std::vector<unsigned char> &iv,
                           const std::vector<unsigned char> &ciphertext, unsigned char out_tag[TAG_LEN])
{
    unsigned int len = 0; // HMAC_Final()에서 길이 반환용
    HMAC_CTX *hctx = HMAC_CTX_new();
    if (!hctx)
        throw std::runtime_error("HMAC_CTX_new failed");

    HMAC_Init_ex(hctx, key, (int)key_len, EVP_sha256(), nullptr);
    HMAC_Update(hctx, salt.data(), salt.size());
    HMAC_Update(hctx, iv.data(), iv.size());
    HMAC_Update(hctx, ciphertext.data(), ciphertext.size());
    HMAC_Final(hctx, out_tag, &len);
    HMAC_CTX_free(hctx);
    if (len != TAG_LEN)
        throw std::runtime_error("HMAC length mismatch");
}
