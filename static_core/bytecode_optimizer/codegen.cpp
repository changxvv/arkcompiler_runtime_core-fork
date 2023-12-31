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
#include "codegen.h"
#include "runtime/include/coretypes/tagged_value.h"

namespace panda::bytecodeopt {

void DoLdaObj(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_LDA_OBJ(reg));
    }
}

void DoLda(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_LDA(reg));
    }
}

void DoLda64(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_LDA_64(reg));
    }
}

void DoStaObj(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_STA_OBJ(reg));
    }
}

void DoSta(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_STA(reg));
    }
}

void DoSta64(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_STA_64(reg));
    }
}

void DoLdaDyn(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_LDA_DYN(reg));
    }
}

void DoStaDyn(compiler::Register reg, std::vector<pandasm::Ins> &result)
{
    if (reg != compiler::ACC_REG_ID) {
        result.emplace_back(pandasm::Create_STA_DYN(reg));
    }
}

void BytecodeGen::AppendCatchBlock(uint32_t type_id, const compiler::BasicBlock *try_begin,
                                   const compiler::BasicBlock *try_end, const compiler::BasicBlock *catch_begin,
                                   const compiler::BasicBlock *catch_end)
{
    auto cb = pandasm::Function::CatchBlock();
    if (type_id != 0U) {
        cb.exception_record = ir_interface_->GetTypeIdByOffset(type_id);
    }
    cb.try_begin_label = BytecodeGen::LabelName(try_begin->GetId());
    cb.try_end_label = "end_" + BytecodeGen::LabelName(try_end->GetId());
    cb.catch_begin_label = BytecodeGen::LabelName(catch_begin->GetId());
    cb.catch_end_label =
        catch_end == nullptr ? cb.catch_begin_label : "end_" + BytecodeGen::LabelName(catch_end->GetId());
    catch_blocks_.emplace_back(cb);
}

void BytecodeGen::VisitTryBegin(const compiler::BasicBlock *bb)
{
    ASSERT(bb->IsTryBegin());
    auto try_inst = GetTryBeginInst(bb);
    auto try_end = try_inst->GetTryEndBlock();
    ASSERT(try_end != nullptr && try_end->IsTryEnd());

    bb->EnumerateCatchHandlers([&, bb, try_end](BasicBlock *catch_handler, size_t type_id) {
        AppendCatchBlock(type_id, bb, try_end, catch_handler);
        return true;
    });
}

void BytecodeGen::AddLineAndColumnNumber(const compiler::Inst *inst, size_t i)
{
    AddLineNumber(inst, i);
    if (GetGraph()->IsDynamicMethod()) {
        AddColumnNumber(inst, i);
    }
}

bool BytecodeGen::RunImpl()
{
    Reserve(function_->ins.size());
    for (auto *bb : GetGraph()->GetBlocksLinearOrder()) {
        EmitLabel(BytecodeGen::LabelName(bb->GetId()));
        if (bb->IsTryEnd() || bb->IsCatchEnd()) {
            auto label = "end_" + BytecodeGen::LabelName(bb->GetId());
            EmitLabel(label);
        }
        for (const auto &inst : bb->AllInsts()) {
            auto start = GetResult().size();
            VisitInstruction(inst);
            if (!GetStatus()) {
                return false;
            }
            auto end = GetResult().size();
            ASSERT(end >= start);
            for (auto i = start; i < end; ++i) {
                AddLineAndColumnNumber(inst, i);
            }
        }
        if (bb->NeedsJump()) {
            EmitJump(bb);
        }
    }
    if (!GetStatus()) {
        return false;
    }
    // Visit try-blocks in order they were declared
    for (auto *bb : GetGraph()->GetTryBeginBlocks()) {
        VisitTryBegin(bb);
    }
    function_->ins = std::move(GetResult());
    function_->catch_blocks = catch_blocks_;
    return true;
}

void BytecodeGen::EmitJump(const BasicBlock *bb)
{
    BasicBlock *suc_bb = nullptr;
    ASSERT(bb != nullptr);

    if (bb->GetLastInst() == nullptr) {
        ASSERT(bb->IsEmpty());
        suc_bb = bb->GetSuccsBlocks()[0U];
        result_.push_back(pandasm::Create_JMP(BytecodeGen::LabelName(suc_bb->GetId())));
        return;
    }

    ASSERT(bb->GetLastInst() != nullptr);
    switch (bb->GetLastInst()->GetOpcode()) {
        case Opcode::If:
        case Opcode::IfImm:
            ASSERT(bb->GetSuccsBlocks().size() == compiler::MAX_SUCCS_NUM);
            suc_bb = bb->GetFalseSuccessor();
            break;
        default:
            suc_bb = bb->GetSuccsBlocks()[0U];
            break;
    }
    result_.push_back(pandasm::Create_JMP(BytecodeGen::LabelName(suc_bb->GetId())));
}

void BytecodeGen::AddLineNumber(const Inst *inst, const size_t idx)
{
    if (ir_interface_ != nullptr && idx < result_.size()) {
        auto ln = ir_interface_->GetLineNumberByPc(inst->GetPc());
        result_[idx].ins_debug.SetLineNumber(ln);
    }
}

void BytecodeGen::AddColumnNumber(const Inst *inst, const uint32_t idx)
{
    if (ir_interface_ != nullptr && idx < result_.size()) {
        auto cn = ir_interface_->GetColumnNumberByPc(inst->GetPc());
        result_[idx].ins_debug.SetColumnNumber(cn);
    }
}

void BytecodeGen::EncodeSpillFillData(const compiler::SpillFillData &sf)
{
    if (sf.SrcType() != compiler::LocationType::REGISTER || sf.DstType() != compiler::LocationType::REGISTER) {
        LOG(ERROR, BYTECODE_OPTIMIZER) << "EncodeSpillFillData with unknown move type, src_type: "
                                       << static_cast<int>(sf.SrcType())
                                       << " dst_type: " << static_cast<int>(sf.DstType());
        success_ = false;
        UNREACHABLE();
        return;
    }
    ASSERT(sf.GetType() != compiler::DataType::NO_TYPE);
    ASSERT(sf.SrcValue() != compiler::INVALID_REG && sf.DstValue() != compiler::INVALID_REG);

    if (sf.SrcValue() == sf.DstValue()) {
        return;
    }

    pandasm::Ins move;
    if (GetGraph()->IsDynamicMethod()) {
        result_.emplace_back(pandasm::Create_MOV_DYN(sf.DstValue(), sf.SrcValue()));
        return;
    }
    switch (sf.GetType()) {
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64:
            move = pandasm::Create_MOV_64(sf.DstValue(), sf.SrcValue());
            break;
        case compiler::DataType::REFERENCE:
            move = pandasm::Create_MOV_OBJ(sf.DstValue(), sf.SrcValue());
            break;
        default:
            move = pandasm::Create_MOV(sf.DstValue(), sf.SrcValue());
    }
    result_.emplace_back(move);
}

