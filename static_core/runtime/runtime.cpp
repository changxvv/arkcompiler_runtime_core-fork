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

#include "runtime/include/runtime.h"

#include "compiler_options.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "assembler/assembly-literals.h"
#include "core/core_language_context.h"
#include "intrinsics.h"
#include "libpandabase/events/events.h"
#include "libpandabase/mem/mem_config.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/os/cpu_affinity.h"
#include "libpandabase/os/mem_hooks.h"
#include "libpandabase/os/native_stack.h"
#include "libpandabase/os/thread.h"
#include "libpandabase/utils/arena_containers.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/dfx.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/file-inl.h"
#include "libpandafile/literal_data_accessor-inl.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "plugins.h"
#include "public.h"
#include "runtime/cha.h"
#include "runtime/compiler.h"
#include "runtime/dprofiler/dprofiler.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/class_linker_extension.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/language_context.h"
#include "runtime/include/locks.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/thread.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/include/tooling/debug_inf.h"
#include "runtime/handle_scope.h"
#include "runtime/handle_scope-inl.h"
#include "mem/refstorage/reference_storage.h"
#include "runtime/mem/gc/gc_stats.h"
#include "runtime/mem/gc/stw-gc/stw-gc.h"
#include "runtime/mem/heap_manager.h"
#include "runtime/mem/memory_manager.h"
#include "runtime/mem/internal_allocator-inl.h"
#include "runtime/mem/gc/gc-hung/gc_hung.h"
#include "runtime/include/panda_vm.h"
#include "runtime/profilesaver/profile_saver.h"
#include "runtime/tooling/debugger.h"
#include "runtime/tooling/memory_allocation_dumper.h"
#include "runtime/include/file_manager.h"
#include "runtime/methodtrace/trace.h"
#include "trace/trace.h"
#include "runtime/tests/intrusive-tests/intrusive_test_option.h"

namespace panda {

using std::unique_ptr;

Runtime *Runtime::instance_ = nullptr;
RuntimeOptions Runtime::options_;    // NOLINT(fuchsia-statically-constructed-objects)
std::string Runtime::runtime_type_;  // NOLINT(fuchsia-statically-constructed-objects)
os::memory::Mutex Runtime::mutex_;   // NOLINT(fuchsia-statically-constructed-objects)
taskmanager::TaskScheduler *Runtime::task_scheduler_ = nullptr;

const LanguageContextBase *CTXS_JS_RUNTIME = nullptr;  // Deprecated. Only for capability with js_runtime.

class RuntimeInternalAllocator {
public:
    static mem::InternalAllocatorPtr Create(bool use_malloc_for_internal_allocation)
    {
        ASSERT(mem::InternalAllocator<>::GetInternalAllocatorFromRuntime() == nullptr);

        mem_stats_s_ = new (std::nothrow) mem::MemStatsType();
        ASSERT(mem_stats_s_ != nullptr);

        if (use_malloc_for_internal_allocation) {
            internal_allocator_s_ = new (std::nothrow)
                mem::InternalAllocatorT<mem::InternalAllocatorConfig::MALLOC_ALLOCATOR>(mem_stats_s_);
        } else {
            internal_allocator_s_ = new (std::nothrow)
                mem::InternalAllocatorT<mem::InternalAllocatorConfig::PANDA_ALLOCATORS>(mem_stats_s_);
        }
        ASSERT(internal_allocator_s_ != nullptr);
        mem::InternalAllocator<>::InitInternalAllocatorFromRuntime(
            static_cast<mem::Allocator *>(internal_allocator_s_));

        return internal_allocator_s_;
    }

    static void Finalize()
    {
        internal_allocator_s_->VisitAndRemoveAllPools(
            [](void *mem, size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    }

    static void Destroy()
    {
        ASSERT(mem::InternalAllocator<>::GetInternalAllocatorFromRuntime() != nullptr);

        mem::InternalAllocator<>::ClearInternalAllocatorFromRuntime();
        delete static_cast<mem::Allocator *>(internal_allocator_s_);
        internal_allocator_s_ = nullptr;

        if (daemon_memory_leak_threshold_ == 0) {
            // One more check that we don't have memory leak in internal allocator.
            ASSERT(daemon_threads_cnt_ > 0 || mem_stats_s_->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL) == 0);
        } else {
            // There might be a memory leaks in daemon threads, which we intend to ignore (issue #6539).
            ASSERT(mem_stats_s_->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL) <= daemon_memory_leak_threshold_);
        }

        delete mem_stats_s_;
        mem_stats_s_ = nullptr;
    }

    static mem::InternalAllocatorPtr Get()
    {
        ASSERT(internal_allocator_s_ != nullptr);
        return internal_allocator_s_;
    }

    static void SetDaemonMemoryLeakThreshold(uint32_t daemon_memory_leak_threshold)
    {
        daemon_memory_leak_threshold_ = daemon_memory_leak_threshold;
    }

    static void SetDaemonThreadsCount(uint32_t daemon_threads_cnt)
    {
        daemon_threads_cnt_ = daemon_threads_cnt;
    }

private:
    static mem::MemStatsType *mem_stats_s_;
    static mem::InternalAllocatorPtr internal_allocator_s_;  // NOLINT(fuchsia-statically-constructed-objects)
    static uint32_t daemon_memory_leak_threshold_;
    static uint32_t daemon_threads_cnt_;
};

uint32_t RuntimeInternalAllocator::daemon_memory_leak_threshold_ = 0;
uint32_t RuntimeInternalAllocator::daemon_threads_cnt_ = 0;

mem::MemStatsType *RuntimeInternalAllocator::mem_stats_s_ = nullptr;
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
mem::InternalAllocatorPtr RuntimeInternalAllocator::internal_allocator_s_ = nullptr;

Runtime::DebugSession::DebugSession(Runtime &runtime)
    : runtime_(runtime), is_jit_enabled_(runtime.IsJitEnabled()), lock_(runtime.debug_session_uniqueness_mutex_)
{
    ASSERT(runtime_.is_debug_mode_);
    runtime_.ForceDisableJit();
    debugger_ = MakePandaUnique<tooling::Debugger>(&runtime_);
}

Runtime::DebugSession::~DebugSession()
{
    debugger_.reset();
    if (is_jit_enabled_) {
        runtime_.ForceEnableJit();
    }
}

tooling::DebugInterface &Runtime::DebugSession::GetDebugger()
{
    return *debugger_;
}

// all GetLanguageContext(...) methods should be based on this one
LanguageContext Runtime::GetLanguageContext(panda_file::SourceLang lang)
{
    if (CTXS_JS_RUNTIME != nullptr) {
        // Deprecated. Only for capability with js_runtime.
        return LanguageContext(CTXS_JS_RUNTIME);
    }

    auto *ctx = plugins::GetLanguageContextBase(lang);
    ASSERT(ctx != nullptr);
    return LanguageContext(ctx);
}

LanguageContext Runtime::GetLanguageContext(const Method &method)
{
    // Check class source lang
    auto *cls = method.GetClass();
    if (cls != nullptr) {
        return GetLanguageContext(cls->GetSourceLang());
    }

    panda_file::MethodDataAccessor mda(*method.GetPandaFile(), method.GetFileId());
    auto res = mda.GetSourceLang();
    return GetLanguageContext(res.value());
}

LanguageContext Runtime::GetLanguageContext(const Class &cls)
{
    return GetLanguageContext(cls.GetSourceLang());
}

LanguageContext Runtime::GetLanguageContext(const BaseClass &cls)
{
    return GetLanguageContext(cls.GetSourceLang());
}

LanguageContext Runtime::GetLanguageContext(panda_file::ClassDataAccessor *cda)
{
    auto res = cda->GetSourceLang();
    if (res) {
        return GetLanguageContext(res.value());
    }

    return GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
}

LanguageContext Runtime::GetLanguageContext(const std::string &runtime_type)
{
    return GetLanguageContext(plugins::RuntimeTypeToLang(runtime_type));
}

/* static */
bool Runtime::CreateInstance(const RuntimeOptions &options, mem::InternalAllocatorPtr internal_allocator)
{
    Locks::Initialize();

    if (options.WasSetEventsOutput()) {
        Events::Create(options.GetEventsOutput(), options.GetEventsFile());
    }

    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex_);

