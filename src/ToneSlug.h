#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

struct ToneSlug
{
    static constexpr float defaultCabSize = 0.15f;
    static constexpr float defaultCabBack = 0.25f;

    struct Patch
    {
        std::array<float, 6> axes {};
        std::array<float, 8> fx {};
        bool bloomShimmer = false;
        bool binaural = false;
        float cabSize = defaultCabSize;
        float cabBack = defaultCabBack;
    };

    static juce::String encode(const std::array<float, 6>& axes)
    {
        return encode(Patch { axes, {}, false, false, defaultCabSize, defaultCabBack });
    }

    static juce::String encode(const Patch& patch)
    {
        juce::uint8 bytes[18] {};
        for (int i = 0; i < 6; ++i)
            bytes[i] = toByte(patch.axes[(size_t) i]);
        for (int i = 0; i < 8; ++i)
            bytes[6 + i] = toByte(patch.fx[(size_t) i]);
        bytes[14] = (juce::uint8) ((patch.bloomShimmer ? 1 : 0) | (patch.binaural ? 2 : 0));
        bytes[15] = toByte(patch.cabSize);
        bytes[16] = toByte(patch.cabBack);
        bytes[17] = 0;
        scramble(bytes, 18);
        return encodeAlphabet(bytes, 18);
    }

    static bool decode(const juce::String& slug, std::array<float, 6>& axes)
    {
        Patch patch;
        if (! decode(slug, patch))
            return false;

        axes = patch.axes;
        return true;
    }

    static bool decode(const juce::String& slug, Patch& patch)
    {
        patch = {};
        const auto text = slug.trim();
        const int length = text.length();
        if (length == 8)
            return decodeLegacyV1(text, patch);
        if (length == 20)
        {
            if (text.containsChar('_'))
                return decodeLegacyV2(text, patch);

            juce::uint8 bytes[15] {};
            if (! decodeAlphabet(text, bytes, 15))
                return false;

            scramble(bytes, 15);
            readPatchV2(bytes, patch);
            return true;
        }
        if (length != 24)
            return false;

        juce::uint8 bytes[18] {};
        if (! decodeAlphabet(text, bytes, 18))
            return false;

        scramble(bytes, 18);
        readPatchV3(bytes, patch);
        return true;
    }

private:
    static constexpr const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789*-";

    static constexpr juce::uint8 mask[18] = {
        0xA7, 0x3C, 0x91, 0x5E, 0x2B, 0xD4,
        0x68, 0xF1, 0x0C, 0xB9, 0x47, 0xE2,
        0x1A, 0x73, 0x8D, 0xC4, 0x5A, 0x2E
    };

    static juce::uint8 toByte(float value)
    {
        return (juce::uint8) juce::jlimit(0, 255, (int) std::lround(value * 255.0f));
    }

    static float fromByte(juce::uint8 value)
    {
        return (float) value / 255.0f;
    }

    static void scramble(juce::uint8* bytes, int size)
    {
        for (int i = 0; i < size; ++i)
            bytes[i] = (juce::uint8) (bytes[i] ^ mask[i]);
    }

    static void readPatchV2(const juce::uint8* bytes, Patch& patch)
    {
        for (int i = 0; i < 6; ++i)
            patch.axes[(size_t) i] = fromByte(bytes[i]);
        for (int i = 0; i < 8; ++i)
            patch.fx[(size_t) i] = fromByte(bytes[6 + i]);
        patch.bloomShimmer = (bytes[14] & 1) != 0;
        patch.binaural = false;
        patch.cabSize = defaultCabSize;
        patch.cabBack = defaultCabBack;
    }

    static void readPatchV3(const juce::uint8* bytes, Patch& patch)
    {
        readPatchV2(bytes, patch);
        patch.binaural = (bytes[14] & 2) != 0;
        patch.cabSize = fromByte(bytes[15]);
        patch.cabBack = fromByte(bytes[16]);
    }

    static int alphabetIndex(juce::juce_wchar c)
    {
        if (c >= 'A' && c <= 'Z')
            return (int) (c - 'A');
        if (c >= 'a' && c <= 'z')
            return 26 + (int) (c - 'a');
        if (c >= '0' && c <= '9')
            return 52 + (int) (c - '0');
        if (c == '*')
            return 62;
        if (c == '-')
            return 63;
        return -1;
    }

    static juce::String encodeAlphabet(const juce::uint8* bytes, int size)
    {
        juce::String out;
        out.preallocateBytes((size_t) (size * 4 / 3 + 1));

        for (int i = 0; i < size; i += 3)
        {
            const int n = ((int) bytes[i] << 16) | ((int) bytes[i + 1] << 8) | (int) bytes[i + 2];
            out += alphabet[(n >> 18) & 63];
            out += alphabet[(n >> 12) & 63];
            out += alphabet[(n >> 6) & 63];
            out += alphabet[n & 63];
        }

        return out;
    }

    static bool decodeAlphabet(const juce::String& text, juce::uint8* bytes, int size)
    {
        if (text.length() != size * 4 / 3)
            return false;

        int written = 0;
        for (int i = 0; i < text.length(); i += 4)
        {
            const int a = alphabetIndex(text[i]);
            const int b = alphabetIndex(text[i + 1]);
            const int c = alphabetIndex(text[i + 2]);
            const int d = alphabetIndex(text[i + 3]);
            if (a < 0 || b < 0 || c < 0 || d < 0)
                return false;

            const int n = (a << 18) | (b << 12) | (c << 6) | d;
            bytes[written++] = (juce::uint8) ((n >> 16) & 255);
            bytes[written++] = (juce::uint8) ((n >> 8) & 255);
            bytes[written++] = (juce::uint8) (n & 255);
        }

        return written == size;
    }

    static bool decodeLegacyV1(const juce::String& slug, Patch& patch)
    {
        auto text = slug.replaceCharacter('-', '+').replaceCharacter('_', '/');
        while (text.length() % 4 != 0)
            text += "=";

        juce::MemoryOutputStream out;
        if (! juce::Base64::convertFromBase64(out, text) || out.getDataSize() != 6)
            return false;

        const auto* bytes = static_cast<const juce::uint8*>(out.getData());
        for (int i = 0; i < 6; ++i)
            patch.axes[(size_t) i] = fromByte(bytes[i]);
        return true;
    }

    static bool decodeLegacyV2(const juce::String& slug, Patch& patch)
    {
        auto text = slug.replaceCharacter('-', '+').replaceCharacter('_', '/');
        while (text.length() % 4 != 0)
            text += "=";

        juce::MemoryOutputStream out;
        if (! juce::Base64::convertFromBase64(out, text) || out.getDataSize() != 15)
            return false;

        readPatchV2(static_cast<const juce::uint8*>(out.getData()), patch);
        return true;
    }
};