void BytecodeGen::VisitSpillFill(GraphVisitor *visitor, Inst *inst)
{
    auto *enc = static_cast<BytecodeGen *>(visitor);
    for (auto sf : inst->CastToSpillFill()->GetSpillFills()) {
        enc->EncodeSpillFillData(sf);
    }
}

template <typename UnaryPred>
bool HasUserPredicate(Inst *inst, UnaryPred p)
{
    bool found = false;
    for (auto const &u : inst->GetUsers()) {
        if (p(u.GetInst())) {
            found = true;
            break;
        }
    }
    return found;
}

static void VisitConstant32(BytecodeGen *enc, compiler::Inst *inst, std::vector<pandasm::Ins> &res)
{
    auto type = inst->GetType();
    ASSERT(compiler::DataType::Is32Bits(type, Arch::NONE));

    pandasm::Ins movi;
    auto dst_reg = inst->GetDstReg();
    movi.regs.emplace_back(inst->GetDstReg());

    switch (type) {
        case compiler::DataType::BOOL:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT32:
        case compiler::DataType::UINT32:
            if (enc->GetGraph()->IsDynamicMethod()) {
                res.emplace_back(pandasm::Create_LDAI_DYN(inst->CastToConstant()->GetInt32Value()));
                DoStaDyn(inst->GetDstReg(), res);
            } else {
                if (dst_reg == compiler::ACC_REG_ID) {
                    pandasm::Ins ldai = pandasm::Create_LDAI(inst->CastToConstant()->GetInt32Value());
                    res.emplace_back(ldai);
                } else {
                    movi = pandasm::Create_MOVI(dst_reg, inst->CastToConstant()->GetInt32Value());
                    res.emplace_back(movi);
                }
            }
            break;
        case compiler::DataType::FLOAT32:
            if (enc->GetGraph()->IsDynamicMethod()) {
                res.emplace_back(pandasm::Create_FLDAI_DYN(inst->CastToConstant()->GetFloatValue()));
                DoStaDyn(inst->GetDstReg(), res);
            } else {
                if (dst_reg == compiler::ACC_REG_ID) {
                    pandasm::Ins ldai = pandasm::Create_FLDAI(inst->CastToConstant()->GetFloatValue());
                    res.emplace_back(ldai);
                } else {
                    movi = pandasm::Create_FMOVI(dst_reg, inst->CastToConstant()->GetFloatValue());
                    res.emplace_back(movi);
                }
            }
            break;
        default:
            UNREACHABLE();
    }
}

static void VisitConstant64(BytecodeGen *enc, compiler::Inst *inst, std::vector<pandasm::Ins> &res)
{
    auto type = inst->GetType();
    ASSERT(compiler::DataType::Is64Bits(type, Arch::NONE));

    pandasm::Ins movi;
    auto dst_reg = inst->GetDstReg();
    movi.regs.emplace_back(inst->GetDstReg());

    switch (type) {
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
            if (enc->GetGraph()->IsDynamicMethod()) {
                res.emplace_back(pandasm::Create_LDAI_DYN(inst->CastToConstant()->GetInt64Value()));
                DoStaDyn(inst->GetDstReg(), res);
            } else {
                if (dst_reg == compiler::ACC_REG_ID) {
                    pandasm::Ins ldai = pandasm::Create_LDAI_64(inst->CastToConstant()->GetInt64Value());
                    res.emplace_back(ldai);
                } else {
                    movi = pandasm::Create_MOVI_64(dst_reg, inst->CastToConstant()->GetInt64Value());
                    res.emplace_back(movi);
                }
            }
            break;
        case compiler::DataType::FLOAT64:
            if (enc->GetGraph()->IsDynamicMethod()) {
                res.emplace_back(pandasm::Create_FLDAI_DYN(inst->CastToConstant()->GetDoubleValue()));
                DoStaDyn(inst->GetDstReg(), res);
            } else {
                if (dst_reg == compiler::ACC_REG_ID) {
                    pandasm::Ins ldai = pandasm::Create_FLDAI_64(inst->CastToConstant()->GetDoubleValue());
                    res.emplace_back(ldai);
                } else {
                    movi = pandasm::Create_FMOVI_64(dst_reg, inst->CastToConstant()->GetDoubleValue());
                    res.emplace_back(movi);
                }
            }
            break;
        default:
            UNREACHABLE();
    }
}

void BytecodeGen::VisitConstant(GraphVisitor *visitor, Inst *inst)
{
    auto *enc = static_cast<BytecodeGen *>(visitor);
    auto type = inst->GetType();

    /* Do not emit unused code for Const -> CastValueToAnyType chains */
    if (enc->GetGraph()->IsDynamicMethod()) {
        if (!HasUserPredicate(inst,
                              [](Inst const *i) { return i->GetOpcode() != compiler::Opcode::CastValueToAnyType; })) {
            return;
        }
    }

    switch (type) {
        case compiler::DataType::BOOL:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT32:
        case compiler::DataType::UINT32:
        case compiler::DataType::FLOAT32:
            VisitConstant32(enc, inst, enc->result_);
            break;
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64:
            VisitConstant64(enc, inst, enc->result_);
            break;
        case compiler::DataType::ANY: {
            UNREACHABLE();
        }
        default:
            UNREACHABLE();
            LOG(ERROR, BYTECODE_OPTIMIZER) << "VisitConstant with unknown type" << type;
            enc->success_ = false;
    }
}