        if (instance_ != nullptr) {
            return false;
        }

        instance_ = new Runtime(options, internal_allocator);
    }

    return true;
}

inline bool CreateMemorySpaces(const RuntimeOptions &options)
{
    uint32_t min_free_percentage = options.GetMinHeapFreePercentage();
    uint32_t max_free_percentage = options.GetMaxHeapFreePercentage();
    if (min_free_percentage > PERCENT_100_U32) {
        LOG(ERROR, RUNTIME) << "Incorrect minimum free heap size percentage (min-free-percentage="
                            << min_free_percentage << "), 0 <= min-free-percentage <= 100";
        return false;
    }
    if (max_free_percentage > PERCENT_100_U32) {
        LOG(ERROR, RUNTIME) << "Incorrect maximum free heap size percentage (max-free-percentage="
                            << max_free_percentage << "), 0 <= max-free-percentage <= 100";
        return false;
    }
    if (min_free_percentage > max_free_percentage) {
        LOG(ERROR, RUNTIME) << "Minimum free heap size percentage(min-free-percentage=" << min_free_percentage
                            << ") must be <= maximum free heap size percentage (max-free-percentage="
                            << max_free_percentage << ")";
        return false;
    }
    size_t initial_object_size = options.GetInitHeapSizeLimit();
    size_t max_object_size = options.GetHeapSizeLimit();
    bool was_set_initial_object_size = options.WasSetInitHeapSizeLimit();
    bool was_set_max_object_size = options.WasSetHeapSizeLimit();
    if (!was_set_initial_object_size && was_set_max_object_size) {
        initial_object_size = max_object_size;
    } else if (initial_object_size > max_object_size) {
        if (was_set_initial_object_size && !was_set_max_object_size) {
            // Initial object heap size was set more default maximum object heap size, so set maximum heap size as
            // initial heap size
            max_object_size = initial_object_size;
        } else {  // In this case user set initial object heap size more maximum object heap size explicitly
            LOG(ERROR, RUNTIME) << "Initial heap size (" << initial_object_size << ") must be <= max heap size ("
                                << max_object_size << ")";
            return false;
        }
    }
    initial_object_size =
        std::max(AlignDown(initial_object_size, PANDA_POOL_ALIGNMENT_IN_BYTES), PANDA_POOL_ALIGNMENT_IN_BYTES);
    max_object_size =
        std::max(AlignDown(max_object_size, PANDA_POOL_ALIGNMENT_IN_BYTES), PANDA_POOL_ALIGNMENT_IN_BYTES);
    // Initialize memory spaces sizes
    mem::MemConfig::Initialize(max_object_size, options.GetInternalMemorySizeLimit(),
                               options.GetCompilerMemorySizeLimit(), options.GetCodeCacheSizeLimit(),
                               options.GetFramesMemorySizeLimit(), options.GetCoroutinesStackMemLimit(),
                               initial_object_size);
    PoolManager::Initialize();
    return true;
}

// Deprecated. Only for capability with js_runtime.
bool Runtime::Create(const RuntimeOptions &options, const std::vector<LanguageContextBase *> &ctxs)
{
    CTXS_JS_RUNTIME = ctxs.front();
    return Runtime::Create(options);
}

/* static */
bool Runtime::Create(const RuntimeOptions &options)
{
    if (instance_ != nullptr) {
        return false;
    }

    IntrusiveTestOption::SetTestId(options);

    const_cast<RuntimeOptions &>(options).InitializeRuntimeSpacesAndType();
    trace::ScopedTrace scoped_trace("Runtime::Create");

    os::CpuAffinityManager::Initialize();

    if (!CreateMemorySpaces(options)) {
        LOG(ERROR, RUNTIME) << "Failed to create memory spaces for runtime";
        return false;
    }

    mem::InternalAllocatorPtr internal_allocator =
        RuntimeInternalAllocator::Create(options.UseMallocForInternalAllocations());

    BlockSignals();

    CreateDfxController(options);

    CreateInstance(options, internal_allocator);

    if (instance_ == nullptr) {
        LOG(ERROR, RUNTIME) << "Failed to create runtime instance";
        return false;
    }

    if (!instance_->Initialize()) {
        LOG(ERROR, RUNTIME) << "Failed to initialize runtime";
        if (instance_->GetPandaVM() != nullptr) {
            instance_->GetPandaVM()->UninitializeThreads();
        }
        delete instance_;
        instance_ = nullptr;
        return false;
    }

    instance_->GetPandaVM()->StartGC();

    auto *thread = ManagedThread::GetCurrent();
    instance_->GetNotificationManager()->VmStartEvent();
    instance_->GetNotificationManager()->VmInitializationEvent(thread);
    instance_->GetNotificationManager()->ThreadStartEvent(thread);

    if (options.IsSamplingProfilerEnable()) {
        instance_->GetTools().CreateSamplingProfiler();
        instance_->GetTools().StartSamplingProfiler(options.GetSamplingProfilerOutputFile(),
                                                    options.GetSamplingProfilerInterval());
    }

    return true;
}

Runtime *Runtime::GetCurrent()
{
    return instance_;
}

