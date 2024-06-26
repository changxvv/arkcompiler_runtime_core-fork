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

#ifndef COMPILER_TESTS_GRAPH_COMPARATOR_H
#define COMPILER_TESTS_GRAPH_COMPARATOR_H

#include "optimizer/ir/ir_constructor.h"
#include "optimizer/analysis/rpo.h"

namespace ark::compiler {

class GraphComparator {
public:
    bool Compare(Graph *graph1, Graph *graph2)
    {
        graph1->InvalidateAnalysis<Rpo>();
        graph2->InvalidateAnalysis<Rpo>();
        if (graph1->GetBlocksRPO().size() != graph2->GetBlocksRPO().size()) {
            std::cerr << "Different number of blocks\n";
            return false;
        }
        for (auto it1 = graph1->GetBlocksRPO().begin(), it2 = graph2->GetBlocksRPO().begin();
             it1 != graph1->GetBlocksRPO().end(); it1++, it2++) {
            auto it = bbMap_.insert({*it1, *it2});
            if (!it.second) {
                return false;
            }
        }
        return std::equal(graph1->GetBlocksRPO().begin(), graph1->GetBlocksRPO().end(), graph2->GetBlocksRPO().begin(),
                          graph2->GetBlocksRPO().end(), [this](auto bb1, auto bb2) { return Compare(bb1, bb2); });
    }

    bool Compare(BasicBlock *block1, BasicBlock *block2)
    {
        if (block1->GetPredsBlocks().size() != block2->GetPredsBlocks().size()) {
            std::cerr << "Different number of preds blocks\n";
            block1->Dump(&std::cerr);
            block2->Dump(&std::cerr);
            return false;
        }
        if (block1->GetSuccsBlocks().size() != block2->GetSuccsBlocks().size()) {
            std::cerr << "Different number of succs blocks\n";
            block1->Dump(&std::cerr);
            block2->Dump(&std::cerr);
            return false;
        }
        auto instCmp = [this](auto inst1, auto inst2) {
            ASSERT(inst2 != nullptr);
            bool t = Compare(inst1, inst2);
            if (!t) {
                std::cerr << "Different instructions:\n";
                inst1->Dump(&std::cerr);
                inst2->Dump(&std::cerr);
            }
            return t;
        };
        return std::equal(block1->AllInsts().begin(), block1->AllInsts().end(), block2->AllInsts().begin(),
                          block2->AllInsts().end(), instCmp);
    }

    bool InstInitialCompare(Inst *inst1, Inst *inst2)
    {
        if (inst1->GetOpcode() != inst2->GetOpcode() || inst1->GetType() != inst2->GetType() ||
            inst1->GetInputsCount() != inst2->GetInputsCount()) {
            instCompareMap_.erase(inst1);
            return false;
        }
        if (inst1->GetFlagsMask() != inst2->GetFlagsMask()) {
            instCompareMap_.erase(inst1);
            return false;
        }
        if (inst1->GetOpcode() != Opcode::Phi) {
            auto inst1Begin = inst1->GetInputs().begin();
            auto inst1End = inst1->GetInputs().end();
            auto inst2Begin = inst2->GetInputs().begin();
            auto eqLambda = [this](Input input1, Input input2) { return Compare(input1.GetInst(), input2.GetInst()); };
            if (!std::equal(inst1Begin, inst1End, inst2Begin, eqLambda)) {
                instCompareMap_.erase(inst1);
                return false;
            }
        } else {
            if (inst1->GetInputsCount() != inst2->GetInputsCount()) {
                instCompareMap_.erase(inst1);
                return false;
            }
            for (size_t index1 = 0; index1 < inst1->GetInputsCount(); index1++) {
                auto input1 = inst1->GetInput(index1).GetInst();
                auto bb1 = inst1->CastToPhi()->GetPhiInputBb(index1);
                if (bbMap_.count(bb1) == 0) {
                    instCompareMap_.erase(inst1);
                    return false;
                }
                auto bb2 = bbMap_.at(bb1);
                auto input2 = inst2->CastToPhi()->GetPhiInput(bb2);
                if (!Compare(input1, input2)) {
                    instCompareMap_.erase(inst1);
                    return false;
                }
            }
        }
        return true;
    }

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage
#define CAST(Opc) CastTo##Opc()
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage
#define CHECK(Opc, Getter)                                                                               \
    if (inst1->GetOpcode() == Opcode::Opc && inst1->CAST(Opc)->Getter() != inst2->CAST(Opc)->Getter()) { \
        instCompareMap_.erase(inst1);                                                                    \
        return false;                                                                                    \
    }

