#pragma once

#include <chrono>

namespace spectator {

// How the Writer buffers lines. A size of 0 turns buffering off, so every line is sent as soon as it is written.
struct BufferConfig
{
    // Bytes each writing thread may hold before its lines are sent. The Writer clamps it to the largest datagram the
    // transport can send, so a full buffer always goes out in one send.
    unsigned int size = 0;

    // The longest a line can wait in a buffer before the drain thread sends it.
    std::chrono::milliseconds flushInterval = std::chrono::seconds(5);

    // What happens when a thread's buffer is full. false: the writing thread sends its buffer and carries on, so no
    // line is lost. true: the line is dropped and counted, and the drain thread is woken to empty the buffer.
    bool dropOnFull = false;
};

}  // namespace spectator