/* static */
bool Runtime::DestroyUnderLockHolder()
{
    os::memory::LockHolder<os::memory::Mutex> lock(mutex_);

    if (instance_ == nullptr) {
        return false;
    }

    if (!instance_->Shutdown()) {
        LOG(ERROR, RUNTIME) << "Failed to shutdown runtime";
        return false;
    }
    if (GetOptions().WasSetEventsOutput()) {
        Events::Destroy();
    }

    /**
     * NOTE: Users threads can call log after destroying Runtime. We can't control these
     *       when they are in NATIVE_CODE mode because we don't destroy logger
     * Logger::Destroy();
     */

    DfxController::Destroy();
    panda::Logger::Sync();
    delete instance_;
    instance_ = nullptr;
    panda::mem::MemConfig::Finalize();

    return true;
}

/* static */
bool Runtime::Destroy()
{
    if (instance_ == nullptr) {
        return false;
    }

    trace::ScopedTrace scoped_trace("Runtime shutdown");

    if (instance_->GetOptions().IsSamplingProfilerEnable()) {
        instance_->GetTools().StopSamplingProfiler();
    }

    // when signal start, but no signal stop tracing, should stop it
    if (Trace::is_tracing_) {
        Trace::StopTracing();
    }

    instance_->GetPandaVM()->SaveProfileInfo();

    instance_->GetNotificationManager()->VmDeathEvent();

    // Stop compiler first to make sure compile memleak doesn't occur
    auto compiler = instance_->GetPandaVM()->GetCompiler();
    if (compiler != nullptr) {
        // ecmascript doesn't have compiler
        // Here we just join JITthread
        // And destruction will be performed after thread uninitialization
        compiler->JoinWorker();
    }

    // Note JIT thread (compiler) may access to thread data,
    // so, it should be stopped before thread destroy
    /* @sync 1
     * @description Before starting to unitialize threads
     * */
    instance_->GetPandaVM()->UninitializeThreads();

    /* @sync 2
     * @description After uninitialization of threads all deamon threads should have gone into the termination loop and
     * all other threads should have finished.
     * */
    // Stop GC after UninitializeThreads because
    // UninitializeThreads may execute managed code which
    // uses barriers
    instance_->GetPandaVM()->StopGC();

    if (task_scheduler_ != nullptr) {
        task_scheduler_->Finalize();
    }

    if (IsEnabled(options_.GetVerificationMode())) {
        verifier::DestroyService(instance_->verifier_service_, options_.IsVerificationUpdateCache());
    }

    DestroyUnderLockHolder();
    RuntimeInternalAllocator::Destroy();

    os::CpuAffinityManager::Finalize();
    if (task_scheduler_ != nullptr) {
        taskmanager::TaskScheduler::Destroy();
        task_scheduler_ = nullptr;
    }

    return true;
}

void Runtime::InitializeVerifierRuntime()
{
    auto mode = options_.GetVerificationMode();
    if (IsEnabled(mode)) {
        std::string const &cache_file = options_.GetVerificationCacheFile();
        verifier_service_ =
            panda::verifier::CreateService(verifier_config_, internal_allocator_, class_linker_, cache_file);
    }
}

/* static */
void Runtime::Halt(int32_t status)
{
    Runtime *runtime = Runtime::GetCurrent();
    if (runtime != nullptr && runtime->exit_ != nullptr) {
        runtime->exit_(status);
    }

    // _exit is safer to call because it guarantees a safe
    // completion in case of multi-threading as static destructors aren't called
    _exit(status);
}

/* static */
void Runtime::Abort(const char *message /* = nullptr */)
{
    Runtime *runtime = Runtime::GetCurrent();
    if (runtime != nullptr && runtime->abort_ != nullptr) {
        runtime->abort_();
    }

    std::cerr << "Runtime::Abort: " << ((message != nullptr) ? message : "") << std::endl;
    std::abort();
}

Runtime::Runtime(const RuntimeOptions &options, mem::InternalAllocatorPtr internal_allocator)
    : internal_allocator_(internal_allocator),
      notification_manager_(new RuntimeNotificationManager(internal_allocator_)),
      cha_(new ClassHierarchyAnalysis)
{
    Runtime::runtime_type_ = options.GetRuntimeType();
    /* @sync 1
     * @description Right before setting runtime options in Runtime constructor
     */
    Runtime::options_ = options;
    /* @sync 3
     * @description Right after setting runtime options in Runtime constructor
     */

    auto spaces = GetOptions().GetBootClassSpaces();

    std::vector<std::unique_ptr<ClassLinkerExtension>> extensions;
    extensions.reserve(spaces.size());

    for (const auto &space : spaces) {
        extensions.push_back(GetLanguageContext(space).CreateClassLinkerExtension());
    }

    class_linker_ = new ClassLinker(internal_allocator_, std::move(extensions));
#ifndef PANDA_TARGET_WINDOWS
    signal_manager_ = new SignalManager(internal_allocator_);
#endif

    if (IsEnableMemoryHooks()) {
        panda::os::mem_hooks::PandaHooks::Initialize();
        panda::os::mem_hooks::PandaHooks::Enable();
    }

    save_profiling_info_ = options_.IsCompilerEnableJit() && options_.IsProfilesaverEnabled();

#ifdef PANDA_COMPILER_ENABLE
    // NOTE(maksenov): Enable JIT for debug mode
    is_jit_enabled_ = !this->IsDebugMode() && Runtime::GetOptions().IsCompilerEnableJit();
#else
    is_jit_enabled_ = false;
#endif

    verifier_config_ = panda::verifier::NewConfig();
    InitializeVerifierRuntime();

    is_zygote_ = options_.IsStartAsZygote();

#ifdef PANDA_ENABLE_RELAYOUT_PROFILE
    relayout_profiler_ = internal_allocator_->New<RelayoutProfiler>();
#endif
    /* @sync 2
     * @description At the very end of the Runtime's constructor when all initialization actions have completed.
     * */
}

Runtime::~Runtime()
{
    panda::verifier::DestroyConfig(verifier_config_);

    if (IsEnableMemoryHooks()) {
        panda::os::mem_hooks::PandaHooks::Disable();
    }
    trace::ScopedTrace scoped_trace("Delete state");

#ifndef PANDA_TARGET_WINDOWS
    signal_manager_->DeleteHandlersArray();
    delete signal_manager_;
#endif
    delete cha_;
    delete class_linker_;
    if (dprofiler_ != nullptr) {
        internal_allocator_->Delete(dprofiler_);
    }
    delete notification_manager_;

    if (panda_vm_ != nullptr) {
        internal_allocator_->Delete(panda_vm_);
        /* @sync 1
         * @description: This point is right after runtime deastroys panda VM.
         * */
    }

#ifdef PANDA_ENABLE_RELAYOUT_PROFILE
    if (relayout_profiler_ != nullptr) {
        internal_allocator_->Delete(relayout_profiler_);
    }
#endif
    /* @sync 1
     * @description Right after runtime's destructor has deleted panda virtual machine
     * */

    // crossing map is shared by different VMs.
    mem::CrossingMapSingleton::Destroy();

    RuntimeInternalAllocator::Finalize();
    PoolManager::Finalize();
}