void BytecodeGen::EncodeSta(compiler::Register reg, compiler::DataType::Type type)
{
    pandasm::Opcode opc;
    switch (type) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::FLOAT32:
            opc = pandasm::Opcode::STA;
            break;
        case compiler::DataType::UINT64:
        case compiler::DataType::INT64:
        case compiler::DataType::FLOAT64:
            opc = pandasm::Opcode::STA_64;
            break;
        case compiler::DataType::ANY:
            opc = pandasm::Opcode::STA_DYN;
            break;
        case compiler::DataType::REFERENCE:
            opc = pandasm::Opcode::STA_OBJ;
            break;
        default:
            UNREACHABLE();
            LOG(ERROR, BYTECODE_OPTIMIZER) << "EncodeSta with unknown type" << type;
            success_ = false;
    }
    pandasm::Ins sta;
    sta.opcode = opc;
    sta.regs.emplace_back(reg);

    result_.emplace_back(sta);
}

static pandasm::Opcode ChooseCallOpcode(compiler::Opcode op, size_t nargs)
{
    ASSERT(op == compiler::Opcode::CallStatic || op == compiler::Opcode::CallVirtual ||
           op == compiler::Opcode::InitObject);
    if (nargs > MAX_NUM_NON_RANGE_ARGS) {
        switch (op) {
            case compiler::Opcode::CallStatic:
                return pandasm::Opcode::CALL_RANGE;
            case compiler::Opcode::CallVirtual:
                return pandasm::Opcode::CALL_VIRT_RANGE;
            case compiler::Opcode::InitObject:
                return pandasm::Opcode::INITOBJ_RANGE;
            default:
                UNREACHABLE();
        }
    } else if (nargs > MAX_NUM_SHORT_CALL_ARGS) {
        switch (op) {
            case compiler::Opcode::CallStatic:
                return pandasm::Opcode::CALL;
            case compiler::Opcode::CallVirtual:
                return pandasm::Opcode::CALL_VIRT;
            case compiler::Opcode::InitObject:
                return pandasm::Opcode::INITOBJ;
            default:
                UNREACHABLE();
        }
    }
    switch (op) {
        case compiler::Opcode::CallStatic:
            return pandasm::Opcode::CALL_SHORT;
        case compiler::Opcode::CallVirtual:
            return pandasm::Opcode::CALL_VIRT_SHORT;
        case compiler::Opcode::InitObject:
            return pandasm::Opcode::INITOBJ_SHORT;
        default:
            UNREACHABLE();
    }
}

static pandasm::Opcode ChooseCallAccOpcode(pandasm::Opcode op)
{
    switch (op) {
        case pandasm::Opcode::CALL_SHORT:
            return pandasm::Opcode::CALL_ACC_SHORT;
        case pandasm::Opcode::CALL:
            return pandasm::Opcode::CALL_ACC;
        case pandasm::Opcode::CALL_VIRT_SHORT:
            return pandasm::Opcode::CALL_VIRT_ACC_SHORT;
        case pandasm::Opcode::CALL_VIRT:
            return pandasm::Opcode::CALL_VIRT_ACC;
        default:
            return pandasm::Opcode::INVALID;
    }
}

static panda::compiler::CallInst *CastToCall(panda::compiler::Inst *inst)
{
    switch (inst->GetOpcode()) {
        case compiler::Opcode::CallStatic:
            return inst->CastToCallStatic();
        case compiler::Opcode::CallVirtual:
            return inst->CastToCallVirtual();
        case compiler::Opcode::InitObject:
            return inst->CastToInitObject();
        default:
            UNREACHABLE();
    }
}

void BytecodeGen::CallHandler(GraphVisitor *visitor, Inst *inst)
{
    auto op = inst->GetOpcode();
    ASSERT(op == compiler::Opcode::CallStatic || op == compiler::Opcode::CallVirtual ||
           op == compiler::Opcode::InitObject);
    auto *enc = static_cast<BytecodeGen *>(visitor);
    auto call_inst = CastToCall(inst);
    auto sf_count = inst->GetInputsCount() - (inst->RequireState() ? 1U : 0U);
    size_t start = op == compiler::Opcode::InitObject ? 1U : 0U;  // exclude LoadAndInitClass
    auto nargs = sf_count - start;                                // exclude LoadAndInitClass
    pandasm::Ins ins;

    ins.opcode = ChooseCallOpcode(op, nargs);

    if (nargs > MAX_NUM_NON_RANGE_ARGS) {
#ifndef NDEBUG
        auto start_reg = inst->GetSrcReg(start);
        ASSERT(start_reg <= MAX_8_BIT_REG);
        for (auto i = start; i < sf_count; ++i) {
            auto reg = inst->GetSrcReg(i);
            ASSERT(reg - start_reg == static_cast<int>(i - start));  // check 'range-ness' of registers
        }
#endif  // !NDEBUG
        ins.regs.emplace_back(inst->GetSrcReg(start));
    } else {
        for (size_t i = start; i < sf_count; ++i) {
            auto reg = inst->GetSrcReg(i);
            ASSERT(reg < NUM_COMPACTLY_ENCODED_REGS || reg == compiler::ACC_REG_ID);
            if (reg == compiler::ACC_REG_ID) {
                ASSERT(inst->IsCall());
                ins.imms.emplace_back(static_cast<int64_t>(i));
                ins.opcode = ChooseCallAccOpcode(ins.opcode);
            } else {
                ins.regs.emplace_back(reg);
            }
        }
    }
    ins.ids.emplace_back(enc->ir_interface_->GetMethodIdByOffset(call_inst->GetCallMethodId()));
    enc->result_.emplace_back(ins);
    if (inst->GetDstReg() != compiler::INVALID_REG && inst->GetDstReg() != compiler::ACC_REG_ID) {
        enc->EncodeSta(inst->GetDstReg(), call_inst->GetType());
    }
}

void BytecodeGen::VisitCallStatic(GraphVisitor *visitor, Inst *inst)
{
    CallHandler(visitor, inst);
}

void BytecodeGen::VisitCallVirtual(GraphVisitor *visitor, Inst *inst)
{
    CallHandler(visitor, inst);
}

void BytecodeGen::VisitInitObject(GraphVisitor *visitor, Inst *inst)
{
    CallHandler(visitor, inst);
}

