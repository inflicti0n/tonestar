#include "App/DiscordPresence.h"
#include "App/AppLog.h"

#include <cstddef>
#include <cstring>

#if JUCE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#ifndef TONESTAR_DISCORD_APP_ID
#define TONESTAR_DISCORD_APP_ID "1538926105107628102"
#endif

namespace
{
constexpr int kMaxPipes = 10;
constexpr size_t kMaxFrame = 64 * 1024;
constexpr juce::uint32 kReconnectMs = 2000;
constexpr juce::uint32 kThrottleMs = 2000;
constexpr juce::uint32 kReadyFallbackMs = 1000;
constexpr float kPullFloor = 0.08f;
constexpr int kBloomFx = 4;

int pairKey(int a, int b)
{
    return a < b ? a * 8 + b : b * 8 + a;
}

const char* guitarVibe(int best, int second, bool hasPair)
{
    if (hasPair)
    {
        switch (pairKey(best, second))
        {
            case 2 * 8 + 3: return "Metal";
            case 0 * 8 + 5: return "Jazzy Tunes";
            case 1 * 8 + 4: return "Cuts Through";
            case 2 * 8 + 4: return "In the Mix";
            case 0 * 8 + 3: return "Bedroom";
            case 1 * 8 + 5: return "Vintage";
            case 2 * 8 + 5: return "Thick";
            case 1 * 8 + 3: return "Classic Grind";
            case 3 * 8 + 4: return "Radio Ready";
            case 0 * 8 + 4: return "Sparkle";
            case 0 * 8 + 1: return "Almost Clean";
            case 1 * 8 + 2: return "High Gain";
            default: break;
        }
    }

    static constexpr const char* singles[] = { "Clean", "Crunch", "Metal", "Tight", "Forward", "Warm" };
    return singles[juce::jlimit(0, 5, best)];
}

const char* vocalVibe(int best, int second, bool hasPair)
{
    if (hasPair)
    {
        switch (pairKey(best, second))
        {
            case 1 * 8 + 2: return "Dry Pop";
            case 0 * 8 + 3: return "Rock Voice";
            case 0 * 8 + 2: return "Scream";
            case 1 * 8 + 4: return "Soul";
            case 1 * 8 + 3: return "Radio Pop";
            case 0 * 8 + 4: return "Bark";
            case 2 * 8 + 3: return "Close Mic";
            default: break;
        }
    }

    static constexpr const char* singles[] = { "Grit", "In Your Face", "Dry", "Forward", "Warm" };
    return singles[juce::jlimit(0, 4, best)];
}

const char* guitarFxVibe(int index, bool shimmer)
{
    if (index == kBloomFx && shimmer)
        return "Shimmer";
    static constexpr const char* names[] = { "Sticky", "Wah", "Octaves", "Echoes", "Reverb", "Doubled", "Swirl", "Trem" };
    return names[juce::jlimit(0, 7, index)];
}

const char* vocalFxVibe(int index)
{
    static constexpr const char* names[] = { "Tuned", "Doubled", "Echoes", "Reverb", "Harmony", "Telephone" };
    return names[juce::jlimit(0, 5, index)];
}

int currentPid()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

std::uint32_t readU32(const std::uint8_t* p)
{
    return (std::uint32_t) p[0]
         | ((std::uint32_t) p[1] << 8)
         | ((std::uint32_t) p[2] << 16)
         | ((std::uint32_t) p[3] << 24);
}

void writeU32(std::uint8_t* p, std::uint32_t value)
{
    p[0] = (std::uint8_t) (value);
    p[1] = (std::uint8_t) (value >> 8);
    p[2] = (std::uint8_t) (value >> 16);
    p[3] = (std::uint8_t) (value >> 24);
}
}

DiscordPresence::DiscordPresence() = default;

DiscordPresence::~DiscordPresence()
{
    clear();
}

void DiscordPresence::start()
{
    started = true;
    startedAt = juce::Time::currentTimeMillis() / 1000;
    lastConnectTryMs = 0;
    tryConnect();
}

void DiscordPresence::refresh(const Snapshot& snap, bool shouldFlushNow)
{
    update(makeActivity(snap), shouldFlushNow);
}

