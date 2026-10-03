#include "../../include/brazier/vendor/Cryptography/AES.hpp"

using namespace brazier::crypto;

std::string AES256::generateKey() {
    return generateRandomBytes(KEY_SIZE);
}

std::string AES256::generateIV() {
    return generateRandomBytes(BLOCK_SIZE);
}

std::string AES256::generateSalt() {
    return generateRandomBytes(SALT_SIZE);
}

std::string AES256::deriveKeyFromPassword(const std::string& password, const std::string& salt, int iterations) {
    std::string derivedKey(KEY_SIZE, '\0');

    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(), (int)password.size(),
        reinterpret_cast<const unsigned char*>(salt.c_str()), (int)salt.size(),
        iterations,
        EVP_sha256(),
        KEY_SIZE,
        reinterpret_cast<unsigned char*>(&derivedKey[0])
    );

    if (result != 1) {
        throw std::runtime_error("PBKDF2 key derivation failed");
    }

    return derivedKey;
}

std::string AES256::encrypt(const std::string& plaintext, const std::string& key, const std::string& iv) {
    validateKey(key);
    validateIV(iv);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
        reinterpret_cast<const unsigned char*>(key.c_str()),
        reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize encryption");
    }

    std::string ciphertext(plaintext.size() + BLOCK_SIZE, '\0');
    int len = 0;
    int totalLen = 0;

    if (EVP_EncryptUpdate(ctx,
        reinterpret_cast<unsigned char*>(&ciphertext[0]), &len,
        reinterpret_cast<const unsigned char*>(plaintext.c_str()), (int)plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encryption update failed");
    }
    totalLen += len;

    if (EVP_EncryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(&ciphertext[totalLen]), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encryption finalization failed");
    }
    totalLen += len;

    ciphertext.resize(totalLen);
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext;
}

std::string AES256::decrypt(const std::string& ciphertext, const std::string& key, const std::string& iv) {
    validateKey(key);
    validateIV(iv);

    if (ciphertext.empty()) {
        return "";
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
        reinterpret_cast<const unsigned char*>(key.c_str()),
        reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    std::string plaintext(ciphertext.size() + BLOCK_SIZE, '\0');
    int len = 0;
    int totalLen = 0;

    if (EVP_DecryptUpdate(ctx,
        reinterpret_cast<unsigned char*>(&plaintext[0]), &len,
        reinterpret_cast<const unsigned char*>(ciphertext.c_str()), (int)ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption update failed");
    }
    totalLen += len;

    if (EVP_DecryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(&plaintext[totalLen]), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption finalization failed - wrong key or corrupted data");
    }
    totalLen += len;

    plaintext.resize(totalLen);
    EVP_CIPHER_CTX_free(ctx);

    return plaintext;
}

std::string AES256::encryptWithHmac(const std::string& plaintext,
    const std::string& key,
    const std::string& iv) {
    if (iv.size() != BLOCK_SIZE) {
        throw std::runtime_error("Invalid IV size");
    }
    std::string ciphertext = encrypt(plaintext, key, iv);
    std::string hmac = computeHmac(iv + ciphertext, key);
    return hmac + ciphertext;
}

std::string AES256::decryptWithHmac(const std::string& data,
    const std::string& key,
    const std::string& iv) {
    if (iv.size() != BLOCK_SIZE) {
        throw std::runtime_error("Invalid IV size");
    }
    if (data.size() < HMAC_SIZE) {
        throw std::runtime_error("Data is too short to contain HMAC");
    }

    std::string hmac = data.substr(0, HMAC_SIZE);
    std::string ciphertext = data.substr(HMAC_SIZE);

    std::string expectedHmac = computeHmac(iv + ciphertext, key);
    if (hmac != expectedHmac) {
        throw std::runtime_error("HMAC verification failed - message corrupted or tampered");
    }

    return decrypt(ciphertext, key, iv);
}
std::string AES256::encryptMessage(const std::string& message, const std::string& key) {
    std::string iv = generateIV();
    std::string ciphertext = encrypt(message, key, iv);
    return iv + ciphertext;
}

std::string AES256::decryptMessage(const std::string& encryptedData, const std::string& key) {
    if (encryptedData.size() < BLOCK_SIZE) {
        throw std::runtime_error("Encrypted data is too short");
    }

    std::string iv = encryptedData.substr(0, BLOCK_SIZE);
    std::string ciphertext = encryptedData.substr(BLOCK_SIZE);

    return decrypt(ciphertext, key, iv);
}

std::string AES256::toHex(const std::string& binary) {
    std::string hex;
    hex.reserve(binary.size() * 2);

    static const char* hexChars = "0123456789abcdef";
    for (unsigned char c : binary) {
        hex.push_back(hexChars[c >> 4]);
        hex.push_back(hexChars[c & 0x0F]);
    }

    return hex;
}

std::string AES256::fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Invalid hex string length");
    }

    std::string binary;
    binary.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        char high = hex[i];
        char low = hex[i + 1];

        unsigned char byte = 0;
        if (high >= '0' && high <= '9') byte = (high - '0') << 4;
        else if (high >= 'a' && high <= 'f') byte = (high - 'a' + 10) << 4;
        else if (high >= 'A' && high <= 'F') byte = (high - 'A' + 10) << 4;
        else throw std::runtime_error("Invalid hex character");

        if (low >= '0' && low <= '9') byte |= (low - '0');
        else if (low >= 'a' && low <= 'f') byte |= (low - 'a' + 10);
        else if (low >= 'A' && low <= 'F') byte |= (low - 'A' + 10);
        else throw std::runtime_error("Invalid hex character");

        binary.push_back(static_cast<char>(byte));
    }

    return binary;
}

bool AES256::validateKey(const std::string& key) {
    if (key.size() != KEY_SIZE) {
        throw std::runtime_error("Invalid key size: must be 32 bytes (256 bits)");
    }
    return true;
}

bool AES256::validateIV(const std::string& iv) {
    if (iv.size() != BLOCK_SIZE) {
        throw std::runtime_error("Invalid IV size: must be 16 bytes (128 bits)");
    }
    return true;
}

std::string AES256::generateRandomBytes(int size) {
    std::string bytes(size, '\0');

    if (RAND_bytes(reinterpret_cast<unsigned char*>(&bytes[0]), size) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }

    return bytes;
}

std::string AES256::computeHmac(const std::string& data, const std::string& key) {
    std::string hmac(HMAC_SIZE, '\0');

    unsigned int len = 0;
    HMAC(EVP_sha256(),
        reinterpret_cast<const unsigned char*>(key.c_str()), (int)key.size(),
        reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
        reinterpret_cast<unsigned char*>(&hmac[0]), &len);

    if (len != HMAC_SIZE) {
        throw std::runtime_error("HMAC computation failed");
    }

    return hmac;
}