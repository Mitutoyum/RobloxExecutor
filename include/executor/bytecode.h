#ifndef EXTERNAL_LUA_BYTECODE_HPP
#define EXTERNAL_LUA_BYTECODE_HPP
#define XXH_INLINE_ALL

#include <vector>
#include <string>

#include "zstd/zstd.h"
#include "luacode.h"
#include "xxhash/xxhash.h"

#include "Luau/BytecodeBuilder.h"
#include "Luau/BytecodeUtils.h"

extern "C" {
#include "blake3/blake3.h"
};

class BytecodeEncoder : public Luau::BytecodeEncoder {
    inline void encode(uint32_t* data, size_t count) override {
        for (auto i = 0u; i < count;) {
            auto& opcode = *reinterpret_cast<uint8_t*>(data + i);
            i += Luau::getOpLength(LuauOpcode(opcode));
            opcode *= 227;
        }
    }
};

class Bytecode {
private:
    static constexpr uint8_t BYTECODE_SIGNATURE[4] = { 'R', 'S', 'B', '1' };
    static constexpr uint8_t BYTECODE_HASH_MULTIPLIER = 41;
    static constexpr uint32_t BYTECODE_HASH_SEED = 42u;

    static constexpr uint32_t MAGIC_A = 0x4C464F52;
    static constexpr uint32_t MAGIC_B = 0x946AC432;
    static constexpr uint8_t  KEY_BYTES[4] = { 0x52, 0x4F, 0x46, 0x4C };

    static inline uint8_t rotl8(uint8_t value, int shift) {
        shift &= 7;
        return (value << shift) | (value >> (8 - shift));
    }

public:
    static std::string compress(const std::string& BytecodeData)
    {
        const auto MaxSize = ZSTD_compressBound(BytecodeData.size());
        auto Buffer = std::vector<char>(MaxSize + 8);

        memcpy(&Buffer[0], BYTECODE_SIGNATURE, 4);

        const auto Size = static_cast<uint32_t>(BytecodeData.size());
        memcpy(&Buffer[4], &Size, sizeof(Size));

        const auto compressed_size = ZSTD_compress(&Buffer[8], MaxSize, BytecodeData.data(), BytecodeData.size(), ZSTD_maxCLevel());
        if (ZSTD_isError(compressed_size))
            return "";

        const auto FinalSize = compressed_size + 8;
        Buffer.resize(FinalSize);

        const auto HashKey = XXH32(Buffer.data(), FinalSize, BYTECODE_HASH_SEED);
        const auto Bytes = reinterpret_cast<const uint8_t*>(&HashKey);

        for (auto i = 0u; i < FinalSize; ++i)
            Buffer[i] ^= (Bytes[i % 4] + i * BYTECODE_HASH_MULTIPLIER) & 0xFF;

        return std::string(Buffer.data(), FinalSize);
    }

    static std::string decompress(const std::string& compressed)
    {
        if (compressed.size() < 8)
            return "";

        auto CompressedData = std::vector<char>(compressed.begin(), compressed.end());
        auto HeaderBuffer = std::vector<uint8_t>(4);

        for (auto i = 0u; i < 4; ++i) {
            HeaderBuffer[i] = CompressedData[i] ^ BYTECODE_SIGNATURE[i];
            HeaderBuffer[i] = (HeaderBuffer[i] - i * BYTECODE_HASH_MULTIPLIER);
        }

        for (auto i = 0u; i < CompressedData.size(); ++i) {
            const auto XorValue = (HeaderBuffer[i % 4] + i * BYTECODE_HASH_MULTIPLIER);
            CompressedData[i] ^= XorValue;
        }

        const auto HashValue = *reinterpret_cast<const uint32_t*>(HeaderBuffer.data());
        const auto Rehash = XXH32(CompressedData.data(), CompressedData.size(), BYTECODE_HASH_SEED);
        if (Rehash != HashValue)
            return "";

        const auto DecompressedSize = *reinterpret_cast<const uint32_t*>(&CompressedData[4]);

        auto Decompressed = std::string(DecompressedSize, '\0');
        const auto ActualSize = ZSTD_decompress(&Decompressed[0], DecompressedSize, &CompressedData[8], CompressedData.size() - 8);

        if (ZSTD_isError(ActualSize) || ActualSize != DecompressedSize)
            return "";

        return Decompressed;
    }

    static std::string sign_bytecode(const std::string& bytecode) {
        if (bytecode.empty()) {
            return "";
        }

        constexpr uint32_t FOOTER_SIZE = 40u;

        std::vector<uint8_t> blake3_hash(32);
        {
            blake3_hasher hasher;
            blake3_hasher_init(&hasher);
            blake3_hasher_update(&hasher, bytecode.data(), bytecode.size());
            blake3_hasher_finalize(&hasher, blake3_hash.data(), blake3_hash.size());
        }

        std::vector<uint8_t> transformed_hash(32);

        for (int i = 0; i < 32; ++i) {
            uint8_t byte = KEY_BYTES[i & 3];
            uint8_t hash_byte = blake3_hash[i];
            uint8_t combined = byte + i;
            uint8_t result;

            switch (i & 3) {
            case 0: {
                int shift = ((combined & 3) + 1);
                result = rotl8(hash_byte ^ ~byte, shift);
                break;
            }
            case 1: {
                int shift = ((combined & 3) + 2);
                result = rotl8(byte ^ ~hash_byte, shift);
                break;
            }
            case 2: {
                int shift = ((combined & 3) + 3);
                result = rotl8(hash_byte ^ ~byte, shift);
                break;
            }
            case 3: {
                int shift = ((combined & 3) + 4);
                result = rotl8(byte ^ ~hash_byte, shift);
                break;
            }
            }
            transformed_hash[i] = result;
        }

        std::vector<uint8_t> footer(FOOTER_SIZE, 0);

        uint32_t first_hash_dword = *reinterpret_cast<uint32_t*>(transformed_hash.data());
        uint32_t footer_prefix = first_hash_dword ^ MAGIC_B;
        memcpy(&footer[0], &footer_prefix, 4);

        uint32_t xor_ed = first_hash_dword ^ MAGIC_A;
        memcpy(&footer[4], &xor_ed, 4);

        memcpy(&footer[8], transformed_hash.data(), 32);

        std::string signed_bytecode = bytecode;
        signed_bytecode.append(reinterpret_cast<const char*>(footer.data()), footer.size());

        return compress(signed_bytecode);
    }
};

#endif