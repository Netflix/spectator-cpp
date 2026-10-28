#pragma once

namespace spectator::detail {

// What happened to a line handed to the sink, the pipeline or a thread session.
enum class WriteResult
{
    Consumed,  // sent, buffered, or deliberately dropped: don't retry
    Closed     // the destination was closed by a reconfigure or shutdown: reconnect before retrying
};

}  // namespace spectator::detail
