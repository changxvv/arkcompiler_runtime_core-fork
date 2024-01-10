/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef LIBLLVMBACKEND_TRANSFORMS_PASSES_INLINE_IR_CLEANUP_INLINE_MODULE_H
#define LIBLLVMBACKEND_TRANSFORMS_PASSES_INLINE_IR_CLEANUP_INLINE_MODULE_H

#include <llvm/IR/PassManager.h>

namespace panda::llvmbackend {
struct LLVMCompilerOptions;
}  // namespace panda::llvmbackend

namespace panda::llvmbackend::passes {

class CleanupInlineModule : public llvm::PassInfoMixin<CleanupInlineModule> {
public:
    explicit CleanupInlineModule();

    CleanupInlineModule(CleanupInlineModule &&other);

    CleanupInlineModule &operator=(CleanupInlineModule &&other);

    CleanupInlineModule(CleanupInlineModule &) = delete;

    CleanupInlineModule &operator=(CleanupInlineModule &) = delete;

    ~CleanupInlineModule();

    static bool ShouldInsert(const panda::llvmbackend::LLVMCompilerOptions *options);

    // NOLINTNEXTLINE(readability-identifier-naming)
    llvm::PreservedAnalyses run(llvm::Module &module, llvm::ModuleAnalysisManager &analysis_manager);

private:
    class InlineModuleCleaner;
    std::unique_ptr<InlineModuleCleaner> cleaner_;

public:
    static constexpr llvm::StringRef ARG_NAME = "cleanup-inline-module";
};

}  // namespace panda::llvmbackend::passes

#endif  // LIBLLVMBACKEND_TRANSFORMS_PASSES_INLINE_IR_CLEANUP_INLINE_MODULE_H