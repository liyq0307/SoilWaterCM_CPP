#pragma once

#include <string>
#include <vector>

namespace soilwatercm::utils
{
    /**
     * @brief 加密与编码工具 (基于 OpenSSL)
     */
    class Crypto
    {
    public:
        /**
         * @brief 口令散列
         *
         * 输出 $2a$ 格式的 bcrypt 风格串, 实际用 PBKDF2-HMAC-SHA256 派生。
         * @param password 明文口令
         * @param work_factor 工作因子, 迭代次数为其 1000 倍
         * @return 散列串 (含盐)
         */
        static std::string bcryptHash(const std::string& password, int work_factor = 12);

        /// @brief 校验口令与散列串是否匹配 (从串中取盐和因子重新派生)
        static bool bcryptVerify(const std::string& password, const std::string& hash);

        /// @brief MD5 摘要 (十六进制串)
        static std::string md5(const std::string& input);

        /// @brief SHA-256 摘要 (十六进制串)
        static std::string sha256(const std::string& input);

        /// @brief Base64 编码
        static std::string base64Encode(const std::string& input);

        /// @brief Base64 解码
        static std::string base64Decode(const std::string& input);

        /// @brief 生成指定长度的随机字母数字串
        static std::string generateRandomString(size_t length);

        /// @brief 生成 UUID v4
        static std::string generateUuid();

        /**
         * @brief AES-256-CBC 加密
         *
         * 随机 IV 拼在密文前面一起返回; 加密失败返回原文。
         * @param plaintext 明文
         * @param key 密钥 (须 32 字节)
         * @return IV + 密文
         */
        static std::string aesEncrypt(const std::string& plaintext, const std::string& key);

        /// @brief AES-256-CBC 解密, 输入为 aesEncrypt 的 IV+密文
        static std::string aesDecrypt(const std::string& ciphertext, const std::string& key);
    };

}  // namespace soilwatercm::utils