bool Runtime::IsEnableMemoryHooks() const
{
    auto log_level = Logger::IsInitialized() ? Logger::GetLevel() : Logger::Level::DEBUG;
    return options_.IsLimitStandardAlloc() &&
           (log_level == Logger::Level::FATAL || log_level == Logger::Level::ERROR) &&
           (!options_.UseMallocForInternalAllocations());
}

static PandaVector<PandaString> GetPandaFilesList(const std::vector<std::string> &stdvec)
{
    PandaVector<PandaString> res;
    for (const auto &i : stdvec) {
        // NOLINTNEXTLINE(readability-redundant-string-cstr)
        res.push_back(i.c_str());
    }

    return res;
}

PandaVector<PandaString> Runtime::GetBootPandaFiles()
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &boot_panda_files = GetPandaFilesList(options_.GetBootPandaFiles());
    return boot_panda_files;
}

PandaVector<PandaString> Runtime::GetPandaFiles()
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &app_panda_files = GetPandaFilesList(options_.GetPandaFiles());
    return app_panda_files;
}

bool Runtime::LoadBootPandaFiles(panda_file::File::OpenMode open_mode)
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &boot_panda_files = options_.GetBootPandaFiles();
    for (const auto &name : boot_panda_files) {
        if (!FileManager::LoadAbcFile(name, open_mode)) {
#ifdef PANDA_PRODUCT_BUILD
            LOG(FATAL, RUNTIME) << "Load boot panda file failed: " << name;
#else
            LOG(ERROR, RUNTIME) << "Load boot panda file failed: " << name;
#endif  // PANDA_PRODUCT_BUILD
            return false;
        }
    }

    return true;
}

void Runtime::CheckBootPandaFiles()
{
    auto skip_last = static_cast<size_t>(options_.GetPandaFiles().empty() && !options_.IsStartAsZygote());
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    const auto &boot_panda_files = options_.GetBootPandaFiles();
    for (size_t i = 0; i + skip_last < boot_panda_files.size(); ++i) {
        auto &name = boot_panda_files[i];
        if (class_linker_->GetAotManager()->FindPandaFile(name) == nullptr) {
            LOG(FATAL, RUNTIME) << "AOT file wasn't loaded for panda file: " << name;
        }
    }
}

void Runtime::SetDaemonMemoryLeakThreshold(uint32_t daemon_memory_leak_threshold)
{
    RuntimeInternalAllocator::SetDaemonMemoryLeakThreshold(daemon_memory_leak_threshold);
}

void Runtime::SetDaemonThreadsCount(uint32_t daemon_threads_cnt)
{
    RuntimeInternalAllocator::SetDaemonThreadsCount(daemon_threads_cnt);
}

mem::GCType Runtime::GetGCType(const RuntimeOptions &options, panda_file::SourceLang lang)
{
    auto gc_type = panda::mem::GCTypeFromString(options.GetGcType(plugins::LangToRuntimeType(lang)));
    if (options.IsNoAsyncJit()) {
        // With no-async-jit we can force compilation inside of c2i bridge (we have DecrementHotnessCounter there)
        // and it can trigger GC which can move objects which are arguments for the method
        // because StackWalker ignores c2i frame
        return (gc_type != panda::mem::GCType::EPSILON_GC) ? (panda::mem::GCType::STW_GC) : gc_type;
    }
    return gc_type;
}

bool Runtime::LoadVerificationConfig()
{
    return !IsEnabled(options_.GetVerificationMode()) ||
           verifier::LoadConfigFile(verifier_config_, options_.GetVerificationConfigFile());
}

bool Runtime::CreatePandaVM(std::string_view runtime_type)
{
    ManagedThread::Initialize();

    panda_vm_ = PandaVM::Create(this, options_, runtime_type);
    if (panda_vm_ == nullptr) {
        LOG(ERROR, RUNTIME) << "Failed to create panda vm";
        return false;
    }

    if (task_scheduler_ != nullptr) {
        task_scheduler_->Initialize();
    }

    panda_file::File::OpenMode open_mode = GetLanguageContext(GetRuntimeType()).GetBootPandaFilesOpenMode();
    bool load_boot_panda_files_is_failed = options_.ShouldLoadBootPandaFiles() && !LoadBootPandaFiles(open_mode);
    if (load_boot_panda_files_is_failed) {
        LOG(ERROR, RUNTIME) << "Failed to load boot panda files";
        return false;
    }

    auto aot_boot_ctx = class_linker_->GetClassContextForAot(options_.IsAotVerifyAbsPath());
    if (options_.GetPandaFiles().empty() && !options_.IsStartAsZygote()) {
        // Main from panda.cpp puts application file into boot panda files as the last element.
        // During AOT compilation of boot files no application panda files were used.
        auto idx = aot_boot_ctx.find_last_of(':');
        if (idx == std::string::npos) {
            // Only application file is in aot_boot_ctx
            class_linker_->GetAotManager()->SetAppClassContext(aot_boot_ctx);
            aot_boot_ctx = "";
        } else {
            // Last file is an application
            class_linker_->GetAotManager()->SetAppClassContext(aot_boot_ctx.substr(idx + 1));
            aot_boot_ctx = aot_boot_ctx.substr(0, idx);
        }
    }
    class_linker_->GetAotManager()->SetBootClassContext(aot_boot_ctx);
    if (panda_vm_->GetLanguageContext().IsEnabledCHA()) {
        class_linker_->GetAotManager()->VerifyClassHierarchy();
    }
#ifndef PANDA_PRODUCT_BUILD
    if (Runtime::GetOptions().IsEnableAnForce() && !Runtime::GetOptions().IsArkAot()) {
        CheckBootPandaFiles();
    }
#endif  // PANDA_PRODUCT_BUILD
    notification_manager_->SetRendezvous(panda_vm_->GetRendezvous());

    return true;
}

bool Runtime::InitializePandaVM()
{
    // temporary solution, see #7225
    if (!options_.IsRuntimeCompressedStringsEnabled()) {
        LOG(FATAL, RUNTIME) << "Non compressed strings is not supported";
    }

    if (!class_linker_->Initialize(options_.IsRuntimeCompressedStringsEnabled())) {
        LOG(ERROR, RUNTIME) << "Failed to initialize class loader";
        return false;
    }

    if (panda_vm_->ShouldEnableDebug()) {
        SetDebugMode(true);
        StartDebugSession();
    }

    if (!panda_vm_->Initialize()) {
        LOG(ERROR, RUNTIME) << "Failed to initialize panda vm";
        return false;
    }

    return true;
}

bool Runtime::HandleAotOptions()
{
    auto aot_files = options_.GetAotFiles();
    const auto &name = options_.GetAotFile();
    if (!name.empty()) {
        aot_files.push_back(name);
    }
    if (!aot_files.empty()) {
        for (auto &fname : aot_files) {
            auto res = FileManager::LoadAnFile(fname, true);
            if (!res) {
                LOG(FATAL, AOT) << "Failed to load AoT file: " << res.Error();
            }
            if (!res.Value()) {
                LOG(FATAL, AOT) << "Failed to load '" << fname << "' with unknown reason";
            }
        }
    }

    return true;
}