DiscordPresence::Activity DiscordPresence::makeActivity(const Snapshot& snap) const
{
    const bool vocals = snap.mode == RigMode::Vocals;
    const int count = juce::jlimit(1, 6, snap.axisCount);

    int best = 0;
    int second = -1;
    float peak = snap.axes[0];
    float nextPeak = -1.0f;
    for (int i = 1; i < count; ++i)
    {
        const float value = snap.axes[(size_t) i];
        if (value > peak)
        {
            nextPeak = peak;
            second = best;
            peak = value;
            best = i;
        }
        else if (value > nextPeak)
        {
            nextPeak = value;
            second = i;
        }
    }

    const bool hasPair = peak >= kPullFloor && second >= 0 && nextPeak >= kPullFloor;
    juce::String details = vocals ? "Open Mic" : "Open Amp";
    if (peak >= kPullFloor)
        details = vocals ? vocalVibe(best, second, hasPair) : guitarVibe(best, second, hasPair);

    juce::StringArray extras;
    const int fxCount = juce::jlimit(0, 8, snap.fxCount);
    int fxBest = 0;
    float fxPeak = fxCount > 0 ? snap.fx[0] : 0.0f;
    for (int i = 1; i < fxCount; ++i)
    {
        if (snap.fx[(size_t) i] > fxPeak)
        {
            fxPeak = snap.fx[(size_t) i];
            fxBest = i;
        }
    }
    if (fxCount > 0 && fxPeak >= kPullFloor)
        extras.add(vocals ? vocalFxVibe(fxBest) : guitarFxVibe(fxBest, snap.shimmer));
    if (vocals && snap.keyLabel.isNotEmpty())
        extras.add(snap.keyLabel);
    if (snap.recording)
        extras.add("Tape");

    Activity activity;
    activity.mode = snap.mode;
    activity.details = details;
    activity.state = extras.joinIntoString(" · ");
    activity.smallImage = vocals ? "star5" : "star6";
    activity.smallText = vocals ? "Vocals" : "Guitar";
    return activity;
}

void DiscordPresence::update(const Activity& next, bool shouldFlushNow)
{
    pending = next;
    dirty = true;
    if (shouldFlushNow)
        immediate = true;
    flushIfDue();
}

void DiscordPresence::tick()
{
    if (! started)
        return;

    if (connected)
    {
        readAvailable();
        if (connected && ! ready
            && juce::Time::getMillisecondCounter() - connectedAtMs >= kReadyFallbackMs)
        {
            ready = true;
        }
    }
    else
    {
        tryConnect();
    }

    flushIfDue();
}

void DiscordPresence::clear()
{
    if (connected && ready)
        sendActivity(pending, true);

    closePipe();
    started = false;
    dirty = false;
    immediate = false;
}