    bool InstPropertiesCompare(Inst *inst1, Inst *inst2)
    {
        CHECK(Cast, GetOperandsType)
        CHECK(Cmp, GetOperandsType)

        CHECK(Compare, GetCc)
        CHECK(Compare, GetOperandsType)

        CHECK(If, GetCc)
        CHECK(If, GetOperandsType)

        CHECK(IfImm, GetCc)
        CHECK(IfImm, GetImm)
        CHECK(IfImm, GetOperandsType)

        CHECK(Select, GetCc)
        CHECK(Select, GetOperandsType)

        CHECK(SelectImm, GetCc)
        CHECK(SelectImm, GetImm)
        CHECK(SelectImm, GetOperandsType)

        CHECK(LoadArrayI, GetImm)
        CHECK(LoadArrayPairI, GetImm)
        CHECK(LoadPairPart, GetImm)
        CHECK(StoreArrayI, GetImm)
        CHECK(StoreArrayPairI, GetImm)
        CHECK(LoadArrayPair, GetImm)
        CHECK(StoreArrayPair, GetImm)
        CHECK(BoundsCheckI, GetImm)
        CHECK(ReturnI, GetImm)
        CHECK(AddI, GetImm)
        CHECK(SubI, GetImm)
        CHECK(ShlI, GetImm)
        CHECK(ShrI, GetImm)
        CHECK(AShrI, GetImm)
        CHECK(AndI, GetImm)
        CHECK(OrI, GetImm)
        CHECK(XorI, GetImm)

        return true;
    }

    bool InstAdditionalPropertiesCompare(Inst *inst1, Inst *inst2)
    {
        CHECK(LoadArray, GetNeedBarrier)
        CHECK(LoadArrayPair, GetNeedBarrier)
        CHECK(StoreArray, GetNeedBarrier)
        CHECK(StoreArrayPair, GetNeedBarrier)
        CHECK(LoadArrayI, GetNeedBarrier)
        CHECK(LoadArrayPairI, GetNeedBarrier)
        CHECK(StoreArrayI, GetNeedBarrier)
        CHECK(StoreArrayPairI, GetNeedBarrier)
        CHECK(LoadStatic, GetNeedBarrier)
        CHECK(StoreStatic, GetNeedBarrier)
        CHECK(LoadObject, GetNeedBarrier)
        CHECK(StoreObject, GetNeedBarrier)
        CHECK(LoadStatic, GetVolatile)
        CHECK(StoreStatic, GetVolatile)
        CHECK(LoadObject, GetVolatile)
        CHECK(StoreObject, GetVolatile)
        CHECK(NewObject, GetNeedBarrier)
        CHECK(NewArray, GetNeedBarrier)
        CHECK(CheckCast, GetNeedBarrier)
        CHECK(IsInstance, GetNeedBarrier)
        CHECK(LoadString, GetNeedBarrier)
        CHECK(LoadConstArray, GetNeedBarrier)
        CHECK(LoadType, GetNeedBarrier)

        CHECK(CallStatic, IsInlined)
        CHECK(CallVirtual, IsInlined)

        CHECK(LoadArray, IsArray)
        CHECK(LenArray, IsArray)

        CHECK(Deoptimize, GetDeoptimizeType)
        CHECK(DeoptimizeIf, GetDeoptimizeType)

        CHECK(CompareAnyType, GetAnyType)
        CHECK(CastValueToAnyType, GetAnyType)
        CHECK(CastAnyTypeValue, GetAnyType)
        CHECK(AnyTypeCheck, GetAnyType)

        CHECK(HclassCheck, GetCheckIsFunction)
        CHECK(HclassCheck, GetCheckFunctionIsNotClassConstructor)

        // Those below can fail because unit test Graph don't have proper Runtime links
        // CHECK(Intrinsic, GetEntrypointId)
        // CHECK(CallStatic, GetCallMethodId)
        // CHECK(CallVirtual, GetCallMethodId)

        // CHECK(InitClass, GetTypeId)
        // CHECK(LoadAndInitClass, GetTypeId)
        // CHECK(LoadStatic, GetTypeId)
        // CHECK(StoreStatic, GetTypeId)
        // CHECK(LoadObject, GetTypeId)
        // CHECK(StoreObject, GetTypeId)
        // CHECK(NewObject, GetTypeId)
        // CHECK(InitObject, GetTypeId)
        // CHECK(NewArray, GetTypeId)
        // CHECK(LoadConstArray, GetTypeId)
        // CHECK(CheckCast, GetTypeId)
        // CHECK(IsInstance, GetTypeId)
        // CHECK(LoadString, GetTypeId)
        // CHECK(LoadType, GetTypeId)

        return true;
    }
#undef CHECK
#undef CAST

