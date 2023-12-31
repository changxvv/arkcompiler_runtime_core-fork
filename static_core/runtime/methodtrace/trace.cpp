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

#include "trace.h"

#include "include/runtime_notification.h"
#include "include/runtime.h"
#include "include/method.h"
#include "include/class.h"
#include "plugins.h"
#include "runtime/include/runtime.h"

#include <string>

namespace panda {

Trace *volatile Trace::singleton_trace_ = nullptr;
bool Trace::is_tracing_ = false;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
LanguageContext Trace::ctx_ = LanguageContext(nullptr);

static uint64_t SystemMicroSecond()
{
    timespec current {};
    clock_gettime(CLOCK_MONOTONIC, &current);
    // NOTE(ht): Deleting OS-specific code, move to libpandabase, issue #3210
    return static_cast<uint64_t>(current.tv_sec) * UINT64_C(1000000) +  // 1000000 - time
           current.tv_nsec / UINT64_C(1000);                            // 1000 - time
}

static uint64_t RealTimeSecond()
{
    timespec current {};
    clock_gettime(CLOCK_REALTIME, &current);
    // NOTE(ht): Deleting OS-specific code, move to libpandabase, issue #3210
    return static_cast<uint64_t>(current.tv_sec);
}

static void ThreadCpuClock(os::thread::NativeHandleType thread, clockid_t *clock_id)
{
    pthread_getcpuclockid(thread, clock_id);
}
static uint64_t GetCpuMicroSecond()
{
    auto thread_self = os::thread::GetNativeHandle();
    clockid_t clock_id;
    ThreadCpuClock(thread_self, &clock_id);
    timespec current {};
    clock_gettime(clock_id, &current);
    // NOTE(ht): Deleting OS-specific code, move to libpandabase, issue #3210
    return static_cast<uint64_t>(current.tv_sec) * UINT64_C(1000000) +  // 1000000 - time
           current.tv_nsec / UINT64_C(1000);                            // 1000 - time
}

Trace::Trace(PandaUniquePtr<panda::os::unix::file::File> trace_file, size_t buffer_size)
    : trace_file_(std::move(trace_file)),
      buffer_size_(std::max(TRACE_HEADER_REAL_LENGTH, buffer_size)),
      trace_start_time_(SystemMicroSecond()),
      base_cpu_time_(GetCpuMicroSecond()),
      buffer_offset_(0)
{
    buffer_ = MakePandaUnique<uint8_t[]>(buffer_size_);  // NOLINT(modernize-avoid-c-arrays)
    if (memset_s(buffer_.get(), buffer_size_, 0, TRACE_HEADER_LENGTH) != EOK) {
        LOG(ERROR, RUNTIME) << __func__ << " memset error";
    }
    WriteDataByte(buffer_.get(), MAGIC_VALUE, number_of_4_bytes_);
    WriteDataByte(buffer_.get() + number_of_4_bytes_, TRACE_VERSION, number_of_2_bytes_);
    WriteDataByte(buffer_.get() + number_of_4_bytes_ + number_of_2_bytes_, TRACE_HEADER_LENGTH, number_of_2_bytes_);
    WriteDataByte(buffer_.get() + number_of_8_bytes_, trace_start_time_, number_of_8_bytes_);
    WriteDataByte(buffer_.get() + number_of_8_bytes_ + number_of_8_bytes_, TRACE_ITEM_SIZE, number_of_2_bytes_);
    // Atomic with relaxed order reason: data race with buffer_offset_ with no synchronization or ordering constraints
    // imposed on other reads or writes
    buffer_offset_.store(TRACE_HEADER_LENGTH, std::memory_order_relaxed);
}

Trace::~Trace() = default;

void Trace::StartTracing(const char *trace_filename, size_t buffer_size)
{
    os::memory::LockHolder lock(TRACE_LOCK);
    if (singleton_trace_ != nullptr) {
        LOG(ERROR, RUNTIME) << "method tracing is running, ignoring new request";
        return;
    }

    PandaString file_name(trace_filename);
    if (file_name.empty()) {
        file_name = "method";
        file_name = file_name + ConvertToString(std::to_string(RealTimeSecond()));
        file_name += ".trace";
#ifdef PANDA_TARGET_MOBILE
        file_name = "/data/misc/trace/" + file_name;
#endif  // PANDA_TARGET_MOBILE
    }

    auto trace_file = MakePandaUnique<panda::os::unix::file::File>(
        panda::os::file::Open(file_name, panda::os::file::Mode::READWRITECREATE).GetFd());
    if (!trace_file->IsValid()) {
        LOG(ERROR, RUNTIME) << "Cannot OPEN/CREATE the trace file " << file_name;
        return;
    }

    panda_file::SourceLang lang = panda::plugins::RuntimeTypeToLang(Runtime::GetRuntimeType());
    ctx_ = Runtime::GetCurrent()->GetLanguageContext(lang);

    singleton_trace_ = ctx_.CreateTrace(std::move(trace_file), buffer_size);

    Runtime::GetCurrent()->GetNotificationManager()->AddListener(singleton_trace_,
                                                                 RuntimeNotificationManager::Event::METHOD_EVENTS);
    is_tracing_ = true;
}

void Trace::StopTracing()
{
    os::memory::LockHolder lock(TRACE_LOCK);
    if (singleton_trace_ == nullptr) {
        LOG(ERROR, RUNTIME) << "need to stop method tracing, but no method tracing is running";
    } else {
        Runtime::GetCurrent()->GetNotificationManager()->RemoveListener(
            singleton_trace_, RuntimeNotificationManager::Event::METHOD_EVENTS);
        singleton_trace_->SaveTracingData();
        is_tracing_ = false;
        Runtime::GetCurrent()->GetInternalAllocator()->Delete(singleton_trace_);
        singleton_trace_ = nullptr;
    }
}

void Trace::TriggerTracing()
{
    if (!is_tracing_) {
        Trace::StartTracing("", FILE_SIZE);
    } else {
        Trace::StopTracing();
    }
}

void Trace::RecordThreadsInfo([[maybe_unused]] PandaOStringStream *os)
{
    os::memory::LockHolder lock(thread_info_lock_);
    for (const auto &info : thread_info_set_) {
        (*os) << info;
    }
}
uint64_t Trace::GetAverageTime()
{
    uint64_t begin = GetCpuMicroSecond();
    for (int i = 0; i < loop_number_; i++) {
        GetCpuMicroSecond();
    }
    uint64_t delay = GetCpuMicroSecond() - begin;
    return delay / divide_number_;
}

void Trace::RecordMethodsInfo(PandaOStringStream *os, const PandaSet<Method *> &called_methods)
{
    for (const auto &method : called_methods) {
        (*os) << GetMethodDetailInfo(method);
    }
}

void Trace::WriteInfoToBuf(const ManagedThread *thread, Method *method, EventFlag event, uint32_t thread_time,
                           uint32_t real_time)
{
    int32_t write_after_offset;
    int32_t write_before_offset;

    // Atomic with relaxed order reason: data race with buffer_offset_ with no synchronization or ordering constraints
    // imposed on other reads or writes
    write_before_offset = buffer_offset_.load(std::memory_order_relaxed);
    do {
        write_after_offset = write_before_offset + TRACE_ITEM_SIZE;
        if (buffer_size_ < static_cast<size_t>(write_after_offset)) {
            overbrim_ = true;
            return;
        }
    } while (!buffer_offset_.compare_exchange_weak(write_before_offset, write_after_offset, std::memory_order_relaxed));

    EventFlag flag = TRACE_METHOD_ENTER;
    switch (event) {
        case EventFlag::TRACE_METHOD_ENTER:
            flag = TRACE_METHOD_ENTER;
            break;
        case EventFlag::TRACE_METHOD_EXIT:
            flag = TRACE_METHOD_EXIT;
            break;
        case EventFlag::TRACE_METHOD_UNWIND:
            flag = TRACE_METHOD_UNWIND;
            break;
        default:
            LOG(ERROR, RUNTIME) << "unrecognized events" << std::endl;
    }
    uint32_t method_action_value = EncodeMethodAndEventToId(method, flag);

    uint8_t *ptr;
    ptr = buffer_.get() + write_before_offset;
    WriteDataByte(ptr, thread->GetId(), number_of_2_bytes_);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ptr += number_of_2_bytes_;
    WriteDataByte(ptr, method_action_value, number_of_4_bytes_);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ptr += number_of_4_bytes_;
    WriteDataByte(ptr, thread_time, number_of_4_bytes_);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ptr += number_of_4_bytes_;
    WriteDataByte(ptr, real_time, number_of_4_bytes_);
}

uint32_t Trace::EncodeMethodAndEventToId(Method *method, EventFlag flag)
{
    uint32_t method_action_id = (EncodeMethodToId(method) << ENCODE_EVENT_BITS) | flag;
    ASSERT(method == DecodeIdToMethod(method_action_id));
    return method_action_id;
}

uint32_t Trace::EncodeMethodToId(Method *method)
{
    os::memory::LockHolder lock(methods_lock_);
    uint32_t method_id_value;
    auto iter = method_id_pandamap_.find(method);
    if (iter != method_id_pandamap_.end()) {
        method_id_value = iter->second;
    } else {
        method_id_value = methods_called_vector_.size();
        methods_called_vector_.push_back(method);
        method_id_pandamap_.emplace(method, method_id_value);
    }
    return method_id_value;
}

Method *Trace::DecodeIdToMethod(uint32_t id)
{
    os::memory::LockHolder lock(methods_lock_);
    return methods_called_vector_[id >> ENCODE_EVENT_BITS];
}

void Trace::GetUniqueMethods(size_t buffer_length, PandaSet<Method *> *called_methods_set)
{
    uint8_t *data_begin = buffer_.get() + TRACE_HEADER_LENGTH;
    uint8_t *data_end = buffer_.get() + buffer_length;

    while (data_begin < data_end) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        uint32_t decoded_data = GetDataFromBuffer(data_begin + number_of_2_bytes_, number_of_4_bytes_);
        Method *method = DecodeIdToMethod(decoded_data);
        called_methods_set->insert(method);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        data_begin += TRACE_ITEM_SIZE;
    }
}