void Runtime::HandleJitOptions()
{
#ifndef PANDA_TARGET_WINDOWS
    auto signal_manager_flag = DfxController::GetOptionValue(DfxOptionHandler::SIGNAL_HANDLER);
    if (signal_manager_flag == 1) {
        signal_manager_->InitSignals();
    } else {
        LOG(ERROR, DFX) << "signal handler disabled, setprop ark.dfx.options to restart";
    }
#endif

    bool enable_np_handler = options_.IsCompilerEnableJit() && panda::compiler::OPTIONS.IsCompilerImplicitNullCheck();
    if (GetClassLinker()->GetAotManager()->HasAotFiles()) {
        ASSERT(GetPandaVM()->GetCompiler()->IsNoAsyncJit() == options_.IsNoAsyncJit());
        if (GetPandaVM()->GetCompiler()->IsNoAsyncJit()) {
            LOG(FATAL, AOT) << "We can't use the option --no-async-jit=true with AOT";
        }
        enable_np_handler = true;
    }

#ifndef PANDA_TARGET_WINDOWS
    if (signal_manager_->IsInitialized() && enable_np_handler) {
        auto *handler = signal_manager_->GetAllocator()->New<NullPointerHandler>();
        signal_manager_->AddHandler(handler, true);
    }
    {
        auto *handler = signal_manager_->GetAllocator()->New<StackOverflowHandler>();
        signal_manager_->AddHandler(handler, true);
    }
#endif
}

bool Runtime::CheckOptionsConsistency()
{
    {
        auto value = options_.GetResolveStringAotThreshold();
        auto limit = RuntimeInterface::RESOLVE_STRING_AOT_COUNTER_LIMIT;
        if (value >= limit) {
            LOG(ERROR, RUNTIME) << "--resolve-string-aot-threshold value (" << value << ") is "
                                << ((value == limit) ? "equal to " : "greater than ") << limit
                                << ", ResolveString optimization won't be applied to AOT-compiled code. "
                                << "Consider value lower than " << limit << " to enable the optimization.";
        }
    }
    return true;
}

void Runtime::SetPandaPath()
{
    PandaVector<PandaString> app_panda_files = GetPandaFiles();
    for (size_t i = 0; i < app_panda_files.size(); ++i) {
        panda_path_string_ += PandaStringToStd(app_panda_files[i]);
        if (i != app_panda_files.size() - 1) {
            panda_path_string_ += ":";
        }
    }
}

void Runtime::SetThreadClassPointers()
{
    ManagedThread *thread = ManagedThread::GetCurrent();
    class_linker_->InitializeRoots(thread);
    auto ext = GetClassLinker()->GetExtension(GetLanguageContext(GetRuntimeType()));
    if (ext != nullptr) {
        thread->SetStringClassPtr(ext->GetClassRoot(ClassRoot::STRING));
        thread->SetArrayU16ClassPtr(ext->GetClassRoot(ClassRoot::ARRAY_U16));
    }
}

bool Runtime::Initialize()
{
    trace::ScopedTrace scoped_trace("Runtime::Initialize");

    if (!CheckOptionsConsistency()) {
        return false;
    }

    if (!LoadVerificationConfig()) {
        return false;
    }

    CheckOptionsFromOs();

    if (!CreatePandaVM(GetRuntimeType())) {
        return false;
    }

#if defined(PANDA_COMPILER_DEBUG_INFO) && !defined(NDEBUG)
    if (!compiler::OPTIONS.WasSetCompilerEmitDebugInfo()) {
        compiler::OPTIONS.SetCompilerEmitDebugInfo(true);
    }
#endif

    // We must load AOT file before InitializePandaVM, because during initialization, code execution may be called.
    if (!HandleAotOptions()) {
        return false;
    }

    if (!InitializePandaVM()) {
        return false;
    }

    SetThreadClassPointers();

    finger_print_ = ConvertToString(options_.GetFingerprint());

    HandleJitOptions();

    SetPandaPath();

    if (!panda_vm_->InitializeFinish()) {
        LOG(ERROR, RUNTIME) << "Failed to finish panda vm initialization";
        return false;
    }

    if (IsDebugMode()) {
        panda_vm_->LoadDebuggerAgent();
    }

    if (options_.WasSetMemAllocDumpExec()) {
        StartMemAllocDumper(ConvertToString(options_.GetMemAllocDumpFile()));
    }

#ifdef PANDA_TARGET_MOBILE
    mem::GcHung::InitPreFork(true);
#else
    mem::GcHung::InitPreFork(false);
#endif  // PANDA_TARGET_MOBILE

    is_initialized_ = true;
    return true;
}

int Runtime::StartMemAllocDumper(const PandaString &dump_file)
{
    ASSERT(mem_alloc_dumper_ == nullptr);

    mem_alloc_dumper_ = internal_allocator_->New<tooling::MemoryAllocationDumper>(dump_file, Runtime::GetCurrent());
    return 0;
}

static bool GetClassAndMethod(std::string_view entry_point, PandaString *class_name, PandaString *method_name)
{
    size_t pos = entry_point.find_last_of("::");
    if (pos == std::string_view::npos) {
        return false;
    }

    *class_name = PandaString(entry_point.substr(0, pos - 1));
    *method_name = PandaString(entry_point.substr(pos + 1));

    return true;
}

static const uint8_t *GetStringArrayDescriptor(const LanguageContext &ctx, PandaString *out)
{
    *out = "[";
    *out += utf::Mutf8AsCString(ctx.GetStringClassDescriptor());

    return utf::CStringAsMutf8(out->c_str());
}

Expected<Method *, Runtime::Error> Runtime::ResolveEntryPoint(std::string_view entry_point)
{
    PandaString class_name;
    PandaString method_name;

    if (!GetClassAndMethod(entry_point, &class_name, &method_name)) {
        LOG(ERROR, RUNTIME) << "Invalid entry point: " << entry_point;
        return Unexpected(Runtime::Error::INVALID_ENTRY_POINT);
    }

    PandaString descriptor;
    auto class_name_bytes = ClassHelper::GetDescriptor(utf::CStringAsMutf8(class_name.c_str()), &descriptor);
    auto method_name_bytes = utf::CStringAsMutf8(method_name.c_str());

    Class *cls = nullptr;
    ClassLinkerContext *context = app_context_.ctx;
    if (context == nullptr) {
        context = class_linker_->GetExtension(GetLanguageContext(GetRuntimeType()))->GetBootContext();
    }

    ManagedThread *thread = ManagedThread::GetCurrent();
    ScopedManagedCodeThread sa(thread);
    cls = class_linker_->GetClass(class_name_bytes, true, context);

    if (cls == nullptr) {
        LOG(ERROR, RUNTIME) << "Cannot find class '" << class_name << "'";
        return Unexpected(Runtime::Error::CLASS_NOT_FOUND);
    }

    LanguageContext ctx = GetLanguageContext(*cls);
    PandaString string_array_descriptor;
    GetStringArrayDescriptor(ctx, &string_array_descriptor);

    Method::Proto proto(Method::Proto::ShortyVector {panda_file::Type(panda_file::Type::TypeId::VOID),
                                                     panda_file::Type(panda_file::Type::TypeId::REFERENCE)},
                        Method::Proto::RefTypeVector {string_array_descriptor});

    auto method = cls->GetDirectMethod(method_name_bytes, proto);
    if (method == nullptr) {
        method = cls->GetDirectMethod(method_name_bytes);
        if (method == nullptr) {
            LOG(ERROR, RUNTIME) << "Cannot find method '" << entry_point << "'";
            return Unexpected(Runtime::Error::METHOD_NOT_FOUND);
        }
    }

    return method;
}

