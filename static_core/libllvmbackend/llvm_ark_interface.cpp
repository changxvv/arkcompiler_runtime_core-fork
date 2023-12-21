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

#include "compiler/optimizer/ir/runtime_interface.h"
#include "runtime/include/method.h"
#include "events/events.h"
#include "compiler_options.h"
#include "utils/cframe_layout.h"
#include "llvm_ark_interface.h"
#include "llvm_logger.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm_options.h"

using panda::compiler::RuntimeInterface;

namespace {
constexpr panda::Arch LLVMArchToArkArch(llvm::Triple::ArchType arch)
{
    switch (arch) {
        case llvm::Triple::ArchType::aarch64:
            return panda::Arch::AARCH64;
        case llvm::Triple::ArchType::x86_64:
            return panda::Arch::X86_64;
        case llvm::Triple::ArchType::x86:
            return panda::Arch::X86;
        case llvm::Triple::ArchType::arm:
            return panda::Arch::AARCH32;
        default:
            UNREACHABLE();
            return panda::Arch::NONE;
    }
}
#include <entrypoints_gen.inl>
#include <intrinsics_gen.inl>
#include <intrinsic_names_gen.inl>
#include <entrypoints_llvm_ark_interface_gen.inl>
}  // namespace

namespace panda::llvmbackend {

panda::llvmbackend::LLVMArkInterface::LLVMArkInterface(RuntimeInterface *runtime, llvm::Triple triple)
    : runtime_(runtime), triple_(std::move(triple))
{
    ASSERT(runtime != nullptr);
}

const char *LLVMArkInterface::GetThreadRegister() const
{
    switch (LLVMArchToArkArch(triple_.getArch())) {
        case Arch::AARCH64: {
            constexpr auto EXPECTED_REGISTER = 28;
            static_assert(ArchTraits<Arch::AARCH64>::THREAD_REG == EXPECTED_REGISTER);
            return "x28";
        }
        case Arch::X86_64: {
            constexpr auto EXPECTED_REGISTER = 15;
            static_assert(ArchTraits<Arch::X86_64>::THREAD_REG == EXPECTED_REGISTER);
            return "r15";
        }
        default:
            UNREACHABLE();
    }
}

const char *LLVMArkInterface::GetFramePointerRegister() const
{
    switch (LLVMArchToArkArch(triple_.getArch())) {
        case Arch::AARCH64:
            return "x29";
        case Arch::X86_64:
            return "rbp";
        default:
            UNREACHABLE();
    }
}

uintptr_t LLVMArkInterface::GetEntrypointTlsOffset(EntrypointId id) const
{
    Arch ark_arch = LLVMArchToArkArch(triple_.getArch());
    using PandaEntrypointId = panda::compiler::RuntimeInterface::EntrypointId;
    return runtime_->GetEntrypointTlsOffset(ark_arch, static_cast<PandaEntrypointId>(id));
}

size_t LLVMArkInterface::GetTlsPreWrbEntrypointOffset() const
{
    Arch ark_arch = LLVMArchToArkArch(triple_.getArch());
    return runtime_->GetTlsPreWrbEntrypointOffset(ark_arch);
}

uint32_t LLVMArkInterface::GetManagedThreadPostWrbOneObjectOffset() const
{
    Arch ark_arch = LLVMArchToArkArch(triple_.getArch());
    return cross_values::GetManagedThreadPostWrbOneObjectOffset(ark_arch);
}

llvm::StringRef LLVMArkInterface::GetRuntimeFunctionName(LLVMArkInterface::RuntimeCallType call_type,
                                                         LLVMArkInterface::IntrinsicId id)
{
    if (call_type == LLVMArkInterface::RuntimeCallType::INTRINSIC) {
        return llvm::StringRef(GetIntrinsicRuntimeFunctionName(id));
    }
    // sanity check
    ASSERT(call_type == LLVMArkInterface::RuntimeCallType::ENTRYPOINT);
    return llvm::StringRef(GetEntrypointRuntimeFunctionName(id));
}

llvm::FunctionType *LLVMArkInterface::GetRuntimeFunctionType(llvm::StringRef name) const
{
    return runtime_function_types_.lookup(name);
}

llvm::FunctionType *LLVMArkInterface::GetOrCreateRuntimeFunctionType(llvm::LLVMContext &ctx, llvm::Module *module,
                                                                     LLVMArkInterface::RuntimeCallType call_type,
                                                                     LLVMArkInterface::IntrinsicId id)
{
    auto rt_function_name = GetRuntimeFunctionName(call_type, id);
    auto rt_function_ty = GetRuntimeFunctionType(rt_function_name);
    if (rt_function_ty != nullptr) {
        return rt_function_ty;
    }

    if (call_type == RuntimeCallType::INTRINSIC) {
        rt_function_ty = GetIntrinsicDeclaration(ctx, static_cast<RuntimeInterface::IntrinsicId>(id));
    } else {
        // sanity check
        ASSERT(call_type == RuntimeCallType::ENTRYPOINT);
        rt_function_ty = GetEntrypointDeclaration(ctx, module, static_cast<RuntimeInterface::EntrypointId>(id));
    }

    ASSERT(rt_function_ty != nullptr);
    runtime_function_types_.insert({rt_function_name, rt_function_ty});
    return rt_function_ty;
}

void LLVMArkInterface::RememberFunctionOrigin(const llvm::Function *function, MethodPtr method_ptr)
{
    ASSERT(function != nullptr);
    ASSERT(method_ptr != nullptr);

    auto file = static_cast<panda_file::File *>(runtime_->GetBinaryFileForMethod(method_ptr));
    [[maybe_unused]] auto insertion_result = function_origins_.insert({function, file});
    ASSERT(insertion_result.second);
    LLVM_LOG(DEBUG, INFRA) << function->getName().data() << " defined in " << runtime_->GetFileName(method_ptr);
}

LLVMArkInterface::RuntimeCallee LLVMArkInterface::GetEntrypointCallee(EntrypointId id) const
{
    using PandaEntrypointId = panda::compiler::RuntimeInterface::EntrypointId;
    auto eid = static_cast<PandaEntrypointId>(id);
    auto function_name = GetEntrypointInternalName(eid);
    auto function_proto = GetRuntimeFunctionType(function_name);
    ASSERT(function_proto != nullptr);
    return {function_proto, function_name};
}

llvm::Function *LLVMArkInterface::GetFunctionByMethodPtr(LLVMArkInterface::MethodPtr method) const
{
    ASSERT(method != nullptr);

    return functions_.lookup(method);
}

void LLVMArkInterface::PutFunction(LLVMArkInterface::MethodPtr method_ptr, llvm::Function *function)
{
    ASSERT(function != nullptr);
    ASSERT(method_ptr != nullptr);
    // We are not expecting `tan` functions other than we are adding in LLVMIrConstructor::EmitTan()
    ASSERT(function->getName() != "tan");

    [[maybe_unused]] auto f_insertion_result = functions_.insert({method_ptr, function});
    ASSERT_PRINT(f_insertion_result.first->second == function,
                 std::string("Attempt to map '") + GetUniqMethodName(method_ptr) + "' to a function = '" +
                     function->getName().str() + "', but the method_ptr already mapped to '" +
                     f_insertion_result.first->second->getName().str() + "'");
    auto source_lang = static_cast<uint8_t>(runtime_->GetMethodSourceLanguage(method_ptr));
    [[maybe_unused]] auto sl_insertion_result = source_languages_.insert({function, source_lang});
    ASSERT(sl_insertion_result.first->second == source_lang);
}

const char *LLVMArkInterface::GetIntrinsicRuntimeFunctionName(LLVMArkInterface::IntrinsicId id) const
{
    ASSERT(id >= 0 && id < static_cast<IntrinsicId>(RuntimeInterface::IntrinsicId::COUNT));
    return GetIntrinsicInternalName(static_cast<RuntimeInterface::IntrinsicId>(id));
}

const char *LLVMArkInterface::GetEntrypointRuntimeFunctionName(LLVMArkInterface::EntrypointId id) const
{
    ASSERT(id >= 0 && id < static_cast<EntrypointId>(RuntimeInterface::EntrypointId::COUNT));
    return GetEntrypointInternalName(static_cast<RuntimeInterface::EntrypointId>(id));
}

std::string LLVMArkInterface::GetUniqMethodName(LLVMArkInterface::MethodPtr method_ptr) const
{
    ASSERT(method_ptr != nullptr);

    if (IsIrtocMode()) {
        return runtime_->GetMethodName(method_ptr);
    }
#ifndef NDEBUG
    auto uniq_name = std::string(runtime_->GetMethodFullName(method_ptr, true));
    uniq_name.append("_id_");
    uniq_name.append(std::to_string(runtime_->GetUniqMethodId(method_ptr)));
#else
    std::stringstream ss_uniq_name;
    ss_uniq_name << "f_" << std::hex << method_ptr;
    auto uniq_name = ss_uniq_name.str();
#endif
    return uniq_name;
}

std::string LLVMArkInterface::GetUniqMethodName(const Method *method) const
{
    ASSERT(method != nullptr);

    auto casted = const_cast<Method *>(method);
    return GetUniqMethodName(static_cast<MethodPtr>(casted));
}

std::string LLVMArkInterface::GetUniqueBasicBlockName(const std::string &bb_name, const std::string &unique_suffix)
{
    std::stringstream unique_name;
    const std::string name_delimiter = "..";
    auto first = bb_name.find(name_delimiter);
    if (first == std::string::npos) {
        unique_name << bb_name << "_" << unique_suffix;
        return unique_name.str();
    }

    auto second = bb_name.rfind(name_delimiter);
    ASSERT(second != std::string::npos);

    unique_name << bb_name.substr(0, first + 2U) << unique_suffix << bb_name.substr(second, bb_name.size());

    return unique_name.str();
}

bool LLVMArkInterface::IsIrtocMode() const
{
    return true;
}

void LLVMArkInterface::AppendIrtocReturnHandler(llvm::StringRef return_handler)
{
    irtoc_return_handlers_.push_back(return_handler);
}

bool LLVMArkInterface::IsIrtocReturnHandler(const llvm::Function &function) const
{
    return std::find(irtoc_return_handlers_.cbegin(), irtoc_return_handlers_.cend(), function.getName()) !=
           irtoc_return_handlers_.cend();
}

}  // namespace panda::llvmbackend