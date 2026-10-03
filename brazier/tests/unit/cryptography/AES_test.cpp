/*
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Gleb Safyannikov
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of brazier.
 *
 * brazier is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * brazier is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with brazier; if not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include <string>
#include <stdexcept>
#include "../../../include/brazier/vendor/Cryptography/AES.hpp"

using namespace brazier::crypto;

class AES256TestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        key_ = AES256::generateKey();
        iv_ = AES256::generateIV();
    }

    void TearDown() override {

    }

    std::string key_;
    std::string iv_;
};

TEST_F(AES256TestFixture, GenerateKey) {
    std::string key = AES256::generateKey();
    EXPECT_EQ(key.size(), AES256::KEY_SIZE);

    bool allZero = true;
    for (unsigned char c : key) {
        if (c != 0) { allZero = false; break; }
    }
    EXPECT_FALSE(allZero);

    std::string key2 = AES256::generateKey();
    EXPECT_NE(key, key2);
}

TEST_F(AES256TestFixture, GenerateIV) {
    std::string iv = AES256::generateIV();
    EXPECT_EQ(iv.size(), AES256::BLOCK_SIZE);

    bool allZero = true;
    for (unsigned char c : iv) {
        if (c != 0) { allZero = false; break; }
    }
    EXPECT_FALSE(allZero);

    std::string iv2 = AES256::generateIV();
    EXPECT_NE(iv, iv2);
}

TEST_F(AES256TestFixture, GenerateSalt) {
    std::string salt = AES256::generateSalt();
    EXPECT_EQ(salt.size(), AES256::SALT_SIZE);

    std::string salt2 = AES256::generateSalt();
    EXPECT_NE(salt, salt2);
}


TEST_F(AES256TestFixture, DeriveKeyFromPassword) {
    const std::string password = "mySecretPassword";
    const std::string salt = AES256::generateSalt();
    const int iterations = 10000;

    std::string key1 = AES256::deriveKeyFromPassword(password, salt, iterations);
    EXPECT_EQ(key1.size(), AES256::KEY_SIZE);

    std::string key2 = AES256::deriveKeyFromPassword(password, salt, iterations);
    EXPECT_EQ(key1, key2);

    std::string salt2 = AES256::generateSalt();
    std::string key3 = AES256::deriveKeyFromPassword(password, salt2, iterations);
    EXPECT_NE(key1, key3);

    std::string key4 = AES256::deriveKeyFromPassword("otherPassword", salt, iterations);
    EXPECT_NE(key1, key4);

    std::string key5 = AES256::deriveKeyFromPassword(password, salt, iterations + 1000);
    EXPECT_NE(key1, key5);

    EXPECT_NO_THROW(AES256::deriveKeyFromPassword("", salt, iterations));
    EXPECT_NO_THROW(AES256::deriveKeyFromPassword(password, "", iterations));
}

TEST_F(AES256TestFixture, ValidateKey) {
    std::string validKey(AES256::KEY_SIZE, 'x');
    EXPECT_TRUE(AES256::validateKey(validKey));

    std::string shortKey(AES256::KEY_SIZE - 1, 'x');
    EXPECT_THROW(AES256::validateKey(shortKey), std::runtime_error);

    std::string longKey(AES256::KEY_SIZE + 1, 'x');
    EXPECT_THROW(AES256::validateKey(longKey), std::runtime_error);
}

TEST_F(AES256TestFixture, ValidateIV) {
    std::string validIV(AES256::BLOCK_SIZE, 'x');
    EXPECT_TRUE(AES256::validateIV(validIV));

    std::string shortIV(AES256::BLOCK_SIZE - 1, 'x');
    EXPECT_THROW(AES256::validateIV(shortIV), std::runtime_error);

    std::string longIV(AES256::BLOCK_SIZE + 1, 'x');
    EXPECT_THROW(AES256::validateIV(longIV), std::runtime_error);
}

TEST_F(AES256TestFixture, EncryptDecrypt) {
    const std::string plaintext = "Hello, world!";

    std::string ciphertext = AES256::encrypt(plaintext, key_, iv_);
    EXPECT_NE(ciphertext, plaintext);
    EXPECT_GT(ciphertext.size(), plaintext.size()); 

    std::string decrypted = AES256::decrypt(ciphertext, key_, iv_);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(AES256TestFixture, EncryptDecryptEmpty) {
    const std::string plaintext;

    std::string ciphertext = AES256::encrypt(plaintext, key_, iv_);
    EXPECT_FALSE(ciphertext.empty());

    std::string decrypted = AES256::decrypt(ciphertext, key_, iv_);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(AES256TestFixture, EncryptDecryptLong) {
    const std::string plaintext(10000, 'A');

    std::string ciphertext = AES256::encrypt(plaintext, key_, iv_);
    std::string decrypted = AES256::decrypt(ciphertext, key_, iv_);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(AES256TestFixture, EncryptWithInvalidKeyThrows) {
    std::string invalidKey(16, 'x'); 
    EXPECT_THROW(AES256::encrypt("test", invalidKey, iv_), std::runtime_error);
}

TEST_F(AES256TestFixture, EncryptWithInvalidIVThrows) {
    std::string invalidIV(8, 'x'); 
    EXPECT_THROW(AES256::encrypt("test", key_, invalidIV), std::runtime_error);
}

TEST_F(AES256TestFixture, DecryptWithWrongKeyThrows) {
    std::string plaintext = "secret";
    std::string wrongKey = AES256::generateKey();

    std::string ciphertext = AES256::encrypt(plaintext, key_, iv_);
    EXPECT_THROW(AES256::decrypt(ciphertext, wrongKey, iv_), std::runtime_error);
}

TEST_F(AES256TestFixture, DecryptWithWrongIVThrows) {
    std::string plaintext = "secret";
    std::string wrongIV = AES256::generateIV();

    std::string ciphertext = AES256::encrypt(plaintext, key_, iv_);
    EXPECT_THROW(AES256::decrypt(ciphertext, key_, wrongIV), std::runtime_error);
}

TEST_F(AES256TestFixture, DifferentIVsGiveDifferentCiphertext) {
    const std::string plaintext = "Same message";
    std::string iv1 = AES256::generateIV();
    std::string iv2 = AES256::generateIV();

    std::string c1 = AES256::encrypt(plaintext, key_, iv1);
    std::string c2 = AES256::encrypt(plaintext, key_, iv2);
    EXPECT_NE(c1, c2);
}

TEST_F(AES256TestFixture, EncryptWithHmac) {
    const std::string plaintext = "Secret message";

    std::string encryptedWithHmac = AES256::encryptWithHmac(plaintext, key_, iv_);
    EXPECT_GT(encryptedWithHmac.size(), plaintext.size() + AES256::HMAC_SIZE);

    std::string decrypted = AES256::decryptWithHmac(encryptedWithHmac, key_, iv_);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(AES256TestFixture, EncryptWithHmacEmpty) {
    const std::string plaintext;

    std::string encrypted = AES256::encryptWithHmac(plaintext, key_, iv_);
    EXPECT_GT(encrypted.size(), AES256::HMAC_SIZE);

    std::string decrypted = AES256::decryptWithHmac(encrypted, key_, iv_);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(AES256TestFixture, EncryptWithHmacInvalidKeyThrows) {
    std::string invalidKey(16, 'x');
    EXPECT_THROW(AES256::encryptWithHmac("test", invalidKey, iv_), std::runtime_error);
}

TEST_F(AES256TestFixture, EncryptWithHmacInvalidIVThrows) {
    std::string invalidIV(8, 'x');
    EXPECT_THROW(AES256::encryptWithHmac("test", key_, invalidIV), std::runtime_error);
}

TEST_F(AES256TestFixture, HmacTamperedThrows) {
    const std::string plaintext = "Secret";

    std::string encrypted = AES256::encryptWithHmac(plaintext, key_, iv_);
    if (encrypted.size() > AES256::HMAC_SIZE + 1) {
        encrypted[AES256::HMAC_SIZE] ^= 0x01;
        EXPECT_THROW(AES256::decryptWithHmac(encrypted, key_, iv_), std::runtime_error);
    }

    std::string encrypted2 = AES256::encryptWithHmac(plaintext, key_, iv_);
    encrypted2[0] ^= 0x01;
    EXPECT_THROW(AES256::decryptWithHmac(encrypted2, key_, iv_), std::runtime_error);
}

TEST_F(AES256TestFixture, DecryptWithHmacTooShortThrows) {
    std::string shortData(AES256::HMAC_SIZE - 1, 'x');
    EXPECT_THROW(AES256::decryptWithHmac(shortData, key_, iv_), std::runtime_error);
}

TEST_F(AES256TestFixture, EncryptMessageDecryptMessage) {
    const std::string message = "Hello, World!";

    std::string encrypted = AES256::encryptMessage(message, key_);
    EXPECT_GT(encrypted.size(), message.size() + AES256::BLOCK_SIZE);

    std::string decrypted = AES256::decryptMessage(encrypted, key_);
    EXPECT_EQ(decrypted, message);
}

TEST_F(AES256TestFixture, EncryptMessageEmpty) {
    const std::string message;

    std::string encrypted = AES256::encryptMessage(message, key_);
    EXPECT_GE(encrypted.size(), AES256::BLOCK_SIZE);

    std::string decrypted = AES256::decryptMessage(encrypted, key_);
    EXPECT_EQ(decrypted, message);
}

TEST_F(AES256TestFixture, EncryptMessageInvalidKeyThrows) {
    std::string invalidKey(16, 'x');
    EXPECT_THROW(AES256::encryptMessage("test", invalidKey), std::runtime_error);
}

TEST_F(AES256TestFixture, DecryptMessageWrongKeyThrows) {
    std::string message = "Hello";
    std::string wrongKey = AES256::generateKey();

    std::string encrypted = AES256::encryptMessage(message, key_);
    EXPECT_THROW(AES256::decryptMessage(encrypted, wrongKey), std::runtime_error);
}

TEST_F(AES256TestFixture, DecryptMessageTooShortThrows) {
    std::string shortData(AES256::BLOCK_SIZE - 1, 'x');
    EXPECT_THROW(AES256::decryptMessage(shortData, key_), std::runtime_error);
}

TEST_F(AES256TestFixture, ToHexFromHex) {
    const std::string binary = "Hello";
    std::string hex = AES256::toHex(binary);
    EXPECT_EQ(hex, "48656c6c6f");

    std::string restored = AES256::fromHex(hex);
    EXPECT_EQ(restored, binary);

    std::string random = AES256::generateKey();
    std::string hex2 = AES256::toHex(random);
    std::string restored2 = AES256::fromHex(hex2);
    EXPECT_EQ(restored2, random);
}

TEST_F(AES256TestFixture, ToHexFromHexWithNullByte) {
    const std::string binary = std::string("A\0B", 3);
    std::string hex = AES256::toHex(binary);
    EXPECT_EQ(hex, "410042");
    std::string restored = AES256::fromHex(hex);
    EXPECT_EQ(restored, binary);
}

TEST_F(AES256TestFixture, FromHexInvalidLength) {
    try {
        AES256::fromHex("gg");
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error&) {
        SUCCEED();
    }
    catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST_F(AES256TestFixture, FromHexInvalidChars) {
    EXPECT_THROW(AES256::fromHex("gg"), std::runtime_error);
    EXPECT_THROW(AES256::fromHex("12g3"), std::runtime_error);
    EXPECT_THROW(AES256::fromHex("ABCDEFGH"), std::runtime_error); 
    EXPECT_NO_THROW(AES256::fromHex("aBcD")); 
}

TEST_F(AES256TestFixture, DifferentKeysDifferentCiphertext) {
    const std::string plaintext = "Same message";
    std::string key2 = AES256::generateKey();

    std::string c1 = AES256::encrypt(plaintext, key_, iv_);
    std::string c2 = AES256::encrypt(plaintext, key2, iv_);
    EXPECT_NE(c1, c2);
}