PandaString Runtime::GetMemoryStatistics()
{
    return panda_vm_->GetMemStats()->GetStatistics();
}

PandaString Runtime::GetFinalStatistics()
{
    return panda_vm_->GetGCStats()->GetFinalStatistics(panda_vm_->GetHeapManager());
}

void Runtime::NotifyAboutLoadedModules()
{
    PandaVector<const panda_file::File *> pfs;

    class_linker_->EnumerateBootPandaFiles([&pfs](const panda_file::File &pf) {
        pfs.push_back(&pf);
        return true;
    });

    for (const auto *pf : pfs) {
        GetNotificationManager()->LoadModuleEvent(pf->GetFilename());
    }
}

Expected<LanguageContext, Runtime::Error> Runtime::ExtractLanguageContext(const panda_file::File *pf,
                                                                          std::string_view entry_point)
{
    PandaString class_name;
    PandaString method_name;
    if (!GetClassAndMethod(entry_point, &class_name, &method_name)) {
        LOG(ERROR, RUNTIME) << "Invalid entry point: " << entry_point;
        return Unexpected(Runtime::Error::INVALID_ENTRY_POINT);
    }

    PandaString descriptor;
    auto class_name_bytes = ClassHelper::GetDescriptor(utf::CStringAsMutf8(class_name.c_str()), &descriptor);
    auto method_name_bytes = utf::CStringAsMutf8(method_name.c_str());

    auto class_id = pf->GetClassId(class_name_bytes);
    if (!class_id.IsValid() || pf->IsExternal(class_id)) {
        LOG(ERROR, RUNTIME) << "Cannot find class '" << class_name << "'";
        return Unexpected(Runtime::Error::CLASS_NOT_FOUND);
    }

    panda_file::ClassDataAccessor cda(*pf, class_id);
    LanguageContext ctx = GetLanguageContext(&cda);
    bool found = false;
    cda.EnumerateMethods([this, &pf, method_name_bytes, &found, &ctx](panda_file::MethodDataAccessor &mda) {
        if (!found && utf::IsEqual(pf->GetStringData(mda.GetNameId()).data, method_name_bytes)) {
            found = true;
            auto val = mda.GetSourceLang();
            if (val) {
                ctx = GetLanguageContext(val.value());
            }
        }
    });

    if (!found) {
        LOG(ERROR, RUNTIME) << "Cannot find method '" << entry_point << "'";
        return Unexpected(Runtime::Error::METHOD_NOT_FOUND);
    }

    return ctx;
}

std::optional<Runtime::Error> Runtime::CreateApplicationClassLinkerContext(std::string_view filename,
                                                                           std::string_view entry_point)
{
    bool is_loaded = false;
    class_linker_->EnumerateBootPandaFiles([&is_loaded, filename](const panda_file::File &pf) {
        if (pf.GetFilename() == filename) {
            is_loaded = true;
            return false;
        }
        return true;
    });

    if (is_loaded) {
        return {};
    }

    auto pf = panda_file::OpenPandaFileOrZip(filename);

    if (pf == nullptr) {
        return Runtime::Error::PANDA_FILE_LOAD_ERROR;
    }

    auto res = ExtractLanguageContext(pf.get(), entry_point);

    if (!res) {
        return res.Error();
    }

    if (!class_linker_->HasExtension(res.Value())) {
        LOG(ERROR, RUNTIME) << "class linker hasn't " << res.Value() << " language extension";
        return Runtime::Error::CLASS_LINKER_EXTENSION_NOT_FOUND;
    }

    auto *ext = class_linker_->GetExtension(res.Value());
    app_context_.lang = ext->GetLanguage();
    app_context_.ctx = class_linker_->GetAppContext(filename);
    if (app_context_.ctx == nullptr) {
        auto app_files = GetPandaFiles();
        auto found_iter = std::find_if(app_files.begin(), app_files.end(),
                                       [&](auto &app_file_name) { return app_file_name == filename; });
        if (found_iter == app_files.end()) {
            PandaString path(filename);
            app_files.push_back(path);
        }
        app_context_.ctx = ext->CreateApplicationClassLinkerContext(app_files);
    }

    PandaString aot_ctx;
    app_context_.ctx->EnumeratePandaFiles(compiler::AotClassContextCollector(&aot_ctx, options_.IsAotVerifyAbsPath()));
    class_linker_->GetAotManager()->SetAppClassContext(aot_ctx);

    tooling::DebugInf::AddCodeMetaInfo(pf.get());
    return {};
}

Expected<int, Runtime::Error> Runtime::ExecutePandaFile(std::string_view filename, std::string_view entry_point,
                                                        const std::vector<std::string> &args)
{
    if (options_.IsDistributedProfiling()) {
        // Create app name from path to executable file.
        std::string_view app_name = [](std::string_view path) -> std::string_view {
            auto pos = path.find_last_of('/');
            return path.substr((pos == std::string_view::npos) ? 0 : (pos + 1));
        }(filename);
        StartDProfiler(app_name);
    }

    auto ctx_err = CreateApplicationClassLinkerContext(filename, entry_point);

    if (ctx_err) {
        return Unexpected(ctx_err.value());
    }

    if (panda_vm_->GetLanguageContext().IsEnabledCHA()) {
        class_linker_->GetAotManager()->VerifyClassHierarchy();
    }

    // Check if all input files are either quickened or not
    uint32_t quickened_files = 0;
    uint32_t panda_files = 0;
    class_linker_->EnumeratePandaFiles([&quickened_files, &panda_files](const panda_file::File &pf) {
        if (pf.GetHeader()->quickened_flag != 0) {
            ++quickened_files;
        }
        panda_files++;
        return true;
    });
    if (quickened_files != 0 && quickened_files != panda_files) {
        LOG(ERROR, RUNTIME) << "All input files should be either quickened or not. Got " << quickened_files
                            << " quickened files out of " << panda_files << " input files.";
        class_linker_->EnumeratePandaFiles([](const panda_file::File &pf) {
            if (pf.GetHeader()->quickened_flag != 0) {
                LOG(ERROR, RUNTIME) << "File " << pf.GetFilename() << " is quickened";
            } else {
                LOG(ERROR, RUNTIME) << "File " << pf.GetFilename() << " is not quickened";
            }
            return true;
        });
        UNREACHABLE();
    }

    return Execute(entry_point, args);
}

