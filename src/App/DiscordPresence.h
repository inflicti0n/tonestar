#pragma once

#include "App/RigMode.h"

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <vector>

class DiscordPresence
{
public:
    struct Snapshot
    {
        RigMode mode = RigMode::Guitar;
        std::array<float, 6> axes {};
        int axisCount = 6;
        std::array<float, 8> fx {};
        int fxCount = 8;
        bool shimmer = false;
        juce::String keyLabel;
        bool recording = false;
    };

    DiscordPresence();
    ~DiscordPresence();

    void start();
    void refresh(const Snapshot& snap, bool immediate);
    void tick();
    void clear();

private:
    struct Activity
    {
        RigMode mode = RigMode::Guitar;
        juce::String details;
        juce::String state;
        juce::String smallImage;
        juce::String smallText;
    };

    void update(const Activity& next, bool immediate);
    Activity makeActivity(const Snapshot& snap) const;
    enum class Opcode : std::uint32_t
    {
        Handshake = 0,
        Frame = 1,
        Close = 2,
        Ping = 3,
        Pong = 4
    };

    void tryConnect();
    void closePipe();
    bool sendFrame(Opcode opcode, const juce::String& json);
    bool writeBytes(const void* data, size_t bytes);
    void readAvailable();
    void handleFrame(Opcode opcode, const juce::String& json);
    void flushIfDue();
    bool sendActivity(const Activity& activity, bool empty);

#if JUCE_WINDOWS
    void* pipe = nullptr;
#else
    int fd = -1;
#endif

    Activity pending;
    juce::int64 startedAt = 0;
    juce::uint32 lastSendMs = 0;
    juce::uint32 lastConnectTryMs = 0;
    juce::uint32 connectedAtMs = 0;
    int nonce = 1;
    bool started = false;
    bool connected = false;
    bool ready = false;
    bool dirty = false;
    bool immediate = false;
    bool loggedOffline = false;
    bool loggedReady = false;
    std::vector<std::uint8_t> incoming;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiscordPresence)
};
