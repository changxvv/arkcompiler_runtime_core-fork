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

#include "libpandabase/os/native_stack.h"
#include "monitor.h"
#include "os/mutex.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread_scopes.h"
#include "utils/logger.h"
#include "utils/span.h"
#include "verification/plugins.h"
#include "verification/util/is_system.h"
#include "verification/util/optional_ref.h"
#include "generated/base_options.h"

// generated
#include "ark_version.h"
#include "generated/verifier_options_gen.h"

#include <csignal>
#include <chrono>
#include <thread>

namespace ark::verifier {

size_t const MAX_THREADS = 64;

void Worker(PandaDeque<Method *> *queue, os::memory::Mutex *lock, size_t threadNum, std::atomic<bool> *result)
{
    LOG(DEBUG, VERIFIER) << "Verifier thread " << threadNum << " starting";
    {
        std::stringstream ss;
        ss << "Verifier #" << threadNum;
        if (os::thread::SetThreadName(os::thread::GetNativeHandle(), ss.str().c_str()) != 0) {
            LOG(ERROR, VERIFIER) << "Failed to set worker thread name " << ss.str();
        }
    }

    auto *service = Runtime::GetCurrent()->GetVerifierService();
    auto options = Runtime::GetCurrent()->GetOptions();
    auto mode = options.GetVerificationMode();

    ManagedThread *thread = nullptr;
    panda_file::SourceLang currentLang = panda_file::SourceLang::INVALID;
    for (;;) {
        Method *method;
        {
            os::memory::LockHolder lh {*lock};
            if (queue->empty()) {
                break;
            }
            method = queue->front();
            queue->pop_front();
        }
        auto methodLang = method->GetClass()->GetSourceLang();
        if (methodLang != currentLang) {
            if (thread != nullptr) {
                plugin::GetLanguagePlugin(currentLang)->DestroyManagedThread(thread);
            }
            thread = plugin::GetLanguagePlugin(methodLang)->CreateManagedThread();
            currentLang = methodLang;
        }
        if (ark::verifier::Verify(service, method, mode) != Status::OK) {
            *result = false;
            LOG(ERROR, VERIFIER) << "Error: method " << method->GetFullName(true) << " failed to verify";
        }
    }
    if (thread != nullptr) {
        plugin::GetLanguagePlugin(currentLang)->DestroyManagedThread(thread);
    }
    LOG(DEBUG, VERIFIER) << "Verifier thread " << threadNum << " finishing";
}

bool RunVerifier(const Options &cliOptions)
{
    auto isPerfMeasure = cliOptions.IsPerfMeasure();
    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end;

    PandaDeque<Method *> queue;
    os::memory::Mutex lock;

    auto &runtime = *Runtime::GetCurrent();
    auto &classLinker = *runtime.GetClassLinker();

    const std::vector<std::string> &classNames = cliOptions.GetClasses();
    const std::vector<std::string> &methodNames = cliOptions.GetMethods();

    std::atomic<bool> result = true;

    PandaUnorderedSet<Method *> methodsSet;

    auto enqueueMethod = [&methodsSet, &queue](Method *method) {
        if (methodsSet.count(method) == 0) {
            queue.push_back(method);
            methodsSet.insert(method);
        }
    };

    bool verifyLibraries = runtime.GetOptions().IsVerifyRuntimeLibraries();

    auto enqueueClass = [&verifyLibraries, &enqueueMethod](const Class &klass) {
        if (!verifyLibraries && IsSystemClass(&klass)) {
            LOG(INFO, VERIFIER) << klass.GetName() << " is a system class, skipping";
            return;
        }
        LOG(INFO, VERIFIER) << "Begin verification of class " << klass.GetName();
        for (auto &method : klass.GetMethods()) {
            enqueueMethod(&method);
        }
    };

    // we need ScopedManagedCodeThread for the verifier since it can allocate objects
    {
        ScopedManagedCodeThread managedObjThread(ManagedThread::GetCurrent());
        if (classNames.empty() && methodNames.empty()) {
            auto handleFile = [&result, &classLinker, &enqueueClass](const panda_file::File &file) {
                LOG(INFO, VERIFIER) << "Processing file" << file.GetFilename();
                for (auto id : file.GetClasses()) {
                    panda_file::File::EntityId entityId {id};
                    if (!file.IsExternal(entityId)) {
                        auto optLang = panda_file::ClassDataAccessor {file, entityId}.GetSourceLang();
                        if (optLang.has_value() && !IsValidSourceLang(optLang.value())) {
                            LOG(ERROR, VERIFIER) << "Unknown SourceLang";
                            result = false;
                            return false;
                        }
                        ClassLinkerExtension *ext =
                            classLinker.GetExtension(optLang.value_or(panda_file::SourceLang::PANDA_ASSEMBLY));
                        if (ext == nullptr) {
                            LOG(ERROR, VERIFIER) << "Error: Class Linker Extension failed to initialize";
                            result = false;
                            return false;
                        }
                        const Class *klass = ext->GetClass(file, entityId);

                        if (klass != nullptr) {
                            enqueueClass(*klass);
                        }
                    }
                }
                return true;
            };

            classLinker.EnumeratePandaFiles(handleFile);

            if (verifyLibraries) {
                classLinker.EnumerateBootPandaFiles(handleFile);
            } else if (runtime.GetPandaFiles().empty()) {
                // in this case the last boot-panda-file and only it is actually not a system file and should be
                // verified
                OptionalConstRef<panda_file::File> file;
                classLinker.EnumerateBootPandaFiles([&file](const panda_file::File &pf) {
                    file = std::cref(pf);
                    return true;
                });
                if (file.HasRef()) {
                    handleFile(*file);
                } else {
                    LOG(ERROR, VERIFIER) << "No files given to verify";
                }
            }
        } else {
            PandaUnorderedMap<std::string, Class *> classesByName;
            ClassLinkerContext *ctx =
                classLinker.GetExtension(runtime.GetLanguageContext(runtime.GetRuntimeType()))->GetBootContext();

            auto getClassByName = [&classesByName, &classLinker, &ctx,
                                   &result](const std::string &className) -> Class * {
                auto it = classesByName.find(className);
                if (it != classesByName.end()) {
                    return it->second;
                }

                PandaString descriptor;
                const uint8_t *classNameBytes =
                    ClassHelper::GetDescriptor(utf::CStringAsMutf8(className.c_str()), &descriptor);
                Class *klass = classLinker.GetClass(classNameBytes, true, ctx);
                if (klass == nullptr) {
                    LOG(ERROR, VERIFIER) << "Error: Cannot resolve class with name " << className;
                    result = false;
                }

                classesByName.emplace(className, klass);
                return klass;
            };

            for (const auto &className : classNames) {
                Class *klass = getClassByName(className);
                // the bad case is already handled in get_class_by_name
                if (klass != nullptr) {
                    enqueueClass(*klass);
                }
            }

            for (const std::string &fqMethodName : methodNames) {
                size_t pos = fqMethodName.find_last_of("::");
                if (pos == std::string::npos) {
                    LOG(ERROR, VERIFIER) << "Error: Fully qualified method name must contain '::', was "
                                         << fqMethodName;
                    result = false;
                    break;
                }
                std::string className = fqMethodName.substr(0, pos - 1);
                std::string_view unqualifiedMethodName = std::string_view(fqMethodName).substr(pos + 1);
                if (std::find(classNames.begin(), classNames.end(), className) != classNames.end()) {
                    // this method was already verified while enumerating class_names
                    continue;
                }
                Class *klass = getClassByName(className);
                if (klass != nullptr) {
                    bool methodFound = false;
                    for (auto &method : klass->GetMethods()) {
                        const char *nameData = utf::Mutf8AsCString(method.GetName().data);
                        if (std::string_view(nameData) == unqualifiedMethodName) {
                            methodFound = true;
                            LOG(INFO, VERIFIER) << "Verification of method '" << fqMethodName << "'";
                            enqueueMethod(&method);
                        }
                    }
                    if (!methodFound) {
                        LOG(ERROR, VERIFIER) << "Error: Cannot resolve method with name " << unqualifiedMethodName
                                             << " in class " << className;
                        result = false;
                    }
                }
            }
        }
    }

    if (isPerfMeasure) {
        begin = std::chrono::steady_clock::now();
    }

    size_t nthreads = runtime.GetOptions().GetVerificationThreads();
    std::vector<std::thread *> threads;
    for (size_t i = 0; i < nthreads; i++) {
        auto *worker = runtime.GetInternalAllocator()->New<std::thread>(Worker, &queue, &lock, i, &result);
        threads.push_back(worker);
    }

    for (auto *thr : threads) {
        thr->join();
        runtime.GetInternalAllocator()->Delete(thr);
    }

    if (isPerfMeasure) {
        end = std::chrono::steady_clock::now();
        std::cout << "Verification time = "
                  << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << " us" << std::endl;
    }

    return result;
}

void PrintHelp(const PandArgParser &paParser)
{
    std::string error = paParser.GetErrorString();
    if (!error.empty()) {
        error += "\n\n";
    }
    std::cerr << error << "Usage: verifier [option...] [file]\n"
              << "Verify specified Panda files (given by file and --panda-files) "
              << "or certain classes/methods in them.\n\n"
              << paParser.GetHelpString() << std::endl;
}

int Main(int argc, const char **argv)
{
    Span<const char *> sp(argv, argc);
    RuntimeOptions runtimeOptions(sp[0]);
    Options cliOptions(sp[0]);
    PandArgParser paParser;
    base_options::Options baseOptions("");

    PandArg<bool> help("help", false, "Print this message and exit");
    PandArg<bool> options("options", false, "Print verifier options");
    // tail argument
    PandArg<std::string> file("file", "", "path to pandafile");

    cliOptions.AddOptions(&paParser);

    paParser.Add(&help);
    paParser.Add(&options);
    paParser.PushBackTail(&file);
    paParser.EnableTail();

    if (!paParser.Parse(argc, argv)) {
        PrintHelp(paParser);
        return 1;
    }

    if (runtimeOptions.IsVersion()) {
        PrintPandaVersion();
        return 0;
    }

    if (help.GetValue()) {
        PrintHelp(paParser);
        return 0;
    }

    if (options.GetValue()) {
        std::cout << paParser.GetRegularArgs() << std::endl;
        return 0;
    }

    auto cliOptionsErr = cliOptions.Validate();
    if (cliOptionsErr) {
        std::cerr << "Error: " << cliOptionsErr.value().GetMessage() << std::endl;
        return 1;
    }

    auto bootPandaFiles = cliOptions.GetBootPandaFiles();
    auto pandaFiles = cliOptions.GetPandaFiles();
    std::string mainFileName = file.GetValue();

    if (!mainFileName.empty()) {
        if (pandaFiles.empty()) {
            bootPandaFiles.push_back(mainFileName);
        } else {
            auto foundIter = std::find_if(pandaFiles.begin(), pandaFiles.end(),
                                          [&mainFileName](auto &fileName) { return mainFileName == fileName; });
            if (foundIter == pandaFiles.end()) {
                pandaFiles.push_back(mainFileName);
            }
        }
    }

    runtimeOptions.SetBootPandaFiles(bootPandaFiles);
    runtimeOptions.SetPandaFiles(pandaFiles);
    runtimeOptions.SetLoadRuntimes(cliOptions.GetLoadRuntimes());
    runtimeOptions.SetGcType(cliOptions.GetGcType());

    baseOptions.SetLogComponents(cliOptions.GetLogComponents());
    baseOptions.SetLogLevel(cliOptions.GetLogLevel());
    baseOptions.SetLogStream(cliOptions.GetLogStream());
    baseOptions.SetLogFile(cliOptions.GetLogFile());
    Logger::Initialize(baseOptions);

    runtimeOptions.SetLimitStandardAlloc(cliOptions.IsLimitStandardAlloc());
    runtimeOptions.SetInternalAllocatorType(cliOptions.GetInternalAllocatorType());
    runtimeOptions.SetInternalMemorySizeLimit(cliOptions.GetInternalMemorySizeLimit());

    runtimeOptions.SetVerificationMode(cliOptions.IsDebugMode() ? VerificationMode::DEBUG
                                                                : VerificationMode::AHEAD_OF_TIME);
    runtimeOptions.SetVerificationConfigFile(cliOptions.GetConfigFile());
    runtimeOptions.SetVerificationCacheFile(cliOptions.GetCacheFile());
    runtimeOptions.SetVerificationUpdateCache(cliOptions.IsUpdateCache());
    runtimeOptions.SetVerifyRuntimeLibraries(cliOptions.IsVerifyRuntimeLibraries());
    uint32_t threads = cliOptions.GetThreads();
    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
        // hardware_concurrency can return 0 if the value is not computable or well defined
        if (threads == 0) {
            threads = 1;
        } else if (threads > MAX_THREADS) {
            threads = MAX_THREADS;
        }
    }
    runtimeOptions.SetVerificationThreads(threads);

    if (!Runtime::Create(runtimeOptions)) {
        std::cerr << "Error: cannot create runtime" << std::endl;
        return -1;
    }

    int ret = RunVerifier(cliOptions) ? 0 : -1;

    if (cliOptions.IsPrintMemoryStatistics()) {
        std::cout << Runtime::GetCurrent()->GetMemoryStatistics();
    }
    if (!Runtime::Destroy()) {
        std::cerr << "Error: cannot destroy runtime" << std::endl;
        return -1;
    }
    return ret;
}

}  // namespace ark::verifier

int main(int argc, const char **argv)
{
    return ark::verifier::Main(argc, argv);
}
