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

#ifndef COMPILER_OPTIMIZER_OPTIMIZATIONS_CONDITION_CHAIN_CACHE_H
#define COMPILER_OPTIMIZER_OPTIMIZATIONS_CONDITION_CHAIN_CACHE_H

#include "libpandabase/utils/arena_containers.h"

namespace panda::compiler {
class ConditionChain;
class Inst;

class ConditionChainCache {
public:
    explicit ConditionChainCache(ArenaAllocator *allocator);
    void Insert(ConditionChain *chain, Inst *phi_inst);
    Inst *FindPhi(const ConditionChain *chain);
    void Clear();

private:
    ArenaMap<ConditionChain *, Inst *> cache_;
};
}  // namespace panda::compiler

#endif  // COMPILER_OPTIMIZER_OPTIMIZATIONS_CONDITION_CHAIN_CACHE_H
