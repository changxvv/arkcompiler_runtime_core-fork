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

#include "reg_encoder.h"
#include "common.h"
#include "compiler/optimizer/ir/basicblock.h"

namespace panda::bytecodeopt {

static bool IsIntrinsicRange(Inst *inst)
{
    if (inst->GetOpcode() != compiler::Opcode::Intrinsic) {
        return false;
    }
#if defined(ENABLE_BYTECODE_OPT) && defined(PANDA_WITH_ECMASCRIPT)
    switch (inst->CastToIntrinsic()->GetIntrinsicId()) {
#ifdef ARK_INTRINSIC_SET
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_CALLI_RANGE_DYN:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_CALLI_THIS_RANGE_DYN:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_NEWOBJ_DYNRANGE:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_SUPER_CALL:
#else
        case compiler::RuntimeInterface::IntrinsicId::ECMA_CALLIRANGEDYN_PREF_IMM16_V8:
        case compiler::RuntimeInterface::IntrinsicId::ECMA_CALLITHISRANGEDYN_PREF_IMM16_V8:
        case compiler::RuntimeInterface::IntrinsicId::ECMA_NEWOBJDYNRANGE_PREF_IMM16_V8:
        case compiler::RuntimeInterface::IntrinsicId::ECMA_SUPERCALL_PREF_IMM16_V8:
        case compiler::RuntimeInterface::IntrinsicId::ECMA_CREATEOBJECTWITHEXCLUDEDKEYS_PREF_IMM16_V8_V8:
#endif
            return true;
        default:
            return false;
    }
#endif
    return false;
}

static bool CanHoldRange(Inst *inst)
{
    switch (inst->GetOpcode()) {
        case compiler::Opcode::CallStatic:
        case compiler::Opcode::CallVirtual:
        case compiler::Opcode::CallLaunchStatic:
        case compiler::Opcode::CallLaunchVirtual:
        case compiler::Opcode::InitObject:
            return true;
        case compiler::Opcode::Intrinsic:
            return IsIntrinsicRange(inst);
        default:
            return false;
    }
}

static compiler::Register CalculateNumNeededRangeTemps(const compiler::Graph *graph)
{
    compiler::Register ret = 0;

    for (auto bb : graph->GetBlocksRPO()) {
        for (const auto &inst : bb->AllInsts()) {
            if (!CanHoldRange(inst)) {
                continue;
            }
            auto nargs = inst->GetInputsCount() - (inst->RequireState() ? 1U : 0U);
            if (inst->GetOpcode() == compiler::Opcode::InitObject) {
                ASSERT(nargs > 0U);
                nargs -= 1;  // exclude LoadAndInitClass
            }
            if (inst->GetOpcode() == compiler::Opcode::CallLaunchVirtual ||
                inst->GetOpcode() == compiler::Opcode::CallLaunchStatic) {
                ASSERT(nargs > 0U);
                nargs -= 1;  // exclude NewObject
            }
            if (ret < nargs && (nargs > MAX_NUM_NON_RANGE_ARGS || IsIntrinsicRange(inst))) {
                ret = nargs;
            }
        }
    }

    return ret;
}

bool RegEncoder::RunImpl()
{
    ASSERT(state_ == RegEncoderState::IDLE);

    num_max_range_input_ = CalculateNumNeededRangeTemps(GetGraph());

    state_ = RegEncoderState::RENUMBER_ARGS;
    if (!RenumberArgRegs()) {
        return false;
    }

    state_ = RegEncoderState::RESERVE_TEMPS;
    ASSERT(num_temps_ == 0U);

    const auto num_regs = GetNumRegs();

    auto max_num_temps = num_temps_;
    CalculateNumNeededTemps();

    while (max_num_temps != num_temps_) {
        ASSERT(num_temps_ > max_num_temps);

        if (num_regs > compiler::VIRTUAL_FRAME_SIZE - num_temps_) {  // to avoid overflow
            return false;                                            // no more free registers left in the frame
        }

        auto delta = static_cast<compiler::Register>(num_temps_ - max_num_temps);
        range_temps_start_ += delta;

        RenumberRegs(MIN_REGISTER_NUMBER, delta);

        max_num_temps = num_temps_;
        CalculateNumNeededTemps();
        if (num_temps_ == 0U) {
            break;
        }
    }

    num_temps_ = num_temps_ > 0U ? num_temps_ : num_changed_width_;
    if (num_temps_ > 0U || num_max_range_input_ > 0U) {
        state_ = RegEncoderState::INSERT_SPILLS;
        InsertSpills();

        auto usage_mask = GetGraph()->GetUsedRegs<compiler::DataType::INT64>();
        for (compiler::Register r = 0; r < num_regs; r++) {
            usage_mask->at(num_regs + num_temps_ - r - 1L) = usage_mask->at(num_regs - r - 1L);
        }
        std::fill(usage_mask->begin(), usage_mask->begin() + num_temps_, true);
    }

    SaveNumLocalsToGraph(GetNumLocalsFromGraph() + num_temps_);
    state_ = RegEncoderState::IDLE;

    return true;
}

static panda::compiler::DataType::Type GetRegType(panda::compiler::DataType::Type type)
{
    if (type == panda::compiler::DataType::REFERENCE) {
        return type;
    }
    if (panda::compiler::DataType::Is32Bits(type, Arch::NONE)) {
        return panda::compiler::DataType::UINT32;
    }
    return panda::compiler::DataType::UINT64;
}

static bool RegNeedsRenumbering(panda::compiler::Register r)
{
    return r != panda::compiler::ACC_REG_ID && r != panda::compiler::INVALID_REG;
}

static panda::compiler::Register RenumberReg(const panda::compiler::Register r, const panda::compiler::Register delta)
{
    if (r == panda::compiler::ACC_REG_ID) {
        return r;
    }
    return r + delta;
}

static void RenumberSpillFillRegs(panda::compiler::SpillFillInst *inst, const panda::compiler::Register min_reg,
                                  const panda::compiler::Register delta)
{
    for (auto &sf : inst->GetSpillFills()) {
        if (sf.SrcType() == compiler::LocationType::REGISTER && sf.SrcValue() >= min_reg) {
            sf.SetSrc(compiler::Location::MakeRegister(RenumberReg(sf.SrcValue(), delta)));
        }
        if (sf.DstType() == compiler::LocationType::REGISTER && sf.DstValue() >= min_reg) {
            sf.SetDst(compiler::Location::MakeRegister(RenumberReg(sf.DstValue(), delta)));
        }
    }
}

void RegEncoder::RenumberRegs(const compiler::Register min_reg, const compiler::Register delta)
{
    // Renumbering always advances register number `delta` positions forward,
    // wrapping around on overflows with well-defined behavour.
    // Hence the requirement to keep delta unsigned.
    static_assert(std::is_unsigned<compiler::Register>::value, "compiler::Register must be unsigned");
    ASSERT(delta > 0U);

    for (auto *bb : GetGraph()->GetBlocksRPO()) {
        for (auto inst : bb->AllInsts()) {
            // Renumber output of any instruction, if applicable:
            if (RegNeedsRenumbering(inst->GetDstReg()) && inst->GetDstReg() >= min_reg) {
                inst->SetDstReg(RenumberReg(inst->GetDstReg(), delta));
            }

            if (inst->IsPhi() || inst->IsCatchPhi()) {
                continue;
            }

            // Renumber inputs and outputs of SpillFill instructions:
            if (inst->IsSpillFill()) {
                RenumberSpillFillRegs(inst->CastToSpillFill(), min_reg, delta);
                continue;
            }

            // Fix inputs of common instructions:
            for (size_t i = 0; i < inst->GetInputsCount(); i++) {
                if (RegNeedsRenumbering(inst->GetSrcReg(i)) && inst->GetSrcReg(i) >= min_reg) {
                    inst->SetSrcReg(i, RenumberReg(inst->GetSrcReg(i), delta));
                }
            }
        }
    }
}

bool RegEncoder::RenumberArgRegs()
{
    const auto usage_mask = GetGraph()->GetUsedRegs<compiler::DataType::INT64>();
    ASSERT(usage_mask->size() == compiler::VIRTUAL_FRAME_SIZE);

    auto frame_size = static_cast<compiler::Register>(usage_mask->size());
    const auto num_args = GetNumArgsFromGraph();
    ASSERT(frame_size >= num_args);

    auto num_non_args = static_cast<compiler::Register>(frame_size - num_args);
    if (num_max_range_input_ > num_non_args) {
        LOG(DEBUG, BYTECODE_OPTIMIZER) << "RegEncoder: The free regs for range call are not enough";
        return false;
    }

    compiler::Register num_locals = 0;
    if (num_non_args != 0U) {
        while (num_locals != num_non_args && usage_mask->at(num_locals)) {
            ++num_locals;
        }
    }

    compiler::Register num_temps = 0;
    if (num_locals != num_non_args) {
        compiler::Register r = num_non_args - 1L;
        while (r < num_non_args && usage_mask->at(r)) {
            ++num_temps;
            --r;
        }
    }

    if (num_locals + num_temps > num_non_args - num_max_range_input_) {
        LOG(DEBUG, BYTECODE_OPTIMIZER) << "RegEncoder: The free regs for range call are not enough";
        return false;
    }

    range_temps_start_ = num_locals;

    bool do_renumber = true;

    if (num_non_args == 0U && num_max_range_input_ == 0U) {  // all registers are arguments: no need to renumber
        do_renumber = false;
    }

    // All free regs will be just enough to encode call.rang: no need to renumber
    if (num_locals + num_temps + num_max_range_input_ == num_non_args) {
        do_renumber = false;
    }

    if (num_temps + num_args == 0U) {  // no temps and no args: nothing to renumber
        do_renumber = false;
    }

    if (do_renumber) {
        const auto min_reg = static_cast<compiler::Register>(num_non_args - num_temps);
        ASSERT(min_reg > MIN_REGISTER_NUMBER);

        // Assert that if temps are present, they are marked allocated in the mask:
        for (compiler::Register r = min_reg; r < min_reg + num_temps; r++) {
            ASSERT(usage_mask->at(r));
        }

        // Assert that there are no used regs between locals and temps + arguments:
        for (compiler::Register r = num_locals; r < min_reg; r++) {
            ASSERT(!usage_mask->at(r));
        }

        auto delta = static_cast<compiler::Register>(num_locals + num_temps + num_max_range_input_ - num_non_args);
        RenumberRegs(min_reg, delta);

        for (compiler::Register r = min_reg; r < frame_size; r++) {
            usage_mask->at(RenumberReg(r, delta)) = usage_mask->at(r);
            usage_mask->at(r) = false;
        }
    }

    SaveNumLocalsToGraph(num_locals + num_temps + num_max_range_input_);
    return true;
}

void RegEncoder::InsertSpills()
{
    ASSERT(num_max_range_input_ > 0U || (num_temps_ > 0U && num_temps_ <= MAX_NUM_INPUTS));

    for (auto *bb : GetGraph()->GetBlocksRPO()) {
        for (auto inst : bb->AllInstsSafe()) {
            if (inst->GetInputsCount() == 0U) {
                continue;
            }

            VisitInstruction(inst);
            // NOTE(aantipina): Enable assert here for GetStatus() as soon code generation is fully supported
        }
    }
}

void RegEncoder::CalculateNumNeededTemps()
{
    num_temps_ = 0;

    for (auto bb : GetGraph()->GetBlocksRPO()) {
        for (auto inst : bb->AllInstsSafe()) {
            if (inst->GetInputsCount() == 0U) {
                continue;
            }

            VisitInstruction(inst);
            // NOTE(aantipina): Enable here for GetStatus() as soon code generation is fully supported
        }
    }

    LOG(DEBUG, BYTECODE_OPTIMIZER) << GetGraph()->GetRuntime()->GetMethodFullName(GetGraph()->GetMethod())
                                   << ": num_temps_ = " << std::to_string(num_temps_);
}

template <typename T>
static void AddMoveBefore(Inst *inst, const T &sp_container)
{
    if (sp_container.empty()) {
        return;
    }
    auto sf_inst = inst->GetBasicBlock()->GetGraph()->CreateInstSpillFill();
    for (auto const &[src, dst] : sp_container) {
        ASSERT(src != compiler::ACC_REG_ID);
        sf_inst->AddMove(src, dst.reg, GetRegType(dst.type));
        LOG(DEBUG, BYTECODE_OPTIMIZER) << "RegEncoder: Move v" << static_cast<int>(dst.reg) << " <- v"
                                       << static_cast<int>(src) << " was added";
    }
    inst->GetBasicBlock()->InsertBefore(sf_inst, inst);
}

static bool IsAccReadPosition(compiler::Inst *inst, size_t pos)
{
    // Calls can have accumulator at any position, return false for them
    return !inst->IsCall() && inst->IsAccRead() && pos == AccReadIndex(inst);
}

void RegEncoder::InsertSpillsForDynInputsInst(compiler::Inst *inst)
{
    ASSERT(state_ == RegEncoderState::INSERT_SPILLS);
    ASSERT(inst->IsStaticCall() || inst->IsVirtualCall() || inst->IsInitObject() || inst->IsIntrinsic());

    RegContentMap spill_map(GetGraph()->GetLocalAllocator()->Adapter());  // src -> (dst, src_type), non-callrange
    RegContentVec spill_vec(GetGraph()->GetLocalAllocator()->Adapter());  // spill_vec is used to handle callrange

    auto nargs = inst->GetInputsCount() - (inst->RequireState() ? 1U : 0U);
    auto start = GetStartInputIndex(inst);
    bool range = IsIntrinsicRange(inst) || (nargs - start > MAX_NUM_NON_RANGE_ARGS && CanHoldRange(inst));

    compiler::Register temp = range ? range_temps_start_ : 0U;

    for (size_t i = start; i < nargs; ++i) {
        auto src_reg = inst->GetSrcReg(i);
        auto type = inst->GetInputType(i);

        // do not spillfill for acc-read position. For example, Intrinsic.FSTARR32
        if (IsAccReadPosition(inst, i)) {
            continue;
        }

        if (!range) {
            if (!RegNeedsRenumbering(src_reg) || src_reg < NUM_COMPACTLY_ENCODED_REGS) {
                continue;
            }

            auto res = spill_map.emplace(src_reg, RegContent(temp, type));
            if (res.second) {
                inst->SetSrcReg(i, temp++);
            } else {
                // Such register is already in map.
                // It can be ok for cases like: CallStatic v49, v49
                // Such instructions can be generated by optimizer too.
                const RegContent &reg_cont = res.first->second;
                inst->SetSrcReg(i, reg_cont.reg);
            }
        } else {
            spill_vec.emplace_back(src_reg, RegContent(temp, type));
            inst->SetSrcReg(i, temp++);
        }
    }

    AddMoveBefore(inst, spill_map);
    AddMoveBefore(inst, spill_vec);
}

size_t RegEncoder::GetStartInputIndex(compiler::Inst *inst)
{
    return (inst->GetOpcode() == compiler::Opcode::InitObject ||
            inst->GetOpcode() == compiler::Opcode::CallLaunchStatic ||
            inst->GetOpcode() == compiler::Opcode::CallLaunchVirtual)
               ? 1U
               : 0U;  // exclude LoadAndInitClass and NewObject
}

static void AddMoveAfter(Inst *inst, compiler::Register src, RegContent dst)
{
    auto sf_inst = inst->GetBasicBlock()->GetGraph()->CreateInstSpillFill();
    sf_inst->AddMove(src, dst.reg, dst.type);
    LOG(DEBUG, BYTECODE_OPTIMIZER) << "RegEncoder: Move v" << static_cast<int>(dst.reg) << " <- v"
                                   << static_cast<int>(src) << " was added";
    inst->GetBasicBlock()->InsertAfter(sf_inst, inst);
}

static bool IsBoundDstSrc(const compiler::Inst *inst)
{
    if (!inst->IsBinaryInst()) {
        return false;
    }
    auto src0 = inst->GetSrcReg(0U);
    auto src1 = inst->GetSrcReg(1U);
    auto dst = inst->GetDstReg();
    if (inst->IsCommutative()) {
        return src0 == dst || src1 == dst;
    }
    return src0 == dst;
}

static bool IsMoveAfter(const compiler::Inst *inst)
{
    auto writes_to_dest = inst->GetDstReg() != compiler::ACC_REG_ID;
    if (inst->IsBinaryImmInst()) {
        return writes_to_dest;
    }
    if (inst->IsBinaryInst()) {
        return writes_to_dest && IsBoundDstSrc(inst);
    }
    switch (inst->GetOpcode()) {
        case compiler::Opcode::LoadObject:
            // Special case for LoadObject, because it can be register instruction.
            return writes_to_dest;
        case compiler::Opcode::NewArray:
            return true;
        default:
            return false;
    }
}

void RegEncoder::InsertSpillsForInst(compiler::Inst *inst)
{
    ASSERT(state_ == RegEncoderState::INSERT_SPILLS);

    RegContentMap spill_map(GetGraph()->GetLocalAllocator()->Adapter());  // src -> (dst, src_type)

    if (inst->IsOperandsDynamic()) {
        InsertSpillsForDynInputsInst(inst);
        return;
    }

    compiler::Register temp = 0;
    for (size_t i = 0; i < inst->GetInputsCount(); i++) {
        // NOTE(mbolshov): make a better solution to skip instructions, that are not relevant to bytecode_opt
        if (inst->GetInput(i).GetInst()->GetOpcode() == Opcode::LoadAndInitClass) {
            continue;
        }
        auto reg = inst->GetSrcReg(i);
        if (RegNeedsRenumbering(reg) && reg >= NUM_COMPACTLY_ENCODED_REGS) {
            auto res = spill_map.emplace(reg, RegContent(temp, GetRegType(inst->GetInputType(i))));
            if (res.second) {
                inst->SetSrcReg(i, temp++);
            } else {
                // Such register is already in map.
                // It can be ok for cases like: and v49, v49
                // Such instructions can be generated by optimizer too.
                const RegContent &reg_cont = res.first->second;
                inst->SetSrcReg(i, reg_cont.reg);
            }
        }
    }

    AddMoveBefore(inst, spill_map);
    if (IsMoveAfter(inst)) {
        auto reg = inst->GetDstReg();
        if (RegNeedsRenumbering(reg) && reg >= NUM_COMPACTLY_ENCODED_REGS) {
            inst->SetDstReg(temp);
            AddMoveAfter(inst, temp, RegContent(reg, GetRegType(inst->GetType())));
        }
    }
}

static void IncTempsIfNeeded(compiler::Inst *inst, const compiler::Register reg, compiler::Register &num_temps,
                             compiler::Register &num_changed_width)
{
    if (RegNeedsRenumbering(reg) && reg >= NUM_COMPACTLY_ENCODED_REGS) {
        num_temps++;
        if (inst->IsBinaryInst()) {
            num_changed_width++;
        }
    }
}

void RegEncoder::CalculateNumNeededTempsForInst(compiler::Inst *inst)
{
    ASSERT(state_ == RegEncoderState::RESERVE_TEMPS);

    compiler::Register num_temps = 0;
    compiler::Register num_changed_width = 0;

    if (inst->IsOperandsDynamic()) {
        if (IsIntrinsicRange(inst)) {
            return;
        }
        ASSERT(inst->IsStaticCall() || inst->IsVirtualCall() || inst->IsInitObject() || inst->IsIntrinsic());

        auto nargs = inst->GetInputsCount() - (inst->RequireState() ? 1U : 0U);
        size_t start = inst->GetOpcode() == compiler::Opcode::InitObject ? 1U : 0U;
        if (nargs - start > MAX_NUM_NON_RANGE_ARGS) {  // is call.range
            return;
        }

        for (size_t i = start; i < nargs; i++) {
            if (IsAccReadPosition(inst, i)) {
                continue;
            }
            auto reg = inst->GetSrcReg(i);
            if (RegNeedsRenumbering(reg) && reg >= NUM_COMPACTLY_ENCODED_REGS) {
                num_temps++;
                if (inst->IsBinaryInst()) {
                    num_changed_width++;
                }
            }
        }
    } else {
        for (size_t i = 0; i < inst->GetInputsCount(); i++) {
            // NOTE(mbolshov): make a better solution to skip instructions, that are not relevant to bytecode_opt
            if (inst->GetInput(i).GetInst()->GetOpcode() == Opcode::LoadAndInitClass) {
                continue;
            }
            IncTempsIfNeeded(inst, inst->GetSrcReg(i), num_temps, num_changed_width);
        }

        if (IsMoveAfter(inst) && !IsBoundDstSrc(inst)) {
            IncTempsIfNeeded(inst, inst->GetDstReg(), num_temps, num_changed_width);
        }
    }

    ASSERT(num_temps <= MAX_NUM_INPUTS);

    num_temps_ = std::max(num_temps, num_temps_);
    num_changed_width_ = std::max(num_changed_width, num_changed_width_);
}

void RegEncoder::Check4Width(compiler::Inst *inst)
{
    switch (state_) {
        case RegEncoderState::RESERVE_TEMPS: {
            CalculateNumNeededTempsForInst(inst);
            break;
        }
        case RegEncoderState::INSERT_SPILLS: {
            InsertSpillsForInst(inst);
            break;
        }
        default:
            UNREACHABLE();
    }
}

void RegEncoder::Check8Width([[maybe_unused]] compiler::Inst *inst)
{
    // NOTE(aantipina): implement after it became possible to use register numbers more than 256 (#2697)
}

void RegEncoder::VisitCallStatic(GraphVisitor *visitor, Inst *inst)
{
    CallHelper(visitor, inst);
}

void RegEncoder::VisitCallVirtual(GraphVisitor *visitor, Inst *inst)
{
    CallHelper(visitor, inst);
}

void RegEncoder::VisitInitObject(GraphVisitor *visitor, Inst *inst)
{
    CallHelper(visitor, inst);
}

void RegEncoder::VisitIntrinsic(GraphVisitor *visitor, Inst *inst)
{
    auto re = static_cast<RegEncoder *>(visitor);
    if (IsIntrinsicRange(inst)) {
        re->Check4Width(inst->CastToIntrinsic());
        return;
    }

    re->Check8Width(inst->CastToIntrinsic());
}

void RegEncoder::VisitLoadObject(GraphVisitor *v, Inst *inst_base)
{
    bool is_acc_type = inst_base->GetDstReg() == compiler::ACC_REG_ID;

    auto re = static_cast<RegEncoder *>(v);
    auto inst = inst_base->CastToLoadObject();
    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT32:
        case compiler::DataType::FLOAT64:
        case compiler::DataType::REFERENCE:
            if (is_acc_type) {
                re->Check8Width(inst);
            } else {
                re->Check4Width(inst);
            }
            break;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Wrong DataType for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            re->success_ = false;
    }
}

