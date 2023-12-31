/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PANDA_RUNTIME_TOOLING_SAMPLER_THREAD_COMMUNICATOR_H_
#define PANDA_RUNTIME_TOOLING_SAMPLER_THREAD_COMMUNICATOR_H_

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "libpandabase/macros.h"
#include "libpandabase/utils/logger.h"
#include "os/failure_retry.h"

#include "runtime/tooling/sampler/sample_info.h"

namespace panda::tooling::sampler {

namespace test {
class SamplerTest;
}  // namespace test

class ThreadCommunicator final {
public:
    static constexpr uint8_t PIPE_READ_ID {0};
    static constexpr uint8_t PIPE_WRITE_ID {1};

    ThreadCommunicator() = default;

    ~ThreadCommunicator()
    {
        for (int fd : listener_pipe_) {
            if (fd != -1) {
                LOG_IF(PANDA_FAILURE_RETRY(::close(fd)) != 0, FATAL, PROFILER) << "Cannot close fd: " << fd;
            }
        }
    }

    bool Init()
    {
        if (listener_pipe_[PIPE_READ_ID] != -1) {
            return true;
        }
        return pipe2(listener_pipe_.data(), O_CLOEXEC) != -1;
    }

    bool IsPipeEmpty() const;
    PANDA_PUBLIC_API bool SendSample(const SampleInfo &sample) const;
    PANDA_PUBLIC_API bool ReadSample(SampleInfo *sample) const;

    NO_COPY_SEMANTIC(ThreadCommunicator);
    NO_MOVE_SEMANTIC(ThreadCommunicator);

private:
    std::array<int, 2> listener_pipe_ {-1, -1};

    friend class test::SamplerTest;
};

}  // namespace panda::tooling::sampler

#endif  // PANDA_RUNTIME_TOOLING_SAMPLER_THREAD_COMMUNICATOR_H_
