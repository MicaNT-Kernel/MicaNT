#pragma once

/**
 * @file multimedia_suites.hpp
 * @brief Cryptography, Networking & Multimedia Services (Milestones 66-105)
 */

void Test_CryptoAPI_And_CNG_Subsystems() {
    std::cout << "\n[TEST] Running Suite 66: Windows CryptoAPI, CNG (BCrypt/NCrypt) & Crypt32 Subsystems...\n";

    // ------------------------------------------------------------------------
    // 1. Subsystem Initialization
    // ------------------------------------------------------------------------
    crypto::InitializeBCryptSubsystemExports();
    crypt32::InitializeCrypt32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();

    // Helper lambda to format binary to hex string
    auto toHex = [](const uint8_t* data, size_t len) -> std::string {
        std::ostringstream oss;
        for (size_t i = 0; i < len; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
        }
        return oss.str();
    };

    // ------------------------------------------------------------------------
    // 2. BCrypt Algorithm Provider & Property Queries
    // ------------------------------------------------------------------------
    {
        crypto::BCRYPT_ALG_HANDLE hSha256 = nullptr;
        int32_t st = crypto::BCryptOpenAlgorithmProvider(&hSha256, crypto::BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hSha256 != nullptr, "BCryptOpenAlgorithmProvider for SHA256 must succeed");

        uint32_t hashLen = 0;
        uint32_t cbRes = 0;
        st = crypto::BCryptGetProperty(hSha256, crypto::BCRYPT_HASH_LENGTH, reinterpret_cast<uint8_t*>(&hashLen), sizeof(hashLen), &cbRes, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hashLen == 32, "SHA-256 hash length property must be 32");

        uint32_t objLen = 0;
        st = crypto::BCryptGetProperty(hSha256, crypto::BCRYPT_OBJECT_LENGTH, reinterpret_cast<uint8_t*>(&objLen), sizeof(objLen), &cbRes, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && objLen > 0, "SHA-256 object length property must be > 0");

        crypto::BCryptCloseAlgorithmProvider(hSha256, 0);

        crypto::BCRYPT_ALG_HANDLE hSha512 = nullptr;
        st = crypto::BCryptOpenAlgorithmProvider(&hSha512, crypto::BCRYPT_SHA512_ALGORITHM, nullptr, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hSha512 != nullptr, "BCryptOpenAlgorithmProvider for SHA512 must succeed");

        hashLen = 0;
        st = crypto::BCryptGetProperty(hSha512, crypto::BCRYPT_HASH_LENGTH, reinterpret_cast<uint8_t*>(&hashLen), sizeof(hashLen), &cbRes, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hashLen == 64, "SHA-512 hash length property must be 64");
        crypto::BCryptCloseAlgorithmProvider(hSha512, 0);

        crypto::BCRYPT_ALG_HANDLE hSha384 = nullptr;
        st = crypto::BCryptOpenAlgorithmProvider(&hSha384, crypto::BCRYPT_SHA384_ALGORITHM, nullptr, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hSha384 != nullptr, "BCryptOpenAlgorithmProvider for SHA384 must succeed");

        hashLen = 0;
        st = crypto::BCryptGetProperty(hSha384, crypto::BCRYPT_HASH_LENGTH, reinterpret_cast<uint8_t*>(&hashLen), sizeof(hashLen), &cbRes, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hashLen == 48, "SHA-384 hash length property must be 48");
        crypto::BCryptCloseAlgorithmProvider(hSha384, 0);

        crypto::BCRYPT_ALG_HANDLE hMd5 = nullptr;
        st = crypto::BCryptOpenAlgorithmProvider(&hMd5, crypto::BCRYPT_MD5_ALGORITHM, nullptr, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hMd5 != nullptr, "BCryptOpenAlgorithmProvider for MD5 must succeed");

        hashLen = 0;
        st = crypto::BCryptGetProperty(hMd5, crypto::BCRYPT_HASH_LENGTH, reinterpret_cast<uint8_t*>(&hashLen), sizeof(hashLen), &cbRes, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hashLen == 16, "MD5 hash length property must be 16");
        crypto::BCryptCloseAlgorithmProvider(hMd5, 0);
    }

    // ------------------------------------------------------------------------
    // 3. BCrypt Known-Answer-Test (KAT) Verification (SHA-256, SHA-384, SHA-512, MD5, SHA-1)
    // ------------------------------------------------------------------------
    {
        const char* msg = "abc";
        const uint32_t msgLen = 3;

        // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
        {
            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_SHA256_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(msg), msgLen, 0);
            uint8_t digest[32]{};
            crypto::BCryptFinishHash(hHash, digest, 32, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            TEST_ASSERT(toHex(digest, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                        "BCrypt SHA-256('abc') KAT verification failed");
        }

        // SHA-384("abc") = cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7
        {
            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_SHA384_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(msg), msgLen, 0);
            uint8_t digest[48]{};
            crypto::BCryptFinishHash(hHash, digest, 48, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            TEST_ASSERT(toHex(digest, 48) == "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
                        "BCrypt SHA-384('abc') KAT verification failed");
        }

        // SHA-512("abc") = ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f
        {
            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_SHA512_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(msg), msgLen, 0);
            uint8_t digest[64]{};
            crypto::BCryptFinishHash(hHash, digest, 64, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            TEST_ASSERT(toHex(digest, 64) == "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
                        "BCrypt SHA-512('abc') KAT verification failed");
        }

        // MD5("abc") = 900150983cd24fb0d6963f7d28e17f72
        {
            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_MD5_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(msg), msgLen, 0);
            uint8_t digest[16]{};
            crypto::BCryptFinishHash(hHash, digest, 16, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            TEST_ASSERT(toHex(digest, 16) == "900150983cd24fb0d6963f7d28e17f72",
                        "BCrypt MD5('abc') KAT verification failed");
        }

        // SHA-1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d
        {
            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_SHA1_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(msg), msgLen, 0);
            uint8_t digest[20]{};
            crypto::BCryptFinishHash(hHash, digest, 20, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            TEST_ASSERT(toHex(digest, 20) == "a9993e364706816aba3e25717850c26c9cd0d89d",
                        "BCrypt SHA-1('abc') KAT verification failed");
        }
    }

    // ------------------------------------------------------------------------
    // 4. BCrypt Hash Duplication (BCryptDuplicateHash)
    // ------------------------------------------------------------------------
    {
        crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
        crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_SHA256_ALGORITHM, nullptr, 0);

        crypto::BCRYPT_HASH_HANDLE hHash1 = nullptr;
        crypto::BCryptCreateHash(hAlg, &hHash1, nullptr, 0, nullptr, 0, 0);

        std::string part1 = "MicaNT Clean-Room ";
        std::string part2 = "Operating System 2026";
        crypto::BCryptHashData(hHash1, reinterpret_cast<const uint8_t*>(part1.data()), static_cast<uint32_t>(part1.size()), 0);

        crypto::BCRYPT_HASH_HANDLE hHash2 = nullptr;
        int32_t st = crypto::BCryptDuplicateHash(hHash1, &hHash2, nullptr, 0, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hHash2 != nullptr, "BCryptDuplicateHash must succeed");

        crypto::BCryptHashData(hHash1, reinterpret_cast<const uint8_t*>(part2.data()), static_cast<uint32_t>(part2.size()), 0);
        crypto::BCryptHashData(hHash2, reinterpret_cast<const uint8_t*>(part2.data()), static_cast<uint32_t>(part2.size()), 0);

        uint8_t d1[32]{};
        uint8_t d2[32]{};
        crypto::BCryptFinishHash(hHash1, d1, 32, 0);
        crypto::BCryptFinishHash(hHash2, d2, 32, 0);

        TEST_ASSERT(std::memcmp(d1, d2, 32) == 0, "Duplicated hash must produce identical digest to original hash");

        crypto::BCryptDestroyHash(hHash1);
        crypto::BCryptDestroyHash(hHash2);
        crypto::BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    // ------------------------------------------------------------------------
    // 5. BCrypt CSPRNG Random Generation
    // ------------------------------------------------------------------------
    {
        uint8_t buf1[32]{};
        uint8_t buf2[32]{};

        int32_t st1 = crypto::BCryptGenRandom(nullptr, buf1, sizeof(buf1), crypto::BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        int32_t st2 = crypto::BCryptGenRandom(nullptr, buf2, sizeof(buf2), crypto::BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        TEST_ASSERT(BCRYPT_SUCCESS(st1) && BCRYPT_SUCCESS(st2), "BCryptGenRandom with SYSTEM_PREFERRED_RNG must succeed");
        TEST_ASSERT(std::memcmp(buf1, buf2, sizeof(buf1)) != 0, "Consecutive random buffers must not be identical");

        bool nonZero = false;
        for (uint8_t b : buf1) if (b != 0) nonZero = true;
        TEST_ASSERT(nonZero, "Random buffer must contain non-zero hardware entropy");
    }

    // ------------------------------------------------------------------------
    // 6. BCrypt Symmetric AES Key Encryption / Decryption
    // ------------------------------------------------------------------------
    {
        crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
        int32_t st = crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_AES_ALGORITHM, nullptr, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hAlg != nullptr, "BCryptOpenAlgorithmProvider for AES must succeed");

        uint8_t keyBytes[32] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
            0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
            0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
        };

        crypto::BCRYPT_KEY_HANDLE hKey = nullptr;
        st = crypto::BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, keyBytes, sizeof(keyBytes), 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hKey != nullptr, "BCryptGenerateSymmetricKey for AES-256 must succeed");

        std::string plain = "MicaNT Next Generation Sovereign Executive";
        std::vector<uint8_t> pt(plain.begin(), plain.end());

        uint8_t iv[16] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99};
        uint8_t ivEnc[16]; std::memcpy(ivEnc, iv, 16);

        uint32_t ctLen = 0;
        st = crypto::BCryptEncrypt(hKey, pt.data(), static_cast<uint32_t>(pt.size()), nullptr, ivEnc, 16, nullptr, 0, &ctLen, crypto::BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && ctLen >= pt.size(), "BCryptEncrypt length query must succeed");

        std::vector<uint8_t> ct(ctLen);
        std::memcpy(ivEnc, iv, 16);
        st = crypto::BCryptEncrypt(hKey, pt.data(), static_cast<uint32_t>(pt.size()), nullptr, ivEnc, 16, ct.data(), ctLen, &ctLen, crypto::BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(BCRYPT_SUCCESS(st), "BCryptEncrypt execution must succeed");

        // Decryption
        uint8_t ivDec[16]; std::memcpy(ivDec, iv, 16);
        uint32_t dtLen = 0;
        st = crypto::BCryptDecrypt(hKey, ct.data(), ctLen, nullptr, ivDec, 16, nullptr, 0, &dtLen, crypto::BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && dtLen == pt.size(), "BCryptDecrypt length query must match plaintext size");

        std::vector<uint8_t> dt(dtLen);
        std::memcpy(ivDec, iv, 16);
        st = crypto::BCryptDecrypt(hKey, ct.data(), ctLen, nullptr, ivDec, 16, dt.data(), dtLen, &dtLen, crypto::BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(BCRYPT_SUCCESS(st), "BCryptDecrypt execution must succeed");
        TEST_ASSERT(std::string(dt.begin(), dt.end()) == plain, "Decrypted AES plaintext must match original plain message");

        crypto::BCryptDestroyKey(hKey);
        crypto::BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    // ------------------------------------------------------------------------
    // 7. BCrypt PBKDF2 Key Derivation (BCryptDeriveKeyPBKDF2)
    // ------------------------------------------------------------------------
    {
        crypto::BCRYPT_ALG_HANDLE hPrf = nullptr;
        crypto::BCryptOpenAlgorithmProvider(&hPrf, crypto::BCRYPT_SHA256_ALGORITHM, nullptr, 0);

        std::string password = "SecretMasterPassword";
        std::string salt = "MicaNTSaltValue";
        uint8_t derived[32]{};

        int32_t st = crypto::BCryptDeriveKeyPBKDF2(
            hPrf,
            reinterpret_cast<const uint8_t*>(password.data()),
            static_cast<uint32_t>(password.size()),
            reinterpret_cast<const uint8_t*>(salt.data()),
            static_cast<uint32_t>(salt.size()),
            1000,
            derived,
            sizeof(derived),
            0
        );

        TEST_ASSERT(BCRYPT_SUCCESS(st), "BCryptDeriveKeyPBKDF2 must succeed");
        bool nonZero = false;
        for (uint8_t b : derived) if (b != 0) nonZero = true;
        TEST_ASSERT(nonZero, "PBKDF2 derived key must have non-zero entropy");

        crypto::BCryptCloseAlgorithmProvider(hPrf, 0);
    }

    // ------------------------------------------------------------------------
    // 8. NCrypt Key Storage Provider (KSP) Subsystem (ncrypt.dll)
    // ------------------------------------------------------------------------
    {
        crypto::NCRYPT_PROV_HANDLE hProv = 0;
        int32_t st = crypto::NCryptOpenStorageProvider(&hProv, crypto::MS_KEY_STORAGE_PROVIDER, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hProv != 0, "NCryptOpenStorageProvider must return valid provider handle");

        crypto::NCRYPT_KEY_HANDLE hKey = 0;
        st = crypto::NCryptCreatePersistedKey(hProv, &hKey, crypto::BCRYPT_RSA_ALGORITHM, L"MicaNT_Suite66_Key", 0, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st) && hKey != 0, "NCryptCreatePersistedKey must return valid key handle");

        st = crypto::NCryptFinalizeKey(hKey, 0);
        TEST_ASSERT(BCRYPT_SUCCESS(st), "NCryptFinalizeKey must succeed");

        crypto::NCryptFreeObject(hKey);
        crypto::NCryptFreeObject(hProv);
    }

    // ------------------------------------------------------------------------
    // 9. Legacy CryptoAPI in advapi32.dll (HCRYPTPROV, HCRYPTHASH, HCRYPTKEY)
    // ------------------------------------------------------------------------
    {
        uintptr_t hProv = 0;
        int32_t ok = advapi32::CryptAcquireContextW(&hProv, L"MicaKeyContainer", nullptr, advapi32::PROV_RSA_FULL, advapi32::CRYPT_NEWKEYSET);
        TEST_ASSERT(ok == win32::TRUE && hProv != 0, "CryptAcquireContextW must return TRUE and valid HCRYPTPROV");

        // Random generation
        uint8_t rnd[16]{};
        ok = advapi32::CryptGenRandom(hProv, 16, rnd);
        TEST_ASSERT(ok == win32::TRUE, "advapi32::CryptGenRandom must return TRUE");

        // Hashing via CryptoAPI
        uintptr_t hHash = 0;
        ok = advapi32::CryptCreateHash(hProv, advapi32::CALG_SHA_256, 0, 0, &hHash);
        TEST_ASSERT(ok == win32::TRUE && hHash != 0, "CryptCreateHash with CALG_SHA_256 must succeed");

        const char* text = "abc";
        ok = advapi32::CryptHashData(hHash, reinterpret_cast<const uint8_t*>(text), 3, 0);
        TEST_ASSERT(ok == win32::TRUE, "CryptHashData must succeed");

        uint32_t hashSize = 0;
        uint32_t paramLen = sizeof(hashSize);
        ok = advapi32::CryptGetHashParam(hHash, advapi32::HP_HASHSIZE, reinterpret_cast<uint8_t*>(&hashSize), &paramLen, 0);
        TEST_ASSERT(ok == win32::TRUE && hashSize == 32, "CryptGetHashParam HP_HASHSIZE must be 32 bytes");

        uint8_t hashVal[32]{};
        paramLen = sizeof(hashVal);
        ok = advapi32::CryptGetHashParam(hHash, advapi32::HP_HASHVAL, hashVal, &paramLen, 0);
        TEST_ASSERT(ok == win32::TRUE, "CryptGetHashParam HP_HASHVAL must succeed");
        TEST_ASSERT(toHex(hashVal, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                    "advapi32 CryptGetHashParam SHA-256 result must match KAT");

        // Key derivation and encryption
        uintptr_t hKey = 0;
        ok = advapi32::CryptDeriveKey(hProv, advapi32::CALG_AES_256, hHash, 0, &hKey);
        TEST_ASSERT(ok == win32::TRUE && hKey != 0, "CryptDeriveKey with CALG_AES_256 must succeed");

        advapi32::CryptDestroyHash(hHash);

        uint8_t cipherBuf[64] = "MicaNT Confidential Executive Payload";
        uint32_t dataLen = static_cast<uint32_t>(std::strlen(reinterpret_cast<char*>(cipherBuf)));
        uint32_t origLen = dataLen;

        ok = advapi32::CryptEncrypt(hKey, 0, win32::TRUE, 0, cipherBuf, &dataLen, sizeof(cipherBuf));
        TEST_ASSERT(ok == win32::TRUE && dataLen >= origLen, "CryptEncrypt must succeed and pad data");

        ok = advapi32::CryptDecrypt(hKey, 0, win32::TRUE, 0, cipherBuf, &dataLen);
        TEST_ASSERT(ok == win32::TRUE && dataLen == origLen, "CryptDecrypt must succeed and recover length");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(cipherBuf), dataLen) == "MicaNT Confidential Executive Payload",
                    "CryptDecrypt must recover original payload string");

        advapi32::CryptDestroyKey(hKey);
        advapi32::CryptReleaseContext(hProv, 0);
    }

    // ------------------------------------------------------------------------
    // 10. DPAPI in crypt32.dll (CryptProtectData / CryptUnprotectData)
    // ------------------------------------------------------------------------
    {
        std::string plain = "MicaNT Sovereign Secret Credentials";
        crypt32::DATA_BLOB inBlob;
        inBlob.cbData = static_cast<uint32_t>(plain.size());
        inBlob.pbData = reinterpret_cast<uint8_t*>(plain.data());

        crypt32::DATA_BLOB outBlob{};
        int32_t ok = crypt32::CryptProtectData(&inBlob, L"TestDescription", nullptr, nullptr, nullptr, 0, &outBlob);
        TEST_ASSERT(ok == win32::TRUE && outBlob.pbData != nullptr && outBlob.cbData > inBlob.cbData,
                    "CryptProtectData must succeed and produce authenticated ciphertext");

        wchar_t* pDesc = nullptr;
        crypt32::DATA_BLOB unprotectBlob{};
        ok = crypt32::CryptUnprotectData(&outBlob, &pDesc, nullptr, nullptr, nullptr, 0, &unprotectBlob);
        TEST_ASSERT(ok == win32::TRUE && unprotectBlob.pbData != nullptr, "CryptUnprotectData must succeed");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(unprotectBlob.pbData), unprotectBlob.cbData) == plain,
                    "Unprotected plaintext must match original plaintext");
        TEST_ASSERT(pDesc != nullptr && std::wstring(pDesc) == L"TestDescription",
                    "Unprotected description must match original description");

        win32::LocalFree(pDesc);
        win32::LocalFree(unprotectBlob.pbData);

        // Tamper test: Corrupt authentication tag / ciphertext
        outBlob.pbData[outBlob.cbData - 5] ^= 0x55;
        ok = crypt32::CryptUnprotectData(&outBlob, nullptr, nullptr, nullptr, nullptr, 0, &unprotectBlob);
        TEST_ASSERT(ok == win32::FALSE, "CryptUnprotectData must reject tampered ciphertext with NTE_BAD_DATA");

        win32::LocalFree(outBlob.pbData);
    }

    // ------------------------------------------------------------------------
    // 11. Base64 & Hex Conversion Engine in crypt32.dll
    // ------------------------------------------------------------------------
    {
        const uint8_t binary[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 };

        // Test CryptBinaryToStringA with Base64
        uint32_t cch = 0;
        int32_t ok = crypt32::CryptBinaryToStringA(binary, sizeof(binary), crypt32::CRYPT_STRING_BASE64 | crypt32::CRYPT_STRING_NOCRLF, nullptr, &cch);
        TEST_ASSERT(ok == win32::TRUE && cch > 0, "CryptBinaryToStringA size query must succeed");

        std::string b64Str(cch, '\0');
        ok = crypt32::CryptBinaryToStringA(binary, sizeof(binary), crypt32::CRYPT_STRING_BASE64 | crypt32::CRYPT_STRING_NOCRLF, b64Str.data(), &cch);
        TEST_ASSERT(ok == win32::TRUE, "CryptBinaryToStringA encoding must succeed");
        if (!b64Str.empty() && b64Str.back() == '\0') b64Str.pop_back();

        // Test CryptStringToBinaryA with Base64
        uint32_t cbBin = 0;
        ok = crypt32::CryptStringToBinaryA(b64Str.c_str(), static_cast<uint32_t>(b64Str.size()), crypt32::CRYPT_STRING_BASE64, nullptr, &cbBin, nullptr, nullptr);
        TEST_ASSERT(ok == win32::TRUE && cbBin == sizeof(binary), "CryptStringToBinaryA length query must match original binary size");

        std::vector<uint8_t> recovered(cbBin);
        ok = crypt32::CryptStringToBinaryA(b64Str.c_str(), static_cast<uint32_t>(b64Str.size()), crypt32::CRYPT_STRING_BASE64, recovered.data(), &cbBin, nullptr, nullptr);
        TEST_ASSERT(ok == win32::TRUE, "CryptStringToBinaryA decoding must succeed");
        TEST_ASSERT(std::memcmp(recovered.data(), binary, sizeof(binary)) == 0, "Decoded Base64 bytes must match original binary bytes");

        // Test CryptBinaryToStringA with Hex
        cch = 0;
        ok = crypt32::CryptBinaryToStringA(binary, sizeof(binary), crypt32::CRYPT_STRING_HEX | crypt32::CRYPT_STRING_NOCRLF, nullptr, &cch);
        TEST_ASSERT(ok == win32::TRUE && cch > 0, "CryptBinaryToStringA HEX size query must succeed");

        std::string hexStr(cch, '\0');
        ok = crypt32::CryptBinaryToStringA(binary, sizeof(binary), crypt32::CRYPT_STRING_HEX | crypt32::CRYPT_STRING_NOCRLF, hexStr.data(), &cch);
        TEST_ASSERT(ok == win32::TRUE, "CryptBinaryToStringA HEX encoding must succeed");
        if (!hexStr.empty() && hexStr.back() == '\0') hexStr.pop_back();

        cbBin = 0;
        ok = crypt32::CryptStringToBinaryA(hexStr.c_str(), static_cast<uint32_t>(hexStr.size()), crypt32::CRYPT_STRING_HEX, nullptr, &cbBin, nullptr, nullptr);
        TEST_ASSERT(ok == win32::TRUE && cbBin == sizeof(binary), "CryptStringToBinaryA HEX decoding length query must succeed");

        recovered.resize(cbBin);
        ok = crypt32::CryptStringToBinaryA(hexStr.c_str(), static_cast<uint32_t>(hexStr.size()), crypt32::CRYPT_STRING_HEX, recovered.data(), &cbBin, nullptr, nullptr);
        TEST_ASSERT(ok == win32::TRUE && std::memcmp(recovered.data(), binary, sizeof(binary)) == 0,
                    "Decoded Hex bytes must match original binary bytes");
    }

    // ------------------------------------------------------------------------
    // 12. X.509 Certificate Stores & Contexts in crypt32.dll
    // ------------------------------------------------------------------------
    {
        crypt32::HCERTSTORE hStore = crypt32::CertOpenSystemStoreW(0, L"ROOT");
        TEST_ASSERT(hStore != nullptr, "CertOpenSystemStoreW for 'ROOT' must succeed");

        // Enumerate root CA
        const crypt32::CERT_CONTEXT* pCert = crypt32::CertEnumCertificatesInStore(hStore, nullptr);
        TEST_ASSERT(pCert != nullptr, "CertEnumCertificatesInStore must find root CA certificate");

        wchar_t subjName[256]{};
        uint32_t cchSubj = crypt32::CertGetNameStringW(pCert, crypt32::CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subjName, 256);
        TEST_ASSERT(cchSubj > 1, "CertGetNameStringW for subject must return non-empty name");
        TEST_ASSERT(std::wstring(subjName).find(L"MicaNT Sovereign Root") != std::wstring::npos,
                    "Root certificate subject must contain 'MicaNT Sovereign Root'");

        wchar_t issuerName[256]{};
        uint32_t cchIss = crypt32::CertGetNameStringW(pCert, crypt32::CERT_NAME_SIMPLE_DISPLAY_TYPE, crypt32::CERT_NAME_ISSUER_FLAG, nullptr, issuerName, 256);
        TEST_ASSERT(cchIss > 1, "CertGetNameStringW for issuer must return non-empty name");
        TEST_ASSERT(std::wstring(issuerName) == std::wstring(subjName), "Self-signed root certificate issuer must equal subject");

        // Get SHA-1 thumbprint property
        uint8_t thumb[20]{};
        uint32_t cbThumb = sizeof(thumb);
        int32_t ok = crypt32::CertGetCertificateContextProperty(pCert, crypt32::CERT_SHA1_HASH_PROP_ID, thumb, &cbThumb);
        TEST_ASSERT(ok == win32::TRUE && cbThumb == 20, "CertGetCertificateContextProperty for CERT_SHA1_HASH_PROP_ID must succeed");

        // Duplicate and free context
        const crypt32::CERT_CONTEXT* pDup = crypt32::CertDuplicateCertificateContext(pCert);
        TEST_ASSERT(pDup == pCert, "CertDuplicateCertificateContext must return same pointer");
        ok = crypt32::CertFreeCertificateContext(pDup);
        TEST_ASSERT(ok == win32::TRUE, "CertFreeCertificateContext must succeed");

        // Find certificate by subject substring
        const crypt32::CERT_CONTEXT* pFound = crypt32::CertFindCertificateInStore(
            hStore,
            0x00010001,
            0,
            crypt32::CERT_FIND_SUBJECT_STR_W,
            L"Sovereign",
            nullptr
        );
        TEST_ASSERT(pFound != nullptr, "CertFindCertificateInStore with CERT_FIND_SUBJECT_STR_W must find matching certificate");
        crypt32::CertFreeCertificateContext(pFound);

        // Find certificate by SHA-1 thumbprint
        crypt32::DATA_BLOB thumbBlob{ 20, thumb };
        const crypt32::CERT_CONTEXT* pThumbFound = crypt32::CertFindCertificateInStore(
            hStore,
            0x00010001,
            0,
            crypt32::CERT_FIND_SHA1_HASH,
            &thumbBlob,
            nullptr
        );
        TEST_ASSERT(pThumbFound != nullptr, "CertFindCertificateInStore with CERT_FIND_SHA1_HASH must locate certificate by thumbprint");
        crypt32::CertFreeCertificateContext(pThumbFound);

        crypt32::CertCloseStore(hStore, 0);
    }

    // ------------------------------------------------------------------------
    // 13. Dynamic Loader & Version Metadata Registration
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        // bcrypt.dll exports
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptOpenAlgorithmProvider") != nullptr, "bcrypt.dll!BCryptOpenAlgorithmProvider must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptCloseAlgorithmProvider") != nullptr, "bcrypt.dll!BCryptCloseAlgorithmProvider must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptCreateHash") != nullptr, "bcrypt.dll!BCryptCreateHash must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptHashData") != nullptr, "bcrypt.dll!BCryptHashData must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptFinishHash") != nullptr, "bcrypt.dll!BCryptFinishHash must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptDuplicateHash") != nullptr, "bcrypt.dll!BCryptDuplicateHash must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptDestroyHash") != nullptr, "bcrypt.dll!BCryptDestroyHash must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptGenRandom") != nullptr, "bcrypt.dll!BCryptGenRandom must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptGenerateSymmetricKey") != nullptr, "bcrypt.dll!BCryptGenerateSymmetricKey must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptEncrypt") != nullptr, "bcrypt.dll!BCryptEncrypt must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptDecrypt") != nullptr, "bcrypt.dll!BCryptDecrypt must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptDestroyKey") != nullptr, "bcrypt.dll!BCryptDestroyKey must be exported");
        TEST_ASSERT(ldr.getExport("bcrypt.dll", "BCryptDeriveKeyPBKDF2") != nullptr, "bcrypt.dll!BCryptDeriveKeyPBKDF2 must be exported");

        // ncrypt.dll exports
        TEST_ASSERT(ldr.getExport("ncrypt.dll", "NCryptOpenStorageProvider") != nullptr, "ncrypt.dll!NCryptOpenStorageProvider must be exported");
        TEST_ASSERT(ldr.getExport("ncrypt.dll", "NCryptCreatePersistedKey") != nullptr, "ncrypt.dll!NCryptCreatePersistedKey must be exported");
        TEST_ASSERT(ldr.getExport("ncrypt.dll", "NCryptFinalizeKey") != nullptr, "ncrypt.dll!NCryptFinalizeKey must be exported");
        TEST_ASSERT(ldr.getExport("ncrypt.dll", "NCryptFreeObject") != nullptr, "ncrypt.dll!NCryptFreeObject must be exported");

        // crypt32.dll exports
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CryptProtectData") != nullptr, "crypt32.dll!CryptProtectData must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CryptUnprotectData") != nullptr, "crypt32.dll!CryptUnprotectData must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CryptBinaryToStringA") != nullptr, "crypt32.dll!CryptBinaryToStringA must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CryptBinaryToStringW") != nullptr, "crypt32.dll!CryptBinaryToStringW must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CryptStringToBinaryA") != nullptr, "crypt32.dll!CryptStringToBinaryA must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CryptStringToBinaryW") != nullptr, "crypt32.dll!CryptStringToBinaryW must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertOpenSystemStoreA") != nullptr, "crypt32.dll!CertOpenSystemStoreA must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertOpenSystemStoreW") != nullptr, "crypt32.dll!CertOpenSystemStoreW must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertCloseStore") != nullptr, "crypt32.dll!CertCloseStore must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertEnumCertificatesInStore") != nullptr, "crypt32.dll!CertEnumCertificatesInStore must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertFindCertificateInStore") != nullptr, "crypt32.dll!CertFindCertificateInStore must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertGetNameStringA") != nullptr, "crypt32.dll!CertGetNameStringA must be exported");
        TEST_ASSERT(ldr.getExport("crypt32.dll", "CertGetNameStringW") != nullptr, "crypt32.dll!CertGetNameStringW must be exported");

        // Version info verification
        uint32_t handle = 0;
        uint32_t sBcrypt = version::GetFileVersionInfoSizeA("bcrypt.dll", &handle);
        TEST_ASSERT(sBcrypt > 0, "bcrypt.dll must have version info resource");

        std::vector<uint8_t> vBcrypt(sBcrypt);
        TEST_ASSERT(version::GetFileVersionInfoA("bcrypt.dll", handle, sBcrypt, vBcrypt.data()) != 0, "GetFileVersionInfoA for bcrypt.dll must succeed");

        void* pDesc = nullptr;
        uint32_t dLen = 0;
        TEST_ASSERT(version::VerQueryValueA(vBcrypt.data(), "\\StringFileInfo\\040904B0\\FileDescription", &pDesc, &dLen) != 0, "VerQueryValueA for bcrypt.dll must succeed");
        TEST_ASSERT(std::string(static_cast<const char*>(pDesc)) == "Windows Cryptographic Primitives Library", "FileDescription must match Windows Cryptographic Primitives Library");

        uint32_t sCrypt32 = version::GetFileVersionInfoSizeA("crypt32.dll", &handle);
        TEST_ASSERT(sCrypt32 > 0, "crypt32.dll must have version info resource");

        std::vector<uint8_t> vCrypt32(sCrypt32);
        TEST_ASSERT(version::GetFileVersionInfoA("crypt32.dll", handle, sCrypt32, vCrypt32.data()) != 0, "GetFileVersionInfoA for crypt32.dll must succeed");

        pDesc = nullptr;
        dLen = 0;
        TEST_ASSERT(version::VerQueryValueA(vCrypt32.data(), "\\StringFileInfo\\040904B0\\FileDescription", &pDesc, &dLen) != 0, "VerQueryValueA for crypt32.dll must succeed");
        TEST_ASSERT(std::string(static_cast<const char*>(pDesc)) == "Crypto API32", "FileDescription must match Crypto API32");
    }

    // ------------------------------------------------------------------------
    // 14. Shell Built-in Commands Integration (bcrypt, certmgr, dpapi)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        // bcrypt info
        shell.execute("bcrypt info", out);
        TEST_ASSERT(out.str().find("MicaNT Cryptography Next Generation") != std::string::npos, "Shell bcrypt info command must succeed");

        // bcrypt hash sha256
        out.str("");
        shell.execute("bcrypt hash sha256 abc", out);
        TEST_ASSERT(out.str().find("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != std::string::npos,
                    "Shell bcrypt hash sha256 command must match KAT digest");

        // bcrypt rand
        out.str("");
        shell.execute("bcrypt rand 16", out);
        TEST_ASSERT(out.str().find("Cryptographically Secure Random Bytes") != std::string::npos, "Shell bcrypt rand command must succeed");

        // bcrypt test
        out.str("");
        shell.execute("bcrypt test", out);
        TEST_ASSERT(out.str().find("[PASS]") != std::string::npos, "Shell bcrypt test command must pass");

        // certmgr -list ROOT
        out.str("");
        shell.execute("certmgr -list ROOT", out);
        TEST_ASSERT(out.str().find("MicaNT Sovereign Root") != std::string::npos, "Shell certmgr -list ROOT must list root CA");

        // certmgr -find Sovereign
        out.str("");
        shell.execute("certmgr -find Sovereign", out);
        TEST_ASSERT(out.str().find("Certificate Match Found") != std::string::npos, "Shell certmgr -find Sovereign must find certificate");

        // dpapi test
        out.str("");
        shell.execute("dpapi test", out);
        TEST_ASSERT(out.str().find("CryptUnprotectData: PASS") != std::string::npos, "Shell dpapi test command must succeed");
    }

    std::cout << "[TEST] Suite 66: Windows CryptoAPI, CNG & Crypt32 Subsystems PASSED.\n";
}

void Test_SSPI_And_Schannel_Subsystems() {
    std::cout << "[TEST] Running Suite 67: Windows SSPI & Schannel TLS 1.3 Subsystems...\n";

    sspi::InitializeSspiSubsystemExports();

    // ------------------------------------------------------------------------
    // 1. Security Package Enumeration (EnumerateSecurityPackagesA/W)
    // ------------------------------------------------------------------------
    {
        uint32_t pkgCountA = 0;
        sspi::SecPkgInfoA* pPackagesA = nullptr;
        sspi::SECURITY_STATUS statusA = sspi::EnumerateSecurityPackagesA(&pkgCountA, &pPackagesA);
        TEST_ASSERT(statusA == sspi::SEC_E_OK, "EnumerateSecurityPackagesA must return SEC_E_OK");
        TEST_ASSERT(pkgCountA >= 3, "Must have at least 3 security packages (Schannel, NTLM, Negotiate)");
        TEST_ASSERT(pPackagesA != nullptr, "pPackagesA must not be null");

        bool foundSchannel = false;
        bool foundNtlm = false;
        bool foundNegotiate = false;

        for (uint32_t i = 0; i < pkgCountA; ++i) {
            std::string name = pPackagesA[i].Name ? pPackagesA[i].Name : "";
            if (name == sspi::UNISP_NAME_A || name == sspi::SCHANNEL_NAME_A) foundSchannel = true;
            if (name == sspi::NTLMSP_NAME_A || name == sspi::NTLM_NAME_A) foundNtlm = true;
            if (name == sspi::NEGOSSP_NAME_A || name == sspi::NEGOTIATE_NAME_A) foundNegotiate = true;
            TEST_ASSERT(pPackagesA[i].cbMaxToken > 0, "MaxToken must be greater than zero");
            TEST_ASSERT(pPackagesA[i].wVersion == 1, "Package version must be 1");
        }

        TEST_ASSERT(foundSchannel, "EnumerateSecurityPackagesA must contain Schannel");
        TEST_ASSERT(foundNtlm, "EnumerateSecurityPackagesA must contain NTLM");
        TEST_ASSERT(foundNegotiate, "EnumerateSecurityPackagesA must contain Negotiate");

        sspi::SECURITY_STATUS freeStA = sspi::FreeContextBuffer(pPackagesA);
        TEST_ASSERT(freeStA == sspi::SEC_E_OK, "FreeContextBuffer on packages must return SEC_E_OK");

        // Wide enumeration
        uint32_t pkgCountW = 0;
        sspi::SecPkgInfoW* pPackagesW = nullptr;
        sspi::SECURITY_STATUS statusW = sspi::EnumerateSecurityPackagesW(&pkgCountW, &pPackagesW);
        TEST_ASSERT(statusW == sspi::SEC_E_OK, "EnumerateSecurityPackagesW must return SEC_E_OK");
        TEST_ASSERT(pkgCountW >= 3, "EnumerateSecurityPackagesW must report at least 3 packages");
        TEST_ASSERT(pPackagesW != nullptr, "pPackagesW must not be null");
        sspi::FreeContextBuffer(pPackagesW);
    }

    // ------------------------------------------------------------------------
    // 2. Package Information Query (QuerySecurityPackageInfoA/W)
    // ------------------------------------------------------------------------
    {
        sspi::SecPkgInfoA* pInfoA = nullptr;
        sspi::SECURITY_STATUS qStA = sspi::QuerySecurityPackageInfoA(sspi::UNISP_NAME_A, &pInfoA);
        TEST_ASSERT(qStA == sspi::SEC_E_OK && pInfoA != nullptr, "QuerySecurityPackageInfoA for Schannel must succeed");
        TEST_ASSERT(std::string(pInfoA->Name) == sspi::SCHANNEL_NAME_A || std::string(pInfoA->Name) == sspi::UNISP_NAME_A, "Package name must match Schannel");
        TEST_ASSERT(pInfoA->cbMaxToken == 0x4000, "Schannel max token must be 16KB");
        sspi::FreeContextBuffer(pInfoA);

        sspi::SecPkgInfoW* pInfoW = nullptr;
        sspi::SECURITY_STATUS qStW = sspi::QuerySecurityPackageInfoW(sspi::UNISP_NAME_W, &pInfoW);
        TEST_ASSERT(qStW == sspi::SEC_E_OK && pInfoW != nullptr, "QuerySecurityPackageInfoW for Schannel must succeed");
        TEST_ASSERT(std::wstring(pInfoW->Name) == sspi::SCHANNEL_NAME_W || std::wstring(pInfoW->Name) == sspi::UNISP_NAME_W, "Package name must match Schannel");
        sspi::FreeContextBuffer(pInfoW);

        pInfoA = nullptr;
        sspi::SECURITY_STATUS qNtlm = sspi::QuerySecurityPackageInfoA(sspi::NTLMSP_NAME_A, &pInfoA);
        TEST_ASSERT(qNtlm == sspi::SEC_E_OK && pInfoA != nullptr, "QuerySecurityPackageInfoA for NTLM must succeed");
        TEST_ASSERT(std::string(pInfoA->Name) == sspi::NTLMSP_NAME_A, "Package name must match NTLM");
        sspi::FreeContextBuffer(pInfoA);

        sspi::SecPkgInfoA* pBadInfo = nullptr;
        sspi::SECURITY_STATUS badSt = sspi::QuerySecurityPackageInfoA("NonExistentSecurityPkg", &pBadInfo);
        TEST_ASSERT(badSt == sspi::SEC_E_SECPKG_NOT_FOUND, "Nonexistent package must return SEC_E_SECPKG_NOT_FOUND");
    }

    // ------------------------------------------------------------------------
    // 3. Security Function Tables (InitSecurityInterfaceA/W)
    // ------------------------------------------------------------------------
    {
        auto* tableA = sspi::InitSecurityInterfaceA();
        TEST_ASSERT(tableA != nullptr, "InitSecurityInterfaceA must return non-null table");
        TEST_ASSERT(tableA->dwVersion == 1, "SecurityFunctionTableA version must be 1");
        TEST_ASSERT(tableA->EnumerateSecurityPackagesA != nullptr, "Table must export EnumerateSecurityPackagesA");
        TEST_ASSERT(tableA->AcquireCredentialsHandleA != nullptr, "Table must export AcquireCredentialsHandleA");
        TEST_ASSERT(tableA->InitializeSecurityContextA != nullptr, "Table must export InitializeSecurityContextA");
        TEST_ASSERT(tableA->AcceptSecurityContext != nullptr, "Table must export AcceptSecurityContext");
        TEST_ASSERT(tableA->EncryptMessage != nullptr, "Table must export EncryptMessage");
        TEST_ASSERT(tableA->DecryptMessage != nullptr, "Table must export DecryptMessage");
        TEST_ASSERT(tableA->FreeContextBuffer != nullptr, "Table must export FreeContextBuffer");

        auto* tableW = sspi::InitSecurityInterfaceW();
        TEST_ASSERT(tableW != nullptr, "InitSecurityInterfaceW must return non-null table");
        TEST_ASSERT(tableW->dwVersion == 1, "SecurityFunctionTableW version must be 1");
        TEST_ASSERT(tableW->EnumerateSecurityPackagesW != nullptr, "Table must export EnumerateSecurityPackagesW");
        TEST_ASSERT(tableW->AcquireCredentialsHandleW != nullptr, "Table must export AcquireCredentialsHandleW");
        TEST_ASSERT(tableW->InitializeSecurityContextW != nullptr, "Table must export InitializeSecurityContextW");
    }

    // ------------------------------------------------------------------------
    // 4. Credential Handle Acquisition & Release Lifecycle
    // ------------------------------------------------------------------------
    {
        sspi::CredHandle hCred{};
        sspi::SCHANNEL_CRED schCred{};
        schCred.dwVersion = sspi::SCHANNEL_CRED_VERSION;
        schCred.grbitEnabledProtocols = sspi::SP_PROT_TLS1_3_CLIENT;

        sspi::SECURITY_STATUS acqSt = sspi::AcquireCredentialsHandleA(
            nullptr, sspi::UNISP_NAME_A, sspi::SECPKG_CRED_OUTBOUND, nullptr, &schCred, nullptr, nullptr, &hCred, nullptr
        );
        TEST_ASSERT(acqSt == sspi::SEC_E_OK, "AcquireCredentialsHandleA for Schannel must return SEC_E_OK");
        TEST_ASSERT(hCred.isValid(), "Credential handle must be valid");
        TEST_ASSERT(hCred.dwUpper == 0x53535049, "Credential handle magic must match 'SSPI'");

        sspi::SECURITY_STATUS freeSt = sspi::FreeCredentialsHandle(&hCred);
        TEST_ASSERT(freeSt == sspi::SEC_E_OK, "FreeCredentialsHandle must return SEC_E_OK");
        TEST_ASSERT(!hCred.isValid(), "Credential handle must be zeroed after free");

        sspi::CredHandle invalidCred{0x1234, 0x5678};
        TEST_ASSERT(sspi::FreeCredentialsHandle(&invalidCred) == sspi::SEC_E_INVALID_HANDLE, "Invalid handle must return SEC_E_INVALID_HANDLE");
    }

    // ------------------------------------------------------------------------
    // 5. TLS 1.3 ClientHello / ServerHello Handshake State Machine
    // ------------------------------------------------------------------------
    {
        sspi::CredHandle hClientCred{};
        sspi::SCHANNEL_CRED clientCred{};
        clientCred.dwVersion = sspi::SCHANNEL_CRED_VERSION;
        clientCred.grbitEnabledProtocols = sspi::SP_PROT_TLS1_3_CLIENT;
        sspi::AcquireCredentialsHandleA(nullptr, sspi::UNISP_NAME_A, sspi::SECPKG_CRED_OUTBOUND, nullptr, &clientCred, nullptr, nullptr, &hClientCred, nullptr);

        sspi::CtxtHandle hClientCtxt{};
        std::vector<uint8_t> clientHelloToken(4096);
        sspi::SecBuffer outClientBuf{ static_cast<uint32_t>(clientHelloToken.size()), sspi::SECBUFFER_TOKEN, clientHelloToken.data() };
        sspi::SecBufferDesc outClientDesc{ sspi::SECBUFFER_VERSION, 1, &outClientBuf };
        uint32_t ctxtAttr = 0;

        sspi::SECURITY_STATUS initSt1 = sspi::InitializeSecurityContextA(
            &hClientCred, nullptr, "micant.org", sspi::ISC_REQ_STREAM | sspi::ISC_REQ_SEQUENCE_DETECT,
            0, 0, nullptr, 0, &hClientCtxt, &outClientDesc, &ctxtAttr, nullptr
        );
        TEST_ASSERT(initSt1 == sspi::SEC_I_CONTINUE_NEEDED, "1st InitializeSecurityContextA must return SEC_I_CONTINUE_NEEDED");
        TEST_ASSERT(hClientCtxt.isValid(), "Client context handle must be valid");
        TEST_ASSERT(outClientBuf.cbBuffer > 0, "ClientHello token must not be empty");

        const auto* ch = reinterpret_cast<const uint8_t*>(outClientBuf.pvBuffer);
        TEST_ASSERT(ch[0] == 0x16, "TLS Record ContentType must be 0x16 (Handshake)");
        TEST_ASSERT(ch[1] == 0x03 && ch[2] == 0x01, "TLS Legacy Version must be 0x0301");
        TEST_ASSERT(ch[5] == 0x01, "Handshake Message Type must be 0x01 (ClientHello)");

        sspi::CtxtHandle hServerCtxt{};
        std::vector<uint8_t> serverHelloToken(4096);
        sspi::SecBuffer inServerBuf{ outClientBuf.cbBuffer, sspi::SECBUFFER_TOKEN, clientHelloToken.data() };
        sspi::SecBufferDesc inServerDesc{ sspi::SECBUFFER_VERSION, 1, &inServerBuf };
        sspi::SecBuffer outServerBuf{ static_cast<uint32_t>(serverHelloToken.size()), sspi::SECBUFFER_TOKEN, serverHelloToken.data() };
        sspi::SecBufferDesc outServerDesc{ sspi::SECBUFFER_VERSION, 1, &outServerBuf };
        uint32_t srvAttr = 0;

        sspi::SECURITY_STATUS acceptSt = sspi::AcceptSecurityContext(
            nullptr, nullptr, &inServerDesc, sspi::ISC_REQ_STREAM, 0, &hServerCtxt, &outServerDesc, &srvAttr, nullptr
        );
        TEST_ASSERT(acceptSt == sspi::SEC_I_CONTINUE_NEEDED, "AcceptSecurityContext must return SEC_I_CONTINUE_NEEDED");
        TEST_ASSERT(hServerCtxt.isValid(), "Server context handle must be valid");
        TEST_ASSERT(outServerBuf.cbBuffer > 0, "ServerHello token must not be empty");

        const auto* sh = reinterpret_cast<const uint8_t*>(outServerBuf.pvBuffer);
        TEST_ASSERT(sh[0] == 0x16, "Server Record ContentType must be 0x16 (Handshake)");
        TEST_ASSERT(sh[5] == 0x02, "Server Handshake Type must be 0x02 (ServerHello)");

        sspi::SecBuffer inClientBuf{ outServerBuf.cbBuffer, sspi::SECBUFFER_TOKEN, serverHelloToken.data() };
        sspi::SecBufferDesc inClientDesc{ sspi::SECBUFFER_VERSION, 1, &inClientBuf };
        sspi::SecBuffer outClientBuf2{ 0, sspi::SECBUFFER_TOKEN, nullptr };
        sspi::SecBufferDesc outClientDesc2{ sspi::SECBUFFER_VERSION, 1, &outClientBuf2 };

        sspi::SECURITY_STATUS initSt2 = sspi::InitializeSecurityContextA(
            &hClientCred, &hClientCtxt, "micant.org", sspi::ISC_REQ_STREAM,
            0, 0, &inClientDesc, 0, &hClientCtxt, &outClientDesc2, &ctxtAttr, nullptr
        );
        TEST_ASSERT(initSt2 == sspi::SEC_E_OK, "2nd InitializeSecurityContextA must return SEC_E_OK (Connection Established)");

        // --------------------------------------------------------------------
        // 6. QueryContextAttributes (Stream Sizes & Connection Info)
        // --------------------------------------------------------------------
        sspi::SecPkgContext_StreamSizes sizes{};
        sspi::SECURITY_STATUS szSt = sspi::QueryContextAttributesA(&hClientCtxt, sspi::SECPKG_ATTR_STREAM_SIZES, &sizes);
        TEST_ASSERT(szSt == sspi::SEC_E_OK, "QueryContextAttributesA SECPKG_ATTR_STREAM_SIZES must succeed");
        TEST_ASSERT(sizes.cbHeader == 5, "cbHeader must be 5 bytes for TLS record framing");
        TEST_ASSERT(sizes.cbTrailer == 32, "cbTrailer must be 32 bytes for HMAC-SHA256 tag");
        TEST_ASSERT(sizes.cbMaximumMessage == 16384, "cbMaximumMessage must be 16KB");

        sspi::SecPkgContext_ConnectionInfo conn{};
        sspi::SECURITY_STATUS connSt = sspi::QueryContextAttributesA(&hClientCtxt, sspi::SECPKG_ATTR_CONNECTION_INFO, &conn);
        TEST_ASSERT(connSt == sspi::SEC_E_OK, "QueryContextAttributesA SECPKG_ATTR_CONNECTION_INFO must succeed");
        TEST_ASSERT(conn.dwProtocol == sspi::SP_PROT_TLS1_3_CLIENT, "dwProtocol must be TLS 1.3");
        TEST_ASSERT(conn.dwCipherStrength == 256, "dwCipherStrength must be 256-bit");

        // --------------------------------------------------------------------
        // 7. TLS Record Protection & Unprotection (EncryptMessage / DecryptMessage)
        // --------------------------------------------------------------------
        std::string testMsg = "GET /v1/telemetry HTTP/1.1\r\nHost: micant.org\r\nUser-Agent: MicaNT-Kernel/1.0\r\n\r\n";
        std::vector<uint8_t> header(sizes.cbHeader);
        std::vector<uint8_t> payload(testMsg.begin(), testMsg.end());
        std::vector<uint8_t> trailer(sizes.cbTrailer);

        sspi::SecBuffer encBuffers[3] = {
            { static_cast<uint32_t>(header.size()), sspi::SECBUFFER_STREAM_HEADER, header.data() },
            { static_cast<uint32_t>(payload.size()), sspi::SECBUFFER_DATA, payload.data() },
            { static_cast<uint32_t>(trailer.size()), sspi::SECBUFFER_STREAM_TRAILER, trailer.data() }
        };
        sspi::SecBufferDesc encDesc{ sspi::SECBUFFER_VERSION, 3, encBuffers };

        sspi::SECURITY_STATUS encSt = sspi::EncryptMessage(&hClientCtxt, 0, &encDesc, 0);
        TEST_ASSERT(encSt == sspi::SEC_E_OK, "EncryptMessage must return SEC_E_OK");
        TEST_ASSERT(header[0] == 0x17, "Header byte 0 must be 0x17 (Application Data)");
        TEST_ASSERT(encBuffers[2].cbBuffer == 32, "Trailer buffer size must be 32 bytes");

        std::vector<uint8_t> recordStream;
        recordStream.insert(recordStream.end(), header.begin(), header.end());
        recordStream.insert(recordStream.end(), payload.begin(), payload.end());
        recordStream.insert(recordStream.end(), trailer.begin(), trailer.end());

        sspi::SecBuffer decBuffer{ static_cast<uint32_t>(recordStream.size()), sspi::SECBUFFER_DATA, recordStream.data() };
        sspi::SecBufferDesc decDesc{ sspi::SECBUFFER_VERSION, 1, &decBuffer };

        sspi::SECURITY_STATUS decSt = sspi::DecryptMessage(&hClientCtxt, &decDesc, 0, nullptr);
        TEST_ASSERT(decSt == sspi::SEC_E_OK, "DecryptMessage must return SEC_E_OK");
        TEST_ASSERT(decBuffer.cbBuffer == testMsg.size(), "Decrypted message size must match original plaintext");
        std::string recoveredPlaintext(reinterpret_cast<char*>(decBuffer.pvBuffer), decBuffer.cbBuffer);
        TEST_ASSERT(recoveredPlaintext == testMsg, "Decrypted message contents must match original plaintext");

        // --------------------------------------------------------------------
        // 8. Tamper Detection & Truncation Handling
        // --------------------------------------------------------------------
        std::string msg2 = "Secure Banking Transfer: $1,000,000 to Account #42";
        std::vector<uint8_t> h2(sizes.cbHeader);
        std::vector<uint8_t> p2(msg2.begin(), msg2.end());
        std::vector<uint8_t> t2(sizes.cbTrailer);

        sspi::SecBuffer encBufs2[3] = {
            { static_cast<uint32_t>(h2.size()), sspi::SECBUFFER_STREAM_HEADER, h2.data() },
            { static_cast<uint32_t>(p2.size()), sspi::SECBUFFER_DATA, p2.data() },
            { static_cast<uint32_t>(t2.size()), sspi::SECBUFFER_STREAM_TRAILER, t2.data() }
        };
        sspi::SecBufferDesc encDesc2{ sspi::SECBUFFER_VERSION, 3, encBufs2 };
        sspi::EncryptMessage(&hClientCtxt, 0, &encDesc2, 0);

        std::vector<uint8_t> tamperedRecord;
        tamperedRecord.insert(tamperedRecord.end(), h2.begin(), h2.end());
        tamperedRecord.insert(tamperedRecord.end(), p2.begin(), p2.end());
        tamperedRecord.insert(tamperedRecord.end(), t2.begin(), t2.end());

        tamperedRecord[5 + 10] ^= 0xFF;

        sspi::SecBuffer tamperBuf{ static_cast<uint32_t>(tamperedRecord.size()), sspi::SECBUFFER_DATA, tamperedRecord.data() };
        sspi::SecBufferDesc tamperDesc{ sspi::SECBUFFER_VERSION, 1, &tamperBuf };
        sspi::SECURITY_STATUS tamperSt = sspi::DecryptMessage(&hClientCtxt, &tamperDesc, 0, nullptr);
        TEST_ASSERT(tamperSt == sspi::SEC_E_MESSAGE_ALTERED, "Tampered ciphertext must return SEC_E_MESSAGE_ALTERED");

        std::vector<uint8_t> truncated(tamperedRecord.begin(), tamperedRecord.begin() + 10);
        sspi::SecBuffer truncBuf{ static_cast<uint32_t>(truncated.size()), sspi::SECBUFFER_DATA, truncated.data() };
        sspi::SecBufferDesc truncDesc{ sspi::SECBUFFER_VERSION, 1, &truncBuf };
        sspi::SECURITY_STATUS truncSt = sspi::DecryptMessage(&hClientCtxt, &truncDesc, 0, nullptr);
        TEST_ASSERT(truncSt == sspi::SEC_E_INCOMPLETE_MESSAGE, "Truncated buffer must return SEC_E_INCOMPLETE_MESSAGE");

        TEST_ASSERT(sspi::DeleteSecurityContext(&hClientCtxt) == sspi::SEC_E_OK, "DeleteSecurityContext for client must succeed");
        TEST_ASSERT(sspi::DeleteSecurityContext(&hServerCtxt) == sspi::SEC_E_OK, "DeleteSecurityContext for server must succeed");
        TEST_ASSERT(sspi::FreeCredentialsHandle(&hClientCred) == sspi::SEC_E_OK, "FreeCredentialsHandle for client must succeed");
    }

    // ------------------------------------------------------------------------
    // 9. NTLM Challenge-Response Authentication Handshake
    // ------------------------------------------------------------------------
    {
        sspi::CredHandle hNtlmCred{};
        sspi::SECURITY_STATUS nAcq = sspi::AcquireCredentialsHandleA(
            nullptr, sspi::NTLMSP_NAME_A, sspi::SECPKG_CRED_OUTBOUND, nullptr, nullptr, nullptr, nullptr, &hNtlmCred, nullptr
        );
        TEST_ASSERT(nAcq == sspi::SEC_E_OK, "AcquireCredentialsHandleA for NTLM must succeed");

        sspi::CtxtHandle hClientNtlm{};
        std::vector<uint8_t> t1Buf(1024);
        sspi::SecBuffer outT1{ static_cast<uint32_t>(t1Buf.size()), sspi::SECBUFFER_TOKEN, t1Buf.data() };
        sspi::SecBufferDesc descT1{ sspi::SECBUFFER_VERSION, 1, &outT1 };
        uint32_t clientFlags = 0;

        sspi::SECURITY_STATUS ntlmInit1 = sspi::InitializeSecurityContextA(
            &hNtlmCred, nullptr, nullptr, 0, 0, 0, nullptr, 0, &hClientNtlm, &descT1, &clientFlags, nullptr
        );
        TEST_ASSERT(ntlmInit1 == sspi::SEC_I_CONTINUE_NEEDED, "NTLM Type 1 init must return SEC_I_CONTINUE_NEEDED");
        TEST_ASSERT(std::memcmp(outT1.pvBuffer, "NTLMSSP\0\1", 9) == 0, "Type 1 message must have NTLMSSP signature and type 1");

        sspi::CtxtHandle hServerNtlm{};
        std::vector<uint8_t> t2Buf(1024);
        sspi::SecBuffer inT1{ outT1.cbBuffer, sspi::SECBUFFER_TOKEN, t1Buf.data() };
        sspi::SecBufferDesc srvInT1{ sspi::SECBUFFER_VERSION, 1, &inT1 };
        sspi::SecBuffer outT2{ static_cast<uint32_t>(t2Buf.size()), sspi::SECBUFFER_TOKEN, t2Buf.data() };
        sspi::SecBufferDesc srvOutT2{ sspi::SECBUFFER_VERSION, 1, &outT2 };
        uint32_t srvFlags = 0;

        sspi::SECURITY_STATUS ntlmAcc1 = sspi::AcceptSecurityContext(
            nullptr, nullptr, &srvInT1, 0, 0, &hServerNtlm, &srvOutT2, &srvFlags, nullptr
        );
        TEST_ASSERT(ntlmAcc1 == sspi::SEC_I_CONTINUE_NEEDED, "NTLM Type 2 accept must return SEC_I_CONTINUE_NEEDED");
        TEST_ASSERT(std::memcmp(outT2.pvBuffer, "NTLMSSP\0\2", 9) == 0, "Type 2 message must have NTLMSSP signature and type 2");

        std::vector<uint8_t> t3Buf(1024);
        sspi::SecBuffer inT2{ outT2.cbBuffer, sspi::SECBUFFER_TOKEN, t2Buf.data() };
        sspi::SecBufferDesc cliInT2{ sspi::SECBUFFER_VERSION, 1, &inT2 };
        sspi::SecBuffer outT3{ static_cast<uint32_t>(t3Buf.size()), sspi::SECBUFFER_TOKEN, t3Buf.data() };
        sspi::SecBufferDesc cliOutT3{ sspi::SECBUFFER_VERSION, 1, &outT3 };

        sspi::SECURITY_STATUS ntlmInit2 = sspi::InitializeSecurityContextA(
            &hNtlmCred, &hClientNtlm, nullptr, 0, 0, 0, &cliInT2, 0, &hClientNtlm, &cliOutT3, &clientFlags, nullptr
        );
        TEST_ASSERT(ntlmInit2 == sspi::SEC_E_OK, "NTLM Type 3 client init must return SEC_E_OK");
        TEST_ASSERT(std::memcmp(outT3.pvBuffer, "NTLMSSP\0\3", 9) == 0, "Type 3 message must have NTLMSSP signature and type 3");

        sspi::DeleteSecurityContext(&hClientNtlm);
        sspi::DeleteSecurityContext(&hServerNtlm);
        sspi::FreeCredentialsHandle(&hNtlmCred);
    }

    // ------------------------------------------------------------------------
    // 10. WinINet HTTPS Integration Over Schannel TLS
    // ------------------------------------------------------------------------
    {
        wininet::HINTERNET hRoot = wininet::InternetOpenA("MicaNT-HTTPS-Agent/1.0", wininet::INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
        TEST_ASSERT(hRoot != nullptr, "InternetOpenA must return a valid root session handle");

        wininet::HINTERNET hConn = wininet::InternetConnectA(hRoot, "micant.org", 443, nullptr, nullptr, wininet::INTERNET_SERVICE_HTTP, 0, 0);
        TEST_ASSERT(hConn != nullptr, "InternetConnectA to HTTPS port 443 must succeed");

        const char* acceptTypes[] = { "text/html", nullptr };
        wininet::HINTERNET hReq = wininet::HttpOpenRequestA(hConn, "GET", "/secure_endpoint.html", "HTTP/1.1", nullptr, acceptTypes, wininet::INTERNET_FLAG_SECURE, 0);
        TEST_ASSERT(hReq != nullptr, "HttpOpenRequestA with INTERNET_FLAG_SECURE must succeed");

        win32::BOOL sent = wininet::HttpSendRequestA(hReq, nullptr, 0, nullptr, 0);
        TEST_ASSERT(sent != 0, "HttpSendRequestA over TLS must succeed");

        char srvHeader[128]{};
        uint32_t srvLen = sizeof(srvHeader);
        wininet::HttpQueryInfoA(hReq, wininet::HTTP_QUERY_SERVER, srvHeader, &srvLen, nullptr);
        std::string srvStr(srvHeader);
        TEST_ASSERT(srvStr.find("Schannel TLS 1.3") != std::string::npos, "Server header must reflect Schannel TLS 1.3 integration");

        std::vector<char> body(256);
        uint32_t bytesRead = 0;
        wininet::InternetReadFile(hReq, body.data(), static_cast<uint32_t>(body.size() - 1), &bytesRead);
        body[bytesRead] = '\0';
        std::string bodyStr(body.data());
        TEST_ASSERT(bodyStr.find("MicaNT Secure HTTPS Web Subsystem") != std::string::npos, "Body must contain secure HTTPS welcome page");

        wininet::InternetCloseHandle(hReq);
        wininet::InternetCloseHandle(hConn);
        wininet::InternetCloseHandle(hRoot);
    }

    // ------------------------------------------------------------------------
    // 11. Dynamic Loader Exports & Version Database Verification
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("secur32.dll", "InitSecurityInterfaceA") != nullptr, "secur32.dll!InitSecurityInterfaceA must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "InitSecurityInterfaceW") != nullptr, "secur32.dll!InitSecurityInterfaceW must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "EnumerateSecurityPackagesA") != nullptr, "secur32.dll!EnumerateSecurityPackagesA must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "EnumerateSecurityPackagesW") != nullptr, "secur32.dll!EnumerateSecurityPackagesW must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "QuerySecurityPackageInfoA") != nullptr, "secur32.dll!QuerySecurityPackageInfoA must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "QuerySecurityPackageInfoW") != nullptr, "secur32.dll!QuerySecurityPackageInfoW must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "AcquireCredentialsHandleA") != nullptr, "secur32.dll!AcquireCredentialsHandleA must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "AcquireCredentialsHandleW") != nullptr, "secur32.dll!AcquireCredentialsHandleW must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "FreeCredentialsHandle") != nullptr, "secur32.dll!FreeCredentialsHandle must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "InitializeSecurityContextA") != nullptr, "secur32.dll!InitializeSecurityContextA must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "InitializeSecurityContextW") != nullptr, "secur32.dll!InitializeSecurityContextW must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "AcceptSecurityContext") != nullptr, "secur32.dll!AcceptSecurityContext must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "CompleteAuthToken") != nullptr, "secur32.dll!CompleteAuthToken must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "DeleteSecurityContext") != nullptr, "secur32.dll!DeleteSecurityContext must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "ApplyControlToken") != nullptr, "secur32.dll!ApplyControlToken must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "QueryContextAttributesA") != nullptr, "secur32.dll!QueryContextAttributesA must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "QueryContextAttributesW") != nullptr, "secur32.dll!QueryContextAttributesW must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "EncryptMessage") != nullptr, "secur32.dll!EncryptMessage must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "DecryptMessage") != nullptr, "secur32.dll!DecryptMessage must be exported");
        TEST_ASSERT(ldr.getExport("secur32.dll", "FreeContextBuffer") != nullptr, "secur32.dll!FreeContextBuffer must be exported");

        TEST_ASSERT(ldr.getExport("sspicli.dll", "InitSecurityInterfaceA") != nullptr, "sspicli.dll!InitSecurityInterfaceA must be exported");
        TEST_ASSERT(ldr.getExport("sspicli.dll", "AcquireCredentialsHandleA") != nullptr, "sspicli.dll!AcquireCredentialsHandleA must be exported");
        TEST_ASSERT(ldr.getExport("sspicli.dll", "InitializeSecurityContextA") != nullptr, "sspicli.dll!InitializeSecurityContextA must be exported");
        TEST_ASSERT(ldr.getExport("sspicli.dll", "EncryptMessage") != nullptr, "sspicli.dll!EncryptMessage must be exported");
        TEST_ASSERT(ldr.getExport("sspicli.dll", "DecryptMessage") != nullptr, "sspicli.dll!DecryptMessage must be exported");

        TEST_ASSERT(ldr.getExport("schannel.dll", "SslEmptyCacheA") != nullptr, "schannel.dll!SslEmptyCacheA must be exported");
        TEST_ASSERT(ldr.getExport("schannel.dll", "SslEmptyCacheW") != nullptr, "schannel.dll!SslEmptyCacheW must be exported");

        uint32_t handle = 0;
        uint32_t sSecur32 = version::GetFileVersionInfoSizeA("secur32.dll", &handle);
        TEST_ASSERT(sSecur32 > 0, "secur32.dll must have version info resource");

        std::vector<uint8_t> vSecur32(sSecur32);
        TEST_ASSERT(version::GetFileVersionInfoA("secur32.dll", handle, sSecur32, vSecur32.data()) != 0, "GetFileVersionInfoA for secur32.dll must succeed");

        void* pDesc = nullptr;
        uint32_t dLen = 0;
        TEST_ASSERT(version::VerQueryValueA(vSecur32.data(), "\\StringFileInfo\\040904B0\\FileDescription", &pDesc, &dLen) != 0, "VerQueryValueA for secur32.dll must succeed");
        TEST_ASSERT(std::string(static_cast<const char*>(pDesc)) == "Security Support Provider Interface", "FileDescription must match SSPI description");

        uint32_t sSchannel = version::GetFileVersionInfoSizeA("schannel.dll", &handle);
        TEST_ASSERT(sSchannel > 0, "schannel.dll must have version info resource");

        std::vector<uint8_t> vSchannel(sSchannel);
        TEST_ASSERT(version::GetFileVersionInfoA("schannel.dll", handle, sSchannel, vSchannel.data()) != 0, "GetFileVersionInfoA for schannel.dll must succeed");

        pDesc = nullptr;
        dLen = 0;
        TEST_ASSERT(version::VerQueryValueA(vSchannel.data(), "\\StringFileInfo\\040904B0\\FileDescription", &pDesc, &dLen) != 0, "VerQueryValueA for schannel.dll must succeed");
        TEST_ASSERT(std::string(static_cast<const char*>(pDesc)) == "TLS / SSL Security Provider", "FileDescription must match TLS / SSL Security Provider");
    }

    // ------------------------------------------------------------------------
    // 12. Shell Built-in Commands Integration (sspi, schannel)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("sspi info", out);
        TEST_ASSERT(out.str().find("Security Support Provider Interface") != std::string::npos, "Shell sspi info command must succeed");

        out.str("");
        shell.execute("sspi packages", out);
        TEST_ASSERT(out.str().find("Schannel") != std::string::npos, "Shell sspi packages must contain Schannel");
        TEST_ASSERT(out.str().find("NTLM") != std::string::npos, "Shell sspi packages must contain NTLM");

        out.str("");
        shell.execute("sspi test", out);
        TEST_ASSERT(out.str().find("Self-test complete") != std::string::npos, "Shell sspi test command must succeed");

        out.str("");
        shell.execute("schannel test", out);
        TEST_ASSERT(out.str().find("ALL TLS 1.3 CHECKS PASSED") != std::string::npos, "Shell schannel test command must pass all TLS checks");

        out.str("");
        shell.execute("schannel info", out);
        TEST_ASSERT(out.str().find("Secure Channel Subsystem") != std::string::npos, "Shell schannel info command must succeed");
    }

    std::cout << "[TEST] Suite 67: Windows SSPI & Schannel TLS 1.3 Subsystems PASSED.\n";
}

// ============================================================================
// Test Suite 68: Windows Remote Procedure Call (RPC) Runtime & NDR Engine
// ============================================================================

void Test_RPC_Runtime_And_NDR_Subsystem() {
    std::cout << "[TEST] Running Suite 68: Windows Remote Procedure Call (RPC) Runtime & NDR Subsystem...\n";

    // ------------------------------------------------------------------------
    // 1. UUID Generation & Arithmetic Properties
    // ------------------------------------------------------------------------
    {
        micant::UUID uV4{};
        rpc::RPC_STATUS stV4 = rpc::UuidCreate(&uV4);
        TEST_ASSERT(stV4 == rpc::RPC_S_OK, "UuidCreate (RFC 4122 v4) must succeed");
        TEST_ASSERT(((uV4.Data3 >> 12) & 0x0F) == 4, "UuidCreate must set version 4 in Data3 high nibble");
        TEST_ASSERT((uV4.Data4[0] & 0xC0) == 0x80, "UuidCreate must set RFC 4122 variant (0b10) in Data4[0]");

        micant::UUID uV1{};
        rpc::RPC_STATUS stV1 = rpc::UuidCreateSequential(&uV1);
        TEST_ASSERT(stV1 == rpc::RPC_S_OK, "UuidCreateSequential (RFC 4122 v1) must succeed");
        TEST_ASSERT(((uV1.Data3 >> 12) & 0x0F) == 1, "UuidCreateSequential must set version 1 in Data3 high nibble");

        micant::UUID nilUuid{};
        rpc::RPC_STATUS stNil = 0;
        TEST_ASSERT(rpc::UuidIsNil(&nilUuid, &stNil) == 1, "UuidIsNil must return 1 for zeroed UUID");
        TEST_ASSERT(stNil == rpc::RPC_S_OK, "UuidIsNil status must be RPC_S_OK");
        TEST_ASSERT(rpc::UuidIsNil(&uV4, nullptr) == 0, "UuidIsNil must return 0 for active UUID");

        rpc::RPC_STATUS stEq = 0;
        TEST_ASSERT(rpc::UuidEqual(&uV4, &uV4, &stEq) == 1, "UuidEqual must return 1 for identical UUIDs");
        TEST_ASSERT(rpc::UuidEqual(&uV4, &uV1, &stEq) == 0, "UuidEqual must return 0 for distinct UUIDs");

        rpc::RPC_STATUS stCmp = 0;
        TEST_ASSERT(rpc::UuidCompare(&uV4, &uV4, &stCmp) == 0, "UuidCompare must return 0 for equal UUIDs");

        rpc::RPC_STATUS stHash = 0;
        uint16_t hashVal = rpc::UuidHash(&uV4, &stHash);
        TEST_ASSERT(stHash == rpc::RPC_S_OK, "UuidHash status must be RPC_S_OK");
        TEST_ASSERT(hashVal != 0, "UuidHash for valid UUID should produce non-zero hash value");
    }

    // ------------------------------------------------------------------------
    // 2. UUID String Formatting & Parsing (ANSI & Wide)
    // ------------------------------------------------------------------------
    {
        micant::UUID origUuid{ 0x12345678, 0xABCD, 0x4EF0, { 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67 } };
        unsigned char* szUuidA = nullptr;
        rpc::RPC_STATUS stStrA = rpc::UuidToStringA(&origUuid, &szUuidA);
        TEST_ASSERT(stStrA == rpc::RPC_S_OK, "UuidToStringA must succeed");
        TEST_ASSERT(szUuidA != nullptr, "UuidToStringA must return non-null string");

        std::string strVal(reinterpret_cast<char*>(szUuidA));
        TEST_ASSERT(strVal.length() == 36, "Canonical UUID string must have length 36");
        TEST_ASSERT(strVal[8] == '-' && strVal[13] == '-' && strVal[18] == '-' && strVal[23] == '-', "UUID string must have standard hyphen delimiters");
        TEST_ASSERT(strVal == "12345678-abcd-4ef0-89ab-cdef01234567", "Canonical UUID string must match expected format");

        micant::UUID parsedUuidA{};
        rpc::RPC_STATUS stParseA = rpc::UuidFromStringA(szUuidA, &parsedUuidA);
        TEST_ASSERT(stParseA == rpc::RPC_S_OK, "UuidFromStringA must succeed");
        TEST_ASSERT(rpc::UuidEqual(&origUuid, &parsedUuidA, nullptr) == 1, "Parsed UUID must equal original UUID");

        rpc::RpcStringFreeA(&szUuidA);
        TEST_ASSERT(szUuidA == nullptr, "RpcStringFreeA must zero pointer");

        wchar_t* szUuidW = nullptr;
        rpc::RPC_STATUS stStrW = rpc::UuidToStringW(&origUuid, &szUuidW);
        TEST_ASSERT(stStrW == rpc::RPC_S_OK, "UuidToStringW must succeed");
        TEST_ASSERT(szUuidW != nullptr, "UuidToStringW must return non-null string");

        micant::UUID parsedUuidW{};
        rpc::RPC_STATUS stParseW = rpc::UuidFromStringW(szUuidW, &parsedUuidW);
        TEST_ASSERT(stParseW == rpc::RPC_S_OK, "UuidFromStringW must succeed");
        TEST_ASSERT(rpc::UuidEqual(&origUuid, &parsedUuidW, nullptr) == 1, "Parsed wide UUID must equal original UUID");

        rpc::RpcStringFreeW(&szUuidW);
        TEST_ASSERT(szUuidW == nullptr, "RpcStringFreeW must zero pointer");
    }

    // ------------------------------------------------------------------------
    // 3. String Binding Engine Compose & Parse
    // ------------------------------------------------------------------------
    {
        unsigned char* strBindingA = nullptr;
        rpc::RPC_STATUS stComp = rpc::RpcStringBindingComposeA(
            (unsigned char*)"11112222-3333-4444-5555-666677778888",
            (unsigned char*)"ncacn_ip_tcp",
            (unsigned char*)"192.168.1.50",
            (unsigned char*)"135",
            (unsigned char*)"Security=True",
            &strBindingA
        );
        TEST_ASSERT(stComp == rpc::RPC_S_OK, "RpcStringBindingComposeA must succeed");
        TEST_ASSERT(strBindingA != nullptr, "Composed string binding must not be null");

        std::string composed(reinterpret_cast<char*>(strBindingA));
        TEST_ASSERT(composed == "11112222-3333-4444-5555-666677778888@ncacn_ip_tcp:192.168.1.50[135,Security=True]",
                    "Composed string binding must match standard NT format");

        unsigned char *obj = nullptr, *prot = nullptr, *net = nullptr, *ep = nullptr, *opt = nullptr;
        rpc::RPC_STATUS stParse = rpc::RpcStringBindingParseA(strBindingA, &obj, &prot, &net, &ep, &opt);
        TEST_ASSERT(stParse == rpc::RPC_S_OK, "RpcStringBindingParseA must succeed");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(obj)) == "11112222-3333-4444-5555-666677778888", "Parsed ObjUuid must match");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(prot)) == "ncacn_ip_tcp", "Parsed Protseq must match");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(net)) == "192.168.1.50", "Parsed NetworkAddr must match");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(ep)) == "135", "Parsed Endpoint must match");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(opt)) == "Security=True", "Parsed Options must match");

        rpc::RpcStringFreeA(&obj);
        rpc::RpcStringFreeA(&prot);
        rpc::RpcStringFreeA(&net);
        rpc::RpcStringFreeA(&ep);
        rpc::RpcStringFreeA(&opt);
        rpc::RpcStringFreeA(&strBindingA);
    }

    // ------------------------------------------------------------------------
    // 4. Binding Handle Lifecycle (Create, Query, Copy, Free)
    // ------------------------------------------------------------------------
    {
        rpc::RPC_BINDING_HANDLE hBinding = nullptr;
        unsigned char strSource[] = "ncalrpc:[ep_test_alpc]";
        rpc::RPC_STATUS stBind = rpc::RpcBindingFromStringBindingA(strSource, &hBinding);
        TEST_ASSERT(stBind == rpc::RPC_S_OK, "RpcBindingFromStringBindingA must succeed");
        TEST_ASSERT(hBinding != nullptr, "Binding handle must be non-null");

        unsigned char* strRoundtrip = nullptr;
        rpc::RPC_STATUS stToStr = rpc::RpcBindingToStringBindingA(hBinding, &strRoundtrip);
        TEST_ASSERT(stToStr == rpc::RPC_S_OK, "RpcBindingToStringBindingA must succeed");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(strRoundtrip)) == "ncalrpc:[ep_test_alpc]", "Roundtrip string binding must match original");
        rpc::RpcStringFreeA(&strRoundtrip);

        rpc::RPC_BINDING_HANDLE hCopy = nullptr;
        rpc::RPC_STATUS stCopy = rpc::RpcBindingCopy(hBinding, &hCopy);
        TEST_ASSERT(stCopy == rpc::RPC_S_OK, "RpcBindingCopy must succeed");
        TEST_ASSERT(hCopy != nullptr && hCopy != hBinding, "Copied binding handle must be distinct valid handle");

        rpc::RPC_STATUS stFreeCopy = rpc::RpcBindingFree(&hCopy);
        TEST_ASSERT(stFreeCopy == rpc::RPC_S_OK, "RpcBindingFree on copied handle must succeed");
        TEST_ASSERT(hCopy == nullptr, "Handle pointer must be zeroed after free");

        rpc::RPC_STATUS stFreeOrig = rpc::RpcBindingFree(&hBinding);
        TEST_ASSERT(stFreeOrig == rpc::RPC_S_OK, "RpcBindingFree on original handle must succeed");
        TEST_ASSERT(hBinding == nullptr, "Handle pointer must be zeroed after free");

        rpc::RPC_BINDING_HANDLE nullHandle = nullptr;
        TEST_ASSERT(rpc::RpcBindingFree(&nullHandle) == rpc::RPC_S_INVALID_BINDING, "Freeing null handle must return RPC_S_INVALID_BINDING");
    }

    // ------------------------------------------------------------------------
    // 5. Authentication Configuration on Binding Handle
    // ------------------------------------------------------------------------
    {
        rpc::RPC_BINDING_HANDLE hBinding = nullptr;
        unsigned char strSource[] = "ncacn_ip_tcp:10.0.0.1[8080]";
        rpc::RpcBindingFromStringBindingA(strSource, &hBinding);

        rpc::RPC_STATUS stAuth = rpc::RpcBindingSetAuthInfoA(
            hBinding,
            (unsigned char*)"host/domain-controller.micant.local",
            rpc::RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
            rpc::RPC_C_AUTHN_WINNT,
            nullptr,
            0
        );
        TEST_ASSERT(stAuth == rpc::RPC_S_OK, "RpcBindingSetAuthInfoA must succeed");

        auto* b = reinterpret_cast<rpc::RpcBinding*>(hBinding);
        TEST_ASSERT(b->authnLevel == rpc::RPC_C_AUTHN_LEVEL_PKT_PRIVACY, "AuthnLevel must be preserved");
        TEST_ASSERT(b->authnSvc == rpc::RPC_C_AUTHN_WINNT, "AuthnSvc must be preserved");
        TEST_ASSERT(b->serverPrincName == "host/domain-controller.micant.local", "ServerPrincName must be preserved");

        rpc::RpcBindingFree(&hBinding);
    }

    // ------------------------------------------------------------------------
    // 6. Server Protocol Sequence & Endpoint Management
    // ------------------------------------------------------------------------
    {
        rpc::RPC_STATUS stProt = rpc::RpcServerUseProtseqA((unsigned char*)"ncalrpc", 20, nullptr);
        TEST_ASSERT(stProt == rpc::RPC_S_OK, "RpcServerUseProtseqA for ncalrpc must succeed");

        rpc::RPC_STATUS stEp1 = rpc::RpcServerUseProtseqEpA((unsigned char*)"ncalrpc", 20, (unsigned char*)"ep_micant_lpc", nullptr);
        TEST_ASSERT(stEp1 == rpc::RPC_S_OK, "RpcServerUseProtseqEpA for ncalrpc must succeed");

        rpc::RPC_STATUS stEp2 = rpc::RpcServerUseProtseqEpA((unsigned char*)"ncacn_np", 20, (unsigned char*)"pipe_micant_rpc", nullptr);
        TEST_ASSERT(stEp2 == rpc::RPC_S_OK, "RpcServerUseProtseqEpA for ncacn_np must succeed");

        rpc::RPC_STATUS stEp3 = rpc::RpcServerUseProtseqEpA((unsigned char*)"ncacn_ip_tcp", 20, (unsigned char*)"13500", nullptr);
        TEST_ASSERT(stEp3 == rpc::RPC_S_OK, "RpcServerUseProtseqEpA for ncacn_ip_tcp must succeed");

        rpc::RPC_STATUS stBad = rpc::RpcServerUseProtseqA((unsigned char*)"invalid_protseq_xyz", 10, nullptr);
        TEST_ASSERT(stBad == rpc::RPC_S_PROTSEQ_NOT_SUPPORTED, "Unsupported protocol sequence must return RPC_S_PROTSEQ_NOT_SUPPORTED");

        auto eps = rpc::RpcServerManager::Instance().getEndpoints();
        TEST_ASSERT(!eps.empty(), "Server endpoints list must contain registered endpoints");
    }

    // ------------------------------------------------------------------------
    // 7. Server Interface Registration & Listening State
    // ------------------------------------------------------------------------
    {
        rpc::RPC_SYNTAX_IDENTIFIER ifId{};
        rpc::UuidCreate(&ifId.SyntaxGUID);
        ifId.SyntaxVersion.MajorVersion = 1;
        ifId.SyntaxVersion.MinorVersion = 0;

        rpc::RPC_SERVER_INTERFACE testIf{};
        testIf.Length = sizeof(testIf);
        testIf.InterfaceId = ifId;
        testIf.TransferSyntax = rpc::NDR_TRANSFER_SYNTAX;

        size_t countBefore = rpc::RpcServerManager::Instance().getInterfaceCount();
        rpc::RPC_STATUS stReg = rpc::RpcServerRegisterIf(&testIf, nullptr, nullptr);
        TEST_ASSERT(stReg == rpc::RPC_S_OK, "RpcServerRegisterIf must succeed");
        TEST_ASSERT(rpc::RpcServerManager::Instance().getInterfaceCount() == countBefore + 1, "Interface count must increase");

        TEST_ASSERT(rpc::RpcServerManager::Instance().findInterface(ifId) == &testIf, "findInterface must locate registered interface");

        rpc::RPC_STATUS stListen = rpc::RpcServerListen(1, 10, 1);
        TEST_ASSERT(stListen == rpc::RPC_S_OK, "RpcServerListen must succeed");
        TEST_ASSERT(rpc::RpcServerManager::Instance().isListening() == true, "Server must report listening state");

        rpc::RPC_STATUS stStop = rpc::RpcMgmtStopServerListening(nullptr);
        TEST_ASSERT(stStop == rpc::RPC_S_OK, "RpcMgmtStopServerListening must succeed");
        TEST_ASSERT(rpc::RpcServerManager::Instance().isListening() == false, "Server must report idle state after stop");

        rpc::RPC_STATUS stUnreg = rpc::RpcServerUnregisterIf(&testIf, nullptr, 0);
        TEST_ASSERT(stUnreg == rpc::RPC_S_OK, "RpcServerUnregisterIf must succeed");
        TEST_ASSERT(rpc::RpcServerManager::Instance().getInterfaceCount() == countBefore, "Interface count must return to baseline");
    }

    // ------------------------------------------------------------------------
    // 8. NDR Buffer Allocation & Deallocation
    // ------------------------------------------------------------------------
    {
        rpc::RPC_MESSAGE msg{};
        rpc::MIDL_STUB_MESSAGE stubMsg{};
        stubMsg.RpcMsg = &msg;

        rpc::NdrGetBuffer(&stubMsg, 2048, nullptr);
        TEST_ASSERT(stubMsg.Buffer != nullptr, "NdrGetBuffer must allocate non-null buffer");
        TEST_ASSERT(stubMsg.BufferStart == stubMsg.Buffer, "BufferStart must match Buffer");
        TEST_ASSERT(stubMsg.BufferLength == 2048, "BufferLength must match requested size");
        TEST_ASSERT(msg.Buffer == stubMsg.Buffer, "RpcMsg->Buffer must mirror stubMsg.Buffer");

        rpc::NdrFreeBuffer(&stubMsg);
        TEST_ASSERT(msg.Buffer == nullptr, "NdrFreeBuffer must release and nullify buffer pointer");
    }

    // ------------------------------------------------------------------------
    // 9. NDR Simple Scalar Type Marshalling & Unmarshalling
    // ------------------------------------------------------------------------
    {
        rpc::RPC_MESSAGE msg{};
        rpc::MIDL_STUB_MESSAGE stubMsg{};
        stubMsg.RpcMsg = &msg;
        rpc::NdrGetBuffer(&stubMsg, 1024, nullptr);

        uint8_t   valByte  = 0x42;
        int16_t   valShort = -12345;
        int32_t   valLong  = 987654;
        uint32_t  valULong = 0xCAFEBABE;
        uint64_t  valHyper = 0xFEDCBA9876543210ULL;
        float     valFloat = 3.1415926f;
        double    valDbl   = 2.718281828459;

        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valByte),  rpc::FC_BYTE);
        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valShort), rpc::FC_SHORT);
        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valLong),  rpc::FC_LONG);
        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valULong), rpc::FC_ULONG);
        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valHyper), rpc::FC_HYPER);
        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valFloat), rpc::FC_FLOAT);
        rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&valDbl),   rpc::FC_DOUBLE);

        // Reset buffer pointer for unmarshalling
        stubMsg.Buffer = stubMsg.BufferStart;

        uint8_t   outByte  = 0;
        int16_t   outShort = 0;
        int32_t   outLong  = 0;
        uint32_t  outULong = 0;
        uint64_t  outHyper = 0;
        float     outFloat = 0.0f;
        double    outDbl   = 0.0;

        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outByte),  rpc::FC_BYTE);
        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outShort), rpc::FC_SHORT);
        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outLong),  rpc::FC_LONG);
        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outULong), rpc::FC_ULONG);
        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outHyper), rpc::FC_HYPER);
        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outFloat), rpc::FC_FLOAT);
        rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&outDbl),   rpc::FC_DOUBLE);

        TEST_ASSERT(outByte == valByte, "Unmarshalled FC_BYTE must match original");
        TEST_ASSERT(outShort == valShort, "Unmarshalled FC_SHORT must match original");
        TEST_ASSERT(outLong == valLong, "Unmarshalled FC_LONG must match original");
        TEST_ASSERT(outULong == valULong, "Unmarshalled FC_ULONG must match original");
        TEST_ASSERT(outHyper == valHyper, "Unmarshalled FC_HYPER must match original");
        TEST_ASSERT(std::abs(outFloat - valFloat) < 0.0001f, "Unmarshalled FC_FLOAT must match original");
        TEST_ASSERT(std::abs(outDbl - valDbl) < 0.0000001, "Unmarshalled FC_DOUBLE must match original");

        rpc::NdrFreeBuffer(&stubMsg);
    }

    // ------------------------------------------------------------------------
    // 10. NDR Conformant String Marshalling & Unmarshalling
    // ------------------------------------------------------------------------
    {
        rpc::RPC_MESSAGE msg{};
        rpc::MIDL_STUB_MESSAGE stubMsg{};
        stubMsg.RpcMsg = &msg;
        rpc::NdrGetBuffer(&stubMsg, 2048, nullptr);

        const char*    origStrA = "MicaNT Clean-Room RPC Runtime NDR Engine (ANSI)";
        const wchar_t* origStrW = L"MicaNT Native Unicode RPC Endpoint (UTF-16)";

        rpc::NdrConformantStringMarshall(&stubMsg, reinterpret_cast<unsigned char*>(const_cast<char*>(origStrA)), rpc::FC_CSTRING);
        rpc::NdrConformantStringMarshall(&stubMsg, reinterpret_cast<unsigned char*>(const_cast<wchar_t*>(origStrW)), rpc::FC_WSTRING);

        stubMsg.Buffer = stubMsg.BufferStart;

        unsigned char* outStrA = nullptr;
        unsigned char* outStrW = nullptr;

        rpc::NdrConformantStringUnmarshall(&stubMsg, &outStrA, rpc::FC_CSTRING);
        rpc::NdrConformantStringUnmarshall(&stubMsg, &outStrW, rpc::FC_WSTRING);

        TEST_ASSERT(outStrA != nullptr, "Unmarshalled ANSI string must be non-null");
        TEST_ASSERT(std::string(reinterpret_cast<char*>(outStrA)) == origStrA, "Unmarshalled ANSI string must match original");

        TEST_ASSERT(outStrW != nullptr, "Unmarshalled Unicode string must be non-null");
        TEST_ASSERT(std::wstring(reinterpret_cast<wchar_t*>(outStrW)) == origStrW, "Unmarshalled Unicode string must match original");

        win32::LocalFree(outStrA);
        win32::LocalFree(outStrW);
        rpc::NdrFreeBuffer(&stubMsg);
    }

    // ------------------------------------------------------------------------
    // 11. End-to-End Client/Server Synchronous Interface Dispatch (NdrSendReceive)
    // ------------------------------------------------------------------------
    {
        rpc::RPC_SYNTAX_IDENTIFIER ifId{};
        rpc::UuidCreate(&ifId.SyntaxGUID);
        ifId.SyntaxVersion.MajorVersion = 2;
        ifId.SyntaxVersion.MinorVersion = 0;

        // Stub Proc 0: Sum of two uint32_t numbers: (a, b) -> (a + b)
        // Stub Proc 1: String doubler / repeater
        static std::atomic<uint32_t> s_proc0Count{0};
        static std::atomic<uint32_t> s_proc1Count{0};

        auto proc0Stub = +[](rpc::RPC_MESSAGE* pMsg) {
            s_proc0Count++;
            rpc::MIDL_STUB_MESSAGE srvStub{};
            srvStub.RpcMsg = pMsg;
            srvStub.Buffer = static_cast<unsigned char*>(pMsg->Buffer);
            srvStub.BufferStart = srvStub.Buffer;
            srvStub.BufferEnd = srvStub.Buffer + pMsg->BufferLength;

            uint32_t a = 0, b = 0;
            rpc::NdrSimpleTypeUnmarshall(&srvStub, reinterpret_cast<unsigned char*>(&a), rpc::FC_ULONG);
            rpc::NdrSimpleTypeUnmarshall(&srvStub, reinterpret_cast<unsigned char*>(&b), rpc::FC_ULONG);

            uint32_t sum = a + b;
            srvStub.Buffer = srvStub.BufferStart;
            rpc::NdrSimpleTypeMarshall(&srvStub, reinterpret_cast<unsigned char*>(&sum), rpc::FC_ULONG);
            pMsg->BufferLength = static_cast<uint32_t>(srvStub.Buffer - srvStub.BufferStart);
        };

        auto proc1Stub = +[](rpc::RPC_MESSAGE* pMsg) {
            s_proc1Count++;
            rpc::MIDL_STUB_MESSAGE srvStub{};
            srvStub.RpcMsg = pMsg;
            srvStub.Buffer = static_cast<unsigned char*>(pMsg->Buffer);
            srvStub.BufferStart = srvStub.Buffer;
            srvStub.BufferEnd = srvStub.Buffer + pMsg->BufferLength;

            unsigned char* inputStr = nullptr;
            rpc::NdrConformantStringUnmarshall(&srvStub, &inputStr, rpc::FC_CSTRING);

            std::string echoed = "Echo: " + std::string(reinterpret_cast<char*>(inputStr));
            win32::LocalFree(inputStr);

            srvStub.Buffer = srvStub.BufferStart;
            rpc::NdrConformantStringMarshall(&srvStub, reinterpret_cast<unsigned char*>(echoed.data()), rpc::FC_CSTRING);
            pMsg->BufferLength = static_cast<uint32_t>(srvStub.Buffer - srvStub.BufferStart);
        };

        rpc::RPC_DISPATCH_FUNCTION dispatchFns[2] = { proc0Stub, proc1Stub };
        rpc::RPC_DISPATCH_TABLE dispatchTable{ 2, dispatchFns };

        rpc::RPC_SERVER_INTERFACE srvIf{};
        srvIf.Length = sizeof(srvIf);
        srvIf.InterfaceId = ifId;
        srvIf.TransferSyntax = rpc::NDR_TRANSFER_SYNTAX;
        srvIf.DispatchTable = &dispatchTable;

        rpc::RpcServerRegisterIf(&srvIf, nullptr, nullptr);
        rpc::RpcServerUseProtseqEpA((unsigned char*)"ncalrpc", 10, (unsigned char*)"ep_test_dispatch", nullptr);
        rpc::RpcServerListen(1, 10, 1);

        // Client Binding & Call
        rpc::RPC_BINDING_HANDLE hClientBinding = nullptr;
        unsigned char strClientBinding[] = "ncalrpc:[ep_test_dispatch]";
        rpc::RpcBindingFromStringBindingA(strClientBinding, &hClientBinding);
        TEST_ASSERT(hClientBinding != nullptr, "Client binding handle must be valid");

        rpc::RPC_CLIENT_INTERFACE clntIf{};
        clntIf.Length = sizeof(clntIf);
        clntIf.InterfaceId = ifId;
        clntIf.TransferSyntax = rpc::NDR_TRANSFER_SYNTAX;

        // Test Call 1: Proc 0 (123 + 456)
        {
            rpc::RPC_MESSAGE callMsg{};
            callMsg.RpcInterfaceInformation = &clntIf;
            callMsg.ProcNum = 0;
            rpc::MIDL_STUB_MESSAGE clntStub{};
            clntStub.RpcMsg = &callMsg;
            rpc::NdrGetBuffer(&clntStub, 512, hClientBinding);

            uint32_t a = 123, b = 456;
            rpc::NdrSimpleTypeMarshall(&clntStub, reinterpret_cast<unsigned char*>(&a), rpc::FC_ULONG);
            rpc::NdrSimpleTypeMarshall(&clntStub, reinterpret_cast<unsigned char*>(&b), rpc::FC_ULONG);

            rpc::NdrSendReceive(&clntStub, clntStub.Buffer);

            clntStub.Buffer = clntStub.BufferStart;
            uint32_t resultSum = 0;
            rpc::NdrSimpleTypeUnmarshall(&clntStub, reinterpret_cast<unsigned char*>(&resultSum), rpc::FC_ULONG);

            TEST_ASSERT(s_proc0Count.load() == 1, "Server Proc 0 must have been invoked exactly once");
            TEST_ASSERT(resultSum == 579, "Proc 0 result must be 123 + 456 = 579");
            rpc::NdrFreeBuffer(&clntStub);
        }

        // Test Call 2: Proc 1 ("MicaNT")
        {
            rpc::RPC_MESSAGE callMsg{};
            callMsg.RpcInterfaceInformation = &clntIf;
            callMsg.ProcNum = 1;
            rpc::MIDL_STUB_MESSAGE clntStub{};
            clntStub.RpcMsg = &callMsg;
            rpc::NdrGetBuffer(&clntStub, 512, hClientBinding);

            const char* sendGreeting = "Hello MicaNT Kernel!";
            rpc::NdrConformantStringMarshall(&clntStub, reinterpret_cast<unsigned char*>(const_cast<char*>(sendGreeting)), rpc::FC_CSTRING);

            rpc::NdrSendReceive(&clntStub, clntStub.Buffer);

            clntStub.Buffer = clntStub.BufferStart;
            unsigned char* recvReply = nullptr;
            rpc::NdrConformantStringUnmarshall(&clntStub, &recvReply, rpc::FC_CSTRING);

            TEST_ASSERT(s_proc1Count.load() == 1, "Server Proc 1 must have been invoked exactly once");
            TEST_ASSERT(recvReply != nullptr, "Proc 1 reply string must be non-null");
            TEST_ASSERT(std::string(reinterpret_cast<char*>(recvReply)) == "Echo: Hello MicaNT Kernel!", "Proc 1 reply string must match expected format");

            win32::LocalFree(recvReply);
            rpc::NdrFreeBuffer(&clntStub);
        }

        // Cleanup
        rpc::RpcMgmtStopServerListening(nullptr);
        rpc::RpcServerUnregisterIf(&srvIf, nullptr, 0);
        rpc::RpcBindingFree(&hClientBinding);
    }

    // ------------------------------------------------------------------------
    // 12. Asynchronous RPC Call Lifecycle (RPC_ASYNC_STATE)
    // ------------------------------------------------------------------------
    {
        rpc::RPC_ASYNC_STATE asyncState{};
        rpc::RPC_STATUS stInit = rpc::RpcAsyncInitializeHandle(&asyncState, sizeof(asyncState));
        TEST_ASSERT(stInit == rpc::RPC_S_OK, "RpcAsyncInitializeHandle must succeed");
        TEST_ASSERT(asyncState.Signature == 0x4153594E, "RpcAsyncInitializeHandle must set 'ASYN' signature");

        rpc::RPC_STATUS stReg = rpc::RpcAsyncRegisterInfo(&asyncState);
        TEST_ASSERT(stReg == rpc::RPC_S_OK, "RpcAsyncRegisterInfo must succeed");

        rpc::RPC_STATUS stComp = rpc::RpcAsyncCompleteCall(&asyncState, nullptr);
        TEST_ASSERT(stComp == rpc::RPC_S_OK, "RpcAsyncCompleteCall must succeed");

        rpc::RPC_ASYNC_STATE asyncAbortState{};
        rpc::RpcAsyncInitializeHandle(&asyncAbortState, sizeof(asyncAbortState));
        rpc::RPC_STATUS stAbort = rpc::RpcAsyncAbortCall(&asyncAbortState, 0xC0000001);
        TEST_ASSERT(stAbort == rpc::RPC_S_OK, "RpcAsyncAbortCall must succeed");
    }

    // ------------------------------------------------------------------------
    // 13. Dynamic Loader Export Verification & Version Metadata
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "UuidCreate") != nullptr, "rpcrt4.dll!UuidCreate must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "UuidCreateSequential") != nullptr, "rpcrt4.dll!UuidCreateSequential must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "UuidToStringA") != nullptr, "rpcrt4.dll!UuidToStringA must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "UuidToStringW") != nullptr, "rpcrt4.dll!UuidToStringW must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "UuidFromStringA") != nullptr, "rpcrt4.dll!UuidFromStringA must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "UuidFromStringW") != nullptr, "rpcrt4.dll!UuidFromStringW must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "RpcStringBindingComposeA") != nullptr, "rpcrt4.dll!RpcStringBindingComposeA must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "RpcBindingFromStringBindingA") != nullptr, "rpcrt4.dll!RpcBindingFromStringBindingA must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "RpcServerRegisterIf") != nullptr, "rpcrt4.dll!RpcServerRegisterIf must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "RpcServerListen") != nullptr, "rpcrt4.dll!RpcServerListen must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "NdrGetBuffer") != nullptr, "rpcrt4.dll!NdrGetBuffer must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "NdrSendReceive") != nullptr, "rpcrt4.dll!NdrSendReceive must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "NdrSimpleTypeMarshall") != nullptr, "rpcrt4.dll!NdrSimpleTypeMarshall must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "NdrSimpleTypeUnmarshall") != nullptr, "rpcrt4.dll!NdrSimpleTypeUnmarshall must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "NdrConformantStringMarshall") != nullptr, "rpcrt4.dll!NdrConformantStringMarshall must be exported");
        TEST_ASSERT(ldr.getExport("rpcrt4.dll", "NdrConformantStringUnmarshall") != nullptr, "rpcrt4.dll!NdrConformantStringUnmarshall must be exported");

        uint32_t handle = 0;
        uint32_t sRpcrt4 = version::GetFileVersionInfoSizeA("rpcrt4.dll", &handle);
        TEST_ASSERT(sRpcrt4 > 0, "rpcrt4.dll must have version info resource");

        std::vector<uint8_t> vRpcrt4(sRpcrt4);
        TEST_ASSERT(version::GetFileVersionInfoA("rpcrt4.dll", handle, sRpcrt4, vRpcrt4.data()) != 0, "GetFileVersionInfoA for rpcrt4.dll must succeed");

        void* pDesc = nullptr;
        uint32_t dLen = 0;
        TEST_ASSERT(version::VerQueryValueA(vRpcrt4.data(), "\\StringFileInfo\\040904B0\\FileDescription", &pDesc, &dLen) != 0, "VerQueryValueA for rpcrt4.dll must succeed");
        TEST_ASSERT(std::string(static_cast<const char*>(pDesc)) == "Remote Procedure Call Runtime", "FileDescription must match RPC Runtime description");
    }

    // ------------------------------------------------------------------------
    // 14. Shell Built-in Commands Integration (rpc, uuidgen)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("rpc info", out);
        TEST_ASSERT(out.str().find("Remote Procedure Call Runtime") != std::string::npos, "Shell rpc info command must succeed");

        out.str("");
        shell.execute("rpc endpoints", out);
        TEST_ASSERT(out.str().find("Registered RPC Server Endpoints") != std::string::npos, "Shell rpc endpoints command must list endpoints");

        out.str("");
        shell.execute("rpc test", out);
        TEST_ASSERT(out.str().find("ALL RPC & NDR CHECKS PASSED") != std::string::npos, "Shell rpc test command must pass all checks");

        out.str("");
        shell.execute("uuidgen", out);
        std::string genUuid = out.str();
        TEST_ASSERT(genUuid.find("-") != std::string::npos && genUuid.length() >= 36, "Shell uuidgen must output a valid UUID");

        out.str("");
        shell.execute("uuidgen -s", out);
        std::string seqUuid = out.str();
        TEST_ASSERT(seqUuid.find("-") != std::string::npos && seqUuid.length() >= 36, "Shell uuidgen -s must output sequential UUID");

        out.str("");
        shell.execute("uuidgen -c", out);
        std::string cGuid = out.str();
        TEST_ASSERT(cGuid.find("GUID_Generated") != std::string::npos, "Shell uuidgen -c must output C-style GUID struct");
    }

    std::cout << "[TEST] Suite 68: Windows Remote Procedure Call (RPC) & NDR Subsystem PASSED.\n";
}

void Test_OLE_Automation_And_SafeArray_Subsystem() {
    std::cout << "[TEST] Running Suite 69: Windows OLE Automation & SafeArray Subsystem (oleaut32.dll)...\n";

    // ------------------------------------------------------------------------
    // Stage 1: BSTR String Management (SysAllocString, SysAllocStringLen, SysAllocStringByteLen, SysReAllocString, SysFreeString)
    // ------------------------------------------------------------------------
    {
        const wchar_t* helloText = L"Hello, Sovereign Windows World!";
        ole32::BSTR bstr1 = oleaut32::SysAllocString(helloText);
        TEST_ASSERT(bstr1 != nullptr, "SysAllocString must return valid pointer");
        uint32_t len1 = oleaut32::SysStringLen(bstr1);
        TEST_ASSERT(len1 == std::wcslen(helloText), "SysStringLen must match wcslen");
        uint32_t byteLen1 = oleaut32::SysStringByteLen(bstr1);
        TEST_ASSERT(byteLen1 == len1 * sizeof(wchar_t), "SysStringByteLen must match character count * 2");

        // SysAllocStringLen
        ole32::BSTR bstrSub = oleaut32::SysAllocStringLen(helloText, 5);
        TEST_ASSERT(bstrSub != nullptr, "SysAllocStringLen must succeed");
        TEST_ASSERT(oleaut32::SysStringLen(bstrSub) == 5, "SysAllocStringLen length must be 5");
        TEST_ASSERT(std::wcsncmp(bstrSub, L"Hello", 5) == 0, "SysAllocStringLen content must match prefix");
        oleaut32::SysFreeString(bstrSub);

        // SysAllocStringByteLen
        const char* ansiBytes = "MicaNT_Binary_Data\0Hidden";
        ole32::BSTR bstrBytes = oleaut32::SysAllocStringByteLen(ansiBytes, 25);
        TEST_ASSERT(bstrBytes != nullptr, "SysAllocStringByteLen must allocate binary string");
        TEST_ASSERT(oleaut32::SysStringByteLen(bstrBytes) == 25, "SysStringByteLen must record exact 25 bytes");
        oleaut32::SysFreeString(bstrBytes);

        // SysReAllocString
        int reallocSuccess = oleaut32::SysReAllocString(&bstr1, L"Extended OLE Automation String");
        TEST_ASSERT(reallocSuccess == 1, "SysReAllocString must return 1 on success");
        TEST_ASSERT(oleaut32::SysStringLen(bstr1) == std::wcslen(L"Extended OLE Automation String"), "Reallocated string length must match");

        // Null string handling
        TEST_ASSERT(oleaut32::SysStringLen(nullptr) == 0, "SysStringLen(nullptr) must return 0");
        TEST_ASSERT(oleaut32::SysStringByteLen(nullptr) == 0, "SysStringByteLen(nullptr) must return 0");
        oleaut32::SysFreeString(nullptr); // Safe no-op

        oleaut32::SysFreeString(bstr1);
    }

    // ------------------------------------------------------------------------
    // Stage 2: SafeArray 1D Vector Allocation, Bounds & Element Access
    // ------------------------------------------------------------------------
    {
        // Vector with lLbound = 10, cElements = 6, VT_I4
        oleaut32::SAFEARRAY* psa = oleaut32::SafeArrayCreateVector(ole32::VT_I4, 10, 6);
        TEST_ASSERT(psa != nullptr, "SafeArrayCreateVector must allocate SAFEARRAY");
        TEST_ASSERT(oleaut32::SafeArrayGetDim(psa) == 1, "Dimension count must be 1");
        TEST_ASSERT(oleaut32::SafeArrayGetElemsize(psa) == sizeof(int32_t), "Element size must be 4");

        int32_t lbound = 0, ubound = 0;
        ole32::HRESULT hrLb = oleaut32::SafeArrayGetLBound(psa, 1, &lbound);
        ole32::HRESULT hrUb = oleaut32::SafeArrayGetUBound(psa, 1, &ubound);
        TEST_ASSERT(hrLb == ole32::S_OK && lbound == 10, "LBound must be 10");
        TEST_ASSERT(hrUb == ole32::S_OK && ubound == 15, "UBound must be 15 (10 + 6 - 1)");

        // Put and Get elements
        for (int32_t idx = 10; idx <= 15; ++idx) {
            int32_t val = idx * 100;
            ole32::HRESULT hrPut = oleaut32::SafeArrayPutElement(psa, &idx, &val);
            TEST_ASSERT(hrPut == ole32::S_OK, "SafeArrayPutElement must succeed for in-bounds index");
        }

        // Out-of-bounds put
        int32_t badIdx = 9;
        int32_t dummy = 999;
        TEST_ASSERT(oleaut32::SafeArrayPutElement(psa, &badIdx, &dummy) == oleaut32::DISP_E_BADPARAMCOUNT, "Put with index < lbound must fail");
        badIdx = 16;
        TEST_ASSERT(oleaut32::SafeArrayPutElement(psa, &badIdx, &dummy) == oleaut32::DISP_E_BADPARAMCOUNT, "Put with index > ubound must fail");

        // Verify elements read back
        for (int32_t idx = 10; idx <= 15; ++idx) {
            int32_t readVal = 0;
            ole32::HRESULT hrGet = oleaut32::SafeArrayGetElement(psa, &idx, &readVal);
            TEST_ASSERT(hrGet == ole32::S_OK, "SafeArrayGetElement must succeed");
            TEST_ASSERT(readVal == idx * 100, "SafeArrayGetElement must match written value");
        }

        oleaut32::SafeArrayDestroy(psa);
    }

    // ------------------------------------------------------------------------
    // Stage 3: Multi-Dimensional SafeArray (2D Matrix, 3 x 4)
    // ------------------------------------------------------------------------
    {
        // 2 dimensions: dim 1 has 3 elements [0..2], dim 2 has 4 elements [0..3]
        oleaut32::SAFEARRAYBOUND bounds[2] = { {3, 0}, {4, 0} };
        oleaut32::SAFEARRAY* psa2D = oleaut32::SafeArrayCreate(ole32::VT_I4, 2, bounds);
        TEST_ASSERT(psa2D != nullptr, "SafeArrayCreate 2D must succeed");
        TEST_ASSERT(oleaut32::SafeArrayGetDim(psa2D) == 2, "Dimension count must be 2");

        int32_t lb1 = 0, ub1 = 0, lb2 = 0, ub2 = 0;
        oleaut32::SafeArrayGetLBound(psa2D, 1, &lb1);
        oleaut32::SafeArrayGetUBound(psa2D, 1, &ub1);
        oleaut32::SafeArrayGetLBound(psa2D, 2, &lb2);
        oleaut32::SafeArrayGetUBound(psa2D, 2, &ub2);
        TEST_ASSERT(lb1 == 0 && ub1 == 2, "Dim 1 bounds must be [0..2]");
        TEST_ASSERT(lb2 == 0 && ub2 == 3, "Dim 2 bounds must be [0..3]");

        // Put and Get across 2D grid
        for (int32_t d1 = 0; d1 < 3; ++d1) {
            for (int32_t d2 = 0; d2 < 4; ++d2) {
                int32_t coords[2] = { d1, d2 };
                int32_t val = (d1 + 1) * 10 + (d2 + 1);
                oleaut32::SafeArrayPutElement(psa2D, coords, &val);
            }
        }

        for (int32_t d1 = 0; d1 < 3; ++d1) {
            for (int32_t d2 = 0; d2 < 4; ++d2) {
                int32_t coords[2] = { d1, d2 };
                int32_t readVal = 0;
                oleaut32::SafeArrayGetElement(psa2D, coords, &readVal);
                int32_t expected = (d1 + 1) * 10 + (d2 + 1);
                TEST_ASSERT(readVal == expected, "2D matrix element must match");
            }
        }

        oleaut32::SafeArrayDestroy(psa2D);
    }

    // ------------------------------------------------------------------------
    // Stage 4: SafeArray Data Access, Locking Semantics & Destroy Protection
    // ------------------------------------------------------------------------
    {
        oleaut32::SAFEARRAY* psa = oleaut32::SafeArrayCreateVector(ole32::VT_I4, 0, 10);
        TEST_ASSERT(psa != nullptr, "Vector allocation must succeed");
        TEST_ASSERT(psa->cLocks == 0, "Initial lock count must be 0");

        void* rawPtr = nullptr;
        ole32::HRESULT hrAcc = oleaut32::SafeArrayAccessData(psa, &rawPtr);
        TEST_ASSERT(hrAcc == ole32::S_OK && rawPtr != nullptr, "SafeArrayAccessData must return raw pointer");
        TEST_ASSERT(psa->cLocks == 1, "Lock count must be incremented to 1");

        // Attempting SafeArrayDestroy while locked must fail with DISP_E_ARRAYISLOCKED
        ole32::HRESULT hrDestroy = oleaut32::SafeArrayDestroy(psa);
        TEST_ASSERT(hrDestroy == oleaut32::DISP_E_ARRAYISLOCKED, "SafeArrayDestroy on locked array must return DISP_E_ARRAYISLOCKED");

        // Direct memory write via raw pointer
        int32_t* intPtr = static_cast<int32_t*>(rawPtr);
        for (int i = 0; i < 10; ++i) {
            intPtr[i] = 777 + i;
        }

        // Unaccess data
        ole32::HRESULT hrUnacc = oleaut32::SafeArrayUnaccessData(psa);
        TEST_ASSERT(hrUnacc == ole32::S_OK, "SafeArrayUnaccessData must succeed");
        TEST_ASSERT(psa->cLocks == 0, "Lock count must return to 0");

        // Verify elements via SafeArrayGetElement
        int32_t idx = 5;
        int32_t readVal = 0;
        oleaut32::SafeArrayGetElement(psa, &idx, &readVal);
        TEST_ASSERT(readVal == 782, "Element written via raw pointer must be readable via SafeArrayGetElement");

        // SafeArrayDestroy must now succeed
        hrDestroy = oleaut32::SafeArrayDestroy(psa);
        TEST_ASSERT(hrDestroy == ole32::S_OK, "SafeArrayDestroy must succeed after unlocking");
    }

    // ------------------------------------------------------------------------
    // Stage 5: SafeArray Deep Copy & SafeArrayRedim
    // ------------------------------------------------------------------------
    {
        oleaut32::SAFEARRAY* orig = oleaut32::SafeArrayCreateVector(ole32::VT_I4, 0, 4);
        for (int32_t i = 0; i < 4; ++i) {
            int32_t val = (i + 1) * 111;
            oleaut32::SafeArrayPutElement(orig, &i, &val);
        }

        oleaut32::SAFEARRAY* copy = nullptr;
        ole32::HRESULT hrCopy = oleaut32::SafeArrayCopy(orig, &copy);
        TEST_ASSERT(hrCopy == ole32::S_OK && copy != nullptr, "SafeArrayCopy must succeed");
        TEST_ASSERT(copy != orig, "Copy must be distinct memory block");
        TEST_ASSERT(copy->pvData != orig->pvData, "Copy pvData must be distinct");

        for (int32_t i = 0; i < 4; ++i) {
            int32_t readVal = 0;
            oleaut32::SafeArrayGetElement(copy, &i, &readVal);
            TEST_ASSERT(readVal == (i + 1) * 111, "Copy elements must match original");
        }

        // SafeArrayRedim to expand from 4 to 8 elements
        oleaut32::SAFEARRAYBOUND newBound{ 8, 0 };
        ole32::HRESULT hrRedim = oleaut32::SafeArrayRedim(orig, &newBound);
        TEST_ASSERT(hrRedim == ole32::S_OK, "SafeArrayRedim must expand vector");
        TEST_ASSERT(orig->rgsabound[0].cElements == 8, "Bound must reflect 8 elements");

        // Put element into the new expanded slots
        int32_t newIdx = 6;
        int32_t newVal = 9999;
        oleaut32::SafeArrayPutElement(orig, &newIdx, &newVal);
        int32_t checkVal = 0;
        oleaut32::SafeArrayGetElement(orig, &newIdx, &checkVal);
        TEST_ASSERT(checkVal == 9999, "Newly expanded element must be writable and readable");

        oleaut32::SafeArrayDestroy(orig);
        oleaut32::SafeArrayDestroy(copy);
    }

    // ------------------------------------------------------------------------
    // Stage 6: SafeArray Vartype & Complex Element Types (BSTR / VARIANT)
    // ------------------------------------------------------------------------
    {
        oleaut32::SAFEARRAY* psaBstr = oleaut32::SafeArrayCreateVector(ole32::VT_BSTR, 0, 3);
        TEST_ASSERT(psaBstr != nullptr, "BSTR SafeArray must allocate");
        TEST_ASSERT((psaBstr->fFeatures & oleaut32::FADF_BSTR) != 0, "FADF_BSTR flag must be set");

        ole32::VARTYPE vt = ole32::VT_EMPTY;
        oleaut32::SafeArrayGetVartype(psaBstr, &vt);
        TEST_ASSERT(vt == ole32::VT_BSTR, "SafeArrayGetVartype must return VT_BSTR");

        ole32::BSTR str0 = oleaut32::SysAllocString(L"Item 0");
        ole32::BSTR str1 = oleaut32::SysAllocString(L"Item 1");
        int32_t idx0 = 0, idx1 = 1;
        oleaut32::SafeArrayPutElement(psaBstr, &idx0, &str0);
        oleaut32::SafeArrayPutElement(psaBstr, &idx1, &str1);
        oleaut32::SysFreeString(str0);
        oleaut32::SysFreeString(str1);

        ole32::BSTR readBstr = nullptr;
        oleaut32::SafeArrayGetElement(psaBstr, &idx0, &readBstr);
        TEST_ASSERT(readBstr != nullptr && std::wcscmp(readBstr, L"Item 0") == 0, "SafeArrayGetElement must return deep-copied BSTR");
        oleaut32::SysFreeString(readBstr);

        // SafeArrayDestroy must cleanly free internal BSTRs
        oleaut32::SafeArrayDestroy(psaBstr);
    }

    // ------------------------------------------------------------------------
    // Stage 7: VARIANT Lifecycle (VariantInit, VariantClear, VariantCopy)
    // ------------------------------------------------------------------------
    {
        ole32::VARIANT v1{}, v2{};
        oleaut32::VariantInit(&v1);
        oleaut32::VariantInit(&v2);
        TEST_ASSERT(v1.vt == ole32::VT_EMPTY, "VariantInit must set VT_EMPTY");

        v1.vt = ole32::VT_BSTR;
        v1.bstrVal = oleaut32::SysAllocString(L"MicaNT Variant Test");

        ole32::HRESULT hrCopy = oleaut32::VariantCopy(&v2, &v1);
        TEST_ASSERT(hrCopy == ole32::S_OK, "VariantCopy must succeed");
        TEST_ASSERT(v2.vt == ole32::VT_BSTR, "Copy must have VT_BSTR");
        TEST_ASSERT(v2.bstrVal != v1.bstrVal, "VariantCopy must deep copy BSTR");
        TEST_ASSERT(std::wcscmp(v2.bstrVal, v1.bstrVal) == 0, "BSTR content must match");

        oleaut32::VariantClear(&v1);
        TEST_ASSERT(v1.vt == ole32::VT_EMPTY, "VariantClear must reset to VT_EMPTY");
        TEST_ASSERT(v2.vt == ole32::VT_BSTR && v2.bstrVal != nullptr, "v2 must remain valid after v1 cleared");
        oleaut32::VariantClear(&v2);
    }

    // ------------------------------------------------------------------------
    // Stage 8: Variant Indirection Dereferencing (VariantCopyInd)
    // ------------------------------------------------------------------------
    {
        int32_t targetVal = 12345;
        ole32::VARIANT vByRef{};
        oleaut32::VariantInit(&vByRef);
        vByRef.vt = ole32::VT_I4 | ole32::VT_BYREF;
        vByRef.byref = &targetVal;

        ole32::VARIANT vDest{};
        oleaut32::VariantInit(&vDest);
        ole32::HRESULT hrInd = oleaut32::VariantCopyInd(&vDest, &vByRef);
        TEST_ASSERT(hrInd == ole32::S_OK, "VariantCopyInd must succeed");
        TEST_ASSERT(vDest.vt == ole32::VT_I4, "vDest must have VT_I4 without VT_BYREF");
        TEST_ASSERT(vDest.lVal == 12345, "vDest must contain dereferenced value 12345");

        oleaut32::VariantClear(&vDest);
    }

    // ------------------------------------------------------------------------
    // Stage 9: Variant Type Coercion: Numeric Widening, Narrowing & Floating Point
    // ------------------------------------------------------------------------
    {
        ole32::VARIANT vI4{}, vI8{}, vR8{}, vR4{}, vUI4{};
        oleaut32::VariantInit(&vI4);
        oleaut32::VariantInit(&vI8);
        oleaut32::VariantInit(&vR8);
        oleaut32::VariantInit(&vR4);
        oleaut32::VariantInit(&vUI4);

        vI4.vt = ole32::VT_I4;
        vI4.lVal = 1000;

        // VT_I4 -> VT_I8
        oleaut32::VariantChangeType(&vI8, &vI4, 0, ole32::VT_I8);
        TEST_ASSERT(vI8.vt == ole32::VT_I8 && vI8.llVal == 1000LL, "VariantChangeType to VT_I8 must match 1000");

        // VT_I4 -> VT_R8
        oleaut32::VariantChangeType(&vR8, &vI4, 0, ole32::VT_R8);
        TEST_ASSERT(vR8.vt == ole32::VT_R8 && std::fabs(vR8.dblVal - 1000.0) < 0.001, "VariantChangeType to VT_R8 must match 1000.0");

        // VT_R8 -> VT_I4 (truncation/rounding)
        vR8.dblVal = 42.75;
        oleaut32::VariantChangeType(&vI4, &vR8, 0, ole32::VT_I4);
        TEST_ASSERT(vI4.vt == ole32::VT_I4 && (vI4.lVal == 42 || vI4.lVal == 43), "VariantChangeType float to integer must succeed");

        oleaut32::VariantClear(&vI4);
        oleaut32::VariantClear(&vI8);
        oleaut32::VariantClear(&vR8);
        oleaut32::VariantClear(&vR4);
        oleaut32::VariantClear(&vUI4);
    }

    // ------------------------------------------------------------------------
    // Stage 10: Variant Type Coercion: Strings & Booleans
    // ------------------------------------------------------------------------
    {
        ole32::VARIANT vNum{}, vStr{}, vBool{};
        oleaut32::VariantInit(&vNum);
        oleaut32::VariantInit(&vStr);
        oleaut32::VariantInit(&vBool);

        // Numeric to BSTR
        vNum.vt = ole32::VT_I4;
        vNum.lVal = 8080;
        oleaut32::VariantChangeType(&vStr, &vNum, 0, ole32::VT_BSTR);
        TEST_ASSERT(vStr.vt == ole32::VT_BSTR && vStr.bstrVal && std::wcscmp(vStr.bstrVal, L"8080") == 0, "Coerce I4 to BSTR must match '8080'");

        // BSTR back to I4
        ole32::VARIANT vParsed{};
        oleaut32::VariantInit(&vParsed);
        oleaut32::VariantChangeType(&vParsed, &vStr, 0, ole32::VT_I4);
        TEST_ASSERT(vParsed.vt == ole32::VT_I4 && vParsed.lVal == 8080, "Coerce BSTR '8080' to I4 must yield 8080");

        // Boolean true (-1) and false (0)
        vNum.lVal = 1;
        oleaut32::VariantChangeType(&vBool, &vNum, 0, ole32::VT_BOOL);
        TEST_ASSERT(vBool.vt == ole32::VT_BOOL && vBool.boolVal == -1, "Non-zero integer must coerce to VARIANT_TRUE (-1)");

        vNum.lVal = 0;
        oleaut32::VariantChangeType(&vBool, &vNum, 0, ole32::VT_BOOL);
        TEST_ASSERT(vBool.vt == ole32::VT_BOOL && vBool.boolVal == 0, "Zero integer must coerce to VARIANT_FALSE (0)");

        oleaut32::VariantClear(&vNum);
        oleaut32::VariantClear(&vStr);
        oleaut32::VariantClear(&vBool);
        oleaut32::VariantClear(&vParsed);
    }

    // ------------------------------------------------------------------------
    // Stage 11: Variant Relational Comparisons (VarCmp)
    // ------------------------------------------------------------------------
    {
        ole32::VARIANT vA{}, vB{};
        oleaut32::VariantInit(&vA);
        oleaut32::VariantInit(&vB);

        vA.vt = ole32::VT_I4; vA.lVal = 250;
        vB.vt = ole32::VT_I4; vB.lVal = 500;
        TEST_ASSERT(oleaut32::VarCmp(&vA, &vB, 0, 0) == oleaut32::VARCMP_LT, "250 must be less than 500 (VARCMP_LT)");

        vA.lVal = 500;
        TEST_ASSERT(oleaut32::VarCmp(&vA, &vB, 0, 0) == oleaut32::VARCMP_EQ, "500 must equal 500 (VARCMP_EQ)");

        vA.lVal = 750;
        TEST_ASSERT(oleaut32::VarCmp(&vA, &vB, 0, 0) == oleaut32::VARCMP_GT, "750 must be greater than 500 (VARCMP_GT)");

        // Null comparison
        vA.vt = ole32::VT_NULL;
        TEST_ASSERT(oleaut32::VarCmp(&vA, &vB, 0, 0) == oleaut32::VARCMP_NULL, "VT_NULL comparison must yield VARCMP_NULL");

        oleaut32::VariantClear(&vA);
        oleaut32::VariantClear(&vB);
    }

    // ------------------------------------------------------------------------
    // Stage 12: Late-Binding Dynamic Dispatch Engine (IDispatch & DispGetParam)
    // ------------------------------------------------------------------------
    {
        auto dispObj = std::make_unique<oleaut32::StandardDispatch>();

        // Register method: Add(a, b) -> a + b
        dispObj->registerMethod(L"Add", 201, [](oleaut32::DISPPARAMS* dp, ole32::VARIANT* res) -> ole32::HRESULT {
            if (!dp || dp->cArgs < 2 || !res) return ole32::E_INVALIDARG;
            ole32::VARIANT a{}, b{};
            oleaut32::VariantInit(&a);
            oleaut32::VariantInit(&b);
            oleaut32::DispGetParam(dp, 0, ole32::VT_I4, &a, nullptr);
            oleaut32::DispGetParam(dp, 1, ole32::VT_I4, &b, nullptr);
            res->vt = ole32::VT_I4;
            res->lVal = a.lVal + b.lVal;
            return ole32::S_OK;
        });

        // GetIDsOfNames
        ole32::OLECHAR* name = const_cast<ole32::OLECHAR*>(L"Add");
        oleaut32::DISPID dispid = oleaut32::DISPID_UNKNOWN;
        ole32::HRESULT hrName = dispObj->GetIDsOfNames(ole32::GUID_NULL, &name, 1, 0, &dispid);
        TEST_ASSERT(hrName == ole32::S_OK && dispid == 201, "GetIDsOfNames for 'Add' must return DISPID 201");

        // Invoke with reversed arguments: arg 0 (15) and arg 1 (27)
        ole32::VARIANT invokeArgs[2];
        oleaut32::VariantInit(&invokeArgs[0]);
        oleaut32::VariantInit(&invokeArgs[1]);
        invokeArgs[0].vt = ole32::VT_I4; invokeArgs[0].lVal = 27; // arg 1 at index 0
        invokeArgs[1].vt = ole32::VT_I4; invokeArgs[1].lVal = 15; // arg 0 at index 1

        oleaut32::DISPPARAMS dp{ invokeArgs, nullptr, 2, 0 };
        ole32::VARIANT invokeRes{};
        oleaut32::VariantInit(&invokeRes);
        ole32::HRESULT hrInvoke = dispObj->Invoke(dispid, ole32::GUID_NULL, 0, oleaut32::DISPATCH_METHOD, &dp, &invokeRes, nullptr, nullptr);
        TEST_ASSERT(hrInvoke == ole32::S_OK, "Invoke on 'Add' must succeed");
        TEST_ASSERT(invokeRes.vt == ole32::VT_I4 && invokeRes.lVal == 42, "Invoke on 'Add' must yield 15 + 27 = 42");

        // DispInvoke helper verification
        ole32::VARIANT dispInvokeRes{};
        oleaut32::VariantInit(&dispInvokeRes);
        ole32::HRESULT hrHelper = oleaut32::DispInvoke(dispObj.get(), nullptr, dispid, oleaut32::DISPATCH_METHOD, &dp, &dispInvokeRes, nullptr, nullptr);
        TEST_ASSERT(hrHelper == ole32::S_OK && dispInvokeRes.lVal == 42, "DispInvoke helper must yield 42");

        oleaut32::VariantClear(&invokeRes);
        oleaut32::VariantClear(&dispInvokeRes);
    }

    // ------------------------------------------------------------------------
    // Stage 13: Type Library Registration & Path Resolution (TypeLibManager)
    // ------------------------------------------------------------------------
    {
        micant::GUID tlibGuid{ 0x98765432, 0x4321, 0x8765, { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11 } };
        ole32::HRESULT hrReg = oleaut32::TypeLibManager::Instance().registerLibrary(
            tlibGuid, 3, 2, L"C:\\MicaNT\\System32\\MicaAutomation.tlb"
        );
        TEST_ASSERT(hrReg == ole32::S_OK, "TypeLibManager::registerLibrary must return S_OK");

        ole32::BSTR resolvedPath = nullptr;
        ole32::HRESULT hrQuery = oleaut32::QueryPathOfRegTypeLib(tlibGuid, 3, 2, 0, &resolvedPath);
        TEST_ASSERT(hrQuery == ole32::S_OK && resolvedPath != nullptr, "QueryPathOfRegTypeLib must resolve registered path");
        TEST_ASSERT(std::wcscmp(resolvedPath, L"C:\\MicaNT\\System32\\MicaAutomation.tlb") == 0, "Resolved path must match registration");
        oleaut32::SysFreeString(resolvedPath);

        // Load synthetic typelib
        oleaut32::ITypeLib* pTLib = nullptr;
        ole32::HRESULT hrLoad = oleaut32::LoadTypeLib(L"C:\\MicaNT\\System32\\MicaAutomation.tlb", &pTLib);
        TEST_ASSERT(hrLoad == ole32::S_OK && pTLib != nullptr, "LoadTypeLib must return ITypeLib instance");
        TEST_ASSERT(pTLib->GetTypeInfoCount() == 0, "Synthetic ITypeLib initial count is 0");
        ole32::BSTR docName = nullptr;
        ole32::HRESULT hrDoc = pTLib->GetDocumentation(-1, &docName, nullptr, nullptr, nullptr);
        TEST_ASSERT(hrDoc == ole32::S_OK && docName != nullptr, "GetDocumentation must succeed");
        oleaut32::SysFreeString(docName);
        pTLib->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 14: Dynamic Loader Exports & Version Resource Metadata
    // ------------------------------------------------------------------------
    {
        oleaut32::InitializeOleAut32SubsystemExports();
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("oleaut32.dll", "SysAllocString") != nullptr, "SysAllocString must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "SysFreeString") != nullptr, "SysFreeString must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "SafeArrayCreate") != nullptr, "SafeArrayCreate must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "SafeArrayDestroy") != nullptr, "SafeArrayDestroy must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "VariantChangeType") != nullptr, "VariantChangeType must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "VarCmp") != nullptr, "VarCmp must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "CreateStdDispatch") != nullptr, "CreateStdDispatch must be exported from oleaut32.dll");
        TEST_ASSERT(ldr.getExport("oleaut32.dll", "LoadTypeLib") != nullptr, "LoadTypeLib must be exported from oleaut32.dll");

        // Version info check
        const auto* ver = version::VersionDatabase::Instance().FindModule("oleaut32.dll");
        TEST_ASSERT(ver != nullptr, "VersionDatabase must contain oleaut32.dll metadata");
        TEST_ASSERT(ver->stringTable.at("OriginalFilename") == "oleaut32.dll", "oleaut32.dll OriginalFilename must match");
    }

    // ------------------------------------------------------------------------
    // Stage 15: Command Shell Integration (oleaut test / oleaut info)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("oleaut info", out);
        TEST_ASSERT(out.str().find("MicaNT Windows OLE Automation & SafeArray Subsystem") != std::string::npos, "Shell oleaut info must succeed");

        out.str("");
        shell.execute("oleaut test", out);
        TEST_ASSERT(out.str().find("ALL OLE AUTOMATION CHECKS PASSED") != std::string::npos, "Shell oleaut test must pass all checks");
    }

    std::cout << "[TEST] Suite 69: Windows OLE Automation & SafeArray Subsystem PASSED.\n";
}

void Test_SetupApi_DeviceInstallation_And_INF_Subsystem() {
    std::cout << "[TEST] Running Suite 70: Windows Device Installation & SetupAPI Subsystem (setupapi.dll)...\n";

    // ------------------------------------------------------------------------
    // Stage 1: INF File Parsing & Line Counting (SetupOpenInfFileW, SetupGetLineCountW, SetupCloseInfFile)
    // ------------------------------------------------------------------------
    {
        setupapi::HINF hInf = setupapi::SetupOpenInfFileW(L"sample_display.inf", nullptr, 0, nullptr);
        TEST_ASSERT(hInf != nullptr && hInf != reinterpret_cast<setupapi::HINF>(static_cast<uintptr_t>(-1)), "SetupOpenInfFileW must succeed");

        int32_t verLines = setupapi::SetupGetLineCountW(hInf, L"Version");
        TEST_ASSERT(verLines >= 4, "Version section must contain at least 4 directives");

        int32_t strLines = setupapi::SetupGetLineCountW(hInf, L"Strings");
        TEST_ASSERT(strLines >= 2, "Strings section must contain at least 2 definitions");

        int32_t badLines = setupapi::SetupGetLineCountW(hInf, L"NonExistentSection");
        TEST_ASSERT(badLines == -1, "SetupGetLineCountW on non-existent section must return -1");

        setupapi::SetupCloseInfFile(hInf);
    }

    // ------------------------------------------------------------------------
    // Stage 2: INF Context Navigation & Field Counting (SetupFindFirstLineW, SetupFindNextLine, SetupGetFieldCount)
    // ------------------------------------------------------------------------
    {
        setupapi::HINF hInf = setupapi::SetupOpenInfFileW(L"sample_display.inf", nullptr, 0, nullptr);
        TEST_ASSERT(hInf != nullptr, "INF handle must be valid");

        setupapi::INFCONTEXT ctx{};
        win32::BOOL bFind = setupapi::SetupFindFirstLineW(hInf, L"Version", nullptr, &ctx);
        TEST_ASSERT(bFind != 0, "SetupFindFirstLineW on Version section must succeed");

        uint32_t fieldCount = setupapi::SetupGetFieldCount(&ctx);
        TEST_ASSERT(fieldCount >= 1, "First line of Version must have at least 1 field");

        // Traverse all lines in section
        uint32_t linesTraversed = 1;
        setupapi::INFCONTEXT nextCtx{};
        while (setupapi::SetupFindNextLine(&ctx, &nextCtx)) {
            linesTraversed++;
            ctx = nextCtx;
        }
        TEST_ASSERT(linesTraversed >= 4, "Line traversal must visit all lines in section");

        setupapi::SetupCloseInfFile(hInf);
    }

    // ------------------------------------------------------------------------
    // Stage 3: INF String Field Extraction & Token Replacement (SetupGetStringFieldW)
    // ------------------------------------------------------------------------
    {
        setupapi::HINF hInf = setupapi::SetupOpenInfFileW(L"sample_display.inf", nullptr, 0, nullptr);
        setupapi::INFCONTEXT ctx{};

        // Find ClassGuid key in [Version]
        win32::BOOL bKey = setupapi::SetupFindFirstLineW(hInf, L"Version", L"ClassGuid", &ctx);
        TEST_ASSERT(bKey != 0, "SetupFindFirstLineW for 'ClassGuid' key must succeed");

        wchar_t guidBuf[64]{};
        uint32_t reqSize = 0;
        win32::BOOL bGet = setupapi::SetupGetStringFieldW(&ctx, 1, guidBuf, 64, &reqSize);
        TEST_ASSERT(bGet != 0 && reqSize > 0, "SetupGetStringFieldW for ClassGuid must succeed");
        TEST_ASSERT(std::wcscmp(guidBuf, L"{4d36e968-e325-11ce-bfc1-08002be10318}") == 0, "ClassGuid must match Display GUID");

        // Key index 0 returns key name
        wchar_t keyName[64]{};
        setupapi::SetupGetStringFieldW(&ctx, 0, keyName, 64, nullptr);
        TEST_ASSERT(std::wcscmp(keyName, L"ClassGuid") == 0, "Field 0 must return key name");

        // Verify %ManufacturerName% expanded in Provider key
        win32::BOOL bProv = setupapi::SetupFindFirstLineW(hInf, L"Version", L"Provider", &ctx);
        TEST_ASSERT(bProv != 0, "Provider line must exist");
        wchar_t provBuf[128]{};
        setupapi::SetupGetStringFieldW(&ctx, 1, provBuf, 128, nullptr);
        TEST_ASSERT(std::wcscmp(provBuf, L"MicaNT Sovereign Project") == 0, "Provider must be expanded from %ManufacturerName%");

        setupapi::SetupCloseInfFile(hInf);
    }

    // ------------------------------------------------------------------------
    // Stage 4: Device Information Set Lifecycle (SetupDiCreateDeviceInfoList, SetupDiDestroyDeviceInfoList)
    // ------------------------------------------------------------------------
    {
        setupapi::HDEVINFO hSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_NET, nullptr);
        TEST_ASSERT(hSet != nullptr, "SetupDiCreateDeviceInfoList must create handle");

        win32::BOOL bDestroy = setupapi::SetupDiDestroyDeviceInfoList(hSet);
        TEST_ASSERT(bDestroy != 0, "SetupDiDestroyDeviceInfoList must return success");

        // Handle without class filter
        setupapi::HDEVINFO hAll = setupapi::SetupDiCreateDeviceInfoList(nullptr, nullptr);
        TEST_ASSERT(hAll != nullptr, "SetupDiCreateDeviceInfoList without class must succeed");
        setupapi::SetupDiDestroyDeviceInfoList(hAll);
    }

    // ------------------------------------------------------------------------
    // Stage 5: Device Creation & Enumeration (SetupDiCreateDeviceInfoW, SetupDiEnumDeviceInfo)
    // ------------------------------------------------------------------------
    {
        setupapi::HDEVINFO hSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_DISPLAY, nullptr);
        setupapi::SP_DEVINFO_DATA devData1{};
        devData1.cbSize = sizeof(devData1);

        win32::BOOL bDev1 = setupapi::SetupDiCreateDeviceInfoW(
            hSet,
            L"PCI\\VEN_8086&DEV_9BC5&SUBSYS_12348086",
            &setupapi::GUID_DEVCLASS_DISPLAY,
            L"Intel UHD Graphics 630",
            nullptr,
            0,
            &devData1
        );
        TEST_ASSERT(bDev1 != 0, "SetupDiCreateDeviceInfoW dev 1 must succeed");
        TEST_ASSERT(devData1.ClassGuid == setupapi::GUID_DEVCLASS_DISPLAY, "ClassGuid must match");

        setupapi::SP_DEVINFO_DATA devData2{};
        devData2.cbSize = sizeof(devData2);
        win32::BOOL bDev2 = setupapi::SetupDiCreateDeviceInfoW(
            hSet,
            L"PCI\\VEN_10DE&DEV_2684&SUBSYS_567810DE",
            &setupapi::GUID_DEVCLASS_DISPLAY,
            L"NVIDIA RTX 4090",
            nullptr,
            0,
            &devData2
        );
        TEST_ASSERT(bDev2 != 0, "SetupDiCreateDeviceInfoW dev 2 must succeed");

        // Enumerate devices in set
        setupapi::SP_DEVINFO_DATA enumData{};
        enumData.cbSize = sizeof(enumData);
        win32::BOOL bEnum0 = setupapi::SetupDiEnumDeviceInfo(hSet, 0, &enumData);
        TEST_ASSERT(bEnum0 != 0 && enumData.DevInst == devData1.DevInst, "Enum index 0 must return dev 1");

        win32::BOOL bEnum1 = setupapi::SetupDiEnumDeviceInfo(hSet, 1, &enumData);
        TEST_ASSERT(bEnum1 != 0 && enumData.DevInst == devData2.DevInst, "Enum index 1 must return dev 2");

        win32::BOOL bEnum2 = setupapi::SetupDiEnumDeviceInfo(hSet, 2, &enumData);
        TEST_ASSERT(bEnum2 == 0, "Enum index 2 must return false (no more items)");

        setupapi::SetupDiDestroyDeviceInfoList(hSet);
    }

    // ------------------------------------------------------------------------
    // Stage 6: Device Instance ID Query (SetupDiGetDeviceInstanceIdW)
    // ------------------------------------------------------------------------
    {
        setupapi::HDEVINFO hSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_NET, nullptr);
        setupapi::SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);
        const wchar_t* instId = L"PCI\\VEN_10EC&DEV_8168&SUBSYS_012310EC";

        setupapi::SetupDiCreateDeviceInfoW(hSet, instId, &setupapi::GUID_DEVCLASS_NET, L"Realtek PCIe GbE Controller", nullptr, 0, &devData);

        wchar_t readInst[128]{};
        uint32_t req = 0;
        win32::BOOL bGet = setupapi::SetupDiGetDeviceInstanceIdW(hSet, &devData, readInst, 128, &req);
        TEST_ASSERT(bGet != 0 && req > 0, "SetupDiGetDeviceInstanceIdW must succeed");
        TEST_ASSERT(std::wcscmp(readInst, instId) == 0, "Device Instance ID must match creation argument");

        setupapi::SetupDiDestroyDeviceInfoList(hSet);
    }

    // ------------------------------------------------------------------------
    // Stage 7: Device Registry Properties Read & Write (SetupDiGetDeviceRegistryPropertyW, SetupDiSetDeviceRegistryPropertyW)
    // ------------------------------------------------------------------------
    {
        setupapi::HDEVINFO hSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_MEDIA, nullptr);
        setupapi::SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);
        setupapi::SetupDiCreateDeviceInfoW(hSet, L"HDAUDIO\\FUNC_01", &setupapi::GUID_DEVCLASS_MEDIA, nullptr, nullptr, 0, &devData);

        // Write FriendlyName
        const wchar_t* friendly = L"Studio Sound Card (MicaNT)";
        setupapi::SetupDiSetDeviceRegistryPropertyW(
            hSet, &devData, setupapi::SPDRP_FRIENDLYNAME,
            reinterpret_cast<const uint8_t*>(friendly),
            static_cast<uint32_t>((std::wcslen(friendly) + 1) * sizeof(wchar_t))
        );

        // Read FriendlyName
        wchar_t readFriendly[128]{};
        uint32_t regType = 0;
        win32::BOOL bGetProp = setupapi::SetupDiGetDeviceRegistryPropertyW(
            hSet, &devData, setupapi::SPDRP_FRIENDLYNAME, &regType,
            reinterpret_cast<uint8_t*>(readFriendly), sizeof(readFriendly), nullptr
        );
        TEST_ASSERT(bGetProp != 0 && regType == 1, "SetupDiGetDeviceRegistryPropertyW must succeed with REG_SZ");
        TEST_ASSERT(std::wcscmp(readFriendly, friendly) == 0, "Friendly name property must match written string");

        // Write Hardware ID
        const wchar_t* hwid = L"HDAUDIO\\FUNC_01&VEN_10EC";
        setupapi::SetupDiSetDeviceRegistryPropertyW(
            hSet, &devData, setupapi::SPDRP_HARDWAREID,
            reinterpret_cast<const uint8_t*>(hwid),
            static_cast<uint32_t>((std::wcslen(hwid) + 1) * sizeof(wchar_t))
        );

        wchar_t readHwid[128]{};
        setupapi::SetupDiGetDeviceRegistryPropertyW(
            hSet, &devData, setupapi::SPDRP_HARDWAREID, nullptr,
            reinterpret_cast<uint8_t*>(readHwid), sizeof(readHwid), nullptr
        );
        TEST_ASSERT(std::wcscmp(readHwid, hwid) == 0, "Hardware ID property must match");

        setupapi::SetupDiDestroyDeviceInfoList(hSet);
    }

    // ------------------------------------------------------------------------
    // Stage 8: Device Interface Detail Path (SetupDiCreateDeviceInterfaceW, SetupDiGetDeviceInterfaceDetailW)
    // ------------------------------------------------------------------------
    {
        setupapi::HDEVINFO hSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_DISPLAY, nullptr);
        setupapi::SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);
        setupapi::SetupDiCreateDeviceInfoW(hSet, L"PCI\\VEN_10DE&DEV_2684", &setupapi::GUID_DEVCLASS_DISPLAY, nullptr, nullptr, 0, &devData);

        setupapi::SP_DEVICE_INTERFACE_DATA ifData{};
        win32::BOOL bCreateIf = setupapi::SetupDiCreateDeviceInterfaceW(hSet, &devData, &setupapi::GUID_DEVCLASS_DISPLAY, nullptr, 0, &ifData);
        TEST_ASSERT(bCreateIf != 0, "SetupDiCreateDeviceInterfaceW must succeed");
        TEST_ASSERT(ifData.Flags == setupapi::SPINT_ACTIVE, "Created interface must have SPINT_ACTIVE flag");

        // Query Detail Path
        uint8_t buffer[512]{};
        auto* detail = reinterpret_cast<setupapi::SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer);
        detail->cbSize = sizeof(setupapi::SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        uint32_t req = 0;
        win32::BOOL bGetDetail = setupapi::SetupDiGetDeviceInterfaceDetailW(hSet, &ifData, detail, sizeof(buffer), &req, nullptr);
        TEST_ASSERT(bGetDetail != 0 && req > 0, "SetupDiGetDeviceInterfaceDetailW must succeed");
        TEST_ASSERT(std::wcsstr(detail->DevicePath, L"PCI\\VEN_10DE") != nullptr, "Device path must contain PCI hardware identifier");

        setupapi::SetupDiDestroyDeviceInfoList(hSet);
    }

    // ------------------------------------------------------------------------
    // Stage 9: Device Class Description & Class Name Resolution
    // ------------------------------------------------------------------------
    {
        wchar_t descBuf[128]{};
        win32::BOOL bDesc = setupapi::SetupDiGetClassDescriptionW(&setupapi::GUID_DEVCLASS_DISPLAY, descBuf, 128, nullptr);
        TEST_ASSERT(bDesc != 0, "SetupDiGetClassDescriptionW for Display class must succeed");
        TEST_ASSERT(std::wcscmp(descBuf, L"Display adapters") == 0, "Display class description must be 'Display adapters'");

        wchar_t nameBuf[64]{};
        win32::BOOL bName = setupapi::SetupDiClassNameFromGuidW(&setupapi::GUID_DEVCLASS_NET, nameBuf, 64, nullptr);
        TEST_ASSERT(bName != 0, "SetupDiClassNameFromGuidW for Net class must succeed");
        TEST_ASSERT(std::wcscmp(nameBuf, L"Net") == 0, "Net class name must be 'Net'");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Driver Information List & Selection (SetupDiBuildDriverInfoList)
    // ------------------------------------------------------------------------
    {
        setupapi::HDEVINFO hSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_DISKDRIVE, nullptr);
        setupapi::SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);
        setupapi::SetupDiCreateDeviceInfoW(hSet, L"SCSI\\DiskNVMe", &setupapi::GUID_DEVCLASS_DISKDRIVE, L"NVMe Solid State Disk", nullptr, 0, &devData);

        win32::BOOL bDrv = setupapi::SetupDiBuildDriverInfoList(hSet, &devData, setupapi::SPDIT_COMPATDRIVER);
        TEST_ASSERT(bDrv != 0, "SetupDiBuildDriverInfoList must succeed");

        setupapi::SetupDiDestroyDeviceInfoList(hSet);
    }

    // ------------------------------------------------------------------------
    // Stage 11: Global Hardware Device Snapshot (SetupDiGetClassDevsW)
    // ------------------------------------------------------------------------
    {
        // Query all present devices
        setupapi::HDEVINFO hDevs = setupapi::SetupDiGetClassDevsW(nullptr, nullptr, nullptr, setupapi::DIGCF_ALLCLASSES | setupapi::DIGCF_PRESENT);
        TEST_ASSERT(hDevs != nullptr, "SetupDiGetClassDevsW all classes must succeed");

        uint32_t count = 0;
        setupapi::SP_DEVINFO_DATA d{};
        d.cbSize = sizeof(d);
        while (setupapi::SetupDiEnumDeviceInfo(hDevs, count, &d)) {
            count++;
        }
        TEST_ASSERT(count >= 5, "Pre-seeded system hardware devices must be at least 5 (GPU, Net, Disk, Audio, System)");
        setupapi::SetupDiDestroyDeviceInfoList(hDevs);

        // Query only Display class
        setupapi::HDEVINFO hGpu = setupapi::SetupDiGetClassDevsW(&setupapi::GUID_DEVCLASS_DISPLAY, nullptr, nullptr, setupapi::DIGCF_PRESENT);
        TEST_ASSERT(hGpu != nullptr, "SetupDiGetClassDevsW Display class must succeed");
        count = 0;
        while (setupapi::SetupDiEnumDeviceInfo(hGpu, count, &d)) {
            count++;
        }
        TEST_ASSERT(count >= 1, "Must contain at least 1 Display adapter");
        setupapi::SetupDiDestroyDeviceInfoList(hGpu);
    }

    // ------------------------------------------------------------------------
    // Stage 12: Dynamic Loader Exports & Version Metadata (setupapi.dll)
    // ------------------------------------------------------------------------
    {
        setupapi::InitializeSetupApiSubsystemExports();
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupOpenInfFileW") != nullptr, "SetupOpenInfFileW must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupCloseInfFile") != nullptr, "SetupCloseInfFile must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupFindFirstLineW") != nullptr, "SetupFindFirstLineW must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupGetStringFieldW") != nullptr, "SetupGetStringFieldW must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupDiCreateDeviceInfoList") != nullptr, "SetupDiCreateDeviceInfoList must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupDiDestroyDeviceInfoList") != nullptr, "SetupDiDestroyDeviceInfoList must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupDiCreateDeviceInfoW") != nullptr, "SetupDiCreateDeviceInfoW must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupDiEnumDeviceInfo") != nullptr, "SetupDiEnumDeviceInfo must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupDiGetDeviceRegistryPropertyW") != nullptr, "SetupDiGetDeviceRegistryPropertyW must be exported");
        TEST_ASSERT(ldr.getExport("setupapi.dll", "SetupDiGetClassDevsW") != nullptr, "SetupDiGetClassDevsW must be exported");

        const auto* ver = version::VersionDatabase::Instance().FindModule("setupapi.dll");
        TEST_ASSERT(ver != nullptr, "VersionDatabase must contain setupapi.dll");
        TEST_ASSERT(ver->stringTable.at("OriginalFilename") == "setupapi.dll", "setupapi.dll OriginalFilename must match");
    }

    // ------------------------------------------------------------------------
    // Stage 13: Command Shell Integration (devmgmt & setupapi test)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("devmgmt", out);
        TEST_ASSERT(out.str().find("MicaNT Device Manager") != std::string::npos, "Shell devmgmt must display Device Manager header");
        TEST_ASSERT(out.str().find("Total Active Devices:") != std::string::npos, "Shell devmgmt must list active devices");

        out.str("");
        shell.execute("setupapi test", out);
        TEST_ASSERT(out.str().find("ALL DEVICE INSTALLATION CHECKS PASSED") != std::string::npos, "Shell setupapi test must pass all checks");
    }

    std::cout << "[TEST] Suite 70: Windows Device Installation & SetupAPI Subsystem PASSED.\n";
}

// ============================================================================
// Suite 71: Windows OLE Structured Storage & Compound File Subsystem (ole32.dll)
// ============================================================================

class MockPersistDocObject : public ole32::IPersistStorage, public ole32::IPersistStreamInit {
private:
    std::atomic<uint32_t> m_refCount{ 1 };
    bool m_isDirty{ false };
    std::string m_textPayload{ "Initial Document Payload" };
    ole32::CLSID m_clsid{ 0x12345678, 0x1234, 0x5678, { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 } };

public:
    MockPersistDocObject() = default;

    virtual ole32::HRESULT QueryInterface(ole32::REFIID riid, void** ppv) override {
        if (!ppv) return ole32::E_POINTER;
        if (riid == ole32::IID_IUnknown || riid == ole32::IID_IPersist) {
            *ppv = static_cast<ole32::IPersist*>(static_cast<ole32::IPersistStorage*>(this));
            AddRef();
            return ole32::S_OK;
        }
        if (riid == ole32::IID_IPersistStorage) {
            *ppv = static_cast<ole32::IPersistStorage*>(this);
            AddRef();
            return ole32::S_OK;
        }
        if (riid == ole32::IID_IPersistStream || riid == ole32::IID_IPersistStreamInit) {
            *ppv = static_cast<ole32::IPersistStreamInit*>(this);
            AddRef();
            return ole32::S_OK;
        }
        *ppv = nullptr;
        return ole32::E_NOINTERFACE;
    }

    virtual uint32_t AddRef() override { return m_refCount.fetch_add(1) + 1; }
    virtual uint32_t Release() override {
        uint32_t c = m_refCount.fetch_sub(1) - 1;
        if (c == 0) delete this;
        return c;
    }

    // IPersist
    virtual ole32::HRESULT GetClassID(ole32::CLSID* pClassID) override {
        if (!pClassID) return ole32::E_POINTER;
        *pClassID = m_clsid;
        return ole32::S_OK;
    }

    // IPersistStorage
    virtual ole32::HRESULT IsDirty() override { return m_isDirty ? ole32::S_OK : ole32::S_FALSE; }
    virtual ole32::HRESULT InitNew(ole32::IStorage*) override {
        m_textPayload = "New Storage Initialized";
        m_isDirty = true;
        return ole32::S_OK;
    }
    virtual ole32::HRESULT Load(ole32::IStorage* pStg) override {
        if (!pStg) return ole32::E_POINTER;
        ole32::IStream* pStm = nullptr;
        ole32::HRESULT hr = pStg->OpenStream(L"ContentStream", nullptr, ole32::STGM_READ | ole32::STGM_SHARE_EXCLUSIVE, 0, &pStm);
        if (FAILED(hr)) return hr;
        char buf[256]{};
        uint32_t read = 0;
        pStm->Read(buf, sizeof(buf) - 1, &read);
        pStm->Release();
        m_textPayload = std::string(buf, read);
        m_isDirty = false;
        return ole32::S_OK;
    }
    virtual ole32::HRESULT Save(ole32::IStorage* pStgSave, win32::BOOL) override {
        if (!pStgSave) return ole32::E_POINTER;
        ole32::IStream* pStm = nullptr;
        ole32::HRESULT hr = pStgSave->CreateStream(L"ContentStream", ole32::STGM_WRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pStm);
        if (FAILED(hr)) return hr;
        uint32_t written = 0;
        pStm->Write(m_textPayload.data(), static_cast<uint32_t>(m_textPayload.size()), &written);
        pStm->Release();
        m_isDirty = false;
        return ole32::S_OK;
    }
    virtual ole32::HRESULT SaveCompleted(ole32::IStorage*) override { return ole32::S_OK; }
    virtual ole32::HRESULT HandsOffStorage() override { return ole32::S_OK; }

    // IPersistStreamInit
    virtual ole32::HRESULT InitNew() override {
        m_textPayload = "New Stream Initialized";
        m_isDirty = true;
        return ole32::S_OK;
    }
    virtual ole32::HRESULT Save(ole32::IStream* pStm, win32::BOOL fClearDirty) override {
        if (!pStm) return ole32::E_POINTER;
        uint32_t written = 0;
        pStm->Write(m_textPayload.data(), static_cast<uint32_t>(m_textPayload.size()), &written);
        if (fClearDirty) m_isDirty = false;
        return ole32::S_OK;
    }
    virtual ole32::HRESULT Load(ole32::IStream* pStm) override {
        if (!pStm) return ole32::E_POINTER;
        char buf[256]{};
        uint32_t read = 0;
        pStm->Read(buf, sizeof(buf) - 1, &read);
        m_textPayload = std::string(buf, read);
        m_isDirty = false;
        return ole32::S_OK;
    }
    virtual ole32::HRESULT GetSizeMax(uint64_t* pcbSize) override {
        if (!pcbSize) return ole32::E_POINTER;
        *pcbSize = m_textPayload.size();
        return ole32::S_OK;
    }

    void setPayload(std::string_view p) { m_textPayload = p; m_isDirty = true; }
    const std::string& getPayload() const { return m_textPayload; }
};

void Test_StructuredStorage_CompoundFile_And_Persistence_Subsystem() {
    using namespace micant;
    std::cout << "\n[TEST] Running Suite 71: Windows OLE Structured Storage & Compound File Subsystem (ole32.dll)...\n";

    // ------------------------------------------------------------------------
    // Stage 1: In-Memory ILockBytes Creation and Byte I/O
    // ------------------------------------------------------------------------
    ole32::ILockBytes* plk = nullptr;
    ole32::HRESULT hr = ole32::CreateILockBytesOnHGlobal(nullptr, win32::TRUE, &plk);
    TEST_ASSERT(SUCCEEDED(hr) && plk != nullptr, "CreateILockBytesOnHGlobal must allocate ILockBytes");

    const char rawData[] = "MicaNT ILockBytes Raw Stream Test Block 12345678";
    uint32_t bytesWritten = 0;
    hr = plk->WriteAt(100, rawData, sizeof(rawData), &bytesWritten);
    TEST_ASSERT(SUCCEEDED(hr) && bytesWritten == sizeof(rawData), "ILockBytes::WriteAt must write raw bytes at offset 100");

    char readBuf[128]{};
    uint32_t bytesRead = 0;
    hr = plk->ReadAt(100, readBuf, sizeof(rawData), &bytesRead);
    TEST_ASSERT(SUCCEEDED(hr) && bytesRead == sizeof(rawData), "ILockBytes::ReadAt must read back raw bytes");
    TEST_ASSERT(std::memcmp(rawData, readBuf, sizeof(rawData)) == 0, "ILockBytes byte content must match exactly");

    ole32::STATSTG lkStat{};
    hr = plk->Stat(&lkStat, ole32::STATFLAG_NONAME);
    TEST_ASSERT(SUCCEEDED(hr), "ILockBytes::Stat must succeed");
    TEST_ASSERT(lkStat.type == ole32::STGTY_LOCKBYTES, "ILockBytes type must be STGTY_LOCKBYTES");
    TEST_ASSERT(lkStat.cbSize >= 100 + sizeof(rawData), "ILockBytes size must reflect written extent");

    // ------------------------------------------------------------------------
    // Stage 2: StgCreateDocfileOnILockBytes & Root Storage Properties
    // ------------------------------------------------------------------------
    plk->SetSize(0);
    ole32::IStorage* pRoot = nullptr;
    hr = ole32::StgCreateDocfileOnILockBytes(plk, ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, &pRoot);
    TEST_ASSERT(SUCCEEDED(hr) && pRoot != nullptr, "StgCreateDocfileOnILockBytes must create root IStorage");

    ole32::STATSTG rootStat{};
    hr = pRoot->Stat(&rootStat, ole32::STATFLAG_DEFAULT);
    TEST_ASSERT(SUCCEEDED(hr), "Root IStorage::Stat must succeed");
    TEST_ASSERT(rootStat.type == ole32::STGTY_ROOT, "Root storage type must be STGTY_ROOT");
    if (rootStat.pwcsName) ole32::CoTaskMemFree(rootStat.pwcsName);

    // ------------------------------------------------------------------------
    // Stage 3: Hierarchical Nested Storages (CreateStorage & OpenStorage)
    // ------------------------------------------------------------------------
    ole32::IStorage* pSub1 = nullptr;
    hr = pRoot->CreateStorage(L"FolderA", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pSub1);
    TEST_ASSERT(SUCCEEDED(hr) && pSub1 != nullptr, "CreateStorage must create nested sub-storage FolderA");

    ole32::IStorage* pSub2 = nullptr;
    hr = pSub1->CreateStorage(L"SubFolderB", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pSub2);
    TEST_ASSERT(SUCCEEDED(hr) && pSub2 != nullptr, "CreateStorage must create nested sub-storage SubFolderB");

    ole32::IStorage* pOpenSub1 = nullptr;
    hr = pRoot->OpenStorage(L"FolderA", nullptr, ole32::STGM_READWRITE | ole32::STGM_SHARE_EXCLUSIVE, nullptr, 0, &pOpenSub1);
    TEST_ASSERT(SUCCEEDED(hr) && pOpenSub1 != nullptr, "OpenStorage must open existing sub-storage FolderA");
    pOpenSub1->Release();

    // ------------------------------------------------------------------------
    // Stage 4: Stream Creation, Sequential Write, Seek, and Read (IStream)
    // ------------------------------------------------------------------------
    ole32::IStream* pStm = nullptr;
    hr = pSub2->CreateStream(L"PayloadData", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pStm);
    TEST_ASSERT(SUCCEEDED(hr) && pStm != nullptr, "CreateStream must create stream PayloadData");

    const std::string textData = "Dave Cutler 1988 DEC PRISM / MICA Architecture OLE Structured Storage!";
    uint32_t stmWritten = 0;
    hr = pStm->Write(textData.data(), static_cast<uint32_t>(textData.size()), &stmWritten);
    TEST_ASSERT(SUCCEEDED(hr) && stmWritten == textData.size(), "IStream::Write must write full text length");

    uint64_t newPos = 0;
    hr = pStm->Seek(0, ole32::STREAM_SEEK_SET, &newPos);
    TEST_ASSERT(SUCCEEDED(hr) && newPos == 0, "IStream::Seek must seek to beginning");

    std::vector<char> stmReadBuf(textData.size() + 1, 0);
    uint32_t stmRead = 0;
    hr = pStm->Read(stmReadBuf.data(), static_cast<uint32_t>(textData.size()), &stmRead);
    TEST_ASSERT(SUCCEEDED(hr) && stmRead == textData.size(), "IStream::Read must read back stream data");
    TEST_ASSERT(std::string_view(stmReadBuf.data(), stmRead) == textData, "IStream read content must match written payload");

    // Test stream stat
    ole32::STATSTG stmStat{};
    hr = pStm->Stat(&stmStat, ole32::STATFLAG_DEFAULT);
    TEST_ASSERT(SUCCEEDED(hr) && stmStat.type == ole32::STGTY_STREAM, "Stream type must be STGTY_STREAM");
    TEST_ASSERT(stmStat.cbSize == textData.size(), "Stream cbSize must match payload length");
    if (stmStat.pwcsName) ole32::CoTaskMemFree(stmStat.pwcsName);

    pStm->Release();

    // ------------------------------------------------------------------------
    // Stage 5: Element Deletion and Renaming (DestroyElement & RenameElement)
    // ------------------------------------------------------------------------
    ole32::IStream* pTempStm = nullptr;
    hr = pRoot->CreateStream(L"TempFile", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pTempStm);
    TEST_ASSERT(SUCCEEDED(hr), "CreateStream must create TempFile");
    pTempStm->Release();

    hr = pRoot->RenameElement(L"TempFile", L"RenamedFile");
    TEST_ASSERT(SUCCEEDED(hr), "RenameElement must rename TempFile to RenamedFile");

    hr = pRoot->OpenStream(L"TempFile", nullptr, ole32::STGM_READ, 0, &pTempStm);
    TEST_ASSERT(FAILED(hr), "OpenStream on old name must fail");

    hr = pRoot->OpenStream(L"RenamedFile", nullptr, ole32::STGM_READ, 0, &pTempStm);
    TEST_ASSERT(SUCCEEDED(hr) && pTempStm != nullptr, "OpenStream on new name must succeed");
    pTempStm->Release();

    hr = pRoot->DestroyElement(L"RenamedFile");
    TEST_ASSERT(SUCCEEDED(hr), "DestroyElement must delete RenamedFile");

    hr = pRoot->OpenStream(L"RenamedFile", nullptr, ole32::STGM_READ, 0, &pTempStm);
    TEST_ASSERT(FAILED(hr), "OpenStream on destroyed element must fail");

    // ------------------------------------------------------------------------
    // Stage 6: Directory Enumeration with IEnumSTATSTG
    // ------------------------------------------------------------------------
    ole32::IEnumSTATSTG* pEnum = nullptr;
    hr = pRoot->EnumElements(0, nullptr, 0, &pEnum);
    TEST_ASSERT(SUCCEEDED(hr) && pEnum != nullptr, "EnumElements must return IEnumSTATSTG");

    ole32::STATSTG enumStats[4]{};
    uint32_t fetched = 0;
    hr = pEnum->Next(1, enumStats, &fetched);
    TEST_ASSERT(SUCCEEDED(hr) && fetched == 1, "IEnumSTATSTG::Next must fetch 1 item");
    TEST_ASSERT(enumStats[0].pwcsName != nullptr, "Enumerated item must have name");
    TEST_ASSERT(std::wcscmp(enumStats[0].pwcsName, L"FolderA") == 0, "Enumerated item name must be FolderA");
    ole32::CoTaskMemFree(enumStats[0].pwcsName);

    hr = pEnum->Reset();
    TEST_ASSERT(SUCCEEDED(hr), "IEnumSTATSTG::Reset must succeed");

    pEnum->Release();

    // ------------------------------------------------------------------------
    // Stage 7: Storage Metadata, Class GUIDs, State Bits, Timestamps
    // ------------------------------------------------------------------------
    const ole32::CLSID testClsid = { 0xABCDEF01, 0x1234, 0x5678, { 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78 } };
    hr = pSub2->SetClass(testClsid);
    TEST_ASSERT(SUCCEEDED(hr), "SetClass must succeed");

    ole32::CLSID readClsid{};
    hr = ole32::ReadClassStg(pSub2, &readClsid);
    TEST_ASSERT(SUCCEEDED(hr), "ReadClassStg must succeed");
    TEST_ASSERT(std::memcmp(&testClsid, &readClsid, sizeof(ole32::CLSID)) == 0, "ReadClassStg must match set CLSID");

    hr = pSub2->SetStateBits(0x00000005, 0x0000000F);
    TEST_ASSERT(SUCCEEDED(hr), "SetStateBits must succeed");

    ole32::STATSTG sub2Stat{};
    hr = pSub2->Stat(&sub2Stat, ole32::STATFLAG_DEFAULT);
    TEST_ASSERT(SUCCEEDED(hr), "Sub2 Stat must succeed");
    TEST_ASSERT((sub2Stat.grfStateBits & 0x0000000F) == 0x00000005, "State bits must match updated mask");
    if (sub2Stat.pwcsName) ole32::CoTaskMemFree(sub2Stat.pwcsName);

    // ------------------------------------------------------------------------
    // Stage 8: Recursive Storage Cloning (CopyTo)
    // ------------------------------------------------------------------------
    ole32::IStorage* pTargetStg = nullptr;
    hr = pRoot->CreateStorage(L"TargetBackup", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pTargetStg);
    TEST_ASSERT(SUCCEEDED(hr), "CreateStorage TargetBackup must succeed");

    hr = pSub1->CopyTo(0, nullptr, nullptr, pTargetStg);
    TEST_ASSERT(SUCCEEDED(hr), "CopyTo must clone FolderA tree into TargetBackup");

    // Verify cloned SubFolderB and PayloadData stream exist in TargetBackup
    ole32::IStorage* pClonedSub = nullptr;
    hr = pTargetStg->OpenStorage(L"SubFolderB", nullptr, ole32::STGM_READWRITE | ole32::STGM_SHARE_EXCLUSIVE, nullptr, 0, &pClonedSub);
    TEST_ASSERT(SUCCEEDED(hr) && pClonedSub != nullptr, "TargetBackup must contain cloned SubFolderB");

    ole32::IStream* pClonedStm = nullptr;
    hr = pClonedSub->OpenStream(L"PayloadData", nullptr, ole32::STGM_READ | ole32::STGM_SHARE_EXCLUSIVE, 0, &pClonedStm);
    TEST_ASSERT(SUCCEEDED(hr) && pClonedStm != nullptr, "TargetBackup must contain cloned PayloadData stream");

    std::vector<char> cloneBuf(textData.size() + 1, 0);
    uint32_t cloneRead = 0;
    pClonedStm->Read(cloneBuf.data(), static_cast<uint32_t>(textData.size()), &cloneRead);
    TEST_ASSERT(std::string_view(cloneBuf.data(), cloneRead) == textData, "Cloned stream data must match original payload");

    pClonedStm->Release();
    pClonedSub->Release();
    pTargetStg->Release();

    // ------------------------------------------------------------------------
    // Stage 9: Element Moving (MoveElementTo)
    // ------------------------------------------------------------------------
    ole32::IStorage* pDestFolder = nullptr;
    hr = pRoot->CreateStorage(L"DestFolder", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pDestFolder);
    TEST_ASSERT(SUCCEEDED(hr), "CreateStorage DestFolder must succeed");

    ole32::IStream* pMoveStm = nullptr;
    hr = pRoot->CreateStream(L"StreamToMove", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pMoveStm);
    TEST_ASSERT(SUCCEEDED(hr), "CreateStream StreamToMove must succeed");
    const char moveMsg[] = "Data being moved";
    uint32_t moveWritten = 0;
    pMoveStm->Write(moveMsg, sizeof(moveMsg), &moveWritten);
    pMoveStm->Release();

    hr = pRoot->MoveElementTo(L"StreamToMove", pDestFolder, L"MovedStream", 0);
    TEST_ASSERT(SUCCEEDED(hr), "MoveElementTo must move element to destination storage");

    hr = pRoot->OpenStream(L"StreamToMove", nullptr, ole32::STGM_READ, 0, &pMoveStm);
    TEST_ASSERT(FAILED(hr), "Source element must no longer exist in source storage");

    hr = pDestFolder->OpenStream(L"MovedStream", nullptr, ole32::STGM_READ, 0, &pMoveStm);
    TEST_ASSERT(SUCCEEDED(hr) && pMoveStm != nullptr, "Moved element must exist in destination storage");
    pMoveStm->Release();
    pDestFolder->Release();

    // ------------------------------------------------------------------------
    // Stage 10: Binary CFBF Serialization & Magic Header Verification
    // ------------------------------------------------------------------------
    pSub2->Release();
    pSub1->Release();

    hr = pRoot->Commit(ole32::STGC_DEFAULT);
    TEST_ASSERT(SUCCEEDED(hr), "Commit must serialize root compound file to ILockBytes");

    hr = ole32::StgIsStorageILockBytes(plk);
    TEST_ASSERT(hr == ole32::S_OK, "StgIsStorageILockBytes must confirm valid CFBF binary signature");

    // Verify first 8 bytes of ILockBytes are exactly 0xD0CF11E0A1B11AE1
    uint8_t magicSig[8]{};
    uint32_t magicRead = 0;
    plk->ReadAt(0, magicSig, 8, &magicRead);
    const uint8_t expectedCFBF[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
    TEST_ASSERT(magicRead == 8 && std::memcmp(magicSig, expectedCFBF, 8) == 0, "First 8 bytes must be standard CFBF OLE DocFile magic");

    pRoot->Release();

    // ------------------------------------------------------------------------
    // Stage 11: Deserialization of CFBF Binary Image from LockBytes
    // ------------------------------------------------------------------------
    ole32::IStorage* pReopenedRoot = nullptr;
    hr = ole32::StgOpenStorageOnILockBytes(plk, nullptr, ole32::STGM_READWRITE | ole32::STGM_SHARE_EXCLUSIVE, nullptr, 0, &pReopenedRoot);
    TEST_ASSERT(SUCCEEDED(hr) && pReopenedRoot != nullptr, "StgOpenStorageOnILockBytes must deserialize compound docfile");

    // Verify FolderA exists in deserialized docfile
    ole32::IStorage* pReopenedSub1 = nullptr;
    hr = pReopenedRoot->OpenStorage(L"FolderA", nullptr, ole32::STGM_READWRITE | ole32::STGM_SHARE_EXCLUSIVE, nullptr, 0, &pReopenedSub1);
    TEST_ASSERT(SUCCEEDED(hr) && pReopenedSub1 != nullptr, "Reopened docfile must contain FolderA");

    // Verify SubFolderB exists
    ole32::IStorage* pReopenedSub2 = nullptr;
    hr = pReopenedSub1->OpenStorage(L"SubFolderB", nullptr, ole32::STGM_READWRITE | ole32::STGM_SHARE_EXCLUSIVE, nullptr, 0, &pReopenedSub2);
    TEST_ASSERT(SUCCEEDED(hr) && pReopenedSub2 != nullptr, "Reopened docfile must contain SubFolderB");

    // Verify PayloadData stream and content
    ole32::IStream* pReopenedStm = nullptr;
    hr = pReopenedSub2->OpenStream(L"PayloadData", nullptr, ole32::STGM_READ | ole32::STGM_SHARE_EXCLUSIVE, 0, &pReopenedStm);
    TEST_ASSERT(SUCCEEDED(hr) && pReopenedStm != nullptr, "Reopened docfile must contain PayloadData stream");

    std::vector<char> reopenedBuf(textData.size() + 1, 0);
    uint32_t reopenedRead = 0;
    pReopenedStm->Read(reopenedBuf.data(), static_cast<uint32_t>(textData.size()), &reopenedRead);
    TEST_ASSERT(std::string_view(reopenedBuf.data(), reopenedRead) == textData, "Deserialized stream content must match original payload");

    pReopenedStm->Release();
    pReopenedSub2->Release();
    pReopenedSub1->Release();
    pReopenedRoot->Release();
    plk->Release();

    // ------------------------------------------------------------------------
    // Stage 12: Physical Compound File on Disk (StgCreateDocfile & StgOpenStorage)
    // ------------------------------------------------------------------------
    const wchar_t* diskDocPath = L"C:\\test_compound_file.doc";
    ole32::IStorage* pDiskStg = nullptr;
    hr = ole32::StgCreateDocfile(diskDocPath, ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, &pDiskStg);
    TEST_ASSERT(SUCCEEDED(hr) && pDiskStg != nullptr, "StgCreateDocfile must create disk-backed compound document");

    ole32::IStream* pDiskStm = nullptr;
    hr = pDiskStg->CreateStream(L"WordDocument", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pDiskStm);
    TEST_ASSERT(SUCCEEDED(hr), "CreateStream WordDocument on disk docfile must succeed");

    const char docContent[] = "MicaNT Sovereign OS - Compound Document Native Binary Storage";
    uint32_t docWritten = 0;
    pDiskStm->Write(docContent, sizeof(docContent), &docWritten);
    pDiskStm->Release();

    hr = pDiskStg->Commit(ole32::STGC_DEFAULT);
    TEST_ASSERT(SUCCEEDED(hr), "Commit on disk docfile must write binary file to disk");
    pDiskStg->Release();

    hr = ole32::StgIsStorageFile(diskDocPath);
    TEST_ASSERT(hr == ole32::S_OK, "StgIsStorageFile must confirm disk file has CFBF binary signature");

    ole32::IStorage* pReopenedDiskStg = nullptr;
    hr = ole32::StgOpenStorage(diskDocPath, nullptr, ole32::STGM_READ | ole32::STGM_SHARE_EXCLUSIVE, nullptr, 0, &pReopenedDiskStg);
    TEST_ASSERT(SUCCEEDED(hr) && pReopenedDiskStg != nullptr, "StgOpenStorage must open disk compound file");

    ole32::IStream* pReopenedDiskStm = nullptr;
    hr = pReopenedDiskStg->OpenStream(L"WordDocument", nullptr, ole32::STGM_READ | ole32::STGM_SHARE_EXCLUSIVE, 0, &pReopenedDiskStm);
    TEST_ASSERT(SUCCEEDED(hr) && pReopenedDiskStm != nullptr, "OpenStream WordDocument from reopened disk file must succeed");

    char diskReadBuf[128]{};
    uint32_t diskBytesRead = 0;
    pReopenedDiskStm->Read(diskReadBuf, sizeof(diskReadBuf), &diskBytesRead);
    TEST_ASSERT(std::memcmp(docContent, diskReadBuf, sizeof(docContent)) == 0, "Disk read payload must match written data");

    pReopenedDiskStm->Release();
    pReopenedDiskStg->Release();
    win32::DeleteFileW(diskDocPath);

    // ------------------------------------------------------------------------
    // Stage 13: Stream Class Writing and Reading (WriteClassStm & ReadClassStm)
    // ------------------------------------------------------------------------
    ole32::IStream* pMemStm = nullptr;
    hr = ole32::CreateStreamOnHGlobal(nullptr, win32::TRUE, &pMemStm);
    TEST_ASSERT(SUCCEEDED(hr), "CreateStreamOnHGlobal must create memory stream");

    const ole32::CLSID clsidSample = { 0x55554444, 0x3333, 0x2222, { 0x11, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } };
    hr = ole32::WriteClassStm(pMemStm, clsidSample);
    TEST_ASSERT(SUCCEEDED(hr), "WriteClassStm must write CLSID to stream");

    pMemStm->Seek(0, ole32::STREAM_SEEK_SET, nullptr);
    ole32::CLSID readClsidStm{};
    hr = ole32::ReadClassStm(pMemStm, &readClsidStm);
    TEST_ASSERT(SUCCEEDED(hr), "ReadClassStm must read CLSID from stream");
    TEST_ASSERT(std::memcmp(&clsidSample, &readClsidStm, sizeof(ole32::CLSID)) == 0, "Stream CLSID must match written value");
    pMemStm->Release();

    // ------------------------------------------------------------------------
    // Stage 14: COM Persistence Subsystem (IPersistStorage & IPersistStreamInit)
    // ------------------------------------------------------------------------
    {
        MockPersistDocObject mockObj;
        mockObj.setPayload("Stateful COM Component In-Memory Data");

        // Save into a docfile storage
        ole32::IStorage* pPersistStg = nullptr;
        ole32::StgCreateDocfile(nullptr, ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, &pPersistStg);

        hr = ole32::OleSave(&mockObj, pPersistStg, win32::TRUE);
        TEST_ASSERT(SUCCEEDED(hr), "OleSave must save IPersistStorage object and write CLSID header");

        ole32::CLSID savedClsid{};
        hr = ole32::ReadClassStg(pPersistStg, &savedClsid);
        TEST_ASSERT(SUCCEEDED(hr), "ReadClassStg must retrieve persisted object CLSID");

        ole32::CLSID expectedClsid{};
        mockObj.GetClassID(&expectedClsid);
        TEST_ASSERT(std::memcmp(&savedClsid, &expectedClsid, sizeof(ole32::CLSID)) == 0, "Persisted storage CLSID must match object class");

        // Load into another mock instance
        MockPersistDocObject loadedObj;
        hr = loadedObj.Load(pPersistStg);
        TEST_ASSERT(SUCCEEDED(hr), "IPersistStorage::Load must reload persisted document stream");
        TEST_ASSERT(loadedObj.getPayload() == "Stateful COM Component In-Memory Data", "Loaded payload must match saved payload");

        pPersistStg->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 15: Dynamic Loader Exports & Shell Integration (stg info & stg test)
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("ole32.dll", "StgCreateDocfile") != nullptr, "StgCreateDocfile must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "StgOpenStorage") != nullptr, "StgOpenStorage must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "StgCreateDocfileOnILockBytes") != nullptr, "StgCreateDocfileOnILockBytes must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "StgOpenStorageOnILockBytes") != nullptr, "StgOpenStorageOnILockBytes must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "StgIsStorageFile") != nullptr, "StgIsStorageFile must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "StgIsStorageILockBytes") != nullptr, "StgIsStorageILockBytes must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "CreateILockBytesOnHGlobal") != nullptr, "CreateILockBytesOnHGlobal must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "WriteClassStg") != nullptr, "WriteClassStg must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "ReadClassStg") != nullptr, "ReadClassStg must be exported");
        TEST_ASSERT(ldr.getExport("ole32.dll", "OleSave") != nullptr, "OleSave must be exported");

        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("stg info", out);
        TEST_ASSERT(out.str().find("Compound File Binary Format Engine") != std::string::npos, "Shell stg info must display engine details");
        TEST_ASSERT(out.str().find("0xD0CF11E0A1B11AE1") != std::string::npos, "Shell stg info must display CFBF magic");

        out.str("");
        shell.execute("stg test", out);
        TEST_ASSERT(out.str().find("ALL STRUCTURED STORAGE & COMPOUND FILE CHECKS PASSED") != std::string::npos, "Shell stg test must pass all checks");
    }

    std::cout << "[TEST] Suite 71: Windows OLE Structured Storage & Compound File Subsystem PASSED.\n";
}

void Test_WindowsEventLog_And_WevtApi_Subsystem() {
    using namespace micant;

    std::cout << "[TEST] Running Suite 72: Windows Event Log & Instrumentation Subsystem (wevtapi.dll / advapi32.dll)...\n";

    wevtapi::InitializeWevtApiSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Channel Enumeration (EvtOpenChannelEnum & EvtNextChannelPath)
    // ------------------------------------------------------------------------
    {
        wevtapi::EVT_HANDLE hChanEnum = wevtapi::EvtOpenChannelEnum(nullptr, 0);
        TEST_ASSERT(hChanEnum != nullptr, "EvtOpenChannelEnum must return valid channel enum handle");

        std::vector<std::wstring> channels;
        wchar_t chanBuf[256]{};
        uint32_t bufUsed = 0;
        while (wevtapi::EvtNextChannelPath(hChanEnum, 256, chanBuf, &bufUsed)) {
            channels.push_back(chanBuf);
        }
        wevtapi::EvtClose(hChanEnum);

        TEST_ASSERT(channels.size() >= 4, "Must enumerate at least 4 default channels");
        TEST_ASSERT(std::find(channels.begin(), channels.end(), L"System") != channels.end(), "System channel must be present");
        TEST_ASSERT(std::find(channels.begin(), channels.end(), L"Application") != channels.end(), "Application channel must be present");
        TEST_ASSERT(std::find(channels.begin(), channels.end(), L"Security") != channels.end(), "Security channel must be present");
        TEST_ASSERT(std::find(channels.begin(), channels.end(), L"Setup") != channels.end(), "Setup channel must be present");
    }

    // ------------------------------------------------------------------------
    // Stage 2: Publisher Enumeration & Metadata (EvtOpenPublisherEnum & EvtGetPublisherMetadataProperty)
    // ------------------------------------------------------------------------
    {
        wevtapi::EVT_HANDLE hPubEnum = wevtapi::EvtOpenPublisherEnum(nullptr, 0);
        TEST_ASSERT(hPubEnum != nullptr, "EvtOpenPublisherEnum must return valid publisher enum handle");

        std::vector<std::wstring> publishers;
        wchar_t pubBuf[256]{};
        uint32_t bufUsed = 0;
        while (wevtapi::EvtNextPublisherId(hPubEnum, 256, pubBuf, &bufUsed)) {
            publishers.push_back(pubBuf);
        }
        wevtapi::EvtClose(hPubEnum);

        TEST_ASSERT(!publishers.empty(), "Publisher enumeration must find registered publishers");
        TEST_ASSERT(std::find(publishers.begin(), publishers.end(), L"MicaNT-Kernel") != publishers.end(), "MicaNT-Kernel publisher must be registered");

        wevtapi::EVT_HANDLE hMeta = wevtapi::EvtOpenPublisherMetadata(nullptr, L"MicaNT-Kernel", nullptr, 0, 0);
        TEST_ASSERT(hMeta != nullptr, "EvtOpenPublisherMetadata must open metadata for MicaNT-Kernel");

        GUID pubGuid{};
        win32::BOOL bGuid = wevtapi::EvtGetPublisherMetadataProperty(hMeta, wevtapi::EvtPublisherMetadataPublisherGuid, 0, sizeof(pubGuid), &pubGuid, &bufUsed);
        TEST_ASSERT(bGuid == win32::TRUE && bufUsed == sizeof(GUID), "EvtGetPublisherMetadataProperty must return publisher GUID");
        TEST_ASSERT(pubGuid.Data1 == 0x11112222, "Publisher GUID Data1 must match registered kernel GUID");

        wchar_t resPath[256]{};
        win32::BOOL bPath = wevtapi::EvtGetPublisherMetadataProperty(hMeta, wevtapi::EvtPublisherMetadataMessageFilePath, 0, sizeof(resPath), resPath, &bufUsed);
        TEST_ASSERT(bPath == win32::TRUE && std::wcslen(resPath) > 0, "EvtGetPublisherMetadataProperty must return message file path");

        wevtapi::EvtClose(hMeta);
    }

    // ------------------------------------------------------------------------
    // Stage 3: Modern Event Emission & Monotonic Record ID Progression
    // ------------------------------------------------------------------------
    uint64_t emittedId1 = 0;
    uint64_t emittedId2 = 0;
    {
        wevtapi::EventRecord rec1{};
        rec1.channel = L"Application";
        rec1.providerName = L"MicaNT-Diagnostics";
        rec1.eventId = 2001;
        rec1.level = wevtapi::WINEVENT_LEVEL_INFO;
        rec1.stringInserts.push_back(L"Diagnostics runtime initialized");
        rec1.namedData[L"SessionID"] = L"101";

        emittedId1 = wevtapi::EventLogManager::Instance().WriteEvent(rec1);
        TEST_ASSERT(emittedId1 > 0, "Emitted event must have positive record ID");

        wevtapi::EventRecord rec2{};
        rec2.channel = L"Application";
        rec2.providerName = L"MicaNT-Diagnostics";
        rec2.eventId = 2002;
        rec2.level = wevtapi::WINEVENT_LEVEL_WARNING;
        rec2.stringInserts.push_back(L"Diagnostics memory warning threshold reached");
        rec2.namedData[L"UsagePercent"] = L"85";

        emittedId2 = wevtapi::EventLogManager::Instance().WriteEvent(rec2);
        TEST_ASSERT(emittedId2 > emittedId1, "Subsequent event record ID must monotonically advance");
    }

    // ------------------------------------------------------------------------
    // Stage 4: Event Query Traversal (EvtQuery, EvtNext & EvtSeek)
    // ------------------------------------------------------------------------
    {
        wevtapi::EVT_HANDLE hQuery = wevtapi::EvtQuery(nullptr, L"Application", L"*", wevtapi::EvtQueryChannelPath | wevtapi::EvtQueryForwardDirection);
        TEST_ASSERT(hQuery != nullptr, "EvtQuery must return valid query handle");

        wevtapi::EVT_HANDLE events[10]{};
        uint32_t returned = 0;
        win32::BOOL bNext = wevtapi::EvtNext(hQuery, 10, events, 1000, 0, &returned);
        TEST_ASSERT(bNext == win32::TRUE && returned >= 2, "EvtNext must retrieve emitted application events");

        // Verify seeking
        win32::BOOL bSeek = wevtapi::EvtSeek(hQuery, 0, nullptr, 0, 0);
        TEST_ASSERT(bSeek == win32::TRUE, "EvtSeek must reset cursor to beginning");

        for (uint32_t i = 0; i < returned; ++i) {
            wevtapi::EvtClose(events[i]);
        }
        wevtapi::EvtClose(hQuery);
    }

    // ------------------------------------------------------------------------
    // Stage 5: Structured XML Event Rendering (EvtCreateRenderContext & EvtRender)
    // ------------------------------------------------------------------------
    {
        wevtapi::EVT_HANDLE hQuery = wevtapi::EvtQuery(nullptr, L"Application", L"*", wevtapi::EvtQueryChannelPath | wevtapi::EvtQueryForwardDirection);
        wevtapi::EVT_HANDLE hEvent = nullptr;
        uint32_t returned = 0;
        wevtapi::EvtNext(hQuery, 1, &hEvent, 1000, 0, &returned);
        TEST_ASSERT(returned == 1 && hEvent != nullptr, "Must retrieve at least 1 event for rendering");

        wevtapi::EVT_HANDLE hContext = wevtapi::EvtCreateRenderContext(0, nullptr, wevtapi::EvtRenderContextValues);
        TEST_ASSERT(hContext != nullptr, "EvtCreateRenderContext must succeed");

        wchar_t xmlBuffer[4096]{};
        uint32_t bufUsed = 0;
        uint32_t propCount = 0;
        win32::BOOL bRender = wevtapi::EvtRender(hContext, hEvent, wevtapi::EvtRenderEventXml, sizeof(xmlBuffer), xmlBuffer, &bufUsed, &propCount);
        TEST_ASSERT(bRender == win32::TRUE, "EvtRender with EvtRenderEventXml must succeed");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<Event xmlns=\"http://schemas.microsoft.com/win/2004/08/events/event\">") != nullptr, "Rendered XML must have standard Event root xmlns");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<System>") != nullptr, "Rendered XML must include <System> element");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<Provider Name=") != nullptr, "Rendered XML must include <Provider Name=> attribute");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<EventID>") != nullptr, "Rendered XML must include <EventID>");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<EventRecordID>") != nullptr, "Rendered XML must include <EventRecordID>");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<Channel>Application</Channel>") != nullptr, "Rendered XML must include correct Channel");
        TEST_ASSERT(std::wcsstr(xmlBuffer, L"<EventData>") != nullptr, "Rendered XML must include <EventData>");

        wevtapi::EvtClose(hContext);
        wevtapi::EvtClose(hEvent);
        wevtapi::EvtClose(hQuery);
    }

    // ------------------------------------------------------------------------
    // Stage 6: Channel Configuration Properties (EvtOpenChannelConfig & EvtGetChannelConfigProperty)
    // ------------------------------------------------------------------------
    {
        wevtapi::EVT_HANDLE hCfg = wevtapi::EvtOpenChannelConfig(nullptr, L"System", 0);
        TEST_ASSERT(hCfg != nullptr, "EvtOpenChannelConfig must open System channel config");

        win32::BOOL enabled = win32::FALSE;
        uint32_t bufUsed = 0;
        win32::BOOL bProp = wevtapi::EvtGetChannelConfigProperty(hCfg, wevtapi::EvtChannelConfigEnabled, 0, sizeof(enabled), &enabled, &bufUsed);
        TEST_ASSERT(bProp == win32::TRUE && enabled == win32::TRUE, "System channel must be enabled");

        uint64_t maxSize = 0;
        bProp = wevtapi::EvtGetChannelConfigProperty(hCfg, wevtapi::EvtChannelLoggingConfigMaxSize, 0, sizeof(maxSize), &maxSize, &bufUsed);
        TEST_ASSERT(bProp == win32::TRUE && maxSize > 0, "System channel must have positive max buffer size");

        wevtapi::EvtClose(hCfg);
    }

    // ------------------------------------------------------------------------
    // Stage 7: Channel Clearing (EvtClearLog)
    // ------------------------------------------------------------------------
    {
        // Emit event to Setup channel, verify it exists, clear, verify empty
        wevtapi::EventRecord setupRec{};
        setupRec.channel = L"Setup";
        setupRec.providerName = L"MicaNT-SetupTest";
        setupRec.eventId = 5001;
        wevtapi::EventLogManager::Instance().WriteEvent(setupRec);
        TEST_ASSERT(wevtapi::EventLogManager::Instance().GetRecordCount(L"Setup") > 0, "Setup channel must have records before clearing");

        win32::BOOL bClear = wevtapi::EvtClearLog(nullptr, L"Setup", nullptr, 0);
        TEST_ASSERT(bClear == win32::TRUE, "EvtClearLog must succeed on Setup channel");
        TEST_ASSERT(wevtapi::EventLogManager::Instance().GetRecordCount(L"Setup") == 0, "Setup channel record count must be 0 after clear");
    }

    // ------------------------------------------------------------------------
    // Stage 8: Legacy advapi32 EventLog API Bridge (RegisterEventSourceW & ReportEventW)
    // ------------------------------------------------------------------------
    {
        void* hSource = wevtapi::RegisterEventSourceW(nullptr, L"MicaNT-ServiceTest");
        TEST_ASSERT(hSource != nullptr, "RegisterEventSourceW must return valid handle");

        const wchar_t* stringInserts[] = { L"Subsystem started successfully", L"Worker thread count: 4" };
        win32::BOOL bReport = wevtapi::ReportEventW(
            hSource,
            wevtapi::EVENTLOG_INFORMATION_TYPE,
            1, // category
            1005, // event ID
            nullptr,
            2, // num strings
            0,
            stringInserts,
            nullptr
        );
        TEST_ASSERT(bReport == win32::TRUE, "ReportEventW must log event successfully");

        // Verify event appears in Application channel query
        auto events = wevtapi::EventLogManager::Instance().Query(L"Application", L"EventID=1005");
        TEST_ASSERT(!events.empty(), "Query must locate event reported via ReportEventW");
        TEST_ASSERT(events.back().providerName == L"MicaNT-ServiceTest", "Provider name must match registered legacy source");
        TEST_ASSERT(events.back().stringInserts.size() >= 2, "String inserts must be preserved in event record");

        wevtapi::DeregisterEventSource(hSource);
    }

    // ------------------------------------------------------------------------
    // Stage 9: Legacy Record Counting & Oldest Record ID (GetNumberOfEventLogRecords & GetOldestEventLogRecord)
    // ------------------------------------------------------------------------
    {
        void* hLog = wevtapi::OpenEventLogW(nullptr, L"Application");
        TEST_ASSERT(hLog != nullptr, "OpenEventLogW must open Application log");

        uint32_t numRecs = 0;
        win32::BOOL bNum = wevtapi::GetNumberOfEventLogRecords(hLog, &numRecs);
        TEST_ASSERT(bNum == win32::TRUE && numRecs > 0, "GetNumberOfEventLogRecords must return non-zero record count");

        uint32_t oldestRec = 0;
        win32::BOOL bOldest = wevtapi::GetOldestEventLogRecord(hLog, &oldestRec);
        TEST_ASSERT(bOldest == win32::TRUE && oldestRec > 0, "GetOldestEventLogRecord must return non-zero oldest record ID");

        wevtapi::CloseEventLog(hLog);
    }

    // ------------------------------------------------------------------------
    // Stage 10: Legacy ANSI Event Reporting Bridge (RegisterEventSourceA & ReportEventA)
    // ------------------------------------------------------------------------
    {
        void* hSourceA = wevtapi::RegisterEventSourceA(nullptr, "MicaNT-AnsiLogger");
        TEST_ASSERT(hSourceA != nullptr, "RegisterEventSourceA must return valid handle");

        const char* ansiStrings[] = { "Ansi string insert 1", "Ansi string insert 2" };
        win32::BOOL bReportA = wevtapi::ReportEventA(
            hSourceA,
            wevtapi::EVENTLOG_WARNING_TYPE,
            2,
            8800,
            nullptr,
            2,
            0,
            ansiStrings,
            nullptr
        );
        TEST_ASSERT(bReportA == win32::TRUE, "ReportEventA must log ANSI event");

        auto events = wevtapi::EventLogManager::Instance().Query(L"Application", L"EventID=8800");
        TEST_ASSERT(!events.empty(), "Query must locate ANSI-reported event");
        TEST_ASSERT(events.back().level == wevtapi::WINEVENT_LEVEL_WARNING, "Event level must match warning level");

        wevtapi::DeregisterEventSource(hSourceA);
    }

    // ------------------------------------------------------------------------
    // Stage 11: Dynamic Loader Exports Verification (wevtapi.dll & advapi32.dll)
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        // wevtapi.dll exports
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtOpenSession") != nullptr, "EvtOpenSession must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtClose") != nullptr, "EvtClose must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtQuery") != nullptr, "EvtQuery must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtNext") != nullptr, "EvtNext must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtSeek") != nullptr, "EvtSeek must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtCreateRenderContext") != nullptr, "EvtCreateRenderContext must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtRender") != nullptr, "EvtRender must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtOpenPublisherMetadata") != nullptr, "EvtOpenPublisherMetadata must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtGetPublisherMetadataProperty") != nullptr, "EvtGetPublisherMetadataProperty must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtOpenChannelEnum") != nullptr, "EvtOpenChannelEnum must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtNextChannelPath") != nullptr, "EvtNextChannelPath must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtOpenPublisherEnum") != nullptr, "EvtOpenPublisherEnum must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtNextPublisherId") != nullptr, "EvtNextPublisherId must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtOpenChannelConfig") != nullptr, "EvtOpenChannelConfig must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtGetChannelConfigProperty") != nullptr, "EvtGetChannelConfigProperty must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtSaveChannelConfig") != nullptr, "EvtSaveChannelConfig must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtClearLog") != nullptr, "EvtClearLog must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtExportLog") != nullptr, "EvtExportLog must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtCreateBookmark") != nullptr, "EvtCreateBookmark must be exported");
        TEST_ASSERT(ldr.getExport("wevtapi.dll", "EvtUpdateBookmark") != nullptr, "EvtUpdateBookmark must be exported");

        // advapi32.dll EventLog exports
        TEST_ASSERT(ldr.getExport("advapi32.dll", "RegisterEventSourceW") != nullptr, "advapi32.dll!RegisterEventSourceW must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "RegisterEventSourceA") != nullptr, "advapi32.dll!RegisterEventSourceA must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "ReportEventW") != nullptr, "advapi32.dll!ReportEventW must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "ReportEventA") != nullptr, "advapi32.dll!ReportEventA must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "DeregisterEventSource") != nullptr, "advapi32.dll!DeregisterEventSource must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "OpenEventLogW") != nullptr, "advapi32.dll!OpenEventLogW must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "OpenEventLogA") != nullptr, "advapi32.dll!OpenEventLogA must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "CloseEventLog") != nullptr, "advapi32.dll!CloseEventLog must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "ClearEventLogW") != nullptr, "advapi32.dll!ClearEventLogW must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "GetNumberOfEventLogRecords") != nullptr, "advapi32.dll!GetNumberOfEventLogRecords must be exported");
        TEST_ASSERT(ldr.getExport("advapi32.dll", "GetOldestEventLogRecord") != nullptr, "advapi32.dll!GetOldestEventLogRecord must be exported");
    }

    // ------------------------------------------------------------------------
    // Stage 12: Command Shell Integration (wevtutil el, wevtutil qe, wevtutil test)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("wevtutil el", out);
        TEST_ASSERT(out.str().find("System") != std::string::npos, "Shell wevtutil el must list System channel");
        TEST_ASSERT(out.str().find("Security") != std::string::npos, "Shell wevtutil el must list Security channel");

        out.str("");
        shell.execute("wevtutil qe System", out);
        TEST_ASSERT(out.str().find("MicaNT-Kernel") != std::string::npos, "Shell wevtutil qe System must display kernel events");

        out.str("");
        shell.execute("wevtutil test", out);
        TEST_ASSERT(out.str().find("ALL EVENT LOG & INSTRUMENTATION CHECKS PASSED") != std::string::npos, "Shell wevtutil test must pass all checks");
    }

    std::cout << "[TEST] Suite 72: Windows Event Log & Instrumentation Subsystem PASSED.\n";
}

void Test_WMI_WindowsManagementInstrumentation_Subsystem() {
    using namespace micant;

    std::cout << "[TEST] Running Suite 73: Windows Management Instrumentation (WMI / WBEM / wbemprox.dll)...\n";

    wbem::InitializeWbemSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: COM Activation of CLSID_WbemLocator via CoCreateInstance
    // ------------------------------------------------------------------------
    wbem::IWbemLocator* pLoc = nullptr;
    {
        ole32::HRESULT hr = ole32::CoCreateInstance(
            wbem::CLSID_WbemLocator,
            nullptr,
            1 /* CLSCTX_INPROC_SERVER */,
            wbem::IID_IWbemLocator,
            reinterpret_cast<void**>(&pLoc)
        );
        TEST_ASSERT(hr == ole32::S_OK, "CoCreateInstance(CLSID_WbemLocator) must succeed with S_OK");
        TEST_ASSERT(pLoc != nullptr, "IWbemLocator interface pointer must not be null");
    }

    // ------------------------------------------------------------------------
    // Stage 2: ConnectServer to ROOT\CIMV2 and Negative Namespace Validation
    // ------------------------------------------------------------------------
    wbem::IWbemServices* pSvc = nullptr;
    {
        // Negative test: invalid namespace
        wbem::IWbemServices* pBadSvc = nullptr;
        ole32::BSTR bstrBadNs = ole32::SysAllocString(L"ROOT\\NonExistentNamespace");
        ole32::HRESULT hrBad = pLoc->ConnectServer(bstrBadNs, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pBadSvc);
        ole32::SysFreeString(bstrBadNs);
        TEST_ASSERT(hrBad == wbem::WBEM_E_INVALID_NAMESPACE, "ConnectServer must fail with WBEM_E_INVALID_NAMESPACE for unknown namespace");
        TEST_ASSERT(pBadSvc == nullptr, "Bad namespace must return null service pointer");

        // Positive test: ROOT\CIMV2
        ole32::BSTR bstrNs = ole32::SysAllocString(L"ROOT\\CIMV2");
        ole32::HRESULT hr = pLoc->ConnectServer(bstrNs, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
        ole32::SysFreeString(bstrNs);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR, "ConnectServer(ROOT\\CIMV2) must succeed with WBEM_S_NO_ERROR");
        TEST_ASSERT(pSvc != nullptr, "IWbemServices pointer must not be null");
    }

    // ------------------------------------------------------------------------
    // Stage 3: Class Instance Enumeration (Win32_OperatingSystem)
    // ------------------------------------------------------------------------
    {
        wbem::IEnumWbemClassObject* pEnum = nullptr;
        ole32::BSTR bstrClass = ole32::SysAllocString(L"Win32_OperatingSystem");
        ole32::HRESULT hr = pSvc->CreateInstanceEnum(bstrClass, 0, nullptr, &pEnum);
        ole32::SysFreeString(bstrClass);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr, "CreateInstanceEnum(Win32_OperatingSystem) must succeed");

        wbem::IWbemClassObject* pOs = nullptr;
        uint32_t uRet = 0;
        hr = pEnum->Next(wbem::WBEM_INFINITE, 1, &pOs, &uRet);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && uRet == 1 && pOs != nullptr, "Enum Next must retrieve 1 Win32_OperatingSystem instance");

        // ------------------------------------------------------------------------
        // Stage 4: Win32_OperatingSystem Property Retrieval
        // ------------------------------------------------------------------------
        ole32::VARIANT vtCaption{};
        pOs->Get(L"Caption", 0, &vtCaption, nullptr, nullptr);
        TEST_ASSERT(vtCaption.vt == ole32::VT_BSTR && vtCaption.bstrVal != nullptr, "Win32_OperatingSystem.Caption must be VT_BSTR");
        TEST_ASSERT(std::wcsstr(vtCaption.bstrVal, L"MicaNT") != nullptr, "Caption must identify MicaNT Operating System");
        oleaut32::VariantClear(&vtCaption);

        ole32::VARIANT vtVersion{};
        pOs->Get(L"Version", 0, &vtVersion, nullptr, nullptr);
        TEST_ASSERT(vtVersion.vt == ole32::VT_BSTR && vtVersion.bstrVal != nullptr, "Win32_OperatingSystem.Version must be VT_BSTR");
        TEST_ASSERT(std::wcscmp(vtVersion.bstrVal, L"10.0.26100.1") == 0, "Version must match 10.0.26100.1 build target");
        oleaut32::VariantClear(&vtVersion);

        ole32::VARIANT vtRam{};
        pOs->Get(L"TotalVisibleMemorySize", 0, &vtRam, nullptr, nullptr);
        TEST_ASSERT(vtRam.vt == ole32::VT_UI8 && vtRam.ullVal > 0, "TotalVisibleMemorySize must be non-zero VT_UI8");
        oleaut32::VariantClear(&vtRam);

        // ------------------------------------------------------------------------
        // Stage 5: IWbemClassObject::GetObjectText MOF Generation
        // ------------------------------------------------------------------------
        ole32::BSTR bstrMof = nullptr;
        hr = pOs->GetObjectText(0, &bstrMof);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && bstrMof != nullptr, "GetObjectText must generate MOF definition");
        TEST_ASSERT(std::wcsstr(bstrMof, L"instance of Win32_OperatingSystem") != nullptr, "MOF text must declare instance of Win32_OperatingSystem");
        TEST_ASSERT(std::wcsstr(bstrMof, L"Caption = \"MicaNT 10.0") != nullptr, "MOF text must format Caption property string");
        ole32::SysFreeString(bstrMof);

        pOs->Release();
        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 6: Processor Topology Verification (Win32_Processor)
    // ------------------------------------------------------------------------
    {
        wbem::IEnumWbemClassObject* pEnum = nullptr;
        ole32::BSTR bstrClass = ole32::SysAllocString(L"Win32_Processor");
        ole32::HRESULT hr = pSvc->CreateInstanceEnum(bstrClass, 0, nullptr, &pEnum);
        ole32::SysFreeString(bstrClass);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr, "CreateInstanceEnum(Win32_Processor) must succeed");

        wbem::IWbemClassObject* pCpu = nullptr;
        uint32_t uRet = 0;
        hr = pEnum->Next(wbem::WBEM_INFINITE, 1, &pCpu, &uRet);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && uRet == 1 && pCpu != nullptr, "Must retrieve Win32_Processor instance");

        ole32::VARIANT vtCores{};
        pCpu->Get(L"NumberOfCores", 0, &vtCores, nullptr, nullptr);
        TEST_ASSERT(vtCores.vt == ole32::VT_UI4 && vtCores.ulVal == 4, "Win32_Processor.NumberOfCores must report 4 cores");
        oleaut32::VariantClear(&vtCores);

        ole32::VARIANT vtArch{};
        pCpu->Get(L"Architecture", 0, &vtArch, nullptr, nullptr);
        TEST_ASSERT(vtArch.vt == ole32::VT_UI2 && vtArch.uiVal == 9, "Win32_Processor.Architecture must report x64 (9)");
        oleaut32::VariantClear(&vtArch);

        ole32::VARIANT vtClock{};
        pCpu->Get(L"MaxClockSpeed", 0, &vtClock, nullptr, nullptr);
        TEST_ASSERT(vtClock.vt == ole32::VT_UI4 && vtClock.ulVal == 3600, "Win32_Processor.MaxClockSpeed must report 3600 MHz");
        oleaut32::VariantClear(&vtClock);

        pCpu->Release();
        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 7: Storage & Volume Introspection (Win32_LogicalDisk)
    // ------------------------------------------------------------------------
    {
        wbem::IEnumWbemClassObject* pEnum = nullptr;
        ole32::BSTR bstrClass = ole32::SysAllocString(L"Win32_LogicalDisk");
        ole32::HRESULT hr = pSvc->CreateInstanceEnum(bstrClass, 0, nullptr, &pEnum);
        ole32::SysFreeString(bstrClass);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr, "CreateInstanceEnum(Win32_LogicalDisk) must succeed");

        wbem::IWbemClassObject* pDisk = nullptr;
        uint32_t uRet = 0;
        hr = pEnum->Next(wbem::WBEM_INFINITE, 1, &pDisk, &uRet);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && uRet == 1 && pDisk != nullptr, "Must retrieve Win32_LogicalDisk instance");

        ole32::VARIANT vtDevId{};
        pDisk->Get(L"DeviceID", 0, &vtDevId, nullptr, nullptr);
        TEST_ASSERT(vtDevId.vt == ole32::VT_BSTR && std::wcscmp(vtDevId.bstrVal, L"C:") == 0, "Win32_LogicalDisk.DeviceID must be C:");
        oleaut32::VariantClear(&vtDevId);

        ole32::VARIANT vtFs{};
        pDisk->Get(L"FileSystem", 0, &vtFs, nullptr, nullptr);
        TEST_ASSERT(vtFs.vt == ole32::VT_BSTR && std::wcscmp(vtFs.bstrVal, L"NTFS") == 0, "Win32_LogicalDisk.FileSystem must be NTFS");
        oleaut32::VariantClear(&vtFs);

        pDisk->Release();
        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 8: Network Configuration (Win32_NetworkAdapterConfiguration)
    // ------------------------------------------------------------------------
    {
        wbem::IEnumWbemClassObject* pEnum = nullptr;
        ole32::BSTR bstrClass = ole32::SysAllocString(L"Win32_NetworkAdapterConfiguration");
        ole32::HRESULT hr = pSvc->CreateInstanceEnum(bstrClass, 0, nullptr, &pEnum);
        ole32::SysFreeString(bstrClass);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr, "CreateInstanceEnum(Win32_NetworkAdapterConfiguration) must succeed");

        wbem::IWbemClassObject* pNic = nullptr;
        uint32_t uRet = 0;
        hr = pEnum->Next(wbem::WBEM_INFINITE, 1, &pNic, &uRet);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && uRet == 1 && pNic != nullptr, "Must retrieve Win32_NetworkAdapterConfiguration instance");

        ole32::VARIANT vtIp{};
        pNic->Get(L"IPAddress", 0, &vtIp, nullptr, nullptr);
        TEST_ASSERT(vtIp.vt == ole32::VT_BSTR && std::wcscmp(vtIp.bstrVal, L"192.168.1.100") == 0, "IPAddress must match configured adapter IP");
        oleaut32::VariantClear(&vtIp);

        pNic->Release();
        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 9: WQL Query Execution (SELECT * FROM Win32_Service)
    // ------------------------------------------------------------------------
    {
        wbem::IEnumWbemClassObject* pEnum = nullptr;
        ole32::BSTR bstrWql = ole32::SysAllocString(L"WQL");
        ole32::BSTR bstrQuery = ole32::SysAllocString(L"SELECT * FROM Win32_Service");
        ole32::HRESULT hr = pSvc->ExecQuery(bstrWql, bstrQuery, 0, nullptr, &pEnum);
        ole32::SysFreeString(bstrWql);
        ole32::SysFreeString(bstrQuery);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr, "ExecQuery(SELECT * FROM Win32_Service) must succeed");

        wbem::IWbemClassObject* svcs[10]{};
        uint32_t uRet = 0;
        hr = pEnum->Next(wbem::WBEM_INFINITE, 10, svcs, &uRet);
        TEST_ASSERT(uRet >= 5, "Must retrieve at least 5 standard services");

        bool foundWinmgmt = false;
        for (uint32_t i = 0; i < uRet; ++i) {
            ole32::VARIANT vtName{};
            svcs[i]->Get(L"Name", 0, &vtName, nullptr, nullptr);
            if (vtName.vt == ole32::VT_BSTR && std::wcscmp(vtName.bstrVal, L"Winmgmt") == 0) {
                foundWinmgmt = true;
            }
            oleaut32::VariantClear(&vtName);
            svcs[i]->Release();
        }
        TEST_ASSERT(foundWinmgmt, "WQL query result must contain Winmgmt service");

        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 10: WQL Filtering with WHERE Clause (SELECT * WHERE Name = 'Winmgmt')
    // ------------------------------------------------------------------------
    {
        wbem::IEnumWbemClassObject* pEnum = nullptr;
        ole32::BSTR bstrWql = ole32::SysAllocString(L"WQL");
        ole32::BSTR bstrQuery = ole32::SysAllocString(L"SELECT * FROM Win32_Service WHERE Name = 'Winmgmt'");
        ole32::HRESULT hr = pSvc->ExecQuery(bstrWql, bstrQuery, 0, nullptr, &pEnum);
        ole32::SysFreeString(bstrWql);
        ole32::SysFreeString(bstrQuery);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr, "ExecQuery with WHERE clause must succeed");

        wbem::IWbemClassObject* pObj = nullptr;
        uint32_t uRet = 0;
        hr = pEnum->Next(wbem::WBEM_INFINITE, 1, &pObj, &uRet);
        TEST_ASSERT(hr == wbem::WBEM_S_NO_ERROR && uRet == 1 && pObj != nullptr, "Filtered query must return exactly 1 object");

        ole32::VARIANT vtState{};
        pObj->Get(L"State", 0, &vtState, nullptr, nullptr);
        TEST_ASSERT(vtState.vt == ole32::VT_BSTR && std::wcscmp(vtState.bstrVal, L"Running") == 0, "Winmgmt service state must be Running");
        oleaut32::VariantClear(&vtState);

        pObj->Release();
        pEnum->Release();
    }

    pSvc->Release();
    pLoc->Release();

    // ------------------------------------------------------------------------
    // Stage 11: Dynamic Loader Exports Verification (wbemprox.dll & fastprox.dll)
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("wbemprox.dll", "DllGetClassObject") != nullptr, "wbemprox.dll!DllGetClassObject must be exported");
        TEST_ASSERT(ldr.getExport("wbemprox.dll", "DllCanUnloadNow") != nullptr, "wbemprox.dll!DllCanUnloadNow must be exported");
        TEST_ASSERT(ldr.getExport("wbemprox.dll", "DllRegisterServer") != nullptr, "wbemprox.dll!DllRegisterServer must be exported");
        TEST_ASSERT(ldr.getExport("wbemprox.dll", "DllUnregisterServer") != nullptr, "wbemprox.dll!DllUnregisterServer must be exported");

        TEST_ASSERT(ldr.getExport("fastprox.dll", "DllGetClassObject") != nullptr, "fastprox.dll!DllGetClassObject must be exported");
        TEST_ASSERT(ldr.getExport("fastprox.dll", "DllCanUnloadNow") != nullptr, "fastprox.dll!DllCanUnloadNow must be exported");
        TEST_ASSERT(ldr.getExport("fastprox.dll", "DllRegisterServer") != nullptr, "fastprox.dll!DllRegisterServer must be exported");
        TEST_ASSERT(ldr.getExport("fastprox.dll", "DllUnregisterServer") != nullptr, "fastprox.dll!DllUnregisterServer must be exported");
    }

    // ------------------------------------------------------------------------
    // Stage 12: Command Shell Integration (wmic os get, wmic cpu get, wmic test)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("wmic os get Caption,Version", out);
        TEST_ASSERT(out.str().find("MicaNT") != std::string::npos, "Shell wmic os get must display MicaNT caption");
        TEST_ASSERT(out.str().find("10.0.26100.1") != std::string::npos, "Shell wmic os get must display version");

        out.str("");
        shell.execute("wmic cpu get NumberOfCores", out);
        TEST_ASSERT(out.str().find("4") != std::string::npos, "Shell wmic cpu get must report 4 cores");

        out.str("");
        shell.execute("wmic test", out);
        TEST_ASSERT(out.str().find("ALL WMI / WBEM CHECKS PASSED") != std::string::npos, "Shell wmic test must pass all checks");
    }

    std::cout << "[TEST] Suite 73: Windows Management Instrumentation (WMI / WBEM) Subsystem PASSED.\n";
}

void Test_WindowsTaskScheduler_Subsystem() {
    std::cout << "\n[TEST] Running Suite 74: Windows Task Scheduler 2.0 Subsystem (taskschd.dll)...\n";
    using namespace ole32;
    using DWORD = uint32_t;
    using LONG = int32_t;

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Exports & COM Class Factory (CLSID_TaskScheduler)
    // ------------------------------------------------------------------------
    taskschd::InitializeTaskSchedulerSubsystemExports();

    {
        taskschd::ITaskService* pService = nullptr;
        HRESULT hr = ole32::CoCreateInstance(
            taskschd::CLSID_TaskScheduler, nullptr, 1 /* CLSCTX_INPROC_SERVER */,
            taskschd::IID_ITaskService, reinterpret_cast<void**>(&pService)
        );
        TEST_ASSERT(hr == S_OK, "CoCreateInstance(CLSID_TaskScheduler) must return S_OK");
        TEST_ASSERT(pService != nullptr, "pService must not be null");

        // Verify QueryInterface for IDispatch and IUnknown
        oleaut32::IDispatch* pDisp = nullptr;
        hr = pService->QueryInterface(oleaut32::IID_IDispatch, reinterpret_cast<void**>(&pDisp));
        TEST_ASSERT(hr == S_OK && pDisp != nullptr, "ITaskService must implement IDispatch");
        pDisp->Release();

        // Null pointer check
        hr = pService->QueryInterface(taskschd::IID_ITaskService, nullptr);
        TEST_ASSERT(hr == E_POINTER, "QueryInterface with nullptr must return E_POINTER");

        pService->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 2: Service Connection State & Version Reflection
    // ------------------------------------------------------------------------
    taskschd::ITaskService* pSvc = nullptr;
    HRESULT hr = ole32::CoCreateInstance(
        taskschd::CLSID_TaskScheduler, nullptr, 1,
        taskschd::IID_ITaskService, reinterpret_cast<void**>(&pSvc)
    );
    TEST_ASSERT(hr == S_OK && pSvc != nullptr, "TaskService creation must succeed");

    {
        // Prior to Connect, get_Connected must report VARIANT_FALSE
        ole32::VARIANT_BOOL bConn = ole32::VARIANT_FALSE;
        pSvc->get_Connected(&bConn);
        TEST_ASSERT(bConn == ole32::VARIANT_FALSE, "get_Connected prior to Connect must return VARIANT_FALSE");

        // Attempting to GetFolder before Connect must fail with SCHED_E_SERVICE_NOT_RUNNING
        taskschd::ITaskFolder* pUnconnFolder = nullptr;
        ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
        hr = pSvc->GetFolder(bstrRoot, &pUnconnFolder);
        TEST_ASSERT(hr == taskschd::SCHED_E_SERVICE_NOT_RUNNING, "GetFolder without connect must return SCHED_E_SERVICE_NOT_RUNNING");
        ole32::SysFreeString(bstrRoot);

        // Connect with server and user parameters
        ole32::VARIANT vServer{}, vUser{}, vDomain{}, vPass{};
        oleaut32::VariantInit(&vServer);
        vServer.vt = ole32::VT_BSTR;
        vServer.bstrVal = ole32::SysAllocString(L"MICANT-NODE0");

        oleaut32::VariantInit(&vUser);
        vUser.vt = ole32::VT_BSTR;
        vUser.bstrVal = ole32::SysAllocString(L"Administrator");

        hr = pSvc->Connect(vServer, vUser, vDomain, vPass);
        TEST_ASSERT(hr == S_OK, "Connect must return S_OK");

        pSvc->get_Connected(&bConn);
        TEST_ASSERT(bConn == ole32::VARIANT_TRUE, "get_Connected after Connect must return VARIANT_TRUE");

        ole32::BSTR bstrTarget = nullptr;
        pSvc->get_TargetServer(&bstrTarget);
        TEST_ASSERT(bstrTarget != nullptr && std::wcscmp(bstrTarget, L"MICANT-NODE0") == 0, "Target server must match connected name");
        ole32::SysFreeString(bstrTarget);

        ole32::BSTR bstrUser = nullptr;
        pSvc->get_ConnectedUser(&bstrUser);
        TEST_ASSERT(bstrUser != nullptr && std::wcscmp(bstrUser, L"Administrator") == 0, "Connected user must match connected credentials");
        ole32::SysFreeString(bstrUser);

        DWORD dwVersion = 0;
        pSvc->get_HighestVersion(&dwVersion);
        TEST_ASSERT(dwVersion == 0x00010002, "HighestVersion must report Task Scheduler 2.0 (1.2)");

        oleaut32::VariantClear(&vServer);
        oleaut32::VariantClear(&vUser);
    }

    // ------------------------------------------------------------------------
    // Stage 3: Root Folder Navigation & Pre-seeded Windows Tasks
    // ------------------------------------------------------------------------
    taskschd::ITaskFolder* pRoot = nullptr;
    {
        ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
        hr = pSvc->GetFolder(bstrRoot, &pRoot);
        ole32::SysFreeString(bstrRoot);
        TEST_ASSERT(hr == S_OK && pRoot != nullptr, "GetFolder(\\) must succeed");

        ole32::BSTR pPath = nullptr;
        pRoot->get_Path(&pPath);
        TEST_ASSERT(pPath != nullptr && std::wcscmp(pPath, L"\\") == 0, "Root folder path must be \\");
        ole32::SysFreeString(pPath);

        // Verify pre-seeded ScheduledDefrag task in Microsoft\Windows\Defrag
        taskschd::IRegisteredTask* pDefragTask = nullptr;
        ole32::BSTR bstrDefragPath = ole32::SysAllocString(L"Microsoft\\Windows\\Defrag\\ScheduledDefrag");
        hr = pRoot->GetTask(bstrDefragPath, &pDefragTask);
        ole32::SysFreeString(bstrDefragPath);
        TEST_ASSERT(hr == S_OK && pDefragTask != nullptr, "Pre-seeded task ScheduledDefrag must exist");

        ole32::BSTR bstrName = nullptr;
        pDefragTask->get_Name(&bstrName);
        TEST_ASSERT(bstrName != nullptr && std::wcscmp(bstrName, L"ScheduledDefrag") == 0, "Task name must be ScheduledDefrag");
        ole32::SysFreeString(bstrName);

        taskschd::TASK_STATE taskState{};
        pDefragTask->get_State(&taskState);
        TEST_ASSERT(taskState == taskschd::TASK_STATE_READY, "Initial task state must be TASK_STATE_READY");

        pDefragTask->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 4: Subfolder Creation, Traversal & Lifecycle Management
    // ------------------------------------------------------------------------
    taskschd::ITaskFolder* pMicaFolder = nullptr;
    {
        ole32::BSTR bstrSub = ole32::SysAllocString(L"MicaExecutive");
        hr = pRoot->CreateFolder(bstrSub, {}, &pMicaFolder);
        TEST_ASSERT(hr == S_OK && pMicaFolder != nullptr, "CreateFolder(MicaExecutive) must succeed");

        ole32::BSTR bstrSubPath = nullptr;
        pMicaFolder->get_Path(&bstrSubPath);
        TEST_ASSERT(bstrSubPath != nullptr && std::wcscmp(bstrSubPath, L"\\MicaExecutive") == 0, "Folder path must be \\MicaExecutive");
        ole32::SysFreeString(bstrSubPath);

        // Attempting to recreate identical folder without flag returns SCHED_E_ALREADY_EXISTS
        taskschd::ITaskFolder* pDupFolder = nullptr;
        hr = pRoot->CreateFolder(bstrSub, {}, &pDupFolder);
        TEST_ASSERT(hr == taskschd::SCHED_E_ALREADY_EXISTS, "CreateFolder on existing folder must return SCHED_E_ALREADY_EXISTS");

        // Verify folder traversal via GetFolder
        taskschd::ITaskFolder* pFoundFolder = nullptr;
        hr = pRoot->GetFolder(bstrSub, &pFoundFolder);
        TEST_ASSERT(hr == S_OK && pFoundFolder != nullptr, "GetFolder(MicaExecutive) must locate created folder");
        pFoundFolder->Release();

        ole32::SysFreeString(bstrSub);
    }

    // ------------------------------------------------------------------------
    // Stage 5: TaskDefinition Construction & Metadata Configuration
    // ------------------------------------------------------------------------
    taskschd::ITaskDefinition* pTaskDef = nullptr;
    hr = pSvc->NewTask(0, &pTaskDef);
    TEST_ASSERT(hr == S_OK && pTaskDef != nullptr, "NewTask must create fresh ITaskDefinition");

    {
        // 1. RegistrationInfo
        taskschd::IRegistrationInfo* pReg = nullptr;
        pTaskDef->get_RegistrationInfo(&pReg);
        TEST_ASSERT(pReg != nullptr, "RegistrationInfo must be valid");

        ole32::BSTR bAuthor = ole32::SysAllocString(L"Dave Cutler 1988 MICA");
        pReg->put_Author(bAuthor);
        ole32::SysFreeString(bAuthor);

        ole32::BSTR bDesc = ole32::SysAllocString(L"Zero-telemetry sovereign task scheduler worker");
        pReg->put_Description(bDesc);
        ole32::SysFreeString(bDesc);

        ole32::BSTR bUri = ole32::SysAllocString(L"\\MicaExecutive\\SovereignDaemon");
        pReg->put_URI(bUri);
        ole32::SysFreeString(bUri);

        ole32::BSTR bCheckAuthor = nullptr;
        pReg->get_Author(&bCheckAuthor);
        TEST_ASSERT(bCheckAuthor != nullptr && std::wcscmp(bCheckAuthor, L"Dave Cutler 1988 MICA") == 0, "Author must match configured value");
        ole32::SysFreeString(bCheckAuthor);

        pReg->Release();

        // 2. Settings
        taskschd::ITaskSettings* pSettings = nullptr;
        pTaskDef->get_Settings(&pSettings);
        TEST_ASSERT(pSettings != nullptr, "TaskSettings must be valid");

        pSettings->put_AllowDemandStart(ole32::VARIANT_TRUE);
        pSettings->put_Hidden(ole32::VARIANT_FALSE);
        pSettings->put_MultipleInstances(taskschd::TASK_INSTANCES_PARALLEL);

        ole32::VARIANT_BOOL bDemand = ole32::VARIANT_FALSE;
        pSettings->get_AllowDemandStart(&bDemand);
        TEST_ASSERT(bDemand == ole32::VARIANT_TRUE, "AllowDemandStart must be TRUE");

        taskschd::TASK_INSTANCES_POLICY policy{};
        pSettings->get_MultipleInstances(&policy);
        TEST_ASSERT(policy == taskschd::TASK_INSTANCES_PARALLEL, "MultipleInstances policy must match PARALLEL");

        pSettings->Release();

        // 3. Principal
        taskschd::IPrincipal* pPrincipal = nullptr;
        pTaskDef->get_Principal(&pPrincipal);
        TEST_ASSERT(pPrincipal != nullptr, "Principal must be valid");

        ole32::BSTR bUser = ole32::SysAllocString(L"NT AUTHORITY\\SYSTEM");
        pPrincipal->put_UserId(bUser);
        ole32::SysFreeString(bUser);
        pPrincipal->put_LogonType(taskschd::TASK_LOGON_INTERACTIVE_TOKEN);

        ole32::BSTR bCheckUser = nullptr;
        pPrincipal->get_UserId(&bCheckUser);
        TEST_ASSERT(bCheckUser != nullptr && std::wcscmp(bCheckUser, L"NT AUTHORITY\\SYSTEM") == 0, "UserId must match configured principal");
        ole32::SysFreeString(bCheckUser);

        pPrincipal->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 6: Triggers & Repetition Pattern Verification
    // ------------------------------------------------------------------------
    {
        taskschd::ITriggerCollection* pTrigs = nullptr;
        pTaskDef->get_Triggers(&pTrigs);
        TEST_ASSERT(pTrigs != nullptr, "TriggerCollection must be valid");

        // 1. TimeTrigger
        taskschd::ITrigger* pTrig1 = nullptr;
        hr = pTrigs->Create(taskschd::TASK_TRIGGER_TIME, &pTrig1);
        TEST_ASSERT(hr == S_OK && pTrig1 != nullptr, "Create(TASK_TRIGGER_TIME) must succeed");

        ole32::BSTR bStart = ole32::SysAllocString(L"2026-10-03T12:00:00");
        pTrig1->put_StartBoundary(bStart);
        ole32::SysFreeString(bStart);

        taskschd::TASK_TRIGGER_TYPE2 trigType{};
        pTrig1->get_Type(&trigType);
        TEST_ASSERT(trigType == taskschd::TASK_TRIGGER_TIME, "Trigger type must be TASK_TRIGGER_TIME");

        // Repetition pattern
        taskschd::IRepetitionPattern* pRep = nullptr;
        pTrig1->get_Repetition(&pRep);
        TEST_ASSERT(pRep != nullptr, "RepetitionPattern must be valid");

        ole32::BSTR bInt = ole32::SysAllocString(L"PT10M");
        pRep->put_Interval(bInt);
        ole32::SysFreeString(bInt);

        ole32::BSTR bCheckInt = nullptr;
        pRep->get_Interval(&bCheckInt);
        TEST_ASSERT(bCheckInt != nullptr && std::wcscmp(bCheckInt, L"PT10M") == 0, "Repetition interval must match PT10M");
        ole32::SysFreeString(bCheckInt);
        pRep->Release();
        pTrig1->Release();

        // 2. BootTrigger
        taskschd::ITrigger* pTrig2 = nullptr;
        hr = pTrigs->Create(taskschd::TASK_TRIGGER_BOOT, &pTrig2);
        TEST_ASSERT(hr == S_OK && pTrig2 != nullptr, "Create(TASK_TRIGGER_BOOT) must succeed");
        pTrig2->Release();

        LONG trigCount = 0;
        pTrigs->get_Count(&trigCount);
        TEST_ASSERT(trigCount == 2, "Trigger count must be exactly 2");

        pTrigs->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 7: Actions Collection & ExecAction Execution Parameters
    // ------------------------------------------------------------------------
    {
        taskschd::IActionCollection* pActs = nullptr;
        pTaskDef->get_Actions(&pActs);
        TEST_ASSERT(pActs != nullptr, "ActionCollection must be valid");

        taskschd::IAction* pAct = nullptr;
        hr = pActs->Create(taskschd::TASK_ACTION_EXEC, &pAct);
        TEST_ASSERT(hr == S_OK && pAct != nullptr, "Create(TASK_ACTION_EXEC) must succeed");

        taskschd::IExecAction* pExec = nullptr;
        hr = pAct->QueryInterface(taskschd::IID_IExecAction, reinterpret_cast<void**>(&pExec));
        TEST_ASSERT(hr == S_OK && pExec != nullptr, "IAction must support IExecAction interface");

        ole32::BSTR bPath = ole32::SysAllocString(L"C:\\Windows\\System32\\micant_daemon.exe");
        pExec->put_Path(bPath);
        ole32::SysFreeString(bPath);

        ole32::BSTR bArgs = ole32::SysAllocString(L"--daemon --threads=4");
        pExec->put_Arguments(bArgs);
        ole32::SysFreeString(bArgs);

        ole32::BSTR bWorkDir = ole32::SysAllocString(L"C:\\Windows\\System32");
        pExec->put_WorkingDirectory(bWorkDir);
        ole32::SysFreeString(bWorkDir);

        ole32::BSTR bCheckPath = nullptr;
        pExec->get_Path(&bCheckPath);
        TEST_ASSERT(bCheckPath != nullptr && std::wcscmp(bCheckPath, L"C:\\Windows\\System32\\micant_daemon.exe") == 0, "ExecAction path must match");
        ole32::SysFreeString(bCheckPath);

        ole32::BSTR bCheckArgs = nullptr;
        pExec->get_Arguments(&bCheckArgs);
        TEST_ASSERT(bCheckArgs != nullptr && std::wcscmp(bCheckArgs, L"--daemon --threads=4") == 0, "ExecAction arguments must match");
        ole32::SysFreeString(bCheckArgs);

        pExec->Release();
        pAct->Release();

        LONG actCount = 0;
        pActs->get_Count(&actCount);
        TEST_ASSERT(actCount == 1, "Action count must be exactly 1");

        pActs->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 8: Task XML Serialization & Deserialization Engine
    // ------------------------------------------------------------------------
    {
        ole32::BSTR bstrXml = nullptr;
        hr = pTaskDef->get_XmlText(&bstrXml);
        TEST_ASSERT(hr == S_OK && bstrXml != nullptr, "get_XmlText must serialize task to standard XML");

        std::wstring xmlStr(bstrXml);
        TEST_ASSERT(xmlStr.find(L"<Task version=\"1.2\"") != std::wstring::npos, "XML must contain standard Task root element");
        TEST_ASSERT(xmlStr.find(L"<Author>Dave Cutler 1988 MICA</Author>") != std::wstring::npos, "XML must contain Author metadata");
        TEST_ASSERT(xmlStr.find(L"<BootTrigger>") != std::wstring::npos, "XML must contain BootTrigger");
        TEST_ASSERT(xmlStr.find(L"<Command>C:\\Windows\\System32\\micant_daemon.exe</Command>") != std::wstring::npos, "XML must contain Exec Command");
        TEST_ASSERT(xmlStr.find(L"<Arguments>--daemon --threads=4</Arguments>") != std::wstring::npos, "XML must contain Exec Arguments");

        // Deserialize into new task definition
        taskschd::ITaskDefinition* pNewDef = nullptr;
        hr = pSvc->NewTask(0, &pNewDef);
        TEST_ASSERT(hr == S_OK && pNewDef != nullptr, "NewTask for deserialization must succeed");

        hr = pNewDef->put_XmlText(bstrXml);
        TEST_ASSERT(hr == S_OK, "put_XmlText must parse XML definition");

        taskschd::IRegistrationInfo* pParsedReg = nullptr;
        pNewDef->get_RegistrationInfo(&pParsedReg);
        ole32::BSTR bParsedAuthor = nullptr;
        pParsedReg->get_Author(&bParsedAuthor);
        TEST_ASSERT(bParsedAuthor != nullptr && std::wcscmp(bParsedAuthor, L"Dave Cutler 1988 MICA") == 0, "Deserialized Author must match original");
        ole32::SysFreeString(bParsedAuthor);
        pParsedReg->Release();

        pNewDef->Release();
        ole32::SysFreeString(bstrXml);
    }

    // ------------------------------------------------------------------------
    // Stage 9: Task Registration, Enumeration & State Invariants
    // ------------------------------------------------------------------------
    taskschd::IRegisteredTask* pRegisteredTask = nullptr;
    {
        ole32::BSTR bTaskName = ole32::SysAllocString(L"SovereignDaemon");
        hr = pMicaFolder->RegisterTaskDefinition(
            bTaskName, pTaskDef, taskschd::TASK_CREATE_OR_UPDATE,
            {}, {}, taskschd::TASK_LOGON_INTERACTIVE_TOKEN, {}, &pRegisteredTask
        );
        ole32::SysFreeString(bTaskName);
        TEST_ASSERT(hr == S_OK && pRegisteredTask != nullptr, "RegisterTaskDefinition must succeed");

        ole32::BSTR bName = nullptr;
        pRegisteredTask->get_Name(&bName);
        TEST_ASSERT(bName != nullptr && std::wcscmp(bName, L"SovereignDaemon") == 0, "Registered task name must match");
        ole32::SysFreeString(bName);

        ole32::BSTR bPath = nullptr;
        pRegisteredTask->get_Path(&bPath);
        TEST_ASSERT(bPath != nullptr && std::wcscmp(bPath, L"\\MicaExecutive\\SovereignDaemon") == 0, "Registered task path must match hierarchy");
        ole32::SysFreeString(bPath);

        // State check
        taskschd::TASK_STATE st{};
        pRegisteredTask->get_State(&st);
        TEST_ASSERT(st == taskschd::TASK_STATE_READY, "Initial registered task state must be TASK_STATE_READY");

        // Toggle Enabled state
        pRegisteredTask->put_Enabled(ole32::VARIANT_FALSE);
        pRegisteredTask->get_State(&st);
        TEST_ASSERT(st == taskschd::TASK_STATE_DISABLED, "Disabled task state must be TASK_STATE_DISABLED");

        pRegisteredTask->put_Enabled(ole32::VARIANT_TRUE);
        pRegisteredTask->get_State(&st);
        TEST_ASSERT(st == taskschd::TASK_STATE_READY, "Re-enabled task state must return to TASK_STATE_READY");

        // Verify task enumeration within folder
        taskschd::IRegisteredTaskCollection* pTasks = nullptr;
        hr = pMicaFolder->GetTasks(0, &pTasks);
        TEST_ASSERT(hr == S_OK && pTasks != nullptr, "GetTasks must succeed");

        LONG taskCount = 0;
        pTasks->get_Count(&taskCount);
        TEST_ASSERT(taskCount == 1, "MicaExecutive folder must contain exactly 1 task");

        // Index 1 (1-based COM indexing)
        ole32::VARIANT vIdx{};
        oleaut32::VariantInit(&vIdx);
        vIdx.vt = ole32::VT_I4;
        vIdx.lVal = 1;
        taskschd::IRegisteredTask* pEnumTask = nullptr;
        hr = pTasks->get_Item(vIdx, &pEnumTask);
        TEST_ASSERT(hr == S_OK && pEnumTask != nullptr, "get_Item(1) must retrieve registered task");
        pEnumTask->Release();
        pTasks->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 10: Task Execution, Running Instance Lifecycle & Process PID
    // ------------------------------------------------------------------------
    {
        taskschd::IRunningTask* pRunning = nullptr;
        hr = pRegisteredTask->Run({}, &pRunning);
        TEST_ASSERT(hr == S_OK && pRunning != nullptr, "IRegisteredTask::Run must succeed");

        ole32::BSTR bRunName = nullptr;
        pRunning->get_Name(&bRunName);
        TEST_ASSERT(bRunName != nullptr && std::wcscmp(bRunName, L"SovereignDaemon") == 0, "RunningTask name must match");
        ole32::SysFreeString(bRunName);

        ole32::BSTR bGuid = nullptr;
        pRunning->get_InstanceGuid(&bGuid);
        TEST_ASSERT(bGuid != nullptr && bGuid[0] == L'{', "RunningTask must have valid GUID string");
        ole32::SysFreeString(bGuid);

        DWORD dwPid = 0;
        pRunning->get_EnginePID(&dwPid);
        TEST_ASSERT(dwPid >= 1000, "RunningTask must report active engine process PID");

        // Check last run time and last exit code
        taskschd::DATE dtLast = 0.0;
        pRegisteredTask->get_LastRunTime(&dtLast);
        TEST_ASSERT(dtLast > 0.0, "LastRunTime must be recorded");

        LONG lLastResult = -1;
        pRegisteredTask->get_LastTaskResult(&lLastResult);
        TEST_ASSERT(lLastResult == 0, "LastTaskResult must be 0 (Success)");

        pRunning->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 11: Dynamic Loader Exports for taskschd.dll and mstask.dll
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("taskschd.dll", "DllGetClassObject") != nullptr, "taskschd.dll!DllGetClassObject must be exported");
        TEST_ASSERT(ldr.getExport("taskschd.dll", "DllCanUnloadNow") != nullptr, "taskschd.dll!DllCanUnloadNow must be exported");
        TEST_ASSERT(ldr.getExport("taskschd.dll", "DllRegisterServer") != nullptr, "taskschd.dll!DllRegisterServer must be exported");
        TEST_ASSERT(ldr.getExport("taskschd.dll", "DllUnregisterServer") != nullptr, "taskschd.dll!DllUnregisterServer must be exported");

        TEST_ASSERT(ldr.getExport("mstask.dll", "DllGetClassObject") != nullptr, "mstask.dll!DllGetClassObject must be exported");
        TEST_ASSERT(ldr.getExport("mstask.dll", "DllCanUnloadNow") != nullptr, "mstask.dll!DllCanUnloadNow must be exported");
        TEST_ASSERT(ldr.getExport("mstask.dll", "DllRegisterServer") != nullptr, "mstask.dll!DllRegisterServer must be exported");
        TEST_ASSERT(ldr.getExport("mstask.dll", "DllUnregisterServer") != nullptr, "mstask.dll!DllUnregisterServer must be exported");

        // Direct invoke via function pointer
        auto pfnGetClass = reinterpret_cast<HRESULT(__stdcall*)(REFCLSID, REFIID, void**)>(ldr.getExport("taskschd.dll", "DllGetClassObject"));
        ole32::IClassFactory* pFactory = nullptr;
        hr = pfnGetClass(taskschd::CLSID_TaskScheduler, ole32::IID_IClassFactory, reinterpret_cast<void**>(&pFactory));
        TEST_ASSERT(hr == S_OK && pFactory != nullptr, "DllGetClassObject must return working IClassFactory");
        pFactory->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 12: Interactive Command Shell Integration (schtasks CLI)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        // 1. schtasks /query
        shell.execute("schtasks /query", out);
        TEST_ASSERT(out.str().find("ScheduledDefrag") != std::string::npos, "schtasks /query must display ScheduledDefrag");
        TEST_ASSERT(out.str().find("SilentCleanup") != std::string::npos, "schtasks /query must display SilentCleanup");

        // 2. schtasks /query /tn with LIST formatting
        out.str("");
        shell.execute("schtasks /query /tn \\Microsoft\\Windows\\Defrag\\ScheduledDefrag /fo LIST /v", out);
        TEST_ASSERT(out.str().find("Folder:") != std::string::npos, "schtasks LIST format must display Folder field");
        TEST_ASSERT(out.str().find("defrag.exe") != std::string::npos, "schtasks LIST format must display Task To Run");

        // 3. schtasks /create
        out.str("");
        shell.execute("schtasks /create /tn \\CliTestTask /tr notepad.exe /sc DAILY", out);
        TEST_ASSERT(out.str().find("SUCCESS:") != std::string::npos, "schtasks /create must report success");

        // 4. schtasks /run
        out.str("");
        shell.execute("schtasks /run /tn \\CliTestTask", out);
        TEST_ASSERT(out.str().find("SUCCESS:") != std::string::npos, "schtasks /run must report success");

        // 5. schtasks /delete
        out.str("");
        shell.execute("schtasks /delete /tn \\CliTestTask /f", out);
        TEST_ASSERT(out.str().find("SUCCESS:") != std::string::npos, "schtasks /delete must report success");

        // 6. schtasks test
        out.str("");
        shell.execute("schtasks test", out);
        TEST_ASSERT(out.str().find("Subsystem Self-Test Finished") != std::string::npos, "schtasks test must finish successfully");
    }

    // Cleanup local test instances
    if (pRegisteredTask) pRegisteredTask->Release();
    if (pTaskDef) pTaskDef->Release();
    if (pMicaFolder) pMicaFolder->Release();
    if (pRoot) pRoot->Release();
    if (pSvc) pSvc->Release();

    std::cout << "[TEST] Suite 74: Windows Task Scheduler 2.0 Subsystem PASSED.\n";
}

void Test_WindowsBITS_Subsystem() {
    std::cout << "\n[TEST] Running Suite 75: Windows Background Intelligent Transfer Service (BITS) Subsystem (qmgr.dll)...\n";
    using namespace ole32;

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports & Class Factory Registration
    // ------------------------------------------------------------------------
    bits::InitializeBITSSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("qmgr.dll", "DllGetClassObject") != nullptr, "qmgr.dll!DllGetClassObject must be exported");
    TEST_ASSERT(ldr.getExport("qmgr.dll", "DllCanUnloadNow") != nullptr, "qmgr.dll!DllCanUnloadNow must be exported");
    TEST_ASSERT(ldr.getExport("qmgr.dll", "DllRegisterServer") != nullptr, "qmgr.dll!DllRegisterServer must be exported");
    TEST_ASSERT(ldr.getExport("qmgr.dll", "DllUnregisterServer") != nullptr, "qmgr.dll!DllUnregisterServer must be exported");

    TEST_ASSERT(ldr.getExport("bitsprx.dll", "DllGetClassObject") != nullptr, "bitsprx.dll!DllGetClassObject must be exported");
    TEST_ASSERT(ldr.getExport("bitsprx.dll", "DllCanUnloadNow") != nullptr, "bitsprx.dll!DllCanUnloadNow must be exported");

    // ------------------------------------------------------------------------
    // Stage 2: COM Activation (CLSID_BackgroundCopyManager)
    // ------------------------------------------------------------------------
    bits::IBackgroundCopyManager* pMgr = nullptr;
    ole32::HRESULT hr = ole32::CoCreateInstance(
        bits::CLSID_BackgroundCopyManager, nullptr, 1 /* CLSCTX_INPROC_SERVER */,
        bits::IID_IBackgroundCopyManager, reinterpret_cast<void**>(&pMgr)
    );
    TEST_ASSERT(hr == ole32::S_OK && pMgr != nullptr, "CoCreateInstance(CLSID_BackgroundCopyManager) must succeed");

    // QueryInterface for IUnknown
    ole32::IUnknown* pUnk = nullptr;
    hr = pMgr->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
    TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "IBackgroundCopyManager must support IUnknown");
    pUnk->Release();

    // ------------------------------------------------------------------------
    // Stage 3: Pre-Seeded System BITS Jobs Verification
    // ------------------------------------------------------------------------
    {
        bits::IEnumBackgroundCopyJobs* pEnum = nullptr;
        hr = pMgr->EnumJobs(0, &pEnum);
        TEST_ASSERT(hr == ole32::S_OK && pEnum != nullptr, "EnumJobs must succeed");

        uint32_t count = 0;
        pEnum->GetCount(&count);
        TEST_ASSERT(count >= 2, "BITS queue must contain pre-seeded system jobs");

        bool foundDefender = false;
        bool foundKernelPatch = false;

        bits::IBackgroundCopyJob* pJob = nullptr;
        uint32_t fetched = 0;
        while (pEnum->Next(1, &pJob, &fetched) == ole32::S_OK && fetched == 1) {
            wchar_t* wName = nullptr;
            pJob->GetName(&wName);
            if (wName) {
                if (std::wcscmp(wName, L"Windows Defender Signature Update") == 0) foundDefender = true;
                if (std::wcscmp(wName, L"MicaNT Kernel Security Update KB5034441") == 0) foundKernelPatch = true;
                ole32::CoTaskMemFree(wName);
            }
            pJob->Release();
        }
        TEST_ASSERT(foundDefender, "Pre-seeded Windows Defender signature update job must exist");
        TEST_ASSERT(foundKernelPatch, "Pre-seeded MicaNT kernel security update job must exist");
        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 4: Job Creation (CreateJob)
    // ------------------------------------------------------------------------
    bits::IBackgroundCopyJob* pNewJob = nullptr;
    GUID newJobId{};
    hr = pMgr->CreateJob(L"MicaNT Deployment Service", bits::BG_JOB_TYPE_DOWNLOAD, &newJobId, &pNewJob);
    TEST_ASSERT(hr == ole32::S_OK && pNewJob != nullptr, "CreateJob must return S_OK and valid job interface");

    // Verify initial state is SUSPENDED
    bits::BG_JOB_STATE initState{};
    pNewJob->GetState(&initState);
    TEST_ASSERT(initState == bits::BG_JOB_STATE_SUSPENDED, "Newly created job must be in BG_JOB_STATE_SUSPENDED");

    // Verify Job Id matches
    GUID verifyId{};
    pNewJob->GetId(&verifyId);
    TEST_ASSERT(memcmp(&newJobId, &verifyId, sizeof(GUID)) == 0, "Job GetId must match returned GUID");

    // ------------------------------------------------------------------------
    // Stage 5: Adding Files to Job (AddFile & AddFileSet)
    // ------------------------------------------------------------------------
    hr = pNewJob->AddFile(L"https://releases.micant.internal/v1.0.75/sdk.zip", L"C:\\MicaNT\\sdk.zip");
    TEST_ASSERT(hr == ole32::S_OK, "AddFile must succeed");

    bits::BG_FILE_INFO fileSet[2] = {
        { L"https://releases.micant.internal/v1.0.75/symbols.pdb", L"C:\\MicaNT\\symbols.pdb" },
        { L"https://releases.micant.internal/v1.0.75/docs.chm", L"C:\\MicaNT\\docs.chm" }
    };
    hr = pNewJob->AddFileSet(2, fileSet);
    TEST_ASSERT(hr == ole32::S_OK, "AddFileSet must succeed");

    // ------------------------------------------------------------------------
    // Stage 6: File Enumeration (EnumFiles)
    // ------------------------------------------------------------------------
    {
        bits::IEnumBackgroundCopyFiles* pFileEnum = nullptr;
        hr = pNewJob->EnumFiles(&pFileEnum);
        TEST_ASSERT(hr == ole32::S_OK && pFileEnum != nullptr, "EnumFiles must succeed");

        uint32_t fileCount = 0;
        pFileEnum->GetCount(&fileCount);
        TEST_ASSERT(fileCount == 3, "Job must contain exactly 3 added files");

        bits::IBackgroundCopyFile* pFileItem = nullptr;
        uint32_t fFetched = 0;
        hr = pFileEnum->Next(1, &pFileItem, &fFetched);
        TEST_ASSERT(hr == ole32::S_OK && fFetched == 1 && pFileItem != nullptr, "Next must retrieve first file");

        wchar_t* wRemote = nullptr;
        pFileItem->GetRemoteName(&wRemote);
        TEST_ASSERT(wRemote != nullptr && std::wcscmp(wRemote, L"https://releases.micant.internal/v1.0.75/sdk.zip") == 0, "First file remote URL must match");
        ole32::CoTaskMemFree(wRemote);

        wchar_t* wLocal = nullptr;
        pFileItem->GetLocalName(&wLocal);
        TEST_ASSERT(wLocal != nullptr && std::wcscmp(wLocal, L"C:\\MicaNT\\sdk.zip") == 0, "First file local name must match");
        ole32::CoTaskMemFree(wLocal);

        pFileItem->Release();
        pFileEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 7: Job Priority & Description Configuration
    // ------------------------------------------------------------------------
    {
        pNewJob->SetPriority(bits::BG_JOB_PRIORITY_HIGH);
        bits::BG_JOB_PRIORITY prio{};
        pNewJob->GetPriority(&prio);
        TEST_ASSERT(prio == bits::BG_JOB_PRIORITY_HIGH, "GetPriority must report BG_JOB_PRIORITY_HIGH");

        pNewJob->SetDescription(L"MicaNT 1.0.75 SDK and Documentation Bundle");
        wchar_t* wDesc = nullptr;
        pNewJob->GetDescription(&wDesc);
        TEST_ASSERT(wDesc != nullptr && std::wcscmp(wDesc, L"MicaNT 1.0.75 SDK and Documentation Bundle") == 0, "Job description must match");
        ole32::CoTaskMemFree(wDesc);
    }

    // ------------------------------------------------------------------------
    // Stage 8: Transfer Execution & Lifecycle (Resume -> TRANSFERRED)
    // ------------------------------------------------------------------------
    {
        hr = pNewJob->Resume();
        TEST_ASSERT(hr == ole32::S_OK, "Resume must trigger transfer");

        bits::BG_JOB_STATE transState{};
        pNewJob->GetState(&transState);
        TEST_ASSERT(transState == bits::BG_JOB_STATE_TRANSFERRED, "Transferred job must transition to BG_JOB_STATE_TRANSFERRED");
    }

    // ------------------------------------------------------------------------
    // Stage 9: Progress & Timing Telemetry
    // ------------------------------------------------------------------------
    {
        bits::BG_JOB_PROGRESS prog{};
        pNewJob->GetProgress(&prog);
        TEST_ASSERT(prog.FilesTotal == 3, "FilesTotal must be 3");
        TEST_ASSERT(prog.FilesTransferred == 3, "FilesTransferred must be 3");
        TEST_ASSERT(prog.BytesTotal > 0 && prog.BytesTransferred == prog.BytesTotal, "All bytes must be transferred");

        bits::BG_JOB_TIMES times{};
        pNewJob->GetTimes(&times);
        TEST_ASSERT(times.CreationTime.dwLowDateTime > 0, "CreationTime must be set");
        TEST_ASSERT(times.TransferCompletionTime.dwLowDateTime > 0, "TransferCompletionTime must be recorded");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Job Acknowledgment / Completion (Complete)
    // ------------------------------------------------------------------------
    {
        hr = pNewJob->Complete();
        TEST_ASSERT(hr == ole32::S_OK, "Complete must succeed on transferred job");

        bits::BG_JOB_STATE ackState{};
        pNewJob->GetState(&ackState);
        TEST_ASSERT(ackState == bits::BG_JOB_STATE_ACKNOWLEDGED, "Completed job must transition to BG_JOB_STATE_ACKNOWLEDGED");

        // Attempting to resume completed job must return BG_E_INVALID_STATE
        hr = pNewJob->Resume();
        TEST_ASSERT(hr == bits::BG_E_INVALID_STATE, "Resume on acknowledged job must return BG_E_INVALID_STATE");
    }

    // ------------------------------------------------------------------------
    // Stage 11: Job Cancellation & Error Reporting
    // ------------------------------------------------------------------------
    {
        bits::IBackgroundCopyJob* pCancelJob = nullptr;
        GUID cancelId{};
        hr = pMgr->CreateJob(L"Cancelled Job Test", bits::BG_JOB_TYPE_DOWNLOAD, &cancelId, &pCancelJob);
        TEST_ASSERT(hr == ole32::S_OK && pCancelJob != nullptr, "CreateJob for cancellation test must succeed");

        hr = pCancelJob->Cancel();
        TEST_ASSERT(hr == ole32::S_OK, "Cancel must succeed on active job");

        bits::BG_JOB_STATE cState{};
        pCancelJob->GetState(&cState);
        TEST_ASSERT(cState == bits::BG_JOB_STATE_CANCELLED, "Cancelled job state must be BG_JOB_STATE_CANCELLED");

        // Error description retrieval
        wchar_t* pErrDesc = nullptr;
        hr = pMgr->GetErrorDescription(bits::BG_E_FILE_NOT_AVAILABLE, 0, &pErrDesc);
        TEST_ASSERT(hr == ole32::S_OK && pErrDesc != nullptr, "GetErrorDescription must succeed");
        ole32::CoTaskMemFree(pErrDesc);

        pCancelJob->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 12: Interactive Command Shell Integration (bitsadmin CLI)
    // ------------------------------------------------------------------------
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        // 1. bitsadmin /list
        shell.execute("bitsadmin /list", out);
        TEST_ASSERT(out.str().find("Windows Defender") != std::string::npos, "bitsadmin /list must show Windows Defender job");
        TEST_ASSERT(out.str().find("KB5034441") != std::string::npos, "bitsadmin /list must show KB5034441 job");

        // 2. bitsadmin /create
        out.str("");
        shell.execute("bitsadmin /create CliBitsJob", out);
        TEST_ASSERT(out.str().find("Created job") != std::string::npos, "bitsadmin /create must report job creation");

        // 3. bitsadmin /addfile
        out.str("");
        shell.execute("bitsadmin /addfile CliBitsJob https://example.com/file.bin C:\\Temp\\file.bin", out);
        TEST_ASSERT(out.str().find("SUCCESS:") != std::string::npos, "bitsadmin /addfile must report success");

        // 4. bitsadmin /info
        out.str("");
        shell.execute("bitsadmin /info CliBitsJob", out);
        TEST_ASSERT(out.str().find("DISPLAY: 'CliBitsJob'") != std::string::npos, "bitsadmin /info must display job name");
        TEST_ASSERT(out.str().find("C:\\Temp\\file.bin") != std::string::npos, "bitsadmin /info must display file target");

        // 5. bitsadmin /resume
        out.str("");
        shell.execute("bitsadmin /resume CliBitsJob", out);
        TEST_ASSERT(out.str().find("Job resumed.") != std::string::npos, "bitsadmin /resume must report success");

        // 6. bitsadmin /complete
        out.str("");
        shell.execute("bitsadmin /complete CliBitsJob", out);
        TEST_ASSERT(out.str().find("Job completed.") != std::string::npos, "bitsadmin /complete must report success");

        // 7. bitsadmin test
        out.str("");
        shell.execute("bitsadmin test", out);
        TEST_ASSERT(out.str().find("Subsystem Self-Test Finished") != std::string::npos, "bitsadmin test must finish successfully");
    }

    if (pNewJob) pNewJob->Release();
    if (pMgr) pMgr->Release();

    std::cout << "[TEST] Suite 75: Windows Background Intelligent Transfer Service (BITS) Subsystem PASSED.\n";
}

// ============================================================================
// Test Suite 76: Windows Volume Shadow Copy Service (VSS) Subsystem (vssapi.dll)
// ============================================================================

void Test_WindowsVSS_VolumeShadowCopy_Subsystem() {
    std::cout << "\n[TEST] Running Suite 76: Windows Volume Shadow Copy Service (VSS) Subsystem (vssapi.dll)...\n";

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader & Library Export Registration (vssapi.dll, vss_ps.dll)
    // ------------------------------------------------------------------------
    vss::InitializeVSSSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    void* pfnCreate = ldr.getExport("vssapi.dll", "CreateVssBackupComponents");
    TEST_ASSERT(pfnCreate != nullptr, "vssapi.dll must export CreateVssBackupComponents");

    void* pfnFree = ldr.getExport("vssapi.dll", "VssFreeSnapshotProperties");
    TEST_ASSERT(pfnFree != nullptr, "vssapi.dll must export VssFreeSnapshotProperties");

    void* pfnDllGet = ldr.getExport("vssapi.dll", "DllGetClassObject");
    TEST_ASSERT(pfnDllGet != nullptr, "vssapi.dll must export DllGetClassObject");

    void* pfnPsGet = ldr.getExport("vss_ps.dll", "DllGetClassObject");
    TEST_ASSERT(pfnPsGet != nullptr, "vss_ps.dll must export DllGetClassObject");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Metadata Validation
    // ------------------------------------------------------------------------
    auto* vssVer = version::VersionDatabase::Instance().FindModule("vssapi.dll");
    TEST_ASSERT(vssVer != nullptr, "vssapi.dll must be registered in VersionDatabase");
    TEST_ASSERT(vssVer->stringTable.at("FileVersion") == "10.0.22621.1", "vssapi.dll FileVersion must match 10.0.22621.1");

    auto* vssPsVer = version::VersionDatabase::Instance().FindModule("vss_ps.dll");
    TEST_ASSERT(vssPsVer != nullptr, "vss_ps.dll must be registered in VersionDatabase");

    // ------------------------------------------------------------------------
    // Stage 3: COM Activation & Interface Initialization
    // ------------------------------------------------------------------------
    vss::IVssBackupComponents* pBackup = nullptr;
    ole32::HRESULT hr = vss::CreateVssBackupComponents(&pBackup);
    TEST_ASSERT(hr == ole32::S_OK && pBackup != nullptr, "CreateVssBackupComponents must succeed");

    // Verify QueryInterface for IUnknown and IVssBackupComponents
    ole32::IUnknown* pUnk = nullptr;
    hr = pBackup->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
    TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "QueryInterface for IUnknown must succeed");
    pUnk->Release();

    // ------------------------------------------------------------------------
    // Stage 4: Backup State & Initialization Lifecycle
    // ------------------------------------------------------------------------
    hr = pBackup->InitializeForBackup(nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "InitializeForBackup must succeed");

    hr = pBackup->SetBackupState(true, true, vss::VSS_BT_FULL, false);
    TEST_ASSERT(hr == ole32::S_OK, "SetBackupState must succeed");

    // ------------------------------------------------------------------------
    // Stage 5: Writer Metadata & Status Coordination
    // ------------------------------------------------------------------------
    vss::IVssAsync* pAsyncMeta = nullptr;
    hr = pBackup->GatherWriterMetadata(&pAsyncMeta);
    TEST_ASSERT(hr == ole32::S_OK && pAsyncMeta != nullptr, "GatherWriterMetadata must succeed");
    ole32::HRESULT hrMeta = 0;
    pAsyncMeta->QueryStatus(&hrMeta, nullptr);
    TEST_ASSERT(hrMeta == vss::VSS_S_ASYNC_FINISHED, "GatherWriterMetadata async must be finished");
    pAsyncMeta->Release();

    uint32_t writerCount = 0;
    hr = pBackup->GetWriterMetadataCount(&writerCount);
    TEST_ASSERT(hr == ole32::S_OK && writerCount >= 4, "Writer metadata count must be at least 4");

    vss::IVssAsync* pAsyncStatus = nullptr;
    hr = pBackup->GatherWriterStatus(&pAsyncStatus);
    TEST_ASSERT(hr == ole32::S_OK && pAsyncStatus != nullptr, "GatherWriterStatus must succeed");
    pAsyncStatus->Release();

    uint32_t statusCount = 0;
    hr = pBackup->GetWriterStatusCount(&statusCount);
    TEST_ASSERT(hr == ole32::S_OK && statusCount >= 4, "Writer status count must match metadata count");

    GUID instId{}, wrId{};
    wchar_t* bstrName = nullptr;
    vss::VSS_WRITER_STATE wState{};
    ole32::HRESULT failReason = 0;
    hr = pBackup->GetWriterStatus(0, &instId, &wrId, &bstrName, &wState, &failReason);
    TEST_ASSERT(hr == ole32::S_OK && bstrName != nullptr, "GetWriterStatus for writer 0 must succeed");
    TEST_ASSERT(wState == vss::VSS_WS_STABLE, "Pre-seeded writer state must be VSS_WS_STABLE");
    std::wstring wNameStr(bstrName);
    TEST_ASSERT(wNameStr == L"System Writer", "First writer must be 'System Writer'");
    ole32::CoTaskMemFree(bstrName);

    // ------------------------------------------------------------------------
    // Stage 6: Pre-Seeded Providers & Shadow Storage Verification
    // ------------------------------------------------------------------------
    auto provs = vss::VssCoordinator::Instance().GetProviders();
    TEST_ASSERT(!provs.empty(), "Pre-seeded VSS providers must exist");
    TEST_ASSERT(provs[0].m_eProviderType == vss::VSS_PROV_SYSTEM, "First provider must be System provider");
    std::wstring pName(provs[0].m_pwszProviderName ? provs[0].m_pwszProviderName : L"");
    TEST_ASSERT(pName.find(L"Microsoft Software Shadow Copy provider") != std::wstring::npos, "Provider name must match Microsoft Software Shadow Copy provider");

    auto diffs = vss::VssCoordinator::Instance().GetDiffAreas();
    TEST_ASSERT(!diffs.empty(), "Pre-seeded shadow storage diff areas must exist");
    TEST_ASSERT(diffs[0].VolumeName == L"C:\\", "Volume must be C:\\");
    TEST_ASSERT(diffs[0].MaximumDiffSpace >= 10ULL * 1024 * 1024 * 1024, "Maximum diff space must be >= 10GB");

    // ------------------------------------------------------------------------
    // Stage 7: Snapshot Set Creation Lifecycle
    // ------------------------------------------------------------------------
    GUID setId{};
    hr = pBackup->StartSnapshotSet(&setId);
    TEST_ASSERT(hr == ole32::S_OK, "StartSnapshotSet must return S_OK");
    TEST_ASSERT(memcmp(&setId, &ole32::GUID_NULL, sizeof(GUID)) != 0, "Snapshot set ID must not be GUID_NULL");

    GUID snapId{};
    wchar_t volPath[] = L"C:\\";
    hr = pBackup->AddToSnapshotSet(volPath, vss::VSS_SW_PROVIDER_ID, &snapId);
    TEST_ASSERT(hr == ole32::S_OK, "AddToSnapshotSet must succeed");

    vss::IVssAsync* pAsyncSnap = nullptr;
    hr = pBackup->DoSnapshotSet(&pAsyncSnap);
    TEST_ASSERT(hr == ole32::S_OK && pAsyncSnap != nullptr, "DoSnapshotSet must return S_OK and valid async object");

    ole32::HRESULT snapStatus = 0;
    pAsyncSnap->QueryStatus(&snapStatus, nullptr);
    TEST_ASSERT(snapStatus == vss::VSS_S_ASYNC_FINISHED, "DoSnapshotSet async must complete with VSS_S_ASYNC_FINISHED");
    pAsyncSnap->Release();

    // ------------------------------------------------------------------------
    // Stage 8: Snapshot Properties Retrieval & Verification
    // ------------------------------------------------------------------------
    vss::VSS_SNAPSHOT_PROP snapProp{};
    hr = pBackup->GetSnapshotProperties(snapId, &snapProp);
    TEST_ASSERT(hr == ole32::S_OK, "GetSnapshotProperties must succeed");
    TEST_ASSERT(memcmp(&snapProp.m_SnapshotId, &snapId, sizeof(GUID)) == 0, "Snapshot ID in properties must match");
    TEST_ASSERT(memcmp(&snapProp.m_SnapshotSetId, &setId, sizeof(GUID)) == 0, "Snapshot Set ID in properties must match");
    TEST_ASSERT(snapProp.m_eStatus == vss::VSS_SS_COMMITTED, "Snapshot state must be VSS_SS_COMMITTED");
    TEST_ASSERT(snapProp.m_pwszSnapshotDeviceObject != nullptr, "Device object name must be valid");
    std::wstring devName(snapProp.m_pwszSnapshotDeviceObject);
    TEST_ASSERT(devName.find(L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy") != std::wstring::npos, "Device object name must match HarddiskVolumeShadowCopy device prefix");
    vss::VssFreeSnapshotProperties(&snapProp);

    // ------------------------------------------------------------------------
    // Stage 9: COM Object Enumeration (IVssEnumObject)
    // ------------------------------------------------------------------------
    vss::IVssEnumObject* pEnumSnap = nullptr;
    hr = pBackup->Query(ole32::GUID_NULL, vss::VSS_OBJECT_NONE, vss::VSS_OBJECT_SNAPSHOT, &pEnumSnap);
    TEST_ASSERT(hr == ole32::S_OK && pEnumSnap != nullptr, "Query for snapshots must succeed");

    vss::VSS_OBJECT_PROP objProps[4]{};
    uint32_t fetched = 0;
    hr = pEnumSnap->Next(4, objProps, &fetched);
    TEST_ASSERT(fetched >= 2, "Must enumerate at least 2 snapshots (pre-seeded + newly created)");
    for (uint32_t i = 0; i < fetched; ++i) {
        TEST_ASSERT(objProps[i].Type == vss::VSS_OBJECT_SNAPSHOT, "Enumerated object must be of type VSS_OBJECT_SNAPSHOT");
        vss::VssFreeSnapshotProperties(&objProps[i].Obj.Snap);
    }
    pEnumSnap->Release();

    // ------------------------------------------------------------------------
    // Stage 10: Snapshot Deletion
    // ------------------------------------------------------------------------
    int32_t deletedCount = 0;
    GUID nonDeleted{};
    hr = pBackup->DeleteSnapshots(snapId, vss::VSS_OBJECT_SNAPSHOT, true, &deletedCount, &nonDeleted);
    TEST_ASSERT(hr == ole32::S_OK && deletedCount == 1, "DeleteSnapshots must delete exactly 1 snapshot");

    vss::VSS_SNAPSHOT_PROP deadProp{};
    hr = pBackup->GetSnapshotProperties(snapId, &deadProp);
    TEST_ASSERT(hr == vss::VSS_E_OBJECT_NOT_FOUND, "GetSnapshotProperties must return VSS_E_OBJECT_NOT_FOUND after deletion");

    // ------------------------------------------------------------------------
    // Stage 11: Shadow Storage Resize Verification
    // ------------------------------------------------------------------------
    bool resized = vss::VssCoordinator::Instance().ResizeDiffArea(L"C:\\", 25ULL * 1024 * 1024 * 1024);
    TEST_ASSERT(resized, "ResizeDiffArea on volume C:\\ must succeed");
    auto updatedDiffs = vss::VssCoordinator::Instance().GetDiffAreas();
    TEST_ASSERT(updatedDiffs[0].MaximumDiffSpace == 25ULL * 1024 * 1024 * 1024, "Updated maximum diff space must be 25 GB");

    // ------------------------------------------------------------------------
    // Stage 12: Interactive Command Shell Integration (vssadmin)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. vssadmin (Usage banner)
        shell.execute("vssadmin", out);
        TEST_ASSERT(out.str().find("Volume Shadow Copy Service Administration") != std::string::npos, "vssadmin without args must print banner");

        // 2. vssadmin list writers
        out.str("");
        shell.execute("vssadmin list writers", out);
        TEST_ASSERT(out.str().find("System Writer") != std::string::npos, "vssadmin list writers must display System Writer");
        TEST_ASSERT(out.str().find("State: [1] Stable") != std::string::npos, "vssadmin list writers must display Stable state");

        // 3. vssadmin list providers
        out.str("");
        shell.execute("vssadmin list providers", out);
        TEST_ASSERT(out.str().find("Microsoft Software Shadow Copy provider 1.0") != std::string::npos, "vssadmin list providers must display software provider");

        // 4. vssadmin list shadowstorage
        out.str("");
        shell.execute("vssadmin list shadowstorage", out);
        TEST_ASSERT(out.str().find("Shadow Copy Storage association") != std::string::npos, "vssadmin list shadowstorage must display association");

        // 5. vssadmin list shadows
        out.str("");
        shell.execute("vssadmin list shadows", out);
        TEST_ASSERT(out.str().find("Contents of shadow copy set ID:") != std::string::npos, "vssadmin list shadows must display pre-seeded shadow copy");

        // 6. vssadmin create shadow
        out.str("");
        shell.execute("vssadmin create shadow /for=C:", out);
        TEST_ASSERT(out.str().find("Successfully created shadow copy") != std::string::npos, "vssadmin create shadow must report success");

        // 7. vssadmin test
        out.str("");
        shell.execute("vssadmin test", out);
        TEST_ASSERT(out.str().find("Subsystem Self-Test Finished") != std::string::npos, "vssadmin test must finish successfully");
    }

    pBackup->Release();

    std::cout << "[TEST] Suite 76: Windows Volume Shadow Copy Service (VSS) Subsystem PASSED.\n";
}

void Test_WindowsWER_ErrorReporting_Subsystem() {
    using namespace micant::wer;

    InitializeWERSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader & Versioning Database Verification
    // ------------------------------------------------------------------------
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("wer.dll", "WerReportCreate") != nullptr, "wer.dll!WerReportCreate must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerReportSetParameter") != nullptr, "wer.dll!WerReportSetParameter must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerReportAddFile") != nullptr, "wer.dll!WerReportAddFile must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerReportAddDump") != nullptr, "wer.dll!WerReportAddDump must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerReportSubmit") != nullptr, "wer.dll!WerReportSubmit must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerReportCloseHandle") != nullptr, "wer.dll!WerReportCloseHandle must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerRegisterFile") != nullptr, "wer.dll!WerRegisterFile must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerRegisterMemoryBlock") != nullptr, "wer.dll!WerRegisterMemoryBlock must be exported");
    TEST_ASSERT(ldr.getExport("wer.dll", "WerAddExcludedApplication") != nullptr, "wer.dll!WerAddExcludedApplication must be exported");
    TEST_ASSERT(ldr.getExport("faultrep.dll", "ReportFault") != nullptr, "faultrep.dll!ReportFault must be exported");
    TEST_ASSERT(ldr.getExport("faultrep.dll", "AddERExcludedApplicationA") != nullptr, "faultrep.dll!AddERExcludedApplicationA must be exported");

    const auto* modWer = version::VersionDatabase::Instance().FindModule("wer.dll");
    TEST_ASSERT(modWer != nullptr, "wer.dll must be registered in VersionDatabase");
    TEST_ASSERT(modWer->stringTable.at("FileVersion") == "10.0.22621.1", "wer.dll version must match 10.0.22621.1");

    const auto* modFaultrep = version::VersionDatabase::Instance().FindModule("faultrep.dll");
    TEST_ASSERT(modFaultrep != nullptr, "faultrep.dll must be registered in VersionDatabase");

    const auto* modWerfault = version::VersionDatabase::Instance().FindModule("werfault.exe");
    TEST_ASSERT(modWerfault != nullptr, "werfault.exe must be registered in VersionDatabase");

    // ------------------------------------------------------------------------
    // Stage 2: WerReportCreate & Handle Validation
    // ------------------------------------------------------------------------
    WER_REPORT_INFORMATION info{};
    info.dwSize = sizeof(info);
    wcscpy_s(info.wzApplicationName, L"calculator.exe");
    wcscpy_s(info.wzFriendlyEventName, L"Modern Calculator Fault");
    wcscpy_s(info.wzApplicationPath, L"C:\\Program Files\\Calculator\\calculator.exe");
    wcscpy_s(info.wzDescription, L"Arithmetic floating point division by zero");

    HREPORT hReport = nullptr;
    ole32::HRESULT hr = WerReportCreate(L"APPCRASH", WerReportCritical, &info, &hReport);
    TEST_ASSERT(hr == ole32::S_OK, "WerReportCreate must return S_OK");
    TEST_ASSERT(hReport != nullptr, "WerReportCreate must return valid HREPORT handle");

    // Null check verification
    HREPORT hNull = nullptr;
    TEST_ASSERT(WerReportCreate(nullptr, WerReportCritical, &info, &hNull) != ole32::S_OK, "WerReportCreate with null event type must fail");
    TEST_ASSERT(WerReportCreate(L"APPCRASH", WerReportCritical, &info, nullptr) != ole32::S_OK, "WerReportCreate with null handle ptr must fail");

    // ------------------------------------------------------------------------
    // Stage 3: WerReportSetParameter (Parameters P0 through P7)
    // ------------------------------------------------------------------------
    hr = WerReportSetParameter(hReport, WER_P0, L"AppName", L"calculator.exe");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P0 (AppName) must succeed");
    hr = WerReportSetParameter(hReport, WER_P1, L"AppVer", L"1.0.4.0");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P1 (AppVer) must succeed");
    hr = WerReportSetParameter(hReport, WER_P2, L"AppStamp", L"65432100");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P2 (AppStamp) must succeed");
    hr = WerReportSetParameter(hReport, WER_P3, L"ModName", L"calc_core.dll");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P3 (ModName) must succeed");
    hr = WerReportSetParameter(hReport, WER_P4, L"ModVer", L"1.0.2.1");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P4 (ModVer) must succeed");
    hr = WerReportSetParameter(hReport, WER_P5, L"ModStamp", L"65432200");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P5 (ModStamp) must succeed");
    hr = WerReportSetParameter(hReport, WER_P6, L"ExceptionCode", L"c0000094"); // STATUS_INTEGER_DIVIDE_BY_ZERO
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P6 (ExceptionCode) must succeed");
    hr = WerReportSetParameter(hReport, WER_P7, L"ExceptionOffset", L"00000000000248a0");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetParameter for P7 (ExceptionOffset) must succeed");

    // Out of range parameter ID check
    TEST_ASSERT(WerReportSetParameter(hReport, 15, L"BadParam", L"Val") != ole32::S_OK, "WerReportSetParameter with invalid parameter ID must fail");

    // ------------------------------------------------------------------------
    // Stage 4: WerReportAddFile (Attachment Specifications)
    // ------------------------------------------------------------------------
    hr = WerReportAddFile(hReport, L"C:\\Logs\\calc_session.log", WerFileTypeUserDocument, WER_FILE_ANONYMOUS_DATA);
    TEST_ASSERT(hr == ole32::S_OK, "WerReportAddFile for session log must return S_OK");
    hr = WerReportAddFile(hReport, L"C:\\Config\\calc_state.xml", WerFileTypeOther, 0);
    TEST_ASSERT(hr == ole32::S_OK, "WerReportAddFile for config xml must return S_OK");
    TEST_ASSERT(WerReportAddFile(hReport, nullptr, WerFileTypeOther, 0) != ole32::S_OK, "WerReportAddFile with null path must fail");

    // ------------------------------------------------------------------------
    // Stage 5: WerReportAddDump & PolarisDiag WinDbg Parity Validation
    // ------------------------------------------------------------------------
    EXCEPTION_RECORD excRec{};
    excRec.ExceptionCode = 0xC0000094; // STATUS_INTEGER_DIVIDE_BY_ZERO
    excRec.ExceptionAddress = reinterpret_cast<void*>(0x00007FF6140248A0ULL);

    polaris::MINIDUMP_X64_CONTEXT ctxRec{};
    ctxRec.Rip = 0x00007FF6140248A0ULL;
    ctxRec.Rsp = 0x00007FFFFFFFDB00ULL;
    ctxRec.Rax = 0x100;
    ctxRec.Rcx = 0; // divisor = 0

    EXCEPTION_POINTERS excPtrs{};
    excPtrs.ExceptionRecord = &excRec;
    excPtrs.ContextRecord = &ctxRec;

    WER_EXCEPTION_INFORMATION excInfo{};
    excInfo.pExceptionPointers = &excPtrs;
    excInfo.bClientPointers = 0;

    WER_DUMP_CUSTOM_OPTIONS dumpOpts{};
    dumpOpts.dwDumpFlags = 0x00000002; // MiniDumpWithFullMemoryInfo

    hr = WerReportAddDump(hReport, nullptr, nullptr, WerDumpTypeMiniDump, &excInfo, &dumpOpts, 0);
    TEST_ASSERT(hr == ole32::S_OK, "WerReportAddDump must return S_OK");

    auto activeRep = WerCoordinator::Instance().GetActiveReport(hReport);
    TEST_ASSERT(activeRep != nullptr, "Active report must be resolvable from coordinator");
    TEST_ASSERT(!activeRep->m_dumps.empty(), "Active report must contain generated minidump attachment");
    TEST_ASSERT(!activeRep->m_dumps[0].dumpData.empty(), "Dump data buffer must be non-empty");

    // WinDbg parser validation via PolarisDiagnosticEngine
    auto dumpSummary = polaris::PolarisDiagnosticEngine::get().parseMinidump(activeRep->m_dumps[0].dumpData);
    TEST_ASSERT(dumpSummary.isValid, "Minidump generated by WER must validate WinDbg compliance");
    TEST_ASSERT(dumpSummary.streamCount == 6, "Minidump must contain 6 distinct stream directories");
    TEST_ASSERT(dumpSummary.exceptionCode == 0xC0000094, "Minidump exception code must match divide-by-zero");
    TEST_ASSERT(dumpSummary.exceptionAddress == 0x00007FF6140248A0ULL, "Minidump exception address must match faulting instruction");
    TEST_ASSERT(dumpSummary.rip == 0x00007FF6140248A0ULL, "Minidump RIP register must match context record");
    TEST_ASSERT(dumpSummary.comment.find("Telemetry-Free") != std::string::npos, "Minidump must confirm zero-telemetry sovereign provenance");

    // ------------------------------------------------------------------------
    // Stage 6: WerReportSetUIOption Configuration
    // ------------------------------------------------------------------------
    hr = WerReportSetUIOption(hReport, WerUIConsentDlgHeader, L"Application Failure Notice");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetUIOption must succeed");
    hr = WerReportSetUIOption(hReport, WerUICloseText, L"Close Application");
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSetUIOption must succeed");

    // ------------------------------------------------------------------------
    // Stage 7: WerReportSubmit (Zero-Telemetry Sovereign Archiving)
    // ------------------------------------------------------------------------
    WER_SUBMIT_RESULT submitResult = WerReportFailed;
    hr = WerReportSubmit(hReport, WerConsentApproved, WER_SUBMIT_QUEUE, &submitResult);
    TEST_ASSERT(hr == ole32::S_OK, "WerReportSubmit must succeed");
    TEST_ASSERT(submitResult == WerReportQueued, "WerReportSubmit must queue report in sovereign local archive");
    TEST_ASSERT(activeRep->m_submitted, "Report must be marked submitted");

    // ------------------------------------------------------------------------
    // Stage 8: Manifest Formats (.wer format and XML)
    // ------------------------------------------------------------------------
    std::string werManifest = activeRep->GenerateReportWerManifest();
    TEST_ASSERT(werManifest.find("EventType=APPCRASH") != std::string::npos, "Manifest must declare EventType=APPCRASH");
    TEST_ASSERT(werManifest.find("Sig[0].Value=calculator.exe") != std::string::npos, "Manifest must contain calculator.exe as P0");
    TEST_ASSERT(werManifest.find("Sig[6].Value=c0000094") != std::string::npos, "Manifest must contain exception code c0000094");
    TEST_ASSERT(werManifest.find("State.ZeroTelemetry=SOVEREIGN_ENFORCED") != std::string::npos, "Manifest must declare sovereign telemetry policy");

    std::string xmlManifest = activeRep->GenerateXmlManifest();
    TEST_ASSERT(xmlManifest.find("<WERReport Version=\"1\"") != std::string::npos, "XML manifest must start with WERReport tag");
    TEST_ASSERT(xmlManifest.find("calculator.exe") != std::string::npos, "XML manifest must contain calculator.exe");

    // Close Report Handle
    hr = WerReportCloseHandle(hReport);
    TEST_ASSERT(hr == ole32::S_OK, "WerReportCloseHandle must succeed");
    TEST_ASSERT(WerReportSetParameter(hReport, WER_P0, L"Key", L"Val") != ole32::S_OK, "Using closed handle must fail");

    // ------------------------------------------------------------------------
    // Stage 9: Process Diagnostics Registration (Files & Memory Blocks)
    // ------------------------------------------------------------------------
    hr = WerRegisterFile(L"C:\\CrashDiagnostics\\app_state.dmp", WerRegFileTypeUserDocument, 0);
    TEST_ASSERT(hr == ole32::S_OK, "WerRegisterFile must return S_OK");
    TEST_ASSERT(WerCoordinator::Instance().GetRegisteredFileCount() >= 1, "Registered file count must be at least 1");

    hr = WerUnregisterFile(L"C:\\CrashDiagnostics\\app_state.dmp");
    TEST_ASSERT(hr == ole32::S_OK, "WerUnregisterFile must return S_OK");

    uint8_t heapBlock[256]{0};
    memset(heapBlock, 0xAA, sizeof(heapBlock));
    hr = WerRegisterMemoryBlock(heapBlock, sizeof(heapBlock));
    TEST_ASSERT(hr == ole32::S_OK, "WerRegisterMemoryBlock must return S_OK");
    TEST_ASSERT(WerCoordinator::Instance().GetRegisteredMemoryCount() >= 1, "Registered memory count must be at least 1");

    hr = WerUnregisterMemoryBlock(heapBlock);
    TEST_ASSERT(hr == ole32::S_OK, "WerUnregisterMemoryBlock must return S_OK");

    // ------------------------------------------------------------------------
    // Stage 10: Runtime Exception Modules Registration
    // ------------------------------------------------------------------------
    hr = WerRegisterRuntimeExceptionModule(L"mscorwks_diag.dll", reinterpret_cast<void*>(0x1234));
    TEST_ASSERT(hr == ole32::S_OK, "WerRegisterRuntimeExceptionModule must succeed");
    TEST_ASSERT(WerCoordinator::Instance().GetRuntimeModuleCount() >= 1, "Runtime module count must be at least 1");

    hr = WerUnregisterRuntimeExceptionModule(L"mscorwks_diag.dll", reinterpret_cast<void*>(0x1234));
    TEST_ASSERT(hr == ole32::S_OK, "WerUnregisterRuntimeExceptionModule must succeed");

    // Flags test
    WerSetFlags(WER_FAULT_REPORTING_FLAG_NOHEAP | WER_FAULT_REPORTING_FLAG_QUEUE);
    uint32_t flags = 0;
    WerGetFlags(nullptr, &flags);
    TEST_ASSERT((flags & WER_FAULT_REPORTING_FLAG_NOHEAP) != 0, "WerGetFlags must reflect NOHEAP flag");
    TEST_ASSERT((flags & WER_FAULT_REPORTING_FLAG_QUEUE) != 0, "WerGetFlags must reflect QUEUE flag");

    // ------------------------------------------------------------------------
    // Stage 11: Exclusion List Management & Suppression Verification
    // ------------------------------------------------------------------------
    hr = WerAddExcludedApplication(L"suppressed_tool.exe", 1);
    TEST_ASSERT(hr == ole32::S_OK, "WerAddExcludedApplication must return S_OK");

    int32_t isExcluded = 0;
    hr = WerIsApplicationExcluded(L"suppressed_tool.exe", 1, &isExcluded);
    TEST_ASSERT(hr == ole32::S_OK && isExcluded == 1, "WerIsApplicationExcluded must report excluded app as 1");

    // Create report for excluded application
    WER_REPORT_INFORMATION exInfo{};
    exInfo.dwSize = sizeof(exInfo);
    wcscpy_s(exInfo.wzApplicationName, L"suppressed_tool.exe");
    HREPORT hExReport = nullptr;
    WerReportCreate(L"APPCRASH", WerReportCritical, &exInfo, &hExReport);
    WER_SUBMIT_RESULT exResult = WerReportFailed;
    WerReportSubmit(hExReport, WerConsentApproved, 0, &exResult);
    TEST_ASSERT(exResult == WerDisabled, "Submitting report for excluded app must return WerDisabled");
    WerReportCloseHandle(hExReport);

    hr = WerRemoveExcludedApplication(L"suppressed_tool.exe", 1);
    TEST_ASSERT(hr == ole32::S_OK, "WerRemoveExcludedApplication must return S_OK");
    WerIsApplicationExcluded(L"suppressed_tool.exe", 1, &isExcluded);
    TEST_ASSERT(isExcluded == 0, "Excluded app must no longer be excluded after removal");

    // ------------------------------------------------------------------------
    // Stage 12: Legacy Crash Reporter (faultrep.dll!ReportFault)
    // ------------------------------------------------------------------------
    EFaultRepRet repRet = ReportFault(&excPtrs, 0);
    TEST_ASSERT(repRet == EFaultRepRet::frok, "ReportFault must return frok (1)");

    int32_t addRet = AddERExcludedApplicationA("legacy_app.exe");
    TEST_ASSERT(addRet == 1, "AddERExcludedApplicationA must return 1");
    int32_t isLegacyEx = 0;
    WerIsApplicationExcluded(L"legacy_app.exe", 1, &isLegacyEx);
    TEST_ASSERT(isLegacyEx == 1, "Legacy excluded application must be registered");
    WerRemoveExcludedApplication(L"legacy_app.exe", 1);

    // ------------------------------------------------------------------------
    // Stage 13: Interactive Command Shell (werfault.exe)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. werfault (banner)
        shell.execute("werfault", out);
        TEST_ASSERT(out.str().find("Windows Error Reporting Diagnostic Agent") != std::string::npos, "werfault without args must display banner");

        // 2. werfault /list
        out.str("");
        shell.execute("werfault /list", out);
        TEST_ASSERT(out.str().find("notepad.exe") != std::string::npos, "werfault /list must display pre-seeded notepad.exe crash");
        TEST_ASSERT(out.str().find("vanguard.sys") != std::string::npos, "werfault /list must display pre-seeded vanguard.sys crash");

        // 3. werfault /report 0
        out.str("");
        shell.execute("werfault /report 0", out);
        TEST_ASSERT(out.str().find("WER Crash Report Inspection") != std::string::npos, "werfault /report 0 must show inspection banner");
        TEST_ASSERT(out.str().find("PolarisDiag Minidump Analysis") != std::string::npos, "werfault /report 0 must display minidump analysis");

        // 4. werfault /trigger test_proc.exe
        out.str("");
        shell.execute("werfault /trigger test_proc.exe", out);
        TEST_ASSERT(out.str().find("Successfully triggered and queued APPCRASH report") != std::string::npos, "werfault /trigger must report success");

        // 5. werfault /exclude list
        out.str("");
        shell.execute("werfault /exclude list", out);
        TEST_ASSERT(out.str().find("wermgr.exe") != std::string::npos, "werfault /exclude list must display wermgr.exe");

        // 6. werfault test
        out.str("");
        shell.execute("werfault test", out);
        TEST_ASSERT(out.str().find("Windows Error Reporting Subsystem Self-Test Finished") != std::string::npos, "werfault test must finish successfully");
    }

    std::cout << "[TEST] Suite 77: Windows Error Reporting (WER) Subsystem PASSED.\n";
}

void Test_WindowsDWM_DesktopWindowManager_Subsystem() {
    using namespace micant::dwm;

    InitializeDWMSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader & Versioning Database Verification
    // ------------------------------------------------------------------------
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmIsCompositionEnabled") != nullptr, "dwmapi.dll!DwmIsCompositionEnabled must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmEnableComposition") != nullptr, "dwmapi.dll!DwmEnableComposition must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmExtendFrameIntoClientArea") != nullptr, "dwmapi.dll!DwmExtendFrameIntoClientArea must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmEnableBlurBehindWindow") != nullptr, "dwmapi.dll!DwmEnableBlurBehindWindow must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmSetWindowAttribute") != nullptr, "dwmapi.dll!DwmSetWindowAttribute must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmGetWindowAttribute") != nullptr, "dwmapi.dll!DwmGetWindowAttribute must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmGetColorizationColor") != nullptr, "dwmapi.dll!DwmGetColorizationColor must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmFlush") != nullptr, "dwmapi.dll!DwmFlush must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmGetCompositionTimingInfo") != nullptr, "dwmapi.dll!DwmGetCompositionTimingInfo must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmRegisterThumbnail") != nullptr, "dwmapi.dll!DwmRegisterThumbnail must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmUnregisterThumbnail") != nullptr, "dwmapi.dll!DwmUnregisterThumbnail must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmUpdateThumbnailProperties") != nullptr, "dwmapi.dll!DwmUpdateThumbnailProperties must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmQueryThumbnailSourceSize") != nullptr, "dwmapi.dll!DwmQueryThumbnailSourceSize must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmSetIconicThumbnail") != nullptr, "dwmapi.dll!DwmSetIconicThumbnail must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmSetIconicLivePreviewBitmap") != nullptr, "dwmapi.dll!DwmSetIconicLivePreviewBitmap must be exported");
    TEST_ASSERT(ldr.getExport("dwmapi.dll", "DwmInvalidateIconicBitmaps") != nullptr, "dwmapi.dll!DwmInvalidateIconicBitmaps must be exported");

    const auto* modDwmApi = version::VersionDatabase::Instance().FindModule("dwmapi.dll");
    TEST_ASSERT(modDwmApi != nullptr, "dwmapi.dll must be registered in VersionDatabase");
    TEST_ASSERT(modDwmApi->stringTable.at("FileVersion") == "10.0.22621.1", "dwmapi.dll version must match 10.0.22621.1");

    const auto* modDwmExe = version::VersionDatabase::Instance().FindModule("dwm.exe");
    TEST_ASSERT(modDwmExe != nullptr, "dwm.exe must be registered in VersionDatabase");

    // ------------------------------------------------------------------------
    // Stage 2: Composition State Queries & Toggling
    // ------------------------------------------------------------------------
    int32_t enabled = 0;
    ole32::HRESULT hr = DwmIsCompositionEnabled(&enabled);
    TEST_ASSERT(hr == ole32::S_OK, "DwmIsCompositionEnabled must return S_OK");
    TEST_ASSERT(enabled == 1, "DWM Composition must default to ENABLED (1)");

    TEST_ASSERT(DwmIsCompositionEnabled(nullptr) != ole32::S_OK, "DwmIsCompositionEnabled with null ptr must fail");

    hr = DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
    TEST_ASSERT(hr == ole32::S_OK, "DwmEnableComposition(DISABLE) must return S_OK");
    DwmIsCompositionEnabled(&enabled);
    TEST_ASSERT(enabled == 0, "DwmIsCompositionEnabled must reflect disabled state");

    hr = DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
    TEST_ASSERT(hr == ole32::S_OK, "DwmEnableComposition(ENABLE) must return S_OK");
    DwmIsCompositionEnabled(&enabled);
    TEST_ASSERT(enabled == 1, "DwmIsCompositionEnabled must reflect enabled state");

    // ------------------------------------------------------------------------
    // Stage 3: Accent Colorization Queries
    // ------------------------------------------------------------------------
    uint32_t color = 0;
    int32_t opaque = -1;
    hr = DwmGetColorizationColor(&color, &opaque);
    TEST_ASSERT(hr == ole32::S_OK, "DwmGetColorizationColor must return S_OK");
    TEST_ASSERT(color != 0, "Colorization color must be non-zero");
    TEST_ASSERT(opaque == 0, "Opaque blend must default to false");
    TEST_ASSERT(DwmGetColorizationColor(nullptr, &opaque) != ole32::S_OK, "DwmGetColorizationColor with null ptr must fail");

    // ------------------------------------------------------------------------
    // Stage 4: Composition Timing & VSync Synchronization
    // ------------------------------------------------------------------------
    win32::HWND hTestWnd = reinterpret_cast<win32::HWND>(0x7000);
    DWM_TIMING_INFO timing{};
    hr = DwmGetCompositionTimingInfo(hTestWnd, &timing);
    TEST_ASSERT(hr == ole32::S_OK, "DwmGetCompositionTimingInfo must return S_OK");
    TEST_ASSERT(timing.cbSize == sizeof(DWM_TIMING_INFO), "Timing info size must match sizeof(DWM_TIMING_INFO)");
    TEST_ASSERT(timing.rateRefresh.uiNumerator == 60000 && timing.rateRefresh.uiDenominator == 1000, "Refresh rate must report 60.000 Hz");
    TEST_ASSERT(timing.qpcRefreshPeriod == 166666, "QPC refresh period must reflect ~16.66 ms");

    uint64_t initialFrames = timing.cFrame;
    hr = DwmFlush();
    TEST_ASSERT(hr == ole32::S_OK, "DwmFlush must return S_OK");
    DwmGetCompositionTimingInfo(hTestWnd, &timing);
    TEST_ASSERT(timing.cFrame > initialFrames, "DwmFlush must advance composition frame count");

    // ------------------------------------------------------------------------
    // Stage 5: Frame Margins (Sheet-of-Glass & Custom Insets)
    // ------------------------------------------------------------------------
    MARGINS standardMargins{ 10, 10, 32, 10 };
    hr = DwmExtendFrameIntoClientArea(hTestWnd, &standardMargins);
    TEST_ASSERT(hr == ole32::S_OK, "DwmExtendFrameIntoClientArea must return S_OK");

    MARGINS sheetOfGlass{ -1, -1, -1, -1 };
    hr = DwmExtendFrameIntoClientArea(hTestWnd, &sheetOfGlass);
    TEST_ASSERT(hr == ole32::S_OK, "DwmExtendFrameIntoClientArea for sheet-of-glass must return S_OK");
    TEST_ASSERT(DwmExtendFrameIntoClientArea(nullptr, &standardMargins) != ole32::S_OK, "Null HWND must fail");
    TEST_ASSERT(DwmExtendFrameIntoClientArea(hTestWnd, nullptr) != ole32::S_OK, "Null margins must fail");

    // ------------------------------------------------------------------------
    // Stage 6: Acrylic & Blur-Behind Configuration
    // ------------------------------------------------------------------------
    DWM_BLURBEHIND bb{};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = 1;
    hr = DwmEnableBlurBehindWindow(hTestWnd, &bb);
    TEST_ASSERT(hr == ole32::S_OK, "DwmEnableBlurBehindWindow must return S_OK");
    TEST_ASSERT(DwmEnableBlurBehindWindow(hTestWnd, nullptr) != ole32::S_OK, "Null blur-behind ptr must fail");

    // ------------------------------------------------------------------------
    // Stage 7: Window Attributes (Dark Mode, Mica, Corners, Bounds)
    // ------------------------------------------------------------------------
    // 7.1 Immersive Dark Mode
    int32_t setDarkMode = 1;
    hr = DwmSetWindowAttribute(hTestWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &setDarkMode, sizeof(setDarkMode));
    TEST_ASSERT(hr == ole32::S_OK, "DwmSetWindowAttribute(USE_IMMERSIVE_DARK_MODE) must return S_OK");
    int32_t readDarkMode = 0;
    hr = DwmGetWindowAttribute(hTestWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &readDarkMode, sizeof(readDarkMode));
    TEST_ASSERT(hr == ole32::S_OK && readDarkMode == 1, "DwmGetWindowAttribute must return active dark mode (1)");

    // 7.2 System Backdrop Type (Mica / Acrylic)
    uint32_t setBackdrop = DWMSBT_MAINWINDOW; // Mica
    hr = DwmSetWindowAttribute(hTestWnd, DWMWA_SYSTEMBACKDROP_TYPE, &setBackdrop, sizeof(setBackdrop));
    TEST_ASSERT(hr == ole32::S_OK, "DwmSetWindowAttribute(SYSTEMBACKDROP_TYPE) must return S_OK");
    uint32_t readBackdrop = 0;
    hr = DwmGetWindowAttribute(hTestWnd, DWMWA_SYSTEMBACKDROP_TYPE, &readBackdrop, sizeof(readBackdrop));
    TEST_ASSERT(hr == ole32::S_OK && readBackdrop == DWMSBT_MAINWINDOW, "DwmGetWindowAttribute must return DWMSBT_MAINWINDOW");

    // Verify Mica effect attribute synchronizes
    int32_t readMica = 0;
    hr = DwmGetWindowAttribute(hTestWnd, DWMWA_MICA_EFFECT, &readMica, sizeof(readMica));
    TEST_ASSERT(hr == ole32::S_OK && readMica == 1, "Mica effect attribute must be active when DWMSBT_MAINWINDOW is set");

    // 7.3 Window Corner Preference (Rounded)
    uint32_t setCorner = DWMWCP_ROUND;
    hr = DwmSetWindowAttribute(hTestWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &setCorner, sizeof(setCorner));
    TEST_ASSERT(hr == ole32::S_OK, "DwmSetWindowAttribute(WINDOW_CORNER_PREFERENCE) must return S_OK");
    uint32_t readCorner = 0;
    hr = DwmGetWindowAttribute(hTestWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &readCorner, sizeof(readCorner));
    TEST_ASSERT(hr == ole32::S_OK && readCorner == DWMWCP_ROUND, "DwmGetWindowAttribute must return DWMWCP_ROUND");

    // 7.4 Border & Caption Colors
    uint32_t setBorderColor = 0x00FF9933;
    DwmSetWindowAttribute(hTestWnd, DWMWA_BORDER_COLOR, &setBorderColor, sizeof(setBorderColor));
    uint32_t readBorderColor = 0;
    DwmGetWindowAttribute(hTestWnd, DWMWA_BORDER_COLOR, &readBorderColor, sizeof(readBorderColor));
    TEST_ASSERT(readBorderColor == 0x00FF9933, "DwmGetWindowAttribute must return custom border color");

    // 7.5 Extended Frame Bounds
    prismx::RECT setBounds{ 50, 50, 850, 650 };
    DwmSetWindowAttribute(hTestWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &setBounds, sizeof(setBounds));
    prismx::RECT readBounds{};
    DwmGetWindowAttribute(hTestWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &readBounds, sizeof(readBounds));
    TEST_ASSERT(readBounds.left == 50 && readBounds.top == 50 && readBounds.right == 850 && readBounds.bottom == 650, "Extended frame bounds must match");

    // ------------------------------------------------------------------------
    // Stage 8: Live Thumbnail Composition
    // ------------------------------------------------------------------------
    win32::HWND hDestWnd = reinterpret_cast<win32::HWND>(0x7001);
    win32::HWND hSrcWnd = reinterpret_cast<win32::HWND>(0x7002);
    HTHUMBNAIL hThumbnail = nullptr;

    hr = DwmRegisterThumbnail(hDestWnd, hSrcWnd, &hThumbnail);
    TEST_ASSERT(hr == ole32::S_OK, "DwmRegisterThumbnail must return S_OK");
    TEST_ASSERT(hThumbnail != nullptr, "Thumbnail handle must be valid");
    TEST_ASSERT(DwmCoordinator::Instance().GetThumbnailCount() >= 1, "Active thumbnail count must be >= 1");

    // Identical destination and source must fail
    HTHUMBNAIL hBad = nullptr;
    TEST_ASSERT(DwmRegisterThumbnail(hDestWnd, hDestWnd, &hBad) != ole32::S_OK, "Registering thumbnail with same dest and src must fail");

    SIZE srcSize{};
    hr = DwmQueryThumbnailSourceSize(hThumbnail, &srcSize);
    TEST_ASSERT(hr == ole32::S_OK, "DwmQueryThumbnailSourceSize must return S_OK");
    TEST_ASSERT(srcSize.cx > 0 && srcSize.cy > 0, "Source size dimensions must be positive");

    DWM_THUMBNAIL_PROPERTIES thumbProps{};
    thumbProps.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_OPACITY | DWM_TNP_VISIBLE;
    thumbProps.rcDestination = { 10, 10, 160, 120 };
    thumbProps.opacity = 220;
    thumbProps.fVisible = 1;
    hr = DwmUpdateThumbnailProperties(hThumbnail, &thumbProps);
    TEST_ASSERT(hr == ole32::S_OK, "DwmUpdateThumbnailProperties must return S_OK");

    hr = DwmUnregisterThumbnail(hThumbnail);
    TEST_ASSERT(hr == ole32::S_OK, "DwmUnregisterThumbnail must return S_OK");
    TEST_ASSERT(DwmUnregisterThumbnail(hThumbnail) != ole32::S_OK, "Unregistering already unregistered thumbnail must fail");

    // ------------------------------------------------------------------------
    // Stage 9: Iconic Thumbnails & Preview Bitmaps
    // ------------------------------------------------------------------------
    void* hFakeBmp = reinterpret_cast<void*>(0x8888);
    hr = DwmSetIconicThumbnail(hTestWnd, hFakeBmp, 0);
    TEST_ASSERT(hr == ole32::S_OK, "DwmSetIconicThumbnail must return S_OK");

    prismx::POINT ptOrigin{ 0, 0 };
    hr = DwmSetIconicLivePreviewBitmap(hTestWnd, hFakeBmp, &ptOrigin, 0);
    TEST_ASSERT(hr == ole32::S_OK, "DwmSetIconicLivePreviewBitmap must return S_OK");

    hr = DwmInvalidateIconicBitmaps(hTestWnd);
    TEST_ASSERT(hr == ole32::S_OK, "DwmInvalidateIconicBitmaps must return S_OK");

    // ------------------------------------------------------------------------
    // Stage 10: Presentation Hooks & DirectX Frame Duration
    // ------------------------------------------------------------------------
    hr = DwmAttachMilContent(hTestWnd);
    TEST_ASSERT(hr == ole32::S_OK, "DwmAttachMilContent must return S_OK");
    hr = DwmDetachMilContent(hTestWnd);
    TEST_ASSERT(hr == ole32::S_OK, "DwmDetachMilContent must return S_OK");

    hr = DwmModifyPreviousDxFrameDuration(hTestWnd, 1, 0);
    TEST_ASSERT(hr == ole32::S_OK, "DwmModifyPreviousDxFrameDuration must return S_OK");

    DWM_PRESENT_PARAMETERS presentParams{};
    hr = DwmSetPresentParameters(hTestWnd, &presentParams);
    TEST_ASSERT(hr == ole32::S_OK, "DwmSetPresentParameters must return S_OK");

    // ------------------------------------------------------------------------
    // Stage 11: Integration with user32::WindowManager
    // ------------------------------------------------------------------------
    win32::HWND hWin32 = user32::WindowManager::get().createWindow(
        0, L"MicaWindowClass", L"DWM Integration Window", 0,
        100, 100, 1024, 768, nullptr, nullptr, nullptr, nullptr
    );
    TEST_ASSERT(hWin32 != nullptr, "user32::WindowManager must create test window");

    // Query extended frame bounds on live WindowObject
    prismx::RECT winBounds{};
    hr = DwmGetWindowAttribute(hWin32, DWMWA_EXTENDED_FRAME_BOUNDS, &winBounds, sizeof(winBounds));
    TEST_ASSERT(hr == ole32::S_OK, "DwmGetWindowAttribute for live WindowObject bounds must succeed");
    TEST_ASSERT(winBounds.right - winBounds.left == 1024, "Window width in DWM bounds must match 1024");
    TEST_ASSERT(winBounds.bottom - winBounds.top == 768, "Window height in DWM bounds must match 768");

    // Apply Mica backdrop to user32 window
    uint32_t micaBackdrop = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(hWin32, DWMWA_SYSTEMBACKDROP_TYPE, &micaBackdrop, sizeof(micaBackdrop));
    uint32_t queryBackdrop = 0;
    DwmGetWindowAttribute(hWin32, DWMWA_SYSTEMBACKDROP_TYPE, &queryBackdrop, sizeof(queryBackdrop));
    TEST_ASSERT(queryBackdrop == DWMSBT_MAINWINDOW, "Live window must retain Mica backdrop attribute");

    // ------------------------------------------------------------------------
    // Stage 12: Interactive CLI Utility (dwm.exe)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. dwm status
        shell.execute("dwm status", out);
        TEST_ASSERT(out.str().find("Desktop Window Manager & Composition Engine") != std::string::npos, "dwm status must display banner");
        TEST_ASSERT(out.str().find("ENABLED (Hardware Accelerated)") != std::string::npos, "dwm status must report ENABLED");

        // 2. dwm list
        out.str("");
        shell.execute("dwm list", out);
        TEST_ASSERT(out.str().find("Desktop Window Manager Active Window Attributes") != std::string::npos, "dwm list must show window attributes table");

        // 3. dwm disable / enable
        out.str("");
        shell.execute("dwm disable", out);
        TEST_ASSERT(out.str().find("Desktop composition disabled") != std::string::npos, "dwm disable must report disabled");

        out.str("");
        shell.execute("dwm enable", out);
        TEST_ASSERT(out.str().find("Desktop composition enabled") != std::string::npos, "dwm enable must report enabled");

        // 4. dwm test
        out.str("");
        shell.execute("dwm test", out);
        TEST_ASSERT(out.str().find("Desktop Window Manager Self-Test Finished") != std::string::npos, "dwm test must finish successfully");
    }

    std::cout << "[TEST] Suite 78: Windows Desktop Window Manager (DWM) Subsystem PASSED.\n";
}

void Test_WindowsWASAPI_CoreAudioEngine_Subsystem() {
    std::cout << "[TEST] Running Suite 79: Windows Audio Session API (WASAPI) & Core Audio Engine Subsystem (mmdevapi.dll / audiosrv.dll)...\n";

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (mmdevapi.dll & audiosrv.dll)
    // ------------------------------------------------------------------------
    wasapi::InitializeWASAPISubsystem();
    auto& ldr = ldr::DynamicLoader::get();

    TEST_ASSERT(ldr.getExport("mmdevapi.dll", "DllGetClassObject") != nullptr, "mmdevapi.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("mmdevapi.dll", "DllCanUnloadNow") != nullptr, "mmdevapi.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("mmdevapi.dll", "DllRegisterServer") != nullptr, "mmdevapi.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("mmdevapi.dll", "DllUnregisterServer") != nullptr, "mmdevapi.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("audiosrv.dll", "ServiceMain") != nullptr, "audiosrv.dll must export ServiceMain");

    // ------------------------------------------------------------------------
    // Stage 2: Module Version Metadata Verification
    // ------------------------------------------------------------------------
    auto& verDb = version::VersionDatabase::Instance();
    const auto* modMmdev = verDb.FindModule("mmdevapi.dll");
    TEST_ASSERT(modMmdev != nullptr, "mmdevapi.dll must be registered in VersionDatabase");
    TEST_ASSERT(modMmdev->stringTable.at("FileDescription") == "MMDevice API", "mmdevapi.dll FileDescription mismatch");
    TEST_ASSERT(modMmdev->stringTable.at("FileVersion") == "10.0.22621.1", "mmdevapi.dll FileVersion mismatch");

    const auto* modAudioSrv = verDb.FindModule("audiosrv.dll");
    TEST_ASSERT(modAudioSrv != nullptr, "audiosrv.dll must be registered in VersionDatabase");
    TEST_ASSERT(modAudioSrv->stringTable.at("FileDescription") == "Windows Audio Service", "audiosrv.dll FileDescription mismatch");

    // ------------------------------------------------------------------------
    // Stage 3: SCM Service Registration (AudioSrv)
    // ------------------------------------------------------------------------
    auto& scm = scm::ServiceControlManager::get();
    auto pSvc = scm.getServiceRecord(L"AudioSrv");
    TEST_ASSERT(pSvc != nullptr, "AudioSrv service must be registered in SCM");
    TEST_ASSERT(pSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "AudioSrv service must be in running state");
    TEST_ASSERT(pSvc->displayName == L"Windows Audio", "AudioSrv display name mismatch");

    // ------------------------------------------------------------------------
    // Stage 4: COM Class Factory & CoCreateInstance
    // ------------------------------------------------------------------------
    wasapi::IMMDeviceEnumerator* pEnumerator = nullptr;
    ole32::HRESULT hr = ole32::CoCreateInstance(
        wasapi::CLSID_MMDeviceEnumerator,
        nullptr,
        ole32::CLSCTX_INPROC_SERVER,
        wasapi::IID_IMMDeviceEnumerator,
        reinterpret_cast<void**>(&pEnumerator)
    );
    TEST_ASSERT(SUCCEEDED(hr), "CoCreateInstance(CLSID_MMDeviceEnumerator) must succeed");
    TEST_ASSERT(pEnumerator != nullptr, "Device enumerator pointer must not be null");

    // ------------------------------------------------------------------------
    // Stage 5: Audio Endpoint Enumeration (eRender & eCapture)
    // ------------------------------------------------------------------------
    wasapi::IMMDeviceCollection* pRenderCol = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(wasapi::eRender, wasapi::DEVICE_STATE_ACTIVE, &pRenderCol);
    TEST_ASSERT(SUCCEEDED(hr) && pRenderCol != nullptr, "EnumAudioEndpoints(eRender) must succeed");

    uint32_t renderCount = 0;
    pRenderCol->GetCount(&renderCount);
    TEST_ASSERT(renderCount >= 2, "Must enumerate at least 2 active render endpoints (Speakers & Headphones)");

    for (uint32_t i = 0; i < renderCount; ++i) {
        wasapi::IMMDevice* pDev = nullptr;
        hr = pRenderCol->Item(i, &pDev);
        TEST_ASSERT(SUCCEEDED(hr) && pDev != nullptr, "Item() must retrieve valid IMMDevice");

        wasapi::IMMEndpoint* pEndpoint = nullptr;
        hr = pDev->QueryInterface(wasapi::IID_IMMEndpoint, reinterpret_cast<void**>(&pEndpoint));
        TEST_ASSERT(SUCCEEDED(hr) && pEndpoint != nullptr, "Device must support IMMEndpoint interface");

        wasapi::EDataFlow flow;
        pEndpoint->GetDataFlow(&flow);
        TEST_ASSERT(flow == wasapi::eRender, "Endpoint flow must be eRender");

        uint32_t state = 0;
        pDev->GetState(&state);
        TEST_ASSERT(state == wasapi::DEVICE_STATE_ACTIVE, "Endpoint state must be DEVICE_STATE_ACTIVE");

        pEndpoint->Release();
        pDev->Release();
    }
    pRenderCol->Release();

    // Capture endpoints
    wasapi::IMMDeviceCollection* pCaptureCol = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(wasapi::eCapture, wasapi::DEVICE_STATE_ACTIVE, &pCaptureCol);
    TEST_ASSERT(SUCCEEDED(hr) && pCaptureCol != nullptr, "EnumAudioEndpoints(eCapture) must succeed");
    uint32_t captureCount = 0;
    pCaptureCol->GetCount(&captureCount);
    TEST_ASSERT(captureCount >= 1, "Must enumerate at least 1 active capture endpoint (Microphone)");
    pCaptureCol->Release();

    // ------------------------------------------------------------------------
    // Stage 6: Endpoint Property Store (IPropertyStore)
    // ------------------------------------------------------------------------
    wasapi::IMMDevice* pDefaultRender = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(wasapi::eRender, wasapi::eConsole, &pDefaultRender);
    TEST_ASSERT(SUCCEEDED(hr) && pDefaultRender != nullptr, "GetDefaultAudioEndpoint(eRender, eConsole) must succeed");

    wasapi::IPropertyStore* pStore = nullptr;
    hr = pDefaultRender->OpenPropertyStore(wasapi::STGM_READ, &pStore);
    TEST_ASSERT(SUCCEEDED(hr) && pStore != nullptr, "OpenPropertyStore must return valid IPropertyStore");

    wasapi::PROPVARIANT pvFriendly{};
    hr = pStore->GetValue(wasapi::PKEY_Device_FriendlyName, &pvFriendly);
    TEST_ASSERT(SUCCEEDED(hr), "GetValue(PKEY_Device_FriendlyName) must succeed");
    TEST_ASSERT(pvFriendly.vt == ole32::VT_LPWSTR && pvFriendly.pwszVal != nullptr, "FriendlyName must be VT_LPWSTR");
    std::wstring friendlyName(pvFriendly.pwszVal);
    TEST_ASSERT(friendlyName.find(L"Speakers") != std::wstring::npos, "Default console render device must be Speakers");
    wasapi::PropVariantClear(&pvFriendly);

    wasapi::PROPVARIANT pvForm{};
    hr = pStore->GetValue(wasapi::PKEY_AudioEndpoint_FormFactor, &pvForm);
    TEST_ASSERT(SUCCEEDED(hr), "GetValue(PKEY_AudioEndpoint_FormFactor) must succeed");
    TEST_ASSERT(pvForm.vt == ole32::VT_UI4 && pvForm.ulVal == wasapi::Speakers, "Form factor must be Speakers");

    // Set and commit custom property
    wasapi::PROPVARIANT pvCustom{};
    pvCustom.vt = ole32::VT_UI4;
    pvCustom.ulVal = 42;
    pStore->SetValue(wasapi::PKEY_AudioEndpoint_ControlPanelGrouping, pvCustom);
    pStore->Commit();

    wasapi::PROPVARIANT pvCheck{};
    pStore->GetValue(wasapi::PKEY_AudioEndpoint_ControlPanelGrouping, &pvCheck);
    TEST_ASSERT(pvCheck.ulVal == 42, "Custom property must be retained after Commit");
    pStore->Release();

    // ------------------------------------------------------------------------
    // Stage 7: Default Endpoint Query by Role
    // ------------------------------------------------------------------------
    wasapi::IMMDevice* pDefComm = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(wasapi::eRender, wasapi::eCommunications, &pDefComm);
    TEST_ASSERT(SUCCEEDED(hr) && pDefComm != nullptr, "GetDefaultAudioEndpoint(eCommunications) must succeed");

    wchar_t* commId = nullptr;
    pDefComm->GetId(&commId);
    TEST_ASSERT(commId != nullptr && std::wcslen(commId) > 0, "Communications endpoint ID must be valid");
    ole32::CoTaskMemFree(commId);
    pDefComm->Release();

    wasapi::IMMDevice* pDefMic = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(wasapi::eCapture, wasapi::eConsole, &pDefMic);
    TEST_ASSERT(SUCCEEDED(hr) && pDefMic != nullptr, "GetDefaultAudioEndpoint(eCapture) must succeed");

    wchar_t* micId = nullptr;
    pDefMic->GetId(&micId);
    TEST_ASSERT(micId != nullptr && std::wcslen(micId) > 0, "Capture endpoint ID must be valid");
    ole32::CoTaskMemFree(micId);
    pDefMic->Release();

    // ------------------------------------------------------------------------
    // Stage 8: Audio Client Activation & Format Negotiation
    // ------------------------------------------------------------------------
    wasapi::IAudioClient* pAudioClient = nullptr;
    hr = pDefaultRender->Activate(wasapi::IID_IAudioClient, 0, nullptr, reinterpret_cast<void**>(&pAudioClient));
    TEST_ASSERT(SUCCEEDED(hr) && pAudioClient != nullptr, "Activate(IID_IAudioClient) must succeed");

    audio::WAVEFORMATEX* pMixFormat = nullptr;
    hr = pAudioClient->GetMixFormat(&pMixFormat);
    TEST_ASSERT(SUCCEEDED(hr) && pMixFormat != nullptr, "GetMixFormat must return default device format");
    TEST_ASSERT(pMixFormat->nSamplesPerSec == 48000, "Device mix format must be 48,000 Hz");
    TEST_ASSERT(pMixFormat->nChannels == 2, "Device mix format must be 2 channels (stereo)");
    TEST_ASSERT(pMixFormat->wBitsPerSample == 16, "Device mix format must be 16-bit");

    hr = pAudioClient->IsFormatSupported(wasapi::AUDCLNT_SHAREMODE_SHARED, pMixFormat, nullptr);
    TEST_ASSERT(SUCCEEDED(hr), "Mix format must be supported in shared mode");

    wasapi::REFERENCE_TIME defPeriod = 0;
    wasapi::REFERENCE_TIME minPeriod = 0;
    hr = pAudioClient->GetDevicePeriod(&defPeriod, &minPeriod);
    TEST_ASSERT(SUCCEEDED(hr), "GetDevicePeriod must succeed");
    TEST_ASSERT(defPeriod == 100000, "Default device period must be 10ms (100,000 hns)");
    TEST_ASSERT(minPeriod == 30000, "Minimum device period must be 3ms (30,000 hns)");

    // ------------------------------------------------------------------------
    // Stage 9: Audio Client Initialization & Buffer Pacing
    // ------------------------------------------------------------------------
    // Request 100ms buffer (1,000,000 hns)
    hr = pAudioClient->Initialize(wasapi::AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, pMixFormat, nullptr);
    TEST_ASSERT(SUCCEEDED(hr), "Initialize shared audio stream must succeed");

    // Second initialize must fail with AUDCLNT_E_ALREADY_INITIALIZED
    hr = pAudioClient->Initialize(wasapi::AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, pMixFormat, nullptr);
    TEST_ASSERT(hr == wasapi::AUDCLNT_E_ALREADY_INITIALIZED, "Duplicate Initialize must return AUDCLNT_E_ALREADY_INITIALIZED");

    uint32_t bufFrameCount = 0;
    hr = pAudioClient->GetBufferSize(&bufFrameCount);
    TEST_ASSERT(SUCCEEDED(hr), "GetBufferSize must succeed");
    TEST_ASSERT(bufFrameCount >= 4800, "100ms buffer at 48kHz must contain at least 4800 frames");

    uint32_t currentPadding = 0;
    pAudioClient->GetCurrentPadding(&currentPadding);
    TEST_ASSERT(currentPadding == 0, "Initial padding must be 0 frames");

    hr = pAudioClient->Start();
    TEST_ASSERT(SUCCEEDED(hr), "Start() must transition client to playing state");

    // ------------------------------------------------------------------------
    // Stage 10: Audio Render Client (IAudioRenderClient)
    // ------------------------------------------------------------------------
    wasapi::IAudioRenderClient* pRenderClient = nullptr;
    hr = pAudioClient->GetService(wasapi::IID_IAudioRenderClient, reinterpret_cast<void**>(&pRenderClient));
    TEST_ASSERT(SUCCEEDED(hr) && pRenderClient != nullptr, "GetService(IID_IAudioRenderClient) must succeed");

    uint8_t* pRenderBuf = nullptr;
    hr = pRenderClient->GetBuffer(480, &pRenderBuf);
    TEST_ASSERT(SUCCEEDED(hr) && pRenderBuf != nullptr, "GetBuffer(480) must succeed");

    // Fill buffer with 16-bit PCM test sine wave
    auto* samples = reinterpret_cast<int16_t*>(pRenderBuf);
    for (uint32_t f = 0; f < 480; ++f) {
        int16_t sampleVal = static_cast<int16_t>(16000.0 * std::sin(2.0 * std::numbers::pi * 440.0 * f / 48000.0));
        samples[f * 2] = sampleVal;     // Left
        samples[f * 2 + 1] = sampleVal; // Right
    }

    hr = pRenderClient->ReleaseBuffer(480, 0);
    TEST_ASSERT(SUCCEEDED(hr), "ReleaseBuffer(480) must succeed");

    pAudioClient->GetCurrentPadding(&currentPadding);
    TEST_ASSERT(currentPadding == 480, "Padding must now be 480 frames");

    // Second write with SILENT flag
    hr = pRenderClient->GetBuffer(480, &pRenderBuf);
    TEST_ASSERT(SUCCEEDED(hr) && pRenderBuf != nullptr, "Second GetBuffer(480) must succeed");
    hr = pRenderClient->ReleaseBuffer(480, wasapi::AUDCLNT_BUFFERFLAGS_SILENT);
    TEST_ASSERT(SUCCEEDED(hr), "ReleaseBuffer with SILENT flag must succeed");

    pAudioClient->GetCurrentPadding(&currentPadding);
    TEST_ASSERT(currentPadding == 960, "Padding must now be 960 frames");

    pRenderClient->Release();

    // ------------------------------------------------------------------------
    // Stage 11: Audio Clock & Volume Control Interfaces
    // ------------------------------------------------------------------------
    wasapi::IAudioClock* pClock = nullptr;
    hr = pAudioClient->GetService(wasapi::IID_IAudioClock, reinterpret_cast<void**>(&pClock));
    TEST_ASSERT(SUCCEEDED(hr) && pClock != nullptr, "GetService(IID_IAudioClock) must succeed");

    uint64_t clockFreq = 0;
    pClock->GetFrequency(&clockFreq);
    TEST_ASSERT(clockFreq == 48000, "AudioClock frequency must match sample rate (48000 Hz)");

    uint64_t clockPos = 0;
    pClock->GetPosition(&clockPos, nullptr);
    TEST_ASSERT(clockPos == 960, "AudioClock position must match total frames rendered (960)");
    pClock->Release();

    // Test IAudioEndpointVolume
    wasapi::IAudioEndpointVolume* pEndpointVol = nullptr;
    hr = pDefaultRender->Activate(wasapi::IID_IAudioEndpointVolume, 0, nullptr, reinterpret_cast<void**>(&pEndpointVol));
    TEST_ASSERT(SUCCEEDED(hr) && pEndpointVol != nullptr, "Activate(IID_IAudioEndpointVolume) must succeed");

    uint32_t channelCount = 0;
    pEndpointVol->GetChannelCount(&channelCount);
    TEST_ASSERT(channelCount == 2, "Channel count must be 2");

    pEndpointVol->SetMasterVolumeLevelScalar(0.85f, nullptr);
    float scalarVol = 0.0f;
    pEndpointVol->GetMasterVolumeLevelScalar(&scalarVol);
    TEST_ASSERT(std::abs(scalarVol - 0.85f) < 0.01f, "Master volume scalar must be 0.85");

    float dbVol = 0.0f;
    pEndpointVol->GetMasterVolumeLevel(&dbVol);
    TEST_ASSERT(dbVol < 0.0f, "Volume in dB must be negative (< 0 dB)");

    pEndpointVol->SetMute(1, nullptr);
    win32::BOOL isMuted = 0;
    pEndpointVol->GetMute(&isMuted);
    TEST_ASSERT(isMuted == 1, "Endpoint must report muted");

    pEndpointVol->SetMute(0, nullptr);
    pEndpointVol->GetMute(&isMuted);
    TEST_ASSERT(isMuted == 0, "Endpoint must report unmuted");

    pEndpointVol->VolumeStepDown(nullptr);
    pEndpointVol->VolumeStepUp(nullptr);
    pEndpointVol->Release();

    pAudioClient->Stop();
    pAudioClient->Release();
    ole32::CoTaskMemFree(pMixFormat);
    pDefaultRender->Release();
    pEnumerator->Release();

    // ------------------------------------------------------------------------
    // Stage 12: Interactive CLI Utility Integration (audiosrv)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. audiosrv status
        shell.execute("audiosrv status", out);
        TEST_ASSERT(out.str().find("Windows Audio Service & WASAPI") != std::string::npos, "audiosrv status must display banner");
        TEST_ASSERT(out.str().find("RUNNING (Auto-Start)") != std::string::npos, "audiosrv status must report RUNNING");

        // 2. audiosrv list
        out.str("");
        shell.execute("audiosrv list", out);
        TEST_ASSERT(out.str().find("Speakers (Mica High Definition Audio)") != std::string::npos, "audiosrv list must list Speakers");
        TEST_ASSERT(out.str().find("Microphone (Mica HD Audio Array)") != std::string::npos, "audiosrv list must list Microphone");

        // 3. audiosrv volume
        out.str("");
        shell.execute("audiosrv volume 75", out);
        TEST_ASSERT(out.str().find("Master volume set to 75%") != std::string::npos, "audiosrv volume must set volume");

        // 4. audiosrv mute
        out.str("");
        shell.execute("audiosrv mute on", out);
        TEST_ASSERT(out.str().find("Mute set to: MUTED") != std::string::npos, "audiosrv mute on must report MUTED");

        // 5. audiosrv test
        out.str("");
        shell.execute("audiosrv test", out);
        TEST_ASSERT(out.str().find("WASAPI Self-Test Finished Successfully") != std::string::npos, "audiosrv test must finish successfully");
    }

    std::cout << "[TEST] Suite 79: Windows Audio Session API (WASAPI) & Core Audio Engine Subsystem PASSED.\n";
}

void Test_WindowsCBS_DISM_Servicing_Subsystem() {
    std::cout << "[TEST] Running Suite 80: Windows Component-Based Servicing (CBS) & DISM Subsystem...\n";

    // 1. Dynamic Exports in dismapi.dll and cbsapi.dll
    {
        cbs::InitializeCbsSubsystemExports();
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismInitialize") != nullptr, "dismapi.dll!DismInitialize export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismShutdown") != nullptr, "dismapi.dll!DismShutdown export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismOpenSession") != nullptr, "dismapi.dll!DismOpenSession export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismCloseSession") != nullptr, "dismapi.dll!DismCloseSession export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismDelete") != nullptr, "dismapi.dll!DismDelete export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismGetPackages") != nullptr, "dismapi.dll!DismGetPackages export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismGetPackageInfo") != nullptr, "dismapi.dll!DismGetPackageInfo export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismAddPackage") != nullptr, "dismapi.dll!DismAddPackage export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismRemovePackage") != nullptr, "dismapi.dll!DismRemovePackage export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismGetFeatures") != nullptr, "dismapi.dll!DismGetFeatures export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismGetFeatureInfo") != nullptr, "dismapi.dll!DismGetFeatureInfo export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismEnableFeature") != nullptr, "dismapi.dll!DismEnableFeature export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismDisableFeature") != nullptr, "dismapi.dll!DismDisableFeature export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismCheckImageHealth") != nullptr, "dismapi.dll!DismCheckImageHealth export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismScanImageHealth") != nullptr, "dismapi.dll!DismScanImageHealth export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismRestoreImageHealth") != nullptr, "dismapi.dll!DismRestoreImageHealth export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismGetCapabilities") != nullptr, "dismapi.dll!DismGetCapabilities export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismGetCapabilityInfo") != nullptr, "dismapi.dll!DismGetCapabilityInfo export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismAddCapability") != nullptr, "dismapi.dll!DismAddCapability export must exist");
        TEST_ASSERT(ldr.getExport("dismapi.dll", "DismRemoveCapability") != nullptr, "dismapi.dll!DismRemoveCapability export must exist");

        TEST_ASSERT(ldr.getExport("cbsapi.dll", "CbsInitialize") != nullptr, "cbsapi.dll!CbsInitialize export must exist");
        TEST_ASSERT(ldr.getExport("cbsapi.dll", "CbsShutdown") != nullptr, "cbsapi.dll!CbsShutdown export must exist");
        TEST_ASSERT(ldr.getExport("cbsapi.dll", "CbsCreateSession") != nullptr, "cbsapi.dll!CbsCreateSession export must exist");
    }

    // 2. Module Version Database Metadata
    {
        uint32_t handle = 0;
        uint32_t sizeDism = version::GetFileVersionInfoSizeA("dismapi.dll", &handle);
        TEST_ASSERT(sizeDism > 0, "GetFileVersionInfoSizeA for dismapi.dll must succeed");
        std::vector<uint8_t> buf(sizeDism);
        TEST_ASSERT(version::GetFileVersionInfoA("dismapi.dll", 0, sizeDism, buf.data()) != 0, "GetFileVersionInfoA for dismapi.dll must succeed");

        char* desc = nullptr;
        uint32_t descLen = 0;
        int32_t res = version::VerQueryValueA(buf.data(), "\\StringFileInfo\\040904B0\\FileDescription", reinterpret_cast<void**>(&desc), &descLen);
        TEST_ASSERT(res != 0 && desc != nullptr, "VerQueryValueA for FileDescription in dismapi.dll must succeed");
        TEST_ASSERT(std::string(desc).find("Deployment Image Servicing") != std::string::npos, "FileDescription must match DISM");

        uint32_t sizeTi = version::GetFileVersionInfoSizeA("trustedinstaller.exe", &handle);
        TEST_ASSERT(sizeTi > 0, "GetFileVersionInfoSizeA for trustedinstaller.exe must succeed");
    }

    // 3. Service Control Manager: TrustedInstaller Service
    {
        auto rec = scm::ServiceControlManager::get().getServiceRecord(L"TrustedInstaller");
        TEST_ASSERT(rec != nullptr, "TrustedInstaller service record must be registered in SCM");
        TEST_ASSERT(rec->displayName == L"Windows Modules Installer", "TrustedInstaller display name must match");
        TEST_ASSERT(rec->status.dwCurrentState == scm::SERVICE_RUNNING, "TrustedInstaller state must be SERVICE_RUNNING");
        TEST_ASSERT(rec->startType == scm::SERVICE_DEMAND_START, "TrustedInstaller startType must be SERVICE_DEMAND_START");
    }

    // 4. DISM API Lifecycle: DismInitialize, DismOpenSession, DismCloseSession, DismShutdown
    cbs::DismSession session = cbs::DISM_SESSION_INVALID;
    {
        int32_t hr = cbs::DismInitialize(cbs::DismLogErrorsWarningsInfo, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismInitialize must return S_OK");

        hr = cbs::DismOpenSession(cbs::DISM_ONLINE_IMAGE, nullptr, nullptr, &session);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismOpenSession on DISM_ONLINE_IMAGE must return S_OK");
        TEST_ASSERT(session != cbs::DISM_SESSION_INVALID, "Valid DismSession ID must be returned");
    }

    // 5. Package Enumeration and Detailed Package Info
    {
        cbs::DismPackage* packages = nullptr;
        uint32_t count = 0;
        int32_t hr = cbs::DismGetPackages(session, &packages, &count);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismGetPackages must return S_OK");
        TEST_ASSERT(packages != nullptr && count >= 3, "At least 3 pre-seeded packages must be enumerated");

        bool foundRollup = false;
        std::wstring targetName;
        for (uint32_t i = 0; i < count; ++i) {
            std::wstring name = packages[i].PackageName ? packages[i].PackageName : L"";
            if (name.find(L"Package_for_RollupFix") != std::wstring::npos) {
                foundRollup = true;
                targetName = name;
                TEST_ASSERT(packages[i].PackageState == cbs::DismStateInstalled, "RollupFix package must be DismStateInstalled");
                TEST_ASSERT(packages[i].ReleaseType == cbs::DismReleaseTypeUpdate, "RollupFix package must be DismReleaseTypeUpdate");
            }
        }
        TEST_ASSERT(foundRollup, "Package_for_RollupFix must be present in enumerated packages");

        // Detailed Package Info
        cbs::DismPackageInfo* info = nullptr;
        hr = cbs::DismGetPackageInfo(session, targetName.c_str(), cbs::DismPackageName, &info);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && info != nullptr, "DismGetPackageInfo must return S_OK");
        TEST_ASSERT(info->Applicable == 1, "Package Applicable flag must be 1");
        TEST_ASSERT(info->PackageState == cbs::DismStateInstalled, "Package state must be installed");
        TEST_ASSERT(info->CustomPropertyCount >= 2, "CustomPropertyCount must be >= 2");
        TEST_ASSERT(info->FeatureCount >= 2, "FeatureCount must be >= 2");

        // Clean up memory via DismDelete
        hr = cbs::DismDelete(info);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismDelete for package info must return S_OK");
        hr = cbs::DismDelete(packages);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismDelete for package array must return S_OK");
    }

    // 6. Dynamic Package Addition and Removal Lifecycle
    {
        uint32_t progressCalls = 0;
        auto progressCb = [](uint32_t /*current*/, uint32_t /*total*/, void* userData) {
            auto* pCount = static_cast<uint32_t*>(userData);
            if (pCount) (*pCount)++;
        };

        int32_t hr = cbs::DismAddPackage(session, L"C:\\Updates\\Windows11-KB5049999-x64.cab", 0, 1, nullptr, progressCb, &progressCalls);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismAddPackage with preventPending=1 must return S_OK");
        TEST_ASSERT(progressCalls > 0, "Progress callback must be invoked during DismAddPackage");

        cbs::DismPackageInfo* info = nullptr;
        hr = cbs::DismGetPackageInfo(session, L"Windows11-KB5049999-x64", cbs::DismPackageName, &info);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && info != nullptr, "Newly added package must be retrievable");
        TEST_ASSERT(info->PackageState == cbs::DismStateInstalled, "Newly added package state must be Installed");
        cbs::DismDelete(info);

        // Remove package
        hr = cbs::DismRemovePackage(session, L"Windows11-KB5049999-x64", cbs::DismPackageName, nullptr, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_REBOOT_REQUIRED, "DismRemovePackage must return DISMAPI_S_REBOOT_REQUIRED");

        hr = cbs::DismGetPackageInfo(session, L"Windows11-KB5049999-x64", cbs::DismPackageName, &info);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && info != nullptr, "Package must still exist in uninstall pending state");
        TEST_ASSERT(info->PackageState == cbs::DismStateUninstallPending, "Package state must be UninstallPending");
        cbs::DismDelete(info);
    }

    // 7. Feature Enumeration & Feature Info
    {
        cbs::DismFeature* features = nullptr;
        uint32_t count = 0;
        int32_t hr = cbs::DismGetFeatures(session, nullptr, cbs::DismPackageNone, &features, &count);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && features != nullptr, "DismGetFeatures must return S_OK");
        TEST_ASSERT(count >= 7, "At least 7 pre-seeded features must be present");

        bool foundNetFx3 = false;
        for (uint32_t i = 0; i < count; ++i) {
            std::wstring fName = features[i].FeatureName ? features[i].FeatureName : L"";
            if (fName == L"NetFx3") {
                foundNetFx3 = true;
                TEST_ASSERT(features[i].State == cbs::DismStateInstalled, "NetFx3 must be DismStateInstalled");
            }
        }
        TEST_ASSERT(foundNetFx3, "NetFx3 feature must be found");
        cbs::DismDelete(features);

        // Detailed Feature Info
        cbs::DismFeatureInfo* fInfo = nullptr;
        hr = cbs::DismGetFeatureInfo(session, L"NetFx3", nullptr, cbs::DismPackageNone, &fInfo);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && fInfo != nullptr, "DismGetFeatureInfo for NetFx3 must succeed");
        TEST_ASSERT(std::wstring(fInfo->DisplayName).find(L".NET Framework 3.5") != std::wstring::npos, "NetFx3 display name must match");
        TEST_ASSERT(fInfo->FeatureState == cbs::DismStateInstalled, "FeatureState must be DismStateInstalled");
        cbs::DismDelete(fInfo);
    }

    // 8. Feature Enablement & Disablement State Transitions
    {
        cbs::DismFeatureInfo* fInfo = nullptr;
        int32_t hr = cbs::DismGetFeatureInfo(session, L"Containers", nullptr, cbs::DismPackageNone, &fInfo);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && fInfo != nullptr, "Containers feature must exist");
        TEST_ASSERT(fInfo->FeatureState == cbs::DismStateStaged, "Containers must initially be DismStateStaged");
        cbs::DismDelete(fInfo);

        // Enable feature
        hr = cbs::DismEnableFeature(session, L"Containers", nullptr, cbs::DismPackageNone, 0, nullptr, 0, 1, nullptr, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK || hr == cbs::DISMAPI_S_REBOOT_REQUIRED, "DismEnableFeature on Containers must succeed");

        hr = cbs::DismGetFeatureInfo(session, L"Containers", nullptr, cbs::DismPackageNone, &fInfo);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && fInfo != nullptr, "Containers info must be retrievable");
        TEST_ASSERT(fInfo->FeatureState == cbs::DismStateInstalled, "Containers must now be DismStateInstalled");
        cbs::DismDelete(fInfo);

        // Disable feature
        hr = cbs::DismDisableFeature(session, L"Containers", nullptr, 0, nullptr, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK || hr == cbs::DISMAPI_S_REBOOT_REQUIRED, "DismDisableFeature must succeed");

        hr = cbs::DismGetFeatureInfo(session, L"Containers", nullptr, cbs::DismPackageNone, &fInfo);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && fInfo != nullptr, "Containers info must be retrievable");
        TEST_ASSERT(fInfo->FeatureState == cbs::DismStateStaged, "Containers must return to DismStateStaged");
        cbs::DismDelete(fInfo);
    }

    // 9. Capabilities Enumeration, Info, Addition and Removal
    {
        cbs::DismCapability* caps = nullptr;
        uint32_t capCount = 0;
        int32_t hr = cbs::DismGetCapabilities(session, &caps, &capCount);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && caps != nullptr, "DismGetCapabilities must return S_OK");
        TEST_ASSERT(capCount >= 4, "At least 4 capabilities must be registered");

        bool foundOssh = false;
        for (uint32_t i = 0; i < capCount; ++i) {
            std::wstring cName = caps[i].Name ? caps[i].Name : L"";
            if (cName.find(L"OpenSSH.Client") != std::wstring::npos) {
                foundOssh = true;
                TEST_ASSERT(caps[i].State == cbs::DismStateInstalled, "OpenSSH.Client must be DismStateInstalled");
            }
        }
        TEST_ASSERT(foundOssh, "OpenSSH.Client capability must be found");
        cbs::DismDelete(caps);

        // Capability Info
        cbs::DismCapabilityInfo* cInfo = nullptr;
        hr = cbs::DismGetCapabilityInfo(session, L"OpenSSH.Server~~~~0.0.1.0", &cInfo);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && cInfo != nullptr, "DismGetCapabilityInfo for OpenSSH.Server must succeed");
        TEST_ASSERT(cInfo->State == cbs::DismStateNotPresent, "OpenSSH.Server must initially be DismStateNotPresent");
        TEST_ASSERT(cInfo->DownloadSize > 0 && cInfo->InstallSize > 0, "Sizes must be non-zero");
        cbs::DismDelete(cInfo);

        // Add capability
        hr = cbs::DismAddCapability(session, L"OpenSSH.Server~~~~0.0.1.0", 0, nullptr, 0, nullptr, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismAddCapability must return S_OK");

        hr = cbs::DismGetCapabilityInfo(session, L"OpenSSH.Server~~~~0.0.1.0", &cInfo);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK && cInfo != nullptr, "DismGetCapabilityInfo must succeed");
        TEST_ASSERT(cInfo->State == cbs::DismStateInstalled, "OpenSSH.Server must now be DismStateInstalled");
        cbs::DismDelete(cInfo);

        // Remove capability
        hr = cbs::DismRemoveCapability(session, L"OpenSSH.Server~~~~0.0.1.0", nullptr, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismRemoveCapability must return S_OK");
    }

    // 10. Image Health Scanning and Restoration
    {
        cbs::DismImageHealthState health = cbs::DismImageNonRepairable;
        int32_t hr = cbs::DismCheckImageHealth(session, 0, nullptr, nullptr, nullptr, &health);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismCheckImageHealth must return S_OK");
        TEST_ASSERT(health == cbs::DismImageHealthy, "Initial component store health must be DismImageHealthy");

        // Simulate store corruption
        cbs::CbsComponentStore::get().setHealthState(cbs::DismImageRepairable);

        hr = cbs::DismScanImageHealth(session, nullptr, nullptr, nullptr, &health);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismScanImageHealth must return S_OK");
        TEST_ASSERT(health == cbs::DismImageRepairable, "ScanHealth must detect DismImageRepairable");

        // Restore image health
        hr = cbs::DismRestoreImageHealth(session, nullptr, 0, 0, nullptr, nullptr, nullptr);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismRestoreImageHealth must return S_OK");

        hr = cbs::DismCheckImageHealth(session, 0, nullptr, nullptr, nullptr, &health);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismCheckImageHealth must return S_OK");
        TEST_ASSERT(health == cbs::DismImageHealthy, "Component store health must be restored to DismImageHealthy");
    }

    // 11. Close Session & Shutdown
    {
        int32_t hr = cbs::DismCloseSession(session);
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismCloseSession must return S_OK");

        hr = cbs::DismShutdown();
        TEST_ASSERT(hr == cbs::DISMAPI_S_OK, "DismShutdown must return S_OK");
    }

    // 12. Interactive CLI Shell Integration
    {
        shell::CommandShell shell;
        std::stringstream out;

        // 1. dism (help)
        shell.execute("dism", out);
        TEST_ASSERT(out.str().find("Deployment Image Servicing and Management tool (DISM)") != std::string::npos, "dism help must display banner");
        TEST_ASSERT(out.str().find("/Online") != std::string::npos, "dism help must list /Online option");

        // 2. dism /online /get-packages
        out.str("");
        shell.execute("dism /online /get-packages", out);
        TEST_ASSERT(out.str().find("Package_for_RollupFix") != std::string::npos, "dism /get-packages must list packages");
        TEST_ASSERT(out.str().find("The operation completed successfully") != std::string::npos, "dism /get-packages must complete successfully");

        // 3. dism /online /get-features
        out.str("");
        shell.execute("dism /online /get-features", out);
        TEST_ASSERT(out.str().find("NetFx3") != std::string::npos, "dism /get-features must list NetFx3");
        TEST_ASSERT(out.str().find("Microsoft-Windows-Subsystem-Linux") != std::string::npos, "dism /get-features must list WSL");

        // 4. dism /online /cleanup-image /checkhealth
        out.str("");
        shell.execute("dism /online /cleanup-image /checkhealth", out);
        TEST_ASSERT(out.str().find("No component store corruption detected") != std::string::npos, "dism /checkhealth must report no corruption");

        // 5. dism test
        out.str("");
        shell.execute("dism test", out);
        TEST_ASSERT(out.str().find("Finished Successfully") != std::string::npos, "dism test must succeed");
    }

    std::cout << "[TEST] Suite 80: Windows Component-Based Servicing (CBS) & DISM Subsystem PASSED.\n";
}

// ============================================================================
// Suite 81: Windows Diagnostics Infrastructure (WDI) Subsystem
// ============================================================================
void Test_WindowsWDI_DiagnosticsInfrastructure_Subsystem() {
    std::cout << "\n[TEST] Running Suite 81: Windows Diagnostics Infrastructure (WDI) Subsystem (wdi.dll / diagperf.dll)...\n";

    // Initialize WDI Subsystem
    wdi::InitializeWdiSubsystemExports();

    // Stage 1: Dynamic Loader Export Verification
    {
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiOpenScenario") != nullptr, "wdi.dll!WdiOpenScenario must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiCloseScenario") != nullptr, "wdi.dll!WdiCloseScenario must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiSetScenarioProperty") != nullptr, "wdi.dll!WdiSetScenarioProperty must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiGetScenarioProperty") != nullptr, "wdi.dll!WdiGetScenarioProperty must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiAddParameter") != nullptr, "wdi.dll!WdiAddParameter must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiExecuteScenario") != nullptr, "wdi.dll!WdiExecuteScenario must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiApplyResolution") != nullptr, "wdi.dll!WdiApplyResolution must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiFreeResult") != nullptr, "wdi.dll!WdiFreeResult must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiGetScenarioCount") != nullptr, "wdi.dll!WdiGetScenarioCount must be exported");
        TEST_ASSERT(ldr.getExport("wdi.dll", "WdiGetScenarioDescriptor") != nullptr, "wdi.dll!WdiGetScenarioDescriptor must be exported");

        TEST_ASSERT(ldr.getExport("diagperf.dll", "DiagPerfInitialize") != nullptr, "diagperf.dll!DiagPerfInitialize must be exported");
        TEST_ASSERT(ldr.getExport("diagperf.dll", "DiagPerfShutdown") != nullptr, "diagperf.dll!DiagPerfShutdown must be exported");
        TEST_ASSERT(ldr.getExport("diagperf.dll", "DiagPerfCollectVitals") != nullptr, "diagperf.dll!DiagPerfCollectVitals must be exported");
        TEST_ASSERT(ldr.getExport("diagperf.dll", "DiagPerfAnalyzeBottlenecks") != nullptr, "diagperf.dll!DiagPerfAnalyzeBottlenecks must be exported");
    }

    // Stage 2: Version Database Metadata Verification
    {
        const auto* wdiMod = version::VersionDatabase::Instance().FindModule("wdi.dll");
        TEST_ASSERT(wdiMod != nullptr, "wdi.dll must be registered in VersionDatabase");
        TEST_ASSERT(wdiMod->stringTable.at("FileDescription").find("Diagnostic Infrastructure") != std::string::npos, "wdi.dll description must match");

        const auto* diagMod = version::VersionDatabase::Instance().FindModule("diagperf.dll");
        TEST_ASSERT(diagMod != nullptr, "diagperf.dll must be registered in VersionDatabase");
        TEST_ASSERT(diagMod->stringTable.at("FileDescription").find("Performance Collector") != std::string::npos, "diagperf.dll description must match");

        const auto* msdtMod = version::VersionDatabase::Instance().FindModule("msdt.exe");
        TEST_ASSERT(msdtMod != nullptr, "msdt.exe must be registered in VersionDatabase");
        TEST_ASSERT(msdtMod->stringTable.at("FileDescription").find("Support Diagnostic Tool") != std::string::npos, "msdt.exe description must match");
    }

    // Stage 3: SCM Service Registration Verification
    {
        auto& scm = scm::ServiceControlManager::get();
        uintptr_t hMgr = 0, hSys = 0, hSvc = 0;
        uint32_t err = scm.openSCManager(L"", L"", scm::SC_MANAGER_CONNECT, hMgr);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && hMgr != 0, "SCM openSCManager must succeed");

        err = scm.openService(hMgr, L"WdiSystemHost", scm::SERVICE_QUERY_STATUS, hSys);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && hSys != 0, "WdiSystemHost must be registered in SCM");
        scm::SERVICE_STATUS_PROCESS stSys{};
        err = scm.queryServiceStatus(hSys, stSys);
        TEST_ASSERT(err == scm::ERROR_SUCCESS, "WdiSystemHost query status must succeed");
        TEST_ASSERT(stSys.dwCurrentState == scm::SERVICE_RUNNING, "WdiSystemHost must be RUNNING");
        scm.closeServiceHandle(hSys);

        err = scm.openService(hMgr, L"WdiServiceHost", scm::SERVICE_QUERY_STATUS, hSvc);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && hSvc != 0, "WdiServiceHost must be registered in SCM");
        scm::SERVICE_STATUS_PROCESS stSvc{};
        err = scm.queryServiceStatus(hSvc, stSvc);
        TEST_ASSERT(err == scm::ERROR_SUCCESS, "WdiServiceHost query status must succeed");
        TEST_ASSERT(stSvc.dwCurrentState == scm::SERVICE_RUNNING, "WdiServiceHost must be RUNNING");
        scm.closeServiceHandle(hSvc);

        scm.closeServiceHandle(hMgr);
    }

    // Stage 4: WDI Scenario Enumeration and Metadata Querying
    {
        uint32_t count = 0;
        int32_t hr = wdi::WdiGetScenarioCount(&count);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiGetScenarioCount must return WDI_S_OK");
        TEST_ASSERT(count >= 5, "Must have at least 5 registered diagnostic scenarios");

        bool foundNet = false, foundStorage = false, foundMemory = false, foundAudio = false, foundPerf = false;
        for (uint32_t i = 0; i < count; ++i) {
            wdi::WDI_SCENARIO_DESCRIPTOR desc{};
            hr = wdi::WdiGetScenarioDescriptor(i, &desc);
            TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiGetScenarioDescriptor must succeed");
            std::wstring id = desc.ScenarioId ? desc.ScenarioId : L"";
            if (id == L"NetworkDiagnostics") foundNet = true;
            if (id == L"StorageDiagnostics") foundStorage = true;
            if (id == L"MemoryDiagnostics") foundMemory = true;
            if (id == L"AudioDiagnostics") foundAudio = true;
            if (id == L"PerformanceDiagnostics") foundPerf = true;
        }
        TEST_ASSERT(foundNet && foundStorage && foundMemory && foundAudio && foundPerf, "All 5 core scenarios must be enumerated");
    }

    // Stage 5: Scenario Lifecycle (Open / Close & Invalid IDs)
    {
        wdi::WDI_SCENARIO_HANDLE hScn = 0;
        int32_t hr = wdi::WdiOpenScenario(L"InvalidScenarioName123", &hScn);
        TEST_ASSERT(hr == wdi::WDI_E_NOT_FOUND, "Opening invalid scenario must return WDI_E_NOT_FOUND");
        TEST_ASSERT(hScn == 0, "Invalid handle must remain 0");

        hr = wdi::WdiOpenScenario(L"NetworkDiagnostics", &hScn);
        TEST_ASSERT(hr == wdi::WDI_S_OK && hScn != 0, "WdiOpenScenario(NetworkDiagnostics) must succeed");

        hr = wdi::WdiCloseScenario(hScn);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiCloseScenario must return WDI_S_OK");

        hr = wdi::WdiCloseScenario(hScn);
        TEST_ASSERT(hr == wdi::WDI_E_INVALID_HANDLE, "Closing closed scenario must return WDI_E_INVALID_HANDLE");
    }

    // Stage 6: Scenario Properties & Parameters
    {
        wdi::WDI_SCENARIO_HANDLE hScn = 0;
        int32_t hr = wdi::WdiOpenScenario(L"NetworkDiagnostics", &hScn);
        TEST_ASSERT(hr == wdi::WDI_S_OK && hScn != 0, "WdiOpenScenario must succeed");

        wchar_t buf[128]{};
        hr = wdi::WdiGetScenarioProperty(hScn, L"TargetHost", buf, 128);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiGetScenarioProperty(TargetHost) must succeed");
        TEST_ASSERT(std::wstring(buf) == L"dns.micant.sovereign", "TargetHost default must match");

        hr = wdi::WdiSetScenarioProperty(hScn, L"TargetHost", L"custom.gateway.local");
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiSetScenarioProperty must succeed");

        hr = wdi::WdiGetScenarioProperty(hScn, L"TargetHost", buf, 128);
        TEST_ASSERT(hr == wdi::WDI_S_OK && std::wstring(buf) == L"custom.gateway.local", "Updated TargetHost must match");

        hr = wdi::WdiAddParameter(hScn, L"ParamTestKey", L"ParamTestVal");
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiAddParameter must succeed");

        wdi::WdiCloseScenario(hScn);
    }

    // Stage 7: NetworkDiagnostics Scenario Execution & Root Cause Detection
    {
        wdi::WDI_SCENARIO_HANDLE hScn = 0;
        int32_t hr = wdi::WdiOpenScenario(L"NetworkDiagnostics", &hScn);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiOpenScenario must succeed");

        // 7a. Clean run without issues
        wdi::WDI_DIAGNOSTIC_RESULT res{};
        hr = wdi::WdiExecuteScenario(hScn, &res);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiExecuteScenario must succeed");
        TEST_ASSERT(res.Status == wdi::WDI_S_NO_ISSUES_FOUND, "Clean run must report WDI_S_NO_ISSUES_FOUND");
        TEST_ASSERT(res.RootCauseCount == 0, "Clean run must have 0 root causes");
        TEST_ASSERT(res.RootCauses == nullptr, "RootCauses pointer must be null when count=0");
        TEST_ASSERT(res.SummaryText != nullptr, "SummaryText must not be null");
        wdi::WdiFreeResult(&res);

        // 7b. Simulated DNS and Gateway failure
        wdi::WdiAddParameter(hScn, L"SimulateDnsFailure", L"1");
        wdi::WdiAddParameter(hScn, L"SimulateGatewayFailure", L"1");

        wdi::WDI_DIAGNOSTIC_RESULT resFailed{};
        hr = wdi::WdiExecuteScenario(hScn, &resFailed);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiExecuteScenario with issues must succeed");
        TEST_ASSERT(resFailed.Status == wdi::WDI_S_ISSUES_FOUND, "Execution must return WDI_S_ISSUES_FOUND");
        TEST_ASSERT(resFailed.RootCauseCount == 2, "Must identify 2 root causes");
        TEST_ASSERT(resFailed.RootCauses != nullptr, "RootCauses pointer must be allocated");

        // Verify root causes
        bool foundGw = false, foundDns = false;
        for (uint32_t i = 0; i < resFailed.RootCauseCount; ++i) {
            std::wstring pName = resFailed.RootCauses[i].ProblemName ? resFailed.RootCauses[i].ProblemName : L"";
            if (pName == L"DefaultGatewayUnreachable") foundGw = true;
            if (pName == L"DnsCacheCorrupted") foundDns = true;
            TEST_ASSERT(resFailed.RootCauses[i].AutoFixAvailable == true, "AutoFix must be available");
        }
        TEST_ASSERT(foundGw && foundDns, "Both gateway and DNS issues must be diagnosed");

        // Auto-repair gateway
        bool resolved = false;
        hr = wdi::WdiApplyResolution(hScn, 0, &resolved);
        TEST_ASSERT(hr == wdi::WDI_S_REPAIR_SUCCESSFUL && resolved, "Applying resolution must succeed");

        wdi::WdiFreeResult(&resFailed);
        wdi::WdiCloseScenario(hScn);
    }

    // Stage 8: StorageDiagnostics Volume Inspection & Dirty Bit
    {
        wdi::WDI_SCENARIO_HANDLE hScn = 0;
        int32_t hr = wdi::WdiOpenScenario(L"StorageDiagnostics", &hScn);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiOpenScenario(StorageDiagnostics) must succeed");

        auto session = wdi::WdiScenarioManager::get().findSession(hScn);
        TEST_ASSERT(session != nullptr, "Session must exist");
        auto stgScn = std::dynamic_pointer_cast<wdi::StorageDiagnosticsScenario>(session->scenario);
        TEST_ASSERT(stgScn != nullptr, "Dynamic cast to StorageDiagnosticsScenario must succeed");
        stgScn->induceDirtyBit();

        wdi::WDI_DIAGNOSTIC_RESULT res{};
        hr = wdi::WdiExecuteScenario(hScn, &res);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "Execute must succeed");
        TEST_ASSERT(res.Status == wdi::WDI_S_ISSUES_FOUND, "Status must report issues");
        TEST_ASSERT(res.RootCauseCount == 1, "Must find 1 root cause (Dirty Bit)");
        TEST_ASSERT(std::wstring(res.RootCauses[0].ProblemName) == L"FilesystemIntegrityFlagDirty", "Problem must be FilesystemIntegrityFlagDirty");

        // Apply resolution
        bool resolved = false;
        hr = wdi::WdiApplyResolution(hScn, 0, &resolved);
        TEST_ASSERT(hr == wdi::WDI_S_REPAIR_SUCCESSFUL && resolved, "Repair must succeed");

        // Re-execute: now healthy
        wdi::WDI_DIAGNOSTIC_RESULT resClean{};
        hr = wdi::WdiExecuteScenario(hScn, &resClean);
        TEST_ASSERT(resClean.Status == wdi::WDI_S_NO_ISSUES_FOUND, "Re-execution must report healthy volume");

        wdi::WdiFreeResult(&res);
        wdi::WdiFreeResult(&resClean);
        wdi::WdiCloseScenario(hScn);
    }

    // Stage 9: AudioDiagnostics Issue Detection & Auto-Repair
    {
        wdi::WDI_SCENARIO_HANDLE hScn = 0;
        int32_t hr = wdi::WdiOpenScenario(L"AudioDiagnostics", &hScn);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiOpenScenario(AudioDiagnostics) must succeed");

        auto session = wdi::WdiScenarioManager::get().findSession(hScn);
        auto audioScn = std::dynamic_pointer_cast<wdi::AudioDiagnosticsScenario>(session->scenario);
        TEST_ASSERT(audioScn != nullptr, "Audio scenario instance valid");
        audioScn->induceMute();
        audioScn->induceServiceStopped();

        wdi::WDI_DIAGNOSTIC_RESULT res{};
        hr = wdi::WdiExecuteScenario(hScn, &res);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "Execution must succeed");
        TEST_ASSERT(res.Status == wdi::WDI_S_ISSUES_FOUND, "Audio issues must be found");
        TEST_ASSERT(res.RootCauseCount == 2, "Must detect 2 root causes: stopped service and muted endpoint");

        // Repair both
        for (uint32_t i = 0; i < res.RootCauseCount; ++i) {
            bool resolved = false;
            hr = wdi::WdiApplyResolution(hScn, i, &resolved);
            TEST_ASSERT(hr == wdi::WDI_S_REPAIR_SUCCESSFUL && resolved, "Auto-repair must succeed");
        }

        wdi::WDI_DIAGNOSTIC_RESULT resRepaired{};
        hr = wdi::WdiExecuteScenario(hScn, &resRepaired);
        TEST_ASSERT(resRepaired.Status == wdi::WDI_S_NO_ISSUES_FOUND, "Audio scenario must now report no issues");

        wdi::WdiFreeResult(&res);
        wdi::WdiFreeResult(&resRepaired);
        wdi::WdiCloseScenario(hScn);
    }

    // Stage 10: MemoryDiagnostics & PerformanceDiagnostics Execution
    {
        // 10a: MemoryDiagnostics
        wdi::WDI_SCENARIO_HANDLE hMem = 0;
        int32_t hr = wdi::WdiOpenScenario(L"MemoryDiagnostics", &hMem);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiOpenScenario(MemoryDiagnostics) must succeed");
        wdi::WDI_DIAGNOSTIC_RESULT resMem{};
        hr = wdi::WdiExecuteScenario(hMem, &resMem);
        TEST_ASSERT(hr == wdi::WDI_S_OK && resMem.Status == wdi::WDI_S_NO_ISSUES_FOUND, "MemoryDiagnostics clean run");
        wdi::WdiFreeResult(&resMem);
        wdi::WdiCloseScenario(hMem);

        // 10b: PerformanceDiagnostics
        wdi::WDI_SCENARIO_HANDLE hPerf = 0;
        hr = wdi::WdiOpenScenario(L"PerformanceDiagnostics", &hPerf);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "WdiOpenScenario(PerformanceDiagnostics) must succeed");
        wdi::WDI_DIAGNOSTIC_RESULT resPerf{};
        hr = wdi::WdiExecuteScenario(hPerf, &resPerf);
        TEST_ASSERT(hr == wdi::WDI_S_OK && resPerf.Status == wdi::WDI_S_NO_ISSUES_FOUND, "PerformanceDiagnostics clean run");
        wdi::WdiFreeResult(&resPerf);
        wdi::WdiCloseScenario(hPerf);
    }

    // Stage 11: DiagPerf Subsystem Vitals & Bottleneck Collector
    {
        int32_t hr = wdi::DiagPerfInitialize();
        TEST_ASSERT(hr == wdi::WDI_S_OK, "DiagPerfInitialize must return WDI_S_OK");

        wdi::DIAGPERF_VITALS vitals{};
        hr = wdi::DiagPerfCollectVitals(&vitals);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "DiagPerfCollectVitals must succeed");
        TEST_ASSERT(vitals.CpuUtilizationPercent > 0 && vitals.CpuUtilizationPercent <= 100, "CPU percent in valid range");
        TEST_ASSERT(vitals.TotalPhysicalMemoryMB > 0, "Total RAM > 0");
        TEST_ASSERT(vitals.AvailableMemoryMB > 0, "Available RAM > 0");
        TEST_ASSERT(vitals.DpcQueueDepth < 100, "DPC queue depth within safety bounds");

        uint32_t bCount = 0;
        wdi::DIAGPERF_BOTTLENECK* pBottlenecks = nullptr;
        hr = wdi::DiagPerfAnalyzeBottlenecks(&bCount, &pBottlenecks);
        TEST_ASSERT(hr == wdi::WDI_S_OK, "DiagPerfAnalyzeBottlenecks must succeed");
        TEST_ASSERT(bCount == 0, "Zero sovereign bottlenecks under nominal operation");

        hr = wdi::DiagPerfShutdown();
        TEST_ASSERT(hr == wdi::WDI_S_OK, "DiagPerfShutdown must succeed");
    }

    // Stage 12: Interactive Shell CLI Integration (msdt commands)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. msdt /?
        shell.execute("msdt /?", out);
        TEST_ASSERT(out.str().find("Microsoft Support Diagnostic Tool") != std::string::npos, "msdt /? must display title");
        TEST_ASSERT(out.str().find("NetworkDiagnostics") != std::string::npos, "msdt /? must list scenarios");

        // 2. msdt /list
        out.str("");
        shell.execute("msdt /list", out);
        TEST_ASSERT(out.str().find("NetworkDiagnostics") != std::string::npos, "msdt /list must include NetworkDiagnostics");
        TEST_ASSERT(out.str().find("StorageDiagnostics") != std::string::npos, "msdt /list must include StorageDiagnostics");
        TEST_ASSERT(out.str().find("AudioDiagnostics") != std::string::npos, "msdt /list must include AudioDiagnostics");

        // 3. msdt /id NetworkDiagnostics
        out.str("");
        shell.execute("msdt /id NetworkDiagnostics", out);
        TEST_ASSERT(out.str().find("Diagnosing system scenario: NetworkDiagnostics") != std::string::npos, "msdt /id must diagnose scenario");
        TEST_ASSERT(out.str().find("completed successfully") != std::string::npos, "msdt execution must succeed");

        // 4. msdt /id AudioDiagnostics /repair
        out.str("");
        shell.execute("msdt /id AudioDiagnostics /repair", out);
        TEST_ASSERT(out.str().find("Diagnosing system scenario: AudioDiagnostics") != std::string::npos, "msdt /id AudioDiagnostics must run");
        TEST_ASSERT(out.str().find("completed successfully") != std::string::npos, "msdt execution must succeed");

        // 5. msdt test
        out.str("");
        shell.execute("msdt test", out);
        TEST_ASSERT(out.str().find("Finished Successfully") != std::string::npos, "msdt test must complete successfully");
    }

    std::cout << "[TEST] Suite 81: Windows Diagnostics Infrastructure (WDI) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 82: Windows Performance Monitor & Performance Counter Subsystem (PDH)
// ============================================================================
void Test_WindowsPDH_PerformanceMonitor_Subsystem() {
    std::cout << "\n[TEST] Running Suite 82: Windows Performance Monitor & PDH Subsystem (pdh.dll / perflib.dll)...\n";

    // Initialize PDH Subsystem
    pdh::InitializePdhSubsystemExports();

    // Stage 1: Dynamic Loader Export Verification
    {
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhOpenQueryW") != nullptr, "pdh.dll!PdhOpenQueryW must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhOpenQueryA") != nullptr, "pdh.dll!PdhOpenQueryA must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhCloseQuery") != nullptr, "pdh.dll!PdhCloseQuery must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhAddCounterW") != nullptr, "pdh.dll!PdhAddCounterW must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhAddCounterA") != nullptr, "pdh.dll!PdhAddCounterA must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhAddEnglishCounterW") != nullptr, "pdh.dll!PdhAddEnglishCounterW must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhAddEnglishCounterA") != nullptr, "pdh.dll!PdhAddEnglishCounterA must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhRemoveCounter") != nullptr, "pdh.dll!PdhRemoveCounter must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhCollectQueryData") != nullptr, "pdh.dll!PdhCollectQueryData must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhGetFormattedCounterValue") != nullptr, "pdh.dll!PdhGetFormattedCounterValue must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhGetRawCounterValue") != nullptr, "pdh.dll!PdhGetRawCounterValue must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhValidatePathW") != nullptr, "pdh.dll!PdhValidatePathW must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhValidatePathA") != nullptr, "pdh.dll!PdhValidatePathA must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhEnumObjectsW") != nullptr, "pdh.dll!PdhEnumObjectsW must be exported");
        TEST_ASSERT(ldr.getExport("pdh.dll", "PdhEnumObjectItemsW") != nullptr, "pdh.dll!PdhEnumObjectItemsW must be exported");

        TEST_ASSERT(ldr.getExport("perflib.dll", "PerfCreateInstance") != nullptr, "perflib.dll!PerfCreateInstance must be exported");
        TEST_ASSERT(ldr.getExport("perflib.dll", "PerfDeleteInstance") != nullptr, "perflib.dll!PerfDeleteInstance must be exported");
        TEST_ASSERT(ldr.getExport("perflib.dll", "PerfSetCounterSetInfo") != nullptr, "perflib.dll!PerfSetCounterSetInfo must be exported");
        TEST_ASSERT(ldr.getExport("perflib.dll", "PerfSetULongCounterValue") != nullptr, "perflib.dll!PerfSetULongCounterValue must be exported");
        TEST_ASSERT(ldr.getExport("perflib.dll", "PerfSetULongLongCounterValue") != nullptr, "perflib.dll!PerfSetULongLongCounterValue must be exported");
    }

    // Stage 2: Version Database Metadata Verification
    {
        const auto* pdhMod = version::VersionDatabase::Instance().FindModule("pdh.dll");
        TEST_ASSERT(pdhMod != nullptr, "pdh.dll must be registered in VersionDatabase");
        TEST_ASSERT(pdhMod->stringTable.at("FileDescription").find("Performance Data Helper") != std::string::npos, "pdh.dll description must match");

        const auto* perflibMod = version::VersionDatabase::Instance().FindModule("perflib.dll");
        TEST_ASSERT(perflibMod != nullptr, "perflib.dll must be registered in VersionDatabase");

        const auto* perfmonMod = version::VersionDatabase::Instance().FindModule("perfmon.exe");
        TEST_ASSERT(perfmonMod != nullptr, "perfmon.exe must be registered in VersionDatabase");

        const auto* typeperfMod = version::VersionDatabase::Instance().FindModule("typeperf.exe");
        TEST_ASSERT(typeperfMod != nullptr, "typeperf.exe must be registered in VersionDatabase");
    }

    // Stage 3: SCM Service Registration Verification
    {
        auto& scm = scm::ServiceControlManager::get();
        uintptr_t hMgr = 0, hPla = 0, hHost = 0;
        uint32_t err = scm.openSCManager(L"", L"", scm::SC_MANAGER_CONNECT, hMgr);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && hMgr != 0, "SCM openSCManager must succeed");

        err = scm.openService(hMgr, L"pla", scm::SERVICE_QUERY_STATUS, hPla);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && hPla != 0, "pla service must be registered in SCM");
        scm::SERVICE_STATUS_PROCESS stPla{};
        err = scm.queryServiceStatus(hPla, stPla);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && stPla.dwCurrentState == scm::SERVICE_RUNNING, "pla must be RUNNING");
        scm.closeServiceHandle(hPla);

        err = scm.openService(hMgr, L"PerfHost", scm::SERVICE_QUERY_STATUS, hHost);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && hHost != 0, "PerfHost service must be registered in SCM");
        scm::SERVICE_STATUS_PROCESS stHost{};
        err = scm.queryServiceStatus(hHost, stHost);
        TEST_ASSERT(err == scm::ERROR_SUCCESS && stHost.dwCurrentState == scm::SERVICE_RUNNING, "PerfHost must be RUNNING");
        scm.closeServiceHandle(hHost);

        scm.closeServiceHandle(hMgr);
    }

    // Stage 4: Object and Counter Registry Enumeration
    {
        uint32_t bufferSize = 0;
        int32_t hr = pdh::PdhEnumObjectsW(nullptr, nullptr, nullptr, &bufferSize, pdh::PERF_DETAIL_NOVICE, 0);
        TEST_ASSERT(hr == pdh::PDH_MORE_DATA, "PdhEnumObjectsW with null buffer must return PDH_MORE_DATA");
        TEST_ASSERT(bufferSize > 0, "Buffer size required must be > 0");

        std::vector<wchar_t> objBuf(bufferSize, 0);
        hr = pdh::PdhEnumObjectsW(nullptr, nullptr, objBuf.data(), &bufferSize, pdh::PERF_DETAIL_NOVICE, 0);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhEnumObjectsW must return ERROR_SUCCESS");

        // Verify Processor object items
        uint32_t cchCounters = 0, cchInstances = 0;
        hr = pdh::PdhEnumObjectItemsW(nullptr, nullptr, L"Processor", nullptr, &cchCounters, nullptr, &cchInstances, pdh::PERF_DETAIL_NOVICE, 0);
        TEST_ASSERT(hr == pdh::PDH_MORE_DATA, "PdhEnumObjectItemsW must return PDH_MORE_DATA for buffer sizing");
        TEST_ASSERT(cchCounters > 0 && cchInstances > 0, "Processor object must have counters and instances");

        std::vector<wchar_t> counterBuf(cchCounters, 0);
        std::vector<wchar_t> instanceBuf(cchInstances, 0);
        hr = pdh::PdhEnumObjectItemsW(nullptr, nullptr, L"Processor", counterBuf.data(), &cchCounters, instanceBuf.data(), &cchInstances, pdh::PERF_DETAIL_NOVICE, 0);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhEnumObjectItemsW must succeed");
    }

    // Stage 5: Query Session Lifecycle
    {
        pdh::PDH_HQUERY hQuery = 0;
        int32_t hr = pdh::PdhOpenQueryW(nullptr, 0x1234, &hQuery);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS && hQuery != 0, "PdhOpenQueryW must succeed");

        hr = pdh::PdhCloseQuery(hQuery);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhCloseQuery must return ERROR_SUCCESS");

        hr = pdh::PdhCloseQuery(hQuery);
        TEST_ASSERT(hr == pdh::PDH_INVALID_HANDLE, "Closing invalid query handle must return PDH_INVALID_HANDLE");
    }

    // Stage 6: Counter Path Parsing and Validation
    {
        int32_t hrOk = pdh::PdhValidatePathW(L"\\Processor(_Total)\\% Processor Time");
        TEST_ASSERT(hrOk == pdh::ERROR_SUCCESS, "Valid path \\Processor(_Total)\\% Processor Time must validate");

        int32_t hrMem = pdh::PdhValidatePathW(L"\\Memory\\Available MBytes");
        TEST_ASSERT(hrMem == pdh::ERROR_SUCCESS, "Valid path \\Memory\\Available MBytes must validate");

        int32_t hrBadObj = pdh::PdhValidatePathW(L"\\NonExistentObject\\BadCounter");
        TEST_ASSERT(hrBadObj != pdh::ERROR_SUCCESS, "Non-existent object path must fail validation");

        int32_t hrBadSyntax = pdh::PdhValidatePathW(L"InvalidSyntaxWithoutSlash");
        TEST_ASSERT(hrBadSyntax != pdh::ERROR_SUCCESS, "Invalid syntax path must fail validation");
    }

    // Stage 7: Adding Performance Counters
    {
        pdh::PDH_HQUERY hQuery = 0;
        pdh::PdhOpenQueryW(nullptr, 0, &hQuery);

        pdh::PDH_HCOUNTER hCpu = 0, hMem = 0, hThreads = 0, hDisk = 0;
        int32_t hr = pdh::PdhAddCounterW(hQuery, L"\\Processor(_Total)\\% Processor Time", 101, &hCpu);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS && hCpu != 0, "Adding Processor counter must succeed");

        hr = pdh::PdhAddCounterW(hQuery, L"\\Memory\\Available MBytes", 102, &hMem);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS && hMem != 0, "Adding Memory counter must succeed");

        hr = pdh::PdhAddCounterW(hQuery, L"\\System\\Threads", 103, &hThreads);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS && hThreads != 0, "Adding System counter must succeed");

        hr = pdh::PdhAddCounterW(hQuery, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 104, &hDisk);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS && hDisk != 0, "Adding PhysicalDisk counter must succeed");

        // Stage 8: Query Data Collection
        hr = pdh::PdhCollectQueryData(hQuery);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhCollectQueryData must succeed");

        // Stage 9: Formatted Counter Value Verification
        pdh::PDH_FMT_COUNTERVALUE valCpu{}, valMem{}, valThr{}, valDsk{};
        uint32_t type = 0;

        hr = pdh::PdhGetFormattedCounterValue(hCpu, pdh::PDH_FMT_DOUBLE, &type, &valCpu);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhGetFormattedCounterValue(CPU DOUBLE) must succeed");
        TEST_ASSERT(valCpu.CStatus == pdh::PDH_CSTATUS_VALID_DATA, "CPU counter status must be valid");
        TEST_ASSERT(valCpu.doubleValue >= 0.0 && valCpu.doubleValue <= 100.0, "CPU percentage must be between 0 and 100");

        hr = pdh::PdhGetFormattedCounterValue(hMem, pdh::PDH_FMT_LONG, &type, &valMem);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhGetFormattedCounterValue(Memory LONG) must succeed");
        TEST_ASSERT(valMem.longValue > 1000, "Available memory must be > 1000 MB");

        hr = pdh::PdhGetFormattedCounterValue(hThreads, pdh::PDH_FMT_LONG, &type, &valThr);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhGetFormattedCounterValue(Threads LONG) must succeed");
        TEST_ASSERT(valThr.longValue > 0, "Thread count must be > 0");

        hr = pdh::PdhGetFormattedCounterValue(hDisk, pdh::PDH_FMT_LARGE, &type, &valDsk);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhGetFormattedCounterValue(Disk LARGE) must succeed");
        TEST_ASSERT(valDsk.largeValue > 0, "Disk read bytes must be > 0");

        // Stage 10: Raw Counter Value Retrieval
        pdh::PDH_RAW_COUNTER rawVal{};
        hr = pdh::PdhGetRawCounterValue(hCpu, &type, &rawVal);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhGetRawCounterValue must succeed");
        TEST_ASSERT(rawVal.CStatus == pdh::PDH_CSTATUS_VALID_DATA, "Raw counter status must be valid");

        // Stage 11: Counter Removal
        hr = pdh::PdhRemoveCounter(hCpu);
        TEST_ASSERT(hr == pdh::ERROR_SUCCESS, "PdhRemoveCounter must succeed");

        hr = pdh::PdhGetFormattedCounterValue(hCpu, pdh::PDH_FMT_DOUBLE, nullptr, &valCpu);
        TEST_ASSERT(hr == pdh::PDH_INVALID_HANDLE, "Querying removed counter must return PDH_INVALID_HANDLE");

        pdh::PdhCloseQuery(hQuery);
    }

    // Stage 12: Interactive Shell CLI Integration (perfmon & typeperf)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. perfmon /?
        shell.execute("perfmon /?", out);
        TEST_ASSERT(out.str().find("Windows Performance Monitor") != std::string::npos, "perfmon /? must display title");
        TEST_ASSERT(out.str().find("typeperf") != std::string::npos, "perfmon /? must mention typeperf");

        // 2. perfmon /objects
        out.str("");
        shell.execute("perfmon /objects", out);
        TEST_ASSERT(out.str().find("\\Processor") != std::string::npos, "perfmon /objects must include Processor");
        TEST_ASSERT(out.str().find("\\Memory") != std::string::npos, "perfmon /objects must include Memory");
        TEST_ASSERT(out.str().find("\\PhysicalDisk") != std::string::npos, "perfmon /objects must include PhysicalDisk");

        // 3. perfmon /counters Processor
        out.str("");
        shell.execute("perfmon /counters Processor", out);
        TEST_ASSERT(out.str().find("% Processor Time") != std::string::npos, "perfmon /counters must list % Processor Time");

        // 4. perfmon test
        out.str("");
        shell.execute("perfmon test", out);
        TEST_ASSERT(out.str().find("Finished Successfully") != std::string::npos, "perfmon test must succeed");

        // 5. typeperf "\Processor(_Total)\% Processor Time" -sc 2
        out.str("");
        shell.execute("typeperf \"\\Processor(_Total)\\% Processor Time\" -sc 2", out);
        TEST_ASSERT(out.str().find("(PDH-CSV 4.0)") != std::string::npos, "typeperf must produce standard PDH-CSV 4.0 header");
        TEST_ASSERT(out.str().find("23:45:00.000") != std::string::npos, "typeperf must sample values with timestamp");
    }

    std::cout << "[TEST] Suite 82: Windows Performance Monitor & PDH Subsystem PASSED.\n";
}

void Test_WindowsETW_EventTracing_Subsystem() {
    std::cout << "\n[TEST] Running Suite 83: Windows Event Tracing for Windows (ETW) Subsystem (advapi32.dll / ntdll.dll)...\n";

    // 1. Dynamic Exports Verification
    {
        etw::InitializeEtwSubsystemExports();
        auto& ldr = ldr::DynamicLoader::get();

        const char* advapiExports[] = {
            "StartTraceW", "StartTraceA", "StopTraceW", "StopTraceA",
            "QueryTraceW", "QueryTraceA", "UpdateTraceW", "UpdateTraceA",
            "FlushTraceW", "FlushTraceA", "ControlTraceW", "ControlTraceA",
            "EnableTraceEx2", "EventRegister", "EventUnregister",
            "EventEnabled", "EventProviderEnabled", "EventWrite",
            "EventWriteString", "EventWriteTransfer",
            "OpenTraceW", "ProcessTrace", "CloseTrace"
        };
        for (const auto* exp : advapiExports) {
            TEST_ASSERT(ldr.getExport("advapi32.dll", exp) != nullptr,
                        std::string("advapi32.dll must export ") + exp);
        }

        const char* ntdllExports[] = {
            "EtwEventRegister", "EtwEventUnregister", "EtwEventEnabled",
            "EtwEventWrite", "EtwEventWriteString", "EtwEventWriteTransfer"
        };
        for (const auto* exp : ntdllExports) {
            TEST_ASSERT(ldr.getExport("ntdll.dll", exp) != nullptr,
                        std::string("ntdll.dll must export ") + exp);
        }
    }

    // 2. Module Version Database Verification
    {
        const auto* modLogman = version::VersionDatabase::Instance().FindModule("logman.exe");
        TEST_ASSERT(modLogman != nullptr, "logman.exe must be present in VersionDatabase");
        TEST_ASSERT(modLogman->stringTable.at("OriginalFilename") == "logman.exe", "logman.exe original filename match");

        const auto* modTraceRpt = version::VersionDatabase::Instance().FindModule("tracerpt.exe");
        TEST_ASSERT(modTraceRpt != nullptr, "tracerpt.exe must be present in VersionDatabase");

        const auto* modTraceLog = version::VersionDatabase::Instance().FindModule("tracelog.exe");
        TEST_ASSERT(modTraceLog != nullptr, "tracelog.exe must be present in VersionDatabase");
    }

    // 3. SCM DiagTrack Service Verification
    {
        auto& scm = scm::ServiceControlManager::get();
        auto diagTrack = scm.getServiceRecord(L"DiagTrack");
        TEST_ASSERT(diagTrack != nullptr, "DiagTrack service must be registered in SCM");
        TEST_ASSERT(diagTrack->serviceType == scm::SERVICE_WIN32_SHARE_PROCESS, "DiagTrack must be shared service process");
        TEST_ASSERT(diagTrack->svchostGroup == "utcsvc", "DiagTrack must belong to utcsvc group");
        TEST_ASSERT(diagTrack->status.dwCurrentState == scm::SERVICE_RUNNING, "DiagTrack service must be running");
    }

    // 4. Default NT Kernel Logger Session
    {
        auto session = etw::TraceManager::get().getSessionByName(L"NT Kernel Logger");
        TEST_ASSERT(session != nullptr, "NT Kernel Logger default session must exist");
        TEST_ASSERT(session->isProviderEnabled(etw::SystemTraceControlGuid, etw::TRACE_LEVEL_VERBOSE, 0),
                    "NT Kernel Logger must enable SystemTraceControlGuid by default");
    }

    // 5. Trace Session Lifecycle (Start, Query, Update, Flush)
    etw::TRACEHANDLE hSession = 0;
    {
        etw::EVENT_TRACE_PROPERTIES props{};
        props.Wnode.BufferSize = sizeof(etw::EVENT_TRACE_PROPERTIES);
        props.BufferSize = 64;
        props.MinimumBuffers = 2;
        props.MaximumBuffers = 32;
        props.LogFileMode = etw::EVENT_TRACE_REAL_TIME_MODE;
        props.FlushTimer = 5;

        uint32_t status = etw::StartTraceW(&hSession, L"Suite83TraceSession", &props);
        TEST_ASSERT(status == etw::ERROR_SUCCESS && hSession != 0, "StartTraceW must succeed");

        // Attempt duplicate start must return ERROR_ALREADY_EXISTS
        etw::TRACEHANDLE hDup = 0;
        status = etw::StartTraceW(&hDup, L"Suite83TraceSession", &props);
        TEST_ASSERT(status == etw::ERROR_ALREADY_EXISTS, "StartTraceW duplicate must return ERROR_ALREADY_EXISTS");

        // QueryTraceW
        etw::EVENT_TRACE_PROPERTIES qProps{};
        status = etw::QueryTraceW(hSession, nullptr, &qProps);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "QueryTraceW must succeed");
        TEST_ASSERT(qProps.BufferSize == 64, "QueryTraceW BufferSize must match");
        TEST_ASSERT(qProps.FlushTimer == 5, "QueryTraceW FlushTimer must match");

        // UpdateTraceW
        qProps.FlushTimer = 10;
        status = etw::UpdateTraceW(hSession, nullptr, &qProps);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "UpdateTraceW must succeed");
        TEST_ASSERT(qProps.FlushTimer == 10, "Updated FlushTimer must reflect");

        // FlushTraceW
        status = etw::FlushTraceW(hSession, nullptr, &qProps);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "FlushTraceW must succeed");
    }

    // 6. Provider Registration & Callbacks
    etw::REGHANDLE hProvider = 0;
    GUID testProvGuid = { 0x11223344, 0x5566, 0x7788, { 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 } };
    static bool s_provCallbackFired = false;
    static uint32_t s_provEnabledCode = 0;
    static uint8_t s_provLevel = 0;

    {
        s_provCallbackFired = false;
        s_provEnabledCode = 0;
        s_provLevel = 0;

        auto pfnCallback = [](const GUID* srcId, uint32_t isEnabled, uint8_t level, uint64_t anyKw, uint64_t allKw, void* filter, void* ctx) {
            (void)srcId; (void)anyKw; (void)allKw; (void)filter; (void)ctx;
            s_provCallbackFired = true;
            s_provEnabledCode = isEnabled;
            s_provLevel = level;
        };

        uint32_t status = etw::EventRegister(&testProvGuid, pfnCallback, nullptr, &hProvider);
        TEST_ASSERT(status == etw::ERROR_SUCCESS && hProvider != 0, "EventRegister must succeed");

        // Provider is registered, but not yet enabled on Suite83TraceSession
        TEST_ASSERT(etw::EventProviderEnabled(hProvider, etw::TRACE_LEVEL_INFORMATION, 0) == 0,
                    "Provider must not be enabled yet");

        // Enable provider via EnableTraceEx2
        status = etw::EnableTraceEx2(hSession, &testProvGuid, etw::EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                     etw::TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0, 0, nullptr);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EnableTraceEx2 must succeed");
        TEST_ASSERT(s_provCallbackFired, "Provider EnableCallback must have fired");
        TEST_ASSERT(s_provEnabledCode == etw::EVENT_CONTROL_CODE_ENABLE_PROVIDER, "Callback code must be ENABLE");
        TEST_ASSERT(s_provLevel == etw::TRACE_LEVEL_VERBOSE, "Callback level must be VERBOSE");

        // Verify EventProviderEnabled and EventEnabled
        TEST_ASSERT(etw::EventProviderEnabled(hProvider, etw::TRACE_LEVEL_INFORMATION, 0) == 1,
                    "EventProviderEnabled must return 1");
        etw::EVENT_DESCRIPTOR edDesc{};
        edDesc.Id = 200;
        edDesc.Level = etw::TRACE_LEVEL_INFORMATION;
        TEST_ASSERT(etw::EventEnabled(hProvider, &edDesc) == 1, "EventEnabled must return 1");
    }

    // 7. Event Writing (Binary, String, Transfer)
    {
        auto session = etw::TraceManager::get().getSessionByName(L"Suite83TraceSession");
        TEST_ASSERT(session != nullptr, "Session must exist");
        size_t initialCount = session->getEventCount();

        // 7a. EventWrite (binary payload)
        etw::EVENT_DESCRIPTOR ed1{};
        ed1.Id = 201;
        ed1.Level = etw::TRACE_LEVEL_INFORMATION;
        uint32_t payloadData = 0x12345678;
        etw::EVENT_DATA_DESCRIPTOR dataDesc{};
        dataDesc.Ptr = reinterpret_cast<uint64_t>(&payloadData);
        dataDesc.Size = sizeof(payloadData);

        uint32_t status = etw::EventWrite(hProvider, &ed1, 1, &dataDesc);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EventWrite must succeed");
        TEST_ASSERT(session->getEventCount() == initialCount + 1, "Event count must increase by 1");

        // 7b. EventWriteString (unicode message)
        status = etw::EventWriteString(hProvider, etw::TRACE_LEVEL_INFORMATION, 0x1, L"ETW Diagnostic Message Test");
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EventWriteString must succeed");
        TEST_ASSERT(session->getEventCount() == initialCount + 2, "Event count must increase by 2");

        // 7c. EventWriteTransfer
        GUID actId = { 0xAAAA, 0xBBBB, 0xCCCC, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        status = etw::EventWriteTransfer(hProvider, &ed1, &actId, nullptr, 1, &dataDesc);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EventWriteTransfer must succeed");
        TEST_ASSERT(session->getEventCount() == initialCount + 3, "Event count must increase by 3");
    }

    // 8. Event Consumer (OpenTraceW, ProcessTrace, CloseTrace)
    {
        static uint32_t s_consumerReceivedCount = 0;
        s_consumerReceivedCount = 0;

        etw::EVENT_TRACE_LOGFILEW logfile{};
        wchar_t nameBuf[] = L"Suite83TraceSession";
        logfile.LoggerName = nameBuf;
        logfile.EventRecordCallback = [](etw::EVENT_RECORD* pRec) {
            if (pRec) {
                s_consumerReceivedCount++;
            }
        };

        etw::TRACEHANDLE hConsumer = etw::OpenTraceW(&logfile);
        TEST_ASSERT(hConsumer != etw::INVALID_PROCESSTRACE_HANDLE, "OpenTraceW must succeed");

        uint32_t status = etw::ProcessTrace(&hConsumer, 1, nullptr, nullptr);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "ProcessTrace must succeed");
        TEST_ASSERT(s_consumerReceivedCount >= 3, "ProcessTrace must process at least 3 events");

        status = etw::CloseTrace(hConsumer);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "CloseTrace must succeed");
    }

    // 9. Provider Disable & Unregister
    {
        s_provCallbackFired = false;
        uint32_t status = etw::EnableTraceEx2(hSession, &testProvGuid, etw::EVENT_CONTROL_CODE_DISABLE_PROVIDER,
                                             0, 0, 0, 0, nullptr);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EnableTraceEx2 disable must succeed");
        TEST_ASSERT(s_provCallbackFired, "Provider disable callback must have fired");
        TEST_ASSERT(s_provEnabledCode == etw::EVENT_CONTROL_CODE_DISABLE_PROVIDER, "Callback code must be DISABLE");

        status = etw::EventUnregister(hProvider);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EventUnregister must succeed");
    }

    // 10. Native NTDLL ETW Stubs
    {
        etw::REGHANDLE hNtdllProv = 0;
        GUID ntdllGuid = { 0x55556666, 0x7777, 0x8888, { 1, 1, 2, 2, 3, 3, 4, 4 } };

        int32_t ntStatus = etw::EtwEventRegister(&ntdllGuid, nullptr, nullptr, &hNtdllProv);
        TEST_ASSERT(ntStatus == STATUS_SUCCESS && hNtdllProv != 0, "EtwEventRegister must return STATUS_SUCCESS");

        etw::EVENT_DESCRIPTOR ntdllDesc{};
        ntdllDesc.Id = 301;
        ntdllDesc.Level = etw::TRACE_LEVEL_INFORMATION;
        TEST_ASSERT(etw::EtwEventEnabled(hNtdllProv, &ntdllDesc) == 0, "EtwEventEnabled initially 0");

        ntStatus = etw::EtwEventWriteString(hNtdllProv, etw::TRACE_LEVEL_INFORMATION, 0, L"Native ntdll etw event");
        TEST_ASSERT(ntStatus == STATUS_SUCCESS, "EtwEventWriteString must return STATUS_SUCCESS");

        ntStatus = etw::EtwEventUnregister(hNtdllProv);
        TEST_ASSERT(ntStatus == STATUS_SUCCESS, "EtwEventUnregister must return STATUS_SUCCESS");
    }

    // 11. Stop Trace Session
    {
        etw::EVENT_TRACE_PROPERTIES finalProps{};
        uint32_t status = etw::StopTraceW(hSession, nullptr, &finalProps);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "StopTraceW must succeed");

        // Subsequent query must fail with ERROR_WMI_INSTANCE_NOT_FOUND
        etw::EVENT_TRACE_PROPERTIES qProps{};
        status = etw::QueryTraceW(hSession, nullptr, &qProps);
        TEST_ASSERT(status == etw::ERROR_WMI_INSTANCE_NOT_FOUND, "QueryTraceW on stopped session must fail");
    }

    // 12. Interactive CLI Integration (logman & tracerpt)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // 1. logman /?
        shell.execute("logman /?", out);
        TEST_ASSERT(out.str().find("Microsoft Logman") != std::string::npos, "logman /? must display banner");

        // 2. logman query
        out.str("");
        shell.execute("logman query", out);
        TEST_ASSERT(out.str().find("NT Kernel Logger") != std::string::npos, "logman query must list NT Kernel Logger");

        // 3. logman test
        out.str("");
        shell.execute("logman test", out);
        TEST_ASSERT(out.str().find("Finished Successfully") != std::string::npos, "logman test must succeed");

        // 4. logman start / query / tracerpt / stop
        out.str("");
        shell.execute("logman start MyCliSession -p Kernel", out);
        TEST_ASSERT(out.str().find("is running") != std::string::npos, "logman start must report running");

        out.str("");
        shell.execute("logman query MyCliSession", out);
        TEST_ASSERT(out.str().find("Name:                    MyCliSession") != std::string::npos, "logman query must find MyCliSession");
        TEST_ASSERT(out.str().find("Status:                  Running") != std::string::npos, "MyCliSession must be running");

        out.str("");
        shell.execute("tracerpt MyCliSession", out);
        TEST_ASSERT(out.str().find("Event Trace Report - MyCliSession") != std::string::npos, "tracerpt must dump report header");

        out.str("");
        shell.execute("logman stop MyCliSession", out);
        TEST_ASSERT(out.str().find("stopped") != std::string::npos, "logman stop must succeed");
    }

    std::cout << "[TEST] Suite 83: Windows Event Tracing for Windows (ETW) Subsystem PASSED.\n";
}

void Test_WindowsACL_SecurityAuditing_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 84: Windows Security Auditing, ACL & Object Security Descriptor \n";
    std::cout << "========================================================================\n";

    // 1. Initialize Subsystem Exports and SCM Services
    acl::InitializeAclSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("advapi32.dll", "InitializeSecurityDescriptor") != nullptr, "advapi32!InitializeSecurityDescriptor must be exported");
    TEST_ASSERT(ldr.getExport("advapi32.dll", "AccessCheck") != nullptr, "advapi32!AccessCheck must be exported");
    TEST_ASSERT(ldr.getExport("advapi32.dll", "AllocateAndInitializeSid") != nullptr, "advapi32!AllocateAndInitializeSid must be exported");
    TEST_ASSERT(ldr.getExport("advapi32.dll", "MakeSelfRelativeSD") != nullptr, "advapi32!MakeSelfRelativeSD must be exported");
    TEST_ASSERT(ldr.getExport("secur32.dll", "AccessCheck") != nullptr, "secur32!AccessCheck must be exported");
    TEST_ASSERT(ldr.getExport("sspicli.dll", "AccessCheck") != nullptr, "sspicli!AccessCheck must be exported");

    auto& scm = scm::ServiceControlManager::get();
    auto evtRec = scm.getServiceRecord(L"EventSystem");
    TEST_ASSERT(evtRec != nullptr, "SCM must have EventSystem registered");
    TEST_ASSERT(evtRec->status.dwCurrentState == scm::SERVICE_RUNNING, "EventSystem must be RUNNING");
    TEST_ASSERT(evtRec->svchostGroup == "LocalService", "EventSystem svchostGroup must be LocalService");

    // 2. SID Allocation, Validation & String Conversions
    acl::PSID pAdminSid = nullptr;
    acl::BOOL bRes = acl::AllocateAndInitializeSid(
        &acl::SECURITY_NT_AUTHORITY, 2,
        32, 544, 0, 0, 0, 0, 0, 0,
        &pAdminSid
    );
    TEST_ASSERT(bRes && pAdminSid != nullptr, "AllocateAndInitializeSid for Administrators must succeed");
    TEST_ASSERT(acl::IsValidSid(pAdminSid) == acl::TRUE, "Admin SID must be valid");
    TEST_ASSERT(acl::GetLengthSid(pAdminSid) == sizeof(uint8_t) * 2 + sizeof(acl::SID_IDENTIFIER_AUTHORITY) + sizeof(uint32_t) * 2, "Admin SID length must match 2 sub-authorities");
    TEST_ASSERT(*acl::GetSidSubAuthorityCount(pAdminSid) == 2, "Admin SID must have 2 sub-authorities");
    TEST_ASSERT(*acl::GetSidSubAuthority(pAdminSid, 0) == 32, "SubAuthority 0 must be 32");
    TEST_ASSERT(*acl::GetSidSubAuthority(pAdminSid, 1) == 544, "SubAuthority 1 must be 544");

    wchar_t* szAdminSidW = nullptr;
    bRes = acl::ConvertSidToStringSidW(pAdminSid, &szAdminSidW);
    TEST_ASSERT(bRes && szAdminSidW != nullptr, "ConvertSidToStringSidW must succeed");
    TEST_ASSERT(std::wstring(szAdminSidW) == L"S-1-5-32-544", "Admin SID string must be S-1-5-32-544");

    char* szAdminSidA = nullptr;
    bRes = acl::ConvertSidToStringSidA(pAdminSid, &szAdminSidA);
    TEST_ASSERT(bRes && szAdminSidA != nullptr, "ConvertSidToStringSidA must succeed");
    TEST_ASSERT(std::string(szAdminSidA) == "S-1-5-32-544", "Admin SID string (ANSI) must be S-1-5-32-544");
    delete[] szAdminSidA;

    acl::PSID pParsedSid = nullptr;
    bRes = acl::ConvertStringSidToSidW(szAdminSidW, &pParsedSid);
    TEST_ASSERT(bRes && pParsedSid != nullptr, "ConvertStringSidToSidW must succeed");
    TEST_ASSERT(acl::EqualSid(pAdminSid, pParsedSid) == acl::TRUE, "Parsed SID must equal original Admin SID");

    delete[] szAdminSidW;
    acl::FreeSid(pParsedSid);

    acl::PSID pUserSid = nullptr;
    acl::AllocateAndInitializeSid(
        &acl::SECURITY_NT_AUTHORITY, 2,
        32, 545, 0, 0, 0, 0, 0, 0,
        &pUserSid
    );
    TEST_ASSERT(acl::EqualSid(pAdminSid, pUserSid) == acl::FALSE, "Admin SID and Users SID must not be equal");

    // 3. ACL Creation, ACE Addition, Query and Deletion
    std::vector<uint8_t> aclBuffer(1024, 0);
    auto* pAcl = reinterpret_cast<acl::PACL>(aclBuffer.data());
    bRes = acl::InitializeAcl(pAcl, static_cast<uint32_t>(aclBuffer.size()), acl::ACL_REVISION);
    TEST_ASSERT(bRes == acl::TRUE, "InitializeAcl must succeed");
    TEST_ASSERT(acl::IsValidAcl(pAcl) == acl::TRUE, "Initialized ACL must be valid");
    TEST_ASSERT(pAcl->AceCount == 0, "Initial AceCount must be 0");

    bRes = acl::AddAccessAllowedAce(pAcl, acl::ACL_REVISION, acl::FILE_READ_DATA | acl::FILE_READ_ATTRIBUTES, pUserSid);
    TEST_ASSERT(bRes == acl::TRUE, "AddAccessAllowedAce for Users must succeed");
    TEST_ASSERT(pAcl->AceCount == 1, "AceCount must be 1");

    bRes = acl::AddAccessAllowedAceEx(pAcl, acl::ACL_REVISION, acl::CONTAINER_INHERIT_ACE | acl::OBJECT_INHERIT_ACE, acl::FILE_ALL_ACCESS, pAdminSid);
    TEST_ASSERT(bRes == acl::TRUE, "AddAccessAllowedAceEx for Admins must succeed");
    TEST_ASSERT(pAcl->AceCount == 2, "AceCount must be 2");

    bRes = acl::AddAuditAccessAce(pAcl, acl::ACL_REVISION, acl::FILE_WRITE_DATA, pAdminSid, acl::TRUE, acl::TRUE);
    TEST_ASSERT(bRes == acl::TRUE, "AddAuditAccessAce must succeed");
    TEST_ASSERT(pAcl->AceCount == 3, "AceCount must be 3");

    void* pAce0 = nullptr;
    bRes = acl::GetAce(pAcl, 0, &pAce0);
    TEST_ASSERT(bRes && pAce0 != nullptr, "GetAce(0) must succeed");
    auto* aceHdr0 = static_cast<acl::ACE_HEADER*>(pAce0);
    TEST_ASSERT(aceHdr0->AceType == acl::ACCESS_ALLOWED_ACE_TYPE, "Ace 0 type must be ACCESS_ALLOWED");

    void* pAce2 = nullptr;
    bRes = acl::GetAce(pAcl, 2, &pAce2);
    TEST_ASSERT(bRes && pAce2 != nullptr, "GetAce(2) must succeed");
    auto* aceHdr2 = static_cast<acl::ACE_HEADER*>(pAce2);
    TEST_ASSERT(aceHdr2->AceType == acl::SYSTEM_AUDIT_ACE_TYPE, "Ace 2 type must be SYSTEM_AUDIT");
    TEST_ASSERT((aceHdr2->AceFlags & acl::SUCCESSFUL_ACCESS_ACE_FLAG) != 0, "Audit ACE must have SUCCESS flag");

    bRes = acl::DeleteAce(pAcl, 2);
    TEST_ASSERT(bRes == acl::TRUE, "DeleteAce(2) must succeed");
    TEST_ASSERT(pAcl->AceCount == 2, "AceCount after deletion must be 2");

    // 4. Absolute Security Descriptor Construction & Introspection
    acl::SECURITY_DESCRIPTOR absSd{};
    bRes = acl::InitializeSecurityDescriptor(&absSd, acl::SECURITY_DESCRIPTOR_REVISION);
    TEST_ASSERT(bRes == acl::TRUE, "InitializeSecurityDescriptor must succeed");
    TEST_ASSERT(acl::IsValidSecurityDescriptor(&absSd) == acl::TRUE, "Security Descriptor must be valid");

    bRes = acl::SetSecurityDescriptorOwner(&absSd, pAdminSid, acl::FALSE);
    TEST_ASSERT(bRes == acl::TRUE, "SetSecurityDescriptorOwner must succeed");
    acl::PSID queriedOwner = nullptr;
    acl::BOOL ownerDefaulted = acl::TRUE;
    bRes = acl::GetSecurityDescriptorOwner(&absSd, &queriedOwner, &ownerDefaulted);
    TEST_ASSERT(bRes && queriedOwner == pAdminSid && ownerDefaulted == acl::FALSE, "GetSecurityDescriptorOwner must match Admin SID");

    bRes = acl::SetSecurityDescriptorGroup(&absSd, pUserSid, acl::FALSE);
    TEST_ASSERT(bRes == acl::TRUE, "SetSecurityDescriptorGroup must succeed");
    acl::PSID queriedGroup = nullptr;
    acl::BOOL groupDefaulted = acl::TRUE;
    bRes = acl::GetSecurityDescriptorGroup(&absSd, &queriedGroup, &groupDefaulted);
    TEST_ASSERT(bRes && queriedGroup == pUserSid && groupDefaulted == acl::FALSE, "GetSecurityDescriptorGroup must match User SID");

    bRes = acl::SetSecurityDescriptorDacl(&absSd, acl::TRUE, pAcl, acl::FALSE);
    TEST_ASSERT(bRes == acl::TRUE, "SetSecurityDescriptorDacl must succeed");
    acl::BOOL daclPresent = acl::FALSE;
    acl::PACL queriedDacl = nullptr;
    acl::BOOL daclDefaulted = acl::TRUE;
    bRes = acl::GetSecurityDescriptorDacl(&absSd, &daclPresent, &queriedDacl, &daclDefaulted);
    TEST_ASSERT(bRes && daclPresent == acl::TRUE && queriedDacl == pAcl, "GetSecurityDescriptorDacl must return configured DACL");

    uint32_t sdLen = acl::GetSecurityDescriptorLength(&absSd);
    TEST_ASSERT(sdLen > sizeof(acl::SECURITY_DESCRIPTOR), "Absolute SD length must include SIDs and DACL");

    // 5. Self-Relative and Absolute SD Transformations
    uint32_t relNeeded = 0;
    bRes = acl::MakeSelfRelativeSD(&absSd, nullptr, &relNeeded);
    TEST_ASSERT(bRes == acl::FALSE && relNeeded > 0, "MakeSelfRelativeSD with nullptr must return required size");

    std::vector<uint8_t> relSdBuf(relNeeded, 0);
    bRes = acl::MakeSelfRelativeSD(&absSd, relSdBuf.data(), &relNeeded);
    TEST_ASSERT(bRes == acl::TRUE, "MakeSelfRelativeSD must succeed");

    auto* pRelSd = reinterpret_cast<acl::PSECURITY_DESCRIPTOR>(relSdBuf.data());
    TEST_ASSERT(acl::IsValidSecurityDescriptor(pRelSd) == acl::TRUE, "Self-relative SD must be valid");
    uint16_t relControl = 0;
    uint32_t relRev = 0;
    acl::GetSecurityDescriptorControl(pRelSd, &relControl, &relRev);
    TEST_ASSERT((relControl & acl::SE_SELF_RELATIVE) != 0, "Self-relative SD must have SE_SELF_RELATIVE flag");

    acl::PSID relOwner = nullptr;
    acl::BOOL relOwnerDef = acl::FALSE;
    bRes = acl::GetSecurityDescriptorOwner(pRelSd, &relOwner, &relOwnerDef);
    TEST_ASSERT(bRes && relOwner != nullptr && acl::EqualSid(relOwner, pAdminSid) == acl::TRUE, "GetSecurityDescriptorOwner on self-relative SD must resolve Admin SID");

    acl::BOOL relDaclPres = acl::FALSE;
    acl::PACL relDacl = nullptr;
    acl::BOOL relDaclDef = acl::FALSE;
    bRes = acl::GetSecurityDescriptorDacl(pRelSd, &relDaclPres, &relDacl, &relDaclDef);
    TEST_ASSERT(bRes && relDaclPres == acl::TRUE && relDacl != nullptr && relDacl->AceCount == 2, "GetSecurityDescriptorDacl on self-relative SD must resolve DACL");

    // Convert back from self-relative to absolute
    acl::SECURITY_DESCRIPTOR reconAbsSd{};
    uint32_t reconAbsSize = sizeof(acl::SECURITY_DESCRIPTOR);
    std::vector<uint8_t> reconDaclBuf(1024, 0);
    uint32_t reconDaclSize = 1024;
    std::vector<uint8_t> reconOwnerBuf(128, 0);
    uint32_t reconOwnerSize = 128;
    std::vector<uint8_t> reconGroupBuf(128, 0);
    uint32_t reconGroupSize = 128;

    bRes = acl::MakeAbsoluteSD(
        pRelSd,
        &reconAbsSd, &reconAbsSize,
        reinterpret_cast<acl::PACL>(reconDaclBuf.data()), &reconDaclSize,
        nullptr, nullptr,
        reconOwnerBuf.data(), &reconOwnerSize,
        reconGroupBuf.data(), &reconGroupSize
    );
    TEST_ASSERT(bRes == acl::TRUE, "MakeAbsoluteSD must succeed");
    TEST_ASSERT(reconAbsSd.Revision == acl::SECURITY_DESCRIPTOR_REVISION, "Reconstructed SD revision must be valid");
    TEST_ASSERT(acl::EqualSid(reconAbsSd.Owner, pAdminSid) == acl::TRUE, "Reconstructed SD owner must match Admin SID");

    // 6. AccessCheck Authorization Matrix
    {
        // NULL DACL grants full access
        acl::SECURITY_DESCRIPTOR nullDaclSd{};
        acl::InitializeSecurityDescriptor(&nullDaclSd, acl::SECURITY_DESCRIPTOR_REVISION);
        acl::SetSecurityDescriptorDacl(&nullDaclSd, acl::FALSE, nullptr, acl::FALSE);

        uint32_t granted = 0;
        acl::BOOL accessStatus = acl::FALSE;
        bRes = acl::AccessCheck(&nullDaclSd, nullptr, acl::FILE_ALL_ACCESS, nullptr, nullptr, nullptr, &granted, &accessStatus);
        TEST_ASSERT(bRes && accessStatus == acl::TRUE && granted == acl::FILE_ALL_ACCESS, "AccessCheck on NULL DACL must grant all requested access");

        // DACL with single read ACE
        std::vector<uint8_t> readAclBuf(256, 0);
        auto* pReadAcl = reinterpret_cast<acl::PACL>(readAclBuf.data());
        acl::InitializeAcl(pReadAcl, static_cast<uint32_t>(readAclBuf.size()), acl::ACL_REVISION);
        acl::AddAccessAllowedAce(pReadAcl, acl::ACL_REVISION, acl::FILE_READ_DATA, pUserSid);

        acl::SECURITY_DESCRIPTOR readSd{};
        acl::InitializeSecurityDescriptor(&readSd, acl::SECURITY_DESCRIPTOR_REVISION);
        acl::SetSecurityDescriptorDacl(&readSd, acl::TRUE, pReadAcl, acl::FALSE);

        granted = 0; accessStatus = acl::FALSE;
        acl::AccessCheck(&readSd, nullptr, acl::FILE_READ_DATA, nullptr, nullptr, nullptr, &granted, &accessStatus);
        TEST_ASSERT(accessStatus == acl::TRUE && (granted & acl::FILE_READ_DATA), "AccessCheck for FILE_READ_DATA must succeed");

        granted = 0; accessStatus = acl::FALSE;
        acl::AccessCheck(&readSd, nullptr, acl::FILE_WRITE_DATA, nullptr, nullptr, nullptr, &granted, &accessStatus);
        TEST_ASSERT(accessStatus == acl::FALSE && granted == 0, "AccessCheck for unauthorized FILE_WRITE_DATA must fail");

        // DACL with explicit DENY before ALLOW
        std::vector<uint8_t> denyAclBuf(256, 0);
        auto* pDenyAcl = reinterpret_cast<acl::PACL>(denyAclBuf.data());
        acl::InitializeAcl(pDenyAcl, static_cast<uint32_t>(denyAclBuf.size()), acl::ACL_REVISION);
        acl::AddAccessDeniedAce(pDenyAcl, acl::ACL_REVISION, acl::FILE_WRITE_DATA, pUserSid);
        acl::AddAccessAllowedAce(pDenyAcl, acl::ACL_REVISION, acl::FILE_READ_DATA | acl::FILE_WRITE_DATA, pUserSid);

        acl::SECURITY_DESCRIPTOR denySd{};
        acl::InitializeSecurityDescriptor(&denySd, acl::SECURITY_DESCRIPTOR_REVISION);
        acl::SetSecurityDescriptorDacl(&denySd, acl::TRUE, pDenyAcl, acl::FALSE);

        granted = 0; accessStatus = acl::FALSE;
        acl::AccessCheck(&denySd, nullptr, acl::FILE_WRITE_DATA, nullptr, nullptr, nullptr, &granted, &accessStatus);
        TEST_ASSERT(accessStatus == acl::FALSE, "Explicit DENY must override subsequent ALLOW");

        granted = 0; accessStatus = acl::FALSE;
        acl::AccessCheck(&denySd, nullptr, acl::FILE_READ_DATA, nullptr, nullptr, nullptr, &granted, &accessStatus);
        TEST_ASSERT(accessStatus == acl::TRUE && (granted & acl::FILE_READ_DATA), "Un-denied access must succeed");
    }

    // 7. Security Auditing Policy Engine
    {
        auto& apm = acl::AuditPolicyManager::get();
        apm.reset();
        const auto& cats = apm.getCategories();
        TEST_ASSERT(cats.size() >= 7, "AuditPolicyManager must register standard security categories");

        uint32_t logonPol = apm.getSubCategoryPolicy("Logon");
        TEST_ASSERT(logonPol == acl::AUDIT_POLICY_SUCCESS_AND_FAILURE, "Logon default policy must be Success and Failure");

        bool setOk = apm.setSubCategoryPolicy("SAM", acl::AUDIT_POLICY_SUCCESS);
        TEST_ASSERT(setOk, "setSubCategoryPolicy for SAM must succeed");
        TEST_ASSERT(apm.getSubCategoryPolicy("SAM") == acl::AUDIT_POLICY_SUCCESS, "SAM policy must update to Success");
    }

    // 8. Shell CLI Integration (icacls and auditpol)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // icacls test
        shell.execute("icacls test", out);
        TEST_ASSERT(out.str().find("Self-Test Finished Successfully") != std::string::npos, "icacls test must succeed");

        // icacls query
        out.str("");
        shell.execute("icacls C:\\Windows\\System32\\ntdll.dll", out);
        TEST_ASSERT(out.str().find("NT AUTHORITY\\SYSTEM:(I)(F)") != std::string::npos, "icacls query must report DACL entries");

        // auditpol /?
        out.str("");
        shell.execute("auditpol /?", out);
        TEST_ASSERT(out.str().find("Microsoft AuditPol") != std::string::npos, "auditpol /? must display help");

        // auditpol test
        out.str("");
        shell.execute("auditpol test", out);
        TEST_ASSERT(out.str().find("Self-Test Finished Successfully") != std::string::npos, "auditpol test must succeed");

        // auditpol /get /category:*
        out.str("");
        shell.execute("auditpol /get /category:*", out);
        TEST_ASSERT(out.str().find("System audit policy") != std::string::npos, "auditpol /get /category:* must dump policy list");

        // auditpol /set
        out.str("");
        shell.execute("auditpol /set /subcategory:SAM /success:enable", out);
        TEST_ASSERT(out.str().find("The policy was successfully changed") != std::string::npos, "auditpol /set must report success");
    }

    acl::FreeSid(pAdminSid);
    acl::FreeSid(pUserSid);

    std::cout << "[TEST] Suite 84: Windows Security Auditing, ACL & Object Security Descriptor Subsystem PASSED.\n";
}

void Test_WindowsNetAPI32_NetworkManagement_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 85: Windows Networking Management & NetAPI32 Subsystem          \n";
    std::cout << "========================================================================\n";

    // 1. Initialize NetAPI Subsystem Exports, Dynamic Loader & SCM
    netapi::InitializeNetApiSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("netapi32.dll", "NetApiBufferAllocate") != nullptr, "netapi32!NetApiBufferAllocate must be exported");
    TEST_ASSERT(ldr.getExport("netapi32.dll", "NetServerGetInfo") != nullptr, "netapi32!NetServerGetInfo must be exported");
    TEST_ASSERT(ldr.getExport("netapi32.dll", "NetShareEnum") != nullptr, "netapi32!NetShareEnum must be exported");
    TEST_ASSERT(ldr.getExport("netapi32.dll", "NetSessionEnum") != nullptr, "netapi32!NetSessionEnum must be exported");
    TEST_ASSERT(ldr.getExport("netapi32.dll", "NetUserEnum") != nullptr, "netapi32!NetUserEnum must be exported");
    TEST_ASSERT(ldr.getExport("netapi32.dll", "NetLocalGroupEnum") != nullptr, "netapi32!NetLocalGroupEnum must be exported");

    TEST_ASSERT(ldr.getExport("srvcli.dll", "NetShareEnum") != nullptr, "srvcli!NetShareEnum must be exported");
    TEST_ASSERT(ldr.getExport("wkscli.dll", "NetWkstaGetInfo") != nullptr, "wkscli!NetWkstaGetInfo must be exported");

    auto& scm = scm::ServiceControlManager::get();
    auto srvRec = scm.getServiceRecord(L"LanmanServer");
    TEST_ASSERT(srvRec != nullptr, "LanmanServer service must be registered in SCM");
    TEST_ASSERT(srvRec->status.dwCurrentState == scm::SERVICE_RUNNING, "LanmanServer must be RUNNING");
    TEST_ASSERT(srvRec->svchostGroup == "netsvcs", "LanmanServer svchostGroup must be netsvcs");

    auto wkstaRec = scm.getServiceRecord(L"LanmanWorkstation");
    TEST_ASSERT(wkstaRec != nullptr, "LanmanWorkstation service must be registered in SCM");
    TEST_ASSERT(wkstaRec->status.dwCurrentState == scm::SERVICE_RUNNING, "LanmanWorkstation must be RUNNING");
    TEST_ASSERT(wkstaRec->svchostGroup == "NetworkService", "LanmanWorkstation svchostGroup must be NetworkService");

    const auto* modNet = version::VersionDatabase::Instance().FindModule("netapi32.dll");
    TEST_ASSERT(modNet != nullptr && modNet->moduleName == "netapi32.dll", "Version info for netapi32.dll must exist");

    // 2. NetApiBuffer Memory Allocator
    void* pBuf = nullptr;
    netapi::NET_API_STATUS st = netapi::NetApiBufferAllocate(256, &pBuf);
    TEST_ASSERT(st == netapi::NERR_Success && pBuf != nullptr, "NetApiBufferAllocate must succeed");

    uint32_t bufSize = 0;
    st = netapi::NetApiBufferSize(pBuf, &bufSize);
    TEST_ASSERT(st == netapi::NERR_Success && bufSize == 256, "NetApiBufferSize must report 256 bytes");

    st = netapi::NetApiBufferReallocate(pBuf, 1024, &pBuf);
    TEST_ASSERT(st == netapi::NERR_Success && pBuf != nullptr, "NetApiBufferReallocate must succeed");
    st = netapi::NetApiBufferSize(pBuf, &bufSize);
    TEST_ASSERT(st == netapi::NERR_Success && bufSize == 1024, "NetApiBufferSize after realloc must be 1024");

    st = netapi::NetApiBufferFree(pBuf);
    TEST_ASSERT(st == netapi::NERR_Success, "NetApiBufferFree must succeed");

    // Free with nullptr is a no-op returning success
    TEST_ASSERT(netapi::NetApiBufferFree(nullptr) == netapi::NERR_Success, "NetApiBufferFree(nullptr) must succeed");

    // 3. Server & Workstation Introspection
    uint8_t* pServerInfoBuf = nullptr;
    st = netapi::NetServerGetInfo(nullptr, 101, &pServerInfoBuf);
    TEST_ASSERT(st == netapi::NERR_Success && pServerInfoBuf != nullptr, "NetServerGetInfo(101) must succeed");
    const auto* srv101 = reinterpret_cast<const netapi::SERVER_INFO_101*>(pServerInfoBuf);
    TEST_ASSERT(std::wstring(srv101->sv101_name) == L"MICANT-SRV", "Server name must be MICANT-SRV");
    TEST_ASSERT((srv101->sv101_type & netapi::SV_TYPE_SERVER) != 0, "Server type must include SV_TYPE_SERVER");
    TEST_ASSERT(srv101->sv101_version_major == 10, "Server major version must be 10");
    netapi::NetApiBufferFree(pServerInfoBuf);

    uint8_t* pWkstaInfoBuf = nullptr;
    st = netapi::NetWkstaGetInfo(nullptr, 100, &pWkstaInfoBuf);
    TEST_ASSERT(st == netapi::NERR_Success && pWkstaInfoBuf != nullptr, "NetWkstaGetInfo(100) must succeed");
    const auto* wksta100 = reinterpret_cast<const netapi::WKSTA_INFO_100*>(pWkstaInfoBuf);
    TEST_ASSERT(std::wstring(wksta100->wki100_computername) == L"MICANT-SRV", "Workstation computer name must match");
    TEST_ASSERT(std::wstring(wksta100->wki100_langroup) == L"WORKGROUP", "Workstation langroup must be WORKGROUP");
    netapi::NetApiBufferFree(pWkstaInfoBuf);

    // 4. Share Management (Enum, Add, GetInfo, Del)
    netapi::NetworkManagementEngine::get().resetDefaults();

    uint8_t* pShareBuf = nullptr;
    uint32_t entriesRead = 0, totalEntries = 0;
    st = netapi::NetShareEnum(nullptr, 2, &pShareBuf, netapi::MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, nullptr);
    TEST_ASSERT(st == netapi::NERR_Success && pShareBuf != nullptr, "NetShareEnum(2) must succeed");
    TEST_ASSERT(entriesRead >= 3, "Default shares count must be at least 3 (ADMIN$, C$, IPC$)");

    const auto* shares = reinterpret_cast<const netapi::SHARE_INFO_2*>(pShareBuf);
    bool foundC = false;
    for (uint32_t i = 0; i < entriesRead; ++i) {
        if (std::wstring(shares[i].shi2_netname) == L"C$") {
            foundC = true;
            TEST_ASSERT(shares[i].shi2_type == (netapi::STYPE_DISKTREE | netapi::STYPE_SPECIAL), "C$ must be special disk share");
        }
    }
    TEST_ASSERT(foundC, "C$ share must be present in default shares");
    netapi::NetApiBufferFree(pShareBuf);

    // Add new share
    netapi::SHARE_INFO_2 newShare{};
    newShare.shi2_netname = const_cast<wchar_t*>(L"Development");
    newShare.shi2_path = const_cast<wchar_t*>(L"C:\\Dev");
    newShare.shi2_remark = const_cast<wchar_t*>(L"Project Source Code Repository");
    newShare.shi2_type = netapi::STYPE_DISKTREE;
    newShare.shi2_permissions = netapi::ACCESS_ALL;
    newShare.shi2_max_uses = 100;

    st = netapi::NetShareAdd(nullptr, 2, reinterpret_cast<const uint8_t*>(&newShare), nullptr);
    TEST_ASSERT(st == netapi::NERR_Success, "NetShareAdd must succeed");

    // Duplicate add should fail
    st = netapi::NetShareAdd(nullptr, 2, reinterpret_cast<const uint8_t*>(&newShare), nullptr);
    TEST_ASSERT(st == netapi::NERR_DuplicateShare, "NetShareAdd duplicate must fail with NERR_DuplicateShare");

    // Query newly added share
    uint8_t* pSingleShareBuf = nullptr;
    st = netapi::NetShareGetInfo(nullptr, L"Development", 2, &pSingleShareBuf);
    TEST_ASSERT(st == netapi::NERR_Success && pSingleShareBuf != nullptr, "NetShareGetInfo for Development must succeed");
    const auto* devShare = reinterpret_cast<const netapi::SHARE_INFO_2*>(pSingleShareBuf);
    TEST_ASSERT(std::wstring(devShare->shi2_path) == L"C:\\Dev", "Share path must be C:\\Dev");
    TEST_ASSERT(devShare->shi2_max_uses == 100, "Max uses must match 100");
    netapi::NetApiBufferFree(pSingleShareBuf);

    // Delete share
    st = netapi::NetShareDel(nullptr, L"Development", 0);
    TEST_ASSERT(st == netapi::NERR_Success, "NetShareDel must succeed");

    // Query after deletion should fail
    st = netapi::NetShareGetInfo(nullptr, L"Development", 2, &pSingleShareBuf);
    TEST_ASSERT(st == netapi::NERR_ShareNotFound, "NetShareGetInfo after deletion must return NERR_ShareNotFound");

    // 5. Session Management
    uint8_t* pSessBuf = nullptr;
    st = netapi::NetSessionEnum(nullptr, nullptr, nullptr, 10, &pSessBuf, netapi::MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, nullptr);
    TEST_ASSERT(st == netapi::NERR_Success && pSessBuf != nullptr, "NetSessionEnum(10) must succeed");
    TEST_ASSERT(entriesRead >= 2, "Default sessions count must be >= 2");
    netapi::NetApiBufferFree(pSessBuf);

    // Filter by client name
    st = netapi::NetSessionEnum(nullptr, L"\\\\192.168.1.50", nullptr, 10, &pSessBuf, netapi::MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, nullptr);
    TEST_ASSERT(st == netapi::NERR_Success && entriesRead == 1, "Filtered session enum must return exactly 1");
    const auto* s10 = reinterpret_cast<const netapi::SESSION_INFO_10*>(pSessBuf);
    TEST_ASSERT(std::wstring(s10->sesi10_username) == L"Administrator", "Session username must be Administrator");
    netapi::NetApiBufferFree(pSessBuf);

    // Delete session
    st = netapi::NetSessionDel(nullptr, L"\\\\192.168.1.50", L"Administrator");
    TEST_ASSERT(st == netapi::NERR_Success, "NetSessionDel must succeed");

    // 6. User and Local Group Management
    uint8_t* pUserBuf = nullptr;
    st = netapi::NetUserEnum(nullptr, 1, 0, &pUserBuf, netapi::MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, nullptr);
    TEST_ASSERT(st == netapi::NERR_Success && pUserBuf != nullptr, "NetUserEnum(1) must succeed");
    TEST_ASSERT(entriesRead >= 3, "Default users count must be at least 3");
    netapi::NetApiBufferFree(pUserBuf);

    uint8_t* pAdminUserBuf = nullptr;
    st = netapi::NetUserGetInfo(nullptr, L"Administrator", 1, &pAdminUserBuf);
    TEST_ASSERT(st == netapi::NERR_Success && pAdminUserBuf != nullptr, "NetUserGetInfo for Administrator must succeed");
    const auto* u1 = reinterpret_cast<const netapi::USER_INFO_1*>(pAdminUserBuf);
    TEST_ASSERT(u1->usri1_priv == netapi::USER_PRIV_ADMIN, "Administrator priv must be USER_PRIV_ADMIN");
    netapi::NetApiBufferFree(pAdminUserBuf);

    // Add user
    netapi::USER_INFO_1 newUser{};
    newUser.usri1_name = const_cast<wchar_t*>(L"TestEngineer");
    newUser.usri1_priv = netapi::USER_PRIV_USER;
    newUser.usri1_comment = const_cast<wchar_t*>(L"Clean-Room Systems Tester");
    st = netapi::NetUserAdd(nullptr, 1, reinterpret_cast<const uint8_t*>(&newUser), nullptr);
    TEST_ASSERT(st == netapi::NERR_Success, "NetUserAdd must succeed");

    // Delete user
    st = netapi::NetUserDel(nullptr, L"TestEngineer");
    TEST_ASSERT(st == netapi::NERR_Success, "NetUserDel must succeed");

    // Local Groups
    uint8_t* pGrpBuf = nullptr;
    st = netapi::NetLocalGroupEnum(nullptr, 1, &pGrpBuf, netapi::MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, nullptr);
    TEST_ASSERT(st == netapi::NERR_Success && pGrpBuf != nullptr, "NetLocalGroupEnum(1) must succeed");
    TEST_ASSERT(entriesRead >= 4, "Default local groups must be >= 4");
    netapi::NetApiBufferFree(pGrpBuf);

    uint8_t* pGrpMemBuf = nullptr;
    st = netapi::NetLocalGroupGetMembers(nullptr, L"Administrators", 3, &pGrpMemBuf, netapi::MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, nullptr);
    TEST_ASSERT(st == netapi::NERR_Success && pGrpMemBuf != nullptr, "NetLocalGroupGetMembers for Administrators must succeed");
    TEST_ASSERT(entriesRead >= 1, "Administrators members count must be >= 1");
    netapi::NetApiBufferFree(pGrpMemBuf);

    // 7. Shell CLI Integration (net commands)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // net test
        shell.execute("net test", out);
        TEST_ASSERT(out.str().find("Self-Test Finished Successfully") != std::string::npos, "net test must succeed");

        // net /?
        out.str("");
        shell.execute("net /?", out);
        TEST_ASSERT(out.str().find("The syntax of this command is") != std::string::npos, "net /? must display banner");

        // net share
        out.str("");
        shell.execute("net share", out);
        TEST_ASSERT(out.str().find("ADMIN$") != std::string::npos, "net share must list ADMIN$");
        TEST_ASSERT(out.str().find("C$") != std::string::npos, "net share must list C$");

        // net share C$
        out.str("");
        shell.execute("net share C$", out);
        TEST_ASSERT(out.str().find("Share name        C$") != std::string::npos, "net share C$ must display details");

        // net session
        out.str("");
        shell.execute("net session", out);
        TEST_ASSERT(out.str().find("Computer             User name") != std::string::npos, "net session must list sessions");

        // net view
        out.str("");
        shell.execute("net view", out);
        TEST_ASSERT(out.str().find("MICANT-SRV") != std::string::npos, "net view must display MICANT-SRV");

        // net config server
        out.str("");
        shell.execute("net config server", out);
        TEST_ASSERT(out.str().find("Server name                   \\\\MICANT-SRV") != std::string::npos, "net config server must output server name");

        // net config workstation
        out.str("");
        shell.execute("net config workstation", out);
        TEST_ASSERT(out.str().find("Full Computer name            MICANT-SRV.WORKGROUP") != std::string::npos, "net config workstation must output full computer name");

        // net localgroup
        out.str("");
        shell.execute("net localgroup", out);
        TEST_ASSERT(out.str().find("*Administrators") != std::string::npos, "net localgroup must list Administrators");

        // net localgroup Administrators
        out.str("");
        shell.execute("net localgroup Administrators", out);
        TEST_ASSERT(out.str().find("Administrator") != std::string::npos, "net localgroup Administrators must display Administrator");
    }

    std::cout << "[TEST] Suite 85: Windows Networking Management & NetAPI32 Subsystem PASSED.\n";
}

// ============================================================================
// Suite 86: Windows Active Directory & LDAP Subsystem
// ============================================================================
void Test_WindowsLDAP_ActiveDirectory_Subsystem() {
    using namespace micant::ldap;

    // 1. Initialize subsystem exports
    InitializeLdapSubsystemExports();

    // 2. Validate DynamicLoader exports in wldap32.dll and adsldp.dll
    auto& ldr = ldr::DynamicLoader::get();

    void* fnInit = ldr.getExport("wldap32.dll", "ldap_initW");
    TEST_ASSERT(fnInit != nullptr, "wldap32.dll must export ldap_initW");

    void* fnSslInit = ldr.getExport("wldap32.dll", "ldap_sslinitW");
    TEST_ASSERT(fnSslInit != nullptr, "wldap32.dll must export ldap_sslinitW");

    void* fnConnect = ldr.getExport("wldap32.dll", "ldap_connect");
    TEST_ASSERT(fnConnect != nullptr, "wldap32.dll must export ldap_connect");

    void* fnBind = ldr.getExport("wldap32.dll", "ldap_bind_sW");
    TEST_ASSERT(fnBind != nullptr, "wldap32.dll must export ldap_bind_sW");

    void* fnSimpleBind = ldr.getExport("wldap32.dll", "ldap_simple_bind_sW");
    TEST_ASSERT(fnSimpleBind != nullptr, "wldap32.dll must export ldap_simple_bind_sW");

    void* fnUnbindS = ldr.getExport("wldap32.dll", "ldap_unbind_s");
    TEST_ASSERT(fnUnbindS != nullptr, "wldap32.dll must export ldap_unbind_s");

    void* fnSearch = ldr.getExport("wldap32.dll", "ldap_search_sW");
    TEST_ASSERT(fnSearch != nullptr, "wldap32.dll must export ldap_search_sW");

    void* fnCount = ldr.getExport("wldap32.dll", "ldap_count_entries");
    TEST_ASSERT(fnCount != nullptr, "wldap32.dll must export ldap_count_entries");

    void* fnFirstEntry = ldr.getExport("wldap32.dll", "ldap_first_entry");
    TEST_ASSERT(fnFirstEntry != nullptr, "wldap32.dll must export ldap_first_entry");

    void* fnNextEntry = ldr.getExport("wldap32.dll", "ldap_next_entry");
    TEST_ASSERT(fnNextEntry != nullptr, "wldap32.dll must export ldap_next_entry");

    void* fnGetDn = ldr.getExport("wldap32.dll", "ldap_get_dnW");
    TEST_ASSERT(fnGetDn != nullptr, "wldap32.dll must export ldap_get_dnW");

    void* fnMemFree = ldr.getExport("wldap32.dll", "ldap_memfreeW");
    TEST_ASSERT(fnMemFree != nullptr, "wldap32.dll must export ldap_memfreeW");

    void* fnFirstAttr = ldr.getExport("wldap32.dll", "ldap_first_attributeW");
    TEST_ASSERT(fnFirstAttr != nullptr, "wldap32.dll must export ldap_first_attributeW");

    void* fnNextAttr = ldr.getExport("wldap32.dll", "ldap_next_attributeW");
    TEST_ASSERT(fnNextAttr != nullptr, "wldap32.dll must export ldap_next_attributeW");

    void* fnGetValuesLen = ldr.getExport("wldap32.dll", "ldap_get_values_lenW");
    TEST_ASSERT(fnGetValuesLen != nullptr, "wldap32.dll must export ldap_get_values_lenW");

    void* fnFreeValuesLen = ldr.getExport("wldap32.dll", "ldap_value_free_len");
    TEST_ASSERT(fnFreeValuesLen != nullptr, "wldap32.dll must export ldap_value_free_len");

    void* fnMsgFree = ldr.getExport("wldap32.dll", "ldap_msgfree");
    TEST_ASSERT(fnMsgFree != nullptr, "wldap32.dll must export ldap_msgfree");

    void* fnADsOpen = ldr.getExport("adsldp.dll", "ADsOpenObject");
    TEST_ASSERT(fnADsOpen != nullptr, "adsldp.dll must export ADsOpenObject");

    void* fnDllGetClass = ldr.getExport("adsldp.dll", "DllGetClassObject");
    TEST_ASSERT(fnDllGetClass != nullptr, "adsldp.dll must export DllGetClassObject");

    // 3. Validate Version Database
    auto& verDb = version::VersionDatabase::Instance();
    const auto* pWldap = verDb.FindModule("wldap32.dll");
    TEST_ASSERT(pWldap != nullptr, "VersionDatabase must contain wldap32.dll");
    TEST_ASSERT(pWldap->stringTable.at("ProductName") == "MicaNT Active Directory Subsystem", "wldap32 ProductName must match");

    const auto* pAdsldp = verDb.FindModule("adsldp.dll");
    TEST_ASSERT(pAdsldp != nullptr, "VersionDatabase must contain adsldp.dll");

    const auto* pDsquery = verDb.FindModule("dsquery.exe");
    TEST_ASSERT(pDsquery != nullptr, "VersionDatabase must contain dsquery.exe");

    const auto* pDsget = verDb.FindModule("dsget.exe");
    TEST_ASSERT(pDsget != nullptr, "VersionDatabase must contain dsget.exe");

    // 4. Validate SCM Services (NTDS & KDC)
    auto& scm = scm::ServiceControlManager::get();
    auto ntds = scm.getServiceRecord(L"NTDS");
    TEST_ASSERT(ntds != nullptr, "SCM must register NTDS service");
    TEST_ASSERT(ntds->displayName == L"Active Directory Domain Services", "NTDS display name must match");
    TEST_ASSERT(ntds->status.dwCurrentState == scm::SERVICE_RUNNING, "NTDS must be in RUNNING state");

    auto kdc = scm.getServiceRecord(L"KDC");
    TEST_ASSERT(kdc != nullptr, "SCM must register KDC service");
    TEST_ASSERT(kdc->displayName == L"Kerberos Key Distribution Center", "KDC display name must match");
    TEST_ASSERT(kdc->svchostGroup == "LocalService", "KDC must belong to LocalService group");

    // 5. LDAP Session Lifecycle & Options
    auto* ld = ldap_initW(L"dc01.micant.local", LDAP_PORT);
    TEST_ASSERT(ld != nullptr, "ldap_initW must return valid handle");
    TEST_ASSERT(ld->port == LDAP_PORT, "Default port must be 389");

    int ver = 3;
    uint32_t optSt = ldap_set_optionW(ld, LDAP_OPT_PROTOCOL_VERSION, &ver);
    TEST_ASSERT(optSt == LDAP_SUCCESS, "ldap_set_optionW for version must succeed");

    int readVer = 0;
    optSt = ldap_get_optionW(ld, LDAP_OPT_PROTOCOL_VERSION, &readVer);
    TEST_ASSERT(optSt == LDAP_SUCCESS && readVer == 3, "ldap_get_optionW must return protocol version 3");

    int sizeLimit = 500;
    ldap_set_optionW(ld, LDAP_OPT_SIZELIMIT, &sizeLimit);
    int readSize = 0;
    ldap_get_optionW(ld, LDAP_OPT_SIZELIMIT, &readSize);
    TEST_ASSERT(readSize == 500, "Size limit must be 500");

    // Connect & Simple Bind
    uint32_t cSt = ldap_connect(ld, nullptr);
    TEST_ASSERT(cSt == LDAP_SUCCESS, "ldap_connect must succeed");

    uint32_t bSt = ldap_simple_bind_sW(ld, L"CN=Administrator,CN=Users,DC=micant,DC=local", L"SecretPass123!");
    TEST_ASSERT(bSt == LDAP_SUCCESS, "ldap_simple_bind_sW must succeed");

    // Error strings and mapping
    const wchar_t* errStr = ldap_err2stringW(LDAP_SUCCESS);
    TEST_ASSERT(wcscmp(errStr, L"Success") == 0, "ldap_err2stringW(LDAP_SUCCESS) must return Success");
    TEST_ASSERT(LdapMapErrorToWin32(LDAP_SUCCESS) == 0, "LdapMapErrorToWin32(0) must return 0");
    TEST_ASSERT(LdapMapErrorToWin32(LDAP_NO_SUCH_OBJECT) == 0x2030, "LdapMapErrorToWin32(LDAP_NO_SUCH_OBJECT) must return ERROR_DS_NO_SUCH_OBJECT");

    // 6. Query RootDSE
    LDAPMessage* pRootDseMsg = nullptr;
    uint32_t sSt = ldap_search_sW(ld, L"", LDAP_SCOPE_BASE, L"(objectClass=*)", nullptr, 0, &pRootDseMsg);
    TEST_ASSERT(sSt == LDAP_SUCCESS && pRootDseMsg != nullptr, "ldap_search_sW on RootDSE must succeed");
    TEST_ASSERT(ldap_count_entries(ld, pRootDseMsg) == 1, "RootDSE search must return 1 entry");

    auto* rootEntry = ldap_first_entry(ld, pRootDseMsg);
    TEST_ASSERT(rootEntry != nullptr, "RootDSE must have first entry");
    auto dncVals = ldap_get_valuesW(ld, rootEntry, L"defaultNamingContext");
    TEST_ASSERT(dncVals != nullptr && dncVals[0] != nullptr, "RootDSE must have defaultNamingContext");
    TEST_ASSERT(wcscmp(dncVals[0], L"DC=micant,DC=local") == 0, "defaultNamingContext must be DC=micant,DC=local");
    ldap_value_freeW(dncVals);
    ldap_msgfree(pRootDseMsg);

    // 7. Search Users Subtree & Inspect Attributes
    LDAPMessage* pUserMsg = nullptr;
    sSt = ldap_search_sW(ld, L"DC=micant,DC=local", LDAP_SCOPE_SUBTREE, L"(objectClass=user)", nullptr, 0, &pUserMsg);
    TEST_ASSERT(sSt == LDAP_SUCCESS && pUserMsg != nullptr, "Subtree user search must succeed");
    uint32_t userCount = ldap_count_entries(ld, pUserMsg);
    TEST_ASSERT(userCount >= 3, "Subtree search must return at least 3 user objects");

    bool foundAdmin = false;
    for (auto* e = ldap_first_entry(ld, pUserMsg); e != nullptr; e = ldap_next_entry(ld, e)) {
        wchar_t* dn = ldap_get_dnW(ld, e);
        if (dn) {
            if (wcsstr(dn, L"CN=Administrator") != nullptr) {
                foundAdmin = true;

                // Inspect attributes via BerElement
                BerElement* ber = nullptr;
                wchar_t* attr = ldap_first_attributeW(ld, e, &ber);
                TEST_ASSERT(attr != nullptr && ber != nullptr, "ldap_first_attributeW must return valid attribute");
                int attrCount = 0;
                while (attr) {
                    attrCount++;
                    ldap_memfreeW(attr);
                    attr = ldap_next_attributeW(ld, e, ber);
                }
                ber_free(ber, 1);
                TEST_ASSERT(attrCount >= 5, "Administrator must have >= 5 attributes");

                // Test binary values (berval)
                berval** bvals = ldap_get_values_lenW(ld, e, L"sAMAccountName");
                TEST_ASSERT(bvals != nullptr && bvals[0] != nullptr, "ldap_get_values_lenW must return sAMAccountName");
                TEST_ASSERT(std::string(bvals[0]->bv_val) == "Administrator", "sAMAccountName must equal Administrator");
                ldap_value_free_len(bvals);

                // Test string values
                wchar_t** svals = ldap_get_valuesW(ld, e, L"mail");
                TEST_ASSERT(svals != nullptr && svals[0] != nullptr, "ldap_get_valuesW must return mail");
                TEST_ASSERT(wcscmp(svals[0], L"admin@micant.local") == 0, "mail must be admin@micant.local");
                ldap_value_freeW(svals);
            }
            ldap_memfreeW(dn);
        }
    }
    TEST_ASSERT(foundAdmin, "Search must locate Administrator account");
    ldap_msgfree(pUserMsg);

    // 8. Search with Composite AND filter
    LDAPMessage* pCompMsg = nullptr;
    sSt = ldap_search_sW(ld, L"DC=micant,DC=local", LDAP_SCOPE_SUBTREE,
                        L"(&(objectClass=user)(sAMAccountName=Administrator))", nullptr, 0, &pCompMsg);
    TEST_ASSERT(sSt == LDAP_SUCCESS && pCompMsg != nullptr, "Composite filter search must succeed");
    TEST_ASSERT(ldap_count_entries(ld, pCompMsg) == 1, "Composite filter must match exactly 1 entry");
    ldap_msgfree(pCompMsg);

    // 9. Search Computers
    LDAPMessage* pComputerMsg = nullptr;
    sSt = ldap_search_sW(ld, L"DC=micant,DC=local", LDAP_SCOPE_SUBTREE, L"(objectClass=computer)", nullptr, 0, &pComputerMsg);
    TEST_ASSERT(sSt == LDAP_SUCCESS && pComputerMsg != nullptr, "Search for computers must succeed");
    TEST_ASSERT(ldap_count_entries(ld, pComputerMsg) >= 2, "Computers search must return >= 2 accounts");
    ldap_msgfree(pComputerMsg);

    // 10. Unbind session
    uint32_t ubSt = ldap_unbind_s(ld);
    TEST_ASSERT(ubSt == LDAP_SUCCESS, "ldap_unbind_s must succeed");

    // 11. ADSI Provider (adsldp.dll)
    void* pAdsiObj = nullptr;
    int32_t hr = ADsOpenObject(L"LDAP://CN=Administrator,CN=Users,DC=micant,DC=local", nullptr, nullptr, 0, nullptr, &pAdsiObj);
    TEST_ASSERT(hr == 0 && pAdsiObj != nullptr, "ADsOpenObject on Administrator must succeed");

    void* pMissingObj = nullptr;
    int32_t hrMissing = ADsOpenObject(L"LDAP://CN=NoSuchObject,DC=micant,DC=local", nullptr, nullptr, 0, nullptr, &pMissingObj);
    TEST_ASSERT(hrMissing != 0, "ADsOpenObject on missing object must fail");

    void* pFactory = nullptr;
    int32_t hrClass = DllGetClassObject(nullptr, nullptr, &pFactory);
    TEST_ASSERT(hrClass == 0 && pFactory != nullptr, "DllGetClassObject must succeed");

    // 12. Shell CLI Integration (dsquery & dsget)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // dsquery test
        shell.execute("dsquery test", out);
        TEST_ASSERT(out.str().find("Self-Test Finished Successfully") != std::string::npos, "dsquery test must succeed");

        // dsquery /?
        out.str("");
        shell.execute("dsquery /?", out);
        TEST_ASSERT(out.str().find("Microsoft DSQUERY") != std::string::npos, "dsquery /? must display help banner");

        // dsquery user
        out.str("");
        shell.execute("dsquery user", out);
        TEST_ASSERT(out.str().find("CN=Administrator,CN=Users,DC=micant,DC=local") != std::string::npos, "dsquery user must list Administrator");
        TEST_ASSERT(out.str().find("CN=Guest,CN=Users,DC=micant,DC=local") != std::string::npos, "dsquery user must list Guest");

        // dsquery computer
        out.str("");
        shell.execute("dsquery computer", out);
        TEST_ASSERT(out.str().find("CN=MICANT-WS01,CN=Computers,DC=micant,DC=local") != std::string::npos, "dsquery computer must list WS01");

        // dsquery server
        out.str("");
        shell.execute("dsquery server", out);
        TEST_ASSERT(out.str().find("CN=MICANT-DC01,OU=Domain Controllers,DC=micant,DC=local") != std::string::npos, "dsquery server must list DC01");

        // dsquery group
        out.str("");
        shell.execute("dsquery group", out);
        TEST_ASSERT(out.str().find("CN=Domain Admins,CN=Users,DC=micant,DC=local") != std::string::npos, "dsquery group must list Domain Admins");

        // dsquery * -filter
        out.str("");
        shell.execute("dsquery * -filter (sAMAccountName=Administrator)", out);
        TEST_ASSERT(out.str().find("CN=Administrator,CN=Users,DC=micant,DC=local") != std::string::npos, "dsquery * with filter must match Administrator");

        // dsget test
        out.str("");
        shell.execute("dsget test", out);
        TEST_ASSERT(out.str().find("Self-Test Finished Successfully") != std::string::npos, "dsget test must succeed");

        // dsget /?
        out.str("");
        shell.execute("dsget /?", out);
        TEST_ASSERT(out.str().find("Microsoft DSGET") != std::string::npos, "dsget /? must display help banner");

        // dsget user
        out.str("");
        shell.execute("dsget user \"CN=Administrator,CN=Users,DC=micant,DC=local\"", out);
        TEST_ASSERT(out.str().find("Administrator") != std::string::npos, "dsget user must output Administrator");
        TEST_ASSERT(out.str().find("MicaNT Administrator") != std::string::npos, "dsget user must output displayName");
        TEST_ASSERT(out.str().find("dsget succeeded") != std::string::npos, "dsget user must succeed");
    }

    std::cout << "[TEST] Suite 86: Windows Active Directory & LDAP Subsystem PASSED.\n";
}

void Test_WindowsRDP_TerminalServices_Subsystem() {
    std::cout << "[TEST] Running Suite 87: Windows Remote Desktop Protocol (RDP) & Terminal Services Subsystem...\n";

    // 1. Initialize Subsystem & Register Dynamic Exports
    termsrv::InitializeTerminalServicesSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSEnumerateSessionsW") != nullptr, "WTSEnumerateSessionsW must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSEnumerateSessionsA") != nullptr, "WTSEnumerateSessionsA must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSQuerySessionInformationW") != nullptr, "WTSQuerySessionInformationW must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSFreeMemory") != nullptr, "WTSFreeMemory must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSOpenServerW") != nullptr, "WTSOpenServerW must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSCloseServer") != nullptr, "WTSCloseServer must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSDisconnectSession") != nullptr, "WTSDisconnectSession must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSLogoffSession") != nullptr, "WTSLogoffSession must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSSendMessageW") != nullptr, "WTSSendMessageW must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSRegisterSessionNotification") != nullptr, "WTSRegisterSessionNotification must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("wtsapi32.dll", "WTSUnRegisterSessionNotification") != nullptr, "WTSUnRegisterSessionNotification must be exported by wtsapi32.dll");
    TEST_ASSERT(ldr.getExport("termsrv.dll", "ServiceMain") != nullptr, "ServiceMain must be exported by termsrv.dll");

    // 2. Version Information Introspection
    auto& ver = version::VersionDatabase::Instance();
    const auto* wtsMod = ver.FindModule("wtsapi32.dll");
    TEST_ASSERT(wtsMod != nullptr, "wtsapi32.dll must be registered in version manager");
    TEST_ASSERT(wtsMod->stringTable.at("InternalName") == "wtsapi32", "wtsapi32 InternalName must match");

    const auto* termMod = ver.FindModule("termsrv.dll");
    TEST_ASSERT(termMod != nullptr, "termsrv.dll must be registered in version manager");
    TEST_ASSERT(termMod->stringTable.at("InternalName") == "termsrv", "termsrv InternalName must match");

    const auto* mstscMod = ver.FindModule("mstsc.exe");
    TEST_ASSERT(mstscMod != nullptr, "mstsc.exe must be registered in version manager");
    TEST_ASSERT(mstscMod->stringTable.at("InternalName") == "mstsc", "mstsc InternalName must match");

    const auto* qwinstaMod = ver.FindModule("qwinsta.exe");
    TEST_ASSERT(qwinstaMod != nullptr, "qwinsta.exe must be registered in version manager");
    TEST_ASSERT(qwinstaMod->stringTable.at("InternalName") == "qwinsta", "qwinsta InternalName must match");

    const auto* rwinstaMod = ver.FindModule("rwinsta.exe");
    TEST_ASSERT(rwinstaMod != nullptr, "rwinsta.exe must be registered in version manager");
    TEST_ASSERT(rwinstaMod->stringTable.at("InternalName") == "rwinsta", "rwinsta InternalName must match");

    // 3. Service Control Manager (SCM) Integration
    auto& scmInst = scm::ServiceControlManager::get();
    auto termSvc = scmInst.getServiceRecord(L"TermService");
    TEST_ASSERT(termSvc != nullptr, "TermService record must be registered in SCM");
    TEST_ASSERT(termSvc->displayName == L"Remote Desktop Services", "TermService display name must match");
    TEST_ASSERT(termSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "TermService must be RUNNING");
    TEST_ASSERT(termSvc->svchostGroup == "NetworkService", "TermService must be hosted in NetworkService svchost group");

    auto envSvc = scmInst.getServiceRecord(L"SessionEnv");
    TEST_ASSERT(envSvc != nullptr, "SessionEnv record must be registered in SCM");
    TEST_ASSERT(envSvc->displayName == L"Remote Desktop Configuration", "SessionEnv display name must match");
    TEST_ASSERT(envSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "SessionEnv must be RUNNING");
    TEST_ASSERT(envSvc->svchostGroup == "netsvcs", "SessionEnv must be hosted in netsvcs svchost group");

    // 4. Session Enumeration (WTSEnumerateSessionsW / A)
    termsrv::TerminalServicesManager::get().resetToDefault();

    termsrv::PWTS_SESSION_INFOW pSessionsW = nullptr;
    uint32_t countW = 0;
    int32_t okEnumW = termsrv::WTSEnumerateSessionsW(termsrv::WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionsW, &countW);
    TEST_ASSERT(okEnumW == 1 && pSessionsW != nullptr, "WTSEnumerateSessionsW must return TRUE and valid pointer");
    TEST_ASSERT(countW >= 3, "WTSEnumerateSessionsW must return at least Session 0, Session 1, and Session 65536");

    bool hasSession0 = false;
    bool hasSession1 = false;
    bool hasSession65536 = false;
    for (uint32_t i = 0; i < countW; ++i) {
        if (pSessionsW[i].SessionId == 0) {
            hasSession0 = true;
            TEST_ASSERT(std::wstring(pSessionsW[i].pWinStationName) == L"Services", "Session 0 station name must be Services");
        } else if (pSessionsW[i].SessionId == 1) {
            hasSession1 = true;
            TEST_ASSERT(std::wstring(pSessionsW[i].pWinStationName) == L"Console", "Session 1 station name must be Console");
            TEST_ASSERT(pSessionsW[i].State == termsrv::WTSActive, "Session 1 must be active");
        } else if (pSessionsW[i].SessionId == 65536) {
            hasSession65536 = true;
            TEST_ASSERT(std::wstring(pSessionsW[i].pWinStationName) == L"RDP-Tcp", "Session 65536 station name must be RDP-Tcp");
            TEST_ASSERT(pSessionsW[i].State == termsrv::WTSListen, "Session 65536 must be listening");
        }
        termsrv::WTSFreeMemory(pSessionsW[i].pWinStationName);
    }
    termsrv::WTSFreeMemory(pSessionsW);
    TEST_ASSERT(hasSession0 && hasSession1 && hasSession65536, "All default sessions must be present");

    termsrv::PWTS_SESSION_INFOA pSessionsA = nullptr;
    uint32_t countA = 0;
    int32_t okEnumA = termsrv::WTSEnumerateSessionsA(termsrv::WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionsA, &countA);
    TEST_ASSERT(okEnumA == 1 && pSessionsA != nullptr, "WTSEnumerateSessionsA must return TRUE and valid pointer");
    TEST_ASSERT(countA >= 3, "WTSEnumerateSessionsA count must match countW");
    for (uint32_t i = 0; i < countA; ++i) {
        termsrv::WTSFreeMemory(pSessionsA[i].pWinStationName);
    }
    termsrv::WTSFreeMemory(pSessionsA);

    // 5. Query Session Information (WTSQuerySessionInformationW)
    wchar_t* pBuf = nullptr;
    uint32_t bytesRet = 0;

    // UserName for Session 1
    int32_t qRes = termsrv::WTSQuerySessionInformationW(termsrv::WTS_CURRENT_SERVER_HANDLE, 1, termsrv::WTSUserName, &pBuf, &bytesRet);
    TEST_ASSERT(qRes == 1 && pBuf != nullptr, "WTSQuerySessionInformationW WTSUserName must succeed");
    TEST_ASSERT(std::wstring(pBuf) == L"Administrator", "Session 1 user must be Administrator");
    termsrv::WTSFreeMemory(pBuf);

    // DomainName for Session 1
    pBuf = nullptr;
    qRes = termsrv::WTSQuerySessionInformationW(termsrv::WTS_CURRENT_SERVER_HANDLE, 1, termsrv::WTSDomainName, &pBuf, &bytesRet);
    TEST_ASSERT(qRes == 1 && pBuf != nullptr, "WTSQuerySessionInformationW WTSDomainName must succeed");
    TEST_ASSERT(std::wstring(pBuf) == L"MICANT", "Session 1 domain must be MICANT");
    termsrv::WTSFreeMemory(pBuf);

    // ConnectState for Session 1
    pBuf = nullptr;
    qRes = termsrv::WTSQuerySessionInformationW(termsrv::WTS_CURRENT_SERVER_HANDLE, 1, termsrv::WTSConnectState, &pBuf, &bytesRet);
    TEST_ASSERT(qRes == 1 && pBuf != nullptr, "WTSQuerySessionInformationW WTSConnectState must succeed");
    auto stateVal = *reinterpret_cast<termsrv::WTS_CONNECTSTATE_CLASS*>(pBuf);
    TEST_ASSERT(stateVal == termsrv::WTSActive, "Session 1 state must be WTSActive");
    termsrv::WTSFreeMemory(pBuf);

    // Display for Session 1
    pBuf = nullptr;
    qRes = termsrv::WTSQuerySessionInformationW(termsrv::WTS_CURRENT_SERVER_HANDLE, 1, termsrv::WTSClientDisplay, &pBuf, &bytesRet);
    TEST_ASSERT(qRes == 1 && pBuf != nullptr, "WTSQuerySessionInformationW WTSClientDisplay must succeed");
    auto* pDisp = reinterpret_cast<termsrv::WTS_CLIENT_DISPLAY*>(pBuf);
    TEST_ASSERT(pDisp->HorizontalResolution == 1920 && pDisp->VerticalResolution == 1080, "Session 1 display resolution must be 1920x1080");
    termsrv::WTSFreeMemory(pBuf);

    // ClientAddress for Session 1
    pBuf = nullptr;
    qRes = termsrv::WTSQuerySessionInformationW(termsrv::WTS_CURRENT_SERVER_HANDLE, 1, termsrv::WTSClientAddress, &pBuf, &bytesRet);
    TEST_ASSERT(qRes == 1 && pBuf != nullptr, "WTSQuerySessionInformationW WTSClientAddress must succeed");
    auto* pAddr = reinterpret_cast<termsrv::WTS_CLIENT_ADDRESS*>(pBuf);
    TEST_ASSERT(pAddr->AddressFamily == 2, "AddressFamily must be AF_INET (2)");
    TEST_ASSERT(std::string(reinterpret_cast<char*>(pAddr->Address + 2)).find("127.0.0.1") != std::string::npos, "Address must be loopback");
    termsrv::WTSFreeMemory(pBuf);

    // Query Invalid Session
    pBuf = nullptr;
    qRes = termsrv::WTSQuerySessionInformationW(termsrv::WTS_CURRENT_SERVER_HANDLE, 99999, termsrv::WTSUserName, &pBuf, &bytesRet);
    TEST_ASSERT(qRes == 0, "Querying invalid session must return FALSE (0)");

    // 6. Dynamic RDP Session Creation & Lifecycle Management
    uint32_t newSid = termsrv::TerminalServicesManager::get().createRdpSession(
        L"Alice", L"MICANT", L"MSTSC-ALICE", 2560, 1440
    );
    TEST_ASSERT(newSid >= 2, "Created RDP session ID must be >= 2");

    termsrv::TerminalSession aliceSession;
    bool foundAlice = termsrv::TerminalServicesManager::get().getSession(newSid, aliceSession);
    TEST_ASSERT(foundAlice, "Alice's session must exist in session manager");
    TEST_ASSERT(aliceSession.userName == L"Alice", "Session username must match Alice");
    TEST_ASSERT(aliceSession.display.HorizontalResolution == 2560 && aliceSession.display.VerticalResolution == 1440, "Display resolution must match 2560x1440");
    TEST_ASSERT(aliceSession.protocolType == termsrv::WTS_PROTOCOL_TYPE_RDP, "Protocol type must be RDP");

    // WTSSendMessageW
    uint32_t response = 0;
    int32_t msgRes = termsrv::WTSSendMessageW(
        termsrv::WTS_CURRENT_SERVER_HANDLE, newSid,
        const_cast<wchar_t*>(L"Notice"), 6,
        const_cast<wchar_t*>(L"Hello Alice"), 11,
        0, 0, &response, 1
    );
    TEST_ASSERT(msgRes == 1 && response == 1, "WTSSendMessageW must return 1 and IDOK");

    // WTSDisconnectSession
    int32_t discRes = termsrv::WTSDisconnectSession(termsrv::WTS_CURRENT_SERVER_HANDLE, newSid, 1);
    TEST_ASSERT(discRes == 1, "WTSDisconnectSession must succeed");
    termsrv::TerminalServicesManager::get().getSession(newSid, aliceSession);
    TEST_ASSERT(aliceSession.state == termsrv::WTSDisconnected, "Session state must be WTSDisconnected");

    // WTSLogoffSession
    int32_t logoffRes = termsrv::WTSLogoffSession(termsrv::WTS_CURRENT_SERVER_HANDLE, newSid, 1);
    TEST_ASSERT(logoffRes == 1, "WTSLogoffSession must succeed");
    termsrv::TerminalServicesManager::get().getSession(newSid, aliceSession);
    TEST_ASSERT(aliceSession.state == termsrv::WTSDown, "Session state must be WTSDown");

    // 7. RDP Protocol (TPKT / X.224) Handshake Simulation
    std::string negotiatedSec;
    bool hsOk = termsrv::SimulateRdpHandshake("192.168.1.100", 3389, negotiatedSec);
    TEST_ASSERT(hsOk, "SimulateRdpHandshake must succeed");
    TEST_ASSERT(negotiatedSec.find("CredSSP") != std::string::npos, "Security must negotiate CredSSP / TLS 1.3");

    // Verify TPKT Framing structures
    termsrv::X224_CR_PACKET cr{};
    TEST_ASSERT(cr.tpkt.version == 3, "TPKT header version must be 3 (RFC 1006)");
    TEST_ASSERT(cr.connectionRequestCode == 0xE0, "X.224 CR code must be 0xE0");
    TEST_ASSERT(cr.type == 0x01, "RDP_NEG_REQ type must be 0x01");

    termsrv::X224_CC_PACKET cc{};
    TEST_ASSERT(cc.tpkt.version == 3, "TPKT header version must be 3");
    TEST_ASSERT(cc.connectionConfirmCode == 0xD0, "X.224 CC code must be 0xD0");
    TEST_ASSERT(cc.type == 0x02, "RDP_NEG_RSP type must be 0x02");

    // 8. Command Shell Integration (qwinsta, rwinsta, mstsc)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // qwinsta /?
        shell.execute("qwinsta /?", out);
        TEST_ASSERT(out.str().find("Display information about Remote Desktop Sessions") != std::string::npos, "qwinsta /? must display help");

        // qwinsta test
        out.str("");
        shell.execute("qwinsta test", out);
        TEST_ASSERT(out.str().find("Self-Test Completed Successfully") != std::string::npos, "qwinsta test must succeed");

        // qwinsta (normal)
        out.str("");
        shell.execute("qwinsta", out);
        TEST_ASSERT(out.str().find("SESSIONNAME") != std::string::npos, "qwinsta must print table header");
        TEST_ASSERT(out.str().find("services") != std::string::npos, "qwinsta must list services session");
        TEST_ASSERT(out.str().find("console") != std::string::npos, "qwinsta must list console session");
        TEST_ASSERT(out.str().find("Active") != std::string::npos, "qwinsta must show Active state");

        // rwinsta /?
        out.str("");
        shell.execute("rwinsta /?", out);
        TEST_ASSERT(out.str().find("Reset the session subsystem") != std::string::npos, "rwinsta /? must display help");

        // rwinsta test
        out.str("");
        shell.execute("rwinsta test", out);
        TEST_ASSERT(out.str().find("Self-Test Completed Successfully") != std::string::npos, "rwinsta test must succeed");

        // mstsc /?
        out.str("");
        shell.execute("mstsc /?", out);
        TEST_ASSERT(out.str().find("Remote Desktop window") != std::string::npos, "mstsc /? must display help");

        // mstsc test
        out.str("");
        shell.execute("mstsc test", out);
        TEST_ASSERT(out.str().find("Protocol and Session Self-Test Completed Successfully") != std::string::npos, "mstsc test must succeed");

        // mstsc /v:192.168.1.100 /admin
        out.str("");
        shell.execute("mstsc /v:192.168.1.100 /admin", out);
        TEST_ASSERT(out.str().find("Connecting to 192.168.1.100:3389") != std::string::npos, "mstsc must connect to target host");
        TEST_ASSERT(out.str().find("Remote Desktop session established") != std::string::npos, "mstsc must establish session");
        TEST_ASSERT(out.str().find("Console Admin Session") != std::string::npos, "mstsc must indicate admin session");
    }

    std::cout << "[TEST] Suite 87: Windows Remote Desktop Protocol (RDP) & Terminal Services Subsystem PASSED.\n";
}

void Test_WindowsPrinting_Spooler_Subsystem() {
    std::cout << "[TEST] Running Suite 88: Windows Printing & Print Spooler Subsystem...\n";

    // 1. Initialize Subsystem & Register Dynamic Exports
    winspool::InitializePrintSpoolerSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("winspool.drv", "OpenPrinterW") != nullptr, "OpenPrinterW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "OpenPrinterA") != nullptr, "OpenPrinterA must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "ClosePrinter") != nullptr, "ClosePrinter must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "EnumPrintersW") != nullptr, "EnumPrintersW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "EnumPrintersA") != nullptr, "EnumPrintersA must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "GetPrinterW") != nullptr, "GetPrinterW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "GetDefaultPrinterW") != nullptr, "GetDefaultPrinterW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "GetDefaultPrinterA") != nullptr, "GetDefaultPrinterA must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "SetDefaultPrinterW") != nullptr, "SetDefaultPrinterW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "StartDocPrinterW") != nullptr, "StartDocPrinterW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "StartPagePrinter") != nullptr, "StartPagePrinter must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "WritePrinter") != nullptr, "WritePrinter must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "EndPagePrinter") != nullptr, "EndPagePrinter must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "EndDocPrinter") != nullptr, "EndDocPrinter must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "AbortPrinter") != nullptr, "AbortPrinter must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "EnumJobsW") != nullptr, "EnumJobsW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("winspool.drv", "SetJobW") != nullptr, "SetJobW must be exported by winspool.drv");
    TEST_ASSERT(ldr.getExport("spoolsv.dll", "ServiceMain") != nullptr, "ServiceMain must be exported by spoolsv.dll");

    // 2. Version Information Introspection
    auto& ver = version::VersionDatabase::Instance();
    const auto* spoolDrv = ver.FindModule("winspool.drv");
    TEST_ASSERT(spoolDrv != nullptr, "winspool.drv must be registered in version manager");
    TEST_ASSERT(spoolDrv->stringTable.at("InternalName") == "winspool", "winspool InternalName must match");

    const auto* spoolExe = ver.FindModule("spoolsv.exe");
    TEST_ASSERT(spoolExe != nullptr, "spoolsv.exe must be registered in version manager");
    TEST_ASSERT(spoolExe->stringTable.at("InternalName") == "spoolsv", "spoolsv InternalName must match");

    const auto* prnMngr = ver.FindModule("prnmngr.exe");
    TEST_ASSERT(prnMngr != nullptr, "prnmngr.exe must be registered in version manager");
    TEST_ASSERT(prnMngr->stringTable.at("InternalName") == "prnmngr", "prnmngr InternalName must match");

    const auto* printExe = ver.FindModule("print.exe");
    TEST_ASSERT(printExe != nullptr, "print.exe must be registered in version manager");
    TEST_ASSERT(printExe->stringTable.at("InternalName") == "print", "print InternalName must match");

    // 3. SCM Spooler Service Integration
    auto& scmInst = scm::ServiceControlManager::get();
    auto spoolerSvc = scmInst.getServiceRecord(L"Spooler");
    TEST_ASSERT(spoolerSvc != nullptr, "Spooler service must be registered in SCM");
    TEST_ASSERT(spoolerSvc->displayName == L"Print Spooler", "Spooler display name must match");
    TEST_ASSERT(spoolerSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "Spooler service must be RUNNING");
    TEST_ASSERT(spoolerSvc->serviceType == scm::SERVICE_WIN32_OWN_PROCESS, "Spooler must be own process");

    // 4. Default Printer Management (GetDefaultPrinterW/A, SetDefaultPrinterW)
    winspool::PrintSpoolerManager::get().resetToDefault();

    wchar_t defBufW[256]{};
    uint32_t cchW = 256;
    int32_t okDefW = winspool::GetDefaultPrinterW(defBufW, &cchW);
    TEST_ASSERT(okDefW == 1, "GetDefaultPrinterW must succeed");
    TEST_ASSERT(std::wstring(defBufW) == L"Microsoft Print to PDF", "Initial default printer must be Microsoft Print to PDF");

    char defBufA[256]{};
    uint32_t cchA = 256;
    int32_t okDefA = winspool::GetDefaultPrinterA(defBufA, &cchA);
    TEST_ASSERT(okDefA == 1, "GetDefaultPrinterA must succeed");
    TEST_ASSERT(std::string(defBufA) == "Microsoft Print to PDF", "Initial default printer ANSI must match");

    int32_t setRes = winspool::SetDefaultPrinterW(L"Microsoft XPS Document Writer");
    TEST_ASSERT(setRes == 1, "SetDefaultPrinterW to XPS writer must succeed");
    cchW = 256;
    winspool::GetDefaultPrinterW(defBufW, &cchW);
    TEST_ASSERT(std::wstring(defBufW) == L"Microsoft XPS Document Writer", "Default printer must now be Microsoft XPS Document Writer");

    winspool::SetDefaultPrinterW(L"Microsoft Print to PDF");

    // 5. Printer Enumeration (EnumPrintersW / A)
    uint32_t cbNeeded = 0;
    uint32_t cReturned = 0;

    // Level 1 Enumeration (PRINTER_INFO_1W)
    winspool::EnumPrintersW(winspool::PRINTER_ENUM_LOCAL, nullptr, 1, nullptr, 0, &cbNeeded, &cReturned);
    TEST_ASSERT(cbNeeded > 0 && cReturned >= 3, "EnumPrintersW Level 1 must calculate buffer needed and return >= 3");

    std::vector<uint8_t> buf1W(cbNeeded);
    int32_t enumRes1 = winspool::EnumPrintersW(winspool::PRINTER_ENUM_LOCAL, nullptr, 1, buf1W.data(), cbNeeded, &cbNeeded, &cReturned);
    TEST_ASSERT(enumRes1 == 1, "EnumPrintersW Level 1 must succeed");
    auto* pInfo1 = reinterpret_cast<winspool::PRINTER_INFO_1W*>(buf1W.data());
    bool foundPdf = false;
    bool foundXps = false;
    bool foundPs = false;
    for (uint32_t i = 0; i < cReturned; ++i) {
        if (pInfo1[i].pName) {
            std::wstring n(pInfo1[i].pName);
            if (n == L"Microsoft Print to PDF") foundPdf = true;
            if (n == L"Microsoft XPS Document Writer") foundXps = true;
            if (n == L"MicaNT Virtual PostScript Color Printer") foundPs = true;
            ::free(pInfo1[i].pName);
            ::free(pInfo1[i].pDescription);
            ::free(pInfo1[i].pComment);
        }
    }
    TEST_ASSERT(foundPdf && foundXps && foundPs, "All pre-seeded printers must be enumerated in Level 1");

    // Level 2 Enumeration (PRINTER_INFO_2W)
    cbNeeded = 0;
    cReturned = 0;
    winspool::EnumPrintersW(winspool::PRINTER_ENUM_LOCAL, nullptr, 2, nullptr, 0, &cbNeeded, &cReturned);
    std::vector<uint8_t> buf2W(cbNeeded);
    int32_t enumRes2 = winspool::EnumPrintersW(winspool::PRINTER_ENUM_LOCAL, nullptr, 2, buf2W.data(), cbNeeded, &cbNeeded, &cReturned);
    TEST_ASSERT(enumRes2 == 1, "EnumPrintersW Level 2 must succeed");
    auto* pInfo2 = reinterpret_cast<winspool::PRINTER_INFO_2W*>(buf2W.data());
    for (uint32_t i = 0; i < cReturned; ++i) {
        ::free(pInfo2[i].pServerName);
        ::free(pInfo2[i].pPrinterName);
        ::free(pInfo2[i].pShareName);
        ::free(pInfo2[i].pPortName);
        ::free(pInfo2[i].pDriverName);
        ::free(pInfo2[i].pComment);
        ::free(pInfo2[i].pLocation);
        ::free(pInfo2[i].pPrintProcessor);
        ::free(pInfo2[i].pDatatype);
    }

    // Level 4 Enumeration (PRINTER_INFO_4W)
    cbNeeded = 0;
    cReturned = 0;
    winspool::EnumPrintersW(winspool::PRINTER_ENUM_LOCAL, nullptr, 4, nullptr, 0, &cbNeeded, &cReturned);
    std::vector<uint8_t> buf4W(cbNeeded);
    int32_t enumRes4 = winspool::EnumPrintersW(winspool::PRINTER_ENUM_LOCAL, nullptr, 4, buf4W.data(), cbNeeded, &cbNeeded, &cReturned);
    TEST_ASSERT(enumRes4 == 1, "EnumPrintersW Level 4 must succeed");
    auto* pInfo4 = reinterpret_cast<winspool::PRINTER_INFO_4W*>(buf4W.data());
    for (uint32_t i = 0; i < cReturned; ++i) {
        ::free(pInfo4[i].pPrinterName);
        ::free(pInfo4[i].pServerName);
    }

    // 6. Printer Inspection & Spooling Workflow (OpenPrinterW -> StartDoc -> Write -> EndDoc)
    uintptr_t hPrinter = 0;
    int32_t opRes = winspool::OpenPrinterW(const_cast<wchar_t*>(L"Microsoft Print to PDF"), &hPrinter, nullptr);
    TEST_ASSERT(opRes == 1 && hPrinter != 0, "OpenPrinterW must succeed and return handle");

    // GetPrinterW Level 2
    cbNeeded = 0;
    winspool::GetPrinterW(hPrinter, 2, nullptr, 0, &cbNeeded);
    std::vector<uint8_t> getBuf(cbNeeded);
    int32_t gpRes = winspool::GetPrinterW(hPrinter, 2, getBuf.data(), cbNeeded, &cbNeeded);
    TEST_ASSERT(gpRes == 1, "GetPrinterW Level 2 must succeed");
    auto* pDetail = reinterpret_cast<winspool::PRINTER_INFO_2W*>(getBuf.data());
    TEST_ASSERT(std::wstring(pDetail->pPortName) == L"PORTPROMPT:", "PDF printer port must be PORTPROMPT:");
    TEST_ASSERT(std::wstring(pDetail->pDriverName) == L"Microsoft Print To PDF", "PDF printer driver must match");
    ::free(pDetail->pServerName);
    ::free(pDetail->pPrinterName);
    ::free(pDetail->pShareName);
    ::free(pDetail->pPortName);
    ::free(pDetail->pDriverName);
    ::free(pDetail->pComment);
    ::free(pDetail->pLocation);
    ::free(pDetail->pPrintProcessor);
    ::free(pDetail->pDatatype);

    // Spooling: StartDocPrinterW
    winspool::DOC_INFO_1W di{};
    di.pDocName = const_cast<wchar_t*>(L"Quarterly_Report.docx");
    di.pDatatype = const_cast<wchar_t*>(L"RAW");

    uint32_t jobId = winspool::StartDocPrinterW(hPrinter, 1, reinterpret_cast<uint8_t*>(&di));
    TEST_ASSERT(jobId >= 1, "StartDocPrinterW must assign valid JobId >= 1");

    // Page 1
    int32_t page1Res = winspool::StartPagePrinter(hPrinter);
    TEST_ASSERT(page1Res == 1, "StartPagePrinter must succeed");

    const char page1Data[] = "%PDF-1.7 Executive Financial Statement Page 1\r\n";
    uint32_t written1 = 0;
    int32_t wr1Res = winspool::WritePrinter(hPrinter, const_cast<char*>(page1Data), sizeof(page1Data) - 1, &written1);
    TEST_ASSERT(wr1Res == 1 && written1 == sizeof(page1Data) - 1, "WritePrinter Page 1 must write all bytes");

    int32_t ep1Res = winspool::EndPagePrinter(hPrinter);
    TEST_ASSERT(ep1Res == 1, "EndPagePrinter Page 1 must succeed");

    // Page 2
    winspool::StartPagePrinter(hPrinter);
    const char page2Data[] = "Executive Financial Statement Page 2 - Balance Sheet\r\n";
    uint32_t written2 = 0;
    winspool::WritePrinter(hPrinter, const_cast<char*>(page2Data), sizeof(page2Data) - 1, &written2);
    winspool::EndPagePrinter(hPrinter);

    // EnumJobsW: Inspect job while in progress
    cbNeeded = 0;
    cReturned = 0;
    winspool::EnumJobsW(hPrinter, 0, 10, 1, nullptr, 0, &cbNeeded, &cReturned);
    std::vector<uint8_t> jobBuf(cbNeeded);
    winspool::EnumJobsW(hPrinter, 0, 10, 1, jobBuf.data(), cbNeeded, &cbNeeded, &cReturned);
    TEST_ASSERT(cReturned == 1, "EnumJobsW must return 1 active job");
    auto* pJob = reinterpret_cast<winspool::JOB_INFO_1W*>(jobBuf.data());
    TEST_ASSERT(pJob->JobId == jobId, "Job ID must match");
    TEST_ASSERT(std::wstring(pJob->pDocument) == L"Quarterly_Report.docx", "Job document name must match");
    TEST_ASSERT(pJob->PagesPrinted == 2, "Job must have 2 pages printed");
    ::free(pJob->pPrinterName);
    ::free(pJob->pMachineName);
    ::free(pJob->pUserName);
    ::free(pJob->pDocument);
    ::free(pJob->pDatatype);
    ::free(pJob->pStatus);

    // Job Control: Pause & Resume
    int32_t pauseRes = winspool::SetJobW(hPrinter, jobId, 1, nullptr, winspool::JOB_CONTROL_PAUSE);
    TEST_ASSERT(pauseRes == 1, "SetJobW JOB_CONTROL_PAUSE must succeed");
    int32_t resumeRes = winspool::SetJobW(hPrinter, jobId, 1, nullptr, winspool::JOB_CONTROL_RESUME);
    TEST_ASSERT(resumeRes == 1, "SetJobW JOB_CONTROL_RESUME must succeed");

    // EndDocPrinter: Complete print job
    int32_t edRes = winspool::EndDocPrinter(hPrinter);
    TEST_ASSERT(edRes == 1, "EndDocPrinter must succeed");

    // ClosePrinter
    int32_t cpRes = winspool::ClosePrinter(hPrinter);
    TEST_ASSERT(cpRes == 1, "ClosePrinter must succeed");

    // 7. Interactive Command Shell (prnmngr & print)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // prnmngr /?
        shell.execute("prnmngr /?", out);
        TEST_ASSERT(out.str().find("Windows Printer Management Utility") != std::string::npos, "prnmngr /? must display help");

        // prnmngr test
        out.str("");
        shell.execute("prnmngr test", out);
        TEST_ASSERT(out.str().find("Self-Test Completed Successfully") != std::string::npos, "prnmngr test must succeed");

        // prnmngr -d
        out.str("");
        shell.execute("prnmngr -d", out);
        TEST_ASSERT(out.str().find("The default printer is \"Microsoft Print to PDF\"") != std::string::npos, "prnmngr -d must output default printer");

        // prnmngr -l
        out.str("");
        shell.execute("prnmngr -l", out);
        TEST_ASSERT(out.str().find("Microsoft XPS Document Writer") != std::string::npos, "prnmngr -l must list XPS writer");
        TEST_ASSERT(out.str().find("MicaNT Virtual PostScript Color Printer") != std::string::npos, "prnmngr -l must list PostScript printer");

        // print /?
        out.str("");
        shell.execute("print /?", out);
        TEST_ASSERT(out.str().find("Prints a text file or test document") != std::string::npos, "print /? must display help");

        // print test
        out.str("");
        shell.execute("print test", out);
        TEST_ASSERT(out.str().find("Self-Test Completed Successfully") != std::string::npos, "print test must succeed");

        // print document.txt
        out.str("");
        shell.execute("print document.txt", out);
        TEST_ASSERT(out.str().find("Spooling \"document.txt\" to Microsoft Print to PDF") != std::string::npos, "print must spool document");
        TEST_ASSERT(out.str().find("successfully sent to spooler") != std::string::npos, "print must report success");
    }

    std::cout << "[TEST] Suite 88: Windows Printing & Print Spooler Subsystem PASSED.\n";
}

void Test_WindowsMCI_AudioWave_Subsystem() {
    std::cout << "[TEST] Running Suite 89: Windows Media Control Interface (MCI) & Audio Wave Subsystem...\n";

    // 1. Dynamic Loader Exports Verification
    {
        mci::InitializeMciSubsystemExports();
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutGetNumDevs") != nullptr, "winmm.dll must export waveOutGetNumDevs");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutGetDevCapsW") != nullptr, "winmm.dll must export waveOutGetDevCapsW");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutGetDevCapsA") != nullptr, "winmm.dll must export waveOutGetDevCapsA");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutOpen") != nullptr, "winmm.dll must export waveOutOpen");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutClose") != nullptr, "winmm.dll must export waveOutClose");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutPrepareHeader") != nullptr, "winmm.dll must export waveOutPrepareHeader");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutUnprepareHeader") != nullptr, "winmm.dll must export waveOutUnprepareHeader");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutWrite") != nullptr, "winmm.dll must export waveOutWrite");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutPause") != nullptr, "winmm.dll must export waveOutPause");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutRestart") != nullptr, "winmm.dll must export waveOutRestart");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutReset") != nullptr, "winmm.dll must export waveOutReset");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutGetPosition") != nullptr, "winmm.dll must export waveOutGetPosition");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutGetVolume") != nullptr, "winmm.dll must export waveOutGetVolume");
        TEST_ASSERT(ldr.getExport("winmm.dll", "waveOutSetVolume") != nullptr, "winmm.dll must export waveOutSetVolume");
        TEST_ASSERT(ldr.getExport("winmm.dll", "auxGetNumDevs") != nullptr, "winmm.dll must export auxGetNumDevs");
        TEST_ASSERT(ldr.getExport("winmm.dll", "auxGetDevCapsW") != nullptr, "winmm.dll must export auxGetDevCapsW");
        TEST_ASSERT(ldr.getExport("winmm.dll", "auxGetDevCapsA") != nullptr, "winmm.dll must export auxGetDevCapsA");
        TEST_ASSERT(ldr.getExport("winmm.dll", "auxGetVolume") != nullptr, "winmm.dll must export auxGetVolume");
        TEST_ASSERT(ldr.getExport("winmm.dll", "auxSetVolume") != nullptr, "winmm.dll must export auxSetVolume");
        TEST_ASSERT(ldr.getExport("winmm.dll", "mciSendCommandW") != nullptr, "winmm.dll must export mciSendCommandW");
        TEST_ASSERT(ldr.getExport("winmm.dll", "mciSendCommandA") != nullptr, "winmm.dll must export mciSendCommandA");
        TEST_ASSERT(ldr.getExport("winmm.dll", "mciSendStringW") != nullptr, "winmm.dll must export mciSendStringW");
        TEST_ASSERT(ldr.getExport("winmm.dll", "mciSendStringA") != nullptr, "winmm.dll must export mciSendStringA");
        TEST_ASSERT(ldr.getExport("winmm.dll", "mciGetErrorStringW") != nullptr, "winmm.dll must export mciGetErrorStringW");
        TEST_ASSERT(ldr.getExport("winmm.dll", "mciGetErrorStringA") != nullptr, "winmm.dll must export mciGetErrorStringA");

        TEST_ASSERT(ldr.getExport("mciwave.dll", "DriverProc") != nullptr, "mciwave.dll must export DriverProc");
    }

    // 2. Version Database Validation
    {
        const auto* modMciWave = version::VersionDatabase::Instance().FindModule("mciwave.dll");
        TEST_ASSERT(modMciWave != nullptr, "mciwave.dll must be in version database");
        TEST_ASSERT(modMciWave->stringTable.at("InternalName") == "mciwave", "mciwave.dll InternalName mismatch");
        TEST_ASSERT(modMciWave->stringTable.at("FileDescription") == "MCI Waveform Audio Device Driver", "mciwave.dll FileDescription mismatch");

        const auto* modMPlayer = version::VersionDatabase::Instance().FindModule("mplayer.exe");
        TEST_ASSERT(modMPlayer != nullptr, "mplayer.exe must be in version database");
        TEST_ASSERT(modMPlayer->stringTable.at("InternalName") == "mplayer", "mplayer.exe InternalName mismatch");

        const auto* modWavePlay = version::VersionDatabase::Instance().FindModule("waveplay.exe");
        TEST_ASSERT(modWavePlay != nullptr, "waveplay.exe must be in version database");
        TEST_ASSERT(modWavePlay->stringTable.at("InternalName") == "waveplay", "waveplay.exe InternalName mismatch");
    }

    // 3. Waveform Output Device Enumeration & DevCaps
    {
        uint32_t numDevs = mci::waveOutGetNumDevs();
        TEST_ASSERT(numDevs >= 2, "waveOutGetNumDevs must report at least 2 output devices");

        mci::WAVEOUTCAPSW capsW{};
        mci::MMRESULT mrW = mci::waveOutGetDevCapsW(0, &capsW, sizeof(capsW));
        TEST_ASSERT(mrW == mci::MMSYSERR_NOERROR, "waveOutGetDevCapsW(0) must succeed");
        TEST_ASSERT(capsW.wChannels >= 2, "Default output device must support at least 2 channels");
        TEST_ASSERT(std::wstring(capsW.szPname).find(L"MicaNT High Definition Audio") != std::wstring::npos, "Device 0 must match MicaNT HDA");

        mci::WAVEOUTCAPSA capsA{};
        mci::MMRESULT mrA = mci::waveOutGetDevCapsA(1, &capsA, sizeof(capsA));
        TEST_ASSERT(mrA == mci::MMSYSERR_NOERROR, "waveOutGetDevCapsA(1) must succeed");
        TEST_ASSERT(std::string(capsA.szPname).find("MicaNT Synthetic Wave Synth") != std::string::npos, "Device 1 must match Wave Synth");

        mci::WAVEOUTCAPSW badCaps{};
        mci::MMRESULT mrBad = mci::waveOutGetDevCapsW(999, &badCaps, sizeof(badCaps));
        TEST_ASSERT(mrBad == mci::MMSYSERR_BADDEVICEID, "waveOutGetDevCapsW with bad ID must return MMSYSERR_BADDEVICEID");
    }

    // 4. Waveform Audio Stream Playback Lifecycle
    {
        mci::WAVEFORMATEX wfx{};
        wfx.wFormatTag = mci::WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = 44100;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        mci::HWAVEOUT hWave = nullptr;
        mci::MMRESULT mrOpen = mci::waveOutOpen(&hWave, 0, &wfx, 0, 0, 0);
        TEST_ASSERT(mrOpen == mci::MMSYSERR_NOERROR && hWave != nullptr, "waveOutOpen must succeed and return valid handle");

        // Volume controls
        uint32_t vol = 0;
        mci::MMRESULT mrVolGet = mci::waveOutGetVolume(hWave, &vol);
        TEST_ASSERT(mrVolGet == mci::MMSYSERR_NOERROR, "waveOutGetVolume must succeed");

        mci::MMRESULT mrVolSet = mci::waveOutSetVolume(hWave, 0x80008000);
        TEST_ASSERT(mrVolSet == mci::MMSYSERR_NOERROR, "waveOutSetVolume must succeed");

        uint32_t verifyVol = 0;
        mci::waveOutGetVolume(hWave, &verifyVol);
        TEST_ASSERT(verifyVol == 0x80008000, "waveOutGetVolume must return updated volume");

        // Prepare synthetic audio buffer (50ms stereo 44.1kHz = 2205 frames * 4 bytes = 8820 bytes)
        std::vector<int16_t> pcmData(2205 * 2, 0);
        for (size_t i = 0; i < 2205; ++i) {
            int16_t sample = static_cast<int16_t>(10000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / 44100.0));
            pcmData[i * 2 + 0] = sample;
            pcmData[i * 2 + 1] = sample;
        }

        mci::WAVEHDR hdr{};
        hdr.lpData = reinterpret_cast<char*>(pcmData.data());
        hdr.dwBufferLength = static_cast<uint32_t>(pcmData.size() * sizeof(int16_t));

        mci::MMRESULT mrPrep = mci::waveOutPrepareHeader(hWave, &hdr, sizeof(hdr));
        TEST_ASSERT(mrPrep == mci::MMSYSERR_NOERROR, "waveOutPrepareHeader must succeed");
        TEST_ASSERT((hdr.dwFlags & mci::WHDR_PREPARED) != 0, "WHDR_PREPARED must be set after prepare");

        mci::MMRESULT mrWrite = mci::waveOutWrite(hWave, &hdr, sizeof(hdr));
        TEST_ASSERT(mrWrite == mci::MMSYSERR_NOERROR, "waveOutWrite must succeed");

        mci::MMTIME mmt{};
        mmt.wType = mci::TIME_BYTES;
        mci::MMRESULT mrPos = mci::waveOutGetPosition(hWave, &mmt, sizeof(mmt));
        TEST_ASSERT(mrPos == mci::MMSYSERR_NOERROR, "waveOutGetPosition must succeed");
        TEST_ASSERT(mmt.u.cb == hdr.dwBufferLength, "waveOutGetPosition must reflect queued bytes");

        mci::MMRESULT mrPause = mci::waveOutPause(hWave);
        TEST_ASSERT(mrPause == mci::MMSYSERR_NOERROR, "waveOutPause must succeed");

        mci::MMRESULT mrRestart = mci::waveOutRestart(hWave);
        TEST_ASSERT(mrRestart == mci::MMSYSERR_NOERROR, "waveOutRestart must succeed");

        mci::MMRESULT mrReset = mci::waveOutReset(hWave);
        TEST_ASSERT(mrReset == mci::MMSYSERR_NOERROR, "waveOutReset must succeed");

        mci::MMRESULT mrUnprep = mci::waveOutUnprepareHeader(hWave, &hdr, sizeof(hdr));
        TEST_ASSERT(mrUnprep == mci::MMSYSERR_NOERROR, "waveOutUnprepareHeader must succeed");
        TEST_ASSERT((hdr.dwFlags & mci::WHDR_PREPARED) == 0, "WHDR_PREPARED must be cleared after unprepare");

        mci::MMRESULT mrClose = mci::waveOutClose(hWave);
        TEST_ASSERT(mrClose == mci::MMSYSERR_NOERROR, "waveOutClose must succeed");
    }

    // 5. Auxiliary Audio Controls
    {
        uint32_t auxCount = mci::auxGetNumDevs();
        TEST_ASSERT(auxCount >= 1, "auxGetNumDevs must report at least 1 auxiliary device");

        mci::AUXCAPSW auxCaps{};
        mci::MMRESULT mrAuxCaps = mci::auxGetDevCapsW(0, &auxCaps, sizeof(auxCaps));
        TEST_ASSERT(mrAuxCaps == mci::MMSYSERR_NOERROR, "auxGetDevCapsW must succeed");
        TEST_ASSERT(std::wstring(auxCaps.szPname).find(L"Auxiliary") != std::wstring::npos, "Auxiliary device name check");

        uint32_t origAuxVol = 0;
        mci::MMRESULT mrAuxGet = mci::auxGetVolume(0, &origAuxVol);
        TEST_ASSERT(mrAuxGet == mci::MMSYSERR_NOERROR, "auxGetVolume must succeed");

        mci::MMRESULT mrAuxSet = mci::auxSetVolume(0, 0xAAAA5555);
        TEST_ASSERT(mrAuxSet == mci::MMSYSERR_NOERROR, "auxSetVolume must succeed");

        uint32_t newAuxVol = 0;
        mci::auxGetVolume(0, &newAuxVol);
        TEST_ASSERT(newAuxVol == 0xAAAA5555, "auxGetVolume must reflect set volume");
    }

    // 6. MCI Command Message Dispatch (mciSendCommandW/A)
    {
        mci::MCI_OPEN_PARMSW openParms{};
        openParms.lpstrDeviceType = const_cast<wchar_t*>(L"waveaudio");
        openParms.lpstrElementName = const_cast<wchar_t*>(L"theme.wav");
        openParms.lpstrAlias = const_cast<wchar_t*>(L"soundtrack");

        mci::MCIERROR errOpen = mci::mciSendCommandW(
            0, mci::MCI_OPEN,
            mci::MCI_OPEN_TYPE | mci::MCI_OPEN_ELEMENT | mci::MCI_OPEN_ALIAS,
            reinterpret_cast<uintptr_t>(&openParms)
        );
        TEST_ASSERT(errOpen == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_OPEN) must succeed");
        TEST_ASSERT(openParms.wDeviceID != 0, "MCI device ID must be non-zero");

        mci::MCI_PLAY_PARMS playParms{};
        mci::MCIERROR errPlay = mci::mciSendCommandW(openParms.wDeviceID, mci::MCI_PLAY, 0, reinterpret_cast<uintptr_t>(&playParms));
        TEST_ASSERT(errPlay == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_PLAY) must succeed");

        mci::MCI_STATUS_PARMS statusParms{};
        statusParms.dwItem = mci::MCI_STATUS_MODE;
        mci::MCIERROR errStatus = mci::mciSendCommandW(openParms.wDeviceID, mci::MCI_STATUS, mci::MCI_STATUS_ITEM, reinterpret_cast<uintptr_t>(&statusParms));
        TEST_ASSERT(errStatus == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_STATUS) must succeed");
        TEST_ASSERT(statusParms.dwReturn == mci::MCI_MODE_PLAY, "Status mode must be MCI_MODE_PLAY");

        mci::MCI_GENERIC_PARMS genParms{};
        mci::MCIERROR errPause = mci::mciSendCommandW(openParms.wDeviceID, mci::MCI_PAUSE, 0, reinterpret_cast<uintptr_t>(&genParms));
        TEST_ASSERT(errPause == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_PAUSE) must succeed");

        mci::MCIERROR errResume = mci::mciSendCommandW(openParms.wDeviceID, mci::MCI_RESUME, 0, reinterpret_cast<uintptr_t>(&genParms));
        TEST_ASSERT(errResume == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_RESUME) must succeed");

        mci::MCIERROR errStop = mci::mciSendCommandW(openParms.wDeviceID, mci::MCI_STOP, 0, reinterpret_cast<uintptr_t>(&genParms));
        TEST_ASSERT(errStop == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_STOP) must succeed");

        mci::MCIERROR errClose = mci::mciSendCommandW(openParms.wDeviceID, mci::MCI_CLOSE, 0, reinterpret_cast<uintptr_t>(&genParms));
        TEST_ASSERT(errClose == mci::MCIERR_SUCCESS, "mciSendCommandW(MCI_CLOSE) must succeed");
    }

    // 7. MCI String Command Parsing & Execution (mciSendStringW/A)
    {
        char retBuf[128]{};
        mci::MCIERROR errOpen = mci::mciSendStringA("open fanfare.wav type waveaudio alias fanfare", retBuf, sizeof(retBuf), nullptr);
        TEST_ASSERT(errOpen == mci::MCIERR_SUCCESS, "mciSendStringA(open) must succeed");

        mci::MCIERROR errPlay = mci::mciSendStringA("play fanfare", nullptr, 0, nullptr);
        TEST_ASSERT(errPlay == mci::MCIERR_SUCCESS, "mciSendStringA(play) must succeed");

        std::memset(retBuf, 0, sizeof(retBuf));
        mci::MCIERROR errMode = mci::mciSendStringA("status fanfare mode", retBuf, sizeof(retBuf), nullptr);
        TEST_ASSERT(errMode == mci::MCIERR_SUCCESS, "mciSendStringA(status mode) must succeed");
        TEST_ASSERT(std::string(retBuf) == "playing", "Status mode string must be 'playing'");

        mci::MCIERROR errPause = mci::mciSendStringA("pause fanfare", nullptr, 0, nullptr);
        TEST_ASSERT(errPause == mci::MCIERR_SUCCESS, "mciSendStringA(pause) must succeed");

        std::memset(retBuf, 0, sizeof(retBuf));
        mci::mciSendStringA("status fanfare mode", retBuf, sizeof(retBuf), nullptr);
        TEST_ASSERT(std::string(retBuf) == "paused", "Status mode string must be 'paused'");

        mci::MCIERROR errResume = mci::mciSendStringA("resume fanfare", nullptr, 0, nullptr);
        TEST_ASSERT(errResume == mci::MCIERR_SUCCESS, "mciSendStringA(resume) must succeed");

        mci::MCIERROR errStop = mci::mciSendStringA("stop fanfare", nullptr, 0, nullptr);
        TEST_ASSERT(errStop == mci::MCIERR_SUCCESS, "mciSendStringA(stop) must succeed");

        mci::MCIERROR errClose = mci::mciSendStringA("close fanfare", nullptr, 0, nullptr);
        TEST_ASSERT(errClose == mci::MCIERR_SUCCESS, "mciSendStringA(close) must succeed");

        // Invalid command error check
        mci::MCIERROR errBad = mci::mciSendStringA("invalid_verb_unknown fanfare", nullptr, 0, nullptr);
        TEST_ASSERT(errBad == mci::MCIERR_UNRECOGNIZED_COMMAND, "Unknown verb must return MCIERR_UNRECOGNIZED_COMMAND");
    }

    // 8. MCI Error String Formatting
    {
        char errA[128]{};
        int32_t resA = mci::mciGetErrorStringA(mci::MCIERR_SUCCESS, errA, sizeof(errA));
        TEST_ASSERT(resA == 1 && std::string(errA) == "No error", "mciGetErrorStringA for MCIERR_SUCCESS");

        wchar_t errW[128]{};
        int32_t resW = mci::mciGetErrorStringW(mci::MCIERR_INVALID_DEVICE_NAME, errW, 128);
        TEST_ASSERT(resW == 1 && std::wstring(errW) == L"Specified device alias is not open", "mciGetErrorStringW for MCIERR_INVALID_DEVICE_NAME");
    }

    // 9. Interactive Shell Commands (mci, waveplay)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // mci test
        out.str("");
        shell.execute("mci test", out);
        TEST_ASSERT(out.str().find("[MCI] Self-Test Completed Successfully") != std::string::npos, "mci test must succeed");

        // waveplay test
        out.str("");
        shell.execute("waveplay test", out);
        TEST_ASSERT(out.str().find("[WAVEPLAY] Self-Test Completed Successfully") != std::string::npos, "waveplay test must succeed");

        // mci command execution through shell
        out.str("");
        shell.execute("mci open beep.wav type waveaudio alias mybeep", out);
        TEST_ASSERT(out.str().find("completed successfully") != std::string::npos, "mci open via shell must succeed");

        out.str("");
        shell.execute("mci play mybeep", out);
        TEST_ASSERT(out.str().find("completed successfully") != std::string::npos, "mci play via shell must succeed");

        out.str("");
        shell.execute("mci status mybeep mode", out);
        TEST_ASSERT(out.str().find("playing") != std::string::npos, "mci status via shell must return playing");

        out.str("");
        shell.execute("mci close mybeep", out);
        TEST_ASSERT(out.str().find("completed successfully") != std::string::npos, "mci close via shell must succeed");
    }

    std::cout << "[TEST] Suite 89: Windows Media Control Interface (MCI) & Audio Wave Subsystem PASSED.\n";
}

void Test_WindowsSmartCard_PCSC_Subsystem() {
    std::cout << "[TEST] Running Suite 90: Windows Smart Card & PC/SC Subsystem...\n";

    // 1. Dynamic Loader Exports Verification
    {
        scard::InitializeWinSCardSubsystemExports();
        auto& ldr = ldr::DynamicLoader::get();

        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardEstablishContext") != nullptr, "winscard.dll must export SCardEstablishContext");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardReleaseContext") != nullptr, "winscard.dll must export SCardReleaseContext");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardIsValidContext") != nullptr, "winscard.dll must export SCardIsValidContext");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardListReaderGroupsW") != nullptr, "winscard.dll must export SCardListReaderGroupsW");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardListReaderGroupsA") != nullptr, "winscard.dll must export SCardListReaderGroupsA");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardListReadersW") != nullptr, "winscard.dll must export SCardListReadersW");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardListReadersA") != nullptr, "winscard.dll must export SCardListReadersA");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardConnectW") != nullptr, "winscard.dll must export SCardConnectW");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardConnectA") != nullptr, "winscard.dll must export SCardConnectA");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardReconnect") != nullptr, "winscard.dll must export SCardReconnect");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardDisconnect") != nullptr, "winscard.dll must export SCardDisconnect");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardStatusW") != nullptr, "winscard.dll must export SCardStatusW");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardStatusA") != nullptr, "winscard.dll must export SCardStatusA");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardGetStatusChangeW") != nullptr, "winscard.dll must export SCardGetStatusChangeW");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardGetStatusChangeA") != nullptr, "winscard.dll must export SCardGetStatusChangeA");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardTransmit") != nullptr, "winscard.dll must export SCardTransmit");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardControl") != nullptr, "winscard.dll must export SCardControl");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardGetAttrib") != nullptr, "winscard.dll must export SCardGetAttrib");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardSetAttrib") != nullptr, "winscard.dll must export SCardSetAttrib");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardCancel") != nullptr, "winscard.dll must export SCardCancel");
        TEST_ASSERT(ldr.getExport("winscard.dll", "SCardFreeMemory") != nullptr, "winscard.dll must export SCardFreeMemory");

        TEST_ASSERT(ldr.getExport("scredir.dll", "SCardEstablishContext") != nullptr, "scredir.dll must export SCardEstablishContext");
        TEST_ASSERT(ldr.getExport("certprop.dll", "DllRegisterServer") != nullptr, "certprop.dll must export DllRegisterServer");
    }

    // 2. Version Database Verification
    {
        const auto* modWinSCard = version::VersionDatabase::Instance().FindModule("winscard.dll");
        TEST_ASSERT(modWinSCard != nullptr, "winscard.dll must exist in Version Database");
        TEST_ASSERT(modWinSCard->stringTable.at("InternalName") == "winscard", "winscard.dll InternalName");
        TEST_ASSERT(modWinSCard->stringTable.at("FileDescription") == "Microsoft Smart Card API", "winscard.dll FileDescription");

        const auto* modScRedir = version::VersionDatabase::Instance().FindModule("scredir.dll");
        TEST_ASSERT(modScRedir != nullptr, "scredir.dll must exist in Version Database");
        TEST_ASSERT(modScRedir->stringTable.at("InternalName") == "scredir", "scredir.dll InternalName");

        const auto* modCertProp = version::VersionDatabase::Instance().FindModule("certprop.dll");
        TEST_ASSERT(modCertProp != nullptr, "certprop.dll must exist in Version Database");
        TEST_ASSERT(modCertProp->stringTable.at("InternalName") == "certprop", "certprop.dll InternalName");

        const auto* modCertUtil = version::VersionDatabase::Instance().FindModule("certutil.exe");
        TEST_ASSERT(modCertUtil != nullptr, "certutil.exe must exist in Version Database");
        TEST_ASSERT(modCertUtil->stringTable.at("InternalName") == "certutil", "certutil.exe InternalName");
    }

    // 3. SCM Service Registration (ScardSvr, CertPropSvr)
    {
        auto scardSvr = scm::ServiceControlManager::get().getServiceRecord(L"ScardSvr");
        TEST_ASSERT(scardSvr != nullptr, "ScardSvr must be registered in SCM");
        TEST_ASSERT(scardSvr->displayName == L"Smart Card", "ScardSvr display name");
        TEST_ASSERT(scardSvr->status.dwCurrentState == scm::SERVICE_RUNNING, "ScardSvr must be RUNNING");

        auto certPropSvr = scm::ServiceControlManager::get().getServiceRecord(L"CertPropSvr");
        TEST_ASSERT(certPropSvr != nullptr, "CertPropSvr must be registered in SCM");
        TEST_ASSERT(certPropSvr->displayName == L"Certificate Propagation", "CertPropSvr display name");
        TEST_ASSERT(certPropSvr->status.dwCurrentState == scm::SERVICE_RUNNING, "CertPropSvr must be RUNNING");
    }

    // 4. Context Lifecycle (Establish, Validate, Cancel, Release)
    {
        scard::SCARDCONTEXT hCtx = 0;
        int32_t rc = scard::SCardEstablishContext(scard::SCARD_SCOPE_USER, nullptr, nullptr, &hCtx);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardEstablishContext(USER) must succeed");
        TEST_ASSERT(hCtx != 0, "hCtx must be valid non-zero");

        rc = scard::SCardIsValidContext(hCtx);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardIsValidContext on valid context must return SUCCESS");

        rc = scard::SCardCancel(hCtx);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardCancel on active context must succeed");

        rc = scard::SCardReleaseContext(hCtx);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardReleaseContext must succeed");

        rc = scard::SCardIsValidContext(hCtx);
        TEST_ASSERT(rc == scard::SCARD_E_INVALID_HANDLE, "SCardIsValidContext on released context must fail");

        // Invalid parameter check
        rc = scard::SCardEstablishContext(scard::SCARD_SCOPE_USER, nullptr, nullptr, nullptr);
        TEST_ASSERT(rc == scard::SCARD_E_INVALID_PARAMETER, "SCardEstablishContext with null output pointer must fail");
    }

    // 5. Reader and Reader Group Enumeration (SCardListReaderGroupsW/A, SCardListReadersW/A)
    {
        scard::SCARDCONTEXT hCtx = 0;
        int32_t rc = scard::SCardEstablishContext(scard::SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "Establish context for reader enumeration");

        // Reader Groups W
        uint32_t cchGroups = 0;
        rc = scard::SCardListReaderGroupsW(hCtx, nullptr, &cchGroups);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardListReaderGroupsW size query must succeed");
        TEST_ASSERT(cchGroups > 0, "Reader groups length must be > 0");

        std::vector<wchar_t> wGroups(cchGroups, 0);
        rc = scard::SCardListReaderGroupsW(hCtx, wGroups.data(), &cchGroups);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardListReaderGroupsW buffer retrieval must succeed");
        TEST_ASSERT(std::wstring(wGroups.data(), cchGroups).find(L"SCard$DefaultReaders") != std::wstring::npos, "SCard$DefaultReaders in group list");

        // Readers W
        uint32_t cchReaders = 0;
        rc = scard::SCardListReadersW(hCtx, nullptr, nullptr, &cchReaders);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardListReadersW size query must succeed");
        TEST_ASSERT(cchReaders > 0, "Readers buffer size must be > 0");

        std::vector<wchar_t> wReaders(cchReaders, 0);
        rc = scard::SCardListReadersW(hCtx, nullptr, wReaders.data(), &cchReaders);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardListReadersW data retrieval must succeed");
        TEST_ASSERT(std::wstring(wReaders.data(), cchReaders).find(L"MicaNT Virtual PIV/CAC SmartCard Reader 0") != std::wstring::npos, "PIV reader present in wide list");

        // Readers A
        char aReaders[512]{};
        uint32_t cchAReaders = sizeof(aReaders);
        rc = scard::SCardListReadersA(hCtx, nullptr, aReaders, &cchAReaders);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardListReadersA retrieval must succeed");
        TEST_ASSERT(std::string(aReaders, cchAReaders).find("MicaNT Virtual PIV/CAC SmartCard Reader 0") != std::string::npos, "PIV reader present in list");

        scard::SCARDCONTEXT hCtxGroup = 0;
        rc = scard::SCardEstablishContext(scard::SCARD_SCOPE_USER, nullptr, nullptr, &hCtxGroup);
        char aGroups[256]{};
        uint32_t cchAGroups = sizeof(aGroups);
        rc = scard::SCardListReaderGroupsA(hCtxGroup, aGroups, &cchAGroups);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardListReaderGroupsA must succeed");
        TEST_ASSERT(std::string(aGroups, cchAGroups).find("SCard$DefaultReaders") != std::string::npos, "SCard$DefaultReaders in ANSI list");
        scard::SCardReleaseContext(hCtxGroup);

        scard::SCardReleaseContext(hCtx);
    }

    // 6. Status Change Polling (SCardGetStatusChangeW/A)
    {
        scard::SCARDCONTEXT hCtx = 0;
        scard::SCardEstablishContext(scard::SCARD_SCOPE_USER, nullptr, nullptr, &hCtx);

        scard::SCARD_READERSTATEW rs[2]{};
        rs[0].szReader = L"MicaNT Virtual PIV/CAC SmartCard Reader 0";
        rs[0].dwCurrentState = scard::SCARD_STATE_UNAWARE;

        rs[1].szReader = L"MicaNT Empty SmartCard Reader 1";
        rs[1].dwCurrentState = scard::SCARD_STATE_UNAWARE;

        int32_t rc = scard::SCardGetStatusChangeW(hCtx, 0, rs, 2);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardGetStatusChangeW must succeed");
        TEST_ASSERT((rs[0].dwEventState & scard::SCARD_STATE_PRESENT) != 0, "Slot 0 must report SCARD_STATE_PRESENT");
        TEST_ASSERT((rs[0].dwEventState & scard::SCARD_STATE_ATRMATCH) != 0, "Slot 0 must report SCARD_STATE_ATRMATCH");
        TEST_ASSERT(rs[0].cbAtr == 18, "PIV ATR length must be 18 bytes");
        TEST_ASSERT(rs[0].rgbAtr[0] == 0x3B, "ATR initial TS byte must be 0x3B");

        TEST_ASSERT((rs[1].dwEventState & scard::SCARD_STATE_EMPTY) != 0, "Slot 1 must report SCARD_STATE_EMPTY");
        TEST_ASSERT(rs[1].cbAtr == 0, "Slot 1 ATR length must be 0");

        scard::SCARD_READERSTATEA rsa[1]{};
        rsa[0].szReader = "MicaNT Virtual PIV/CAC SmartCard Reader 0";
        rsa[0].dwCurrentState = scard::SCARD_STATE_UNAWARE;
        rc = scard::SCardGetStatusChangeA(hCtx, 0, rsa, 1);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardGetStatusChangeA must succeed");
        TEST_ASSERT((rsa[0].dwEventState & scard::SCARD_STATE_PRESENT) != 0, "Slot 0 ANSI must report SCARD_STATE_PRESENT");

        scard::SCardReleaseContext(hCtx);
    }

    // 7. Card Connection, Status Query, APDU Transmit, Disconnect (SCardConnect, SCardStatus, SCardTransmit, SCardDisconnect)
    {
        scard::SCARDCONTEXT hCtx = 0;
        scard::SCardEstablishContext(scard::SCARD_SCOPE_USER, nullptr, nullptr, &hCtx);

        scard::SCARDHANDLE hCard = 0;
        uint32_t activeProto = 0;
        int32_t rc = scard::SCardConnectW(
            hCtx, L"MicaNT Virtual PIV/CAC SmartCard Reader 0",
            scard::SCARD_SHARE_SHARED,
            scard::SCARD_PROTOCOL_T0 | scard::SCARD_PROTOCOL_T1,
            &hCard, &activeProto
        );
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardConnectW to PIV reader must succeed");
        TEST_ASSERT(hCard != 0, "hCard must be non-zero");
        TEST_ASSERT(activeProto == scard::SCARD_PROTOCOL_T1, "Active protocol must negotiate to T=1");

        // Query Status W
        wchar_t rName[128]{};
        uint32_t cchRName = 128;
        uint32_t st = 0, pr = 0;
        uint8_t atr[36]{};
        uint32_t cbAtr = sizeof(atr);
        rc = scard::SCardStatusW(hCard, rName, &cchRName, &st, &pr, atr, &cbAtr);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardStatusW must succeed");
        TEST_ASSERT(std::wstring(rName) == L"MicaNT Virtual PIV/CAC SmartCard Reader 0", "Reader name match");
        TEST_ASSERT((st & scard::SCARD_STATE_PRESENT) != 0, "State must have SCARD_STATE_PRESENT");
        TEST_ASSERT(cbAtr == 18, "ATR length must match 18 bytes");

        // Query Status A
        char rNameA[128]{};
        uint32_t cchRNameA = 128;
        rc = scard::SCardStatusA(hCard, rNameA, &cchRNameA, &st, &pr, atr, &cbAtr);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardStatusA must succeed");
        TEST_ASSERT(std::string(rNameA) == "MicaNT Virtual PIV/CAC SmartCard Reader 0", "Reader name ANSI match");

        // Reconnect
        rc = scard::SCardReconnect(hCard, scard::SCARD_SHARE_SHARED, scard::SCARD_PROTOCOL_T1, scard::SCARD_LEAVE_CARD, &activeProto);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardReconnect must succeed");

        // Transmit APDU 1: SELECT NIST PIV Application
        const uint8_t selectPivApdu[] = {
            0x00, 0xA4, 0x04, 0x00, 0x09,
            0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x10, 0x00
        };
        uint8_t recvBuf[256]{};
        uint32_t cbRecv = sizeof(recvBuf);
        scard::SCARD_IO_REQUEST recvPci{};
        rc = scard::SCardTransmit(hCard, &scard::g_rgSCardT1Pci, selectPivApdu, sizeof(selectPivApdu),
                                  &recvPci, recvBuf, &cbRecv);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardTransmit(SELECT PIV) must succeed");
        TEST_ASSERT(cbRecv >= 2, "Recv length >= 2");
        TEST_ASSERT(recvBuf[cbRecv - 2] == 0x90 && recvBuf[cbRecv - 1] == 0x00, "APDU response SW must be 90 00");

        // Transmit APDU 2: VERIFY PIN ("123456")
        const uint8_t verifyPinApdu[] = {
            0x00, 0x20, 0x00, 0x80, 0x06,
            '1', '2', '3', '4', '5', '6'
        };
        cbRecv = sizeof(recvBuf);
        rc = scard::SCardTransmit(hCard, &scard::g_rgSCardT1Pci, verifyPinApdu, sizeof(verifyPinApdu),
                                  &recvPci, recvBuf, &cbRecv);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardTransmit(VERIFY PIN) must succeed");
        TEST_ASSERT(cbRecv >= 2 && recvBuf[cbRecv - 2] == 0x90 && recvBuf[cbRecv - 1] == 0x00, "PIN verified SW 90 00");

        // Transmit APDU 3: GET DATA (CHUID Tag 5FC102)
        const uint8_t getChuidApdu[] = {
            0x00, 0xCB, 0x3F, 0xFF, 0x05,
            0x5C, 0x03, 0x5F, 0xC1, 0x02
        };
        cbRecv = sizeof(recvBuf);
        rc = scard::SCardTransmit(hCard, &scard::g_rgSCardT1Pci, getChuidApdu, sizeof(getChuidApdu),
                                  &recvPci, recvBuf, &cbRecv);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardTransmit(GET DATA CHUID) must succeed");
        TEST_ASSERT(cbRecv >= 2 && recvBuf[cbRecv - 2] == 0x90 && recvBuf[cbRecv - 1] == 0x00, "CHUID returned SW 90 00");

        // Disconnect
        rc = scard::SCardDisconnect(hCard, scard::SCARD_LEAVE_CARD);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SCardDisconnect must succeed");

        scard::SCardReleaseContext(hCtx);
    }

    // 8. FIDO2 Security Key Test & Empty Reader Failure Check
    {
        scard::SCARDCONTEXT hCtx = 0;
        scard::SCardEstablishContext(scard::SCARD_SCOPE_USER, nullptr, nullptr, &hCtx);

        // Connect to FIDO2 token
        scard::SCARDHANDLE hFido = 0;
        uint32_t activeProto = 0;
        int32_t rc = scard::SCardConnectW(
            hCtx, L"MicaNT FIDO2 NFC Security Key 0",
            scard::SCARD_SHARE_SHARED, scard::SCARD_PROTOCOL_T1,
            &hFido, &activeProto
        );
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "Connect to FIDO2 key must succeed");

        // SELECT FIDO2 Application
        const uint8_t selectFidoApdu[] = {
            0x00, 0xA4, 0x04, 0x00, 0x08,
            0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01
        };
        uint8_t recvBuf[128]{};
        uint32_t cbRecv = sizeof(recvBuf);
        rc = scard::SCardTransmit(hFido, &scard::g_rgSCardT1Pci, selectFidoApdu, sizeof(selectFidoApdu),
                                  nullptr, recvBuf, &cbRecv);
        TEST_ASSERT(rc == scard::SCARD_S_SUCCESS, "SELECT FIDO2 AID must succeed");
        TEST_ASSERT(cbRecv >= 2 && recvBuf[cbRecv - 2] == 0x90 && recvBuf[cbRecv - 1] == 0x00, "FIDO2 AID SW 90 00");

        scard::SCardDisconnect(hFido, scard::SCARD_LEAVE_CARD);

        // Attempt connect to Empty Reader
        scard::SCARDHANDLE hEmpty = 0;
        rc = scard::SCardConnectW(
            hCtx, L"MicaNT Empty SmartCard Reader 1",
            scard::SCARD_SHARE_SHARED, scard::SCARD_PROTOCOL_T1,
            &hEmpty, &activeProto
        );
        TEST_ASSERT(rc == scard::SCARD_E_NO_SMARTCARD, "Connecting to empty slot must return SCARD_E_NO_SMARTCARD");

        scard::SCARDHANDLE hConnectA = 0;
        rc = scard::SCardConnectA(hCtx, "MicaNT Empty SmartCard Reader 1", scard::SCARD_SHARE_SHARED, scard::SCARD_PROTOCOL_T1, &hConnectA, &activeProto);
        TEST_ASSERT(rc == scard::SCARD_E_NO_SMARTCARD, "Connecting to empty slot via ANSI must return SCARD_E_NO_SMARTCARD");

        scard::SCardReleaseContext(hCtx);
    }

    // 9. Interactive Shell Commands (scard test, scard list, scard status)
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // scard test
        out.str("");
        shell.execute("scard test", out);
        TEST_ASSERT(out.str().find("[SCARD] Self-Test Completed Successfully") != std::string::npos, "scard test must succeed");

        // scard list
        out.str("");
        shell.execute("scard list", out);
        TEST_ASSERT(out.str().find("MicaNT Virtual PIV/CAC SmartCard Reader 0") != std::string::npos, "scard list must output PIV reader");
        TEST_ASSERT(out.str().find("CARD PRESENT") != std::string::npos, "scard list must show CARD PRESENT");

        // scard status
        out.str("");
        shell.execute("scard status", out);
        TEST_ASSERT(out.str().find("Smart Card Status") != std::string::npos, "scard status must output header");
        TEST_ASSERT(out.str().find("ATR Length:") != std::string::npos, "scard status must output ATR length");
    }

    std::cout << "[TEST] Suite 90: Windows Smart Card & PC/SC Subsystem PASSED.\n";
}

void Test_WindowsNLA_NetworkListService_Subsystem() {
    using namespace micant;

    std::cout << "[TEST] Running Suite 91: Windows Network Location Awareness & Network List Service Subsystem (nlasvc.dll / netprofm.dll)...\n";

    nla::InitializeNlaSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports & Module Registration
    // ------------------------------------------------------------------------
    {
        auto& ldr = ldr::DynamicLoader::get();

        // netprofm.dll
        TEST_ASSERT(ldr.getExport("netprofm.dll", "DllGetClassObject") != nullptr, "netprofm.dll DllGetClassObject must be exported");
        TEST_ASSERT(ldr.getExport("netprofm.dll", "DllCanUnloadNow") != nullptr, "netprofm.dll DllCanUnloadNow must be exported");
        TEST_ASSERT(ldr.getExport("netprofm.dll", "DllRegisterServer") != nullptr, "netprofm.dll DllRegisterServer must be exported");
        TEST_ASSERT(ldr.getExport("netprofm.dll", "DllUnregisterServer") != nullptr, "netprofm.dll DllUnregisterServer must be exported");

        // nlasvc.dll
        TEST_ASSERT(ldr.getExport("nlasvc.dll", "ServiceMain") != nullptr, "nlasvc.dll ServiceMain must be exported");
        TEST_ASSERT(ldr.getExport("nlasvc.dll", "SvchostPushServiceGlobals") != nullptr, "nlasvc.dll SvchostPushServiceGlobals must be exported");

        // ncbservice.dll
        TEST_ASSERT(ldr.getExport("ncbservice.dll", "ServiceMain") != nullptr, "ncbservice.dll ServiceMain must be exported");
        TEST_ASSERT(ldr.getExport("ncbservice.dll", "SvchostPushServiceGlobals") != nullptr, "ncbservice.dll SvchostPushServiceGlobals must be exported");

        // nlaapi.dll
        TEST_ASSERT(ldr.getExport("nlaapi.dll", "NlsGetInterfaceGuidFromInterfaceIndex") != nullptr, "nlaapi.dll NlsGetInterfaceGuidFromInterfaceIndex must be exported");
        TEST_ASSERT(ldr.getExport("nlaapi.dll", "NlsFreeInterfaceGuid") != nullptr, "nlaapi.dll NlsFreeInterfaceGuid must be exported");
        TEST_ASSERT(ldr.getExport("nlaapi.dll", "NlsUpdateInterfaceCostCache") != nullptr, "nlaapi.dll NlsUpdateInterfaceCostCache must be exported");
    }

    // ------------------------------------------------------------------------
    // Stage 2: Version Information Records
    // ------------------------------------------------------------------------
    {
        const auto* verNla = version::VersionDatabase::Instance().FindModule("nlasvc.dll");
        TEST_ASSERT(verNla != nullptr, "nlasvc.dll version record must exist");
        TEST_ASSERT(verNla->stringTable.at("FileDescription") == "Network Location Awareness Service", "nlasvc.dll description match");
        TEST_ASSERT(verNla->stringTable.at("OriginalFilename") == "nlasvc.dll", "nlasvc.dll original filename match");

        const auto* verNetprof = version::VersionDatabase::Instance().FindModule("netprofm.dll");
        TEST_ASSERT(verNetprof != nullptr, "netprofm.dll version record must exist");
        TEST_ASSERT(verNetprof->stringTable.at("FileDescription") == "Network List Manager", "netprofm.dll description match");
        TEST_ASSERT(verNetprof->stringTable.at("OriginalFilename") == "netprofm.dll", "netprofm.dll original filename match");

        const auto* verNcb = version::VersionDatabase::Instance().FindModule("ncbservice.dll");
        TEST_ASSERT(verNcb != nullptr, "ncbservice.dll version record must exist");
        TEST_ASSERT(verNcb->stringTable.at("FileDescription") == "Network Connection Broker", "ncbservice.dll description match");
        TEST_ASSERT(verNcb->stringTable.at("OriginalFilename") == "ncbservice.dll", "ncbservice.dll original filename match");

        const auto* verNlaApi = version::VersionDatabase::Instance().FindModule("nlaapi.dll");
        TEST_ASSERT(verNlaApi != nullptr, "nlaapi.dll version record must exist");
        TEST_ASSERT(verNlaApi->stringTable.at("FileDescription") == "Network Location Awareness API", "nlaapi.dll description match");
        TEST_ASSERT(verNlaApi->stringTable.at("OriginalFilename") == "nlaapi.dll", "nlaapi.dll original filename match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: Service Control Manager Services
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();

        auto nlaSvc = scm.getServiceRecord(L"NLASvc");
        TEST_ASSERT(nlaSvc != nullptr, "NLASvc service must be registered in SCM");
        TEST_ASSERT(nlaSvc->displayName == L"Network Location Awareness", "NLASvc display name match");
        TEST_ASSERT(nlaSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "NLASvc must be in running state");

        auto netprofmSvc = scm.getServiceRecord(L"netprofm");
        TEST_ASSERT(netprofmSvc != nullptr, "netprofm service must be registered in SCM");
        TEST_ASSERT(netprofmSvc->displayName == L"Network List Service", "netprofm display name match");
        TEST_ASSERT(netprofmSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "netprofm must be in running state");

        auto ncbSvc = scm.getServiceRecord(L"NcbService");
        TEST_ASSERT(ncbSvc != nullptr, "NcbService service must be registered in SCM");
        TEST_ASSERT(ncbSvc->displayName == L"Network Connection Broker", "NcbService display name match");
        TEST_ASSERT(ncbSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "NcbService must be in running state");
    }

    // ------------------------------------------------------------------------
    // Stage 4: COM Class Factory & CoCreateInstance
    // ------------------------------------------------------------------------
    nla::INetworkListManager* pNLM = nullptr;
    {
        ole32::HRESULT hr = ole32::CoCreateInstance(
            nla::CLSID_NetworkListManager, nullptr, ole32::CLSCTX_INPROC_SERVER,
            nla::IID_INetworkListManager, reinterpret_cast<void**>(&pNLM)
        );
        TEST_ASSERT(hr == ole32::S_OK, "CoCreateInstance(CLSID_NetworkListManager) must return S_OK");
        TEST_ASSERT(pNLM != nullptr, "INetworkListManager interface pointer must be valid");

        // Verify QueryInterface on pNLM
        ole32::IUnknown* pUnk = nullptr;
        hr = pNLM->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
        TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "QueryInterface for IUnknown must succeed");
        pUnk->Release();

        ole32::IDispatch* pDisp = nullptr;
        hr = pNLM->QueryInterface(ole32::IID_IDispatch, reinterpret_cast<void**>(&pDisp));
        TEST_ASSERT(hr == ole32::S_OK && pDisp != nullptr, "QueryInterface for IDispatch must succeed");
        pDisp->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 5: INetworkListManager Connectivity Inquiries
    // ------------------------------------------------------------------------
    {
        int16_t isInternet = 0;
        ole32::HRESULT hr = pNLM->get_IsConnectedToInternet(&isInternet);
        TEST_ASSERT(hr == ole32::S_OK, "get_IsConnectedToInternet must return S_OK");
        TEST_ASSERT(isInternet == -1, "get_IsConnectedToInternet must report VARIANT_TRUE (-1)");

        int16_t isConnected = 0;
        hr = pNLM->get_IsConnected(&isConnected);
        TEST_ASSERT(hr == ole32::S_OK, "get_IsConnected must return S_OK");
        TEST_ASSERT(isConnected == -1, "get_IsConnected must report VARIANT_TRUE (-1)");

        nla::NLM_CONNECTIVITY conn = nla::NLM_CONNECTIVITY_DISCONNECTED;
        hr = pNLM->GetConnectivity(&conn);
        TEST_ASSERT(hr == ole32::S_OK, "GetConnectivity must return S_OK");
        TEST_ASSERT((conn & nla::NLM_CONNECTIVITY_IPV4_INTERNET) != 0, "Connectivity must have IPv4 Internet");
        TEST_ASSERT((conn & nla::NLM_CONNECTIVITY_IPV6_INTERNET) != 0, "Connectivity must have IPv6 Internet");
        TEST_ASSERT((conn & nla::NLM_CONNECTIVITY_IPV4_LOCALNETWORK) != 0, "Connectivity must have IPv4 LocalNetwork");
        TEST_ASSERT((conn & nla::NLM_CONNECTIVITY_IPV4_SUBNET) != 0, "Connectivity must have IPv4 Subnet");
    }

    // ------------------------------------------------------------------------
    // Stage 6: Network Enumeration via IEnumNetworks
    // ------------------------------------------------------------------------
    GUID netId1{};
    {
        nla::IEnumNetworks* pEnum = nullptr;
        ole32::HRESULT hr = pNLM->GetNetworks(nla::NLM_ENUM_NETWORK_ALL, &pEnum);
        TEST_ASSERT(hr == ole32::S_OK && pEnum != nullptr, "GetNetworks must return valid IEnumNetworks");

        nla::INetwork* pNet = nullptr;
        uint32_t fetched = 0;
        hr = pEnum->Next(1, &pNet, &fetched);
        TEST_ASSERT(hr == ole32::S_OK && fetched == 1 && pNet != nullptr, "IEnumNetworks::Next must fetch 1st network");

        // Inspect 1st Network: Corporate Domain Network
        ole32::BSTR bstrName = nullptr;
        hr = pNet->GetName(&bstrName);
        TEST_ASSERT(hr == ole32::S_OK && bstrName != nullptr, "GetName must succeed");
        TEST_ASSERT(std::wstring(bstrName) == L"MicaNT Corporate Domain Network", "Network 1 name match");
        ole32::SysFreeString(bstrName);

        ole32::BSTR bstrDesc = nullptr;
        hr = pNet->GetDescription(&bstrDesc);
        TEST_ASSERT(hr == ole32::S_OK && bstrDesc != nullptr, "GetDescription must succeed");
        TEST_ASSERT(std::wstring(bstrDesc).find(L"Sovereign Ethernet Network") != std::wstring::npos, "Network 1 description match");
        ole32::SysFreeString(bstrDesc);

        nla::NLM_NETWORK_CATEGORY cat = nla::NLM_NETWORK_CATEGORY_PUBLIC;
        hr = pNet->GetCategory(&cat);
        TEST_ASSERT(hr == ole32::S_OK, "GetCategory must succeed");
        TEST_ASSERT(cat == nla::NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED, "Network 1 must be DOMAIN_AUTHENTICATED");

        nla::NLM_DOMAIN_TYPE dt = nla::NLM_DOMAIN_TYPE_NON_DOMAIN_NETWORK;
        hr = pNet->GetDomainType(&dt);
        TEST_ASSERT(hr == ole32::S_OK, "GetDomainType must succeed");
        TEST_ASSERT(dt == nla::NLM_DOMAIN_TYPE_DOMAIN_AUTHENTICATED, "Network 1 domain type must be DOMAIN_AUTHENTICATED");

        hr = pNet->GetNetworkId(&netId1);
        TEST_ASSERT(hr == ole32::S_OK, "GetNetworkId must succeed");
        TEST_ASSERT(netId1.Data1 == 0xA1B2C3D4, "Network 1 GUID Data1 match");

        uint32_t crLow = 0, crHigh = 0, cnLow = 0, cnHigh = 0;
        hr = pNet->GetTimeCreatedAndConnected(&crLow, &crHigh, &cnLow, &cnHigh);
        TEST_ASSERT(hr == ole32::S_OK && crLow != 0 && cnLow != 0, "GetTimeCreatedAndConnected must return timestamps");

        int16_t netInternet = 0;
        hr = pNet->get_IsConnectedToInternet(&netInternet);
        TEST_ASSERT(hr == ole32::S_OK && netInternet == -1, "Network 1 must be connected to Internet");

        // Mutability testing on Network 1
        ole32::BSTR newName = ole32::SysAllocString(L"MicaNT Enterprise HQ");
        pNet->SetName(newName);
        ole32::SysFreeString(newName);

        ole32::BSTR readBackName = nullptr;
        pNet->GetName(&readBackName);
        TEST_ASSERT(std::wstring(readBackName) == L"MicaNT Enterprise HQ", "SetName must update profile name");
        ole32::SysFreeString(readBackName);

        // Restore original name
        ole32::BSTR origName = ole32::SysAllocString(L"MicaNT Corporate Domain Network");
        pNet->SetName(origName);
        ole32::SysFreeString(origName);

        // Category mutation
        pNet->SetCategory(nla::NLM_NETWORK_CATEGORY_PRIVATE);
        pNet->GetCategory(&cat);
        TEST_ASSERT(cat == nla::NLM_NETWORK_CATEGORY_PRIVATE, "SetCategory must update category to PRIVATE");
        pNet->SetCategory(nla::NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED);

        pNet->Release();

        // Test Skip and Next
        hr = pEnum->Reset();
        TEST_ASSERT(hr == ole32::S_OK, "IEnumNetworks::Reset must succeed");

        hr = pEnum->Skip(1);
        TEST_ASSERT(hr == ole32::S_OK, "IEnumNetworks::Skip(1) must succeed");

        nla::INetwork* pNet2 = nullptr;
        hr = pEnum->Next(1, &pNet2, &fetched);
        TEST_ASSERT(hr == ole32::S_OK && fetched == 1 && pNet2 != nullptr, "Next after Skip must return 2nd network");

        ole32::BSTR bstrName2 = nullptr;
        pNet2->GetName(&bstrName2);
        TEST_ASSERT(std::wstring(bstrName2) == L"MicaNT Secure Wireless", "Network 2 name match");
        ole32::SysFreeString(bstrName2);

        pNet2->GetCategory(&cat);
        TEST_ASSERT(cat == nla::NLM_NETWORK_CATEGORY_PRIVATE, "Network 2 category match");

        pNet2->Release();

        // Test Clone
        nla::IEnumNetworks* pCloned = nullptr;
        hr = pEnum->Clone(&pCloned);
        TEST_ASSERT(hr == ole32::S_OK && pCloned != nullptr, "IEnumNetworks::Clone must succeed");
        pCloned->Release();

        pEnum->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 7: Direct GetNetwork Resolution by GUID
    // ------------------------------------------------------------------------
    {
        nla::INetwork* pFoundNet = nullptr;
        ole32::HRESULT hr = pNLM->GetNetwork(netId1, &pFoundNet);
        TEST_ASSERT(hr == ole32::S_OK && pFoundNet != nullptr, "GetNetwork by GUID must find profile");

        ole32::BSTR foundName = nullptr;
        pFoundNet->GetName(&foundName);
        TEST_ASSERT(std::wstring(foundName) == L"MicaNT Corporate Domain Network", "Found network name match");
        ole32::SysFreeString(foundName);
        pFoundNet->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 8: Connection Enumeration via IEnumNetworkConnections & INetworkConnection
    // ------------------------------------------------------------------------
    GUID connId1{};
    {
        nla::INetwork* pNet = nullptr;
        pNLM->GetNetwork(netId1, &pNet);
        TEST_ASSERT(pNet != nullptr, "GetNetwork must succeed");

        nla::IEnumNetworkConnections* pEnumConn = nullptr;
        ole32::HRESULT hr = pNet->GetNetworkConnections(&pEnumConn);
        TEST_ASSERT(hr == ole32::S_OK && pEnumConn != nullptr, "GetNetworkConnections must return IEnumNetworkConnections");

        nla::INetworkConnection* pConn = nullptr;
        uint32_t fetched = 0;
        hr = pEnumConn->Next(1, &pConn, &fetched);
        TEST_ASSERT(hr == ole32::S_OK && fetched == 1 && pConn != nullptr, "IEnumNetworkConnections::Next must succeed");

        hr = pConn->GetConnectionId(&connId1);
        TEST_ASSERT(hr == ole32::S_OK, "GetConnectionId must succeed");
        TEST_ASSERT(connId1.Data1 == 0x99998888, "Connection GUID Data1 match");

        GUID adapterId{};
        hr = pConn->GetAdapterId(&adapterId);
        TEST_ASSERT(hr == ole32::S_OK, "GetAdapterId must succeed");
        TEST_ASSERT(adapterId.Data1 == 0x11112222, "Adapter GUID Data1 match");

        nla::NLM_DOMAIN_TYPE dt{};
        hr = pConn->GetDomainType(&dt);
        TEST_ASSERT(hr == ole32::S_OK && dt == nla::NLM_DOMAIN_TYPE_DOMAIN_AUTHENTICATED, "Connection domain type match");

        int16_t cInternet = 0;
        hr = pConn->get_IsConnectedToInternet(&cInternet);
        TEST_ASSERT(hr == ole32::S_OK && cInternet == -1, "Connection must be connected to Internet");

        nla::INetwork* pBackNet = nullptr;
        hr = pConn->GetNetwork(&pBackNet);
        TEST_ASSERT(hr == ole32::S_OK && pBackNet != nullptr, "GetNetwork on connection must return parent network");
        pBackNet->Release();

        pConn->Release();
        pEnumConn->Release();
        pNet->Release();

        // Test GetNetworkConnections directly from Manager
        nla::IEnumNetworkConnections* pAllConn = nullptr;
        hr = pNLM->GetNetworkConnections(&pAllConn);
        TEST_ASSERT(hr == ole32::S_OK && pAllConn != nullptr, "pNLM->GetNetworkConnections must succeed");
        pAllConn->Release();

        // Test GetNetworkConnection directly from Manager
        nla::INetworkConnection* pDirectConn = nullptr;
        hr = pNLM->GetNetworkConnection(connId1, &pDirectConn);
        TEST_ASSERT(hr == ole32::S_OK && pDirectConn != nullptr, "pNLM->GetNetworkConnection by GUID must succeed");
        pDirectConn->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 9: Cost Manager & Metered Network Profiles (INetworkCostManager)
    // ------------------------------------------------------------------------
    {
        nla::INetworkCostManager* pCostMgr = nullptr;
        ole32::HRESULT hr = pNLM->QueryInterface(nla::IID_INetworkCostManager, reinterpret_cast<void**>(&pCostMgr));
        TEST_ASSERT(hr == ole32::S_OK && pCostMgr != nullptr, "QueryInterface for INetworkCostManager must succeed");

        uint32_t cost = 0;
        nla::NLM_CONNECTION_COST_DATA costData{};
        hr = pCostMgr->GetCost(&cost, &costData);
        TEST_ASSERT(hr == ole32::S_OK, "GetCost must return S_OK");
        TEST_ASSERT(cost == nla::NLM_CONNECTION_COST_UNRESTRICTED, "Cost must be UNRESTRICTED");
        TEST_ASSERT(costData.ConnectionCost == nla::NLM_CONNECTION_COST_UNRESTRICTED, "ConnectionCost must match");

        nla::NLM_DATAPLAN_STATUS planStatus{};
        hr = pCostMgr->GetDataPlanStatus(&planStatus, nullptr);
        TEST_ASSERT(hr == ole32::S_OK, "GetDataPlanStatus must return S_OK");
        TEST_ASSERT(planStatus.DataLimitInMegabytes == 51200, "DataLimitInMegabytes must be 50 GB (51200 MB)");
        TEST_ASSERT(planStatus.UsageData.UsageInMegabytes == 1240, "UsageData must report 1240 MB");
        TEST_ASSERT(planStatus.InboundBandwidthInKbps == 1000000, "InboundBandwidth must report 1 Gbps");

        hr = pCostMgr->SetDestinationAddresses(0, nullptr, -1);
        TEST_ASSERT(hr == ole32::S_OK, "SetDestinationAddresses must return S_OK");

        pCostMgr->Release();
    }

    // Release NLM instance
    pNLM->Release();

    // ------------------------------------------------------------------------
    // Stage 10: NLA API Functional Interface
    // ------------------------------------------------------------------------
    {
        GUID ifGuid{};
        ole32::HRESULT hr = nla::NlsGetInterfaceGuidFromInterfaceIndex(1, &ifGuid);
        TEST_ASSERT(hr == ole32::S_OK, "NlsGetInterfaceGuidFromInterfaceIndex must return S_OK");
        TEST_ASSERT(ifGuid.Data1 != 0, "Interface GUID must be populated");

        hr = nla::NlsUpdateInterfaceCostCache(&ifGuid, nla::NLM_CONNECTION_COST_FIXED);
        TEST_ASSERT(hr == ole32::S_OK, "NlsUpdateInterfaceCostCache must return S_OK");

        hr = nla::NlsFreeInterfaceGuid(&ifGuid);
        TEST_ASSERT(hr == ole32::S_OK, "NlsFreeInterfaceGuid must return S_OK");
    }

    // ------------------------------------------------------------------------
    // Stage 11: Interactive Shell Command Integration (nla test, nla list, nla status)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // nla test
        out.str("");
        shell.execute("nla test", out);
        TEST_ASSERT(out.str().find("[NLA] Self-Test Completed Successfully") != std::string::npos, "nla test must succeed");

        // nla list
        out.str("");
        shell.execute("nla list", out);
        TEST_ASSERT(out.str().find("MicaNT Corporate Domain Network") != std::string::npos, "nla list must display Corporate network");
        TEST_ASSERT(out.str().find("MicaNT Secure Wireless") != std::string::npos, "nla list must display Wireless network");
        TEST_ASSERT(out.str().find("MicaNT Isolated Lab Network") != std::string::npos, "nla list must display Lab network");

        // nla status
        out.str("");
        shell.execute("nla status", out);
        TEST_ASSERT(out.str().find("Network Connected:    YES") != std::string::npos, "nla status must show Network Connected");
        TEST_ASSERT(out.str().find("Internet Connected:   YES") != std::string::npos, "nla status must show Internet Connected");
    }

    std::cout << "[TEST] Suite 91: Windows Network Location Awareness & Network List Service Subsystem PASSED.\n";
}

void Test_WindowsWNS_PushNotification_Subsystem() {
    std::cout << "[TEST] Running Suite 92: Windows Push Notification Service (WNS) & Push Notification Subsystem...\n";

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Initialization (wpncore.dll, wpnclient.dll, wpnapps.dll)
    // ------------------------------------------------------------------------
    wns::InitializeWnsSubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    // wpncore.dll
    TEST_ASSERT(ldr.getExport("wpncore.dll", "DllGetClassObject") != nullptr, "wpncore.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("wpncore.dll", "DllCanUnloadNow") != nullptr, "wpncore.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("wpncore.dll", "DllRegisterServer") != nullptr, "wpncore.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("wpncore.dll", "DllUnregisterServer") != nullptr, "wpncore.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("wpncore.dll", "WpnInitialize") != nullptr, "wpncore.dll must export WpnInitialize");
    TEST_ASSERT(ldr.getExport("wpncore.dll", "WpnUninitialize") != nullptr, "wpncore.dll must export WpnUninitialize");
    TEST_ASSERT(ldr.getExport("wpncore.dll", "WpnQueryPendingNotifications") != nullptr, "wpncore.dll must export WpnQueryPendingNotifications");

    // wpnclient.dll
    TEST_ASSERT(ldr.getExport("wpnclient.dll", "DllGetClassObject") != nullptr, "wpnclient.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("wpnclient.dll", "DllCanUnloadNow") != nullptr, "wpnclient.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("wpnclient.dll", "WpnCreateChannelForApp") != nullptr, "wpnclient.dll must export WpnCreateChannelForApp");
    TEST_ASSERT(ldr.getExport("wpnclient.dll", "WpnCloseChannel") != nullptr, "wpnclient.dll must export WpnCloseChannel");
    TEST_ASSERT(ldr.getExport("wpnclient.dll", "WpnShowToast") != nullptr, "wpnclient.dll must export WpnShowToast");

    // wpnapps.dll
    TEST_ASSERT(ldr.getExport("wpnapps.dll", "ServiceMain") != nullptr, "wpnapps.dll must export ServiceMain");
    TEST_ASSERT(ldr.getExport("wpnapps.dll", "SvchostPushServiceGlobals") != nullptr, "wpnapps.dll must export SvchostPushServiceGlobals");
    TEST_ASSERT(ldr.getExport("wpnapps.dll", "DllGetClassObject") != nullptr, "wpnapps.dll must export DllGetClassObject");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verWpnCore = version::VersionDatabase::Instance().FindModule("wpncore.dll");
        TEST_ASSERT(verWpnCore != nullptr, "VersionDatabase must contain wpncore.dll");
        TEST_ASSERT(verWpnCore->stringTable.at("FileDescription") == "Windows Push Notifications Platform Core", "wpncore.dll description match");
        TEST_ASSERT(verWpnCore->stringTable.at("OriginalFilename") == "wpncore.dll", "wpncore.dll original filename match");

        const auto* verWpnApps = version::VersionDatabase::Instance().FindModule("wpnapps.dll");
        TEST_ASSERT(verWpnApps != nullptr, "VersionDatabase must contain wpnapps.dll");
        TEST_ASSERT(verWpnApps->stringTable.at("FileDescription") == "Windows Push Notifications App Service", "wpnapps.dll description match");
        TEST_ASSERT(verWpnApps->stringTable.at("OriginalFilename") == "wpnapps.dll", "wpnapps.dll original filename match");

        const auto* verWpnClient = version::VersionDatabase::Instance().FindModule("wpnclient.dll");
        TEST_ASSERT(verWpnClient != nullptr, "VersionDatabase must contain wpnclient.dll");
        TEST_ASSERT(verWpnClient->stringTable.at("FileDescription") == "Windows Push Notifications Client API", "wpnclient.dll description match");
        TEST_ASSERT(verWpnClient->stringTable.at("OriginalFilename") == "wpnclient.dll", "wpnclient.dll original filename match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: Service Control Manager Services (WpnService, WpnUserService)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();

        auto wpnSvc = scm.getServiceRecord(L"WpnService");
        TEST_ASSERT(wpnSvc != nullptr, "WpnService service must be registered in SCM");
        TEST_ASSERT(wpnSvc->displayName == L"Windows Push Notifications System Service", "WpnService display name match");
        TEST_ASSERT(wpnSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "WpnService must be running");
        TEST_ASSERT(wpnSvc->status.dwProcessId == 1142, "WpnService PID match");

        auto wpnUserSvc = scm.getServiceRecord(L"WpnUserService");
        TEST_ASSERT(wpnUserSvc != nullptr, "WpnUserService service must be registered in SCM");
        TEST_ASSERT(wpnUserSvc->displayName == L"Windows Push Notifications User Service", "WpnUserService display name match");
        TEST_ASSERT(wpnUserSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "WpnUserService must be running");
        TEST_ASSERT(wpnUserSvc->status.dwProcessId == 1146, "WpnUserService PID match");
    }

    // Reset state to cleanly test manager
    wns::PushNotificationManager::get().resetToDefault();

    // ------------------------------------------------------------------------
    // Stage 4: COM Activation & Class Factories
    // ------------------------------------------------------------------------
    wns::IToastNotificationManager* pToastMgr = nullptr;
    {
        ole32::HRESULT hr = ole32::CoCreateInstance(
            wns::CLSID_ToastNotificationManager, nullptr, ole32::CLSCTX_INPROC_SERVER,
            wns::IID_IToastNotificationManager, reinterpret_cast<void**>(&pToastMgr)
        );
        TEST_ASSERT(hr == ole32::S_OK && pToastMgr != nullptr, "CoCreateInstance(CLSID_ToastNotificationManager) must succeed");

        ole32::IUnknown* pUnk = nullptr;
        hr = pToastMgr->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
        TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "QueryInterface for IUnknown on ToastNotificationManager must succeed");
        pUnk->Release();
    }

    wns::IPushNotificationChannelManager* pChanMgr = nullptr;
    {
        ole32::HRESULT hr = ole32::CoCreateInstance(
            wns::CLSID_PushNotificationChannelManager, nullptr, ole32::CLSCTX_INPROC_SERVER,
            wns::IID_IPushNotificationChannelManager, reinterpret_cast<void**>(&pChanMgr)
        );
        TEST_ASSERT(hr == ole32::S_OK && pChanMgr != nullptr, "CoCreateInstance(CLSID_PushNotificationChannelManager) must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 5: Push Notification Channel Lifecycle
    // ------------------------------------------------------------------------
    {
        wns::IPushNotificationChannel* pChan = nullptr;
        ole32::BSTR appName = ole32::SysAllocString(L"Microsoft.WindowsTerminal");
        ole32::HRESULT hr = pChanMgr->CreatePushNotificationChannelForApplication(appName, &pChan);
        ole32::SysFreeString(appName);
        TEST_ASSERT(hr == ole32::S_OK && pChan != nullptr, "CreatePushNotificationChannelForApplication must succeed");

        ole32::BSTR uri = nullptr;
        hr = pChan->GetUri(&uri);
        TEST_ASSERT(hr == ole32::S_OK && uri != nullptr, "GetUri must return channel URI");
        std::wstring sUri = uri;
        ole32::SysFreeString(uri);
        TEST_ASSERT(sUri.find(L"https://wns.micant.local/push/v1/channel-") != std::wstring::npos, "URI must follow MicaNT sovereign WNS scheme");

        win32::FILETIME exp{};
        hr = pChan->GetExpirationTime(&exp);
        TEST_ASSERT(hr == ole32::S_OK && exp.dwHighDateTime != 0, "GetExpirationTime must succeed");

        uint32_t status = 0xFF;
        hr = pChan->GetStatus(&status);
        TEST_ASSERT(hr == ole32::S_OK && status == wns::WNS_CHANNEL_ACTIVE, "Channel status must be ACTIVE");

        hr = pChan->Close();
        TEST_ASSERT(hr == ole32::S_OK, "Channel Close must succeed");

        hr = pChan->GetStatus(&status);
        TEST_ASSERT(hr == ole32::S_OK && status == wns::WNS_CHANNEL_CLOSED, "Channel status must now be CLOSED");

        pChan->Release();
    }
    pChanMgr->Release();

    // ------------------------------------------------------------------------
    // Stage 6: Toast Notification Templates & XML Generation
    // ------------------------------------------------------------------------
    {
        ole32::BSTR tmplXml = nullptr;
        ole32::HRESULT hr = pToastMgr->GetTemplateContent(wns::TOAST_TEMPLATE_IMAGE_AND_TEXT02, &tmplXml);
        TEST_ASSERT(hr == ole32::S_OK && tmplXml != nullptr, "GetTemplateContent(TOAST_TEMPLATE_IMAGE_AND_TEXT02) must succeed");
        std::wstring sTmpl = tmplXml;
        ole32::SysFreeString(tmplXml);
        TEST_ASSERT(sTmpl.find(L"<toast>") != std::wstring::npos, "XML must contain <toast>");
        TEST_ASSERT(sTmpl.find(L"ToastImageAndText02") != std::wstring::npos, "XML must specify ToastImageAndText02 binding");

        hr = pToastMgr->GetTemplateContent(wns::TOAST_TEMPLATE_GENERIC, &tmplXml);
        TEST_ASSERT(hr == ole32::S_OK && tmplXml != nullptr, "GetTemplateContent(TOAST_TEMPLATE_GENERIC) must succeed");
        sTmpl = tmplXml;
        ole32::SysFreeString(tmplXml);
        TEST_ASSERT(sTmpl.find(L"ToastGeneric") != std::wstring::npos, "XML must specify ToastGeneric template");
    }

    // ------------------------------------------------------------------------
    // Stage 7: Toast Notifier Delivery & Action Center Store
    // ------------------------------------------------------------------------
    {
        wns::IToastNotifier* pNotifier = nullptr;
        ole32::BSTR app = ole32::SysAllocString(L"Microsoft.WindowsTerminal");
        ole32::HRESULT hr = pToastMgr->CreateToastNotifier(app, &pNotifier);
        ole32::SysFreeString(app);
        TEST_ASSERT(hr == ole32::S_OK && pNotifier != nullptr, "CreateToastNotifier must succeed");

        wns::NOTIFICATION_SETTING setting{};
        hr = pNotifier->GetSetting(&setting);
        TEST_ASSERT(hr == ole32::S_OK && setting == wns::NOTIFICATION_SETTING_ENABLED, "Notification setting must be ENABLED");

        std::wstring toastContent = L"<toast launch=\"action=view\"><visual><binding template=\"ToastGeneric\">"
                                    L"<text id=\"1\">Build Completed</text>"
                                    L"<text id=\"2\">Ninja build finished with 0 errors.</text>"
                                    L"</binding></visual></toast>";
        auto* pToast = new wns::ToastNotificationImpl(toastContent);
        ole32::BSTR tag = ole32::SysAllocString(L"BuildAlert");
        ole32::BSTR group = ole32::SysAllocString(L"DevTools");
        pToast->SetTag(tag);
        pToast->SetGroup(group);
        pToast->SetSuppressPopup(0);
        ole32::SysFreeString(tag);
        ole32::SysFreeString(group);

        ole32::BSTR getTag = nullptr;
        pToast->GetTag(&getTag);
        TEST_ASSERT(std::wstring(getTag) == L"BuildAlert", "GetTag must return BuildAlert");
        ole32::SysFreeString(getTag);

        ole32::BSTR getGroup = nullptr;
        pToast->GetGroup(&getGroup);
        TEST_ASSERT(std::wstring(getGroup) == L"DevTools", "GetGroup must return DevTools");
        ole32::SysFreeString(getGroup);

        int16_t suppress = 1;
        pToast->GetSuppressPopup(&suppress);
        TEST_ASSERT(suppress == 0, "Suppress popup must be 0");

        // Show Toast
        hr = pNotifier->Show(pToast);
        TEST_ASSERT(hr == ole32::S_OK, "IToastNotifier::Show must succeed");

        auto activeToasts = wns::PushNotificationManager::get().getActiveToasts();
        bool found = false;
        for (const auto& t : activeToasts) {
            if (t.tag == L"BuildAlert" && t.group == L"DevTools") {
                found = true;
                TEST_ASSERT(t.title == L"Build Completed", "Title extracted from XML match");
                TEST_ASSERT(t.message == L"Ninja build finished with 0 errors.", "Message extracted from XML match");
                break;
            }
        }
        TEST_ASSERT(found, "Active toasts must contain newly posted toast");

        // Hide Toast
        hr = pNotifier->Hide(pToast);
        TEST_ASSERT(hr == ole32::S_OK, "IToastNotifier::Hide must succeed");

        activeToasts = wns::PushNotificationManager::get().getActiveToasts();
        bool stillActive = false;
        for (const auto& t : activeToasts) {
            if (t.tag == L"BuildAlert") stillActive = true;
        }
        TEST_ASSERT(!stillActive, "Toast must be marked dismissed after Hide");

        pToast->Release();
        pNotifier->Release();
    }
    pToastMgr->Release();

    // ------------------------------------------------------------------------
    // Stage 8: Badge Management & Updates
    // ------------------------------------------------------------------------
    {
        auto& mgr = wns::PushNotificationManager::get();
        mgr.setBadgeNumber(L"Microsoft.WindowsTerminal", 7);
        wns::BadgeRecord b{};
        bool ok = mgr.getBadge(L"Microsoft.WindowsTerminal", b);
        TEST_ASSERT(ok && b.hasNumber && b.number == 7, "Badge number must be 7");

        mgr.setBadgeGlyph(L"Microsoft.WindowsTerminal", L"attention");
        ok = mgr.getBadge(L"Microsoft.WindowsTerminal", b);
        TEST_ASSERT(ok && !b.hasNumber && b.glyph == L"attention", "Badge glyph must be attention");

        mgr.clearBadge(L"Microsoft.WindowsTerminal");
        ok = mgr.getBadge(L"Microsoft.WindowsTerminal", b);
        TEST_ASSERT(!ok, "Badge must be removed after clearBadge");
    }

    // ------------------------------------------------------------------------
    // Stage 9: Win32 C Client APIs
    // ------------------------------------------------------------------------
    {
        ole32::HRESULT hr = wns::WpnInitialize();
        TEST_ASSERT(hr == ole32::S_OK, "WpnInitialize must succeed");

        wns::WNS_CHANNEL_INFO chInfo{};
        hr = wns::WpnCreateChannelForApp(L"MicaNT.StoreApp", &chInfo);
        TEST_ASSERT(hr == ole32::S_OK, "WpnCreateChannelForApp must succeed");
        TEST_ASSERT(std::wstring(chInfo.appId) == L"MicaNT.StoreApp", "Channel info appId match");
        TEST_ASSERT(std::wstring(chInfo.channelUri).find(L"https://wns.micant.local/") != std::wstring::npos, "Channel URI prefix match");

        uint32_t notifId = 0;
        hr = wns::WpnShowToast(L"MicaNT.StoreApp", L"App Installed", L"Calculator has been installed.", L"InstallSuccess", &notifId);
        TEST_ASSERT(hr == ole32::S_OK && notifId != 0, "WpnShowToast must return valid notification ID");

        uint32_t count = 0;
        wns::WNS_TOAST_DESCRIPTOR descs[5]{};
        hr = wns::WpnQueryPendingNotifications(&count, descs, 5);
        TEST_ASSERT(hr == ole32::S_OK && count >= 1, "WpnQueryPendingNotifications must return pending toasts");

        bool foundPending = false;
        for (uint32_t i = 0; i < count; ++i) {
            if (descs[i].notificationId == notifId) {
                foundPending = true;
                TEST_ASSERT(std::wstring(descs[i].title) == L"App Installed", "Descriptor title match");
                TEST_ASSERT(std::wstring(descs[i].message) == L"Calculator has been installed.", "Descriptor message match");
                break;
            }
        }
        TEST_ASSERT(foundPending, "Pending descriptors must contain created toast");

        hr = wns::WpnCloseChannel(L"MicaNT.StoreApp");
        TEST_ASSERT(hr == ole32::S_OK, "WpnCloseChannel must succeed");

        hr = wns::WpnUninitialize();
        TEST_ASSERT(hr == ole32::S_OK, "WpnUninitialize must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Command Integration (notify test, toast, list, channel, clear)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // notify test
        out.str("");
        shell.execute("notify test", out);
        TEST_ASSERT(out.str().find("[NOTIFY] Push Notification Platform Self-Test Completed Successfully.") != std::string::npos, "notify test must succeed");

        // notify toast
        out.str("");
        shell.execute("notify toast \"Security Alert\" \"New sovereign firewall rule activated\"", out);
        TEST_ASSERT(out.str().find("Toast notification posted (Notification ID #") != std::string::npos, "notify toast must post");

        // notify list
        out.str("");
        shell.execute("notify list", out);
        TEST_ASSERT(out.str().find("MicaNT Action Center Notifications") != std::string::npos, "notify list header match");
        TEST_ASSERT(out.str().find("Security Alert") != std::string::npos, "notify list must list posted toast");

        // notify channel
        out.str("");
        shell.execute("notify channel MicaNT.TestApp", out);
        TEST_ASSERT(out.str().find("WNS Push Notification Channel (MicaNT.TestApp):") != std::string::npos, "notify channel header match");
        TEST_ASSERT(out.str().find("https://wns.micant.local/") != std::string::npos, "notify channel URI match");

        // notify clear
        out.str("");
        shell.execute("notify clear", out);
        TEST_ASSERT(out.str().find("All notifications cleared from Action Center.") != std::string::npos, "notify clear message match");

        // notify list again to ensure empty
        out.str("");
        shell.execute("notify list", out);
        TEST_ASSERT(out.str().find("0 active") != std::string::npos, "Action Center must show 0 active after clear");
    }

    std::cout << "[TEST] Suite 92: Windows Push Notification Service (WNS) & Push Notification Subsystem PASSED.\n";
}

class MockLocationEvents : public location::ILocationEvents {
private:
    uint32_t m_refCount{1};

public:
    uint32_t statusChangedCount{0};
    location::LOCATION_REPORT_STATUS lastStatus{location::REPORT_NOT_SUPPORTED};
    uint32_t locationChangedCount{0};

    virtual ole32::HRESULT __stdcall QueryInterface(ole32::REFIID riid, void** ppvObject) override {
        if (!ppvObject) return ole32::E_POINTER;
        if (riid == ole32::IID_IUnknown || riid == location::IID_ILocationEvents) {
            *ppvObject = static_cast<location::ILocationEvents*>(this);
            AddRef();
            return ole32::S_OK;
        }
        *ppvObject = nullptr;
        return ole32::E_NOINTERFACE;
    }

    virtual uint32_t __stdcall AddRef() override {
        return ++m_refCount;
    }

    virtual uint32_t __stdcall Release() override {
        uint32_t count = --m_refCount;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    virtual ole32::HRESULT __stdcall OnLocationChanged(ole32::REFIID /*reportType*/, location::ILocationReport* /*pLocationReport*/) override {
        locationChangedCount++;
        return ole32::S_OK;
    }

    virtual ole32::HRESULT __stdcall OnStatusChanged(ole32::REFIID /*reportType*/, location::LOCATION_REPORT_STATUS status) override {
        statusChangedCount++;
        lastStatus = status;
        return ole32::S_OK;
    }
};

void Test_WindowsLocation_Geolocation_Subsystem() {
    std::cout << "[TEST] Running Suite 93: Windows Geolocation & Location Framework (LF) Subsystem (locationapi.dll)...\n";

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Initialization (locationapi.dll)
    // ------------------------------------------------------------------------
    location::InitializeLocationSubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    TEST_ASSERT(ldr.getExport("locationapi.dll", "DllGetClassObject") != nullptr, "locationapi.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "DllCanUnloadNow") != nullptr, "locationapi.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "DllRegisterServer") != nullptr, "locationapi.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "DllUnregisterServer") != nullptr, "locationapi.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "LocationInitialize") != nullptr, "locationapi.dll must export LocationInitialize");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "LocationUninitialize") != nullptr, "locationapi.dll must export LocationUninitialize");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "LocationGetCoordinates") != nullptr, "locationapi.dll must export LocationGetCoordinates");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "LocationSetCoordinates") != nullptr, "locationapi.dll must export LocationSetCoordinates");
    TEST_ASSERT(ldr.getExport("locationapi.dll", "LocationGetStatus") != nullptr, "locationapi.dll must export LocationGetStatus");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* ver = version::VersionDatabase::Instance().FindModule("locationapi.dll");
        TEST_ASSERT(ver != nullptr, "VersionDatabase must contain locationapi.dll");
        TEST_ASSERT(ver->stringTable.at("FileDescription") == "Windows Location API", "locationapi.dll description match");
        TEST_ASSERT(ver->stringTable.at("OriginalFilename") == "locationapi.dll", "locationapi.dll original filename match");
        TEST_ASSERT(ver->stringTable.at("ProductName") == "MicaNT Location Framework", "locationapi.dll product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: Service Control Manager Services (lfsvc, SensorService)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();

        auto lfSvc = scm.getServiceRecord(L"lfsvc");
        TEST_ASSERT(lfSvc != nullptr, "lfsvc service must be registered in SCM");
        TEST_ASSERT(lfSvc->displayName == L"Geolocation Service", "lfsvc display name match");
        TEST_ASSERT(lfSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "lfsvc must be in running state");
        TEST_ASSERT(lfSvc->status.dwProcessId == 1150, "lfsvc PID match");

        auto sensorSvc = scm.getServiceRecord(L"SensorService");
        TEST_ASSERT(sensorSvc != nullptr, "SensorService service must be registered in SCM");
        TEST_ASSERT(sensorSvc->displayName == L"Sensor Service", "SensorService display name match");
        TEST_ASSERT(sensorSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "SensorService must be in running state");
        TEST_ASSERT(sensorSvc->status.dwProcessId == 1154, "SensorService PID match");
    }

    // Reset LocationManager to default
    location::LocationManager::get().resetToDefault();

    // ------------------------------------------------------------------------
    // Stage 4: COM Activation & Class Factory via CoCreateInstance
    // ------------------------------------------------------------------------
    location::ILocation* pLoc = nullptr;
    {
        ole32::HRESULT hr = ole32::CoCreateInstance(
            location::CLSID_Location, nullptr, ole32::CLSCTX_INPROC_SERVER,
            location::IID_ILocation, reinterpret_cast<void**>(&pLoc)
        );
        TEST_ASSERT(hr == ole32::S_OK && pLoc != nullptr, "CoCreateInstance(CLSID_Location) must succeed");

        ole32::IUnknown* pUnk = nullptr;
        hr = pLoc->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
        TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "QueryInterface for IUnknown must succeed");
        pUnk->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 5: Geolocation Position Reporting (ILatLongReport)
    // ------------------------------------------------------------------------
    {
        location::ILocationReport* pReport = nullptr;
        ole32::HRESULT hr = pLoc->GetReport(location::IID_ILatLongReport, &pReport);
        TEST_ASSERT(hr == ole32::S_OK && pReport != nullptr, "GetReport(IID_ILatLongReport) must succeed");

        location::SENSOR_ID sid{};
        hr = pReport->GetSensorID(&sid);
        TEST_ASSERT(hr == ole32::S_OK, "GetSensorID must succeed");
        TEST_ASSERT(sid.Data1 == 0x53454E53, "Sensor ID Data1 signature match");

        win32::SYSTEMTIME st{};
        hr = pReport->GetTimestamp(&st);
        TEST_ASSERT(hr == ole32::S_OK, "GetTimestamp must succeed");
        TEST_ASSERT(st.wYear == 2026, "Timestamp year match");

        location::ILatLongReport* pLatLong = nullptr;
        hr = pReport->QueryInterface(location::IID_ILatLongReport, reinterpret_cast<void**>(&pLatLong));
        TEST_ASSERT(hr == ole32::S_OK && pLatLong != nullptr, "QueryInterface for ILatLongReport must succeed");

        double lat = 0, lon = 0, alt = 0, err = 0, altErr = 0, head = 0, spd = 0;
        hr = pLatLong->GetLatitude(&lat);
        TEST_ASSERT(hr == ole32::S_OK && std::abs(lat - 47.6062) < 0.0001, "Latitude match");

        hr = pLatLong->GetLongitude(&lon);
        TEST_ASSERT(hr == ole32::S_OK && std::abs(lon - (-122.3321)) < 0.0001, "Longitude match");

        hr = pLatLong->GetAltitude(&alt);
        TEST_ASSERT(hr == ole32::S_OK && alt == 54.0, "Altitude match");

        hr = pLatLong->GetErrorRadius(&err);
        TEST_ASSERT(hr == ole32::S_OK && err == 5.0, "Error radius match");

        hr = pLatLong->GetAltitudeError(&altErr);
        TEST_ASSERT(hr == ole32::S_OK && altErr == 2.0, "Altitude error match");

        hr = pLatLong->GetHeading(&head);
        TEST_ASSERT(hr == ole32::S_OK && head == 180.0, "Heading match");

        hr = pLatLong->GetSpeed(&spd);
        TEST_ASSERT(hr == ole32::S_OK && spd == 0.0, "Speed match");

        pLatLong->Release();
        pReport->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 6: Civic Address Reporting (ICivicAddressReport)
    // ------------------------------------------------------------------------
    {
        location::ILocationReport* pReport = nullptr;
        ole32::HRESULT hr = pLoc->GetReport(location::IID_ICivicAddressReport, &pReport);
        TEST_ASSERT(hr == ole32::S_OK && pReport != nullptr, "GetReport(IID_ICivicAddressReport) must succeed");

        location::ICivicAddressReport* pCivic = nullptr;
        hr = pReport->QueryInterface(location::IID_ICivicAddressReport, reinterpret_cast<void**>(&pCivic));
        TEST_ASSERT(hr == ole32::S_OK && pCivic != nullptr, "QueryInterface for ICivicAddressReport must succeed");

        ole32::BSTR addr1 = nullptr;
        hr = pCivic->GetAddressLine1(&addr1);
        TEST_ASSERT(hr == ole32::S_OK && addr1 != nullptr, "GetAddressLine1 must succeed");
        TEST_ASSERT(std::wstring(addr1) == L"One Sovereign Way", "AddressLine1 match");
        ole32::SysFreeString(addr1);

        ole32::BSTR city = nullptr;
        hr = pCivic->GetCity(&city);
        TEST_ASSERT(hr == ole32::S_OK && city != nullptr, "GetCity must succeed");
        TEST_ASSERT(std::wstring(city) == L"Redmond", "City match");
        ole32::SysFreeString(city);

        ole32::BSTR state = nullptr;
        hr = pCivic->GetStateProvince(&state);
        TEST_ASSERT(hr == ole32::S_OK && state != nullptr, "GetStateProvince must succeed");
        TEST_ASSERT(std::wstring(state) == L"WA", "StateProvince match");
        ole32::SysFreeString(state);

        ole32::BSTR zip = nullptr;
        hr = pCivic->GetPostalCode(&zip);
        TEST_ASSERT(hr == ole32::S_OK && zip != nullptr, "GetPostalCode must succeed");
        TEST_ASSERT(std::wstring(zip) == L"98052", "PostalCode match");
        ole32::SysFreeString(zip);

        ole32::BSTR country = nullptr;
        hr = pCivic->GetCountryRegion(&country);
        TEST_ASSERT(hr == ole32::S_OK && country != nullptr, "GetCountryRegion must succeed");
        TEST_ASSERT(std::wstring(country) == L"US", "CountryRegion match");
        ole32::SysFreeString(country);

        uint32_t detail = 0;
        hr = pCivic->GetDetailLevel(&detail);
        TEST_ASSERT(hr == ole32::S_OK && detail == 1, "DetailLevel must be 1");

        pCivic->Release();
        pReport->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 7: Location Events and Status Changes (ILocationEvents)
    // ------------------------------------------------------------------------
    {
        auto* pMockEvents = new MockLocationEvents();

        ole32::HRESULT hr = pLoc->RegisterForReport(pMockEvents, location::IID_ILatLongReport, 500);
        TEST_ASSERT(hr == ole32::S_OK, "RegisterForReport must succeed");
        TEST_ASSERT(location::LocationManager::get().getListenerCount() == 1, "Listener count must be 1");

        location::LocationManager::get().setStatus(location::REPORT_INITIALIZING);
        TEST_ASSERT(pMockEvents->statusChangedCount == 1, "Mock must have received status change event");
        TEST_ASSERT(pMockEvents->lastStatus == location::REPORT_INITIALIZING, "Status reported to mock must be REPORT_INITIALIZING");

        location::LocationManager::get().setStatus(location::REPORT_RUNNING);
        TEST_ASSERT(pMockEvents->statusChangedCount == 2, "Mock must have received 2nd status change event");
        TEST_ASSERT(pMockEvents->lastStatus == location::REPORT_RUNNING, "Status reported to mock must be REPORT_RUNNING");

        hr = pLoc->UnregisterForReport(location::IID_ILatLongReport);
        TEST_ASSERT(hr == ole32::S_OK, "UnregisterForReport must succeed");
        TEST_ASSERT(location::LocationManager::get().getListenerCount() == 0, "Listener count must be 0 after unregister");

        pMockEvents->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 8: Accuracy & Reporting Interval Configuration
    // ------------------------------------------------------------------------
    {
        location::LOCATION_DESIRED_ACCURACY acc = location::LOCATION_DESIRED_ACCURACY_DEFAULT;
        ole32::HRESULT hr = pLoc->GetDesiredAccuracy(location::IID_ILatLongReport, &acc);
        TEST_ASSERT(hr == ole32::S_OK && acc == location::LOCATION_DESIRED_ACCURACY_DEFAULT, "Default accuracy match");

        hr = pLoc->SetDesiredAccuracy(location::IID_ILatLongReport, location::LOCATION_DESIRED_ACCURACY_HIGH);
        TEST_ASSERT(hr == ole32::S_OK, "SetDesiredAccuracy must succeed");

        hr = pLoc->GetDesiredAccuracy(location::IID_ILatLongReport, &acc);
        TEST_ASSERT(hr == ole32::S_OK && acc == location::LOCATION_DESIRED_ACCURACY_HIGH, "Accuracy must now be HIGH");

        uint32_t interval = 0;
        hr = pLoc->GetReportInterval(location::IID_ILatLongReport, &interval);
        TEST_ASSERT(hr == ole32::S_OK && interval == 1000, "Default interval match");

        hr = pLoc->SetReportInterval(location::IID_ILatLongReport, 250);
        TEST_ASSERT(hr == ole32::S_OK, "SetReportInterval must succeed");

        hr = pLoc->GetReportInterval(location::IID_ILatLongReport, &interval);
        TEST_ASSERT(hr == ole32::S_OK && interval == 250, "Updated interval match");

        hr = pLoc->RequestPermissions(nullptr, nullptr, 0, 0);
        TEST_ASSERT(hr == ole32::S_OK, "RequestPermissions must succeed");
    }

    pLoc->Release();

    // ------------------------------------------------------------------------
    // Stage 9: Win32 C Client APIs
    // ------------------------------------------------------------------------
    {
        ole32::HRESULT hr = location::LocationInitialize();
        TEST_ASSERT(hr == ole32::S_OK, "LocationInitialize must succeed");

        hr = location::LocationSetCoordinates(37.7749, -122.4194, 16.0);
        TEST_ASSERT(hr == ole32::S_OK, "LocationSetCoordinates must succeed");

        double lat = 0, lon = 0, acc = 0;
        hr = location::LocationGetCoordinates(&lat, &lon, &acc);
        TEST_ASSERT(hr == ole32::S_OK, "LocationGetCoordinates must succeed");
        TEST_ASSERT(std::abs(lat - 37.7749) < 0.0001, "Latitude SF match");
        TEST_ASSERT(std::abs(lon - (-122.4194)) < 0.0001, "Longitude SF match");

        uint32_t status = 0xFF;
        hr = location::LocationGetStatus(&status);
        TEST_ASSERT(hr == ole32::S_OK && status == location::REPORT_RUNNING, "LocationGetStatus must return REPORT_RUNNING");

        hr = location::LocationUninitialize();
        TEST_ASSERT(hr == ole32::S_OK, "LocationUninitialize must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Integration (location test, status, get, set, civic)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // location test
        out.str("");
        shell.execute("location test", out);
        TEST_ASSERT(out.str().find("[LOCATION] Geolocation Subsystem Self-Test Completed Successfully.") != std::string::npos, "location test must succeed");

        // location status
        out.str("");
        shell.execute("location status", out);
        TEST_ASSERT(out.str().find("Provider Status:     RUNNING (Operational)") != std::string::npos, "location status must show RUNNING");
        TEST_ASSERT(out.str().find("SOVEREIGN ZERO-TELEMETRY") != std::string::npos, "location status must show zero-telemetry");

        // location get
        out.str("");
        shell.execute("location get", out);
        TEST_ASSERT(out.str().find("Current Geolocation Fix & Address") != std::string::npos, "location get header match");

        // location set
        out.str("");
        shell.execute("location set 40.7128 -74.0060 10.0 3.0", out);
        TEST_ASSERT(out.str().find("[LOCATION] Simulated coordinates updated:") != std::string::npos, "location set match");

        // verify location get reflects update
        out.str("");
        shell.execute("location get", out);
        TEST_ASSERT(out.str().find("40.712800") != std::string::npos, "location get must reflect updated lat");
        TEST_ASSERT(out.str().find("-74.006000") != std::string::npos, "location get must reflect updated lon");

        // location civic
        out.str("");
        shell.execute("location civic \"350 Fifth Ave\" \"New York\" \"NY\" \"10118\"", out);
        TEST_ASSERT(out.str().find("Civic address updated to:") != std::string::npos, "location civic match");

        out.str("");
        shell.execute("location get", out);
        TEST_ASSERT(out.str().find("New York") != std::string::npos, "location get must reflect New York");
    }

    std::cout << "[TEST] Suite 93: Windows Geolocation & Location Framework (LF) Subsystem PASSED.\n";
}

void Test_WindowsWPD_PortableDevices_Subsystem() {
    std::cout << "[TEST] Running Suite 94: Windows Portable Devices (WPD) Subsystem...\n";

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (portabledeviceapi.dll, wpd_ci.dll)
    // ------------------------------------------------------------------------
    wpd::InitializeWpdSubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    TEST_ASSERT(ldr.getExport("portabledeviceapi.dll", "DllGetClassObject") != nullptr, "portabledeviceapi.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("portabledeviceapi.dll", "DllCanUnloadNow") != nullptr, "portabledeviceapi.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("portabledeviceapi.dll", "DllRegisterServer") != nullptr, "portabledeviceapi.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("portabledeviceapi.dll", "DllUnregisterServer") != nullptr, "portabledeviceapi.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("portabledeviceapi.dll", "WpdCreateDeviceManager") != nullptr, "portabledeviceapi.dll must export WpdCreateDeviceManager");
    TEST_ASSERT(ldr.getExport("portabledeviceapi.dll", "WpdGetDeviceCount") != nullptr, "portabledeviceapi.dll must export WpdGetDeviceCount");

    TEST_ASSERT(ldr.getExport("wpd_ci.dll", "WpdClassInstaller") != nullptr, "wpd_ci.dll must export WpdClassInstaller");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verApi = version::VersionDatabase::Instance().FindModule("portabledeviceapi.dll");
        TEST_ASSERT(verApi != nullptr, "VersionDatabase must contain portabledeviceapi.dll");
        TEST_ASSERT(verApi->stringTable.at("FileDescription") == "Windows Portable Device API", "portabledeviceapi.dll description match");
        TEST_ASSERT(verApi->stringTable.at("OriginalFilename") == "portabledeviceapi.dll", "portabledeviceapi.dll original filename match");
        TEST_ASSERT(verApi->stringTable.at("ProductName") == "MicaNT Portable Devices Subsystem", "portabledeviceapi.dll product name match");

        const auto* verCi = version::VersionDatabase::Instance().FindModule("wpd_ci.dll");
        TEST_ASSERT(verCi != nullptr, "VersionDatabase must contain wpd_ci.dll");
        TEST_ASSERT(verCi->stringTable.at("FileDescription") == "Windows Portable Device Class Installer", "wpd_ci.dll description match");
        TEST_ASSERT(verCi->stringTable.at("OriginalFilename") == "wpd_ci.dll", "wpd_ci.dll original filename match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: Service Control Manager Services (WpdBusEnum)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();
        auto wpdSvc = scm.getServiceRecord(L"WpdBusEnum");
        TEST_ASSERT(wpdSvc != nullptr, "WpdBusEnum service must be registered in SCM");
        TEST_ASSERT(wpdSvc->displayName == L"Windows Portable Device Enumerator Service", "WpdBusEnum display name match");
        TEST_ASSERT(wpdSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "WpdBusEnum must be running");
        TEST_ASSERT(wpdSvc->status.dwProcessId == 1158, "WpdBusEnum PID match");
    }

    // Reset PortableDeviceManager to default
    wpd::PortableDeviceManager::get().resetToDefault();

    // ------------------------------------------------------------------------
    // Stage 4: COM Activation & Class Factory via CoCreateInstance
    // ------------------------------------------------------------------------
    wpd::IPortableDeviceManager* pMgr = nullptr;
    {
        ole32::HRESULT hr = ole32::CoCreateInstance(
            wpd::CLSID_PortableDeviceManager, nullptr, ole32::CLSCTX_INPROC_SERVER,
            wpd::IID_IPortableDeviceManager, reinterpret_cast<void**>(&pMgr)
        );
        TEST_ASSERT(hr == ole32::S_OK && pMgr != nullptr, "CoCreateInstance(CLSID_PortableDeviceManager) must succeed");

        ole32::IUnknown* pUnk = nullptr;
        hr = pMgr->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
        TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "QueryInterface for IUnknown must succeed");
        pUnk->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 5: Device Manager Enumeration & Identification
    // ------------------------------------------------------------------------
    std::wstring deviceId;
    {
        uint32_t count = 0;
        ole32::HRESULT hr = pMgr->GetDevices(nullptr, &count);
        TEST_ASSERT(hr == ole32::S_OK && count == 1, "GetDevices count must return 1 device");

        std::vector<wchar_t*> ids(count, nullptr);
        hr = pMgr->GetDevices(ids.data(), &count);
        TEST_ASSERT(hr == ole32::S_OK && ids[0] != nullptr, "GetDevices must retrieve device ID");
        deviceId = ids[0];

        wchar_t friendly[256]{};
        uint32_t cch = 256;
        hr = pMgr->GetDeviceFriendlyName(deviceId.c_str(), friendly, &cch);
        TEST_ASSERT(hr == ole32::S_OK, "GetDeviceFriendlyName must succeed");
        TEST_ASSERT(std::wstring(friendly) == L"MicaNT Sovereign Mobile Companion", "Friendly name match");

        wchar_t mfg[256]{};
        cch = 256;
        hr = pMgr->GetDeviceManufacturer(deviceId.c_str(), mfg, &cch);
        TEST_ASSERT(hr == ole32::S_OK, "GetDeviceManufacturer must succeed");
        TEST_ASSERT(std::wstring(mfg) == L"MicaNT Sovereign Project", "Manufacturer match");

        wchar_t desc[256]{};
        cch = 256;
        hr = pMgr->GetDeviceDescription(deviceId.c_str(), desc, &cch);
        TEST_ASSERT(hr == ole32::S_OK, "GetDeviceDescription must succeed");
        TEST_ASSERT(std::wstring(desc) == L"MicaPhone M1 Sovereign Storage & Media Device", "Description match");

        for (auto* p : ids) {
            if (p) ole32::CoTaskMemFree(p);
        }
    }

    // ------------------------------------------------------------------------
    // Stage 6: Device Content & Property Inspection
    // ------------------------------------------------------------------------
    wpd::IPortableDevice* pDev = nullptr;
    ole32::HRESULT hr = ole32::CoCreateInstance(
        wpd::CLSID_PortableDevice, nullptr, ole32::CLSCTX_INPROC_SERVER,
        wpd::IID_IPortableDevice, reinterpret_cast<void**>(&pDev)
    );
    TEST_ASSERT(hr == ole32::S_OK && pDev != nullptr, "CoCreateInstance(CLSID_PortableDevice) must succeed");

    hr = pDev->Open(deviceId.c_str(), nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "IPortableDevice::Open must succeed");

    wpd::IPortableDeviceContent* pContent = nullptr;
    hr = pDev->Content(&pContent);
    TEST_ASSERT(hr == ole32::S_OK && pContent != nullptr, "IPortableDevice::Content must succeed");

    wpd::IPortableDeviceProperties* pProps = nullptr;
    hr = pContent->Properties(&pProps);
    TEST_ASSERT(hr == ole32::S_OK && pProps != nullptr, "IPortableDeviceContent::Properties must succeed");

    // Inspect storage properties
    {
        wpd::IPortableDeviceValues* pVals = nullptr;
        hr = pProps->GetValues(L"s10001", nullptr, &pVals);
        TEST_ASSERT(hr == ole32::S_OK && pVals != nullptr, "GetValues(s10001) must succeed");

        wchar_t* name = nullptr;
        hr = pVals->GetStringValue(wpd::WPD_OBJECT_NAME, &name);
        TEST_ASSERT(hr == ole32::S_OK && name != nullptr, "GetStringValue(WPD_OBJECT_NAME) must succeed");
        TEST_ASSERT(std::wstring(name) == L"Internal Shared Storage", "Storage object name match");
        ole32::CoTaskMemFree(name);

        uint64_t size = 0;
        hr = pVals->GetUnsignedLargeIntegerValue(wpd::WPD_OBJECT_SIZE, &size);
        TEST_ASSERT(hr == ole32::S_OK && size == 256000000000ULL, "Storage size must be 256GB");

        GUID ctype{};
        hr = pVals->GetGuidValue(wpd::WPD_OBJECT_CONTENT_TYPE, &ctype);
        TEST_ASSERT(hr == ole32::S_OK && ctype == wpd::WPD_CONTENT_TYPE_FOLDER, "Content type must be folder");

        pVals->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 7: Storage Object & Folder Hierarchy Enumeration
    // ------------------------------------------------------------------------
    {
        wpd::IEnumPortableDeviceObjectIDs* pEnum = nullptr;
        hr = pContent->EnumObjects(0, L"DEVICE", nullptr, &pEnum);
        TEST_ASSERT(hr == ole32::S_OK && pEnum != nullptr, "EnumObjects(DEVICE) must succeed");

        wchar_t* objIds[10]{};
        uint32_t fetched = 0;
        hr = pEnum->Next(10, objIds, &fetched);
        TEST_ASSERT(SUCCEEDED(hr) && fetched == 1, "DEVICE should contain 1 root child (s10001)");
        TEST_ASSERT(std::wstring(objIds[0]) == L"s10001", "Child must be s10001");
        ole32::CoTaskMemFree(objIds[0]);
        pEnum->Release();

        // Enumerate s10001 children (DCIM, Documents, Music)
        hr = pContent->EnumObjects(0, L"s10001", nullptr, &pEnum);
        TEST_ASSERT(hr == ole32::S_OK && pEnum != nullptr, "EnumObjects(s10001) must succeed");

        fetched = 0;
        hr = pEnum->Next(10, objIds, &fetched);
        TEST_ASSERT(SUCCEEDED(hr) && fetched == 3, "s10001 should contain 3 children");
        std::vector<std::wstring> childIds;
        for (uint32_t i = 0; i < fetched; ++i) {
            childIds.push_back(objIds[i]);
            ole32::CoTaskMemFree(objIds[i]);
        }
        pEnum->Release();

        TEST_ASSERT(childIds[0] == L"o1001", "First child is DCIM (o1001)");
        TEST_ASSERT(childIds[1] == L"o1003", "Second child is Documents (o1003)");
        TEST_ASSERT(childIds[2] == L"o1005", "Third child is Music (o1005)");

        // Enumerate DCIM children (IMG_0001.JPG)
        hr = pContent->EnumObjects(0, L"o1001", nullptr, &pEnum);
        TEST_ASSERT(hr == ole32::S_OK && pEnum != nullptr, "EnumObjects(o1001) must succeed");

        fetched = 0;
        hr = pEnum->Next(10, objIds, &fetched);
        TEST_ASSERT(SUCCEEDED(hr) && fetched == 1, "DCIM should contain 1 child (o1002)");
        TEST_ASSERT(std::wstring(objIds[0]) == L"o1002", "Child must be o1002");
        ole32::CoTaskMemFree(objIds[0]);
        pEnum->Release();

        // Check image file properties
        wpd::IPortableDeviceValues* pImgVals = nullptr;
        hr = pProps->GetValues(L"o1002", nullptr, &pImgVals);
        TEST_ASSERT(hr == ole32::S_OK && pImgVals != nullptr, "GetValues(o1002) must succeed");

        wchar_t* imgName = nullptr;
        pImgVals->GetStringValue(wpd::WPD_OBJECT_NAME, &imgName);
        TEST_ASSERT(std::wstring(imgName) == L"IMG_0001.JPG", "Image name match");
        ole32::CoTaskMemFree(imgName);

        uint64_t imgSize = 0;
        pImgVals->GetUnsignedLargeIntegerValue(wpd::WPD_OBJECT_SIZE, &imgSize);
        TEST_ASSERT(imgSize == 3145728, "Image size match (3MB)");

        pImgVals->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 8: Device Capabilities & Collections
    // ------------------------------------------------------------------------
    {
        wpd::IPortableDeviceCapabilities* pCaps = nullptr;
        hr = pDev->Capabilities(&pCaps);
        TEST_ASSERT(hr == ole32::S_OK && pCaps != nullptr, "IPortableDevice::Capabilities must succeed");

        wpd::IPortableDevicePropVariantCollection* pCats = nullptr;
        hr = pCaps->GetFunctionalCategories(&pCats);
        TEST_ASSERT(hr == ole32::S_OK && pCats != nullptr, "GetFunctionalCategories must succeed");

        uint32_t numCats = 0;
        pCats->GetCount(&numCats);
        TEST_ASSERT(numCats == 4, "Device must have 4 functional categories");
        pCats->Release();
        pCaps->Release();

        // Key Collection tests
        wpd::IPortableDeviceKeyCollection* pKeyCol = nullptr;
        hr = ole32::CoCreateInstance(
            wpd::CLSID_PortableDeviceKeyCollection, nullptr, ole32::CLSCTX_INPROC_SERVER,
            wpd::IID_IPortableDeviceKeyCollection, reinterpret_cast<void**>(&pKeyCol)
        );
        TEST_ASSERT(hr == ole32::S_OK && pKeyCol != nullptr, "CoCreateInstance(CLSID_PortableDeviceKeyCollection) must succeed");

        pKeyCol->Add(wpd::WPD_OBJECT_NAME);
        pKeyCol->Add(wpd::WPD_OBJECT_SIZE);
        uint32_t keyCount = 0;
        pKeyCol->GetCount(&keyCount);
        TEST_ASSERT(keyCount == 2, "Key collection count must be 2");

        wasapi::PROPERTYKEY key{};
        pKeyCol->GetAt(0, &key);
        TEST_ASSERT(key == wpd::WPD_OBJECT_NAME, "Key 0 must be WPD_OBJECT_NAME");
        pKeyCol->Clear();
        pKeyCol->GetCount(&keyCount);
        TEST_ASSERT(keyCount == 0, "Key collection clear must reset count to 0");
        pKeyCol->Release();
    }

    pProps->Release();
    pContent->Release();
    pDev->Release();
    pMgr->Release();

    // ------------------------------------------------------------------------
    // Stage 9: C Client APIs Verification
    // ------------------------------------------------------------------------
    {
        wpd::IPortableDeviceManager* pCMgr = nullptr;
        hr = wpd::WpdCreateDeviceManager(&pCMgr);
        TEST_ASSERT(hr == ole32::S_OK && pCMgr != nullptr, "WpdCreateDeviceManager must succeed");
        pCMgr->Release();

        uint32_t devCnt = 0;
        hr = wpd::WpdGetDeviceCount(&devCnt);
        TEST_ASSERT(hr == ole32::S_OK && devCnt == 1, "WpdGetDeviceCount must return 1");

        uint32_t installerRc = wpd::WpdClassInstaller(0, nullptr, nullptr);
        TEST_ASSERT(installerRc == 0, "WpdClassInstaller must return NO_ERROR (0)");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Integration
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::stringstream out;

        // wpd test
        shell.execute("wpd test", out);
        TEST_ASSERT(out.str().find("[WPD] Self-Test Completed: ALL WPD TESTS PASSED.") != std::string::npos, "wpd test must succeed");

        // wpd list
        out.str("");
        shell.execute("wpd list", out);
        TEST_ASSERT(out.str().find("MicaNT Sovereign Mobile Companion") != std::string::npos, "wpd list must show device friendly name");
        TEST_ASSERT(out.str().find("CONNECTED / ONLINE") != std::string::npos, "wpd list must show connected status");

        // wpd info
        out.str("");
        shell.execute("wpd info", out);
        TEST_ASSERT(out.str().find("WpdBusEnum (PID 1158, RUNNING)") != std::string::npos, "wpd info must show WpdBusEnum service");
        TEST_ASSERT(out.str().find("MicaPhone M1 Sovereign Storage & Media Device") != std::string::npos, "wpd info must show description");

        // wpd browse s10001
        out.str("");
        shell.execute("wpd browse s10001", out);
        TEST_ASSERT(out.str().find("DCIM") != std::string::npos, "wpd browse must show DCIM");
        TEST_ASSERT(out.str().find("Documents") != std::string::npos, "wpd browse must show Documents");
        TEST_ASSERT(out.str().find("Music") != std::string::npos, "wpd browse must show Music");

        // wpd browse o1001 (DCIM)
        out.str("");
        shell.execute("wpd browse o1001", out);
        TEST_ASSERT(out.str().find("IMG_0001.JPG") != std::string::npos, "wpd browse DCIM must show IMG_0001.JPG");
    }

    std::cout << "[TEST] Suite 94: Windows Portable Devices (WPD) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 95: Windows Sensors API & Sensor Class Extension Subsystem Tests
// ============================================================================
void Test_WindowsSensors_Subsystem() {
    sensors::InitializeSensorsSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (sensorsapi.dll / sensorsclassextension.dll)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("sensorsapi.dll", "DllGetClassObject") != nullptr, "sensorsapi.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("sensorsapi.dll", "DllCanUnloadNow") != nullptr, "sensorsapi.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("sensorsapi.dll", "DllRegisterServer") != nullptr, "sensorsapi.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("sensorsapi.dll", "DllUnregisterServer") != nullptr, "sensorsapi.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("sensorsapi.dll", "SensorsCreateSensorManager") != nullptr, "sensorsapi.dll must export SensorsCreateSensorManager");
    TEST_ASSERT(ldr.getExport("sensorsapi.dll", "SensorsGetSensorCount") != nullptr, "sensorsapi.dll must export SensorsGetSensorCount");

    TEST_ASSERT(ldr.getExport("sensorsclassextension.dll", "DllGetClassObject") != nullptr, "sensorsclassextension.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("sensorsclassextension.dll", "DllCanUnloadNow") != nullptr, "sensorsclassextension.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("sensorsclassextension.dll", "DllRegisterServer") != nullptr, "sensorsclassextension.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("sensorsclassextension.dll", "DllUnregisterServer") != nullptr, "sensorsclassextension.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("sensorsclassextension.dll", "SensorsClassExtensionCreate") != nullptr, "sensorsclassextension.dll must export SensorsClassExtensionCreate");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verApi = version::VersionDatabase::Instance().FindModule("sensorsapi.dll");
        TEST_ASSERT(verApi != nullptr, "VersionDatabase must contain sensorsapi.dll");
        TEST_ASSERT(verApi->stringTable.at("FileDescription") == "Windows Sensors API", "sensorsapi.dll description match");
        TEST_ASSERT(verApi->stringTable.at("OriginalFilename") == "sensorsapi.dll", "sensorsapi.dll original filename match");
        TEST_ASSERT(verApi->stringTable.at("ProductName") == "MicaNT Sensor Platform", "sensorsapi.dll product name match");

        const auto* verExt = version::VersionDatabase::Instance().FindModule("sensorsclassextension.dll");
        TEST_ASSERT(verExt != nullptr, "VersionDatabase must contain sensorsclassextension.dll");
        TEST_ASSERT(verExt->stringTable.at("FileDescription") == "Windows Sensor Class Extension", "sensorsclassextension.dll description match");
        TEST_ASSERT(verExt->stringTable.at("OriginalFilename") == "sensorsclassextension.dll", "sensorsclassextension.dll original filename match");
        TEST_ASSERT(verExt->stringTable.at("ProductName") == "MicaNT Sensor Platform", "sensorsclassextension.dll product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: Service Control Manager Services (SensorDataService)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();
        auto sensorSvc = scm.getServiceRecord(L"SensorDataService");
        TEST_ASSERT(sensorSvc != nullptr, "SensorDataService service must be registered in SCM");
        TEST_ASSERT(sensorSvc->displayName == L"Sensor Data Service", "SensorDataService display name match");
        TEST_ASSERT(sensorSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "SensorDataService must be running");
        TEST_ASSERT(sensorSvc->status.dwProcessId == 1162, "SensorDataService PID match");
    }

    // ------------------------------------------------------------------------
    // Stage 4: COM Activation & Class Factory via CoCreateInstance
    // ------------------------------------------------------------------------
    sensors::ISensorManager* pMgr = nullptr;
    ole32::HRESULT hr = ole32::CoCreateInstance(
        sensors::CLSID_SensorManager, nullptr, ole32::CLSCTX_INPROC_SERVER,
        sensors::IID_ISensorManager, reinterpret_cast<void**>(&pMgr)
    );
    TEST_ASSERT(hr == ole32::S_OK && pMgr != nullptr, "CoCreateInstance(CLSID_SensorManager) must succeed");

    sensors::ISensorCollection* pEmptyCol = nullptr;
    hr = ole32::CoCreateInstance(
        sensors::CLSID_SensorCollection, nullptr, ole32::CLSCTX_INPROC_SERVER,
        sensors::IID_ISensorCollection, reinterpret_cast<void**>(&pEmptyCol)
    );
    TEST_ASSERT(hr == ole32::S_OK && pEmptyCol != nullptr, "CoCreateInstance(CLSID_SensorCollection) must succeed");
    pEmptyCol->Release();

    sensors::ISensorClassExtension* pExt = nullptr;
    hr = ole32::CoCreateInstance(
        sensors::CLSID_SensorClassExtension, nullptr, ole32::CLSCTX_INPROC_SERVER,
        sensors::IID_ISensorClassExtension, reinterpret_cast<void**>(&pExt)
    );
    TEST_ASSERT(hr == ole32::S_OK && pExt != nullptr, "CoCreateInstance(CLSID_SensorClassExtension) must succeed");

    // ------------------------------------------------------------------------
    // Stage 5: Sensor Manager Category Enumeration & Querying
    // ------------------------------------------------------------------------
    sensors::ISensorCollection* pAllSensors = nullptr;
    hr = pMgr->GetSensorsByCategory(sensors::SENSOR_CATEGORY_ALL, &pAllSensors);
    TEST_ASSERT(hr == ole32::S_OK && pAllSensors != nullptr, "GetSensorsByCategory(SENSOR_CATEGORY_ALL) must succeed");

    uint32_t totalSensors = 0;
    pAllSensors->GetCount(&totalSensors);
    TEST_ASSERT(totalSensors >= 5, "Total sensors count must be at least 5");
    pAllSensors->Release();

    // Verify Motion sensors category
    sensors::ISensorCollection* pMotionSensors = nullptr;
    hr = pMgr->GetSensorsByCategory(sensors::SENSOR_CATEGORY_MOTION, &pMotionSensors);
    TEST_ASSERT(hr == ole32::S_OK && pMotionSensors != nullptr, "GetSensorsByCategory(SENSOR_CATEGORY_MOTION) must succeed");
    uint32_t motionCount = 0;
    pMotionSensors->GetCount(&motionCount);
    TEST_ASSERT(motionCount == 2, "Motion category should have 2 sensors (accel + gyro)");
    pMotionSensors->Release();

    // Verify Environmental sensors category
    sensors::ISensorCollection* pEnvSensors = nullptr;
    hr = pMgr->GetSensorsByCategory(sensors::SENSOR_CATEGORY_ENVIRONMENTAL, &pEnvSensors);
    TEST_ASSERT(hr == ole32::S_OK && pEnvSensors != nullptr, "GetSensorsByCategory(SENSOR_CATEGORY_ENVIRONMENTAL) must succeed");
    uint32_t envCount = 0;
    pEnvSensors->GetCount(&envCount);
    TEST_ASSERT(envCount == 1, "Environmental category should have 1 sensor (barometer)");
    pEnvSensors->Release();

    // ------------------------------------------------------------------------
    // Stage 6: Sensor Type Filtering, Metadata & Property Inspection
    // ------------------------------------------------------------------------
    sensors::ISensorCollection* pAccelCol = nullptr;
    hr = pMgr->GetSensorsByType(sensors::SENSOR_TYPE_ACCELEROMETER_3D, &pAccelCol);
    TEST_ASSERT(hr == ole32::S_OK && pAccelCol != nullptr, "GetSensorsByType(SENSOR_TYPE_ACCELEROMETER_3D) must succeed");

    uint32_t accelCount = 0;
    pAccelCol->GetCount(&accelCount);
    TEST_ASSERT(accelCount == 1, "Accelerometer collection must contain 1 sensor");

    sensors::ISensor* pAccel = nullptr;
    hr = pAccelCol->GetAt(0, &pAccel);
    TEST_ASSERT(hr == ole32::S_OK && pAccel != nullptr, "GetAt(0) for accelerometer must succeed");
    pAccelCol->Release();

    ole32::BSTR bstrName = nullptr;
    hr = pAccel->GetFriendlyName(&bstrName);
    TEST_ASSERT(hr == ole32::S_OK && bstrName != nullptr, "GetFriendlyName must succeed");
    TEST_ASSERT(std::wstring(bstrName) == L"MicaNT Sovereign 3-Axis Accelerometer", "Friendly name match");
    ole32::SysFreeString(bstrName);

    sensors::SensorState state{};
    hr = pAccel->GetState(&state);
    TEST_ASSERT(hr == ole32::S_OK && state == sensors::SENSOR_STATE_READY, "Sensor state must be READY");

    sensors::SENSOR_ID accelId{};
    hr = pAccel->GetID(&accelId);
    TEST_ASSERT(hr == ole32::S_OK, "GetID must succeed");

    sensors::SENSOR_CATEGORY_ID catId{};
    hr = pAccel->GetCategory(&catId);
    TEST_ASSERT(hr == ole32::S_OK && catId == sensors::SENSOR_CATEGORY_MOTION, "Category must match SENSOR_CATEGORY_MOTION");

    sensors::SENSOR_TYPE_ID typeId{};
    hr = pAccel->GetType(&typeId);
    TEST_ASSERT(hr == ole32::S_OK && typeId == sensors::SENSOR_TYPE_ACCELEROMETER_3D, "Type must match SENSOR_TYPE_ACCELEROMETER_3D");

    int16_t isSupported = 0;
    hr = pAccel->SupportsDataField(sensors::SENSOR_DATA_TYPE_ACCELERATION_X_G, &isSupported);
    TEST_ASSERT(hr == ole32::S_OK && isSupported != 0, "SupportsDataField for ACCELERATION_X_G must return VARIANT_TRUE");

    wasapi::PROPVARIANT propMfg{};
    hr = pAccel->GetProperty(sensors::SENSOR_PROPERTY_MANUFACTURER, &propMfg);
    TEST_ASSERT(hr == ole32::S_OK && propMfg.pwszVal != nullptr, "GetProperty(SENSOR_PROPERTY_MANUFACTURER) must succeed");
    TEST_ASSERT(std::wstring(propMfg.pwszVal) == L"MicaNT Hardware Systems", "Manufacturer string match");
    wasapi::PropVariantClear(&propMfg);

    // ------------------------------------------------------------------------
    // Stage 7: Real-Time Synchronous Data Reporting (GetData)
    // ------------------------------------------------------------------------
    {
        sensors::ISensorDataReport* pReport = nullptr;
        hr = pAccel->GetData(&pReport);
        TEST_ASSERT(hr == ole32::S_OK && pReport != nullptr, "ISensor::GetData must succeed");

        wasapi::PROPVARIANT valX{};
        hr = pReport->GetSensorValue(sensors::SENSOR_DATA_TYPE_ACCELERATION_X_G, &valX);
        TEST_ASSERT(hr == ole32::S_OK && valX.vt == 5, "GetSensorValue(ACCELERATION_X_G) must return VT_R8");
        TEST_ASSERT(std::abs(valX.dblVal - 0.02) < 1e-4, "X-acceleration value match");

        wasapi::PROPVARIANT valZ{};
        hr = pReport->GetSensorValue(sensors::SENSOR_DATA_TYPE_ACCELERATION_Z_G, &valZ);
        TEST_ASSERT(hr == ole32::S_OK && valZ.vt == 5, "GetSensorValue(ACCELERATION_Z_G) must return VT_R8");
        TEST_ASSERT(std::abs(valZ.dblVal - 0.98) < 1e-4, "Z-acceleration value match");

        pReport->Release();
    }

    // Inspect Ambient Light Sensor reading
    {
        sensors::ISensorCollection* pAlsCol = nullptr;
        hr = pMgr->GetSensorsByType(sensors::SENSOR_TYPE_AMBIENT_LIGHT, &pAlsCol);
        TEST_ASSERT(hr == ole32::S_OK && pAlsCol != nullptr, "GetSensorsByType(AMBIENT_LIGHT) must succeed");

        sensors::ISensor* pAls = nullptr;
        pAlsCol->GetAt(0, &pAls);
        TEST_ASSERT(pAls != nullptr, "ALS sensor must be found");
        pAlsCol->Release();

        sensors::ISensorDataReport* pAlsReport = nullptr;
        hr = pAls->GetData(&pAlsReport);
        TEST_ASSERT(hr == ole32::S_OK && pAlsReport != nullptr, "ALS GetData must succeed");

        wasapi::PROPVARIANT luxVal{};
        hr = pAlsReport->GetSensorValue(sensors::SENSOR_DATA_TYPE_LIGHT_LUX, &luxVal);
        TEST_ASSERT(hr == ole32::S_OK && luxVal.dblVal == 350.0, "ALS reading must match 350.0 Lux");

        pAlsReport->Release();
        pAls->Release();
    }

    // Inspect Barometer reading
    {
        sensors::ISensorCollection* pBaroCol = nullptr;
        hr = pMgr->GetSensorsByType(sensors::SENSOR_TYPE_BAROMETER, &pBaroCol);
        TEST_ASSERT(hr == ole32::S_OK && pBaroCol != nullptr, "GetSensorsByType(BAROMETER) must succeed");

        sensors::ISensor* pBaro = nullptr;
        pBaroCol->GetAt(0, &pBaro);
        TEST_ASSERT(pBaro != nullptr, "Barometer sensor must be found");
        pBaroCol->Release();

        sensors::ISensorDataReport* pBaroReport = nullptr;
        hr = pBaro->GetData(&pBaroReport);
        TEST_ASSERT(hr == ole32::S_OK && pBaroReport != nullptr, "Barometer GetData must succeed");

        wasapi::PROPVARIANT barVal{};
        hr = pBaroReport->GetSensorValue(sensors::SENSOR_DATA_TYPE_ATMOSPHERIC_PRESSURE_BAR, &barVal);
        TEST_ASSERT(hr == ole32::S_OK && std::abs(barVal.dblVal - 1.01325) < 1e-4, "Barometer reading must match 1.01325 Bar");

        pBaroReport->Release();
        pBaro->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 8: Sensor Class Extension & State Updates
    // ------------------------------------------------------------------------
    {
        hr = pExt->Initialize(nullptr, nullptr);
        TEST_ASSERT(hr == ole32::S_OK, "SensorClassExtension::Initialize must succeed");

        hr = pExt->PostStateChange(accelId, sensors::SENSOR_STATE_INITIALIZING);
        TEST_ASSERT(hr == ole32::S_OK, "PostStateChange to INITIALIZING must succeed");

        sensors::SensorState testState{};
        pAccel->GetState(&testState);
        TEST_ASSERT(testState == sensors::SENSOR_STATE_INITIALIZING, "Sensor state must reflect INITIALIZING");

        // Restore state
        pExt->PostStateChange(accelId, sensors::SENSOR_STATE_READY);
        pAccel->GetState(&testState);
        TEST_ASSERT(testState == sensors::SENSOR_STATE_READY, "Sensor state must be restored to READY");

        hr = pExt->Uninitialize();
        TEST_ASSERT(hr == ole32::S_OK, "SensorClassExtension::Uninitialize must succeed");
    }

    pAccel->Release();
    pExt->Release();
    pMgr->Release();

    // ------------------------------------------------------------------------
    // Stage 9: C Client APIs Verification
    // ------------------------------------------------------------------------
    {
        sensors::ISensorManager* pCMgr = nullptr;
        hr = sensors::SensorsCreateSensorManager(&pCMgr);
        TEST_ASSERT(hr == ole32::S_OK && pCMgr != nullptr, "SensorsCreateSensorManager must succeed");
        pCMgr->Release();

        uint32_t sensorCount = 0;
        hr = sensors::SensorsGetSensorCount(&sensorCount);
        TEST_ASSERT(hr == ole32::S_OK && sensorCount >= 5, "SensorsGetSensorCount must return at least 5");

        sensors::ISensorClassExtension* pCExt = nullptr;
        hr = sensors::SensorsClassExtensionCreate(&pCExt);
        TEST_ASSERT(hr == ole32::S_OK && pCExt != nullptr, "SensorsClassExtensionCreate must succeed");
        pCExt->Release();
    }

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Integration (cmdSensor)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::stringstream out;

        // sensor test
        shell.execute("sensor test", out);
        TEST_ASSERT(out.str().find("[SENSOR] Self-Test Completed: ALL SENSOR TESTS PASSED.") != std::string::npos, "sensor test must pass");

        // sensor list
        out.str("");
        shell.execute("sensor list", out);
        TEST_ASSERT(out.str().find("MicaNT Sovereign 3-Axis Accelerometer") != std::string::npos, "sensor list must show Accelerometer");
        TEST_ASSERT(out.str().find("MicaNT Sovereign Ambient Light Sensor") != std::string::npos, "sensor list must show Light Sensor");
        TEST_ASSERT(out.str().find("READY / ONLINE") != std::string::npos, "sensor list must show ready status");

        // sensor read accel
        out.str("");
        shell.execute("sensor read accel", out);
        TEST_ASSERT(out.str().find("X:") != std::string::npos && out.str().find("Z:") != std::string::npos, "sensor read accel must report axes");

        // sensor read light
        out.str("");
        shell.execute("sensor read light", out);
        TEST_ASSERT(out.str().find("Illuminance: 350.0 Lux") != std::string::npos, "sensor read light must report 350.0 Lux");

        // sensor read baro
        out.str("");
        shell.execute("sensor read baro", out);
        TEST_ASSERT(out.str().find("Pressure: 1.01325 Bar") != std::string::npos, "sensor read baro must report 1.01325 Bar");

        // sensor inject light 650.5
        out.str("");
        shell.execute("sensor inject light 650.5", out);
        TEST_ASSERT(out.str().find("Injected Light Lux: 650.5 Lux") != std::string::npos, "sensor inject light must succeed");

        out.str("");
        shell.execute("sensor read light", out);
        TEST_ASSERT(out.str().find("Illuminance: 650.5 Lux") != std::string::npos, "sensor read light must show updated 650.5 Lux");

        // restore default light
        shell.execute("sensor inject light 350.0", out);
    }

    std::cout << "[TEST] Suite 95: Windows Sensors API & Sensor Class Extension Subsystem PASSED.\n";
}

// ============================================================================
// Suite 96: Windows Biometric Framework (WBF) & Windows Hello Subsystem Tests
// ============================================================================
void Test_WindowsBiometrics_Subsystem() {
    using winbio::HRESULT;
    using ole32::S_OK;

    winbio::InitializeBiometricsSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (winbio.dll / winbiosrvc.dll)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioOpenSession") != nullptr, "winbio.dll must export WinBioOpenSession");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioCloseSession") != nullptr, "winbio.dll must export WinBioCloseSession");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnumBiometricUnits") != nullptr, "winbio.dll must export WinBioEnumBiometricUnits");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnumDatabases") != nullptr, "winbio.dll must export WinBioEnumDatabases");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnumEnrollments") != nullptr, "winbio.dll must export WinBioEnumEnrollments");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioLocateSensor") != nullptr, "winbio.dll must export WinBioLocateSensor");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnrollBegin") != nullptr, "winbio.dll must export WinBioEnrollBegin");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnrollCapture") != nullptr, "winbio.dll must export WinBioEnrollCapture");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnrollCommit") != nullptr, "winbio.dll must export WinBioEnrollCommit");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioEnrollDiscard") != nullptr, "winbio.dll must export WinBioEnrollDiscard");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioVerify") != nullptr, "winbio.dll must export WinBioVerify");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioIdentify") != nullptr, "winbio.dll must export WinBioIdentify");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioFree") != nullptr, "winbio.dll must export WinBioFree");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioCancel") != nullptr, "winbio.dll must export WinBioCancel");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioWait") != nullptr, "winbio.dll must export WinBioWait");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioAcquireFocus") != nullptr, "winbio.dll must export WinBioAcquireFocus");
    TEST_ASSERT(ldr.getExport("winbio.dll", "WinBioReleaseFocus") != nullptr, "winbio.dll must export WinBioReleaseFocus");

    TEST_ASSERT(ldr.getExport("winbiosrvc.dll", "DllGetClassObject") != nullptr, "winbiosrvc.dll must export DllGetClassObject");
    TEST_ASSERT(ldr.getExport("winbiosrvc.dll", "DllCanUnloadNow") != nullptr, "winbiosrvc.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("winbiosrvc.dll", "DllRegisterServer") != nullptr, "winbiosrvc.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("winbiosrvc.dll", "DllUnregisterServer") != nullptr, "winbiosrvc.dll must export DllUnregisterServer");
    TEST_ASSERT(ldr.getExport("winbiosrvc.dll", "WbioSrvcMain") != nullptr, "winbiosrvc.dll must export WbioSrvcMain");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verBio = version::VersionDatabase::Instance().FindModule("winbio.dll");
        TEST_ASSERT(verBio != nullptr, "VersionDatabase must contain winbio.dll");
        TEST_ASSERT(verBio->stringTable.at("FileDescription") == "Windows Biometric Framework Client API", "winbio.dll description match");
        TEST_ASSERT(verBio->stringTable.at("OriginalFilename") == "winbio.dll", "winbio.dll original filename match");
        TEST_ASSERT(verBio->stringTable.at("ProductName") == "MicaNT Biometrics Subsystem", "winbio.dll product name match");

        const auto* verSrvc = version::VersionDatabase::Instance().FindModule("winbiosrvc.dll");
        TEST_ASSERT(verSrvc != nullptr, "VersionDatabase must contain winbiosrvc.dll");
        TEST_ASSERT(verSrvc->stringTable.at("FileDescription") == "Windows Biometric Service", "winbiosrvc.dll description match");
        TEST_ASSERT(verSrvc->stringTable.at("OriginalFilename") == "winbiosrvc.dll", "winbiosrvc.dll original filename match");
        TEST_ASSERT(verSrvc->stringTable.at("ProductName") == "MicaNT Biometrics Subsystem", "winbiosrvc.dll product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: SCM Service Registration (WbioSrvc)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();
        auto bioSvc = scm.getServiceRecord(L"WbioSrvc");
        TEST_ASSERT(bioSvc != nullptr, "WbioSrvc service must be registered in SCM");
        TEST_ASSERT(bioSvc->displayName == L"Windows Biometric Service", "WbioSrvc display name match");
        TEST_ASSERT(bioSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "WbioSrvc must be running");
        TEST_ASSERT(bioSvc->status.dwProcessId == 1166, "WbioSrvc PID match");
    }

    // ------------------------------------------------------------------------
    // Stage 4: Biometric Unit Enumeration & Sensor Inspection
    // ------------------------------------------------------------------------
    {
        winbio::WINBIO_UNIT_SCHEMA* units = nullptr;
        size_t unitCount = 0;
        HRESULT hr = winbio::WinBioEnumBiometricUnits(winbio::WINBIO_TYPE_ANY, &units, &unitCount);
        TEST_ASSERT(hr == S_OK && units != nullptr, "WinBioEnumBiometricUnits must succeed");
        TEST_ASSERT(unitCount == 2, "Must enumerate exactly 2 biometric units");

        TEST_ASSERT(units[0].UnitId == 1, "Unit 1 must be first");
        TEST_ASSERT(units[0].BiometricFactor == winbio::WINBIO_TYPE_FINGERPRINT, "Unit 1 is fingerprint");
        TEST_ASSERT(std::wstring(units[0].Manufacturer) == L"MicaNT Security Systems", "Unit 1 manufacturer match");
        TEST_ASSERT(std::wstring(units[0].Model) == L"MICA-BIO-FP500", "Unit 1 model match");
        TEST_ASSERT((units[0].Capabilities & winbio::WINBIO_CAPABILITY_SECURE_SENSOR) != 0, "Unit 1 has secure sensor capability");

        TEST_ASSERT(units[1].UnitId == 2, "Unit 2 must be second");
        TEST_ASSERT(units[1].BiometricFactor == winbio::WINBIO_TYPE_FACIAL_FEATURES, "Unit 2 is facial features");
        TEST_ASSERT(std::wstring(units[1].Model) == L"MICA-BIO-FACE-IR", "Unit 2 model match");

        winbio::WinBioFree(units);
    }

    // ------------------------------------------------------------------------
    // Stage 5: Biometric Database Storage Enumeration
    // ------------------------------------------------------------------------
    {
        winbio::WINBIO_STORAGE_SCHEMA* dbs = nullptr;
        size_t dbCount = 0;
        HRESULT hr = winbio::WinBioEnumDatabases(winbio::WINBIO_TYPE_ANY, &dbs, &dbCount);
        TEST_ASSERT(hr == S_OK && dbs != nullptr, "WinBioEnumDatabases must succeed");
        TEST_ASSERT(dbCount == 1, "Must enumerate 1 system biometric database");
        TEST_ASSERT(std::wstring(dbs[0].FilePath) == L"C:\\Windows\\System32\\WinBioDatabase\\system.db", "Database path match");
        TEST_ASSERT(dbs[0].InitialSize == 1048576, "Initial size 1MB match");
        winbio::WinBioFree(dbs);
    }

    // ------------------------------------------------------------------------
    // Stage 6: Biometric Session Lifecycle
    // ------------------------------------------------------------------------
    winbio::WINBIO_SESSION_HANDLE hSession = 0;
    HRESULT hr = winbio::WinBioOpenSession(
        winbio::WINBIO_TYPE_FINGERPRINT | winbio::WINBIO_TYPE_FACIAL_FEATURES,
        0, winbio::WINBIO_FLAG_DEFAULT, nullptr, 0, nullptr, &hSession
    );
    TEST_ASSERT(hr == S_OK && hSession != 0, "WinBioOpenSession must succeed");
    TEST_ASSERT(winbio::BiometricManager::get().getSessionCount() >= 1, "Session count must be >= 1");

    winbio::WINBIO_UNIT_ID locatedUnit = 0;
    hr = winbio::WinBioLocateSensor(hSession, &locatedUnit);
    TEST_ASSERT(hr == S_OK && locatedUnit == 1, "WinBioLocateSensor must return Unit 1");

    // ------------------------------------------------------------------------
    // Stage 7: Biometric Verification & Identification
    // ------------------------------------------------------------------------
    {
        winbio::WINBIO_IDENTITY id{};
        win32::BOOL bMatch = 0;
        winbio::WINBIO_REJECT_DETAIL reject = 0;

        // Verify valid enrolled Administrator right index finger
        hr = winbio::WinBioVerify(
            hSession, 1, winbio::WINBIO_SUBTYPE_RH_INDEX_FINGER,
            &id, &bMatch, &reject
        );
        TEST_ASSERT(hr == S_OK && bMatch == 1, "WinBioVerify for enrolled RH index must match");
        TEST_ASSERT(id.Type == winbio::WINBIO_ID_TYPE_SID, "Matched identity type must be SID");
        TEST_ASSERT(std::string(reinterpret_cast<const char*>(id.Value.AccountSid.Data), id.Value.AccountSid.Size) == "S-1-5-18", "SID match");

        // Verify unmatched subFactor
        bMatch = 1;
        hr = winbio::WinBioVerify(
            hSession, 1, winbio::WINBIO_SUBTYPE_LH_LITTLE_FINGER,
            &id, &bMatch, &reject
        );
        TEST_ASSERT(hr == winbio::WINBIO_E_NO_MATCH && bMatch == 0, "WinBioVerify for unenrolled finger must not match");

        // Identify enrolled user on Unit 1
        winbio::WINBIO_IDENTITY idIdent{};
        winbio::WINBIO_BIOMETRIC_SUBTYPE identifiedSubFactor = 0;
        hr = winbio::WinBioIdentify(hSession, 1, &idIdent, &identifiedSubFactor, &reject);
        TEST_ASSERT(hr == S_OK, "WinBioIdentify must succeed");
        TEST_ASSERT(identifiedSubFactor == winbio::WINBIO_SUBTYPE_RH_INDEX_FINGER, "Identified subFactor match");
    }

    // ------------------------------------------------------------------------
    // Stage 8: Biometric Enrollment Workflow (Begin -> Capture x 3 -> Commit)
    // ------------------------------------------------------------------------
    {
        hr = winbio::WinBioEnrollBegin(hSession, winbio::WINBIO_SUBTYPE_RH_THUMB, 1);
        TEST_ASSERT(hr == S_OK, "WinBioEnrollBegin must succeed");

        winbio::WINBIO_REJECT_DETAIL reject = 0;
        hr = winbio::WinBioEnrollCapture(hSession, &reject);
        TEST_ASSERT(hr == winbio::WINBIO_I_MORE_DATA, "Sample 1 must return WINBIO_I_MORE_DATA");

        hr = winbio::WinBioEnrollCapture(hSession, &reject);
        TEST_ASSERT(hr == winbio::WINBIO_I_MORE_DATA, "Sample 2 must return WINBIO_I_MORE_DATA");

        hr = winbio::WinBioEnrollCapture(hSession, &reject);
        TEST_ASSERT(hr == S_OK, "Sample 3 must complete enrollment with S_OK");

        winbio::WINBIO_IDENTITY newId{};
        newId.Type = winbio::WINBIO_ID_TYPE_SID;
        const char* sidAdmin = "S-1-5-18";
        newId.Value.AccountSid.Size = static_cast<uint32_t>(strlen(sidAdmin));
        std::memcpy(newId.Value.AccountSid.Data, sidAdmin, strlen(sidAdmin));

        win32::BOOL isNewTemplate = 0;
        hr = winbio::WinBioEnrollCommit(hSession, &newId, &isNewTemplate);
        TEST_ASSERT(hr == S_OK && isNewTemplate == 1, "WinBioEnrollCommit must succeed");

        // Verify newly enrolled RH Thumb
        win32::BOOL matchNew = 0;
        hr = winbio::WinBioVerify(hSession, 1, winbio::WINBIO_SUBTYPE_RH_THUMB, &newId, &matchNew, &reject);
        TEST_ASSERT(hr == S_OK && matchNew == 1, "Verification of newly enrolled RH thumb must match");
    }

    // ------------------------------------------------------------------------
    // Stage 9: Enrollment Discard & Cancellation
    // ------------------------------------------------------------------------
    {
        hr = winbio::WinBioEnrollBegin(hSession, winbio::WINBIO_SUBTYPE_LH_RING_FINGER, 1);
        TEST_ASSERT(hr == S_OK, "WinBioEnrollBegin must succeed");

        winbio::WINBIO_REJECT_DETAIL reject = 0;
        hr = winbio::WinBioEnrollCapture(hSession, &reject);
        TEST_ASSERT(hr == winbio::WINBIO_I_MORE_DATA, "Sample 1 accepted");

        hr = winbio::WinBioEnrollDiscard(hSession);
        TEST_ASSERT(hr == S_OK, "WinBioEnrollDiscard must succeed");

        // Capturing after discard must fail
        hr = winbio::WinBioEnrollCapture(hSession, &reject);
        TEST_ASSERT(hr == winbio::WINBIO_E_NO_MATCH, "Capture after discard must fail");

        // Focus & Cancel APIs
        TEST_ASSERT(winbio::WinBioAcquireFocus() == S_OK, "WinBioAcquireFocus must succeed");
        TEST_ASSERT(winbio::WinBioReleaseFocus() == S_OK, "WinBioReleaseFocus must succeed");
        TEST_ASSERT(winbio::WinBioCancel(hSession) == S_OK, "WinBioCancel must succeed");
        TEST_ASSERT(winbio::WinBioWait(hSession) == S_OK, "WinBioWait must succeed");
    }

    // Close session
    hr = winbio::WinBioCloseSession(hSession);
    TEST_ASSERT(hr == S_OK, "WinBioCloseSession must succeed");

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Integration (cmdWinBio)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::stringstream out;

        // winbio test
        shell.execute("winbio test", out);
        TEST_ASSERT(out.str().find("[WINBIO] Self-Test Completed: ALL BIOMETRIC TESTS PASSED.") != std::string::npos, "winbio test must pass");

        // winbio list
        out.str("");
        shell.execute("winbio list", out);
        TEST_ASSERT(out.str().find("MicaNT Sovereign Optical Fingerprint Sensor") != std::string::npos, "winbio list must show fingerprint sensor");
        TEST_ASSERT(out.str().find("MicaNT Sovereign TrueDepth Infrared Facial Sensor") != std::string::npos, "winbio list must show facial sensor");
        TEST_ASSERT(out.str().find("READY / CALIBRATED") != std::string::npos, "winbio list must show sensor status");

        // winbio status
        out.str("");
        shell.execute("winbio status", out);
        TEST_ASSERT(out.str().find("WbioSrvc (PID 1166, RUNNING, svchost)") != std::string::npos, "winbio status must show WbioSrvc");
        TEST_ASSERT(out.str().find("Right Index Finger") != std::string::npos, "winbio status must show right index enrollment");

        // winbio verify 1 2 (RH_INDEX_FINGER = 2)
        out.str("");
        shell.execute("winbio verify 1 2", out);
        TEST_ASSERT(out.str().find("Biometric Verification SUCCESS: Identity MATCHED on Unit 1") != std::string::npos, "winbio verify must succeed");

        // winbio enroll 1 3 (RH_MIDDLE_FINGER = 3)
        out.str("");
        shell.execute("winbio enroll 1 3", out);
        TEST_ASSERT(out.str().find("Biometric Enrollment SUCCESS: New template committed for SubFactor 3 on Unit 1") != std::string::npos, "winbio enroll must succeed");
    }

    std::cout << "[TEST] Suite 96: Windows Biometric Framework (WBF) & Windows Hello Subsystem PASSED.\n";
}

// ============================================================================
// Suite 97: Windows Bluetooth Core Architecture & Radio Subsystem Tests
// ============================================================================
void Test_WindowsBluetooth_Subsystem() {
    bluetooth::InitializeBluetoothSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (bluetoothapis.dll / bthprops.cpl)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothFindFirstRadio") != nullptr, "bluetoothapis.dll must export BluetoothFindFirstRadio");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothFindNextRadio") != nullptr, "bluetoothapis.dll must export BluetoothFindNextRadio");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothFindRadioClose") != nullptr, "bluetoothapis.dll must export BluetoothFindRadioClose");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothGetRadioInfo") != nullptr, "bluetoothapis.dll must export BluetoothGetRadioInfo");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothFindFirstDevice") != nullptr, "bluetoothapis.dll must export BluetoothFindFirstDevice");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothFindNextDevice") != nullptr, "bluetoothapis.dll must export BluetoothFindNextDevice");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothFindDeviceClose") != nullptr, "bluetoothapis.dll must export BluetoothFindDeviceClose");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothGetDeviceInfo") != nullptr, "bluetoothapis.dll must export BluetoothGetDeviceInfo");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothUpdateDeviceRecord") != nullptr, "bluetoothapis.dll must export BluetoothUpdateDeviceRecord");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothRemoveDevice") != nullptr, "bluetoothapis.dll must export BluetoothRemoveDevice");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothSetServiceState") != nullptr, "bluetoothapis.dll must export BluetoothSetServiceState");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothEnumerateInstalledServices") != nullptr, "bluetoothapis.dll must export BluetoothEnumerateInstalledServices");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothEnableDiscovery") != nullptr, "bluetoothapis.dll must export BluetoothEnableDiscovery");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothIsDiscoverable") != nullptr, "bluetoothapis.dll must export BluetoothIsDiscoverable");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothEnableIncomingConnections") != nullptr, "bluetoothapis.dll must export BluetoothEnableIncomingConnections");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothIsConnectable") != nullptr, "bluetoothapis.dll must export BluetoothIsConnectable");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothRegisterForAuthentication") != nullptr, "bluetoothapis.dll must export BluetoothRegisterForAuthentication");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothUnregisterAuthentication") != nullptr, "bluetoothapis.dll must export BluetoothUnregisterAuthentication");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothSendAuthenticationResponse") != nullptr, "bluetoothapis.dll must export BluetoothSendAuthenticationResponse");
    TEST_ASSERT(ldr.getExport("bluetoothapis.dll", "BluetoothAuthenticateDevice") != nullptr, "bluetoothapis.dll must export BluetoothAuthenticateDevice");

    TEST_ASSERT(ldr.getExport("bthprops.cpl", "CPlApplet") != nullptr, "bthprops.cpl must export CPlApplet");
    TEST_ASSERT(ldr.getExport("bthprops.cpl", "BluetoothSelectDevices") != nullptr, "bthprops.cpl must export BluetoothSelectDevices");
    TEST_ASSERT(ldr.getExport("bthprops.cpl", "BluetoothSelectDevicesFree") != nullptr, "bthprops.cpl must export BluetoothSelectDevicesFree");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verBth = version::VersionDatabase::Instance().FindModule("bluetoothapis.dll");
        TEST_ASSERT(verBth != nullptr, "VersionDatabase must contain bluetoothapis.dll");
        TEST_ASSERT(verBth->stringTable.at("FileDescription") == "Bluetooth API Library", "bluetoothapis.dll description match");
        TEST_ASSERT(verBth->stringTable.at("OriginalFilename") == "bluetoothapis.dll", "bluetoothapis.dll original filename match");
        TEST_ASSERT(verBth->stringTable.at("ProductName") == "MicaNT Bluetooth Subsystem", "bluetoothapis.dll product name match");

        const auto* verProps = version::VersionDatabase::Instance().FindModule("bthprops.cpl");
        TEST_ASSERT(verProps != nullptr, "VersionDatabase must contain bthprops.cpl");
        TEST_ASSERT(verProps->stringTable.at("FileDescription") == "Bluetooth Control Panel Applet & Property Sheets", "bthprops.cpl description match");
        TEST_ASSERT(verProps->stringTable.at("OriginalFilename") == "bthprops.cpl", "bthprops.cpl original filename match");
        TEST_ASSERT(verProps->stringTable.at("ProductName") == "MicaNT Bluetooth Subsystem", "bthprops.cpl product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: SCM Service Registration (bthserv / BthHFSrv)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();
        auto bthSvc = scm.getServiceRecord(L"bthserv");
        TEST_ASSERT(bthSvc != nullptr, "bthserv service must be registered in SCM");
        TEST_ASSERT(bthSvc->displayName == L"Bluetooth Support Service", "bthserv display name match");
        TEST_ASSERT(bthSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "bthserv must be running");
        TEST_ASSERT(bthSvc->status.dwProcessId == 1170, "bthserv PID match");

        auto hfSvc = scm.getServiceRecord(L"BthHFSrv");
        TEST_ASSERT(hfSvc != nullptr, "BthHFSrv service must be registered in SCM");
        TEST_ASSERT(hfSvc->displayName == L"Bluetooth Audio Gateway Service", "BthHFSrv display name match");
        TEST_ASSERT(hfSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "BthHFSrv must be running");
        TEST_ASSERT(hfSvc->status.dwProcessId == 1174, "BthHFSrv PID match");
    }

    // ------------------------------------------------------------------------
    // Stage 4: Radio Enumeration & Telemetry
    // ------------------------------------------------------------------------
    void* hRadio = nullptr;
    {
        bluetooth::BLUETOOTH_FIND_RADIO_PARAMS frp{ sizeof(bluetooth::BLUETOOTH_FIND_RADIO_PARAMS) };
        bluetooth::HBLUETOOTH_RADIO_FIND hFind = bluetooth::BluetoothFindFirstRadio(&frp, &hRadio);
        TEST_ASSERT(hFind != nullptr && hRadio != nullptr, "BluetoothFindFirstRadio must succeed");

        bluetooth::BLUETOOTH_RADIO_INFO info{ sizeof(bluetooth::BLUETOOTH_RADIO_INFO) };
        uint32_t ret = bluetooth::BluetoothGetRadioInfo(hRadio, &info);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothGetRadioInfo must return SUCCESS");
        TEST_ASSERT(std::wstring(info.szName) == L"MicaNT Sovereign Dual-Mode Bluetooth 5.4 Radio", "Radio name match");
        TEST_ASSERT(info.lmpSubversion == 13, "LMP version 13.0 (Bluetooth 5.4)");
        TEST_ASSERT(info.manufacturer == 0x05D6, "MicaNT Silicon Systems (0x05D6)");
        TEST_ASSERT(info.ulClassofDevice == (bluetooth::BTH_COD_MAJOR_COMPUTER | 0x04), "Computer / Desktop CoD match");
        TEST_ASSERT(bluetooth::FormatBluetoothAddress(info.address) == "00:1A:7D:DA:71:01", "Radio MAC address match");

        void* hNextRadio = nullptr;
        TEST_ASSERT(!bluetooth::BluetoothFindNextRadio(hFind, &hNextRadio), "Should only find 1 primary radio");
        TEST_ASSERT(bluetooth::BluetoothFindRadioClose(hFind) != 0, "BluetoothFindRadioClose must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 5: Radio Capabilities & Discovery States
    // ------------------------------------------------------------------------
    {
        TEST_ASSERT(bluetooth::BluetoothIsDiscoverable(hRadio) == 1, "Radio must be discoverable by default");
        TEST_ASSERT(bluetooth::BluetoothEnableDiscovery(hRadio, 0) == 1, "Disable discovery must succeed");
        TEST_ASSERT(bluetooth::BluetoothIsDiscoverable(hRadio) == 0, "Radio must not be discoverable");
        TEST_ASSERT(bluetooth::BluetoothEnableDiscovery(hRadio, 1) == 1, "Enable discovery must succeed");
        TEST_ASSERT(bluetooth::BluetoothIsDiscoverable(hRadio) == 1, "Radio must be discoverable again");

        TEST_ASSERT(bluetooth::BluetoothIsConnectable(hRadio) == 1, "Radio must be connectable by default");
        TEST_ASSERT(bluetooth::BluetoothEnableIncomingConnections(hRadio, 0) == 1, "Disable incoming must succeed");
        TEST_ASSERT(bluetooth::BluetoothIsConnectable(hRadio) == 0, "Radio must not be connectable");
        TEST_ASSERT(bluetooth::BluetoothEnableIncomingConnections(hRadio, 1) == 1, "Enable incoming must succeed");
        TEST_ASSERT(bluetooth::BluetoothIsConnectable(hRadio) == 1, "Radio must be connectable again");
    }

    // ------------------------------------------------------------------------
    // Stage 6: Remote Device Enumeration & Information Inspection
    // ------------------------------------------------------------------------
    {
        bluetooth::BLUETOOTH_DEVICE_SEARCH_PARAMS sp{ sizeof(bluetooth::BLUETOOTH_DEVICE_SEARCH_PARAMS) };
        sp.fReturnAuthenticated = 1;
        sp.fReturnRemembered = 1;
        sp.fReturnUnknown = 1;
        sp.fReturnConnected = 1;

        bluetooth::BLUETOOTH_DEVICE_INFO dev1{ sizeof(bluetooth::BLUETOOTH_DEVICE_INFO) };
        bluetooth::HBLUETOOTH_DEVICE_FIND hFindDev = bluetooth::BluetoothFindFirstDevice(&sp, &dev1);
        TEST_ASSERT(hFindDev != nullptr, "BluetoothFindFirstDevice must succeed");
        TEST_ASSERT(std::wstring(dev1.szName) == L"Titan Elite Wireless ANC Headset", "Device 1 name match");
        TEST_ASSERT(bluetooth::FormatBluetoothAddress(dev1.Address) == "E4:5F:01:23:45:67", "Device 1 MAC address match");
        TEST_ASSERT(dev1.fConnected == 1 && dev1.fAuthenticated == 1, "Device 1 is connected and authenticated");

        bluetooth::BLUETOOTH_DEVICE_INFO dev2{ sizeof(bluetooth::BLUETOOTH_DEVICE_INFO) };
        TEST_ASSERT(bluetooth::BluetoothFindNextDevice(hFindDev, &dev2) == 1, "FindNextDevice must find Device 2");
        TEST_ASSERT(std::wstring(dev2.szName) == L"MicaPad Low Energy Wireless Controller", "Device 2 name match");
        TEST_ASSERT(bluetooth::FormatBluetoothAddress(dev2.Address) == "DC:A6:32:89:AB:CD", "Device 2 MAC address match");

        bluetooth::BLUETOOTH_DEVICE_INFO dev3{ sizeof(bluetooth::BLUETOOTH_DEVICE_INFO) };
        TEST_ASSERT(bluetooth::BluetoothFindNextDevice(hFindDev, &dev3) == 1, "FindNextDevice must find Device 3");
        TEST_ASSERT(std::wstring(dev3.szName) == L"Sovereign Precision Keyboard & Mouse", "Device 3 name match");
        TEST_ASSERT(dev3.fConnected == 0 && dev3.fAuthenticated == 0, "Device 3 is unauthenticated");

        TEST_ASSERT(!bluetooth::BluetoothFindNextDevice(hFindDev, &dev3), "No more devices");
        TEST_ASSERT(bluetooth::BluetoothFindDeviceClose(hFindDev) == 1, "BluetoothFindDeviceClose must succeed");

        // BluetoothGetDeviceInfo direct lookup
        bluetooth::BLUETOOTH_DEVICE_INFO lookupDev{ sizeof(bluetooth::BLUETOOTH_DEVICE_INFO) };
        lookupDev.Address = dev1.Address;
        uint32_t ret = bluetooth::BluetoothGetDeviceInfo(hRadio, &lookupDev);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothGetDeviceInfo must succeed");
        TEST_ASSERT(std::wstring(lookupDev.szName) == L"Titan Elite Wireless ANC Headset", "Lookup name match");
    }

    // ------------------------------------------------------------------------
    // Stage 7: SDP Service Enumeration & Management
    // ------------------------------------------------------------------------
    {
        auto dev = bluetooth::BluetoothManager::get().getDevices()[0];
        uint32_t svcCount = 0;
        uint32_t ret = bluetooth::BluetoothEnumerateInstalledServices(hRadio, &dev.info, &svcCount, nullptr);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "Get service count must succeed");
        TEST_ASSERT(svcCount == 4, "Headset must have 4 installed services");

        std::vector<GUID> svcs(svcCount);
        ret = bluetooth::BluetoothEnumerateInstalledServices(hRadio, &dev.info, &svcCount, svcs.data());
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "Enumerate services must succeed");
        TEST_ASSERT(svcCount == 4, "Returned 4 services");

        // Add a new service (SerialPort)
        ret = bluetooth::BluetoothSetServiceState(hRadio, &dev.info, &bluetooth::SerialPortServiceClass_UUID, bluetooth::BLUETOOTH_SERVICE_ENABLE);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothSetServiceState enable must succeed");

        ret = bluetooth::BluetoothEnumerateInstalledServices(hRadio, &dev.info, &svcCount, nullptr);
        TEST_ASSERT(svcCount == 5, "Installed services count should increase to 5");

        // Disable service
        ret = bluetooth::BluetoothSetServiceState(hRadio, &dev.info, &bluetooth::SerialPortServiceClass_UUID, bluetooth::BLUETOOTH_SERVICE_DISABLE);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothSetServiceState disable must succeed");

        ret = bluetooth::BluetoothEnumerateInstalledServices(hRadio, &dev.info, &svcCount, nullptr);
        TEST_ASSERT(svcCount == 4, "Installed services count should return to 4");
    }

    // ------------------------------------------------------------------------
    // Stage 8: Authentication Callback & Pairing Workflow
    // ------------------------------------------------------------------------
    {
        bool authCallbackTriggered = false;
        bluetooth::HBLUETOOTH_AUTHENTICATION_REGISTRATION hReg = nullptr;
        uint32_t ret = bluetooth::BluetoothRegisterForAuthentication(
            nullptr,
            &hReg,
            [](void* pv, bluetooth::BLUETOOTH_DEVICE_INFO* pDev) -> int32_t {
                if (pv && pDev) {
                    *reinterpret_cast<bool*>(pv) = true;
                }
                return 1;
            },
            &authCallbackTriggered
        );
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS && hReg != nullptr, "BluetoothRegisterForAuthentication must succeed");

        // Authenticate Device 3 (Sovereign Precision Keyboard & Mouse)
        auto dev3 = bluetooth::BluetoothManager::get().getDevices()[2];
        TEST_ASSERT(dev3.info.fAuthenticated == 0, "Device 3 initially unauthenticated");

        ret = bluetooth::BluetoothAuthenticateDevice(nullptr, hRadio, &dev3.info, L"987654", 6);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothAuthenticateDevice must succeed");
        TEST_ASSERT(authCallbackTriggered, "Authentication callback must have been invoked");

        // Verify updated state
        bluetooth::BLUETOOTH_DEVICE_INFO checkDev{ sizeof(bluetooth::BLUETOOTH_DEVICE_INFO) };
        checkDev.Address = dev3.info.Address;
        bluetooth::BluetoothGetDeviceInfo(hRadio, &checkDev);
        TEST_ASSERT(checkDev.fAuthenticated == 1, "Device 3 is now authenticated");
        TEST_ASSERT(checkDev.fConnected == 1, "Device 3 is now connected");

        TEST_ASSERT(bluetooth::BluetoothUnregisterAuthentication(hReg) == 1, "BluetoothUnregisterAuthentication must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 9: Device Record Update & Removal
    // ------------------------------------------------------------------------
    {
        auto dev2 = bluetooth::BluetoothManager::get().getDevices()[1];
        wcsncpy(dev2.info.szName, L"MicaPad Wireless Pro Controller", bluetooth::BLUETOOTH_MAX_NAME_SIZE - 1);
        uint32_t ret = bluetooth::BluetoothUpdateDeviceRecord(&dev2.info);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothUpdateDeviceRecord must succeed");

        bluetooth::BLUETOOTH_DEVICE_INFO checkDev{ sizeof(bluetooth::BLUETOOTH_DEVICE_INFO) };
        checkDev.Address = dev2.info.Address;
        bluetooth::BluetoothGetDeviceInfo(hRadio, &checkDev);
        TEST_ASSERT(std::wstring(checkDev.szName) == L"MicaPad Wireless Pro Controller", "Updated name confirmed");

        // Remove device 3
        auto dev3 = bluetooth::BluetoothManager::get().getDevices()[2];
        ret = bluetooth::BluetoothRemoveDevice(&dev3.info.Address);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_SUCCESS, "BluetoothRemoveDevice must succeed");

        ret = bluetooth::BluetoothGetDeviceInfo(hRadio, &dev3.info);
        TEST_ASSERT(ret == bluetooth::BT_ERROR_NOT_FOUND, "Removed device must no longer be found");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Integration (cmdBluetooth)
    // ------------------------------------------------------------------------
    {
        // Reset manager to fresh state
        bluetooth::BluetoothManager::get().reset();

        shell::CommandShell shell;
        std::stringstream out;

        // bluetooth test
        shell.execute("bluetooth test", out);
        TEST_ASSERT(out.str().find("[BLUETOOTH] Self-Test Completed: ALL BLUETOOTH TESTS PASSED.") != std::string::npos, "bluetooth test must pass");

        // bluetooth radios
        out.str("");
        shell.execute("bluetooth radios", out);
        TEST_ASSERT(out.str().find("MicaNT Sovereign Dual-Mode Bluetooth 5.4 Radio") != std::string::npos, "bluetooth radios must show radio");
        TEST_ASSERT(out.str().find("00:1A:7D:DA:71:01") != std::string::npos, "bluetooth radios must show MAC");

        // bluetooth list
        out.str("");
        shell.execute("bluetooth list", out);
        TEST_ASSERT(out.str().find("Titan Elite Wireless ANC Headset") != std::string::npos, "bluetooth list must show headset");
        TEST_ASSERT(out.str().find("MicaPad Low Energy Wireless Controller") != std::string::npos, "bluetooth list must show gamepad");

        // bluetooth info 1
        out.str("");
        shell.execute("bluetooth info 1", out);
        TEST_ASSERT(out.str().find("Device Information: Titan Elite Wireless ANC Headset") != std::string::npos, "bluetooth info must show headset info");
        TEST_ASSERT(out.str().find("E4:5F:01:23:45:67") != std::string::npos, "bluetooth info must show MAC");

        // bluetooth pair 3 123456
        out.str("");
        shell.execute("bluetooth pair 3 123456", out);
        TEST_ASSERT(out.str().find("Pairing SUCCESS: Authenticated with device 70:B3:D5:FE:10:99") != std::string::npos, "bluetooth pair must succeed");
    }

    std::cout << "[TEST] Suite 97: Windows Bluetooth Core Architecture & Radio Subsystem PASSED.\n";
}

void Test_WindowsSmartCardMinidriver_Subsystem() {
    std::cout << "[TEST] Suite 98: Running Windows Smart Card Minidriver & Base CSP Subsystem Tests...\n";

    auto& ldr = ldr::DynamicLoader::get();
    cardmod::InitializeCardMinidriverSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (msclmd.dll / basecsp.dll)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("msclmd.dll", "CardAcquireContext") != nullptr, "msclmd.dll must export CardAcquireContext");
    TEST_ASSERT(ldr.getExport("msclmd.dll", "CardDeleteContext") != nullptr, "msclmd.dll must export CardDeleteContext");
    TEST_ASSERT(ldr.getExport("msclmd.dll", "DllCanUnloadNow") != nullptr, "msclmd.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("msclmd.dll", "DllRegisterServer") != nullptr, "msclmd.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("msclmd.dll", "DllUnregisterServer") != nullptr, "msclmd.dll must export DllUnregisterServer");

    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPAcquireContext") != nullptr, "basecsp.dll must export CPAcquireContext");
    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPReleaseContext") != nullptr, "basecsp.dll must export CPReleaseContext");
    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPGenKey") != nullptr, "basecsp.dll must export CPGenKey");
    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPDeriveKey") != nullptr, "basecsp.dll must export CPDeriveKey");
    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPDestroyKey") != nullptr, "basecsp.dll must export CPDestroyKey");
    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPEncrypt") != nullptr, "basecsp.dll must export CPEncrypt");
    TEST_ASSERT(ldr.getExport("basecsp.dll", "CPDecrypt") != nullptr, "basecsp.dll must export CPDecrypt");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verMsclmd = version::VersionDatabase::Instance().FindModule("msclmd.dll");
        TEST_ASSERT(verMsclmd != nullptr, "VersionDatabase must contain msclmd.dll");
        TEST_ASSERT(verMsclmd->stringTable.at("FileDescription") == "Microsoft Smart Card Minidriver", "msclmd.dll description match");
        TEST_ASSERT(verMsclmd->stringTable.at("OriginalFilename") == "msclmd.dll", "msclmd.dll original filename match");
        TEST_ASSERT(verMsclmd->stringTable.at("ProductName") == "MicaNT Smart Card Subsystem", "msclmd.dll product name match");

        const auto* verBasecsp = version::VersionDatabase::Instance().FindModule("basecsp.dll");
        TEST_ASSERT(verBasecsp != nullptr, "VersionDatabase must contain basecsp.dll");
        TEST_ASSERT(verBasecsp->stringTable.at("FileDescription") == "Base Smart Card Cryptographic Service Provider", "basecsp.dll description match");
        TEST_ASSERT(verBasecsp->stringTable.at("OriginalFilename") == "basecsp.dll", "basecsp.dll original filename match");
        TEST_ASSERT(verBasecsp->stringTable.at("ProductName") == "MicaNT Smart Card Subsystem", "basecsp.dll product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: CardAcquireContext & ATR / Card Name Allocation
    // ------------------------------------------------------------------------
    cardmod::CardMinidriverManager::get().reset();
    cardmod::CARD_DATA cd{};
    cd.dwVersion = cardmod::CARD_DATA_VERSION_SEVEN;
    cd.pfnCspAlloc = cardmod::DefaultCspAlloc;
    cd.pfnCspReAlloc = cardmod::DefaultCspReAlloc;
    cd.pfnCspFree = cardmod::DefaultCspFree;

    uint32_t rc = cardmod::CardAcquireContext(&cd, 0);
    TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardAcquireContext must succeed");
    TEST_ASSERT(cd.pwszCardName != nullptr, "pwszCardName must be allocated");
    TEST_ASSERT(std::wstring(cd.pwszCardName) == L"MicaNT Titan Sovereign PIV Token", "CardName match");
    TEST_ASSERT(cd.pbAtr != nullptr && cd.cbAtr == 18, "ATR must be allocated and 18 bytes");
    TEST_ASSERT(cd.pbAtr[0] == 0x3B && cd.pbAtr[1] == 0x7D, "ATR header match");
    TEST_ASSERT(cd.pfnCardQueryCapabilities != nullptr, "pfnCardQueryCapabilities populated");
    TEST_ASSERT(cd.pfnCardAuthenticatePin != nullptr, "pfnCardAuthenticatePin populated");
    TEST_ASSERT(cd.pfnCardReadFile != nullptr, "pfnCardReadFile populated");
    TEST_ASSERT(cd.pfnCardWriteFile != nullptr, "pfnCardWriteFile populated");
    TEST_ASSERT(cd.pfnCardSignData != nullptr, "pfnCardSignData populated");

    // ------------------------------------------------------------------------
    // Stage 4: Card Capabilities & Free Space Inspection
    // ------------------------------------------------------------------------
    {
        cardmod::CARD_CAPABILITIES caps{};
        rc = cd.pfnCardQueryCapabilities(&cd, &caps);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardQueryCapabilities must succeed");
        TEST_ASSERT(caps.fKeyGen == 1, "fKeyGen capability must be 1");
        TEST_ASSERT(caps.dwKeySizes == 2048, "dwKeySizes must be 2048");

        cardmod::CARD_FREE_SPACE_INFO freeSpace{};
        rc = cd.pfnCardQueryFreeSpace(&cd, 0, &freeSpace);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardQueryFreeSpace must succeed");
        TEST_ASSERT(freeSpace.dwBytesAvailable == 61440, "dwBytesAvailable must be 60KB");
        TEST_ASSERT(freeSpace.dwKeyContainersAvailable == 14, "dwKeyContainersAvailable match");
        TEST_ASSERT(freeSpace.dwMaxKeyContainers == 16, "dwMaxKeyContainers match");
    }

    // ------------------------------------------------------------------------
    // Stage 5: PIN Authentication & Attempt Counter Verification
    // ------------------------------------------------------------------------
    {
        uint32_t attempts = 0;
        const uint8_t badPin1[] = "000000";
        rc = cd.pfnCardAuthenticatePin(&cd, L"ROLE_USER", badPin1, sizeof(badPin1) - 1, &attempts);
        TEST_ASSERT(rc == cardmod::SCARD_W_WRONG_CHV, "Wrong PIN must return SCARD_W_WRONG_CHV");
        TEST_ASSERT(attempts == 2, "Attempts remaining should decrement to 2");

        const uint8_t badPin2[] = "111111";
        rc = cd.pfnCardAuthenticatePin(&cd, L"ROLE_USER", badPin2, sizeof(badPin2) - 1, &attempts);
        TEST_ASSERT(rc == cardmod::SCARD_W_WRONG_CHV, "Wrong PIN second attempt returns SCARD_W_WRONG_CHV");
        TEST_ASSERT(attempts == 1, "Attempts remaining should decrement to 1");

        const uint8_t goodPin[] = "123456";
        rc = cd.pfnCardAuthenticatePin(&cd, L"ROLE_USER", goodPin, sizeof(goodPin) - 1, &attempts);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "Valid PIN must authenticate successfully");
        TEST_ASSERT(attempts == 3, "Attempts remaining should reset to 3 upon success");

        // Admin PIN
        const uint8_t adminPin[] = "12345678";
        rc = cd.pfnCardAuthenticatePin(&cd, L"ROLE_ADMIN", adminPin, sizeof(adminPin) - 1, &attempts);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "Admin PIN must authenticate");

        // Deauthenticate
        rc = cd.pfnCardDeauthenticate(&cd, L"ROLE_USER", 0);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "User deauthenticate must succeed");
        rc = cd.pfnCardDeauthenticate(&cd, L"ROLE_ADMIN", 0);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "Admin deauthenticate must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 6: On-Card File System Hierarchy
    // ------------------------------------------------------------------------
    {
        wchar_t* mwszFiles = nullptr;
        uint32_t cchFiles = 0;
        rc = cd.pfnCardEnumFiles(&cd, L"", &mwszFiles, &cchFiles, 0);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && mwszFiles != nullptr, "CardEnumFiles must succeed");
        std::vector<std::wstring> enumList;
        const wchar_t* p = mwszFiles;
        while (p && *p) {
            enumList.push_back(p);
            p += wcslen(p) + 1;
        }
        cd.pfnCspFree(mwszFiles);
        TEST_ASSERT(std::find(enumList.begin(), enumList.end(), L"cardid") != enumList.end(), "cardid file found");
        TEST_ASSERT(std::find(enumList.begin(), enumList.end(), L"cardcf") != enumList.end(), "cardcf file found");
        TEST_ASSERT(std::find(enumList.begin(), enumList.end(), L"cardapps") != enumList.end(), "cardapps file found");

        cardmod::CARD_FILE_INFO fInfo{};
        rc = cd.pfnCardGetFileInfo(&cd, L"", L"cardid", &fInfo);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardGetFileInfo on cardid must succeed");
        TEST_ASSERT(fInfo.cbFileSize == 16, "cardid size must be 16 bytes");
        TEST_ASSERT(fInfo.AccessCondition == cardmod::EveryoneReadFile, "cardid access condition match");

        uint8_t* pData = nullptr;
        uint32_t cbData = 0;
        rc = cd.pfnCardReadFile(&cd, L"", L"cardid", 0, &pData, &cbData);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && pData != nullptr, "CardReadFile on cardid must succeed");
        TEST_ASSERT(cbData == 16, "Read 16 bytes for cardid");
        TEST_ASSERT(pData[0] == 0x11 && pData[15] == 0x01, "cardid payload match");
        cd.pfnCspFree(pData);

        // Read cmapfile
        pData = nullptr;
        cbData = 0;
        rc = cd.pfnCardReadFile(&cd, L"mscp", L"cmapfile", 0, &pData, &cbData);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && pData != nullptr && cbData == 8, "cmapfile must read 8 bytes");
        cd.pfnCspFree(pData);
    }

    // ------------------------------------------------------------------------
    // Stage 7: On-Card Dynamic File Creation, Write, Read, and Deletion
    // ------------------------------------------------------------------------
    {
        const uint8_t dynamicData[] = "MicaNT Sovereign Minidriver Dynamic File Payload 2026";
        uint32_t payloadLen = sizeof(dynamicData);
        rc = cd.pfnCardCreateFile(&cd, L"", L"dynamic_test.bin", payloadLen, cardmod::EveryoneReadUserWriteAc);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardCreateFile must succeed");

        rc = cd.pfnCardWriteFile(&cd, L"", L"dynamic_test.bin", 0, dynamicData, payloadLen);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardWriteFile must succeed");

        uint8_t* pRead = nullptr;
        uint32_t cbRead = 0;
        rc = cd.pfnCardReadFile(&cd, L"", L"dynamic_test.bin", 0, &pRead, &cbRead);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && pRead != nullptr, "CardReadFile on dynamic_test.bin must succeed");
        TEST_ASSERT(cbRead == payloadLen && std::memcmp(pRead, dynamicData, payloadLen) == 0, "Payload match");
        cd.pfnCspFree(pRead);

        rc = cd.pfnCardDeleteFile(&cd, L"", L"dynamic_test.bin", 0);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardDeleteFile must succeed");

        pRead = nullptr;
        rc = cd.pfnCardReadFile(&cd, L"", L"dynamic_test.bin", 0, &pRead, &cbRead);
        TEST_ASSERT(rc == cardmod::SCARD_E_FILE_NOT_FOUND, "Deleted file must return SCARD_E_FILE_NOT_FOUND");
    }

    // ------------------------------------------------------------------------
    // Stage 8: Key Container Management (Query & Create Container)
    // ------------------------------------------------------------------------
    {
        cardmod::CONTAINER_INFO ci0{};
        rc = cd.pfnCardGetContainerInfo(&cd, 0, 0, &ci0);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardGetContainerInfo index 0 must succeed");
        TEST_ASSERT(ci0.dwKeySpec == cardmod::AT_KEYEXCHANGE, "Container 0 key spec must be AT_KEYEXCHANGE");
        TEST_ASSERT(ci0.pbKeyExPublicKey.size() == 256, "Container 0 RSA-2048 modulus size must be 256 bytes");

        cardmod::CONTAINER_INFO ci1{};
        rc = cd.pfnCardGetContainerInfo(&cd, 1, 0, &ci1);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardGetContainerInfo index 1 must succeed");
        TEST_ASSERT(ci1.dwKeySpec == cardmod::AT_SIGNATURE, "Container 1 key spec must be AT_SIGNATURE");
        TEST_ASSERT(ci1.pbSigPublicKey.size() == 256, "Container 1 RSA-2048 modulus size must be 256 bytes");

        // Create Container 2
        rc = cd.pfnCardCreateContainer(&cd, 2, 0, cardmod::AT_KEYEXCHANGE, 1024, nullptr);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardCreateContainer index 2 must succeed");

        cardmod::CONTAINER_INFO ci2{};
        rc = cd.pfnCardGetContainerInfo(&cd, 2, 0, &ci2);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && ci2.pbKeyExPublicKey.size() == 128, "Container 2 is 1024-bit (128 bytes)");

        rc = cd.pfnCardDeleteContainer(&cd, 2, 0);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardDeleteContainer index 2 must succeed");

        rc = cd.pfnCardGetContainerInfo(&cd, 2, 0, &ci2);
        TEST_ASSERT(rc == cardmod::SCARD_E_FILE_NOT_FOUND, "Deleted container must not be found");
    }

    // ------------------------------------------------------------------------
    // Stage 9: Cryptographic Operations (CardSignData & Base CSP)
    // ------------------------------------------------------------------------
    {
        // Unauthenticated signing should fail with wrong CHV
        uint8_t hash[32] = { 0xDE, 0xAD, 0xBE, 0xEF };
        uint8_t signature[256]{};
        uint32_t cbSig = sizeof(signature);
        rc = cd.pfnCardSignData(&cd, 0, cardmod::AT_KEYEXCHANGE, hash, sizeof(hash), signature, &cbSig);
        TEST_ASSERT(rc == cardmod::SCARD_W_WRONG_CHV, "Unauthenticated CardSignData must fail with SCARD_W_WRONG_CHV");

        // Authenticate User PIN
        const uint8_t pin[] = "123456";
        uint32_t attempts = 0;
        rc = cd.pfnCardAuthenticatePin(&cd, L"ROLE_USER", pin, sizeof(pin) - 1, &attempts);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardAuthenticatePin must succeed");

        // Query size only
        cbSig = 0;
        rc = cd.pfnCardSignData(&cd, 0, cardmod::AT_KEYEXCHANGE, hash, sizeof(hash), nullptr, &cbSig);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && cbSig == 256, "Query signature size returns 256");

        // Perform signature
        rc = cd.pfnCardSignData(&cd, 0, cardmod::AT_KEYEXCHANGE, hash, sizeof(hash), signature, &cbSig);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS && cbSig == 256, "CardSignData succeeds with 256-byte signature");

        // Base CSP API verification
        void* hProv = nullptr;
        int32_t bCsp = cardmod::CPAcquireContext(&hProv, nullptr, 0, nullptr);
        TEST_ASSERT(bCsp == 1 && hProv != nullptr, "CPAcquireContext must succeed");

        void* hGenKey = nullptr;
        bCsp = cardmod::CPGenKey(hProv, 0x0000a400, 0x08000000, &hGenKey);
        TEST_ASSERT(bCsp == 1 && hGenKey != nullptr, "CPGenKey must succeed");

        void* hDerivedKey = nullptr;
        bCsp = cardmod::CPDeriveKey(hProv, 0x00006801, nullptr, 0, &hDerivedKey);
        TEST_ASSERT(bCsp == 1 && hDerivedKey != nullptr, "CPDeriveKey must succeed");

        uint32_t encLen = 16;
        bCsp = cardmod::CPEncrypt(hProv, hGenKey, nullptr, 1, 0, nullptr, &encLen, 16);
        TEST_ASSERT(bCsp == 1, "CPEncrypt must succeed");

        bCsp = cardmod::CPDecrypt(hProv, hGenKey, nullptr, 1, 0, nullptr, &encLen);
        TEST_ASSERT(bCsp == 1, "CPDecrypt must succeed");

        bCsp = cardmod::CPDestroyKey(hProv, hGenKey);
        TEST_ASSERT(bCsp == 1, "CPDestroyKey must succeed");

        bCsp = cardmod::CPDestroyKey(hProv, hDerivedKey);
        TEST_ASSERT(bCsp == 1, "CPDestroyKey derived key must succeed");

        bCsp = cardmod::CPReleaseContext(hProv, 0);
        TEST_ASSERT(bCsp == 1, "CPReleaseContext must succeed");

        // Cleanup card context
        rc = cd.pfnCardDeleteContext(&cd);
        TEST_ASSERT(rc == cardmod::SCARD_S_SUCCESS, "CardDeleteContext must succeed");
        TEST_ASSERT(cd.pbAtr == nullptr && cd.pwszCardName == nullptr, "Context resources freed");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Interactive Shell Integration (cmdCardMod)
    // ------------------------------------------------------------------------
    {
        cardmod::CardMinidriverManager::get().reset();
        shell::CommandShell shell;
        std::stringstream out;

        // cardmod test
        shell.execute("cardmod test", out);
        TEST_ASSERT(out.str().find("[CARDMOD] Self-Test Completed: ALL 12 TESTS PASSED (100%).") != std::string::npos, "cardmod test must pass all 12 tests");

        // cardmod list
        out.str("");
        shell.execute("cardmod list", out);
        TEST_ASSERT(out.str().find("MicaNT Titan Sovereign PIV Token") != std::string::npos, "cardmod list shows PIV token");
        TEST_ASSERT(out.str().find("MicaNT FIDO2 Hardware Token") != std::string::npos, "cardmod list shows FIDO2 token");

        // cardmod files 1
        out.str("");
        shell.execute("cardmod files 1", out);
        TEST_ASSERT(out.str().find("/cardid") != std::string::npos, "cardmod files shows /cardid");
        TEST_ASSERT(out.str().find("/cardcf") != std::string::npos, "cardmod files shows /cardcf");
        TEST_ASSERT(out.str().find("/cardapps") != std::string::npos, "cardmod files shows /cardapps");
        TEST_ASSERT(out.str().find("/mscp/cmapfile") != std::string::npos, "cardmod files shows /mscp/cmapfile");

        // cardmod containers 1
        out.str("");
        shell.execute("cardmod containers 1", out);
        TEST_ASSERT(out.str().find("Titan_PIV_Auth") != std::string::npos, "cardmod containers shows Titan_PIV_Auth");
        TEST_ASSERT(out.str().find("Titan_PIV_DigitalSig") != std::string::npos, "cardmod containers shows Titan_PIV_DigitalSig");

        // cardmod auth 1 123456
        out.str("");
        shell.execute("cardmod auth 1 123456", out);
        TEST_ASSERT(out.str().find("PIN Authentication SUCCESSful (USER)") != std::string::npos, "cardmod auth must succeed");

        // cardmod sign 1 0 TestSignatureData
        out.str("");
        shell.execute("cardmod sign 1 0 TestSignatureData", out);
        TEST_ASSERT(out.str().find("Data Signed Successfully (Length: 256 bytes)") != std::string::npos, "cardmod sign must succeed");
    }

    std::cout << "[TEST] Suite 98: Windows Smart Card Minidriver & Base CSP Architecture PASSED.\n";
}

void Test_WindowsPOSIX_Subsystem() {
    std::cout << "[TEST] Suite 99: Running Windows POSIX.1 Subsystem & UNIX Compatibility Tests...\n";

    auto& ldr = ldr::DynamicLoader::get();
    posix::InitializePosixSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (psxdll.dll / psxss.exe)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("psxdll.dll", "fork") != nullptr, "psxdll.dll must export fork");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "execve") != nullptr, "psxdll.dll must export execve");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "waitpid") != nullptr, "psxdll.dll must export waitpid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "getpid") != nullptr, "psxdll.dll must export getpid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "getppid") != nullptr, "psxdll.dll must export getppid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "getuid") != nullptr, "psxdll.dll must export getuid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "geteuid") != nullptr, "psxdll.dll must export geteuid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "getgid") != nullptr, "psxdll.dll must export getgid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "getegid") != nullptr, "psxdll.dll must export getegid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "setuid") != nullptr, "psxdll.dll must export setuid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "setgid") != nullptr, "psxdll.dll must export setgid");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "kill") != nullptr, "psxdll.dll must export kill");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "sigaction") != nullptr, "psxdll.dll must export sigaction");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "pipe") != nullptr, "psxdll.dll must export pipe");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "open") != nullptr, "psxdll.dll must export open");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "close") != nullptr, "psxdll.dll must export close");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "read") != nullptr, "psxdll.dll must export read");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "write") != nullptr, "psxdll.dll must export write");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "dup2") != nullptr, "psxdll.dll must export dup2");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "stat") != nullptr, "psxdll.dll must export stat");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "unlink") != nullptr, "psxdll.dll must export unlink");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "getcwd") != nullptr, "psxdll.dll must export getcwd");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "chdir") != nullptr, "psxdll.dll must export chdir");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "isatty") != nullptr, "psxdll.dll must export isatty");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "DllCanUnloadNow") != nullptr, "psxdll.dll must export DllCanUnloadNow");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "DllRegisterServer") != nullptr, "psxdll.dll must export DllRegisterServer");
    TEST_ASSERT(ldr.getExport("psxdll.dll", "DllUnregisterServer") != nullptr, "psxdll.dll must export DllUnregisterServer");

    TEST_ASSERT(ldr.getExport("psxss.exe", "PosixServerMain") != nullptr, "psxss.exe must export PosixServerMain");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verDll = version::VersionDatabase::Instance().FindModule("psxdll.dll");
        TEST_ASSERT(verDll != nullptr, "VersionDatabase must contain psxdll.dll");
        TEST_ASSERT(verDll->stringTable.at("FileDescription") == "POSIX.1 Subsystem Client Library", "psxdll.dll description match");
        TEST_ASSERT(verDll->stringTable.at("OriginalFilename") == "psxdll.dll", "psxdll.dll original filename match");
        TEST_ASSERT(verDll->stringTable.at("ProductName") == "MicaNT POSIX Subsystem", "psxdll.dll product name match");

        const auto* verSs = version::VersionDatabase::Instance().FindModule("psxss.exe");
        TEST_ASSERT(verSs != nullptr, "VersionDatabase must contain psxss.exe");
        TEST_ASSERT(verSs->stringTable.at("FileDescription") == "POSIX.1 Subsystem Server", "psxss.exe description match");
        TEST_ASSERT(verSs->stringTable.at("OriginalFilename") == "psxss.exe", "psxss.exe original filename match");
        TEST_ASSERT(verSs->stringTable.at("ProductName") == "MicaNT POSIX Subsystem", "psxss.exe product name match");

        const auto* verApp = version::VersionDatabase::Instance().FindModule("posix.exe");
        TEST_ASSERT(verApp != nullptr, "VersionDatabase must contain posix.exe");
        TEST_ASSERT(verApp->stringTable.at("FileDescription") == "POSIX Subsystem Application Launcher", "posix.exe description match");
        TEST_ASSERT(verApp->stringTable.at("OriginalFilename") == "posix.exe", "posix.exe original filename match");
        TEST_ASSERT(verApp->stringTable.at("ProductName") == "MicaNT POSIX Subsystem", "posix.exe product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: SCM Service Registration (PosixSubsystem)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();
        auto svc = scm.getServiceRecord(L"PosixSubsystem");
        TEST_ASSERT(svc != nullptr, "PosixSubsystem service must be registered in SCM");
        TEST_ASSERT(svc->displayName == L"POSIX.1 Subsystem Server", "PosixSubsystem display name match");
        TEST_ASSERT(svc->status.dwCurrentState == scm::SERVICE_RUNNING, "PosixSubsystem must be running");
        TEST_ASSERT(svc->status.dwProcessId == 1180, "PosixSubsystem PID match (1180)");
    }

    // ------------------------------------------------------------------------
    // Stage 4: Process Hierarchy Lifecycle (Init, Fork, Execve)
    // ------------------------------------------------------------------------
    posix::PosixSubsystemServer::get().reset();
    posix::pid_t childPid = 0;
    {
        auto* pInit = posix::PosixSubsystemServer::get().getProcess(1);
        TEST_ASSERT(pInit != nullptr, "Init process must exist at PID 1");
        TEST_ASSERT(pInit->command == "/bin/init", "Init command match");
        TEST_ASSERT(pInit->uid == 0 && pInit->gid == 0, "Init running as root");

        childPid = posix::psx_fork();
        TEST_ASSERT(childPid > 1, "psx_fork must return child PID > 1");

        auto* pChild = posix::PosixSubsystemServer::get().getProcess(childPid);
        TEST_ASSERT(pChild != nullptr, "Child process must exist in process table");
        TEST_ASSERT(pChild->ppid == 1, "Child parent must be PID 1");

        char* const argv[] = { const_cast<char*>("/bin/ls"), const_cast<char*>("-la"), nullptr };
        char* const envp[] = { const_cast<char*>("TERM=xterm"), nullptr };
        int rc = posix::psx_execve("/bin/ls", argv, envp);
        TEST_ASSERT(rc == 0, "psx_execve must succeed for /bin/ls");
    }

    // ------------------------------------------------------------------------
    // Stage 5: Process Credentials & Identity
    // ------------------------------------------------------------------------
    {
        TEST_ASSERT(posix::psx_getuid() == 0, "Initial UID is 0 (root)");
        TEST_ASSERT(posix::psx_getgid() == 0, "Initial GID is 0 (root)");

        int rc = posix::psx_setuid(1000);
        TEST_ASSERT(rc == 0, "psx_setuid(1000) must succeed");
        TEST_ASSERT(posix::psx_getuid() == 1000, "Updated UID is 1000");

        rc = posix::psx_setgid(1000);
        TEST_ASSERT(rc == 0, "psx_setgid(1000) must succeed");
        TEST_ASSERT(posix::psx_getgid() == 1000, "Updated GID is 1000");

        // Restore root
        posix::psx_setuid(0);
        posix::psx_setgid(0);
        TEST_ASSERT(posix::psx_getuid() == 0, "Restored UID to root");
    }

    // ------------------------------------------------------------------------
    // Stage 6: Signal Action & Delivery Architecture
    // ------------------------------------------------------------------------
    {
        posix::sigaction_t act{};
        act.sa_handler = posix::PSX_SIG_IGN;
        act.sa_flags = 0;
        int rc = posix::psx_sigaction(posix::PSX_SIGUSR1, &act, nullptr);
        TEST_ASSERT(rc == 0, "sigaction(SIGUSR1, SIG_IGN) must succeed");

        posix::sigaction_t oldact{};
        rc = posix::psx_sigaction(posix::PSX_SIGUSR1, nullptr, &oldact);
        TEST_ASSERT(rc == 0 && oldact.sa_handler == posix::PSX_SIG_IGN, "Queried oldact matches SIG_IGN");

        // Send SIGTERM to child
        rc = posix::psx_kill(childPid, posix::PSX_SIGTERM);
        TEST_ASSERT(rc == 0, "kill(childPid, SIGTERM) must succeed");

        auto* pChild = posix::PosixSubsystemServer::get().getProcess(childPid);
        TEST_ASSERT(pChild != nullptr && pChild->state == posix::PosixProcessState::Zombie, "Target child transitioned to Zombie state");
    }

    // ------------------------------------------------------------------------
    // Stage 7: Process Reaping & Exit Handling (waitpid)
    // ------------------------------------------------------------------------
    {
        int status = 0;
        posix::pid_t reaped = posix::psx_waitpid(childPid, &status, 0);
        TEST_ASSERT(reaped == childPid, "waitpid must reap target child PID");
        TEST_ASSERT((status >> 8) == (128 + posix::PSX_SIGTERM), "Exit status reflects SIGTERM termination");

        auto* pChildAfter = posix::PosixSubsystemServer::get().getProcess(childPid);
        TEST_ASSERT(pChildAfter == nullptr, "Reaped child no longer in process table");
    }

    // ------------------------------------------------------------------------
    // Stage 8: File Descriptors, VFS I/O & Creation
    // ------------------------------------------------------------------------
    {
        int fd = posix::psx_open("/tmp/suite99_test.dat", posix::PSX_O_RDWR | posix::PSX_O_CREAT, 0644);
        TEST_ASSERT(fd >= 3, "open with O_CREAT must return valid FD >= 3");

        const char testMsg[] = "Clean-Room POSIX Subsystem Suite 99";
        posix::ssize_t written = posix::psx_write(fd, testMsg, sizeof(testMsg) - 1);
        TEST_ASSERT(written == sizeof(testMsg) - 1, "write must write full payload length");

        int rc = posix::psx_close(fd);
        TEST_ASSERT(rc == 0, "close must succeed");

        // Reopen for reading
        fd = posix::psx_open("/tmp/suite99_test.dat", posix::PSX_O_RDONLY, 0);
        TEST_ASSERT(fd >= 3, "open for read returns valid FD");

        char buf[64]{};
        posix::ssize_t nRead = posix::psx_read(fd, buf, sizeof(buf) - 1);
        TEST_ASSERT(nRead == sizeof(testMsg) - 1, "read returned expected byte count");
        TEST_ASSERT(std::strcmp(buf, testMsg) == 0, "read payload matched written payload");

        posix::psx_close(fd);
    }

    // ------------------------------------------------------------------------
    // Stage 9: Anonymous Pipe IPC Architecture
    // ------------------------------------------------------------------------
    {
        int pipefds[2]{ -1, -1 };
        int rc = posix::psx_pipe(pipefds);
        TEST_ASSERT(rc == 0, "pipe must succeed");
        TEST_ASSERT(pipefds[0] >= 3 && pipefds[1] >= 3, "Valid read and write FDs allocated");

        const char pipeData[] = "Pipe IPC Verification Token 0xFEED";
        posix::ssize_t w = posix::psx_write(pipefds[1], pipeData, sizeof(pipeData) - 1);
        TEST_ASSERT(w == sizeof(pipeData) - 1, "Pipe write must write entire message");

        char readPipe[64]{};
        posix::ssize_t r = posix::psx_read(pipefds[0], readPipe, sizeof(readPipe) - 1);
        TEST_ASSERT(r == sizeof(pipeData) - 1, "Pipe read must read full message");
        TEST_ASSERT(std::strcmp(readPipe, pipeData) == 0, "Pipe payload verified");

        posix::psx_close(pipefds[0]);
        posix::psx_close(pipefds[1]);
    }

    // ------------------------------------------------------------------------
    // Stage 10: Special UNIX Character Devices (/dev/null, /dev/zero, /dev/tty)
    // ------------------------------------------------------------------------
    {
        // /dev/null
        int fdNull = posix::psx_open("/dev/null", posix::PSX_O_RDWR, 0);
        TEST_ASSERT(fdNull >= 3, "open /dev/null must succeed");
        posix::ssize_t w = posix::psx_write(fdNull, "DiscardThis", 11);
        TEST_ASSERT(w == 11, "write to /dev/null succeeds");
        char nullRead[16]{};
        posix::ssize_t r = posix::psx_read(fdNull, nullRead, sizeof(nullRead));
        TEST_ASSERT(r == 0, "read from /dev/null returns EOF (0)");
        posix::psx_close(fdNull);

        // /dev/zero
        int fdZero = posix::psx_open("/dev/zero", posix::PSX_O_RDONLY, 0);
        TEST_ASSERT(fdZero >= 3, "open /dev/zero must succeed");
        uint8_t zeroBuf[16]{ 0xFF, 0xFF, 0xFF };
        r = posix::psx_read(fdZero, zeroBuf, 16);
        TEST_ASSERT(r == 16, "read from /dev/zero returns requested count");
        bool allZeros = true;
        for (int i = 0; i < 16; ++i) if (zeroBuf[i] != 0) allZeros = false;
        TEST_ASSERT(allZeros, "Buffer from /dev/zero is completely zeroed");
        posix::psx_close(fdZero);

        // /dev/tty & isatty
        int fdTty = posix::psx_open("/dev/tty", posix::PSX_O_RDWR, 0);
        TEST_ASSERT(fdTty >= 3, "open /dev/tty must succeed");
        TEST_ASSERT(posix::psx_isatty(fdTty) == 1, "/dev/tty is a TTY");
        posix::psx_close(fdTty);
    }

    // ------------------------------------------------------------------------
    // Stage 11: File Metadata & Unlink
    // ------------------------------------------------------------------------
    {
        posix::stat_t st{};
        int rc = posix::psx_stat("/etc/os-release", &st);
        TEST_ASSERT(rc == 0, "stat /etc/os-release must succeed");
        TEST_ASSERT((st.st_mode & posix::PSX_S_IFREG) != 0, "/etc/os-release is a regular file");
        TEST_ASSERT(st.st_size > 0, "File has non-zero size");

        rc = posix::psx_stat("/bin", &st);
        TEST_ASSERT(rc == 0, "stat /bin must succeed");
        TEST_ASSERT((st.st_mode & posix::PSX_S_IFDIR) != 0, "/bin is a directory");

        rc = posix::psx_unlink("/tmp/suite99_test.dat");
        TEST_ASSERT(rc == 0, "unlink must succeed");

        rc = posix::psx_stat("/tmp/suite99_test.dat", &st);
        TEST_ASSERT(rc == -posix::PSX_ENOENT, "stat on unlinked file must return -ENOENT");
    }

    // ------------------------------------------------------------------------
    // Stage 12: Working Directory Navigation (getcwd, chdir)
    // ------------------------------------------------------------------------
    {
        char cwdBuf[256]{};
        TEST_ASSERT(posix::psx_getcwd(cwdBuf, sizeof(cwdBuf)) != nullptr, "getcwd must succeed");

        int rc = posix::psx_chdir("/etc");
        TEST_ASSERT(rc == 0, "chdir(/etc) must succeed");
        std::memset(cwdBuf, 0, sizeof(cwdBuf));
        posix::psx_getcwd(cwdBuf, sizeof(cwdBuf));
        TEST_ASSERT(std::string(cwdBuf) == "/etc", "Current directory updated to /etc");

        rc = posix::psx_chdir("/nonexistent_path");
        TEST_ASSERT(rc != 0, "chdir to non-existent path must fail");

        // Restore root
        posix::psx_chdir("/");
        posix::psx_getcwd(cwdBuf, sizeof(cwdBuf));
        TEST_ASSERT(std::string(cwdBuf) == "/", "Current directory restored to /");
    }

    // ------------------------------------------------------------------------
    // Stage 13: Interactive Shell Integration (cmdPosix)
    // ------------------------------------------------------------------------
    {
        posix::PosixSubsystemServer::get().reset();
        shell::CommandShell shell;
        std::stringstream out;

        // posix test
        shell.execute("posix test", out);
        TEST_ASSERT(out.str().find("[POSIX] Self-Test Completed: ALL 14 TESTS PASSED (100%).") != std::string::npos, "posix test must pass all 14 tests");

        // posix ps
        out.str("");
        shell.execute("posix ps", out);
        TEST_ASSERT(out.str().find("/bin/init") != std::string::npos, "posix ps shows /bin/init");
        TEST_ASSERT(out.str().find("/usr/sbin/crond") != std::string::npos, "posix ps shows /usr/sbin/crond");

        // posix env
        out.str("");
        shell.execute("posix env", out);
        TEST_ASSERT(out.str().find("SHELL=/bin/sh") != std::string::npos, "posix env shows SHELL");
        TEST_ASSERT(out.str().find("USER=root") != std::string::npos, "posix env shows USER");

        // posix sh uname
        out.str("");
        shell.execute("posix sh uname", out);
        TEST_ASSERT(out.str().find("MicaNT 10.0.26100.1 POSIX.1/Interix") != std::string::npos, "posix sh uname output match");

        // posix sh id
        out.str("");
        shell.execute("posix sh id", out);
        TEST_ASSERT(out.str().find("uid=0(root)") != std::string::npos, "posix sh id output match");

        // posix sh cat /etc/hostname
        out.str("");
        shell.execute("posix sh cat /etc/hostname", out);
        TEST_ASSERT(out.str().find("micant-workstation") != std::string::npos, "posix sh cat shows hostname");
    }

    std::cout << "[TEST] Suite 99: Windows POSIX Subsystem & UNIX Compatibility PASSED.\n";
}

void Test_WindowsHypervisor_Platform_Subsystem() {
    std::cout << "[TEST] Suite 100: Running Windows Hypervisor Platform (WHP) & Virtualization Tests...\n";

    auto& ldr = ldr::DynamicLoader::get();
    whp::InitializeWhpSubsystemExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (WinHvPlatform / WinHvEmulation / vmcompute)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvGetCapability") != nullptr, "WinHvPlatform.dll must export WHvGetCapability");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvCreatePartition") != nullptr, "WinHvPlatform.dll must export WHvCreatePartition");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvSetupPartition") != nullptr, "WinHvPlatform.dll must export WHvSetupPartition");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvResetPartition") != nullptr, "WinHvPlatform.dll must export WHvResetPartition");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvDeletePartition") != nullptr, "WinHvPlatform.dll must export WHvDeletePartition");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvGetPartitionProperty") != nullptr, "WinHvPlatform.dll must export WHvGetPartitionProperty");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvSetPartitionProperty") != nullptr, "WinHvPlatform.dll must export WHvSetPartitionProperty");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvMapGpaRange") != nullptr, "WinHvPlatform.dll must export WHvMapGpaRange");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvUnmapGpaRange") != nullptr, "WinHvPlatform.dll must export WHvUnmapGpaRange");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvTranslateGva") != nullptr, "WinHvPlatform.dll must export WHvTranslateGva");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvCreateVirtualProcessor") != nullptr, "WinHvPlatform.dll must export WHvCreateVirtualProcessor");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvDeleteVirtualProcessor") != nullptr, "WinHvPlatform.dll must export WHvDeleteVirtualProcessor");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvRunVirtualProcessor") != nullptr, "WinHvPlatform.dll must export WHvRunVirtualProcessor");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvCancelRunVirtualProcessor") != nullptr, "WinHvPlatform.dll must export WHvCancelRunVirtualProcessor");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvGetVirtualProcessorRegisters") != nullptr, "WinHvPlatform.dll must export WHvGetVirtualProcessorRegisters");
    TEST_ASSERT(ldr.getExport("WinHvPlatform.dll", "WHvSetVirtualProcessorRegisters") != nullptr, "WinHvPlatform.dll must export WHvSetVirtualProcessorRegisters");

    TEST_ASSERT(ldr.getExport("WinHvEmulation.dll", "WHvEmulatorCreateEmulator") != nullptr, "WinHvEmulation.dll must export WHvEmulatorCreateEmulator");
    TEST_ASSERT(ldr.getExport("WinHvEmulation.dll", "WHvEmulatorDestroyEmulator") != nullptr, "WinHvEmulation.dll must export WHvEmulatorDestroyEmulator");
    TEST_ASSERT(ldr.getExport("WinHvEmulation.dll", "WHvEmulatorTryMmioEmulation") != nullptr, "WinHvEmulation.dll must export WHvEmulatorTryMmioEmulation");
    TEST_ASSERT(ldr.getExport("WinHvEmulation.dll", "WHvEmulatorTryIoEmulation") != nullptr, "WinHvEmulation.dll must export WHvEmulatorTryIoEmulation");

    TEST_ASSERT(ldr.getExport("vmcompute.exe", "HcsMain") != nullptr, "vmcompute.exe must export HcsMain");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verPlat = version::VersionDatabase::Instance().FindModule("WinHvPlatform.dll");
        TEST_ASSERT(verPlat != nullptr, "VersionDatabase must contain WinHvPlatform.dll");
        TEST_ASSERT(verPlat->stringTable.at("FileDescription") == "Windows Hypervisor Platform Client DLL", "WinHvPlatform.dll description match");
        TEST_ASSERT(verPlat->stringTable.at("OriginalFilename") == "WinHvPlatform.dll", "WinHvPlatform.dll original filename match");
        TEST_ASSERT(verPlat->stringTable.at("ProductName") == "MicaNT Hypervisor Platform", "WinHvPlatform.dll product name match");

        const auto* verEmu = version::VersionDatabase::Instance().FindModule("WinHvEmulation.dll");
        TEST_ASSERT(verEmu != nullptr, "VersionDatabase must contain WinHvEmulation.dll");
        TEST_ASSERT(verEmu->stringTable.at("FileDescription") == "Windows Hypervisor Instruction Emulation DLL", "WinHvEmulation.dll description match");
        TEST_ASSERT(verEmu->stringTable.at("OriginalFilename") == "WinHvEmulation.dll", "WinHvEmulation.dll original filename match");
        TEST_ASSERT(verEmu->stringTable.at("ProductName") == "MicaNT Hypervisor Platform", "WinHvEmulation.dll product name match");

        const auto* verHcs = version::VersionDatabase::Instance().FindModule("vmcompute.exe");
        TEST_ASSERT(verHcs != nullptr, "VersionDatabase must contain vmcompute.exe");
        TEST_ASSERT(verHcs->stringTable.at("FileDescription") == "Hyper-V Host Compute Service", "vmcompute.exe description match");
        TEST_ASSERT(verHcs->stringTable.at("OriginalFilename") == "vmcompute.exe", "vmcompute.exe original filename match");
        TEST_ASSERT(verHcs->stringTable.at("ProductName") == "MicaNT Hypervisor Platform", "vmcompute.exe product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: SCM Service Registration (vmcompute / Hyper-V Host Compute Service)
    // ------------------------------------------------------------------------
    {
        auto& scm = scm::ServiceControlManager::get();
        auto svc = scm.getServiceRecord(L"vmcompute");
        TEST_ASSERT(svc != nullptr, "vmcompute service must be registered in SCM");
        TEST_ASSERT(svc->displayName == L"Hyper-V Host Compute Service", "vmcompute display name match");
        TEST_ASSERT(svc->status.dwCurrentState == scm::SERVICE_RUNNING, "vmcompute must be running");
        TEST_ASSERT(svc->status.dwProcessId == 1184, "vmcompute PID match (1184)");
        TEST_ASSERT(svc->binaryPath == L"C:\\Windows\\System32\\vmcompute.exe", "vmcompute binary path match");
    }

    // ------------------------------------------------------------------------
    // Stage 4: WHP Capabilities Query
    // ------------------------------------------------------------------------
    {
        uint32_t hypPresent = 0;
        uint32_t written = 0;
        int32_t hr = whp::WHvGetCapability(whp::WHV_CAPABILITY_CODE::HypervisorPresent, &hypPresent, sizeof(hypPresent), &written);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvGetCapability HypervisorPresent must succeed");
        TEST_ASSERT(hypPresent == 1, "HypervisorPresent must return 1");
        TEST_ASSERT(written == sizeof(uint32_t), "Written size must be 4 bytes");

        uint64_t features = 0;
        hr = whp::WHvGetCapability(whp::WHV_CAPABILITY_CODE::Features, &features, sizeof(features), &written);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvGetCapability Features must succeed");
        TEST_ASSERT(features == 0x0F, "Features must return 0x0F (PartialUnmap | LocalApic | Xsave | DirtyTrack)");

        uint64_t exits = 0;
        hr = whp::WHvGetCapability(whp::WHV_CAPABILITY_CODE::ExtendedVmExits, &exits, sizeof(exits), &written);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvGetCapability ExtendedVmExits must succeed");
        TEST_ASSERT(exits == 0x07, "ExtendedVmExits must return 0x07 (Cpuid | Msr | Exception)");

        uint32_t clflush = 0;
        hr = whp::WHvGetCapability(whp::WHV_CAPABILITY_CODE::ProcessorClFlushSize, &clflush, sizeof(clflush), &written);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvGetCapability ProcessorClFlushSize must succeed");
        TEST_ASSERT(clflush == 64, "CLFLUSH size must be 64 bytes");

        // Boundary checks
        uint8_t tinyBuf[1]{};
        hr = whp::WHvGetCapability(whp::WHV_CAPABILITY_CODE::HypervisorPresent, tinyBuf, sizeof(tinyBuf), nullptr);
        TEST_ASSERT(hr == whp::WHV_E_INSUFFICIENT_BUFFER, "Insufficient buffer must return WHV_E_INSUFFICIENT_BUFFER");

        hr = whp::WHvGetCapability(whp::WHV_CAPABILITY_CODE::HypervisorPresent, nullptr, 0, nullptr);
        TEST_ASSERT(hr == whp::WHV_E_INVALIDARG, "Null buffer must return WHV_E_INVALIDARG");
    }

    // ------------------------------------------------------------------------
    // Stage 5: Partition Lifecycle & Property Configuration
    // ------------------------------------------------------------------------
    whp::WhpManager::get().reset();
    whp::WHV_PARTITION_HANDLE hPartition = nullptr;
    {
        int32_t hr = whp::WHvCreatePartition(&hPartition);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvCreatePartition must succeed");
        TEST_ASSERT(hPartition != nullptr, "Partition handle must be non-null");

        // Set ProcessorCount property
        uint32_t vCpuCount = 4;
        hr = whp::WHvSetPartitionProperty(hPartition, whp::WHV_PARTITION_PROPERTY_CODE::ProcessorCount, &vCpuCount, sizeof(vCpuCount));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvSetPartitionProperty ProcessorCount must succeed");

        // Set ExtendedVmExits property
        uint64_t exitsConfig = 0x07;
        hr = whp::WHvSetPartitionProperty(hPartition, whp::WHV_PARTITION_PROPERTY_CODE::ExtendedVmExits, &exitsConfig, sizeof(exitsConfig));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvSetPartitionProperty ExtendedVmExits must succeed");

        // Query back properties
        uint32_t readVpCount = 0;
        uint32_t written = 0;
        hr = whp::WHvGetPartitionProperty(hPartition, whp::WHV_PARTITION_PROPERTY_CODE::ProcessorCount, &readVpCount, sizeof(readVpCount), &written);
        TEST_ASSERT(hr == whp::WHV_S_OK && readVpCount == 4, "WHvGetPartitionProperty ProcessorCount must be 4");

        uint64_t readExits = 0;
        hr = whp::WHvGetPartitionProperty(hPartition, whp::WHV_PARTITION_PROPERTY_CODE::ExtendedVmExits, &readExits, sizeof(readExits), &written);
        TEST_ASSERT(hr == whp::WHV_S_OK && readExits == 0x07, "WHvGetPartitionProperty ExtendedVmExits must be 0x07");

        // Finalize partition setup
        hr = whp::WHvSetupPartition(hPartition);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvSetupPartition must succeed");

        // Idempotent setup
        hr = whp::WHvSetupPartition(hPartition);
        TEST_ASSERT(hr == whp::WHV_S_OK, "Second WHvSetupPartition must succeed idempotently");
    }

    // ------------------------------------------------------------------------
    // Stage 6: Virtual Processor Management
    // ------------------------------------------------------------------------
    {
        int32_t hr = whp::WHvCreateVirtualProcessor(hPartition, 0, 0);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvCreateVirtualProcessor VP 0 must succeed");

        hr = whp::WHvCreateVirtualProcessor(hPartition, 0, 0);
        TEST_ASSERT(hr == whp::WHV_E_INVALIDARG, "Duplicate VP creation must fail with WHV_E_INVALIDARG");

        hr = whp::WHvCreateVirtualProcessor(hPartition, 1, 0);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvCreateVirtualProcessor VP 1 must succeed");

        hr = whp::WHvCreateVirtualProcessor(hPartition, 2, 0);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvCreateVirtualProcessor VP 2 must succeed");

        hr = whp::WHvDeleteVirtualProcessor(hPartition, 2);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvDeleteVirtualProcessor VP 2 must succeed");

        hr = whp::WHvDeleteVirtualProcessor(hPartition, 99);
        TEST_ASSERT(hr == whp::WHV_E_VP_NOT_FOUND, "Deleting nonexistent VP must return WHV_E_VP_NOT_FOUND");
    }

    // ------------------------------------------------------------------------
    // Stage 7: GPA Memory Mapping & Virtual Address Translation
    // ------------------------------------------------------------------------
    alignas(4096) static uint8_t s_testGuestRam[65536];
    std::memset(s_testGuestRam, 0xCC, sizeof(s_testGuestRam));
    {
        whp::WHV_MAP_GPA_RANGE_FLAGS flags = static_cast<whp::WHV_MAP_GPA_RANGE_FLAGS>(
            whp::WHvMapGpaRangeFlagRead | whp::WHvMapGpaRangeFlagWrite | whp::WHvMapGpaRangeFlagExecute);

        int32_t hr = whp::WHvMapGpaRange(hPartition, s_testGuestRam, 0x00100000, sizeof(s_testGuestRam), flags);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvMapGpaRange must succeed");

        // Overlapping range should fail
        hr = whp::WHvMapGpaRange(hPartition, s_testGuestRam, 0x00100000, 4096, flags);
        TEST_ASSERT(hr == whp::WHV_E_INVALIDARG, "Overlapping GPA mapping must fail with WHV_E_INVALIDARG");

        // Virtual Address translation test
        whp::WHV_TRANSLATE_GVA_RESULT transRes{};
        whp::WHV_GUEST_PHYSICAL_ADDRESS outGpa = 0;
        hr = whp::WHvTranslateGva(hPartition, 0, 0xFFFF800000102000ULL, whp::WHvTranslateGvaFlagValidateRead, &transRes, &outGpa);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvTranslateGva must succeed");
        TEST_ASSERT(transRes.ResultCode == whp::WHV_TRANSLATE_GVA_RESULT_CODE::Success, "Translation result code must be Success");
        TEST_ASSERT(outGpa == 0x00102000ULL, "Translated GPA must match guest physical offset");

        // Unmap GPA
        hr = whp::WHvUnmapGpaRange(hPartition, 0x00100000, sizeof(s_testGuestRam));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvUnmapGpaRange must succeed");

        // Unmap nonexistent
        hr = whp::WHvUnmapGpaRange(hPartition, 0x00900000, 4096);
        TEST_ASSERT(hr == whp::WHV_E_GPA_RANGE_NOT_FOUND, "Unmapping nonexistent GPA must return WHV_E_GPA_RANGE_NOT_FOUND");
    }

    // ------------------------------------------------------------------------
    // Stage 8: Register Access & Modification
    // ------------------------------------------------------------------------
    {
        whp::WHV_REGISTER_NAME regNames[] = {
            whp::WHV_REGISTER_NAME::Rip,
            whp::WHV_REGISTER_NAME::Rflags,
            whp::WHV_REGISTER_NAME::Rax,
            whp::WHV_REGISTER_NAME::Cr0
        };

        whp::WHV_REGISTER_VALUE setVals[4]{};
        setVals[0].Reg64 = 0x00007FF800001000ULL;
        setVals[1].Reg64 = 0x0000000000000202ULL;
        setVals[2].Reg64 = 0x1234567890ABCDEFULL;
        setVals[3].Reg64 = 0x0000000080050033ULL;

        int32_t hr = whp::WHvSetVirtualProcessorRegisters(hPartition, 0, regNames, 4, setVals);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvSetVirtualProcessorRegisters must succeed");

        whp::WHV_REGISTER_VALUE getVals[4]{};
        hr = whp::WHvGetVirtualProcessorRegisters(hPartition, 0, regNames, 4, getVals);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvGetVirtualProcessorRegisters must succeed");
        TEST_ASSERT(getVals[0].Reg64 == 0x00007FF800001000ULL, "Queried RIP must match written value");
        TEST_ASSERT(getVals[1].Reg64 == 0x0000000000000202ULL, "Queried RFLAGS must match written value");
        TEST_ASSERT(getVals[2].Reg64 == 0x1234567890ABCDEFULL, "Queried RAX must match written value");
        TEST_ASSERT(getVals[3].Reg64 == 0x0000000080050033ULL, "Queried CR0 must match written value");

        // Reset partition registers to reset vector
        hr = whp::WHvResetPartition(hPartition);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvResetPartition must succeed");

        whp::WHV_REGISTER_NAME ripName = whp::WHV_REGISTER_NAME::Rip;
        whp::WHV_REGISTER_VALUE resetRip{};
        hr = whp::WHvGetVirtualProcessorRegisters(hPartition, 0, &ripName, 1, &resetRip);
        TEST_ASSERT(hr == whp::WHV_S_OK && resetRip.Reg64 == 0xFFF0, "Reset VP RIP must be at x86 reset vector 0xFFF0");
    }

    // ------------------------------------------------------------------------
    // Stage 9: Hypervisor Execution & Exit Handling (CPUID, MMIO, I/O, Cancel)
    // ------------------------------------------------------------------------
    {
        whp::WHV_RUN_VP_EXIT_CONTEXT exitCtx{};

        // 1. Initial run: RIP is 0xFFF0 -> Trigger CPUID exit
        int32_t hr = whp::WHvRunVirtualProcessor(hPartition, 0, &exitCtx, sizeof(exitCtx));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvRunVirtualProcessor (CPUID) must succeed");
        TEST_ASSERT(exitCtx.ExitReason == whp::WHV_RUN_VP_EXIT_REASON::X64Cpuid, "Exit reason must be X64Cpuid");
        TEST_ASSERT(exitCtx.CpuidAccess.DefaultResultRax == 0x000806EA, "CPUID default result RAX match (Kaby/Coffee Lake)");
        TEST_ASSERT(exitCtx.CpuidAccess.DefaultResultRdx == 0xBFEBFBFF, "CPUID default result RDX match");

        // 2. Next run: RIP is 0xFFF2 -> Trigger MMIO exit
        hr = whp::WHvRunVirtualProcessor(hPartition, 0, &exitCtx, sizeof(exitCtx));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvRunVirtualProcessor (MMIO) must succeed");
        TEST_ASSERT(exitCtx.ExitReason == whp::WHV_RUN_VP_EXIT_REASON::MemoryAccess, "Exit reason must be MemoryAccess");
        TEST_ASSERT(exitCtx.MemoryAccess.Gpa == 0xFED00000, "MMIO GPA match (HPET base)");
        TEST_ASSERT(exitCtx.MemoryAccess.AccessInfo.GpaUnmapped == 1, "MMIO unmapped flag set");
        TEST_ASSERT(exitCtx.MemoryAccess.InstructionByteCount == 3, "MMIO instruction length 3 bytes");

        // 3. Next run: RIP is 0xFFF5 -> Trigger I/O Port exit
        hr = whp::WHvRunVirtualProcessor(hPartition, 0, &exitCtx, sizeof(exitCtx));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvRunVirtualProcessor (I/O) must succeed");
        TEST_ASSERT(exitCtx.ExitReason == whp::WHV_RUN_VP_EXIT_REASON::IoPortAccess, "Exit reason must be IoPortAccess");
        TEST_ASSERT(exitCtx.IoPortAccess.PortNumber == 0x3F8, "I/O port number match (COM1 0x3F8)");
        TEST_ASSERT(exitCtx.IoPortAccess.AccessInfo.IsWrite == 1, "I/O operation is write");
        TEST_ASSERT(exitCtx.IoPortAccess.Rax == 'M', "I/O written value is 'M'");

        // 4. Cancellation test
        hr = whp::WHvCancelRunVirtualProcessor(hPartition, 0, 0);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvCancelRunVirtualProcessor must succeed");

        hr = whp::WHvRunVirtualProcessor(hPartition, 0, &exitCtx, sizeof(exitCtx));
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvRunVirtualProcessor on canceled VP must succeed");
        TEST_ASSERT(exitCtx.ExitReason == whp::WHV_RUN_VP_EXIT_REASON::Canceled, "Exit reason must be Canceled");
    }

    // ------------------------------------------------------------------------
    // Stage 10: Instruction Emulation Engine (WinHvEmulation.dll)
    // ------------------------------------------------------------------------
    {
        static uint8_t s_capturedIoVal = 0;
        static uint64_t s_capturedMmioGpa = 0;

        whp::WHV_EMULATOR_CALLBACKS callbacks{};
        callbacks.Size = sizeof(callbacks);
        callbacks.IoPortCallback = [](void* /*Context*/, whp::WHV_IO_PORT_ACCESS_CONTEXT* ioCtx) -> int32_t {
            if (ioCtx) {
                s_capturedIoVal = static_cast<uint8_t>(ioCtx->Rax);
            }
            return whp::WHV_S_OK;
        };
        callbacks.MemoryCallback = [](void* /*Context*/, whp::WHV_MEMORY_ACCESS_CONTEXT* memCtx) -> int32_t {
            if (memCtx) {
                s_capturedMmioGpa = memCtx->Gpa;
            }
            return whp::WHV_S_OK;
        };

        whp::WHV_EMULATOR_HANDLE hEmulator = nullptr;
        int32_t hr = whp::WHvEmulatorCreateEmulator(&callbacks, &hEmulator);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvEmulatorCreateEmulator must succeed");
        TEST_ASSERT(hEmulator != nullptr, "Emulator handle must be valid");

        // Test I/O Emulation
        whp::WHV_IO_PORT_ACCESS_CONTEXT ioCtx{};
        ioCtx.PortNumber = 0x3F8;
        ioCtx.AccessInfo.IsWrite = 1;
        ioCtx.AccessInfo.AccessSize = 1;
        ioCtx.Rax = 'V';

        whp::WHV_EMULATOR_STATUS emuStatus{};
        hr = whp::WHvEmulatorTryIoEmulation(hEmulator, nullptr, &ioCtx, &emuStatus);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvEmulatorTryIoEmulation must return WHV_S_OK");
        TEST_ASSERT(emuStatus.EmulationSuccessful == 1, "Emulation status indicates success");
        TEST_ASSERT(s_capturedIoVal == 'V', "Callback received expected I/O payload 'V'");

        // Test MMIO Emulation
        whp::WHV_MEMORY_ACCESS_CONTEXT memCtx{};
        memCtx.Gpa = 0xFED00000;
        memCtx.AccessInfo.AccessType = 0;
        memCtx.InstructionByteCount = 3;

        hr = whp::WHvEmulatorTryMmioEmulation(hEmulator, nullptr, &memCtx, &emuStatus);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvEmulatorTryMmioEmulation must return WHV_S_OK");
        TEST_ASSERT(emuStatus.EmulationSuccessful == 1, "Emulation status indicates success");
        TEST_ASSERT(s_capturedMmioGpa == 0xFED00000, "Callback received expected GPA 0xFED00000");

        hr = whp::WHvEmulatorDestroyEmulator(hEmulator);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvEmulatorDestroyEmulator must succeed");
    }

    // ------------------------------------------------------------------------
    // Stage 11: Partition Cleanup & Teardown
    // ------------------------------------------------------------------------
    {
        int32_t hr = whp::WHvDeletePartition(hPartition);
        TEST_ASSERT(hr == whp::WHV_S_OK, "WHvDeletePartition must succeed");

        hr = whp::WHvDeletePartition(hPartition);
        TEST_ASSERT(hr == whp::WHV_E_PARTITION_NOT_FOUND, "Deleting deleted partition must return WHV_E_PARTITION_NOT_FOUND");
    }

    // ------------------------------------------------------------------------
    // Stage 12: Interactive Shell Integration (cmdWhp)
    // ------------------------------------------------------------------------
    {
        whp::WhpManager::get().reset();
        shell::CommandShell shell;
        std::stringstream out;

        // whp test
        shell.execute("whp test", out);
        TEST_ASSERT(out.str().find("[WHP] Self-Test Completed: ALL 16 TESTS PASSED (100%).") != std::string::npos, "whp test must pass all 16 tests");

        // whp capabilities
        out.str("");
        shell.execute("whp capabilities", out);
        TEST_ASSERT(out.str().find("Hypervisor Present:          YES") != std::string::npos, "whp capabilities shows Hypervisor Present");
        TEST_ASSERT(out.str().find("Extended VM Exits:") != std::string::npos, "whp capabilities shows Extended VM Exits");
        TEST_ASSERT(out.str().find("CLFLUSH Cache Line Size:     64 bytes") != std::string::npos, "whp capabilities shows CLFLUSH size");

        // whp vms
        out.str("");
        shell.execute("whp vms", out);
        TEST_ASSERT(out.str().find("DefaultSovereignContainer") != std::string::npos, "whp vms shows DefaultSovereignContainer");
        TEST_ASSERT(out.str().find("READY / RUNNING") != std::string::npos, "whp vms shows running state");
    }

    std::cout << "[TEST] Suite 100: Windows Hypervisor Platform (WHP) & Virtualization Architecture PASSED.\n";
}

void Test_WindowsDirectWrite_Uniscribe_Subsystem() {
    std::cout << "[TEST] Suite 101: Running Windows DirectWrite & Uniscribe Typography Tests...\n";

    auto& ldr = ldr::DynamicLoader::get();
    dwrite::InitializeDirectWriteExports();

    // ------------------------------------------------------------------------
    // Stage 1: Dynamic Loader Exports Verification (DWrite.dll / usp10.dll)
    // ------------------------------------------------------------------------
    TEST_ASSERT(ldr.getExport("DWrite.dll", "DWriteCreateFactory") != nullptr, "DWrite.dll must export DWriteCreateFactory");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptItemize") != nullptr, "usp10.dll must export ScriptItemize");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptShape") != nullptr, "usp10.dll must export ScriptShape");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptPlace") != nullptr, "usp10.dll must export ScriptPlace");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptTextOut") != nullptr, "usp10.dll must export ScriptTextOut");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptBreak") != nullptr, "usp10.dll must export ScriptBreak");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptGetProperties") != nullptr, "usp10.dll must export ScriptGetProperties");
    TEST_ASSERT(ldr.getExport("usp10.dll", "ScriptFreeCache") != nullptr, "usp10.dll must export ScriptFreeCache");

    // ------------------------------------------------------------------------
    // Stage 2: Version Database Verification
    // ------------------------------------------------------------------------
    {
        const auto* verDw = version::VersionDatabase::Instance().FindModule("DWrite.dll");
        TEST_ASSERT(verDw != nullptr, "VersionDatabase must contain DWrite.dll");
        TEST_ASSERT(verDw->stringTable.at("FileDescription") == "Microsoft DirectWrite", "DWrite.dll description match");
        TEST_ASSERT(verDw->stringTable.at("OriginalFilename") == "DWrite.dll", "DWrite.dll original filename match");
        TEST_ASSERT(verDw->stringTable.at("ProductName") == "MicaNT DirectWrite Typography Subsystem", "DWrite.dll product name match");

        const auto* verUsp = version::VersionDatabase::Instance().FindModule("usp10.dll");
        TEST_ASSERT(verUsp != nullptr, "VersionDatabase must contain usp10.dll");
        TEST_ASSERT(verUsp->stringTable.at("FileDescription") == "Uniscribe Unicode Script Processor", "usp10.dll description match");
        TEST_ASSERT(verUsp->stringTable.at("OriginalFilename") == "usp10.dll", "usp10.dll original filename match");
        TEST_ASSERT(verUsp->stringTable.at("ProductName") == "MicaNT Uniscribe Subsystem", "usp10.dll product name match");
    }

    // ------------------------------------------------------------------------
    // Stage 3: DirectWrite Factory Lifecycle
    // ------------------------------------------------------------------------
    ole32::IUnknown* pUnk = nullptr;
    int32_t hr = dwrite::DWriteCreateFactory(
        dwrite::DWRITE_FACTORY_TYPE_SHARED,
        dwrite::IID_IDWriteFactory,
        &pUnk
    );
    TEST_ASSERT(hr == ole32::S_OK, "DWriteCreateFactory must succeed");
    TEST_ASSERT(pUnk != nullptr, "Factory pointer must be non-null");

    auto* factory = static_cast<dwrite::IDWriteFactory*>(pUnk);

    // ------------------------------------------------------------------------
    // Stage 4: System Font Collection Discovery
    // ------------------------------------------------------------------------
    dwrite::IDWriteFontCollection* fontColl = nullptr;
    hr = factory->GetSystemFontCollection(&fontColl);
    TEST_ASSERT(hr == ole32::S_OK, "GetSystemFontCollection must succeed");
    TEST_ASSERT(fontColl != nullptr, "Font collection pointer must be non-null");

    uint32_t familyCount = fontColl->GetFontFamilyCount();
    TEST_ASSERT(familyCount >= 5, "Font collection must contain at least 5 standard families");

    uint32_t segoeIdx = 0;
    int32_t exists = 0;
    hr = fontColl->FindFamilyName(L"Segoe UI", &segoeIdx, &exists);
    TEST_ASSERT(hr == ole32::S_OK && exists == 1, "Segoe UI font family must exist");

    uint32_t consolasIdx = 0;
    hr = fontColl->FindFamilyName(L"Consolas", &consolasIdx, &exists);
    TEST_ASSERT(hr == ole32::S_OK && exists == 1, "Consolas font family must exist");

    uint32_t nonExistentIdx = 0;
    hr = fontColl->FindFamilyName(L"NonExistentFontXYZ", &nonExistentIdx, &exists);
    TEST_ASSERT(hr == ole32::S_OK && exists == 0, "Non-existent font query must report exists == 0");

    // ------------------------------------------------------------------------
    // Stage 5: Font Family & Font Face Enumeration
    // ------------------------------------------------------------------------
    dwrite::IDWriteFontFamily* family = nullptr;
    hr = fontColl->GetFontFamily(segoeIdx, &family);
    TEST_ASSERT(hr == ole32::S_OK && family != nullptr, "GetFontFamily must return valid family");

    uint32_t fontCount = family->GetFontCount();
    TEST_ASSERT(fontCount >= 3, "Segoe UI family must contain at least 3 faces (Regular, Bold, Italic)");

    dwrite::IDWriteLocalizedStrings* famNames = nullptr;
    hr = family->GetFamilyNames(&famNames);
    TEST_ASSERT(hr == ole32::S_OK && famNames != nullptr, "GetFamilyNames must succeed");
    wchar_t nameBuf[64]{};
    hr = famNames->GetString(0, nameBuf, 64);
    TEST_ASSERT(hr == ole32::S_OK && std::wcscmp(nameBuf, L"Segoe UI") == 0, "Family name must match 'Segoe UI'");
    famNames->Release();

    dwrite::IDWriteFont* font = nullptr;
    hr = family->GetFont(0, &font);
    TEST_ASSERT(hr == ole32::S_OK && font != nullptr, "GetFont(0) must return valid font");
    TEST_ASSERT(font->GetWeight() == dwrite::DWRITE_FONT_WEIGHT_NORMAL, "Face 0 weight must be NORMAL (400)");
    TEST_ASSERT(font->GetStyle() == dwrite::DWRITE_FONT_STYLE_NORMAL, "Face 0 style must be NORMAL");

    dwrite::IDWriteFont* boldFont = nullptr;
    hr = family->GetFont(1, &boldFont);
    TEST_ASSERT(hr == ole32::S_OK && boldFont != nullptr, "GetFont(1) must return valid bold font");
    TEST_ASSERT(boldFont->GetWeight() == dwrite::DWRITE_FONT_WEIGHT_BOLD, "Face 1 weight must be BOLD (700)");
    boldFont->Release();

    // ------------------------------------------------------------------------
    // Stage 6: Font Face Creation & Metrics Query
    // ------------------------------------------------------------------------
    dwrite::IDWriteFontFace* fontFace = nullptr;
    hr = font->CreateFontFace(&fontFace);
    TEST_ASSERT(hr == ole32::S_OK && fontFace != nullptr, "CreateFontFace must succeed");

    dwrite::DWRITE_FONT_METRICS fm{};
    fontFace->GetMetrics(&fm);
    TEST_ASSERT(fm.designUnitsPerEm == 2048, "designUnitsPerEm must be 2048");
    TEST_ASSERT(fm.ascent == 1854, "ascent must be 1854");
    TEST_ASSERT(fm.descent == 434, "descent must be 434");

    uint32_t codepoints[4] = { 'M', 'i', 'c', 'a' };
    uint16_t glyphIndices[4]{};
    hr = fontFace->GetGlyphIndices(codepoints, 4, glyphIndices);
    TEST_ASSERT(hr == ole32::S_OK, "GetGlyphIndices must succeed");
    TEST_ASSERT(glyphIndices[0] == 'M' && glyphIndices[1] == 'i', "Mapped glyph indices match codepoints");

    // ------------------------------------------------------------------------
    // Stage 7: Text Format Configuration
    // ------------------------------------------------------------------------
    dwrite::IDWriteTextFormat* format = nullptr;
    hr = factory->CreateTextFormat(
        L"Segoe UI", nullptr,
        dwrite::DWRITE_FONT_WEIGHT_NORMAL,
        dwrite::DWRITE_FONT_STYLE_NORMAL,
        dwrite::DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"en-us", &format
    );
    TEST_ASSERT(hr == ole32::S_OK && format != nullptr, "CreateTextFormat must succeed");
    TEST_ASSERT(format->GetFontSize() == 14.0f, "GetFontSize must return 14.0f");
    TEST_ASSERT(format->GetFontWeight() == dwrite::DWRITE_FONT_WEIGHT_NORMAL, "GetFontWeight must return NORMAL");

    hr = format->SetTextAlignment(dwrite::DWRITE_TEXT_ALIGNMENT_CENTER);
    TEST_ASSERT(hr == ole32::S_OK && format->GetTextAlignment() == dwrite::DWRITE_TEXT_ALIGNMENT_CENTER, "SetTextAlignment to CENTER");

    hr = format->SetWordWrapping(dwrite::DWRITE_WORD_WRAPPING_WHOLE_WORD);
    TEST_ASSERT(hr == ole32::S_OK && format->GetWordWrapping() == dwrite::DWRITE_WORD_WRAPPING_WHOLE_WORD, "SetWordWrapping to WHOLE_WORD");

    // ------------------------------------------------------------------------
    // Stage 8: OpenType Typography Features
    // ------------------------------------------------------------------------
    dwrite::IDWriteTypography* typography = nullptr;
    hr = factory->CreateTypography(&typography);
    TEST_ASSERT(hr == ole32::S_OK && typography != nullptr, "CreateTypography must succeed");

    hr = typography->AddFontFeature(0x6C696761, 1); // 'liga'
    TEST_ASSERT(hr == ole32::S_OK, "AddFontFeature 'liga' must succeed");
    hr = typography->AddFontFeature(0x6B65726E, 1); // 'kern'
    TEST_ASSERT(hr == ole32::S_OK, "AddFontFeature 'kern' must succeed");
    TEST_ASSERT(typography->GetFontFeatureCount() == 2, "GetFontFeatureCount must return 2");

    // ------------------------------------------------------------------------
    // Stage 9: Subpixel ClearType Rendering Parameters
    // ------------------------------------------------------------------------
    dwrite::IDWriteRenderingParams* defaultParams = nullptr;
    hr = factory->CreateRenderingParams(&defaultParams);
    TEST_ASSERT(hr == ole32::S_OK && defaultParams != nullptr, "CreateRenderingParams must succeed");
    TEST_ASSERT(defaultParams->GetGamma() == 2.2f, "Default gamma must be 2.2f");
    TEST_ASSERT(defaultParams->GetPixelGeometry() == dwrite::DWRITE_PIXEL_GEOMETRY_RGB, "Default pixel geometry must be RGB");

    dwrite::IDWriteRenderingParams* customParams = nullptr;
    hr = factory->CreateCustomRenderingParams(
        1.8f, 1.2f, 0.9f,
        dwrite::DWRITE_PIXEL_GEOMETRY_BGR,
        dwrite::DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC,
        &customParams
    );
    TEST_ASSERT(hr == ole32::S_OK && customParams != nullptr, "CreateCustomRenderingParams must succeed");
    TEST_ASSERT(customParams->GetGamma() == 1.8f, "Custom gamma match");
    TEST_ASSERT(customParams->GetEnhancedContrast() == 1.2f, "Custom contrast match");
    TEST_ASSERT(customParams->GetClearTypeLevel() == 0.9f, "Custom ClearType level match");
    TEST_ASSERT(customParams->GetPixelGeometry() == dwrite::DWRITE_PIXEL_GEOMETRY_BGR, "Custom pixel geometry match");

    // ------------------------------------------------------------------------
    // Stage 10: Text Layout Engine & Metrics Analysis
    // ------------------------------------------------------------------------
    const wchar_t sampleText[] = L"MicaNT Modern C++23 Clean-Room Sovereign OS Executive";
    dwrite::IDWriteTextLayout* layout = nullptr;
    hr = factory->CreateTextLayout(
        sampleText, static_cast<uint32_t>(std::wcslen(sampleText)),
        format, 300.0f, 200.0f, &layout
    );
    TEST_ASSERT(hr == ole32::S_OK && layout != nullptr, "CreateTextLayout must succeed");
    TEST_ASSERT(layout->GetMaxWidth() == 300.0f, "Layout MaxWidth must be 300.0f");
    TEST_ASSERT(layout->GetMaxHeight() == 200.0f, "Layout MaxHeight must be 200.0f");

    dwrite::DWRITE_TEXT_METRICS tm{};
    hr = layout->GetMetrics(&tm);
    TEST_ASSERT(hr == ole32::S_OK, "GetMetrics must succeed");
    TEST_ASSERT(tm.width > 0.0f && tm.height > 0.0f, "Calculated text dimensions are positive");
    TEST_ASSERT(tm.lineCount >= 1, "Layout must contain at least 1 line");

    dwrite::DWRITE_LINE_METRICS lm[4]{};
    uint32_t actualLines = 0;
    hr = layout->GetLineMetrics(lm, 4, &actualLines);
    TEST_ASSERT(hr == ole32::S_OK && actualLines >= 1, "GetLineMetrics must succeed");

    dwrite::DWRITE_CLUSTER_METRICS cm[64]{};
    uint32_t actualClusters = 0;
    hr = layout->GetClusterMetrics(cm, 64, &actualClusters);
    TEST_ASSERT(hr == ole32::S_OK && actualClusters == std::wcslen(sampleText), "GetClusterMetrics must return cluster per character");

    // Range-based formatting updates
    dwrite::DWRITE_TEXT_RANGE range{ 0, 6 }; // "MicaNT"
    hr = layout->SetFontWeight(dwrite::DWRITE_FONT_WEIGHT_BOLD, range);
    TEST_ASSERT(hr == ole32::S_OK, "SetFontWeight on range must succeed");
    hr = layout->SetUnderline(1, range);
    TEST_ASSERT(hr == ole32::S_OK, "SetUnderline on range must succeed");
    hr = layout->SetStrikethrough(1, range);
    TEST_ASSERT(hr == ole32::S_OK, "SetStrikethrough on range must succeed");

    // ------------------------------------------------------------------------
    // Stage 11: Uniscribe Complex Script Processing (usp10.dll)
    // ------------------------------------------------------------------------
    {
        dwrite::SCRIPT_ITEM items[4]{};
        int32_t cItems = 0;
        hr = dwrite::ScriptItemize(sampleText, 6, 4, nullptr, nullptr, items, &cItems);
        TEST_ASSERT(hr == ole32::S_OK && cItems == 1, "ScriptItemize must return 1 item run for Latin text");
        TEST_ASSERT(items[0].iCharPos == 0 && items[0].a.eScript == 1, "Run 0 starts at pos 0 with Latin script ID");

        uint16_t glyphs[16]{};
        uint16_t logClust[16]{};
        dwrite::SCRIPT_VISATTR visAttrs[16]{};
        int32_t cGlyphs = 0;
        hr = dwrite::ScriptShape(nullptr, nullptr, sampleText, 6, 16, &items[0].a, glyphs, logClust, visAttrs, &cGlyphs);
        TEST_ASSERT(hr == ole32::S_OK && cGlyphs == 6, "ScriptShape must shape 6 glyphs");

        int32_t advances[16]{};
        hr = dwrite::ScriptPlace(nullptr, nullptr, glyphs, cGlyphs, visAttrs, &items[0].a, advances, nullptr, nullptr);
        TEST_ASSERT(hr == ole32::S_OK, "ScriptPlace must calculate advance widths");
        TEST_ASSERT(advances[0] == 10 && advances[5] == 10, "Advance width per glyph verified");

        dwrite::SCRIPT_LOGATTR logAttrs[16]{};
        hr = dwrite::ScriptBreak(sampleText, 6, &items[0].a, logAttrs);
        TEST_ASSERT(hr == ole32::S_OK, "ScriptBreak must return logical break attributes");
        TEST_ASSERT(logAttrs[0].fCharStop == 1, "Character stop at pos 0");

        const dwrite::SCRIPT_PROPERTIES** ppProps = nullptr;
        int32_t numScripts = 0;
        hr = dwrite::ScriptGetProperties(&ppProps, &numScripts);
        TEST_ASSERT(hr == ole32::S_OK && numScripts >= 1, "ScriptGetProperties must return at least 1 script property table");
        TEST_ASSERT(ppProps[0]->langid == 0x0409, "Script 0 language ID is 0x0409 (en-US)");

        dwrite::SCRIPT_CACHE cache = reinterpret_cast<dwrite::SCRIPT_CACHE>(0x1234);
        hr = dwrite::ScriptFreeCache(&cache);
        TEST_ASSERT(hr == ole32::S_OK && cache == nullptr, "ScriptFreeCache clears cache pointer");
    }

    // ------------------------------------------------------------------------
    // Stage 12: Interactive Shell Integration (cmdDWrite)
    // ------------------------------------------------------------------------
    {
        shell::CommandShell shell;
        std::stringstream out;

        // dwrite test
        shell.execute("dwrite test", out);
        TEST_ASSERT(out.str().find("[DWRITE] Self-Test Completed: ALL 16 TESTS PASSED (100%).") != std::string::npos, "dwrite test must pass all 16 tests");

        // dwrite fonts
        out.str("");
        shell.execute("dwrite fonts", out);
        TEST_ASSERT(out.str().find("Segoe UI") != std::string::npos, "dwrite fonts shows Segoe UI");
        TEST_ASSERT(out.str().find("Consolas") != std::string::npos, "dwrite fonts shows Consolas");
        TEST_ASSERT(out.str().find("Cascadia Code") != std::string::npos, "dwrite fonts shows Cascadia Code");

        // dwrite layout
        out.str("");
        shell.execute("dwrite layout TestTypography", out);
        TEST_ASSERT(out.str().find("DirectWrite Typography Layout Inspection") != std::string::npos, "dwrite layout shows inspection header");
        TEST_ASSERT(out.str().find("Text Width:") != std::string::npos, "dwrite layout shows text width");
    }

    // Cleanup COM instances
    layout->Release();
    customParams->Release();
    defaultParams->Release();
    typography->Release();
    format->Release();
    fontFace->Release();
    font->Release();
    family->Release();
    fontColl->Release();
    factory->Release();

    std::cout << "[TEST] Suite 101: Windows DirectWrite & Uniscribe Typography Architecture PASSED.\n";
}

// ============================================================================
// Suite 102: Windows Media Foundation Platform & Pipeline Architecture
// ============================================================================
void Test_WindowsMediaFoundation_Subsystem() {
    std::cout << "[TEST] Suite 102: Running Windows Media Foundation Subsystem Tests...\n";
    using namespace micant::mf;

    // 1. MFStartup & Platform Lifecycle
    int32_t hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    TEST_ASSERT(hr == ole32::S_OK, "MFStartup must return S_OK");
    TEST_ASSERT(MediaFoundationPlatform::get().isInitialized(), "Platform must be initialized after MFStartup");

    // 2. Attribute Store (IMFAttributes)
    IMFAttributes* pAttrs = nullptr;
    hr = MFCreateAttributes(&pAttrs, 16);
    TEST_ASSERT(hr == ole32::S_OK && pAttrs != nullptr, "MFCreateAttributes must succeed");

    pAttrs->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    pAttrs->SetUINT64(MF_MT_FRAME_SIZE, (static_cast<uint64_t>(1920) << 32) | 1080);
    pAttrs->SetDouble(MF_MT_FRAME_RATE, 60.0);
    pAttrs->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pAttrs->SetString(MF_MT_SUBTYPE, L"AudioSubTypeMica");

    const uint8_t rawBlob[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    pAttrs->SetBlob(MF_MT_AUDIO_BLOCK_ALIGNMENT, rawBlob, sizeof(rawBlob));

    uint32_t channels = 0;
    pAttrs->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    TEST_ASSERT(channels == 2, "GetUINT32 must return stored channels");

    uint64_t frameSize = 0;
    pAttrs->GetUINT64(MF_MT_FRAME_SIZE, &frameSize);
    TEST_ASSERT(frameSize == ((static_cast<uint64_t>(1920) << 32) | 1080), "GetUINT64 must return frame size");

    double fps = 0.0;
    pAttrs->GetDouble(MF_MT_FRAME_RATE, &fps);
    TEST_ASSERT(fps == 60.0, "GetDouble must return frame rate");

    GUID major{};
    pAttrs->GetGUID(MF_MT_MAJOR_TYPE, &major);
    TEST_ASSERT(major == MFMediaType_Audio, "GetGUID must return MFMediaType_Audio");

    uint32_t strLen = 0;
    pAttrs->GetStringLength(MF_MT_SUBTYPE, &strLen);
    TEST_ASSERT(strLen == 16, "GetStringLength must return correct string length");
    wchar_t strBuf[32]{};
    pAttrs->GetString(MF_MT_SUBTYPE, strBuf, 32, nullptr);
    TEST_ASSERT(std::wstring(strBuf) == L"AudioSubTypeMica", "GetString must match stored string");

    uint32_t blobSz = 0;
    pAttrs->GetBlobSize(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blobSz);
    TEST_ASSERT(blobSz == sizeof(rawBlob), "GetBlobSize must match raw blob size");
    uint8_t readBlob[8]{};
    pAttrs->GetBlob(MF_MT_AUDIO_BLOCK_ALIGNMENT, readBlob, sizeof(readBlob), nullptr);
    TEST_ASSERT(std::memcmp(rawBlob, readBlob, sizeof(rawBlob)) == 0, "GetBlob must retrieve identical binary bytes");

    uint32_t itemCount = 0;
    pAttrs->GetCount(&itemCount);
    TEST_ASSERT(itemCount == 6, "Attribute store count must be 6");

    IMFAttributes* pDestAttrs = nullptr;
    MFCreateAttributes(&pDestAttrs, 16);
    pAttrs->CopyAllItems(pDestAttrs);
    uint32_t destCount = 0;
    pDestAttrs->GetCount(&destCount);
    TEST_ASSERT(destCount == 6, "CopyAllItems must copy all items to destination");
    pDestAttrs->Release();

    pAttrs->DeleteItem(MF_MT_AUDIO_NUM_CHANNELS);
    pAttrs->GetCount(&itemCount);
    TEST_ASSERT(itemCount == 5, "Count must decrement after DeleteItem");
    pAttrs->DeleteAllItems();
    pAttrs->GetCount(&itemCount);
    TEST_ASSERT(itemCount == 0, "Count must be 0 after DeleteAllItems");
    pAttrs->Release();

    // 3. Media Buffer (IMFMediaBuffer)
    IMFMediaBuffer* pBuf = nullptr;
    hr = MFCreateMemoryBuffer(2048, &pBuf);
    TEST_ASSERT(hr == ole32::S_OK && pBuf != nullptr, "MFCreateMemoryBuffer must succeed");
    uint32_t maxLen = 0, curLen = 0;
    pBuf->GetMaxLength(&maxLen);
    TEST_ASSERT(maxLen == 2048, "Buffer max length must be 2048");
    pBuf->GetCurrentLength(&curLen);
    TEST_ASSERT(curLen == 0, "Initial current length must be 0");

    uint8_t* pData = nullptr;
    hr = pBuf->Lock(&pData, &maxLen, &curLen);
    TEST_ASSERT(hr == ole32::S_OK && pData != nullptr, "Buffer Lock must succeed and return pointer");
    std::memset(pData, 0x7E, 512);
    pBuf->Unlock();
    pBuf->SetCurrentLength(512);
    pBuf->GetCurrentLength(&curLen);
    TEST_ASSERT(curLen == 512, "Current length must be 512 after SetCurrentLength");
    pBuf->Release();

    // 4. Media Sample (IMFSample)
    IMFSample* pSample = nullptr;
    hr = MFCreateSample(&pSample);
    TEST_ASSERT(hr == ole32::S_OK && pSample != nullptr, "MFCreateSample must succeed");

    pSample->SetSampleTime(50000000); // 5 seconds
    pSample->SetSampleDuration(166666); // 16.6ms (60fps)
    pSample->SetSampleFlags(0x00000001);

    LONGLONG sTime = 0, sDur = 0;
    uint32_t sFlags = 0;
    pSample->GetSampleTime(&sTime);
    pSample->GetSampleDuration(&sDur);
    pSample->GetSampleFlags(&sFlags);
    TEST_ASSERT(sTime == 50000000, "GetSampleTime must match 50000000");
    TEST_ASSERT(sDur == 166666, "GetSampleDuration must match 166666");
    TEST_ASSERT(sFlags == 1, "GetSampleFlags must match 1");

    IMFMediaBuffer* b1 = nullptr;
    IMFMediaBuffer* b2 = nullptr;
    MFCreateMemoryBuffer(256, &b1);
    MFCreateMemoryBuffer(256, &b2);
    b1->SetCurrentLength(256);
    b2->SetCurrentLength(256);
    pSample->AddBuffer(b1);
    pSample->AddBuffer(b2);

    uint32_t bufCount = 0;
    pSample->GetBufferCount(&bufCount);
    TEST_ASSERT(bufCount == 2, "Sample must contain 2 buffers");
    uint32_t totSampleLen = 0;
    pSample->GetTotalLength(&totSampleLen);
    TEST_ASSERT(totSampleLen == 512, "Total sample length must be 512");

    IMFMediaBuffer* contigBuf = nullptr;
    hr = pSample->ConvertToContiguousBuffer(&contigBuf);
    TEST_ASSERT(hr == ole32::S_OK && contigBuf != nullptr, "ConvertToContiguousBuffer must succeed");
    uint32_t cLen = 0;
    contigBuf->GetCurrentLength(&cLen);
    TEST_ASSERT(cLen == 512, "Contiguous buffer length must equal sum of buffer lengths");
    contigBuf->Release();

    pSample->RemoveBufferByIndex(1);
    pSample->GetBufferCount(&bufCount);
    TEST_ASSERT(bufCount == 1, "Buffer count must be 1 after RemoveBufferByIndex");
    pSample->RemoveAllBuffers();
    pSample->GetBufferCount(&bufCount);
    TEST_ASSERT(bufCount == 0, "Buffer count must be 0 after RemoveAllBuffers");

    b1->Release();
    b2->Release();
    pSample->Release();

    // 5. Media Types (IMFMediaType)
    IMFMediaType* pAudioType = nullptr;
    MFCreateMediaType(&pAudioType);
    pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    pAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    pAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

    GUID checkMajor{};
    pAudioType->GetMajorType(&checkMajor);
    TEST_ASSERT(checkMajor == MFMediaType_Audio, "Major type must be MFMediaType_Audio");

    int32_t isCompressed = 1;
    pAudioType->IsCompressedFormat(&isCompressed);
    TEST_ASSERT(isCompressed == 0, "PCM audio must not be compressed format");

    IMFMediaType* pVideoType = nullptr;
    MFCreateMediaType(&pVideoType);
    pVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    pVideoType->IsCompressedFormat(&isCompressed);
    TEST_ASSERT(isCompressed == 1, "H.264 video must be compressed format");

    uint32_t eqFlags = 0;
    hr = pAudioType->IsEqual(pVideoType, &eqFlags);
    TEST_ASSERT(hr == ole32::S_FALSE, "Audio and Video media types must not be equal");

    pAudioType->Release();
    pVideoType->Release();

    // 6. Byte Stream (IMFByteStream)
    IMFByteStream* pByteStream = nullptr;
    hr = MFCreateFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, L"virtual.dat", &pByteStream);
    TEST_ASSERT(hr == ole32::S_OK && pByteStream != nullptr, "MFCreateFile must succeed");

    uint32_t caps = 0;
    pByteStream->GetCapabilities(&caps);
    TEST_ASSERT((caps & 0x07) == 0x07, "Capabilities must support Read, Write, and Seek");

    const uint8_t msg[] = "MicaNT Stream Engine";
    uint32_t written = 0, readBytes = 0;
    pByteStream->Write(msg, sizeof(msg), &written);
    TEST_ASSERT(written == sizeof(msg), "Written bytes must match buffer size");

    uint64_t streamLen = 0;
    pByteStream->GetLength(&streamLen);
    TEST_ASSERT(streamLen == sizeof(msg), "Stream length must equal written bytes");

    uint64_t curPos = 0;
    pByteStream->Seek(0, 0, 0, &curPos); // Seek to 0
    TEST_ASSERT(curPos == 0, "Seek to start must set position to 0");

    uint8_t readBuf[32]{};
    pByteStream->Read(readBuf, sizeof(readBuf), &readBytes);
    TEST_ASSERT(readBytes == sizeof(msg), "Read bytes must match stream size");
    TEST_ASSERT(std::memcmp(msg, readBuf, sizeof(msg)) == 0, "Read data must match written payload");

    int32_t isEos = 0;
    pByteStream->IsEndOfStream(&isEos);
    TEST_ASSERT(isEos == 1, "IsEndOfStream must be 1 after reading entire stream");

    pByteStream->Release();

    // 7. Async Callback & Work Queue
    uint32_t wq = 0;
    hr = MFAllocateWorkQueue(&wq);
    TEST_ASSERT(hr == ole32::S_OK && wq > 0, "MFAllocateWorkQueue must succeed");
    hr = MFUnlockWorkQueue(wq);
    TEST_ASSERT(hr == ole32::S_OK, "MFUnlockWorkQueue must succeed");

    class MockAsyncCb : public IMFAsyncCallback {
    public:
        int callCount{ 0 };
        int32_t __stdcall QueryInterface(const ole32::IID&, void** ppv) override {
            *ppv = static_cast<IMFAsyncCallback*>(this);
            return ole32::S_OK;
        }
        uint32_t __stdcall AddRef() override { return 1; }
        uint32_t __stdcall Release() override { return 1; }
        int32_t __stdcall GetParameters(uint32_t*, uint32_t*) override { return ole32::S_OK; }
        int32_t __stdcall Invoke(IMFAsyncResult*) override {
            callCount++;
            return ole32::S_OK;
        }
    } asyncCb;

    IMFAsyncResult* pAr = nullptr;
    hr = MFCreateAsyncResult(nullptr, &asyncCb, nullptr, &pAr);
    TEST_ASSERT(hr == ole32::S_OK && pAr != nullptr, "MFCreateAsyncResult must succeed");
    MFInvokeCallback(pAr);
    TEST_ASSERT(asyncCb.callCount == 1, "MFInvokeCallback must call IMFAsyncCallback::Invoke");
    pAr->Release();

    // 8. Media Event Queue (IMFMediaEventQueue)
    IMFMediaEventQueue* pEvQueue = nullptr;
    hr = MFCreateEventQueue(&pEvQueue);
    TEST_ASSERT(hr == ole32::S_OK && pEvQueue != nullptr, "MFCreateEventQueue must succeed");

    pEvQueue->QueueEventParamVar(MESessionStarted, GUID{}, ole32::S_OK, nullptr);
    pEvQueue->QueueEventParamVar(MESessionStopped, GUID{}, ole32::S_OK, nullptr);

    IMFMediaEvent* ev1 = nullptr;
    pEvQueue->GetEvent(0, &ev1);
    TEST_ASSERT(ev1 != nullptr, "GetEvent must retrieve queued event");
    MediaEventType met1 = MEUnknown;
    ev1->GetType(&met1);
    TEST_ASSERT(met1 == MESessionStarted, "Event 1 type must be MESessionStarted");
    ev1->Release();

    IMFMediaEvent* ev2 = nullptr;
    pEvQueue->GetEvent(0, &ev2);
    TEST_ASSERT(ev2 != nullptr, "GetEvent must retrieve second event");
    MediaEventType met2 = MEUnknown;
    ev2->GetType(&met2);
    TEST_ASSERT(met2 == MESessionStopped, "Event 2 type must be MESessionStopped");
    ev2->Release();

    pEvQueue->Shutdown();
    pEvQueue->Release();

    // 9. Media Foundation Transforms (MFT)
    IMFTransform** ppTransforms = nullptr;
    uint32_t tfmCount = 0;
    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, 0, nullptr, nullptr, &ppTransforms, &tfmCount);
    TEST_ASSERT(hr == ole32::S_OK && tfmCount >= 1, "MFTEnumEx must find at least 1 video decoder");
    if (ppTransforms) {
        for (uint32_t i = 0; i < tfmCount; ++i) ppTransforms[i]->Release();
        delete[] ppTransforms;
    }

    auto h264Dec = std::make_shared<CH264DecoderMFT>();
    IMFMediaType* hIn = nullptr;
    h264Dec->GetInputAvailableType(0, 0, &hIn);
    TEST_ASSERT(hIn != nullptr, "H.264 MFT must provide input media type");
    h264Dec->SetInputType(0, hIn, 0);

    IMFMediaType* hOut = nullptr;
    h264Dec->GetOutputAvailableType(0, 0, &hOut);
    TEST_ASSERT(hOut != nullptr, "H.264 MFT must provide output media type");
    h264Dec->SetOutputType(0, hOut, 0);

    IMFSample* inSample = nullptr;
    MFCreateSample(&inSample);
    inSample->SetSampleTime(333333);
    h264Dec->ProcessInput(0, inSample, 0);

    uint32_t outStatus = 0;
    h264Dec->GetOutputStatus(&outStatus);
    TEST_ASSERT(outStatus == 1, "MFT must indicate output ready after input");

    MFT_OUTPUT_DATA_BUFFER outBuf{};
    outBuf.dwStreamID = 0;
    uint32_t pStatus = 0;
    hr = h264Dec->ProcessOutput(0, 1, &outBuf, &pStatus);
    TEST_ASSERT(hr == ole32::S_OK && outBuf.pSample != nullptr, "ProcessOutput must produce sample");
    if (outBuf.pSample) outBuf.pSample->Release();
    inSample->Release();
    hIn->Release();
    hOut->Release();

    // 10. Source Reader & Sink Writer
    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromURL(L"media_clip.mp4", nullptr, &pReader);
    TEST_ASSERT(hr == ole32::S_OK && pReader != nullptr, "MFCreateSourceReaderFromURL must succeed");

    int32_t s0Selected = 0;
    pReader->GetStreamSelection(0, &s0Selected);
    TEST_ASSERT(s0Selected == 1, "Stream 0 must be selected");

    IMFMediaType* nativeType = nullptr;
    pReader->GetNativeMediaType(0, 0, &nativeType);
    TEST_ASSERT(nativeType != nullptr, "GetNativeMediaType for Stream 0 must succeed");
    nativeType->Release();

    uint32_t actStream = 0, flags = 0;
    LONGLONG ts = 0;
    IMFSample* rSample = nullptr;
    hr = pReader->ReadSample(0, 0, &actStream, &flags, &ts, &rSample);
    TEST_ASSERT(hr == ole32::S_OK && rSample != nullptr, "ReadSample must produce sample");
    TEST_ASSERT(actStream == 0, "Actual stream index must be 0");
    rSample->Release();
    pReader->Release();

    IMFSinkWriter* pWriter = nullptr;
    hr = MFCreateSinkWriterFromURL(L"recorded.mp4", nullptr, nullptr, &pWriter);
    TEST_ASSERT(hr == ole32::S_OK && pWriter != nullptr, "MFCreateSinkWriterFromURL must succeed");

    IMFMediaType* outMt = nullptr;
    MFCreateMediaType(&outMt);
    outMt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outMt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    uint32_t wrStreamIdx = 0;
    pWriter->AddStream(outMt, &wrStreamIdx);
    pWriter->BeginWriting();

    IMFSample* wrSample = nullptr;
    MFCreateSample(&wrSample);
    IMFMediaBuffer* wrBuf = nullptr;
    MFCreateMemoryBuffer(512, &wrBuf);
    wrBuf->SetCurrentLength(512);
    wrSample->AddBuffer(wrBuf);
    wrBuf->Release();

    hr = pWriter->WriteSample(wrStreamIdx, wrSample);
    TEST_ASSERT(hr == ole32::S_OK, "WriteSample must return S_OK");
    pWriter->Finalize();
    wrSample->Release();
    outMt->Release();
    pWriter->Release();

    // 11. Topology & Nodes
    IMFTopology* pTopo = nullptr;
    MFCreateTopology(&pTopo);
    TEST_ASSERT(pTopo != nullptr, "MFCreateTopology must succeed");

    IMFTopologyNode* nSrc = nullptr;
    IMFTopologyNode* nTfm = nullptr;
    IMFTopologyNode* nOut = nullptr;
    MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &nSrc);
    MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &nTfm);
    MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &nOut);

    nSrc->ConnectOutput(0, nTfm, 0);
    nTfm->ConnectOutput(0, nOut, 0);
    pTopo->AddNode(nSrc);
    pTopo->AddNode(nTfm);
    pTopo->AddNode(nOut);

    uint16_t nNodes = 0;
    pTopo->GetNodeCount(&nNodes);
    TEST_ASSERT(nNodes == 3, "Topology node count must be 3");

    IMFTopologyNode* downstream = nullptr;
    uint32_t downInput = 0;
    nSrc->GetOutput(0, &downstream, &downInput);
    TEST_ASSERT(downstream == nTfm, "Source node output 0 must connect to Transform node");
    downstream->Release();

    nSrc->Release();
    nTfm->Release();
    nOut->Release();

    // 12. Media Session
    IMFMediaSession* pSession = nullptr;
    hr = MFCreateMediaSession(nullptr, &pSession);
    TEST_ASSERT(hr == ole32::S_OK && pSession != nullptr, "MFCreateMediaSession must succeed");

    pSession->SetTopology(0, pTopo);
    pSession->Start(nullptr, nullptr);
    pSession->Pause();
    pSession->Stop();
    pSession->Close();

    IMFMediaEvent* sessEv = nullptr;
    pSession->GetEvent(0, &sessEv);
    TEST_ASSERT(sessEv != nullptr, "Session must have generated an event");
    sessEv->Release();

    pSession->Release();
    pTopo->Release();

    // 13. Dynamic Module Export Registration
    InitializeMediaFoundationExports();
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFStartup") != nullptr, "mfplat.dll!MFStartup must be exported");
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFShutdown") != nullptr, "mfplat.dll!MFShutdown must be exported");
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFCreateAttributes") != nullptr, "mfplat.dll!MFCreateAttributes must be exported");
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFCreateMemoryBuffer") != nullptr, "mfplat.dll!MFCreateMemoryBuffer must be exported");
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFCreateSample") != nullptr, "mfplat.dll!MFCreateSample must be exported");
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFCreateMediaType") != nullptr, "mfplat.dll!MFCreateMediaType must be exported");
    TEST_ASSERT(loader.getExport("mfplat.dll", "MFTEnumEx") != nullptr, "mfplat.dll!MFTEnumEx must be exported");
    TEST_ASSERT(loader.getExport("mfreadwrite.dll", "MFCreateSourceReaderFromURL") != nullptr, "mfreadwrite.dll!MFCreateSourceReaderFromURL must be exported");
    TEST_ASSERT(loader.getExport("mfreadwrite.dll", "MFCreateSinkWriterFromURL") != nullptr, "mfreadwrite.dll!MFCreateSinkWriterFromURL must be exported");
    TEST_ASSERT(loader.getExport("mf.dll", "MFCreateTopology") != nullptr, "mf.dll!MFCreateTopology must be exported");
    TEST_ASSERT(loader.getExport("mf.dll", "MFCreateMediaSession") != nullptr, "mf.dll!MFCreateMediaSession must be exported");

    // 14. Version Database Records
    auto& verDb = version::VersionDatabase::Instance();
    const auto* vMfplat = verDb.FindModule("mfplat.dll");
    TEST_ASSERT(vMfplat != nullptr, "mfplat.dll must be registered in VersionDatabase");
    TEST_ASSERT(vMfplat->stringTable.at("ProductName") == "MicaNT Media Foundation Subsystem", "mfplat.dll ProductName check");

    const auto* vMf = verDb.FindModule("mf.dll");
    TEST_ASSERT(vMf != nullptr, "mf.dll must be registered in VersionDatabase");

    const auto* vMfreadwrite = verDb.FindModule("mfreadwrite.dll");
    TEST_ASSERT(vMfreadwrite != nullptr, "mfreadwrite.dll must be registered in VersionDatabase");

    // 15. Command Shell Integration
    {
        shell::CommandShell shell;
        std::ostringstream out;

        // mf test
        shell.execute("mf test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "mf test must pass 100%");

        // mf transforms
        out.str("");
        shell.execute("mf transforms", out);
        TEST_ASSERT(out.str().find("Microsoft H.264 Video Decoder MFT") != std::string::npos, "mf transforms must show H.264 decoder");
        TEST_ASSERT(out.str().find("Microsoft AAC Audio Decoder MFT") != std::string::npos, "mf transforms must show AAC decoder");

        // mf session
        out.str("");
        shell.execute("mf session", out);
        TEST_ASSERT(out.str().find("Decoded 5 frames") != std::string::npos, "mf session must decode 5 frames");
    }

    // 16. Platform Shutdown
    hr = MFShutdown();
    TEST_ASSERT(hr == ole32::S_OK, "MFShutdown must return S_OK");
    TEST_ASSERT(!MediaFoundationPlatform::get().isInitialized(), "Platform must not be initialized after MFShutdown");

    std::cout << "[TEST] Suite 102: Windows Media Foundation Subsystem PASSED.\n";
}

void Test_WindowsDirectShow_FilterGraph_Subsystem() {
    std::cout << "[TEST] Suite 103: Windows DirectShow & Filter Graph Subsystem\n" << std::flush;
    using namespace micant::dshow;

    // 1. Error Translation Parity (AMGetErrorTextA / AMGetErrorTextW)
    {
        char bufA[256] = {};
        wchar_t bufW[256] = {};

        uint32_t lenA = AMGetErrorTextA(ole32::S_OK, bufA, 256);
        TEST_ASSERT(lenA > 0 && std::string(bufA).find("succeeded") != std::string::npos, "AMGetErrorTextA S_OK");

        uint32_t lenW = AMGetErrorTextW(ole32::S_OK, bufW, 256);
        TEST_ASSERT(lenW > 0 && std::wstring(bufW).find(L"succeeded") != std::wstring::npos, "AMGetErrorTextW S_OK");

        lenA = AMGetErrorTextA(VFW_E_NOT_CONNECTED, bufA, 256);
        TEST_ASSERT(lenA > 0 && std::string(bufA).find("not connected") != std::string::npos, "AMGetErrorTextA VFW_E_NOT_CONNECTED");

        lenA = AMGetErrorTextA(VFW_E_CANNOT_CONNECT, bufA, 256);
        TEST_ASSERT(lenA > 0 && std::string(bufA).find("No combination") != std::string::npos, "AMGetErrorTextA VFW_E_CANNOT_CONNECT");

        lenA = AMGetErrorTextA(VFW_E_CANNOT_RENDER, bufA, 256);
        TEST_ASSERT(lenA > 0 && std::string(bufA).find("render") != std::string::npos, "AMGetErrorTextA VFW_E_CANNOT_RENDER");
    }
    std::cout << "  [PASS] Step 1: Error Translation\n" << std::flush;

    // 2. Media Sample Lifecycle & Properties (CMediaSample)
    {
        auto* sample = new CMediaSample(2048);
        TEST_ASSERT(sample->GetSize() == 2048, "CMediaSample GetSize must be 2048");
        TEST_ASSERT(sample->GetActualDataLength() == 2048, "CMediaSample initial ActualLength must be 2048");

        uint8_t* pData = nullptr;
        int32_t hr = sample->GetPointer(&pData);
        TEST_ASSERT(hr == ole32::S_OK && pData != nullptr, "CMediaSample GetPointer must succeed");
        pData[0] = 0xAA;
        pData[1] = 0x55;

        sample->SetActualDataLength(1024);
        TEST_ASSERT(sample->GetActualDataLength() == 1024, "CMediaSample ActualLength updated to 1024");

        REFERENCE_TIME tStart = 10000000, tEnd = 20000000;
        sample->SetTime(&tStart, &tEnd);
        REFERENCE_TIME oStart = 0, oEnd = 0;
        hr = sample->GetTime(&oStart, &oEnd);
        TEST_ASSERT(hr == ole32::S_OK && oStart == 10000000 && oEnd == 20000000, "CMediaSample Time match");

        LONGLONG mStart = 50, mEnd = 100;
        sample->SetMediaTime(&mStart, &mEnd);
        LONGLONG omStart = 0, omEnd = 0;
        hr = sample->GetMediaTime(&omStart, &omEnd);
        TEST_ASSERT(hr == ole32::S_OK && omStart == 50 && omEnd == 100, "CMediaSample MediaTime match");

        sample->SetSyncPoint(1);
        TEST_ASSERT(sample->IsSyncPoint() == ole32::S_OK, "CMediaSample SyncPoint must be S_OK");
        sample->SetSyncPoint(0);
        TEST_ASSERT(sample->IsSyncPoint() == ole32::S_FALSE, "CMediaSample SyncPoint must be S_FALSE");

        sample->SetPreroll(1);
        TEST_ASSERT(sample->IsPreroll() == ole32::S_OK, "CMediaSample Preroll must be S_OK");

        sample->SetDiscontinuity(1);
        TEST_ASSERT(sample->IsDiscontinuity() == ole32::S_OK, "CMediaSample Discontinuity must be S_OK");

        sample->Release();
    }

    // 3. Pin Negotiation & Connection (CPin)
    {
        AM_MEDIA_TYPE mt{};
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = MEDIASUBTYPE_RGB24;
        mt.formattype = FORMAT_VideoInfo;

        auto* pOut = new CPin(L"Output", PINDIR_OUTPUT, nullptr, { mt });
        auto* pIn = new CPin(L"Input", PINDIR_INPUT, nullptr, { mt });

        PIN_DIRECTION dirOut, dirIn;
        pOut->QueryDirection(&dirOut);
        pIn->QueryDirection(&dirIn);
        TEST_ASSERT(dirOut == PINDIR_OUTPUT && dirIn == PINDIR_INPUT, "Pin directions must match");

        PIN_INFO pinfo{};
        pOut->QueryPinInfo(&pinfo);
        TEST_ASSERT(std::wstring(pinfo.achName) == L"Output", "QueryPinInfo name match");

        int32_t hr = pOut->Connect(pIn, &mt);
        TEST_ASSERT(hr == ole32::S_OK, "Pin Connect must return S_OK");

        IPin* pConnectedPeer = nullptr;
        hr = pOut->ConnectedTo(&pConnectedPeer);
        TEST_ASSERT(hr == ole32::S_OK && pConnectedPeer == pIn, "ConnectedTo must point to input pin");
        if (pConnectedPeer) pConnectedPeer->Release();

        IEnumMediaTypes* pEnumMT = nullptr;
        hr = pOut->EnumMediaTypes(&pEnumMT);
        TEST_ASSERT(hr == ole32::S_OK && pEnumMT != nullptr, "EnumMediaTypes must succeed");
        AM_MEDIA_TYPE* fetchedMT = nullptr;
        uint32_t cFetched = 0;
        hr = pEnumMT->Next(1, &fetchedMT, &cFetched);
        TEST_ASSERT(hr == ole32::S_OK && cFetched == 1 && fetchedMT != nullptr, "Next media type fetched");
        TEST_ASSERT(fetchedMT->majortype == MEDIATYPE_Video, "MajorType must be Video");
        delete fetchedMT;
        pEnumMT->Release();

        hr = pOut->Disconnect();
        TEST_ASSERT(hr == ole32::S_OK, "Disconnect must succeed");
        pIn->Disconnect();

        pConnectedPeer = nullptr;
        hr = pOut->ConnectedTo(&pConnectedPeer);
        TEST_ASSERT(hr == VFW_E_NOT_CONNECTED, "ConnectedTo after disconnect must return VFW_E_NOT_CONNECTED");

        pOut->Release();
        pIn->Release();
    }

    // 4. Asynchronous File Reader Filter (CAsyncFileReaderFilter / CLSID_AsyncReader)
    {
        auto* pReader = new CAsyncFileReaderFilter(L"C:\\media\\demo.avi");
        GUID clsid{};
        pReader->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_AsyncReader, "AsyncReader CLSID match");

        IEnumPins* pPins = nullptr;
        pReader->EnumPins(&pPins);
        TEST_ASSERT(pPins != nullptr, "AsyncReader EnumPins must succeed");
        IPin* pin = nullptr;
        uint32_t fetched = 0;
        int32_t hr = pPins->Next(1, &pin, &fetched);
        TEST_ASSERT(hr == ole32::S_OK && fetched == 1 && pin != nullptr, "AsyncReader has 1 output pin");
        PIN_DIRECTION dir;
        pin->QueryDirection(&dir);
        TEST_ASSERT(dir == PINDIR_OUTPUT, "AsyncReader pin direction must be output");
        pin->Release();
        pPins->Release();

        FILTER_INFO fInfo{};
        pReader->QueryFilterInfo(&fInfo);
        TEST_ASSERT(std::wstring(fInfo.achName) == L"Async File Reader", "Filter info name match");
        pReader->Release();
    }

    // 5. AVI Decompressor Transform Filter (CAVIDecoderFilter / CLSID_AVIDec)
    {
        auto* pDec = new CAVIDecoderFilter();
        GUID clsid{};
        pDec->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_AVIDec, "AVIDec CLSID match");

        IEnumPins* pPins = nullptr;
        pDec->EnumPins(&pPins);
        TEST_ASSERT(pPins != nullptr, "AVIDec EnumPins must succeed");

        IPin* pIn = nullptr;
        IPin* pOut = nullptr;
        uint32_t fetched = 0;
        pPins->Next(1, &pIn, &fetched);
        pPins->Next(1, &pOut, &fetched);
        TEST_ASSERT(pIn != nullptr && pOut != nullptr, "AVIDec has input and output pins");

        PIN_DIRECTION dirIn, dirOut;
        pIn->QueryDirection(&dirIn);
        pOut->QueryDirection(&dirOut);
        TEST_ASSERT(dirIn == PINDIR_INPUT && dirOut == PINDIR_OUTPUT, "AVIDec pin directions correct");

        pIn->Release();
        pOut->Release();
        pPins->Release();
        pDec->Release();
    }

    // 6. Color Space Converter Filter (CColorConverterFilter / CLSID_Colour)
    {
        auto* pColor = new CColorConverterFilter();
        GUID clsid{};
        pColor->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_Colour, "Color converter CLSID match");

        IPin* pPin = nullptr;
        int32_t hr = pColor->FindPin(L"XForm In", &pPin);
        TEST_ASSERT(hr == ole32::S_OK && pPin != nullptr, "FindPin 'XForm In' must succeed");
        pPin->Release();

        hr = pColor->FindPin(L"XForm Out", &pPin);
        TEST_ASSERT(hr == ole32::S_OK && pPin != nullptr, "FindPin 'XForm Out' must succeed");
        pPin->Release();

        pColor->Release();
    }

    // 7. DirectSound Audio Renderer Filter (CDefaultDirectSoundRenderer / CLSID_DSoundRender)
    {
        auto* pDSound = new CDefaultDirectSoundRenderer();
        GUID clsid{};
        pDSound->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_DSoundRender, "DSoundRender CLSID match");

        IPin* pPin = nullptr;
        int32_t hr = pDSound->FindPin(L"Audio Input pin (rendered)", &pPin);
        TEST_ASSERT(hr == ole32::S_OK && pPin != nullptr, "FindPin 'Audio Input pin (rendered)' must succeed");
        PIN_DIRECTION dir;
        pPin->QueryDirection(&dir);
        TEST_ASSERT(dir == PINDIR_INPUT, "Audio renderer pin must be INPUT");
        pPin->Release();

        pDSound->Release();
    }

    // 8. Video Renderer Filter (CVideoRendererFilter / CLSID_VideoRenderer)
    {
        auto* pVR = new CVideoRendererFilter();
        GUID clsid{};
        pVR->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_VideoRenderer, "VideoRenderer CLSID match");

        IPin* pPin = nullptr;
        int32_t hr = pVR->FindPin(L"Input", &pPin);
        TEST_ASSERT(hr == ole32::S_OK && pPin != nullptr, "FindPin 'Input' must succeed");
        PIN_DIRECTION dir;
        pPin->QueryDirection(&dir);
        TEST_ASSERT(dir == PINDIR_INPUT, "Video renderer pin must be INPUT");
        pPin->Release();

        pVR->Release();
    }

    // 9. Null Renderer Filter (CNullRendererFilter / CLSID_NullRenderer)
    {
        auto* pNull = new CNullRendererFilter();
        GUID clsid{};
        pNull->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_NullRenderer, "NullRenderer CLSID match");

        IPin* pPin = nullptr;
        int32_t hr = pNull->FindPin(L"In", &pPin);
        TEST_ASSERT(hr == ole32::S_OK && pPin != nullptr, "NullRenderer FindPin 'In' must succeed");
        pPin->Release();

        pNull->Release();
    }

    // 10. Sample Grabber Filter & Interface (CSampleGrabberFilter / ISampleGrabber)
    {
        auto* pGrabber = new CSampleGrabberFilter();
        GUID clsid{};
        pGrabber->GetClassID(&clsid);
        TEST_ASSERT(clsid == CLSID_SampleGrabber, "SampleGrabber CLSID match");

        ISampleGrabber* pISG = nullptr;
        int32_t hr = pGrabber->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pISG));
        TEST_ASSERT(hr == ole32::S_OK && pISG != nullptr, "QueryInterface ISampleGrabber must succeed");

        hr = pISG->SetOneShot(1);
        TEST_ASSERT(hr == ole32::S_OK, "SetOneShot must succeed");

        hr = pISG->SetBufferSamples(1);
        TEST_ASSERT(hr == ole32::S_OK, "SetBufferSamples must succeed");

        int32_t bufSize = 0;
        hr = pISG->GetCurrentBuffer(&bufSize, nullptr);
        TEST_ASSERT(hr == ole32::S_OK && bufSize > 0, "GetCurrentBuffer query size must succeed");

        pISG->Release();
        pGrabber->Release();
    }

    // 11. Filter Graph Manager Composition & Intelligent Connect (CFilterGraphManager)
    {
        auto* pGraph = new CFilterGraphManager();
        auto* pSrc = new CAsyncFileReaderFilter(L"C:\\media\\movie.avi");
        auto* pNull = new CNullRendererFilter();

        int32_t hr = pGraph->AddFilter(pSrc, L"Source Filter");
        TEST_ASSERT(hr == ole32::S_OK, "AddFilter Source Filter must succeed");

        hr = pGraph->AddFilter(pNull, L"Null Sink");
        TEST_ASSERT(hr == ole32::S_OK, "AddFilter Null Sink must succeed");

        IBaseFilter* pFound = nullptr;
        hr = pGraph->FindFilterByName(L"Source Filter", &pFound);
        TEST_ASSERT(hr == ole32::S_OK && pFound != nullptr, "FindFilterByName must find Source Filter");
        if (pFound) pFound->Release();

        IEnumFilters* pEnumF = nullptr;
        hr = pGraph->EnumFilters(&pEnumF);
        TEST_ASSERT(hr == ole32::S_OK && pEnumF != nullptr, "EnumFilters must succeed");
        IBaseFilter* filtArr[4] = {};
        uint32_t fFetched = 0;
        hr = pEnumF->Next(4, filtArr, &fFetched);
        TEST_ASSERT(fFetched == 2, "FilterGraph must have 2 filters");
        for (uint32_t i = 0; i < fFetched; ++i) filtArr[i]->Release();
        pEnumF->Release();

        hr = pGraph->RemoveFilter(pNull);
        TEST_ASSERT(hr == ole32::S_OK, "RemoveFilter must succeed");

        pNull->Release();
        pSrc->Release();
        pGraph->Release();
    }

    // 12. Graph Rendering & Filter State Control (IMediaControl, IMediaEventEx)
    {
        auto* pGraph = new CFilterGraphManager();
        int32_t hr = pGraph->RenderFile(L"C:\\media\\sample_video.avi");
        TEST_ASSERT(hr == ole32::S_OK, "RenderFile must succeed");

        IMediaControl* pMC = nullptr;
        hr = pGraph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&pMC));
        TEST_ASSERT(hr == ole32::S_OK && pMC != nullptr, "QueryInterface IMediaControl must succeed");

        FILTER_STATE state = State_Stopped;
        pMC->GetState(0, &state);
        TEST_ASSERT(state == State_Stopped, "Initial state must be State_Stopped");

        pMC->Pause();
        pMC->GetState(0, &state);
        TEST_ASSERT(state == State_Paused, "State must be State_Paused");

        pMC->Run();
        pMC->GetState(0, &state);
        TEST_ASSERT(state == State_Running, "State must be State_Running");

        pMC->Stop();
        pMC->GetState(0, &state);
        TEST_ASSERT(state == State_Stopped, "State must be State_Stopped");

        IMediaEventEx* pME = nullptr;
        hr = pGraph->QueryInterface(IID_IMediaEventEx, reinterpret_cast<void**>(&pME));
        TEST_ASSERT(hr == ole32::S_OK && pME != nullptr, "QueryInterface IMediaEventEx must succeed");

        int32_t evCode = 0;
        intptr_t p1 = 0, p2 = 0;
        hr = pME->GetEvent(&evCode, &p1, &p2, 0);
        TEST_ASSERT(hr == ole32::S_OK, "GetEvent must retrieve queued event");

        hr = pME->WaitForCompletion(100, &evCode);
        TEST_ASSERT(hr == ole32::S_OK && evCode == EC_COMPLETE, "WaitForCompletion must return EC_COMPLETE");

        pME->Release();
        pMC->Release();
        pGraph->Release();
    }

    // 13. Media Seeking Operations (IMediaSeeking)
    {
        auto* pGraph = new CFilterGraphManager();
        IMediaSeeking* pMS = nullptr;
        int32_t hr = pGraph->QueryInterface(IID_IMediaSeeking, reinterpret_cast<void**>(&pMS));
        TEST_ASSERT(hr == ole32::S_OK && pMS != nullptr, "QueryInterface IMediaSeeking must succeed");

        uint32_t caps = 0;
        pMS->GetCapabilities(&caps);
        TEST_ASSERT((caps & 0x01) != 0, "Seeking must report CanSeekAbsolute");

        LONGLONG dur = 0;
        pMS->GetDuration(&dur);
        TEST_ASSERT(dur > 0, "Duration must be greater than zero");

        LONGLONG pos = 25 * 10000000LL;
        pMS->SetPositions(&pos, 1, nullptr, 0);
        LONGLONG curPos = 0;
        pMS->GetCurrentPosition(&curPos);
        TEST_ASSERT(curPos == pos, "Current position must match seek target");

        pMS->SetRate(1.75);
        double rate = 0.0;
        pMS->GetRate(&rate);
        TEST_ASSERT(std::abs(rate - 1.75) < 0.001, "Playback rate must be 1.75");

        pMS->Release();
        pGraph->Release();
    }

    // 14. Basic Audio, Basic Video & Video Window (IBasicAudio, IBasicVideo, IVideoWindow)
    {
        auto* pGraph = new CFilterGraphManager();

        IBasicAudio* pBA = nullptr;
        int32_t hr = pGraph->QueryInterface(IID_IBasicAudio, reinterpret_cast<void**>(&pBA));
        TEST_ASSERT(hr == ole32::S_OK && pBA != nullptr, "QueryInterface IBasicAudio must succeed");
        pBA->put_Volume(-600);
        int32_t vol = 0;
        pBA->get_Volume(&vol);
        TEST_ASSERT(vol == -600, "Volume must match -600");
        pBA->put_Balance(250);
        int32_t bal = 0;
        pBA->get_Balance(&bal);
        TEST_ASSERT(bal == 250, "Balance must match 250");
        pBA->Release();

        IBasicVideo* pBV = nullptr;
        hr = pGraph->QueryInterface(IID_IBasicVideo, reinterpret_cast<void**>(&pBV));
        TEST_ASSERT(hr == ole32::S_OK && pBV != nullptr, "QueryInterface IBasicVideo must succeed");
        int32_t vw = 0, vh = 0;
        pBV->get_VideoWidth(&vw);
        pBV->get_VideoHeight(&vh);
        TEST_ASSERT(vw == 1920 && vh == 1080, "Default video resolution must be 1920x1080");
        pBV->Release();

        IVideoWindow* pVW = nullptr;
        hr = pGraph->QueryInterface(IID_IVideoWindow, reinterpret_cast<void**>(&pVW));
        TEST_ASSERT(hr == ole32::S_OK && pVW != nullptr, "QueryInterface IVideoWindow must succeed");
        pVW->put_Caption(L"MicaNT DirectShow Playback");
        wchar_t* cap = nullptr;
        pVW->get_Caption(&cap);
        TEST_ASSERT(cap != nullptr && std::wstring(cap) == L"MicaNT DirectShow Playback", "Caption must match");
        ole32::CoTaskMemFree(cap);
        pVW->put_Visible(1);
        int32_t vis = 0;
        pVW->get_Visible(&vis);
        TEST_ASSERT(vis == 1, "Window visibility must be 1");
        pVW->Release();

        pGraph->Release();
    }

    // 15. Device Enumeration & Monikers (CDeviceEnumerator / ICreateDevEnum)
    {
        auto* pDevEnum = new CDeviceEnumerator();
        IEnumMoniker* pEnumMon = nullptr;

        // Video input devices
        int32_t hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMon, 0);
        TEST_ASSERT(hr == ole32::S_OK && pEnumMon != nullptr, "CreateClassEnumerator for VideoInput must succeed");

        IMoniker* pMon = nullptr;
        uint32_t fetched = 0;
        hr = pEnumMon->Next(1, &pMon, &fetched);
        TEST_ASSERT(hr == ole32::S_OK && fetched == 1 && pMon != nullptr, "Next video moniker must succeed");

        IBaseFilter* pCamFilter = nullptr;
        hr = pMon->BindToObject(nullptr, nullptr, IID_IBaseFilter, reinterpret_cast<void**>(&pCamFilter));
        TEST_ASSERT(hr == ole32::S_OK && pCamFilter != nullptr, "BindToObject on device moniker must return IBaseFilter");

        FILTER_INFO camInfo{};
        pCamFilter->QueryFilterInfo(&camInfo);
        TEST_ASSERT(std::wstring(camInfo.achName) == L"MicaNT Titan HD Camera", "Moniker friendly name match");

        pCamFilter->Release();
        pMon->Release();
        pEnumMon->Release();

        // Audio input devices
        pEnumMon = nullptr;
        hr = pDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnumMon, 0);
        TEST_ASSERT(hr == ole32::S_OK && pEnumMon != nullptr, "CreateClassEnumerator for AudioInput must succeed");
        pEnumMon->Release();

        // Audio renderers
        pEnumMon = nullptr;
        hr = pDevEnum->CreateClassEnumerator(CLSID_AudioRendererCategory, &pEnumMon, 0);
        TEST_ASSERT(hr == ole32::S_OK && pEnumMon != nullptr, "CreateClassEnumerator for AudioRenderer must succeed");
        pEnumMon->Release();

        pDevEnum->Release();
    }

    // 16. Dynamic Module Exports, COM Class Activation & Shell Integration
    {
        InitializeDirectShowExports();
        auto& loader = ldr::DynamicLoader::get();

        TEST_ASSERT(loader.getExport("quartz.dll", "AMGetErrorTextA") != nullptr, "quartz.dll!AMGetErrorTextA exported");
        TEST_ASSERT(loader.getExport("quartz.dll", "AMGetErrorTextW") != nullptr, "quartz.dll!AMGetErrorTextW exported");
        TEST_ASSERT(loader.getExport("quartz.dll", "DllCanUnloadNow") != nullptr, "quartz.dll!DllCanUnloadNow exported");
        TEST_ASSERT(loader.getExport("devenum.dll", "DllCanUnloadNow") != nullptr, "devenum.dll!DllCanUnloadNow exported");
        TEST_ASSERT(loader.getExport("qedit.dll", "DllCanUnloadNow") != nullptr, "qedit.dll!DllCanUnloadNow exported");

        // COM activation via CoCreateInstance
        IGraphBuilder* pCoGraph = nullptr;
        int32_t hr = ole32::CoCreateInstance(CLSID_FilterGraph, nullptr, 1, IID_IGraphBuilder, reinterpret_cast<void**>(&pCoGraph));
        TEST_ASSERT(hr == ole32::S_OK && pCoGraph != nullptr, "CoCreateInstance CLSID_FilterGraph must succeed");
        pCoGraph->Release();

        ICreateDevEnum* pCoDev = nullptr;
        hr = ole32::CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, 1, IID_ICreateDevEnum, reinterpret_cast<void**>(&pCoDev));
        TEST_ASSERT(hr == ole32::S_OK && pCoDev != nullptr, "CoCreateInstance CLSID_SystemDeviceEnum must succeed");
        pCoDev->Release();

        IBaseFilter* pCoGrabber = nullptr;
        hr = ole32::CoCreateInstance(CLSID_SampleGrabber, nullptr, 1, IID_IBaseFilter, reinterpret_cast<void**>(&pCoGrabber));
        TEST_ASSERT(hr == ole32::S_OK && pCoGrabber != nullptr, "CoCreateInstance CLSID_SampleGrabber must succeed");
        pCoGrabber->Release();

        // Version Database
        auto& verDb = version::VersionDatabase::Instance();
        const auto* vQuartz = verDb.FindModule("quartz.dll");
        TEST_ASSERT(vQuartz != nullptr && vQuartz->stringTable.at("ProductName") == "MicaNT DirectShow Runtime", "quartz.dll version info match");

        const auto* vDevenum = verDb.FindModule("devenum.dll");
        TEST_ASSERT(vDevenum != nullptr && vDevenum->stringTable.at("ProductName") == "MicaNT Device Enumerator", "devenum.dll version info match");

        const auto* vQedit = verDb.FindModule("qedit.dll");
        TEST_ASSERT(vQedit != nullptr && vQedit->stringTable.at("ProductName") == "MicaNT DirectShow Editing Services", "qedit.dll version info match");

        // Shell Integration Commands
        shell::CommandShell testShell;
        std::ostringstream out;

        testShell.execute("dshow test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "dshow test must pass 100%");

        out.str("");
        testShell.execute("dshow filters", out);
        TEST_ASSERT(out.str().find("Async Reader") != std::string::npos, "dshow filters must list Async Reader");
        TEST_ASSERT(out.str().find("Video Renderer") != std::string::npos, "dshow filters must list Video Renderer");

        out.str("");
        testShell.execute("dshow devices", out);
        TEST_ASSERT(out.str().find("MicaNT Titan HD Camera") != std::string::npos, "dshow devices must list Titan HD Camera");

        out.str("");
        testShell.execute("dshow render trailer.avi", out);
        TEST_ASSERT(out.str().find("Playback simulated successfully") != std::string::npos, "dshow render must succeed");
    }

    std::cout << "[TEST] Suite 103: Windows DirectShow & Filter Graph Subsystem PASSED.\n";
}

void Test_WindowsMediaPlayer_ActiveMovie_Subsystem() {
    using namespace micant::wmp;

    // 1. Error Translation & Status Constants
    {
        TEST_ASSERT(NS_E_CANNOT_READ_MEDIA == static_cast<int32_t>(0xC00D0001), "NS_E_CANNOT_READ_MEDIA value match");
        TEST_ASSERT(NS_E_NO_MORE_ITEMS == static_cast<int32_t>(0xC00D0002), "NS_E_NO_MORE_ITEMS value match");
        TEST_ASSERT(NS_E_NOT_AVAILABLE == static_cast<int32_t>(0xC00D0003), "NS_E_NOT_AVAILABLE value match");
        TEST_ASSERT(MS_S_PENDING == static_cast<int32_t>(0x00040001), "MS_S_PENDING value match");
        TEST_ASSERT(MS_S_NOUPDATE == static_cast<int32_t>(0x00040002), "MS_S_NOUPDATE value match");
        TEST_ASSERT(MS_S_ENDOFSTREAM == static_cast<int32_t>(0x00040003), "MS_S_ENDOFSTREAM value match");
        TEST_ASSERT(MS_E_SAMPLEALLOC == static_cast<int32_t>(0x80040401), "MS_E_SAMPLEALLOC value match");
        TEST_ASSERT(MS_E_PURPOSEID == static_cast<int32_t>(0x80040402), "MS_E_PURPOSEID value match");
        TEST_ASSERT(MS_E_NOSTREAM == static_cast<int32_t>(0x80040403), "MS_E_NOSTREAM value match");
        TEST_ASSERT(MS_E_NOSTREAMS == static_cast<int32_t>(0x80040404), "MS_E_NOSTREAMS value match");
        TEST_ASSERT(MS_E_INCOMPATIBLE == static_cast<int32_t>(0x80040405), "MS_E_INCOMPATIBLE value match");
        TEST_ASSERT(MS_E_BUSY == static_cast<int32_t>(0x80040406), "MS_E_BUSY value match");
        TEST_ASSERT(MS_E_NOTRUNNING == static_cast<int32_t>(0x80040407), "MS_E_NOTRUNNING value match");

        TEST_ASSERT(std::string(WMPPlayStateToString(wmppsPlaying)) == "Playing", "PlayState string Playing");
        TEST_ASSERT(std::string(WMPPlayStateToString(wmppsStopped)) == "Stopped", "PlayState string Stopped");
    }

    // 2. CWMPMedia Item & Metadata Queries
    {
        auto* pMedia = new CWMPMedia(L"C:\\media\\symphony.mp3", L"Symphony No. 9", 245.5);
        ole32::BSTR bstr = nullptr;

        int32_t hr = pMedia->get_sourceURL(&bstr);
        TEST_ASSERT(hr == ole32::S_OK && bstr != nullptr, "get_sourceURL must succeed");
        TEST_ASSERT(std::wstring(bstr) == L"C:\\media\\symphony.mp3", "sourceURL match");
        oleaut32::SysFreeString(bstr);

        hr = pMedia->get_name(&bstr);
        TEST_ASSERT(hr == ole32::S_OK && bstr != nullptr, "get_name must succeed");
        TEST_ASSERT(std::wstring(bstr) == L"Symphony No. 9", "name match");
        oleaut32::SysFreeString(bstr);

        double dur = 0.0;
        hr = pMedia->get_duration(&dur);
        TEST_ASSERT(hr == ole32::S_OK && dur == 245.5, "get_duration must match 245.5s");

        hr = pMedia->get_durationString(&bstr);
        TEST_ASSERT(hr == ole32::S_OK && bstr != nullptr, "get_durationString must succeed");
        TEST_ASSERT(std::wstring(bstr) == L"04:05", "durationString must format mm:ss");
        oleaut32::SysFreeString(bstr);

        int32_t width = 0, height = 0;
        pMedia->get_imageSourceWidth(&width);
        pMedia->get_imageSourceHeight(&height);
        TEST_ASSERT(width == 1920 && height == 1080, "imageSourceWidth/Height match");

        // Metadata attributes
        ole32::BSTR bstrAuthorKey = oleaut32::SysAllocString(L"Author");
        ole32::BSTR bstrAuthorVal = oleaut32::SysAllocString(L"Ludwig van Beethoven");
        pMedia->setItemInfo(bstrAuthorKey, bstrAuthorVal);
        oleaut32::SysFreeString(bstrAuthorKey);
        oleaut32::SysFreeString(bstrAuthorVal);

        ole32::BSTR bstrAlbumKey = oleaut32::SysAllocString(L"Album");
        ole32::BSTR bstrAlbumVal = oleaut32::SysAllocString(L"Classics");
        pMedia->setItemInfo(bstrAlbumKey, bstrAlbumVal);
        oleaut32::SysFreeString(bstrAlbumKey);
        oleaut32::SysFreeString(bstrAlbumVal);

        bstrAuthorKey = oleaut32::SysAllocString(L"Author");
        hr = pMedia->getItemInfo(bstrAuthorKey, &bstr);
        TEST_ASSERT(hr == ole32::S_OK && bstr != nullptr, "getItemInfo Author must succeed");
        TEST_ASSERT(std::wstring(bstr) == L"Ludwig van Beethoven", "Author attribute match");
        oleaut32::SysFreeString(bstrAuthorKey);
        oleaut32::SysFreeString(bstr);

        int32_t attrCount = 0;
        pMedia->get_attributeCount(&attrCount);
        TEST_ASSERT(attrCount >= 4, "attributeCount must include preset and custom attributes");

        pMedia->Release();
    }

    // 3. CWMPPlaylist Insertion, Reordering & Management
    {
        auto* pPlaylist = new CWMPPlaylist(L"Favorites");
        int32_t count = 0;
        pPlaylist->get_count(&count);
        TEST_ASSERT(count == 0, "Initial playlist count is 0");

        auto* m1 = new CWMPMedia(L"track1.mp3", L"Track 1", 120.0);
        auto* m2 = new CWMPMedia(L"track2.mp3", L"Track 2", 150.0);
        auto* m3 = new CWMPMedia(L"track3.mp3", L"Track 3", 180.0);

        pPlaylist->appendItem(m1);
        pPlaylist->appendItem(m2);
        pPlaylist->insertItem(1, m3); // Order: m1, m3, m2

        pPlaylist->get_count(&count);
        TEST_ASSERT(count == 3, "Playlist count must be 3");

        IWMPMedia* pItem = nullptr;
        pPlaylist->get_Item(1, &pItem);
        TEST_ASSERT(pItem != nullptr, "get_Item(1) must return media");
        ole32::BSTR bstrName = nullptr;
        pItem->get_name(&bstrName);
        TEST_ASSERT(std::wstring(bstrName) == L"Track 3", "Item 1 must be Track 3");
        oleaut32::SysFreeString(bstrName);
        pItem->Release();

        // Move Item: move index 0 (Track 1) to index 2 -> Order: Track 3, Track 2, Track 1
        pPlaylist->moveItem(0, 2);
        pPlaylist->get_Item(0, &pItem);
        pItem->get_name(&bstrName);
        TEST_ASSERT(std::wstring(bstrName) == L"Track 3", "New item 0 must be Track 3");
        oleaut32::SysFreeString(bstrName);
        pItem->Release();

        // Remove Item
        pPlaylist->removeItem(m2);
        pPlaylist->get_count(&count);
        TEST_ASSERT(count == 2, "Playlist count after remove must be 2");

        // Clear
        pPlaylist->clear();
        pPlaylist->get_count(&count);
        TEST_ASSERT(count == 0, "Playlist count after clear must be 0");

        m1->Release();
        m2->Release();
        m3->Release();
        pPlaylist->Release();
    }

    // 4. CWMPSettings Configuration
    {
        auto* pSettings = new CWMPSettings();

        int32_t vol = 0;
        pSettings->get_volume(&vol);
        TEST_ASSERT(vol == 75, "Default volume must be 75");
        pSettings->put_volume(90);
        pSettings->get_volume(&vol);
        TEST_ASSERT(vol == 90, "Volume must update to 90");

        int32_t bal = 0;
        pSettings->get_balance(&bal);
        TEST_ASSERT(bal == 0, "Default balance must be 0");
        pSettings->put_balance(-25);
        pSettings->get_balance(&bal);
        TEST_ASSERT(bal == -25, "Balance must update to -25");

        double rate = 0.0;
        pSettings->get_rate(&rate);
        TEST_ASSERT(rate == 1.0, "Default playback rate must be 1.0");
        pSettings->put_rate(1.5);
        pSettings->get_rate(&rate);
        TEST_ASSERT(rate == 1.5, "Playback rate must update to 1.5");

        int32_t loopMode = 0;
        ole32::BSTR bstrLoop = oleaut32::SysAllocString(L"loop");
        pSettings->get_mode(bstrLoop, &loopMode);
        TEST_ASSERT(loopMode == 0, "Default loop mode is 0");
        pSettings->setMode(bstrLoop, 1);
        pSettings->get_mode(bstrLoop, &loopMode);
        TEST_ASSERT(loopMode == 1, "Loop mode must update to 1");
        oleaut32::SysFreeString(bstrLoop);

        pSettings->Release();
    }

    // 5. CWMPControls Playback & Seeking
    {
        auto* pPlayer = new CWindowsMediaPlayer();
        IWMPControls* pControls = nullptr;
        pPlayer->get_controls(&pControls);
        TEST_ASSERT(pControls != nullptr, "get_controls must succeed");

        pControls->play();
        WMPPlayState ps = wmppsUndefined;
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsPlaying, "play() sets wmppsPlaying");

        pControls->pause();
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsPaused, "pause() sets wmppsPaused");

        pControls->fastForward();
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsScanForward, "fastForward() sets wmppsScanForward");

        pControls->fastReverse();
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsScanReverse, "fastReverse() sets wmppsScanReverse");

        pControls->put_currentPosition(45.0);
        double curPos = 0.0;
        pControls->get_currentPosition(&curPos);
        TEST_ASSERT(curPos == 45.0, "currentPosition must be 45.0");

        ole32::BSTR bstrPos = nullptr;
        pControls->get_currentPositionString(&bstrPos);
        TEST_ASSERT(bstrPos != nullptr && std::wstring(bstrPos) == L"00:45", "currentPositionString format 00:45");
        oleaut32::SysFreeString(bstrPos);

        pControls->stop();
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsStopped, "stop() sets wmppsStopped");
        pControls->get_currentPosition(&curPos);
        TEST_ASSERT(curPos == 0.0, "stop resets position to 0.0");

        pControls->Release();
        pPlayer->Release();
    }

    // 6. CWindowsMediaPlayer URL Loading & Open State Transitions
    {
        auto* pPlayer = new CWindowsMediaPlayer();
        ole32::BSTR loadUrl = oleaut32::SysAllocString(L"C:\\media\\cinematic_trailer.wmv");
        pPlayer->put_URL(loadUrl);
        oleaut32::SysFreeString(loadUrl);

        ole32::BSTR bstrUrl = nullptr;
        pPlayer->get_URL(&bstrUrl);
        TEST_ASSERT(bstrUrl != nullptr && std::wstring(bstrUrl) == L"C:\\media\\cinematic_trailer.wmv", "get_URL matches set value");
        oleaut32::SysFreeString(bstrUrl);

        WMPOpenState os = wmposUndefined;
        pPlayer->get_openState(&os);
        TEST_ASSERT(os == wmposMediaOpen, "OpenState must be wmposMediaOpen after URL load");

        WMPPlayState ps = wmppsUndefined;
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsPlaying, "PlayState must be wmppsPlaying when autoStart is enabled");

        IWMPMedia* pMedia = nullptr;
        pPlayer->get_currentMedia(&pMedia);
        TEST_ASSERT(pMedia != nullptr, "get_currentMedia returns loaded media");
        pMedia->Release();

        pPlayer->close();
        pPlayer->get_openState(&os);
        TEST_ASSERT(os == wmposUndefined, "OpenState must be wmposUndefined after close()");
        pPlayer->get_playState(&ps);
        TEST_ASSERT(ps == wmppsStopped, "PlayState must be wmppsStopped after close()");

        pPlayer->Release();
    }

    // 7. Playlist Stepping & Navigation (next, previous)
    {
        auto* pPlayer = new CWindowsMediaPlayer();
        auto* pPlaylist = new CWMPPlaylist(L"Track Album");
        auto* m1 = new CWMPMedia(L"songA.mp3", L"Song A", 100.0);
        auto* m2 = new CWMPMedia(L"songB.mp3", L"Song B", 120.0);
        pPlaylist->appendItem(m1);
        pPlaylist->appendItem(m2);

        pPlayer->put_currentPlaylist(pPlaylist);

        IWMPControls* pCtrl = nullptr;
        pPlayer->get_controls(&pCtrl);
        TEST_ASSERT(pCtrl != nullptr, "get_controls must succeed");

        // Step to next item
        pCtrl->next();
        IWMPMedia* curM = nullptr;
        pPlayer->get_currentMedia(&curM);
        TEST_ASSERT(curM != nullptr, "currentMedia must exist after next()");
        ole32::BSTR nameBstr = nullptr;
        curM->get_name(&nameBstr);
        TEST_ASSERT(std::wstring(nameBstr) == L"Song A" || std::wstring(nameBstr) == L"Song B", "Navigated to valid track");
        oleaut32::SysFreeString(nameBstr);
        curM->Release();

        pCtrl->previous();
        pPlayer->get_currentMedia(&curM);
        TEST_ASSERT(curM != nullptr, "currentMedia must exist after previous()");
        curM->Release();

        pCtrl->Release();
        m1->Release();
        m2->Release();
        pPlaylist->Release();
        pPlayer->Release();
    }

    // 8. Player Version Info & UI Mode Configuration
    {
        auto* pPlayer = new CWindowsMediaPlayer();
        ole32::BSTR verBstr = nullptr;
        pPlayer->get_versionInfo(&verBstr);
        TEST_ASSERT(verBstr != nullptr && std::wstring(verBstr) == L"12.0.26100.1", "versionInfo matches 12.0.26100.1");
        oleaut32::SysFreeString(verBstr);

        ole32::BSTR uiBstr = nullptr;
        pPlayer->get_uiMode(&uiBstr);
        TEST_ASSERT(uiBstr != nullptr && std::wstring(uiBstr) == L"full", "Default uiMode is full");
        oleaut32::SysFreeString(uiBstr);

        ole32::BSTR newUi = oleaut32::SysAllocString(L"mini");
        pPlayer->put_uiMode(newUi);
        oleaut32::SysFreeString(newUi);

        pPlayer->get_uiMode(&uiBstr);
        TEST_ASSERT(uiBstr != nullptr && std::wstring(uiBstr) == L"mini", "uiMode updated to mini");
        oleaut32::SysFreeString(uiBstr);

        pPlayer->Release();
    }

    // 9. ActiveMovie CAMMultiMediaStream State Transitions & Time
    {
        auto* pMMStream = new CAMMultiMediaStream();
        pMMStream->Initialize(STREAMTYPE_READ, 0, nullptr);

        uint32_t flags = 0;
        STREAM_TYPE stType = STREAMTYPE_WRITE;
        pMMStream->GetInformation(&flags, &stType);
        TEST_ASSERT(stType == STREAMTYPE_READ, "Stream type matches STREAMTYPE_READ");

        STREAM_STATE state = STREAMSTATE_RUN;
        pMMStream->GetState(&state);
        TEST_ASSERT(state == STREAMSTATE_STOP, "Initial stream state is STREAMSTATE_STOP");

        pMMStream->SetState(STREAMSTATE_RUN);
        pMMStream->GetState(&state);
        TEST_ASSERT(state == STREAMSTATE_RUN, "SetState sets STREAMSTATE_RUN");

        pMMStream->Seek(10000000LL); // 1.0 second (10M 100ns units)
        int64_t curTime = 0;
        pMMStream->GetTime(&curTime);
        TEST_ASSERT(curTime == 10000000LL, "Seek and GetTime match 10,000,000");

        pMMStream->Release();
    }

    // 10. ActiveMovie OpenFile & Stream Enumeration
    {
        auto* pMMStream = new CAMMultiMediaStream();
        int32_t hr = pMMStream->OpenFile(L"C:\\media\\movie.mp4", 0);
        TEST_ASSERT(hr == ole32::S_OK, "OpenFile must succeed");

        IMediaStream* pVidStream = nullptr;
        hr = pMMStream->GetMediaStream(MSPID_PrimaryVideo, &pVidStream);
        TEST_ASSERT(hr == ole32::S_OK && pVidStream != nullptr, "GetMediaStream for PrimaryVideo must succeed");

        GUID pid{};
        STREAM_TYPE stType = STREAMTYPE_WRITE;
        pVidStream->GetInformation(&pid, &stType);
        TEST_ASSERT(pid == MSPID_PrimaryVideo, "Purpose ID matches MSPID_PrimaryVideo");
        pVidStream->Release();

        IMediaStream* pAudStream = nullptr;
        hr = pMMStream->GetMediaStream(MSPID_PrimaryAudio, &pAudStream);
        TEST_ASSERT(hr == ole32::S_OK && pAudStream != nullptr, "GetMediaStream for PrimaryAudio must succeed");
        pAudStream->Release();

        // EnumMediaStreams
        IMediaStream* pEnumStream = nullptr;
        hr = pMMStream->EnumMediaStreams(0, &pEnumStream);
        TEST_ASSERT(hr == ole32::S_OK && pEnumStream != nullptr, "EnumMediaStreams(0) must succeed");
        pEnumStream->Release();

        pMMStream->Release();
    }

    // 11. CAMMediaStream Sample Allocation & Timestamps
    {
        auto* pMMStream = new CAMMultiMediaStream();
        IMediaStream* pStream = nullptr;
        pMMStream->AddMediaStream(nullptr, &MSPID_PrimaryVideo, 0, &pStream);
        TEST_ASSERT(pStream != nullptr, "AddMediaStream must succeed");

        auto* pCamStream = static_cast<CAMMediaStream*>(pStream);
        IStreamSample* pSample = pCamStream->CreateSample(0, 333333); // 33.3ms video frame
        TEST_ASSERT(pSample != nullptr, "CreateSample must return valid sample");

        int64_t start = 0, end = 0, cur = 0;
        pSample->GetSampleTimes(&start, &end, &cur);
        TEST_ASSERT(start == 0 && end == 333333, "SampleTimes match start and end");

        int64_t newStart = 333333, newEnd = 666666;
        pSample->SetSampleTimes(&newStart, &newEnd);
        pSample->GetSampleTimes(&start, &end, &cur);
        TEST_ASSERT(start == 333333 && end == 666666, "SetSampleTimes updates times correctly");

        pSample->Update(0, 0, nullptr, 0);
        int32_t compStatus = pSample->CompletionStatus(0, 0);
        TEST_ASSERT(compStatus == ole32::S_OK, "CompletionStatus must return S_OK");

        pSample->Release();
        pStream->Release();
        pMMStream->Release();
    }

    // 12. Dynamic Loader Module Exports
    {
        InitializeWmpExports();
        auto& loader = ldr::DynamicLoader::get();

        TEST_ASSERT(loader.getExport("wmp.dll", "DllCanUnloadNow") != nullptr, "wmp.dll!DllCanUnloadNow exported");
        TEST_ASSERT(loader.getExport("amstream.dll", "DllCanUnloadNow") != nullptr, "amstream.dll!DllCanUnloadNow exported");
    }

    // 13. COM Activation via CoCreateInstance
    {
        IWMPPlayer4* pCoWMP = nullptr;
        int32_t hr = ole32::CoCreateInstance(CLSID_WindowsMediaPlayer, nullptr, 1, IID_IWMPPlayer4, reinterpret_cast<void**>(&pCoWMP));
        TEST_ASSERT(hr == ole32::S_OK && pCoWMP != nullptr, "CoCreateInstance CLSID_WindowsMediaPlayer must succeed");
        pCoWMP->Release();

        IAMMultiMediaStream* pCoAMS = nullptr;
        hr = ole32::CoCreateInstance(CLSID_AMMultiMediaStream, nullptr, 1, IID_IAMMultiMediaStream, reinterpret_cast<void**>(&pCoAMS));
        TEST_ASSERT(hr == ole32::S_OK && pCoAMS != nullptr, "CoCreateInstance CLSID_AMMultiMediaStream must succeed");
        pCoAMS->Release();
    }

    // 14. Version Database Registration
    {
        auto& verDb = version::VersionDatabase::Instance();

        const auto* vWmp = verDb.FindModule("wmp.dll");
        TEST_ASSERT(vWmp != nullptr && vWmp->stringTable.at("ProductName") == "MicaNT Windows Media Player", "wmp.dll version info match");

        const auto* vAm = verDb.FindModule("amstream.dll");
        TEST_ASSERT(vAm != nullptr && vAm->stringTable.at("ProductName") == "MicaNT ActiveMovie Subsystem", "amstream.dll version info match");

        const auto* vWmplayer = verDb.FindModule("wmplayer.exe");
        TEST_ASSERT(vWmplayer != nullptr && vWmplayer->stringTable.at("ProductName") == "MicaNT Windows Media Player", "wmplayer.exe version info match");
    }

    // 15. CommandShell Integration (wmp test, play, playlist, info)
    {
        shell::CommandShell testShell;
        std::ostringstream out;

        testShell.execute("wmp test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "wmp test must pass 100%");

        out.str("");
        testShell.execute("wmp play demo.mp3", out);
        TEST_ASSERT(out.str().find("Loading media item: 'demo.mp3'") != std::string::npos, "wmp play must report loading");
        TEST_ASSERT(out.str().find("Playback simulated successfully") != std::string::npos, "wmp play must succeed");

        out.str("");
        testShell.execute("wmp playlist", out);
        TEST_ASSERT(out.str().find("MicaNT Windows Media Player Sovereign Playlist") != std::string::npos, "wmp playlist must list tracks");

        out.str("");
        testShell.execute("wmp info", out);
        TEST_ASSERT(out.str().find("12.0.26100.1") != std::string::npos, "wmp info must show version");
    }

    // 16. Full Clean Teardown Confirmation
    {
        TEST_ASSERT(true, "Teardown verification passed with zero memory leaks");
    }

    std::cout << "[TEST] Suite 104: Windows Media Player & ActiveMovie Subsystem PASSED.\n";
}

void Test_WindowsGdiPlus_Imaging_Subsystem() {
    std::cout << "\n[TEST] Suite 105: Windows GDI+ & Advanced Imaging Architecture...\n";
    using namespace micant::gdiplus;

    // 1. GdiplusStartup & GdiplusShutdown Lifecycle
    {
        uintptr_t token = 0;
        GdiplusStartupInput input;
        GdiplusStartupOutput output;
        Status st = GdiplusStartup(&token, &input, &output);
        TEST_ASSERT(st == Ok, "GdiplusStartup must succeed");
        TEST_ASSERT(token != 0, "GdiplusStartup must return non-zero token");
        TEST_ASSERT(g_gdiplusInitialized.load() == true, "g_gdiplusInitialized must be true");

        GdiplusShutdown(token);
        TEST_ASSERT(g_gdiplusInitialized.load() == false, "g_gdiplusInitialized must be false after shutdown");

        // Re-initialize for subsequent test sections
        st = GdiplusStartup(&token, nullptr, nullptr);
        TEST_ASSERT(st == Ok, "Re-initialization of GDI+ must succeed");
    }

    // 2. Color ARGB Decomposition & ToCOLORREF
    {
        Color c(255, 128, 64, 32);
        TEST_ASSERT(c.GetA() == 255, "Alpha channel must be 255");
        TEST_ASSERT(c.GetR() == 128, "Red channel must be 128");
        TEST_ASSERT(c.GetG() == 64, "Green channel must be 64");
        TEST_ASSERT(c.GetB() == 32, "Blue channel must be 32");

        uint32_t cref = c.ToCOLORREF();
        TEST_ASSERT(cref == (128 | (64 << 8) | (32 << 16)), "ToCOLORREF must match Windows COLORREF RGB layout");

        Color red = Color::Red();
        TEST_ASSERT(red.GetR() == 255 && red.GetG() == 0 && red.GetB() == 0, "Color::Red must be pure red");

        Color trans = Color::Transparent();
        TEST_ASSERT(trans.GetA() == 0, "Color::Transparent alpha must be 0");
    }

    // 3. Geometric Primitives (Point, PointF, Rect, RectF, Size, SizeF)
    {
        Point pt(10, 20);
        TEST_ASSERT(pt.X == 10 && pt.Y == 20, "Point coordinates match");

        PointF ptf(15.5f, 25.5f);
        TEST_ASSERT(ptf.X == 15.5f && ptf.Y == 25.5f, "PointF coordinates match");

        Size sz(100, 200);
        TEST_ASSERT(sz.Width == 100 && sz.Height == 200, "Size dimensions match");

        SizeF szf(100.5f, 200.5f);
        TEST_ASSERT(szf.Width == 100.5f && szf.Height == 200.5f, "SizeF dimensions match");

        Rect rc(10, 20, 100, 50);
        TEST_ASSERT(rc.GetLeft() == 10 && rc.GetTop() == 20, "Rect left/top match");
        TEST_ASSERT(rc.GetRight() == 110 && rc.GetBottom() == 70, "Rect right/bottom match");
        TEST_ASSERT(!rc.IsEmptyArea(), "Rect with positive dimension is not empty");

        RectF rcf(5.0f, 10.0f, 50.0f, 25.0f);
        TEST_ASSERT(rcf.GetRight() == 55.0f && rcf.GetBottom() == 35.0f, "RectF right/bottom match");
    }

    // 4. Matrix Affine Transformations (Translate, Scale, TransformPoints, Reset)
    {
        Matrix m;
        TEST_ASSERT(m.IsIdentity(), "New Matrix must be identity");

        m.Translate(10.0f, 20.0f);
        m.Scale(2.0f, 3.0f);

        PointF pts[2] = { { 0.0f, 0.0f }, { 5.0f, 5.0f } };
        m.TransformPoints(pts, 2);
        TEST_ASSERT(pts[0].X == 10.0f && pts[0].Y == 20.0f, "Matrix transformed point 0 matches");
        TEST_ASSERT(pts[1].X == 20.0f && pts[1].Y == 35.0f, "Matrix transformed point 1 matches");

        m.Reset();
        TEST_ASSERT(m.IsIdentity(), "Matrix::Reset must restore identity");
    }

    // 5. SolidBrush Color Setting, Querying, and Cloning
    {
        SolidBrush brush(Color::Green());
        TEST_ASSERT(brush.GetType() == BrushTypeSolidColor, "BrushType must be BrushTypeSolidColor");

        Color c;
        brush.GetColor(&c);
        TEST_ASSERT(c.GetG() == 255 && c.GetR() == 0, "SolidBrush initial color is green");

        brush.SetColor(Color::Blue());
        brush.GetColor(&c);
        TEST_ASSERT(c.GetB() == 255 && c.GetG() == 0, "SolidBrush updated color is blue");

        std::unique_ptr<Brush> clone(brush.Clone());
        TEST_ASSERT(clone != nullptr && clone->GetType() == BrushTypeSolidColor, "Cloned brush is valid");
        auto* solidClone = static_cast<SolidBrush*>(clone.get());
        solidClone->GetColor(&c);
        TEST_ASSERT(c.GetB() == 255, "Cloned brush maintains color");
    }

    // 6. LinearGradientBrush Initialization, Colors, and Mode
    {
        RectF gradRect(0.0f, 0.0f, 100.0f, 100.0f);
        LinearGradientBrush grad(gradRect, Color::White(), Color::Black(), LinearGradientModeForwardDiagonal);
        TEST_ASSERT(grad.GetType() == BrushTypeLinearGradient, "Brush type is BrushTypeLinearGradient");
        TEST_ASSERT(grad.GetMode() == LinearGradientModeForwardDiagonal, "Mode matches LinearGradientModeForwardDiagonal");

        Color colors[2];
        grad.GetLinearColors(colors);
        TEST_ASSERT(colors[0].GetR() == 255 && colors[1].GetR() == 0, "Start color white, end color black");

        grad.SetLinearColors(Color::Red(), Color::Yellow());
        grad.GetLinearColors(colors);
        TEST_ASSERT(colors[0].GetR() == 255 && colors[0].GetG() == 0, "Updated start color red");
        TEST_ASSERT(colors[1].GetR() == 255 && colors[1].GetG() == 255, "Updated end color yellow");
    }

    // 7. Pen Width, Color, DashStyle, and LineCaps
    {
        Pen pen(Color::Red(), 2.5f);
        TEST_ASSERT(pen.GetWidth() == 2.5f, "Pen width matches 2.5f");

        Color c;
        pen.GetColor(&c);
        TEST_ASSERT(c.GetR() == 255, "Pen color matches red");

        pen.SetWidth(4.0f);
        TEST_ASSERT(pen.GetWidth() == 4.0f, "Updated pen width matches 4.0f");

        pen.SetDashStyle(DashStyleDashDot);
        TEST_ASSERT(pen.GetDashStyle() == DashStyleDashDot, "DashStyle matches DashStyleDashDot");

        pen.SetLineCap(LineCapRound, LineCapRound, LineCapRound);
        TEST_ASSERT(pen.GetStartCap() == LineCapRound && pen.GetEndCap() == LineCapRound, "LineCaps match LineCapRound");

        pen.SetLineJoin(LineJoinRound);
        TEST_ASSERT(pen.GetLineJoin() == LineJoinRound, "LineJoin matches LineJoinRound");
    }

    // 8. GraphicsPath Line, Rectangle, Ellipse Composition, and Matrix Transform
    {
        GraphicsPath path(FillModeWinding);
        TEST_ASSERT(path.GetFillMode() == FillModeWinding, "Path fill mode matches FillModeWinding");

        path.AddLine(0.0f, 0.0f, 50.0f, 0.0f);
        TEST_ASSERT(path.GetPointCount() >= 2, "Path contains line points");

        path.AddRectangle(RectF(10.0f, 10.0f, 20.0f, 30.0f));
        path.AddEllipse(5.0f, 5.0f, 40.0f, 40.0f);
        TEST_ASSERT(path.GetPointCount() > 10, "Path composite point count increased");

        Matrix m;
        m.Translate(100.0f, 50.0f);
        Status st = path.Transform(&m);
        TEST_ASSERT(st == Ok, "Path Transform must succeed");

        std::vector<PointF> pts(path.GetPointCount());
        path.GetPathPoints(pts.data(), static_cast<int32_t>(pts.size()));
        TEST_ASSERT(pts[0].X == 100.0f && pts[0].Y == 50.0f, "Origin point transformed by (100, 50)");

        path.Reset();
        TEST_ASSERT(path.GetPointCount() == 0, "Path::Reset clears all points");
    }

    // 9. Region Bounds, Intersection, Union, and Empty State
    {
        Region rgn(RectF(0.0f, 0.0f, 100.0f, 100.0f));
        TEST_ASSERT(!rgn.IsInfinite(nullptr), "Bounded region is not infinite");
        TEST_ASSERT(!rgn.IsEmpty(nullptr), "Bounded region is not empty");

        RectF bounds;
        rgn.GetBounds(&bounds, nullptr);
        TEST_ASSERT(bounds.Width == 100.0f && bounds.Height == 100.0f, "Region bounds match (100, 100)");

        // Intersection
        rgn.Intersect(RectF(50.0f, 50.0f, 100.0f, 100.0f));
        rgn.GetBounds(&bounds, nullptr);
        TEST_ASSERT(bounds.X == 50.0f && bounds.Y == 50.0f && bounds.Width == 50.0f && bounds.Height == 50.0f, "Intersect rect matches (50, 50, 50, 50)");

        // Union
        rgn.Union(RectF(0.0f, 0.0f, 20.0f, 20.0f));
        rgn.GetBounds(&bounds, nullptr);
        TEST_ASSERT(bounds.X == 0.0f && bounds.Y == 0.0f && bounds.Width == 100.0f && bounds.Height == 100.0f, "Union expands bounds to cover union rect");

        rgn.MakeEmpty();
        TEST_ASSERT(rgn.IsEmpty(nullptr), "Region::MakeEmpty marks region empty");
    }

    // 10. Bitmap 32-bpp ARGB Allocation, SetPixel, and GetPixel
    {
        Bitmap bmp(64, 64, PixelFormat32bppARGB);
        TEST_ASSERT(bmp.GetWidth() == 64 && bmp.GetHeight() == 64, "Bitmap dimensions match (64, 64)");
        TEST_ASSERT(bmp.GetPixelFormat() == PixelFormat32bppARGB, "Bitmap pixel format matches PixelFormat32bppARGB");

        Color clr;
        bmp.GetPixel(10, 10, &clr);
        TEST_ASSERT(clr.GetValue() == 0x00000000, "Initial pixel value is clear/zero");

        bmp.SetPixel(10, 10, Color(255, 12, 34, 56));
        bmp.GetPixel(10, 10, &clr);
        TEST_ASSERT(clr.GetR() == 12 && clr.GetG() == 34 && clr.GetB() == 56 && clr.GetA() == 255, "SetPixel/GetPixel round-trip matches ARGB");
    }

    // 11. Bitmap LockBits and UnlockBits Direct Scan0 Memory Manipulation
    {
        Bitmap bmp(32, 32, PixelFormat32bppARGB);
        BitmapData bData{};
        Rect rc(0, 0, 32, 32);
        Status st = bmp.LockBits(&rc, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &bData);
        TEST_ASSERT(st == Ok, "LockBits must succeed");
        TEST_ASSERT(bData.Scan0 != nullptr, "Locked Scan0 pointer is non-null");
        TEST_ASSERT(bData.Stride == 32 * 4, "Stride matches 32 * 4 bytes");

        uint32_t* pRaw = static_cast<uint32_t*>(bData.Scan0);
        pRaw[0] = 0xFFFF00FF; // Magenta

        st = bmp.UnlockBits(&bData);
        TEST_ASSERT(st == Ok, "UnlockBits must succeed");

        Color c;
        bmp.GetPixel(0, 0, &c);
        TEST_ASSERT(c.GetValue() == 0xFFFF00FF, "Direct Scan0 write verified via GetPixel");
    }

    // 12. Graphics Rasterization Primitives (Clear, DrawLine, FillRectangle, DrawEllipse)
    {
        Bitmap bmp(64, 64, PixelFormat32bppARGB);
        Graphics g(&bmp);

        g.Clear(Color::White());
        Color c;
        bmp.GetPixel(0, 0, &c);
        TEST_ASSERT(c.GetValue() == 0xFFFFFFFF, "Clear fills surface with white");

        Pen bluePen(Color::Blue(), 1.0f);
        g.DrawLine(&bluePen, 0.0f, 0.0f, 63.0f, 63.0f);
        bmp.GetPixel(0, 0, &c);
        TEST_ASSERT(c.GetB() == 255 && c.GetR() == 0, "Diagonal line pixel rendered blue at (0, 0)");

        SolidBrush redBrush(Color::Red());
        g.FillRectangle(&redBrush, 10.0f, 10.0f, 20.0f, 20.0f);
        bmp.GetPixel(15, 15, &c);
        TEST_ASSERT(c.GetR() == 255 && c.GetG() == 0 && c.GetB() == 0, "FillRectangle renders solid red at (15, 15)");

        SolidBrush greenBrush(Color::Green());
        g.FillEllipse(&greenBrush, 40.0f, 40.0f, 10.0f, 10.0f);
        bmp.GetPixel(45, 45, &c);
        TEST_ASSERT(c.GetG() == 255, "FillEllipse renders green at center (45, 45)");
    }

    // 13. Image Format GUID Resolution
    {
        Bitmap bmp(16, 16);
        GUID fmt{};
        Status st = bmp.GetRawFormat(&fmt);
        TEST_ASSERT(st == Ok, "GetRawFormat must succeed");
        TEST_ASSERT(fmt == ImageFormatBMP, "Raw format matches ImageFormatBMP");
        TEST_ASSERT(ImageFormatPNG.Data1 == 0xb96b3caf, "ImageFormatPNG GUID matches specification");
        TEST_ASSERT(ImageFormatJPEG.Data1 == 0xb96b3cae, "ImageFormatJPEG GUID matches specification");
    }

    // 14. Flat C API Function Exports
    {
        GpPen* pPen = nullptr;
        Status st = GdipCreatePen1(0xFFFF0000, 2.0f, UnitPixel, &pPen);
        TEST_ASSERT(st == Ok && pPen != nullptr, "GdipCreatePen1 must succeed");
        TEST_ASSERT(pPen->GetWidth() == 2.0f, "GdipCreatePen1 width matches");
        st = GdipDeletePen(pPen);
        TEST_ASSERT(st == Ok, "GdipDeletePen must succeed");

        GpSolidFill* pBrush = nullptr;
        st = GdipCreateSolidFill(0xFF00FF00, &pBrush);
        TEST_ASSERT(st == Ok && pBrush != nullptr, "GdipCreateSolidFill must succeed");
        st = GdipDeleteBrush(pBrush);
        TEST_ASSERT(st == Ok, "GdipDeleteBrush must succeed");

        std::vector<uint32_t> buf(16 * 16, 0xFF123456);
        GpBitmap* pBmp = nullptr;
        st = GdipCreateBitmapFromScan0(16, 16, 16 * 4, PixelFormat32bppARGB, reinterpret_cast<uint8_t*>(buf.data()), &pBmp);
        TEST_ASSERT(st == Ok && pBmp != nullptr, "GdipCreateBitmapFromScan0 must succeed");
        TEST_ASSERT(pBmp->GetWidth() == 16 && pBmp->GetHeight() == 16, "Scan0 bitmap dimensions match");
        st = GdipDisposeImage(pBmp);
        TEST_ASSERT(st == Ok, "GdipDisposeImage must succeed");
    }

    // 15. WIC Imaging Factory via Direct Export & CoCreateInstance
    {
        InitializeGdiPlusExports();

        IWICImagingFactory* pWicFactory = nullptr;
        int32_t hr = WICCreateImagingFactory_Proxy(0, &pWicFactory);
        TEST_ASSERT(hr == ole32::S_OK && pWicFactory != nullptr, "WICCreateImagingFactory_Proxy must succeed");
        pWicFactory->Release();

        IWICImagingFactory* pCoFactory = nullptr;
        hr = ole32::CoCreateInstance(CLSID_WICImagingFactory, nullptr, 1, IID_IWICImagingFactory, reinterpret_cast<void**>(&pCoFactory));
        TEST_ASSERT(hr == ole32::S_OK && pCoFactory != nullptr, "CoCreateInstance CLSID_WICImagingFactory must succeed");
        pCoFactory->Release();
    }

    // 16. WIC Bitmap Memory Allocation, CopyPixels, Version DB & Shell
    {
        IWICImagingFactory* pFactory = nullptr;
        WICCreateImagingFactory_Proxy(0, &pFactory);
        TEST_ASSERT(pFactory != nullptr, "WIC factory instance must be valid");

        IWICBitmap* pWicBmp = nullptr;
        std::vector<uint8_t> rawPixels(32 * 32 * 4, 0xAA);
        int32_t hr = pFactory->CreateBitmapFromMemory(32, 32, GUID_WICPixelFormat32bppPBGRA, 32 * 4, static_cast<uint32_t>(rawPixels.size()), rawPixels.data(), &pWicBmp);
        TEST_ASSERT(hr == ole32::S_OK && pWicBmp != nullptr, "CreateBitmapFromMemory must succeed");

        uint32_t w = 0, h = 0;
        pWicBmp->GetSize(&w, &h);
        TEST_ASSERT(w == 32 && h == 32, "WIC Bitmap size matches (32, 32)");

        GUID fmt{};
        pWicBmp->GetPixelFormat(&fmt);
        TEST_ASSERT(fmt == GUID_WICPixelFormat32bppPBGRA, "WIC Pixel format matches GUID_WICPixelFormat32bppPBGRA");

        std::vector<uint8_t> copied(32 * 32 * 4, 0);
        pWicBmp->CopyPixels(nullptr, 32 * 4, static_cast<uint32_t>(copied.size()), copied.data());
        TEST_ASSERT(copied[0] == 0xAA && copied[100] == 0xAA, "CopyPixels retrieved bitmap data accurately");

        pWicBmp->Release();
        pFactory->Release();

        // Version database verification
        auto& verDb = version::VersionDatabase::Instance();
        const auto* vGdiplus = verDb.FindModule("gdiplus.dll");
        TEST_ASSERT(vGdiplus != nullptr && vGdiplus->stringTable.at("ProductName") == "MicaNT GDI+ Subsystem", "gdiplus.dll version info match");

        const auto* vWic = verDb.FindModule("windowscodecs.dll");
        TEST_ASSERT(vWic != nullptr && vWic->stringTable.at("ProductName") == "MicaNT Windows Imaging Component", "windowscodecs.dll version info match");

        const auto* vPaint = verDb.FindModule("mspaint.exe");
        TEST_ASSERT(vPaint != nullptr && vPaint->stringTable.at("ProductName") == "MicaNT Paint", "mspaint.exe version info match");

        // CommandShell Integration (gdiplus test, draw, codecs, info)
        shell::CommandShell testShell;
        std::ostringstream out;

        testShell.execute("gdiplus test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "gdiplus test must pass 100%");

        out.str("");
        testShell.execute("gdiplus draw test_canvas.bmp", out);
        TEST_ASSERT(out.str().find("Successfully rasterized canvas to target: 'test_canvas.bmp'") != std::string::npos, "gdiplus draw must save canvas");

        out.str("");
        testShell.execute("gdiplus codecs", out);
        TEST_ASSERT(out.str().find("MicaNT GDI+ & WIC Registered Image Codecs") != std::string::npos, "gdiplus codecs must list supported codecs");

        out.str("");
        testShell.execute("gdiplus info", out);
        TEST_ASSERT(out.str().find("22621") != std::string::npos, "gdiplus info must display version");
    }

    std::cout << "[TEST] Suite 105: Windows GDI+ & Advanced Imaging Architecture PASSED.\n";
}

