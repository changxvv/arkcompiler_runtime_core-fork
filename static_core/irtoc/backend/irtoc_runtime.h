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

#ifndef PANDA_IRTOC_RUNTIME_H
#define PANDA_IRTOC_RUNTIME_H

#include <limits>

#include "compiler/optimizer/ir/runtime_interface.h"
#include "compilation.h"
#include "irtoc_interface_extensions_includes.inl.h"
#include "libpandabase/mem/gc_barrier.h"
#include "runtime/mem/gc/g1/g1-allocator_constants.h"
#include "runtime/include/hclass.h"
#include "libpandabase/utils/bit_utils.h"

namespace panda::irtoc {

class IrtocRuntimeInterface : public compiler::RuntimeInterface {
public:
    Function *MethodCast(MethodPtr method) const
    {
        return reinterpret_cast<Function *>(method);
    }

    std::string GetMethodName(MethodPtr method) const override
    {
        return MethodCast(method)->GetName();
    }

    std::string GetExternalMethodName(MethodPtr method, uint32_t external_id) const override
    {
        return reinterpret_cast<Function *>(method)->GetExternalFunction(external_id);
    }

    std::string GetMethodFullName(MethodPtr method, [[maybe_unused]] bool with_signature) const override
    {
        return std::string("Irtoc::") + MethodCast(method)->GetName();
    }

    std::string GetClassName([[maybe_unused]] ClassPtr klass) const override
    {
        return "Irtoc";
    }

    std::string GetClassNameFromMethod([[maybe_unused]] MethodPtr method) const override
    {
        return "Irtoc";
    }

    compiler::DataType::Type GetMethodReturnType(MethodPtr method) const override
    {
        auto unit = reinterpret_cast<Function *>(method);
        ASSERT(unit->GetGraph()->HasEndBlock());
        auto ret_type = compiler::DataType::NO_TYPE;
        for (auto exit : unit->GetGraph()->GetEndBlock()->GetPredsBlocks()) {
            if (exit->IsEmpty()) {
                continue;
            }
            auto last = exit->GetLastInst();
            if (last->IsReturn()) {
                ASSERT(ret_type == compiler::DataType::NO_TYPE || ret_type == last->GetType());
                ret_type = last->GetType();
            }
        }
        return ret_type;
    }

    size_t GetMethodTotalArgumentsCount(MethodPtr method) const override
    {
        return GetMethodArgumentsCount(method);
    }

    size_t GetMethodArgumentsCount(MethodPtr method) const override
    {
        auto unit = reinterpret_cast<Function *>(method);
        return unit->GetArgsCount();
    }

    ::panda::mem::BarrierType GetPreType() const override
    {
        return ::panda::mem::BarrierType::PRE_SATB_BARRIER;
    }

    ::panda::mem::BarrierType GetPostType() const override
    {
        return ::panda::mem::BarrierType::POST_INTERREGION_BARRIER;
    }

    ::panda::mem::BarrierOperand GetBarrierOperand([[maybe_unused]] ::panda::mem::BarrierPosition barrier_position,
                                                   [[maybe_unused]] std::string_view operand_name) const override
    {
        ASSERT(operand_name == "REGION_SIZE_BITS");
        uint8_t region_size_bits = helpers::math::GetIntLog2(panda::mem::G1_REGION_SIZE);
        return mem::BarrierOperand(mem::BarrierOperandType::UINT8_LITERAL, mem::BarrierOperandValue(region_size_bits));
    }

    size_t GetFieldOffset(FieldPtr field) const override
    {
        ASSERT((reinterpret_cast<uintptr_t>(field) & 1U) == 1U);
        return reinterpret_cast<uintptr_t>(field) >> 1U;
    }

    FieldPtr GetFieldByOffset(size_t offset) const override
    {
        ASSERT(MinimumBitsToStore(offset) < std::numeric_limits<uintptr_t>::digits);
        return reinterpret_cast<FieldPtr>((offset << 1U) | 1U);
    }

    SourceLanguage GetMethodSourceLanguage(MethodPtr method) const override
    {
        return MethodCast(method)->GetLanguage();
    }

    compiler::DataType::Type GetMethodTotalArgumentType(MethodPtr method, size_t index) const override
    {
        auto unit = reinterpret_cast<Function *>(method);
        for (auto param : unit->GetGraph()->GetParameters()) {
            if (param->CastToParameter()->GetArgNumber() == index) {
                return param->GetType();
            }
        }
        return compiler::DataType::NO_TYPE;
    }
    uint32_t GetCallableMask() const override
    {
        return HClass::GetCallableMask();
    }
#include "irtoc_interface_extensions.inl.h"
};

}  // namespace panda::irtoc

#endif  // PANDA_IRTOC_RUNTIME_H
