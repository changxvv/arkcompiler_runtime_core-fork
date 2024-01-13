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

#include "compiler/code_info/code_info.h"
#include "compiler/optimizer/code_generator/relocations.h"
#include "events/events.h"
#include "optimizer/ir/graph.h"
#include "runtime/include/method.h"
#include "compiler_options.h"
#include "target_machine_builder.h"

#include "llvm_irtoc_compiler.h"
#include "llvm_logger.h"
#include "llvm_options.h"
#include "mir_compiler.h"

#include "lowering/llvm_ir_constructor.h"
#include "transforms/passes/inline_ir/patch_return_handler_stack_adjustment.h"

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/Passes.h>
// Suppress warning about forward Pass declaration defined in another namespace
#include <llvm/Pass.h>
#include <llvm/Support/FileSystem.h>

using panda::compiler::LLVMIrConstructor;

namespace panda::llvmbackend {

std::unique_ptr<IrtocCompilerInterface> CreateLLVMIrtocCompiler(panda::compiler::RuntimeInterface *runtime,
                                                                panda::ArenaAllocator *allocator, panda::Arch arch)
{
    return std::make_unique<LLVMIrtocCompiler>(runtime, allocator, arch, "irtoc_file_name.hack");
}

LLVMIrtocCompiler::LLVMIrtocCompiler(panda::compiler::RuntimeInterface *runtime, panda::ArenaAllocator *allocator,
                                     panda::Arch arch, std::string filename)
    : LLVMCompiler(arch),
      methods_(allocator->Adapter()),
      filename_(std::move(filename)),
      arkInterface_(runtime, GetTripleForArch(arch))
{
    InitializeSpecificLLVMOptions(arch);
    auto llvmCompilerOptions = InitializeLLVMCompilerOptions();

    // clang-format off
    targetMachine_ = cantFail(panda::llvmbackend::TargetMachineBuilder {}
                                .SetCPU(GetCPUForArch(arch))
                                .SetOptLevel(static_cast<llvm::CodeGenOpt::Level>(llvmCompilerOptions.optlevel))
                                .SetFeatures(GetFeaturesForArch(GetArch()))
                                .SetTriple(GetTripleForArch(GetArch()))
                                .Build());
    // clang-format on
    mirCompiler_ = std::make_unique<MIRCompiler>(
        targetMachine_, [this](panda::llvmbackend::InsertingPassManager *manager) -> void {
            manager->InsertBefore(&llvm::FEntryInserterID,
                                  panda::llvmbackend::CreatePatchReturnHandlerStackAdjustmentPass(&arkInterface_));
        });
    optimizer_ = std::make_unique<panda::llvmbackend::LLVMOptimizer>(llvmCompilerOptions, &arkInterface_,
                                                                     mirCompiler_->GetTargetMachine());
    InitializeModule();

    debugData_ = std::make_unique<DebugDataBuilder>(module_.get(), filename_);
}

std::vector<std::string> LLVMIrtocCompiler::GetFeaturesForArch(Arch arch)
{
    if (arch == Arch::X86_64 && panda::compiler::g_options.IsCpuFeatureEnabled(compiler::SSE42)) {
        return {std::string("+sse4.2")};
    }
    return {};
}

static llvm::CallingConv::ID GetFastPathCallingConv(uint32_t numArgs)
{
    ASSERT(numArgs <= 5U);
    switch (numArgs) {
        case 1U:
            return llvm::CallingConv::ArkFast1;
        case 2U:
            return llvm::CallingConv::ArkFast2;
        case 3U:
            return llvm::CallingConv::ArkFast3;
        case 4U:
            return llvm::CallingConv::ArkFast4;
        case 5U:
            return llvm::CallingConv::ArkFast5;
        default:
            UNREACHABLE();
    }
}

bool LLVMIrtocCompiler::AddGraph(compiler::Graph *graph)
{
    ASSERT(graph != nullptr);
    ASSERT(graph->GetArch() == GetArch());
    ASSERT(!graph->SupportManagedCode());
    auto method = graph->GetMethod();

    LLVMIrConstructor ctor(graph, module_.get(), GetLLVMContext(), &arkInterface_, debugData_);
    auto llvmFunction = ctor.GetFunc();
    if (graph->GetMode().IsInterpreter()) {
        llvmFunction->setCallingConv(llvm::CallingConv::ArkInt);
    } else if (graph->GetMode().IsFastPath()) {
        // Excluding fake thread and frame arguments
        uint32_t constexpr FAKE_ARGS_NUM = 2U;
        ASSERT(llvmFunction->arg_size() >= FAKE_ARGS_NUM);
        if (llvmFunction->arg_size() - FAKE_ARGS_NUM == 0) {
            if (llvmFunction->getReturnType()->isVoidTy()) {
                llvmFunction->setCallingConv(llvm::CallingConv::ArkFast0);
            } else {
                llvmFunction->setCallingConv(llvm::CallingConv::ArkFast1);
            }
        } else {
            llvmFunction->setCallingConv(GetFastPathCallingConv(llvmFunction->arg_size() - FAKE_ARGS_NUM));
        }
        llvmFunction->addFnAttr("target-features", GetFastPathFeatures());
    }

    bool builtIr = ctor.BuildIr();
    if (!builtIr) {
        irFailed_ = true;
        LLVM_LOG(ERROR, INFRA) << "LLVM failed to build IR for method " << arkInterface_.GetUniqMethodName(method);
        llvmFunction->deleteBody();
        return false;
    }

    LLVM_LOG(DEBUG, INFRA) << "LLVM built LLVM IR for method  " << arkInterface_.GetUniqMethodName(method);
    methods_.push_back(static_cast<Method *>(method));
    return true;
}

Expected<bool, std::string> LLVMIrtocCompiler::CanCompile(panda::compiler::Graph *graph)
{
    LLVM_LOG(DEBUG, INFRA) << "LLVM checking graph for method " << arkInterface_.GetUniqMethodName(graph->GetMethod());
    return LLVMIrConstructor::CanCompile(graph);
}

void LLVMIrtocCompiler::CompileAll()
{
    // Compile even if there are no methods because we have to produce an object file, even an empty one
    ASSERT_PRINT(!HasCompiledCode(), "Cannot compile twice");

    optimizer_->OptimizeModule(module_.get());
    debugData_->Finalize();
    objectFile_ = exitOnErr_(mirCompiler_->CompileModule(*module_));
}

std::string LLVMIrtocCompiler::GetFastPathFeatures() const
{
    std::string features;
    for (const auto &feature : GetFeaturesForArch(GetArch())) {
        features.append(feature).append(",");
    }
    // FastPath may use FP register. We should be ready for this
    switch (GetArch()) {
        case Arch::AARCH64:
            features.append("+reserve-").append(arkInterface_.GetFramePointerRegister());
            features.append(",");
            features.append("+reserve-").append(arkInterface_.GetThreadRegister());
            break;
        default:
            UNREACHABLE();
    }
    return features;
}

void LLVMIrtocCompiler::InitializeSpecificLLVMOptions(Arch arch)
{
    if (arch == Arch::X86_64) {
        SetLLVMOption("x86-use-base-pointer", false);
    }
    if (arch == Arch::AARCH64) {
        SetLLVMOption("aarch64-enable-ptr32", true);
    }
    SetLLVMOption("inline-remark-attribute", true);
}

void LLVMIrtocCompiler::InitializeModule()
{
    auto moduleFile = llvmbackend::g_options.GetLlvmInlineModule();
    auto layout = targetMachine_->createDataLayout();
    if (moduleFile.empty()) {
        module_ = std::make_unique<llvm::Module>("irtoc empty module", *GetLLVMContext());
        module_->setDataLayout(layout);
        module_->setTargetTriple(GetTripleForArch(GetArch()).getTriple());
        return;
    }
    auto buffer = errorOrToExpected(llvm::MemoryBuffer::getFile(moduleFile));
    LLVM_LOG_IF(!buffer, FATAL, INFRA) << "Could not read inline module from file = '" << moduleFile << "', error: '"
                                       << toString(buffer.takeError()) << "'";

    auto contents = llvm::getBitcodeFileContents(*buffer.get());
    LLVM_LOG_IF(!contents, FATAL, INFRA) << "Could get bitcode file contents from file = '" << moduleFile
                                         << "', error: '" << toString(contents.takeError()) << "'";

    static constexpr int32_t EXPECTED_MODULES = 1;
    LLVM_LOG_IF(contents->Mods.size() != EXPECTED_MODULES, FATAL, INFRA)
        << "Inline module file '" << moduleFile << "' has unexpected number of modules = " << contents->Mods.size()
        << ", expected " << EXPECTED_MODULES;
    auto module = contents->Mods[0].parseModule(*GetLLVMContext());
    LLVM_LOG_IF(!module, FATAL, INFRA) << "Could not parse inline module from file '" << moduleFile << "', error: '"
                                       << toString(buffer.takeError()) << "'";
    module_ = std::move(*module);
    module_->setDataLayout(layout);
    optimizer_->ProcessInlineModule(module_.get());
}

void LLVMIrtocCompiler::WriteObjectFile(std::string_view output)
{
    ASSERT_PRINT(HasCompiledCode(), "Call CompileAll first");
    objectFile_->WriteTo(output);
}

size_t LLVMIrtocCompiler::GetObjectFileSize()
{
    return objectFile_->Size();
}

CompiledCode LLVMIrtocCompiler::GetCompiledCode(std::string_view functionName)
{
    ASSERT(objectFile_ != nullptr);
    auto reference = objectFile_->GetSectionByFunctionName(std::string {functionName});
    CompiledCode code {};
    code.size = reference.GetSize();
    code.code = reference.GetMemory();
    return code;
}
}  // namespace panda::llvmbackend
