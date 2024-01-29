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

#ifndef LIBLLVMBACKEND_TRANSFORMS_BUILTINS_H
#define LIBLLVMBACKEND_TRANSFORMS_BUILTINS_H

#include <llvm/ADT/Triple.h>
#include <llvm/IR/IRBuilder.h>

namespace panda::llvmbackend {
class LLVMArkInterface;
}  // namespace panda::llvmbackend

namespace panda::llvmbackend::builtins {
llvm::Function *LenArray(llvm::Module *module);
llvm::Function *LoadClass(llvm::Module *module);
llvm::Function *LoadInitClass(llvm::Module *module);
llvm::Function *PreWRB(llvm::Module *module, unsigned addrSpace);
llvm::Function *PostWRB(llvm::Module *module);
llvm::Function *LoadString(llvm::Module *module);
llvm::Function *ResolveVirtual(llvm::Module *module);
llvm::Value *LowerBuiltin(llvm::IRBuilder<> *builder, llvm::CallInst *inst,
                          panda::llvmbackend::LLVMArkInterface *arkInterface);
constexpr auto BUILTIN_SECTION = ".builtins";
constexpr auto LEN_ARRAY_BUILTIN = "__builtin_lenarray";
constexpr auto LOAD_CLASS_BUILTIN = "__builtin_load_class";
constexpr auto LOAD_INIT_CLASS_BUILTIN = "__builtin_load_init_class";
constexpr auto PRE_WRB_BUILTIN = "__builtin_pre_wrb";
constexpr auto PRE_WRB_GCADR_BUILTIN = "__builtin_pre_wrb_gcadr";
constexpr auto POST_WRB_BUILTIN = "__builtin_post_wrb";
constexpr auto LOAD_STRING_BUILTIN = "__builtin_load_string";
constexpr auto RESOLVE_VIRTUAL_BUILTIN = "__builtin_resolve_virtual";
}  // namespace panda::llvmbackend::builtins

#endif  // LIBLLVMBACKEND_TRANSFORMS_BUILTINS_H