Expected<int, Runtime::Error> Runtime::Execute(std::string_view entry_point, const std::vector<std::string> &args)
{
    auto resolve_res = ResolveEntryPoint(entry_point);

    if (!resolve_res) {
        return Unexpected(resolve_res.Error());
    }

    NotifyAboutLoadedModules();

    Method *method = resolve_res.Value();

    return panda_vm_->InvokeEntrypoint(method, args);
}

int Runtime::StartDProfiler(std::string_view app_name)
{
    if (dprofiler_ != nullptr) {
        LOG(ERROR, RUNTIME) << "DProfiller already started";
        return -1;
    }

    dprofiler_ = internal_allocator_->New<DProfiler>(app_name, Runtime::GetCurrent());
    return 0;
}

Runtime::DebugSessionHandle Runtime::StartDebugSession()
{
    os::memory::LockHolder<os::memory::Mutex> lock(debug_session_creation_mutex_);

    auto session = debug_session_;
    if (session) {
        return session;
    }

    session = MakePandaShared<DebugSession>(*this);

    debug_session_ = session;

    return session;
}

bool Runtime::Shutdown()
{
    if (mem_alloc_dumper_ != nullptr) {
        internal_allocator_->Delete(mem_alloc_dumper_);
    }
    panda_vm_->UnloadDebuggerAgent();
    debug_session_.reset();
    ManagedThread::Shutdown();
    return true;
}

coretypes::String *Runtime::ResolveString(PandaVM *vm, const Method &caller, panda_file::File::EntityId id)
{
    auto *pf = caller.GetPandaFile();
    return vm->ResolveString(*pf, id);
}

coretypes::String *Runtime::ResolveStringFromCompiledCode(PandaVM *vm, const Method &caller,
                                                          panda_file::File::EntityId id)
{
    auto *pf = caller.GetPandaFile();
    return vm->ResolveStringFromCompiledCode(*pf, id);
}

coretypes::String *Runtime::ResolveString(PandaVM *vm, const panda_file::File &pf, panda_file::File::EntityId id,
                                          const LanguageContext &ctx)
{
    coretypes::String *str = vm->GetStringTable()->GetInternalStringFast(pf, id);
    if (str != nullptr) {
        return str;
    }
    str = vm->GetStringTable()->GetOrInternInternalString(pf, id, ctx);
    return str;
}

coretypes::String *Runtime::ResolveString(PandaVM *vm, const uint8_t *mutf8, uint32_t length,
                                          const LanguageContext &ctx)
{
    return vm->GetStringTable()->GetOrInternString(mutf8, length, ctx);
}

coretypes::Array *Runtime::ResolveLiteralArray(PandaVM *vm, const Method &caller, uint32_t id)
{
    auto *pf = caller.GetPandaFile();
    id = pf->GetLiteralArrays()[id];
    LanguageContext ctx = GetLanguageContext(caller);
    return ResolveLiteralArray(vm, *pf, id, ctx);
}

Class *Runtime::GetClassRootForLiteralTag(const ClassLinkerExtension &ext, panda_file::LiteralTag tag) const
{
    switch (tag) {
        case panda_file::LiteralTag::ARRAY_U1:
            return ext.GetClassRoot(ClassRoot::ARRAY_U1);
        case panda_file::LiteralTag::ARRAY_U8:
            return ext.GetClassRoot(ClassRoot::ARRAY_U8);
        case panda_file::LiteralTag::ARRAY_I8:
            return ext.GetClassRoot(ClassRoot::ARRAY_I8);
        case panda_file::LiteralTag::ARRAY_U16:
            return ext.GetClassRoot(ClassRoot::ARRAY_U16);
        case panda_file::LiteralTag::ARRAY_I16:
            return ext.GetClassRoot(ClassRoot::ARRAY_I16);
        case panda_file::LiteralTag::ARRAY_U32:
            return ext.GetClassRoot(ClassRoot::ARRAY_U32);
        case panda_file::LiteralTag::ARRAY_I32:
            return ext.GetClassRoot(ClassRoot::ARRAY_I32);
        case panda_file::LiteralTag::ARRAY_U64:
            return ext.GetClassRoot(ClassRoot::ARRAY_U64);
        case panda_file::LiteralTag::ARRAY_I64:
            return ext.GetClassRoot(ClassRoot::ARRAY_I64);
        case panda_file::LiteralTag::ARRAY_F32:
            return ext.GetClassRoot(ClassRoot::ARRAY_F32);
        case panda_file::LiteralTag::ARRAY_F64:
            return ext.GetClassRoot(ClassRoot::ARRAY_F64);
        case panda_file::LiteralTag::ARRAY_STRING:
            return ext.GetClassRoot(ClassRoot::ARRAY_STRING);
        case panda_file::LiteralTag::TAGVALUE:
        case panda_file::LiteralTag::BOOL:
        case panda_file::LiteralTag::INTEGER:
        case panda_file::LiteralTag::FLOAT:
        case panda_file::LiteralTag::DOUBLE:
        case panda_file::LiteralTag::STRING:
        case panda_file::LiteralTag::METHOD:
        case panda_file::LiteralTag::GENERATORMETHOD:
        case panda_file::LiteralTag::ASYNCGENERATORMETHOD:
        case panda_file::LiteralTag::ASYNCMETHOD:
        case panda_file::LiteralTag::ACCESSOR:
        case panda_file::LiteralTag::NULLVALUE: {
            break;
        }
        default: {
            break;
        }
    }
    UNREACHABLE();
    return nullptr;
}

/* static */
bool Runtime::GetLiteralTagAndValue(const panda_file::File &pf, uint32_t id, panda_file::LiteralTag *tag,
                                    panda_file::LiteralDataAccessor::LiteralValue *value)
{
    panda_file::File::EntityId literal_arrays_id = pf.GetLiteralArraysId();
    panda_file::LiteralDataAccessor literal_data_accessor(pf, literal_arrays_id);
    bool result = false;
    literal_data_accessor.EnumerateLiteralVals(
        panda_file::File::EntityId(id), [tag, value, &result](const panda_file::LiteralDataAccessor::LiteralValue &val,
                                                              const panda_file::LiteralTag &tg) {
            *tag = tg;
            *value = val;
            result = true;
        });
    return result;
}