    bool InstSaveStateCompare(Inst *inst1, Inst *inst2)
    {
        auto *svSt1 = static_cast<SaveStateInst *>(inst1);
        auto *svSt2 = static_cast<SaveStateInst *>(inst2);
        if (svSt1->GetImmediatesCount() != svSt2->GetImmediatesCount()) {
            instCompareMap_.erase(inst1);
            return false;
        }

        std::vector<VirtualRegister::ValueType> regs1;
        std::vector<VirtualRegister::ValueType> regs2;
        regs1.reserve(svSt1->GetInputsCount());
        regs2.reserve(svSt2->GetInputsCount());
        for (size_t i {0}; i < svSt1->GetInputsCount(); ++i) {
            regs1.emplace_back(svSt1->GetVirtualRegister(i).Value());
            regs2.emplace_back(svSt2->GetVirtualRegister(i).Value());
        }
        std::sort(regs1.begin(), regs1.end());
        std::sort(regs2.begin(), regs2.end());
        if (regs1 != regs2) {
            instCompareMap_.erase(inst1);
            return false;
        }
        if (svSt1->GetImmediatesCount() != 0) {
            auto eqLambda = [](SaveStateImm i1, SaveStateImm i2) {
                return i1.value == i2.value && i1.vreg == i2.vreg && i1.vregType == i2.vregType && i1.type == i2.type;
            };
            if (!std::equal(svSt1->GetImmediates()->begin(), svSt1->GetImmediates()->end(),
                            svSt2->GetImmediates()->begin(), eqLambda)) {
                instCompareMap_.erase(inst1);
                return false;
            }
        }
        return true;
    }

    bool Compare(Inst *inst1, Inst *inst2)
    {
        if (auto it = instCompareMap_.insert({inst1, inst2}); !it.second) {
            if (inst2 == it.first->second) {
                return true;
            }
            instCompareMap_.erase(inst1);
            return false;
        }

        if (!InstInitialCompare(inst1, inst2)) {
            return false;
        }

        if (!InstPropertiesCompare(inst1, inst2)) {
            return false;
        }

        if (!InstAdditionalPropertiesCompare(inst1, inst2)) {
            return false;
        }

        if (inst1->GetOpcode() == Opcode::Constant) {
            auto c1 = inst1->CastToConstant();
            auto c2 = inst2->CastToConstant();
            bool same = false;
            switch (inst1->GetType()) {
                case DataType::FLOAT32:
                case DataType::INT32:
                    same = static_cast<uint32_t>(c1->GetRawValue()) == static_cast<uint32_t>(c2->GetRawValue());
                    break;
                default:
                    same = c1->GetRawValue() == c2->GetRawValue();
                    break;
            }
            if (!same) {
                instCompareMap_.erase(inst1);
                return false;
            }
        }
        if (inst1->GetOpcode() == Opcode::Cmp && IsFloatType(inst1->GetInput(0).GetInst()->GetType())) {
            auto cmp1 = static_cast<CmpInst *>(inst1);
            auto cmp2 = static_cast<CmpInst *>(inst2);
            if (cmp1->IsFcmpg() != cmp2->IsFcmpg()) {
                instCompareMap_.erase(inst1);
                return false;
            }
        }
        for (size_t i = 0; i < inst2->GetInputsCount(); i++) {
            if (inst1->GetInputType(i) != inst2->GetInputType(i)) {
                instCompareMap_.erase(inst1);
                return false;
            }
        }
        if (inst1->IsSaveState()) {
            return InstSaveStateCompare(inst1, inst2);
        }
        return true;
    }

private:
    std::unordered_map<Inst *, Inst *> instCompareMap_;
    std::unordered_map<BasicBlock *, BasicBlock *> bbMap_;
};
}  // namespace ark::compiler

#endif  // COMPILER_TESTS_GRAPH_COMPARATOR_H