void DiscordPresence::tryConnect()
{
    if (connected)
        return;

    const auto now = juce::Time::getMillisecondCounter();
    if (lastConnectTryMs != 0 && now - lastConnectTryMs < kReconnectMs)
        return;
    lastConnectTryMs = now;

#if JUCE_WINDOWS
    for (int i = 0; i < kMaxPipes; ++i)
    {
        const juce::String path = "\\\\.\\pipe\\discord-ipc-" + juce::String(i);
        HANDLE handle = CreateFileA(path.toRawUTF8(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    OPEN_EXISTING,
                                    0,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            continue;

        pipe = (void*) handle;
        connected = true;
        break;
    }
#else
    auto tryPath = [this] (const juce::String& path) -> bool
    {
        const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0)
            return false;

        sockaddr_un addr {};
        addr.sun_family = AF_UNIX;
        const auto utf8 = path.toRawUTF8();
        if (std::strlen(utf8) >= sizeof(addr.sun_path))
        {
            ::close(sock);
            return false;
        }
        std::strncpy(addr.sun_path, utf8, sizeof(addr.sun_path) - 1);

        if (::connect(sock, (sockaddr*) &addr, sizeof(addr)) != 0)
        {
            ::close(sock);
            return false;
        }

        const int flags = ::fcntl(sock, F_GETFL, 0);
        if (flags >= 0)
            ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        fd = sock;
        connected = true;
        return true;
    };

    const juce::String runtime = juce::SystemStats::getEnvironmentVariable("XDG_RUNTIME_DIR", {});
    const juce::String tmp = juce::SystemStats::getEnvironmentVariable("TMPDIR", "/tmp");

    for (int i = 0; i < kMaxPipes && ! connected; ++i)
    {
        const juce::String name = "discord-ipc-" + juce::String(i);
        if (runtime.isNotEmpty() && tryPath(runtime + "/" + name))
            break;
        if (runtime.isNotEmpty() && tryPath(runtime + "/app/com.discordapp.Discord/" + name))
            break;
        if (tryPath(tmp + "/" + name))
            break;
        if (tryPath(juce::String("/tmp/") + name))
            break;
    }
#endif

    if (! connected)
    {
        if (! loggedOffline)
        {
            loggedOffline = true;
            AppLog::note("discord ipc offline");
        }
        return;
    }

    incoming.clear();
    ready = false;
    connectedAtMs = juce::Time::getMillisecondCounter();

    juce::String handshake;
    handshake << "{\"v\":1,\"client_id\":\"" << TONESTAR_DISCORD_APP_ID << "\"}";
    if (! sendFrame(Opcode::Handshake, handshake))
    {
        closePipe();
        return;
    }

    AppLog::note("discord ipc connected");
}

void DiscordPresence::closePipe()
{
#if JUCE_WINDOWS
    if (pipe != nullptr)
    {
        CloseHandle((HANDLE) pipe);
        pipe = nullptr;
    }
#else
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
#endif
    connected = false;
    ready = false;
    incoming.clear();
}

bool DiscordPresence::sendFrame(Opcode opcode, const juce::String& json)
{
    const auto* utf8 = json.toRawUTF8();
    const auto len = (std::uint32_t) std::strlen(utf8);
    if (len > kMaxFrame)
        return false;

    std::vector<std::uint8_t> buf(8 + (size_t) len);
    writeU32(buf.data(), (std::uint32_t) opcode);
    writeU32(buf.data() + 4, len);
    if (len > 0)
        std::memcpy(buf.data() + 8, utf8, (size_t) len);

    if (! writeBytes(buf.data(), buf.size()))
    {
        closePipe();
        return false;
    }
    return true;
}

bool DiscordPresence::writeBytes(const void* data, size_t bytes)
{
    auto* p = static_cast<const std::uint8_t*>(data);
    size_t left = bytes;

#if JUCE_WINDOWS
    if (pipe == nullptr)
        return false;

    while (left > 0)
    {
        DWORD written = 0;
        if (! WriteFile((HANDLE) pipe, p, (DWORD) left, &written, nullptr) || written == 0)
            return false;
        p += written;
        left -= (size_t) written;
    }
    return true;
#else
    if (fd < 0)
        return false;

    while (left > 0)
    {
        const ssize_t n = ::send(fd, p, left, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;
        p += (size_t) n;
        left -= (size_t) n;
    }
    return true;
#endif
}

void DiscordPresence::readAvailable()
{
#if JUCE_WINDOWS
    if (pipe == nullptr)
        return;

    DWORD avail = 0;
    if (! PeekNamedPipe((HANDLE) pipe, nullptr, 0, nullptr, &avail, nullptr))
    {
        closePipe();
        return;
    }
    if (avail == 0)
        return;

    const size_t at = incoming.size();
    incoming.resize(at + (size_t) avail);
    DWORD got = 0;
    if (! ReadFile((HANDLE) pipe, incoming.data() + at, avail, &got, nullptr))
    {
        closePipe();
        return;
    }
    incoming.resize(at + (size_t) got);
#else
    if (fd < 0)
        return;

    std::uint8_t chunk[4096];
    for (;;)
    {
        const ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n > 0)
        {
            incoming.insert(incoming.end(), chunk, chunk + n);
            continue;
        }
        if (n == 0)
        {
            closePipe();
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        closePipe();
        return;
    }
#endif

    while (incoming.size() >= 8)
    {
        const auto opcode = (Opcode) readU32(incoming.data());
        const auto length = readU32(incoming.data() + 4);
        if (length > kMaxFrame)
        {
            closePipe();
            return;
        }
        if (incoming.size() < 8 + (size_t) length)
            break;

        const juce::String json = juce::String::fromUTF8(
            (const char*) incoming.data() + 8, (int) length);
        incoming.erase(incoming.begin(), incoming.begin() + (std::ptrdiff_t) (8 + (size_t) length));
        handleFrame(opcode, json);
        if (! connected)
            return;
    }
}

void DiscordPresence::handleFrame(Opcode opcode, const juce::String& json)
{
    if (opcode == Opcode::Close)
    {
        closePipe();
        return;
    }

    if (opcode == Opcode::Ping)
    {
        sendFrame(Opcode::Pong, json);
        return;
    }

    if (opcode == Opcode::Frame && ! ready)
    {
        ready = true;
        if (! loggedReady)
        {
            loggedReady = true;
            AppLog::note("discord ipc ready");
        }
    }
}

void DiscordPresence::flushIfDue()
{
    if (! dirty || ! connected)
        return;

    if (! ready)
        return;

    const auto now = juce::Time::getMillisecondCounter();
    if (! immediate && lastSendMs != 0 && now - lastSendMs < kThrottleMs)
        return;

    if (sendActivity(pending, false))
    {
        lastSendMs = now;
        dirty = false;
        immediate = false;
    }
}

bool DiscordPresence::sendActivity(const Activity& activity, bool empty)
{
    juce::DynamicObject::Ptr args = new juce::DynamicObject();
    args->setProperty("pid", currentPid());
    if (empty)
    {
        args->setProperty("activity", juce::var());
    }
    else
    {
        juce::DynamicObject::Ptr act = new juce::DynamicObject();
        act->setProperty("type", 0);
        act->setProperty("details", activity.details);
        act->setProperty("state", activity.state);

        juce::DynamicObject::Ptr timestamps = new juce::DynamicObject();
        timestamps->setProperty("start", startedAt);
        act->setProperty("timestamps", juce::var(timestamps.get()));

        juce::DynamicObject::Ptr assets = new juce::DynamicObject();
        assets->setProperty("large_image", "logo");
        assets->setProperty("large_text", "ToneStar");
        assets->setProperty("small_image", activity.smallImage);
        assets->setProperty("small_text", activity.smallText);
        act->setProperty("assets", juce::var(assets.get()));

        args->setProperty("activity", juce::var(act.get()));
    }

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("nonce", juce::String(nonce++));
    root->setProperty("cmd", "SET_ACTIVITY");
    root->setProperty("args", juce::var(args.get()));
    return sendFrame(Opcode::Frame, juce::JSON::toString(juce::var(root.get()), true));
}
