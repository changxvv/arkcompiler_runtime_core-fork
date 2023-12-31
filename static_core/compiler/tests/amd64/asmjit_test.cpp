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

#include "gtest/gtest.h"

#include "asmjit/x86.h"

#include "mem/pool_manager.h"

namespace panda::compiler {
using namespace asmjit;  // NOLINT(google-build-using-namespace)

class AsmJitTest : public ::testing::Test {
public:
    AsmJitTest()
    {
        // NOLINTNEXTLINE(readability-magic-numbers)
        panda::mem::MemConfig::Initialize(64_MB, 64_MB, 64_MB, 32_MB, 0, 0);
        PoolManager::Initialize();
        allocator_ = new ArenaAllocator(SpaceType::SPACE_TYPE_COMPILER);
    }

    ~AsmJitTest() override
    {
        delete allocator_;
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }

    NO_MOVE_SEMANTIC(AsmJitTest);
    NO_COPY_SEMANTIC(AsmJitTest);

    ArenaAllocator *GetAllocator()
    {
        return allocator_;
    }

private:
    ArenaAllocator *allocator_ {nullptr};
};

TEST_F(AsmJitTest, HelloWorld)
{
    // Runtime designed for JIT code execution.
    JitRuntime rt;

    // Holds code and relocation information.
    CodeHolder code(GetAllocator());
    code.init(rt.environment());

    x86::Assembler a(&code);
    a.mov(x86::rax, 1);
    a.ret();

    // Signature of the generated function.
    using Func = int (*)();
    Func fn {nullptr};

    // Add the generated code to the runtime.
    Error err = rt.add(&fn, &code);
    ASSERT_FALSE(err);

    int result {fn()};
    ASSERT_EQ(1, result);
}

TEST_F(AsmJitTest, Add)
{
    // Runtime designed for JIT code execution.
    JitRuntime rt;

    // Holds code and relocation information.
    CodeHolder code(GetAllocator());
    code.init(rt.environment());

    // Generating code:
    x86::Assembler a(&code);
    x86::Gp lhs = a.zax();
    x86::Gp rhs = a.zcx();

    FuncDetail func;
    func.init(FuncSignatureT<size_t, size_t, size_t>(CallConv::kIdHost), code.environment());

    FuncFrame frame;
    frame.init(func);
    frame.addDirtyRegs(lhs, rhs);

    FuncArgsAssignment args(&func);
    args.assignAll(lhs, rhs);
    args.updateFuncFrame(frame);
    frame.finalize();

    a.emitProlog(frame);
    a.emitArgsAssignment(frame, args);
    a.add(lhs, rhs);
    a.emitEpilog(frame);

    // Signature of the generated function.
    using Func = size_t (*)(size_t, size_t);
    Func fn {nullptr};

    // Add the generated code to the runtime.
    Error err = rt.add(&fn, &code);
    ASSERT_FALSE(err);

    size_t result {fn(size_t(2), size_t(3))};
    ASSERT_EQ(size_t(5), result);
}

TEST_F(AsmJitTest, Add2)
{
    JitRuntime rt;

    CodeHolder code(GetAllocator());
    code.init(rt.environment());

    // Generating code:
    x86::Assembler a(&code);
    a.add(x86::rdi, x86::rsi);
    a.mov(x86::rax, x86::rdi);
    a.ret();

    // Signature of the generated function.
    using Func = size_t (*)(size_t, size_t);
    Func fn {nullptr};

    // Add the generated code to the runtime.
    Error err = rt.add(&fn, &code);
    ASSERT_FALSE(err);

    size_t result {fn(size_t(2), size_t(3))};
    ASSERT_EQ(size_t(5), result);
}

TEST_F(AsmJitTest, AddDouble)
{
    JitRuntime rt;

    CodeHolder code(GetAllocator());
    code.init(rt.environment());

    // Generating code:
    x86::Assembler a(&code);
    a.addsd(x86::xmm0, x86::xmm1);
    a.ret();

    // Signature of the generated function.
    using Func = double (*)(double, double);
    Func fn {nullptr};

    // Add the generated code to the runtime.
    Error err = rt.add(&fn, &code);
    ASSERT_FALSE(err);

    double result {fn(2.0, 3.0)};  // NOLINT(readability-magic-numbers)
    ASSERT_EQ(5.0, result);
}

TEST_F(AsmJitTest, AddExplicit)
{
    Environment env = hostEnvironment();
    JitAllocator allocator;

    CodeHolder code(GetAllocator());
    code.init(env);

    // Generating code:
    x86::Assembler a(&code);
    x86::Gp lhs = a.zax();
    x86::Gp rhs = a.zcx();

    FuncDetail func;
    func.init(FuncSignatureT<size_t, size_t, size_t>(CallConv::kIdHost), code.environment());

    FuncFrame frame;
    frame.init(func);
    frame.addDirtyRegs(lhs, rhs);

    FuncArgsAssignment args(&func);
    args.assignAll(lhs, rhs);
    args.updateFuncFrame(frame);
    frame.finalize();

    a.emitProlog(frame);
    a.emitArgsAssignment(frame, args);
    a.add(lhs, rhs);
    a.emitEpilog(frame);

    code.flatten();
    code.resolveUnresolvedLinks();
    size_t estimated_size = code.codeSize();

    // Allocate memory for the function and relocate it there.
    void *ro_ptr;
    void *rw_ptr;
    Error err = allocator.alloc(&ro_ptr, &rw_ptr, estimated_size);
    ASSERT_FALSE(err);

    // Relocate to the base-address of the allocated memory.
    code.relocateToBase(reinterpret_cast<uintptr_t>(rw_ptr));
    size_t code_size = code.codeSize();

    code.copyFlattenedData(rw_ptr, code_size, CodeHolder::kCopyPadSectionBuffer);

    // Execute the function and test whether it works.
    using Func = size_t (*)(size_t lhs, size_t rhs);
    Func fn = reinterpret_cast<Func>(ro_ptr);

    size_t result {fn(size_t(2), size_t(3))};
    ASSERT_EQ(size_t(5), result);

    err = allocator.release(ro_ptr);
    ASSERT_FALSE(err);
}
}  // namespace panda::compiler
