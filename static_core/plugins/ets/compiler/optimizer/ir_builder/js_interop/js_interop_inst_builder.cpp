/*
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

#include "optimizer/ir_builder/inst_builder.h"
#include "optimizer/ir_builder/ir_builder.h"
#include "optimizer/ir/inst.h"
#include "bytecode_instruction.h"
#include "bytecode_instruction-inl.h"

namespace ark::compiler {

template <RuntimeInterface::IntrinsicId ID, DataType::Type RET_TYPE, DataType::Type... PARAM_TYPES>
struct IntrinsicBuilder {
    static constexpr size_t N = sizeof...(PARAM_TYPES);
    template <typename... ARGS>
    static IntrinsicInst *Build(InstBuilder *ib, size_t pc, const ARGS &...inputs)
    {
        static_assert(sizeof...(inputs) == N + 1);
        return ib->BuildInteropIntrinsic<N>(pc, ID, RET_TYPE, {PARAM_TYPES...}, {inputs...});
    }
};

using IntrinsicCompilerConvertJSValueToLocal =
    IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_CONVERT_JS_VALUE_TO_LOCAL, DataType::POINTER,
                     DataType::REFERENCE>;
using IntrinsicCompilerResolveQualifiedJSCall =
    IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_RESOLVE_QUALIFIED_JS_CALL, DataType::POINTER,
                     DataType::POINTER, DataType::REFERENCE>;
using IntrinsicCompilerGetJSNamedProperty =
    IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_GET_JS_NAMED_PROPERTY, DataType::POINTER,
                     DataType::POINTER, DataType::POINTER>;
using IntrinsicCompilerConvertLocalToJSValue =
    IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_CONVERT_LOCAL_TO_JS_VALUE, DataType::REFERENCE,
                     DataType::POINTER>;
using IntrinsicCompilerCreateLocalScope =
    IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_CREATE_LOCAL_SCOPE, DataType::VOID>;
using IntrinsicCompilerDestroyLocalScope =
    IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_DESTROY_LOCAL_SCOPE, DataType::VOID>;
using IntrinsicCompilerJSCallCheck = IntrinsicBuilder<RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_JS_CALL_CHECK,
                                                      DataType::POINTER, DataType::POINTER>;

template <size_t N>
IntrinsicInst *InstBuilder::BuildInteropIntrinsic(size_t pc, RuntimeInterface::IntrinsicId id, DataType::Type retType,
                                                  const std::array<DataType::Type, N> &types,
                                                  const std::array<Inst *, N + 1> &inputs)
{
    auto saveState = CreateSaveState(Opcode::SaveState, pc);
    AddInstruction(saveState);
    auto intrinsic = GetGraph()->CreateInstIntrinsic(retType, pc, id);
    intrinsic->AllocateInputTypes(GetGraph()->GetAllocator(), N + 1);
    for (size_t i = 0; i < N; ++i) {
        intrinsic->AppendInputAndType(inputs[i], types[i]);
    }
    intrinsic->AppendInputAndType(inputs[N], DataType::NO_TYPE);  // SaveState input
    AddInstruction(intrinsic);
    return intrinsic;
}

std::pair<Inst *, Inst *> InstBuilder::BuildResolveInteropCallIntrinsic(RuntimeInterface::InteropCallKind callKind,
                                                                        size_t pc, RuntimeInterface::MethodPtr method,
                                                                        Inst *arg0, Inst *arg1,
                                                                        SaveStateInst *saveState)
{
    IntrinsicInst *jsThis = nullptr;
    IntrinsicInst *jsFn = nullptr;
    if (callKind == RuntimeInterface::InteropCallKind::CALL_BY_VALUE) {
        jsFn = IntrinsicCompilerConvertJSValueToLocal::Build(this, pc, arg0, saveState);
        jsThis = IntrinsicCompilerConvertJSValueToLocal::Build(this, pc, arg1, saveState);
    } else {
        auto jsVal = IntrinsicCompilerConvertJSValueToLocal::Build(this, pc, arg0, saveState);

        auto strId = arg1->CastToLoadString()->GetTypeId();
        jsThis = IntrinsicCompilerResolveQualifiedJSCall::Build(this, pc, jsVal, arg1, saveState);

        Inst *propName = nullptr;
        if (!GetGraph()->IsAotMode()) {
            auto propNameStr = GetGraph()->GetRuntime()->GetFuncPropName(method, strId);
            propName = GetGraph()->CreateInstLoadImmediate(DataType::POINTER, pc, propNameStr,
                                                           LoadImmediateInst::ObjectType::STRING);
        } else {
            auto propNameStrOffset = GetGraph()->GetRuntime()->GetFuncPropNameOffset(method, strId);
            propName = GetGraph()->CreateInstLoadImmediate(DataType::POINTER, pc, propNameStrOffset,
                                                           LoadImmediateInst::ObjectType::PANDA_FILE_OFFSET);
        }
        AddInstruction(propName);

        jsFn = IntrinsicCompilerGetJSNamedProperty::Build(this, pc, jsThis, propName, saveState);
    }
    return {jsThis, jsFn};
}

void InstBuilder::BuildReturnValueConvertInteropIntrinsic(RuntimeInterface::InteropCallKind callKind, size_t pc,
                                                          RuntimeInterface::MethodPtr method, Inst *jsCall,
                                                          SaveStateInst *saveState)
{
    if (callKind == RuntimeInterface::InteropCallKind::NEW_INSTANCE) {
        auto ret = IntrinsicCompilerConvertLocalToJSValue::Build(this, pc, jsCall, saveState);
        UpdateDefinitionAcc(ret);
    } else {
        auto retIntrinsicId = GetGraph()->GetRuntime()->GetInfoForInteropCallRetValueConversion(method);
        if (retIntrinsicId.has_value()) {
            Inst *ret = nullptr;
            auto [id, retType] = retIntrinsicId.value();
            if (id == RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_CONVERT_LOCAL_TO_REF_TYPE) {
                auto loadClass = BuildLoadClass(GetRuntime()->GetMethodReturnTypeId(method), pc, saveState);
                AddInstruction(loadClass);
                // LoadClass returns ref, create a new SaveState
                saveState = CreateSaveState(Opcode::SaveState, pc);
                AddInstruction(saveState);
                ret = BuildInteropIntrinsic<2U>(pc, id, retType, {DataType::REFERENCE, DataType::POINTER},
                                                {loadClass, jsCall, saveState});
            } else {
                ret = BuildInteropIntrinsic<1>(pc, id, retType, {DataType::POINTER}, {jsCall, saveState});
            }
            UpdateDefinitionAcc(ret);
        }
    }
}

void InstBuilder::BuildInteropCall(const BytecodeInstruction *bcInst, RuntimeInterface::InteropCallKind callKind,
                                   RuntimeInterface::MethodPtr method, bool isRange, bool accRead)
{
    auto pc = GetPc(bcInst->GetAddress());
    auto saveState = CreateSaveState(Opcode::SaveState, pc);
    AddInstruction(saveState);

    // Create LOCAL scope
    IntrinsicCompilerCreateLocalScope::Build(this, pc, saveState);
    // Resolve call target
    auto [jsThis, jsFn] =
        BuildResolveInteropCallIntrinsic(callKind, pc, method, GetArgDefinition(bcInst, 0, accRead, isRange),
                                         GetArgDefinition(bcInst, 1, accRead, isRange), saveState);
    // js call check
    auto jsCallCheck = IntrinsicCompilerJSCallCheck::Build(this, pc, jsFn, saveState);

    // js call
    RuntimeInterface::IntrinsicId callId = RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_JS_CALL_FUNCTION;
    if (callKind == RuntimeInterface::InteropCallKind::NEW_INSTANCE) {
        callId = RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_JS_NEW_INSTANCE;
    }
    auto jsCall = GetGraph()->CreateInstIntrinsic(DataType::POINTER, pc, callId);
    ArenaVector<std::pair<RuntimeInterface::IntrinsicId, DataType::Type>> intrinsicsIds(
        GetGraph()->GetLocalAllocator()->Adapter());
    GetGraph()->GetRuntime()->GetInfoForInteropCallArgsConversion(method, &intrinsicsIds);
    if (callKind != RuntimeInterface::InteropCallKind::NEW_INSTANCE) {
        jsCall->AllocateInputTypes(GetGraph()->GetAllocator(), intrinsicsIds.size() + 4U);
        jsCall->AppendInputs({{jsThis, DataType::POINTER},
                              {jsCallCheck, DataType::POINTER},
                              {GetGraph()->FindOrCreateConstant(intrinsicsIds.size()), DataType::UINT32}});
    } else {
        jsCall->AllocateInputTypes(GetGraph()->GetAllocator(), intrinsicsIds.size() + 3U);
        jsCall->AppendInputs({{jsCallCheck, DataType::POINTER},
                              {GetGraph()->FindOrCreateConstant(intrinsicsIds.size()), DataType::UINT32}});
    }

    // Convert args
    size_t argIdx = 0;
    for (auto [intrinsicId, type] : intrinsicsIds) {
        Inst *arg = nullptr;
        if (type != DataType::NO_TYPE) {
            arg = BuildInteropIntrinsic<1>(pc, intrinsicId, DataType::POINTER, {type},
                                           {GetArgDefinition(bcInst, argIdx + 2U, accRead, isRange), saveState});
        } else {
            arg = BuildInteropIntrinsic<0>(pc, intrinsicId, DataType::POINTER, {}, {saveState});
        }
        jsCall->AppendInputAndType(arg, DataType::POINTER);
        argIdx++;
    }

    jsCall->AppendInputAndType(saveState, DataType::NO_TYPE);
    AddInstruction(jsCall);

    // Convert ret value
    BuildReturnValueConvertInteropIntrinsic(callKind, pc, method, jsCall, saveState);
    // Create new a SaveState because instruction with ref value was built
    saveState = CreateSaveState(Opcode::SaveState, pc);
    AddInstruction(saveState);
    // Destroy handle scope
    IntrinsicCompilerDestroyLocalScope::Build(this, pc, saveState);
}

bool InstBuilder::TryBuildInteropCall(const BytecodeInstruction *bcInst, bool isRange, bool accRead)
{
    auto methodId = GetRuntime()->ResolveMethodIndex(GetMethod(), bcInst->GetId(0).AsIndex());
    auto method = GetRuntime()->GetMethodById(GetMethod(), methodId);
    if (g_options.IsCompilerEnableFastInterop()) {
        auto interopCallKind = GetRuntime()->GetInteropCallKind(method);
        if (interopCallKind != RuntimeInterface::InteropCallKind::UNKNOWN) {
            BuildInteropCall(bcInst, interopCallKind, method, isRange, accRead);
            return true;
        }
    }
    return false;
}
}  // namespace ark::compiler
