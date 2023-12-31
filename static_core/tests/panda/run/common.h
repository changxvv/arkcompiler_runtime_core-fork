/**
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "os/exec.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unistd.h>

namespace panda::cli::test {

inline std::string Separator()
{
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

class PandaTest : public testing::Test {
public:
    PandaTest() = default;

public:
    template <typename... Args>
    inline auto RunPandaFile(Args... args) const
    {
        // NOTE(mgonopolskiy): add an ability to specifty env to the Exec function
        setenv(ASAN_OPTIONS.data(), ASAN_NO_LEAKS.data(), OVERWRITE);
        auto res = os::exec::Exec(GetPanda().c_str(), args...);
        unsetenv(ASAN_OPTIONS.data());
        return res;
    }

public:
    inline void SetEnv(const std::string &path)
    {
        auto pos = path.rfind(Separator());
        panda_path_ = path.substr(0, pos) + Separator() + ".." + Separator() + "bin" + Separator() + "ark";
        source_path_ = path.substr(0, pos) + Separator() + ".." + Separator() + "tests" + Separator() + "panda" +
                       Separator() + "bin";
    }

    inline std::string GetPanda() const
    {
        return panda_path_;
    }

    inline std::string GetBinPath() const
    {
        return source_path_;
    }

public:
    virtual std::string GetDirName() const = 0;

private:
    static constexpr size_t OVERWRITE = 1;
    static constexpr std::string_view ASAN_OPTIONS = "ASAN_OPTIONS";
    static constexpr std::string_view ASAN_NO_LEAKS = "detect_leaks=0";

private:
    std::string panda_path_;
    std::string source_path_;
};

}  // namespace panda::cli::test