void RegEncoder::VisitLoadStatic(GraphVisitor *v, Inst *inst_base)
{
    auto re = static_cast<RegEncoder *>(v);
    auto inst = inst_base->CastToLoadStatic();

    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT32:
        case compiler::DataType::FLOAT64:
        case compiler::DataType::REFERENCE:
            return;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Wrong DataType for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            re->success_ = false;
    }
}

void RegEncoder::VisitStoreObject(GraphVisitor *v, Inst *inst_base)
{
    bool is_acc_type = inst_base->GetSrcReg(1U) == compiler::ACC_REG_ID;

    auto re = static_cast<RegEncoder *>(v);
    auto inst = inst_base->CastToStoreObject();
    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT32:
        case compiler::DataType::FLOAT64:
        case compiler::DataType::REFERENCE:
            if (is_acc_type) {
                re->Check8Width(inst);
            } else {
                re->Check4Width(inst);
            }
            break;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Wrong DataType for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            re->success_ = false;
    }
}

void RegEncoder::VisitStoreStatic(GraphVisitor *v, Inst *inst_base)
{
    auto re = static_cast<RegEncoder *>(v);
    auto inst = inst_base->CastToStoreStatic();

    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT32:
        case compiler::DataType::FLOAT64:
        case compiler::DataType::REFERENCE:
            return;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Wrong DataType for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            re->success_ = false;
    }
}
void RegEncoder::VisitSpillFill([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst) {}
void RegEncoder::VisitConstant([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst) {}
void RegEncoder::VisitLoadString([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst) {}
void RegEncoder::VisitReturn([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst) {}
void RegEncoder::VisitCatchPhi([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst) {}
void RegEncoder::VisitCastValueToAnyType([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst) {}

#include "generated/check_width.cpp"
}  // namespace panda::bytecodeopt