uintptr_t Runtime::GetPointerToConstArrayData(const panda_file::File &pf, uint32_t id) const
{
    panda_file::LiteralTag tag;
    panda_file::LiteralDataAccessor::LiteralValue value;
    if (!GetLiteralTagAndValue(pf, id, &tag, &value)) {
        UNREACHABLE();
        return 0;
    }

    auto sp = pf.GetSpanFromId(panda_file::File::EntityId(std::get<uint32_t>(value)));
    // first element in the sp is array size, panda_file::helpers::Read move sp pointer to next element
    [[maybe_unused]] auto len = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
    return reinterpret_cast<uintptr_t>(sp.data());
}

coretypes::Array *Runtime::ResolveLiteralArray(PandaVM *vm, const panda_file::File &pf, uint32_t id,
                                               const LanguageContext &ctx)
{
    panda_file::LiteralTag tag;
    panda_file::LiteralDataAccessor::LiteralValue value;
    if (!GetLiteralTagAndValue(pf, id, &tag, &value)) {
        return nullptr;
    }

    auto sp = pf.GetSpanFromId(panda_file::File::EntityId(std::get<uint32_t>(value)));

    auto len = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
    auto ext = Runtime::GetCurrent()->GetClassLinker()->GetExtension(ctx);

    if (tag != panda_file::LiteralTag::ARRAY_STRING) {
        return coretypes::Array::Create(GetClassRootForLiteralTag(*ext, tag), sp.data(), len);
    }

    // special handling of arrays of strings
    auto array = coretypes::Array::Create(GetClassRootForLiteralTag(*ext, tag), len);
    [[maybe_unused]] HandleScope<ObjectHeader *> scope(ManagedThread::GetCurrent());
    VMHandle<coretypes::Array> obj(ManagedThread::GetCurrent(), array);
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (size_t i = 0; i < len; i++) {
        auto str_id = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
        auto str = Runtime::GetCurrent()->ResolveString(vm, pf, panda_file::File::EntityId(str_id), ctx);
        obj->Set<ObjectHeader *>(i, str);
    }
    return obj.GetPtr();
}

void Runtime::UpdateProcessState([[maybe_unused]] int state)
{
    LOG(INFO, RUNTIME) << __func__ << " is an empty implementation now.";
}

void Runtime::RegisterSensitiveThread() const
{
    LOG(INFO, RUNTIME) << __func__ << " is an empty implementation now.";
}

void Runtime::CreateDfxController(const RuntimeOptions &options)
{
    DfxController::Initialize();
    DfxController::SetOptionValue(DfxOptionHandler::COMPILER_NULLCHECK, options.GetCompilerNullcheck());
    DfxController::SetOptionValue(DfxOptionHandler::REFERENCE_DUMP, options.GetReferenceDump());
    DfxController::SetOptionValue(DfxOptionHandler::SIGNAL_CATCHER, options.GetSignalCatcher());
    DfxController::SetOptionValue(DfxOptionHandler::SIGNAL_HANDLER, options.GetSignalHandler());
    DfxController::SetOptionValue(DfxOptionHandler::ARK_SIGQUIT, options.GetSigquitFlag());
    DfxController::SetOptionValue(DfxOptionHandler::ARK_SIGUSR1, options.GetSigusr1Flag());
    DfxController::SetOptionValue(DfxOptionHandler::ARK_SIGUSR2, options.GetSigusr2Flag());
    DfxController::SetOptionValue(DfxOptionHandler::MOBILE_LOG, options.GetMobileLogFlag());
    DfxController::SetOptionValue(DfxOptionHandler::DFXLOG, options.GetDfxLog());

    auto compiler_nullcheck_flag = DfxController::GetOptionValue(DfxOptionHandler::COMPILER_NULLCHECK);
    if (compiler_nullcheck_flag == 0) {
        panda::compiler::OPTIONS.SetCompilerImplicitNullCheck(false);
    }
}

void Runtime::BlockSignals()
{
    sigset_t set;
    if (sigemptyset(&set) == -1) {
        LOG(ERROR, RUNTIME) << "sigemptyset failed";
        return;
    }
    int rc = 0;
    rc += sigaddset(&set, SIGPIPE);
#ifdef PANDA_TARGET_MOBILE
    rc += sigaddset(&set, SIGQUIT);
    rc += sigaddset(&set, SIGUSR1);
    rc += sigaddset(&set, SIGUSR2);
#endif  // PANDA_TARGET_MOBILE
    if (rc < 0) {
        LOG(ERROR, RUNTIME) << "sigaddset failed";
        return;
    }

    if (os::native_stack::g_PandaThreadSigmask(SIG_BLOCK, &set, nullptr) != 0) {
        LOG(ERROR, RUNTIME) << "g_PandaThreadSigmask failed";
    }
}

void Runtime::DumpForSigQuit(std::ostream &os)
{
    os << "\n";
    os << "-> Dump class loaders\n";
    class_linker_->EnumerateContextsForDump(
        [](ClassLinkerContext *ctx, std::ostream &stream, ClassLinkerContext *parent) {
            ctx->Dump(stream);
            return ctx->FindClassLoaderParent(parent);
        },
        os);
    os << "\n";

    // dump GC
    os << "-> Dump GC\n";
    os << GetFinalStatistics();
    os << "\n";

    // dump memory management
    os << "-> Dump memory management\n";
    os << GetMemoryStatistics();
    os << "\n";

    // dump PandaVM
    os << "-> Dump Ark VM\n";
    panda_vm_->DumpForSigQuit(os);
    os << "\n";

    WRITE_RELAYOUT_PROFILE_DATA();
}

void Runtime::InitNonZygoteOrPostFork(bool is_system_server, [[maybe_unused]] const char *isa,
                                      const std::function<void()> &init_hook,
                                      [[maybe_unused]] bool profile_system_server)
{
    is_zygote_ = false;

    // NOTE(00510180): wait NativeBridge ready

    // NOTE(00510180): wait profile ready

    // NOTE(00510180): wait ThreadPool ready

    // NOTE(00510180): wait ResetGcPerformanceInfo() ready

    panda_vm_->PreStartup();

    init_hook();

    mem::GcHung::InitPostFork(is_system_server);
}

void Runtime::PreZygoteFork()
{
    panda_vm_->PreZygoteFork();
}

void Runtime::PostZygoteFork()
{
    panda_vm_->PostZygoteFork();
}

// Returns true if profile saving is enabled. GetJit() will be not null in this case.
bool Runtime::SaveProfileInfo() const
{
    return save_profiling_info_;
}

void Runtime::CheckOptionsFromOs() const
{
#if !defined(PANDA_QEMU_BUILD) && !defined(PANDA_TARGET_MOBILE)
    // for qemu-aarch64 we will get 32 from GetCacheLineSize()
    // for native arm and qemu-arm we will get 0 from GetCacheLineSize()
    ASSERT(panda::CACHE_LINE_SIZE == os::mem::GetCacheLineSize());
#endif
}

}  // namespace panda