static void VisitIf32(BytecodeGen *enc, compiler::IfInst *inst, std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is32Bits(inst->GetInputType(0U), Arch::NONE));

    if (enc->GetGraph()->IsDynamicMethod()) {
        DoLdaDyn(inst->GetSrcReg(0U), res);
    } else {
        DoLda(inst->GetSrcReg(0U), res);
    }

    compiler::Register src = inst->GetSrcReg(1);
    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQ(src, label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNE(src, label));
            break;
        }
        case compiler::CC_LT: {
            res.emplace_back(pandasm::Create_JLT(src, label));
            break;
        }
        case compiler::CC_LE: {
            res.emplace_back(pandasm::Create_JLE(src, label));
            break;
        }
        case compiler::CC_GT: {
            res.emplace_back(pandasm::Create_JGT(src, label));
            break;
        }
        case compiler::CC_GE: {
            res.emplace_back(pandasm::Create_JGE(src, label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void VisitIf64Signed(BytecodeGen *enc, compiler::IfInst *inst, std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is64Bits(inst->GetInputType(0U), Arch::NONE));
    ASSERT(IsTypeSigned(inst->GetInputType(0U)));

    if (enc->GetGraph()->IsDynamicMethod()) {
        DoLdaDyn(inst->GetSrcReg(0U), res);
    } else {
        DoLda64(inst->GetSrcReg(0U), res);
    }

    res.emplace_back(pandasm::Create_CMP_64(inst->GetSrcReg(1U)));
    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQZ(label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNEZ(label));
            break;
        }
        case compiler::CC_LT: {
            res.emplace_back(pandasm::Create_JLTZ(label));
            break;
        }
        case compiler::CC_LE: {
            res.emplace_back(pandasm::Create_JLEZ(label));
            break;
        }
        case compiler::CC_GT: {
            res.emplace_back(pandasm::Create_JGTZ(label));
            break;
        }
        case compiler::CC_GE: {
            res.emplace_back(pandasm::Create_JGEZ(label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void VisitIf64Unsigned(BytecodeGen *enc, compiler::IfInst *inst, std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is64Bits(inst->GetInputType(0U), Arch::NONE));
    ASSERT(!IsTypeSigned(inst->GetInputType(0U)));

    if (enc->GetGraph()->IsDynamicMethod()) {
        DoLdaDyn(inst->GetSrcReg(0U), res);
    } else {
        DoLda64(inst->GetSrcReg(0U), res);
    }

    res.emplace_back(pandasm::Create_UCMP_64(inst->GetSrcReg(1U)));
    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQZ(label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNEZ(label));
            break;
        }
        case compiler::CC_LT: {
            res.emplace_back(pandasm::Create_JLTZ(label));
            break;
        }
        case compiler::CC_LE: {
            res.emplace_back(pandasm::Create_JLEZ(label));
            break;
        }
        case compiler::CC_GT: {
            res.emplace_back(pandasm::Create_JGTZ(label));
            break;
        }
        case compiler::CC_GE: {
            res.emplace_back(pandasm::Create_JGEZ(label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void VisitIfRef([[maybe_unused]] BytecodeGen *enc, compiler::IfInst *inst, std::vector<pandasm::Ins> &res,
                       bool &success)
{
    ASSERT(IsReference(inst->GetInputType(0U)));

    DoLdaObj(inst->GetSrcReg(0U), res);

    compiler::Register src = inst->GetSrcReg(1);
    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQ_OBJ(src, label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNE_OBJ(src, label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

// NOLINTNEXTLINE(readability-function-size)
void BytecodeGen::VisitIf(GraphVisitor *v, Inst *inst_base)
{
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToIf();
    switch (inst->GetInputType(0U)) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32: {
            VisitIf32(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::INT64: {
            VisitIf64Signed(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::UINT64: {
            VisitIf64Unsigned(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::REFERENCE: {
            VisitIfRef(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::ANY: {
            if (enc->GetGraph()->IsDynamicMethod()) {
#if defined(ENABLE_BYTECODE_OPT) && defined(PANDA_WITH_ECMASCRIPT) && defined(ARK_INTRINSIC_SET)
                IfEcma(v, inst);
                break;
#endif
            }
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

#if defined(ENABLE_BYTECODE_OPT) && defined(PANDA_WITH_ECMASCRIPT)
static std::optional<coretypes::TaggedValue> IsEcmaConstTemplate(Inst const *inst)
{
    if (inst->GetOpcode() != compiler::Opcode::CastValueToAnyType) {
        return {};
    }
    auto cvat_inst = inst->CastToCastValueToAnyType();
    if (!cvat_inst->GetInput(0U).GetInst()->IsConst()) {
        return {};
    }
    auto const_inst = cvat_inst->GetInput(0U).GetInst()->CastToConstant();

    switch (cvat_inst->GetAnyType()) {
        case compiler::AnyBaseType::ECMASCRIPT_UNDEFINED_TYPE:
            return coretypes::TaggedValue(coretypes::TaggedValue::VALUE_UNDEFINED);
        case compiler::AnyBaseType::ECMASCRIPT_INT_TYPE:
            return coretypes::TaggedValue(static_cast<int32_t>(const_inst->GetIntValue()));
        case compiler::AnyBaseType::ECMASCRIPT_DOUBLE_TYPE:
            return coretypes::TaggedValue(const_inst->GetDoubleValue());
        case compiler::AnyBaseType::ECMASCRIPT_BOOLEAN_TYPE:
            return coretypes::TaggedValue(static_cast<bool>(const_inst->GetInt64Value() != 0U));
        case compiler::AnyBaseType::ECMASCRIPT_NULL_TYPE:
            return coretypes::TaggedValue(coretypes::TaggedValue::VALUE_NULL);
        default:
            return {};
    }
}

#if defined(ARK_INTRINSIC_SET)
void BytecodeGen::IfEcma(GraphVisitor *v, compiler::IfInst *inst)
{
    auto enc = static_cast<BytecodeGen *>(v);

    compiler::Register reg = compiler::INVALID_REG_ID;
    coretypes::TaggedValue cmp_val;

    auto test_lhs = IsEcmaConstTemplate(inst->GetInput(0U).GetInst());
    auto test_rhs = IsEcmaConstTemplate(inst->GetInput(1U).GetInst());

    if (test_lhs.has_value() && test_lhs->IsBoolean()) {
        cmp_val = test_lhs.value();
        reg = inst->GetSrcReg(1U);
    } else if (test_rhs.has_value() && test_rhs->IsBoolean()) {
        cmp_val = test_rhs.value();
        reg = inst->GetSrcReg(0U);
    } else {
        LOG(ERROR, BYTECODE_OPTIMIZER) << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
        enc->success_ = false;
        return;
    }

    DoLdaDyn(reg, enc->result_);
    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            if (cmp_val.IsTrue()) {
                enc->result_.emplace_back(
                    pandasm::Create_ECMA_JTRUE(LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId())));
            } else {
                enc->result_.emplace_back(
                    pandasm::Create_ECMA_JFALSE(LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId())));
            }
            break;
        }
        case compiler::CC_NE: {
            if (cmp_val.IsTrue()) {
                enc->result_.emplace_back(pandasm::Create_ECMA_ISTRUE());
            } else {
                enc->result_.emplace_back(pandasm::Create_ECMA_ISFALSE());
            }
            enc->result_.emplace_back(
                pandasm::Create_ECMA_JFALSE(LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId())));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
            return;
    }
}
#endif
#endif

void BytecodeGen::VisitIfImm(GraphVisitor *v, Inst *inst_base)
{
    auto inst = inst_base->CastToIfImm();
    auto imm = inst->GetImm();
    if (imm == 0U) {
        IfImmZero(v, inst_base);
        return;
    }
    IfImmNonZero(v, inst_base);
}

static void IfImmZero32(BytecodeGen *enc, compiler::IfImmInst *inst, std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is32Bits(inst->GetInputType(0U), Arch::NONE));

    if (enc->GetGraph()->IsDynamicMethod()) {
        DoLdaDyn(inst->GetSrcReg(0U), res);
    } else {
        DoLda(inst->GetSrcReg(0U), res);
    }

    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQZ(label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNEZ(label));
            break;
        }
        case compiler::CC_LT: {
            res.emplace_back(pandasm::Create_JLTZ(label));
            break;
        }
        case compiler::CC_LE: {
            res.emplace_back(pandasm::Create_JLEZ(label));
            break;
        }
        case compiler::CC_GT: {
            res.emplace_back(pandasm::Create_JGTZ(label));
            break;
        }
        case compiler::CC_GE: {
            res.emplace_back(pandasm::Create_JGEZ(label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void IfImmZeroRef(BytecodeGen *enc, compiler::IfImmInst *inst, std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(IsReference(inst->GetInputType(0U)));

    if (enc->GetGraph()->IsDynamicMethod()) {
        DoLdaDyn(inst->GetSrcReg(0U), res);
    } else {
        DoLdaObj(inst->GetSrcReg(0U), res);
    }

    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQZ_OBJ(label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNEZ_OBJ(label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

// NOLINTNEXTLINE(readability-function-size)
void BytecodeGen::IfImmZero(GraphVisitor *v, Inst *inst_base)
{
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToIfImm();
    switch (inst->GetInputType(0U)) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32: {
            IfImmZero32(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64: {
            IfImm64(v, inst_base);
            break;
        }
        case compiler::DataType::REFERENCE: {
            IfImmZeroRef(enc, inst, enc->result_, enc->success_);
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

static void IfImmNonZero32(BytecodeGen *enc, compiler::IfImmInst *inst, std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is32Bits(inst->GetInputType(0U), Arch::NONE));

    if (enc->GetGraph()->IsDynamicMethod()) {
        res.emplace_back(pandasm::Create_LDAI_DYN(inst->GetImm()));
    } else {
        res.emplace_back(pandasm::Create_LDAI(inst->GetImm()));
    }

    compiler::Register src = inst->GetSrcReg(0);
    std::string label = BytecodeGen::LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            res.emplace_back(pandasm::Create_JEQ(src, label));
            break;
        }
        case compiler::CC_NE: {
            res.emplace_back(pandasm::Create_JNE(src, label));
            break;
        }
        case compiler::CC_LT: {
            res.emplace_back(pandasm::Create_JLT(src, label));
            break;
        }
        case compiler::CC_LE: {
            res.emplace_back(pandasm::Create_JLE(src, label));
            break;
        }
        case compiler::CC_GT: {
            res.emplace_back(pandasm::Create_JGT(src, label));
            break;
        }
        case compiler::CC_GE: {
            res.emplace_back(pandasm::Create_JGE(src, label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

// NOLINTNEXTLINE(readability-function-size)
void BytecodeGen::IfImmNonZero(GraphVisitor *v, Inst *inst_base)
{
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToIfImm();
    switch (inst->GetInputType(0U)) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32: {
            IfImmNonZero32(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64: {
            IfImm64(v, inst_base);
            break;
        }
        case compiler::DataType::REFERENCE: {
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "VisitIfImm does not support non-zero imm of type reference, as no pandasm matches";
            enc->success_ = false;
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

// NOLINTNEXTLINE(readability-function-size)
void BytecodeGen::IfImm64(GraphVisitor *v, Inst *inst_base)
{
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToIfImm();

    if (enc->GetGraph()->IsDynamicMethod()) {
        enc->result_.emplace_back(pandasm::Create_LDAI_DYN(inst->GetImm()));
    } else {
        enc->result_.emplace_back(pandasm::Create_LDAI_64(inst->GetImm()));
    }

    std::string label = LabelName(inst->GetBasicBlock()->GetTrueSuccessor()->GetId());

    switch (inst->GetInputType(0U)) {
        case compiler::DataType::INT64: {
            enc->result_.emplace_back(pandasm::Create_CMP_64(inst->GetSrcReg(0U)));
            break;
        }
        case compiler::DataType::UINT64: {
            enc->result_.emplace_back(pandasm::Create_UCMP_64(inst->GetSrcReg(0U)));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
            return;
    }

    switch (inst->GetCc()) {
        case compiler::CC_EQ: {
            enc->result_.emplace_back(pandasm::Create_JEQZ(label));
            break;
        }
        case compiler::CC_NE: {
            enc->result_.emplace_back(pandasm::Create_JNEZ(label));
            break;
        }
        case compiler::CC_LT: {
            enc->result_.emplace_back(pandasm::Create_JGTZ(label));
            break;
        }
        case compiler::CC_LE: {
            enc->result_.emplace_back(pandasm::Create_JGEZ(label));
            break;
        }
        case compiler::CC_GT: {
            enc->result_.emplace_back(pandasm::Create_JLTZ(label));
            break;
        }
        case compiler::CC_GE: {
            enc->result_.emplace_back(pandasm::Create_JLEZ(label));
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
            return;
    }
}

static void VisitCastFromI32([[maybe_unused]] BytecodeGen *enc, compiler::CastInst *inst,
                             std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is32Bits(inst->GetInputType(0U), Arch::NONE));

    constexpr int64_t SHLI_ASHRI_16 = 16;
    constexpr int64_t SHLI_ASHRI_24 = 24;
    constexpr int64_t ANDI_16 = 0xffff;
    constexpr int64_t ANDI_8 = 0xff;

    if (inst->GetType() != compiler::DataType::UINT32) {
        DoLda(inst->GetSrcReg(0U), res);
    }

    switch (inst->GetType()) {
        case compiler::DataType::FLOAT32: {
            res.emplace_back(pandasm::Create_I32TOF32());
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::FLOAT64: {
            res.emplace_back(pandasm::Create_I32TOF64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT64: {
            res.emplace_back(pandasm::Create_I32TOI64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT32: {
            break;
        }
        case compiler::DataType::INT16: {
            res.emplace_back(pandasm::Create_SHLI(SHLI_ASHRI_16));
            res.emplace_back(pandasm::Create_ASHRI(SHLI_ASHRI_16));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT16: {
            res.emplace_back(pandasm::Create_ANDI(ANDI_16));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT8: {
            res.emplace_back(pandasm::Create_SHLI(SHLI_ASHRI_24));
            res.emplace_back(pandasm::Create_ASHRI(SHLI_ASHRI_24));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT8: {
            res.emplace_back(pandasm::Create_ANDI(ANDI_8));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::ANY: {
            UNREACHABLE();
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void VisitCastFromI64([[maybe_unused]] BytecodeGen *enc, compiler::CastInst *inst,
                             std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is64Bits(inst->GetInputType(0U), Arch::NONE));

    constexpr int64_t SHLI_ASHRI_16 = 16;
    constexpr int64_t SHLI_ASHRI_24 = 24;
    constexpr int64_t ANDI_16 = 0xffff;
    constexpr int64_t ANDI_8 = 0xff;
    constexpr int64_t ANDI_32 = 0xffffffff;

    DoLda64(inst->GetSrcReg(0U), res);

    switch (inst->GetType()) {
        case compiler::DataType::INT32: {
            res.emplace_back(pandasm::Create_I64TOI32());
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT32: {
            res.emplace_back(pandasm::Create_ANDI(ANDI_32));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::FLOAT32: {
            res.emplace_back(pandasm::Create_I64TOF32());
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::FLOAT64: {
            res.emplace_back(pandasm::Create_I64TOF64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT16: {
            res.emplace_back(pandasm::Create_I64TOI32());
            res.emplace_back(pandasm::Create_SHLI(SHLI_ASHRI_16));
            res.emplace_back(pandasm::Create_ASHRI(SHLI_ASHRI_16));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT16: {
            res.emplace_back(pandasm::Create_I64TOI32());
            res.emplace_back(pandasm::Create_ANDI(ANDI_16));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT8: {
            res.emplace_back(pandasm::Create_I64TOI32());
            res.emplace_back(pandasm::Create_SHLI(SHLI_ASHRI_24));
            res.emplace_back(pandasm::Create_ASHRI(SHLI_ASHRI_24));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT8: {
            res.emplace_back(pandasm::Create_I64TOI32());
            res.emplace_back(pandasm::Create_ANDI(ANDI_8));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::ANY: {
            UNREACHABLE();
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void VisitCastFromF32([[maybe_unused]] BytecodeGen *enc, compiler::CastInst *inst,
                             std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is32Bits(inst->GetInputType(0U), Arch::NONE));
    ASSERT(IsFloatType(inst->GetInputType(0U)));

    constexpr int64_t ANDI_32 = 0xffffffff;

    DoLda(inst->GetSrcReg(0U), res);

    switch (inst->GetType()) {
        case compiler::DataType::FLOAT64: {
            res.emplace_back(pandasm::Create_F32TOF64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT64: {
            res.emplace_back(pandasm::Create_F32TOI64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT64: {
            res.emplace_back(pandasm::Create_F32TOU64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT32: {
            res.emplace_back(pandasm::Create_F32TOI32());
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT32: {
            res.emplace_back(pandasm::Create_F32TOU32());
            res.emplace_back(pandasm::Create_ANDI(ANDI_32));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::ANY: {
            UNREACHABLE();
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

static void VisitCastFromF64([[maybe_unused]] BytecodeGen *enc, compiler::CastInst *inst,
                             std::vector<pandasm::Ins> &res, bool &success)
{
    ASSERT(Is64Bits(inst->GetInputType(0U), Arch::NONE));
    ASSERT(IsFloatType(inst->GetInputType(0U)));

    constexpr int64_t ANDI_32 = 0xffffffff;

    DoLda64(inst->GetSrcReg(0U), res);

    switch (inst->GetType()) {
        case compiler::DataType::FLOAT32: {
            res.emplace_back(pandasm::Create_F64TOF32());
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT64: {
            res.emplace_back(pandasm::Create_F64TOI64());
            DoSta64(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::INT32: {
            res.emplace_back(pandasm::Create_F64TOI32());
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::UINT32: {
            res.emplace_back(pandasm::Create_F64TOI64());
            res.emplace_back(pandasm::Create_ANDI(ANDI_32));
            DoSta(inst->GetDstReg(), res);
            break;
        }
        case compiler::DataType::ANY: {
            UNREACHABLE();
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            success = false;
    }
}

// NOLINTNEXTLINE(readability-function-size)
void BytecodeGen::VisitCast(GraphVisitor *v, Inst *inst_base)
{
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToCast();
    switch (inst->GetInputType(0U)) {
        case compiler::DataType::INT32: {
            VisitCastFromI32(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::INT64: {
            VisitCastFromI64(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::FLOAT32: {
            VisitCastFromF32(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::FLOAT64: {
            VisitCastFromF64(enc, inst, enc->result_, enc->success_);
            break;
        }
        case compiler::DataType::REFERENCE: {
            switch (inst->GetType()) {
                case compiler::DataType::ANY: {
                    UNREACHABLE();
                }
                default:
                    LOG(ERROR, BYTECODE_OPTIMIZER)
                        << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
                    enc->success_ = false;
            }
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

void BytecodeGen::VisitLoadString(GraphVisitor *v, Inst *inst_base)
{
    pandasm::Ins ins;
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToLoadString();

    /* Do not emit unused code for Str -> CastValueToAnyType chains */
    if (enc->GetGraph()->IsDynamicMethod()) {
        if (!HasUserPredicate(inst,
                              [](Inst const *i) { return i->GetOpcode() != compiler::Opcode::CastValueToAnyType; })) {
            return;
        }
    }

    enc->result_.emplace_back(pandasm::Create_LDA_STR(enc->ir_interface_->GetStringIdByOffset(inst->GetTypeId())));
    if (inst->GetDstReg() != compiler::ACC_REG_ID) {
        if (enc->GetGraph()->IsDynamicMethod()) {
            enc->result_.emplace_back(pandasm::Create_STA_DYN(inst->GetDstReg()));
        } else {
            enc->result_.emplace_back(pandasm::Create_STA_OBJ(inst->GetDstReg()));
        }
    }
}

void BytecodeGen::VisitReturn(GraphVisitor *v, Inst *inst_base)
{
    pandasm::Ins ins;
    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToReturn();
    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::FLOAT32: {
            DoLda(inst->GetSrcReg(0U), enc->result_);
            enc->result_.emplace_back(pandasm::Create_RETURN());
            break;
        }
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64: {
            DoLda64(inst->GetSrcReg(0U), enc->result_);
            enc->result_.emplace_back(pandasm::Create_RETURN_64());
            break;
        }
        case compiler::DataType::REFERENCE: {
            DoLdaObj(inst->GetSrcReg(0U), enc->result_);
            enc->result_.emplace_back(pandasm::Create_RETURN_OBJ());
            break;
        }
        case compiler::DataType::VOID: {
            enc->result_.emplace_back(pandasm::Create_RETURN_VOID());
            break;
        }
        case compiler::DataType::ANY: {
#if defined(ENABLE_BYTECODE_OPT) && defined(PANDA_WITH_ECMASCRIPT)
            auto test_arg = IsEcmaConstTemplate(inst->GetInput(0U).GetInst());
            if (test_arg.has_value() && test_arg->IsUndefined()) {
                enc->result_.emplace_back(pandasm::Create_ECMA_RETURNUNDEFINED());
                break;
            }
#endif
            DoLdaDyn(inst->GetSrcReg(0U), enc->result_);
#ifdef ARK_INTRINSIC_SET
            enc->result_.emplace_back(pandasm::Create_ECMA_RETURN_DYN());
#else
            // Do not support DataType::ANY in this case
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
#endif  // ARK_INTRINSIC_SET
            break;
        }
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

void BytecodeGen::VisitCastValueToAnyType([[maybe_unused]] GraphVisitor *v, [[maybe_unused]] Inst *inst_base)
{
    auto enc = static_cast<BytecodeGen *>(v);

#if defined(ENABLE_BYTECODE_OPT) && defined(PANDA_WITH_ECMASCRIPT)
    auto cvat = inst_base->CastToCastValueToAnyType();
    switch (cvat->GetAnyType()) {
        case compiler::AnyBaseType::ECMASCRIPT_NULL_TYPE:
            enc->result_.emplace_back(pandasm::Create_ECMA_LDNULL());
            break;
        case compiler::AnyBaseType::ECMASCRIPT_UNDEFINED_TYPE:
            if (!HasUserPredicate(cvat,
                                  [](Inst const *inst) { return inst->GetOpcode() != compiler::Opcode::Return; })) {
                return;
            }
            enc->result_.emplace_back(pandasm::Create_ECMA_LDUNDEFINED());
            break;
        case compiler::AnyBaseType::ECMASCRIPT_INT_TYPE: {
            ASSERT(cvat->GetInput(0U).GetInst()->IsConst());
            auto input = cvat->GetInput(0U).GetInst()->CastToConstant();
            enc->result_.emplace_back(pandasm::Create_LDAI_DYN(input->GetIntValue()));
            break;
        }
        case compiler::AnyBaseType::ECMASCRIPT_DOUBLE_TYPE: {
            ASSERT(cvat->GetInput(0U).GetInst()->IsConst());
            auto input = cvat->GetInput(0U).GetInst()->CastToConstant();
            enc->result_.emplace_back(pandasm::Create_FLDAI_DYN(input->GetDoubleValue()));
            break;
        }
        case compiler::AnyBaseType::ECMASCRIPT_BOOLEAN_TYPE: {
            ASSERT(cvat->GetInput(0U).GetInst()->IsBoolConst());
            auto input = cvat->GetInput(0U).GetInst()->CastToConstant();
            if (!HasUserPredicate(cvat, [](Inst const *inst) { return inst->GetOpcode() != compiler::Opcode::If; })) {
                return;
            }
            uint64_t val = input->GetInt64Value();
            if (val != 0U) {
                enc->result_.emplace_back(pandasm::Create_ECMA_LDTRUE());
            } else {
                enc->result_.emplace_back(pandasm::Create_ECMA_LDFALSE());
            }
            break;
        }
        case compiler::AnyBaseType::ECMASCRIPT_STRING_TYPE: {
            auto input = cvat->GetInput(0U).GetInst()->CastToLoadString();
            enc->result_.emplace_back(
                pandasm::Create_LDA_STR(enc->ir_interface_->GetStringIdByOffset(input->GetTypeId())));
            break;
        }
        case compiler::AnyBaseType::ECMASCRIPT_BIGINT_TYPE: {
            auto input = cvat->GetInput(0U).GetInst()->CastToLoadString();
            enc->result_.emplace_back(
                pandasm::Create_ECMA_LDBIGINT(enc->ir_interface_->GetStringIdByOffset(input->GetTypeId())));
            break;
        }
        default:
            UNREACHABLE();
    }
    DoStaDyn(cvat->GetDstReg(), enc->result_);
#else
    LOG(ERROR, BYTECODE_OPTIMIZER) << "Codegen for " << compiler::GetOpcodeString(inst_base->GetOpcode()) << " failed";
    enc->success_ = false;
#endif
}

// NOLINTNEXTLINE(readability-function-size)
void BytecodeGen::VisitStoreObject(GraphVisitor *v, Inst *inst_base)
{
    if (TryPluginStoreObjectVisitor(v, inst_base)) {
        return;
    }

    auto enc = static_cast<BytecodeGen *>(v);
    const compiler::StoreObjectInst *inst = inst_base->CastToStoreObject();

    compiler::Register vd = inst->GetSrcReg(0U);
    compiler::Register vs = inst->GetSrcReg(1U);
    std::string id = enc->ir_interface_->GetFieldIdByOffset(inst->GetTypeId());

    bool is_acc_type = (vs == compiler::ACC_REG_ID);

    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::FLOAT32:
            if (is_acc_type) {
                enc->result_.emplace_back(pandasm::Create_STOBJ(vd, id));
            } else {
                enc->result_.emplace_back(pandasm::Create_STOBJ_V(vs, vd, id));
            }
            break;
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64:
            if (is_acc_type) {
                enc->result_.emplace_back(pandasm::Create_STOBJ_64(vd, id));
            } else {
                enc->result_.emplace_back(pandasm::Create_STOBJ_V_64(vs, vd, id));
            }
            break;
        case compiler::DataType::REFERENCE:
            if (is_acc_type) {
                enc->result_.emplace_back(pandasm::Create_STOBJ_OBJ(vd, id));
            } else {
                enc->result_.emplace_back(pandasm::Create_STOBJ_V_OBJ(vs, vd, id));
            }
            break;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Wrong DataType for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

void BytecodeGen::VisitStoreStatic(GraphVisitor *v, Inst *inst_base)
{
    if (TryPluginStoreStaticVisitor(v, inst_base)) {
        return;
    }

    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToStoreStatic();

    compiler::Register vs = inst->GetSrcReg(1U);
    std::string id = enc->ir_interface_->GetFieldIdByOffset(inst->GetTypeId());

    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::FLOAT32:
            DoLda(vs, enc->result_);
            enc->result_.emplace_back(pandasm::Create_STSTATIC(id));
            break;
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64:
            DoLda64(vs, enc->result_);
            enc->result_.emplace_back(pandasm::Create_STSTATIC_64(id));
            break;
        case compiler::DataType::REFERENCE:
            DoLdaObj(vs, enc->result_);
            enc->result_.emplace_back(pandasm::Create_STSTATIC_OBJ(id));
            break;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

static bool IsAccLoadObject(const compiler::LoadObjectInst *inst)
{
    return inst->GetDstReg() == compiler::ACC_REG_ID;
}

void BytecodeGen::VisitLoadObject(GraphVisitor *v, Inst *inst_base)
{
    if (TryPluginLoadObjectVisitor(v, inst_base)) {
        return;
    }

    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToLoadObject();

    compiler::Register vs = inst->GetSrcReg(0U);
    compiler::Register vd = inst->GetDstReg();
    std::string id = enc->ir_interface_->GetFieldIdByOffset(inst->GetTypeId());

    bool is_acc_type = IsAccLoadObject(inst);

    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::FLOAT32:
            if (is_acc_type) {
                enc->result_.emplace_back(pandasm::Create_LDOBJ(vs, id));
            } else {
                enc->result_.emplace_back(pandasm::Create_LDOBJ_V(vd, vs, id));
            }
            break;
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64:
            if (is_acc_type) {
                enc->result_.emplace_back(pandasm::Create_LDOBJ_64(vs, id));
            } else {
                enc->result_.emplace_back(pandasm::Create_LDOBJ_V_64(vd, vs, id));
            }
            break;
        case compiler::DataType::REFERENCE:
            if (is_acc_type) {
                enc->result_.emplace_back(pandasm::Create_LDOBJ_OBJ(vs, id));
            } else {
                enc->result_.emplace_back(pandasm::Create_LDOBJ_V_OBJ(vd, vs, id));
            }
            break;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Wrong DataType for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

void BytecodeGen::VisitLoadStatic(GraphVisitor *v, Inst *inst_base)
{
    if (TryPluginLoadStaticVisitor(v, inst_base)) {
        return;
    }

    auto enc = static_cast<BytecodeGen *>(v);
    auto inst = inst_base->CastToLoadStatic();

    compiler::Register vd = inst->GetDstReg();
    std::string id = enc->ir_interface_->GetFieldIdByOffset(inst->GetTypeId());

    switch (inst->GetType()) {
        case compiler::DataType::BOOL:
        case compiler::DataType::UINT8:
        case compiler::DataType::INT8:
        case compiler::DataType::UINT16:
        case compiler::DataType::INT16:
        case compiler::DataType::UINT32:
        case compiler::DataType::INT32:
        case compiler::DataType::FLOAT32:
            enc->result_.emplace_back(pandasm::Create_LDSTATIC(id));
            DoSta(vd, enc->result_);
            break;
        case compiler::DataType::INT64:
        case compiler::DataType::UINT64:
        case compiler::DataType::FLOAT64:
            enc->result_.emplace_back(pandasm::Create_LDSTATIC_64(id));
            DoSta64(vd, enc->result_);
            break;
        case compiler::DataType::REFERENCE:
            enc->result_.emplace_back(pandasm::Create_LDSTATIC_OBJ(id));
            DoStaObj(vd, enc->result_);
            break;
        default:
            LOG(ERROR, BYTECODE_OPTIMIZER)
                << "Codegen for " << compiler::GetOpcodeString(inst->GetOpcode()) << " failed";
            enc->success_ = false;
    }
}

void BytecodeGen::VisitCatchPhi(GraphVisitor *v, Inst *inst)
{
    if (inst->CastToCatchPhi()->IsAcc()) {
        bool has_real_users = false;
        for (auto &user : inst->GetUsers()) {
            if (!user.GetInst()->IsSaveState()) {
                has_real_users = true;
                break;
            }
        }
        if (has_real_users) {
            auto enc = static_cast<BytecodeGen *>(v);
            enc->result_.emplace_back(pandasm::Create_STA_OBJ(inst->GetDstReg()));
        }
    }
}

#include "generated/codegen_intrinsics.cpp"
#include "generated/insn_selection.cpp"
}  // namespace panda::bytecodeopt
