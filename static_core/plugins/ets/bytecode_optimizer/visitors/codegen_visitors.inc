/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License"
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

static panda::compiler::CallInst *CastToCallLaunch(panda::compiler::Inst *inst)
{
    switch (inst->GetOpcode()) {
        case compiler::Opcode::CallLaunchStatic:
            return inst->CastToCallLaunchStatic();
        case compiler::Opcode::CallLaunchVirtual:
            return inst->CastToCallLaunchVirtual();
        default:
            UNREACHABLE();
    }
}

static panda::pandasm::Opcode ChooseCallLaunchOpcode(panda::compiler::Opcode op, size_t nargs)
{
    ASSERT(op == compiler::Opcode::CallLaunchStatic || op == compiler::Opcode::CallLaunchVirtual);
    if (nargs > MAX_NUM_NON_RANGE_ARGS) {
        switch (op) {
            case compiler::Opcode::CallLaunchStatic:
                return pandasm::Opcode::ETS_LAUNCH_RANGE;
            case compiler::Opcode::CallLaunchVirtual:
                return pandasm::Opcode::ETS_LAUNCH_VIRT_RANGE;
            default:
                UNREACHABLE();
        }
    } else if (nargs > MAX_NUM_SHORT_CALL_ARGS) {
        switch (op) {
            case compiler::Opcode::CallLaunchStatic:
                return pandasm::Opcode::ETS_LAUNCH;
            case compiler::Opcode::CallLaunchVirtual:
                return pandasm::Opcode::ETS_LAUNCH_VIRT;
            default:
                UNREACHABLE();
        }
    }
    switch (op) {
        case compiler::Opcode::CallLaunchStatic:
            return pandasm::Opcode::ETS_LAUNCH_SHORT;
        case compiler::Opcode::CallLaunchVirtual:
            return pandasm::Opcode::ETS_LAUNCH_VIRT_SHORT;
        default:
            UNREACHABLE();
    }
}

static void CallLaunchHandler(panda::compiler::GraphVisitor *visitor, panda::compiler::Inst *inst)
{
    auto op = inst->GetOpcode();
    ASSERT(op == panda::compiler::Opcode::CallLaunchStatic || op == panda::compiler::Opcode::CallLaunchVirtual);
    auto *enc = static_cast<BytecodeGen *>(visitor);
    auto call_inst = CastToCallLaunch(inst);
    auto sf_count = inst->GetInputsCount() - (inst->RequireState() ? 1 : 0);
    size_t start = 1;
    auto nargs = sf_count - start;
    pandasm::Ins ins;
    ins.opcode = ChooseCallLaunchOpcode(op, nargs);

    auto zero_arg = inst->GetInput(0).GetInst();
    ASSERT(zero_arg->GetOpcode() == Opcode::NewObject);
    auto newobj_inst = zero_arg->CastToNewObject();

    if (nargs > MAX_NUM_NON_RANGE_ARGS) {
#ifndef NDEBUG
        auto start_reg = inst->GetSrcReg(start);
        ASSERT(start_reg <= MAX_8_BIT_REG);
        for (size_t i = start; i < sf_count; ++i) {
            auto reg = inst->GetSrcReg(i);
            ASSERT(reg - start_reg == static_cast<int>(i - start));  // check 'range-ness' of registers
        }
#endif  // !NDEBUG
        ins.regs.emplace_back(inst->GetSrcReg(start));
    } else {
        for (size_t i = start; i < sf_count; ++i) {
            auto reg = inst->GetSrcReg(i);
            ASSERT(reg < NUM_COMPACTLY_ENCODED_REGS);
            ins.regs.emplace_back(reg);
        }
    }
    ins.ids.emplace_back(enc->ir_interface_->GetMethodIdByOffset(call_inst->GetCallMethodId()));
    enc->result_.emplace_back(ins);
    if (newobj_inst->GetDstReg() != compiler::ACC_REG_ID) {
        enc->result_.emplace_back(pandasm::Create_STA_OBJ(newobj_inst->GetDstReg()));
    }
}

static void VisitCallLaunchStatic(panda::compiler::GraphVisitor *visitor, panda::compiler::Inst *inst)
{
    CallLaunchHandler(visitor, inst);
}

static void VisitCallLaunchVirtual(GraphVisitor *visitor, Inst *inst)
{
    CallLaunchHandler(visitor, inst);
}

static void VisitEtsLdObjByName(BytecodeGen *enc, compiler::IntrinsicInst *inst)
{
    auto v0 = inst->GetSrcReg(0);
    auto bc_id0 = enc->ir_interface_->GetFieldIdByOffset(static_cast<uint32_t>(inst->GetImms()[0]));
    switch (inst->GetIntrinsicId()) {
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_LD_OBJ_BY_NAME_I32:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_LD_OBJ_BY_NAME_F32:
            enc->result_.emplace_back(pandasm::Create_ETS_LDOBJ_NAME(v0, bc_id0));
            break;
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_LD_OBJ_BY_NAME_I64:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_LD_OBJ_BY_NAME_F64:
            enc->result_.emplace_back(pandasm::Create_ETS_LDOBJ_NAME_64(v0, bc_id0));
            break;
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_LD_OBJ_BY_NAME_OBJ:
            enc->result_.emplace_back(pandasm::Create_ETS_LDOBJ_NAME_OBJ(v0, bc_id0));
            break;
        default:
            UNREACHABLE();
    }
    DoSta(inst->GetDstReg(), enc->result_);
}

static void VisitEtsStObjByName(BytecodeGen *enc, compiler::IntrinsicInst *inst)
{
    DoLda(inst->GetSrcReg(1U), enc->result_);
    auto v0 = inst->GetSrcReg(0);
    auto bc_id0 = enc->ir_interface_->GetFieldIdByOffset(static_cast<uint32_t>(inst->GetImms()[0]));
    switch (inst->GetIntrinsicId()) {
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_ST_OBJ_BY_NAME_I32:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_ST_OBJ_BY_NAME_F32:
            enc->result_.emplace_back(pandasm::Create_ETS_STOBJ_NAME(v0, bc_id0));
            break;
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_ST_OBJ_BY_NAME_I64:
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_ST_OBJ_BY_NAME_F64:
            enc->result_.emplace_back(pandasm::Create_ETS_STOBJ_NAME_64(v0, bc_id0));
            break;
        case compiler::RuntimeInterface::IntrinsicId::INTRINSIC_COMPILER_ETS_ST_OBJ_BY_NAME_OBJ:
            enc->result_.emplace_back(pandasm::Create_ETS_STOBJ_NAME_OBJ(v0, bc_id0));
            break;
        default:
            UNREACHABLE();
    }
}

static void VisitEtsLdundefined(BytecodeGen *enc, compiler::IntrinsicInst *inst)
{
    if (inst->GetDstReg() == compiler::ACC_REG_ID) {
        enc->result_.emplace_back(pandasm::Create_ETS_LDUNDEFINED());
    } else {
        enc->result_.emplace_back(pandasm::Create_ETS_MOVUNDEFINED(inst->GetDstReg()));
    }
}

static void VisitEtsIsundefined(BytecodeGen *enc, compiler::IntrinsicInst *inst)
{
    DoLdaObj(inst->GetSrcReg(0), enc->result_);
    enc->result_.emplace_back(pandasm::Create_ETS_ISUNDEFINED());
    DoSta(inst->GetDstReg(), enc->result_);
}
