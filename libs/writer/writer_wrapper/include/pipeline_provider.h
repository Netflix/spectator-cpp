#pragma once

#include <memory>

namespace spectator::detail {

class Pipeline;

// Where a thread session finds the Writer's current pipeline.
class PipelineProvider
{
   public:
    virtual ~PipelineProvider() = default;

    // The current pipeline, or null if the writer is not initialized.
    virtual std::shared_ptr<Pipeline> CurrentPipeline() = 0;
};

}  // namespace spectator::detail