void Trace::SaveTracingData()
{
    uint64_t elapsed = SystemMicroSecond() - trace_start_time_;
    PandaOStringStream ostream;
    ostream << TRACE_STAR_CHAR << "version\n";
    ostream << TRACE_VERSION << "\n";
    ostream << "data-file-overflow=" << (overbrim_ ? "true" : "false") << "\n";
    ostream << "clock=dual\n";
    ostream << "elapsed-time-usec=" << elapsed << "\n";

    size_t buf_end;
    // Atomic with relaxed order reason: data race with buffer_offset_ with no synchronization or ordering constraints
    // imposed on other reads or writes
    buf_end = buffer_offset_.load(std::memory_order_relaxed);
    size_t num_records = (buf_end - TRACE_HEADER_LENGTH) / TRACE_ITEM_SIZE;
    ostream << "num-method-calls=" << num_records << "\n";
    ostream << "clock-call-overhead-nsec=" << GetAverageTime() << "\n";
    ostream << "pid=" << getpid() << "\n";
    ostream << "vm=panda\n";

    ostream << TRACE_STAR_CHAR << "threads\n";
    RecordThreadsInfo(&ostream);
    ostream << TRACE_STAR_CHAR << "methods\n";

    PandaSet<Method *> called_methods_set;
    GetUniqueMethods(buf_end, &called_methods_set);

    RecordMethodsInfo(&ostream, called_methods_set);
    ostream << TRACE_STAR_CHAR << "end\n";
    PandaString methods_info(ostream.str());

    trace_file_->WriteAll(reinterpret_cast<const void *>(methods_info.c_str()), methods_info.length());
    trace_file_->WriteAll(buffer_.get(), buf_end);
    trace_file_->Close();
}

void Trace::MethodEntry(ManagedThread *thread, Method *method)
{
    os::memory::LockHolder lock(thread_info_lock_);
    uint32_t thread_time = 0;
    uint32_t real_time = 0;
    GetTimes(&thread_time, &real_time);
    WriteInfoToBuf(thread, method, EventFlag::TRACE_METHOD_ENTER, thread_time, real_time);

    PandaOStringStream os;
    auto thread_name = GetThreadName(thread);
    os << thread->GetId() << "\t" << thread_name << "\n";
    PandaString info = os.str();
    thread_info_set_.insert(info);
}

void Trace::MethodExit(ManagedThread *thread, Method *method)
{
    uint32_t thread_time = 0;
    uint32_t real_time = 0;
    GetTimes(&thread_time, &real_time);
    WriteInfoToBuf(thread, method, EventFlag::TRACE_METHOD_EXIT, thread_time, real_time);
}

void Trace::GetTimes(uint32_t *thread_time, uint32_t *real_time)
{
    *thread_time = GetCpuMicroSecond() - base_cpu_time_;
    *real_time = SystemMicroSecond() - trace_start_time_;
}

}  // namespace panda
