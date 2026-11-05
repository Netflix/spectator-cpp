#pragma once

#include "histogram.h"
#include "scenario.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace perf {

// What arrived at a Receiver.
struct Received
{
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;
    std::uint64_t lines = 0;
    // With timestamps: how long each line took from being recorded to being read here, in microseconds.
    Histogram delayMicros;
};

// Stands in for spectatord: a socket on this machine for the library to send to, and a thread that reads it and
// counts what arrives. The test knows what was recorded, so with this it also knows what was lost on the way.
class Receiver
{
   public:
    explicit Receiver(WriterKind kind);
    ~Receiver();

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    // Where the socket is, in the form that WriterConfig takes: udp://127.0.0.1:<port> or unix://<path>.
    const std::string& Location() const { return m_location; }

    // Starts reading. With `timestamps`, the value of every line is taken to be the time at which the line was
    // recorded, as the paced threads in recording_run.cpp set it, and the delay of every line is kept.
    void Start(bool timestamps);

    // To be called when the sending process has exited: reads what is still on its way, stops reading and returns
    // what arrived.
    Received Stop();

    // Whether a socket like the library's can send a datagram of `bytes` to a socket of this kind on this machine.
    // The limit differs between systems: macOS, for one, takes 2048 bytes on a Unix datagram socket by default.
    static bool Carries(WriterKind kind, std::size_t bytes);

   private:
    void Read();
    void KeepDelays(const char* datagram, std::size_t size);

    int m_socket = -1;
    std::string m_location;
    std::string m_socketPath;  // of a Unix socket, to remove it again
    bool m_timestamps = false;
    std::atomic<bool> m_stopping{false};
    std::thread m_thread;
    Received m_received;  // the reading thread's, until it has been joined
};

}  // namespace perf
