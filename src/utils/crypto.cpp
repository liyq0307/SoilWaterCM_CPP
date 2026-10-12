#include "crypto.h"

#include <iomanip>
#include <random>
#include <sstream>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

namespace soilwatercm::utils
{
    std::string Crypto::bcryptHash(const std::string& password, int work_factor)
    {
        std::string salt = "$2a$" + std::to_string(work_factor) + "$";
        std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 61);

        for (int i = 0; i < 22; ++i)
        {
            salt.push_back(chars[dis(gen)]);
        }
        salt.push_back('$');

        unsigned char derived[32];
        const unsigned char* saltBytes = reinterpret_cast<const unsigned char*>(salt.c_str());
        PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), saltBytes, static_cast<int>(salt.size()),
                          work_factor * 1000, EVP_sha256(), 32, derived);

        std::ostringstream oss;
        oss << salt;
        for (int i = 0; i < 31; ++i)
        {
            int idx = derived[i % 32] % 62;
            oss << chars[idx];
        }

        return oss.str();
    }

    bool Crypto::bcryptVerify(const std::string& password, const std::string& hash)
    {
        if (hash.empty() || hash[0] != '$')
        {
            return false;
        }

        size_t dollarPos1 = hash.find('$', 1);
        if (dollarPos1 == std::string::npos)
            return false;

        size_t dollarPos2 = hash.find('$', dollarPos1 + 1);
        if (dollarPos2 == std::string::npos)
            return false;

        size_t dollarPos3 = hash.find('$', dollarPos2 + 1);
        if (dollarPos3 == std::string::npos)
            return false;

        std::string workFactorStr = hash.substr(dollarPos1 + 1, dollarPos2 - dollarPos1 - 1);
        int workFactor = 12;
        try
        {
            workFactor = std::stoi(workFactorStr);
        }
        catch (...)
        {
        }

        std::string salt = hash.substr(0, dollarPos3 + 1);

        unsigned char derived[32];
        const unsigned char* saltBytes = reinterpret_cast<const unsigned char*>(salt.c_str());
        PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), saltBytes, static_cast<int>(salt.size()),
                          workFactor * 1000, EVP_sha256(), 32, derived);

        std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::ostringstream oss;
        oss << salt;
        for (int i = 0; i < 31; ++i)
        {
            int idx = derived[i % 32] % 62;
            oss << chars[idx];
        }

        return oss.str() == hash;
    }

    std::string Crypto::md5(const std::string& input)
    {
        unsigned char digest[MD5_DIGEST_LENGTH];
#pragma warning(push)
#pragma warning(disable : 4996)
        MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), digest);
#pragma warning(pop)

        std::ostringstream oss;
        for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return oss.str();
    }

    std::string Crypto::sha256(const std::string& input)
    {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), digest);

        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return oss.str();
    }

    std::string Crypto::base64Encode(const std::string& input)
    {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve(((input.size() + 2) / 3) * 4);

        for (size_t i = 0; i < input.size(); i += 3)
        {
            unsigned char b1 = static_cast<unsigned char>(input[i]);
            unsigned char b2 = (i + 1 < input.size()) ? static_cast<unsigned char>(input[i + 1]) : 0;
            unsigned char b3 = (i + 2 < input.size()) ? static_cast<unsigned char>(input[i + 2]) : 0;

            result.push_back(chars[b1 >> 2]);
            result.push_back(chars[((b1 & 0x03) << 4) | (b2 >> 4)]);
            result.push_back((i + 1 < input.size()) ? chars[((b2 & 0x0f) << 2) | (b3 >> 6)] : '=');
            result.push_back((i + 2 < input.size()) ? chars[b3 & 0x3f] : '=');
        }

        return result;
    }

    std::string Crypto::base64Decode(const std::string& input)
    {
        static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string result;
        result.reserve(input.size() * 3 / 4);

        int val = 0, bits = -8;
        for (char c : input)
        {
            if (c == '=')
                break;
            size_t pos = chars.find(c);
            if (pos == std::string::npos)
                continue;
            val = (val << 6) + static_cast<int>(pos);
            bits += 6;
            if (bits >= 0)
            {
                result.push_back(static_cast<char>((val >> bits) & 0xff));
                bits -= 8;
            }
        }

        return result;
    }

    std::string Crypto::generateRandomString(size_t length)
    {
        static const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 61);

        std::string result;
        result.reserve(length);

        for (size_t i = 0; i < length; ++i)
        {
            result.push_back(chars[dis(gen)]);
        }

        return result;
    }

    std::string Crypto::generateUuid()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        ss << std::hex;

        for (int i = 0; i < 8; ++i)
            ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; ++i)
            ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; ++i)
            ss << dis(gen);
        ss << "-";
        ss << dis(gen) % 4 + 8;
        for (int i = 0; i < 3; ++i)
            ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; ++i)
            ss << dis(gen);

        return ss.str();
    }

    std::string Crypto::aesEncrypt(const std::string& plaintext, const std::string& key)
    {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            return plaintext;

        unsigned char iv[16];
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (int i = 0; i < 16; ++i)
            iv[i] = static_cast<unsigned char>(dis(gen));

        std::string result(reinterpret_cast<char*>(iv), 16);

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(key.c_str()),
                               iv) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return plaintext;
        }

        int outLen = 0;
        int totalLen = 0;
        std::vector<unsigned char> out(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));

        if (EVP_EncryptUpdate(ctx, out.data(), &outLen, reinterpret_cast<const unsigned char*>(plaintext.c_str()),
                              static_cast<int>(plaintext.size())) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return plaintext;
        }
        totalLen = outLen;

        if (EVP_EncryptFinal_ex(ctx, out.data() + totalLen, &outLen) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return plaintext;
        }
        totalLen += outLen;

        result.append(reinterpret_cast<char*>(out.data()), totalLen);
        EVP_CIPHER_CTX_free(ctx);

        return result;
    }

    std::string Crypto::aesDecrypt(const std::string& ciphertext, const std::string& key)
    {
        if (ciphertext.size() < 16)
            return ciphertext;

        const unsigned char* iv = reinterpret_cast<const unsigned char*>(ciphertext.c_str());
        std::string data = ciphertext.substr(16);

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            return ciphertext;

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(key.c_str()),
                               iv) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return ciphertext;
        }

        int outLen = 0;
        int totalLen = 0;
        std::vector<unsigned char> out(data.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));

        if (EVP_DecryptUpdate(ctx, out.data(), &outLen, reinterpret_cast<const unsigned char*>(data.c_str()),
                              static_cast<int>(data.size())) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return ciphertext;
        }
        totalLen = outLen;

        if (EVP_DecryptFinal_ex(ctx, out.data() + totalLen, &outLen) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return ciphertext;
        }
        totalLen += outLen;

        std::string result(reinterpret_cast<char*>(out.data()), totalLen);
        EVP_CIPHER_CTX_free(ctx);

        return result;
    }

}  // namespace soilwatercm::utils
