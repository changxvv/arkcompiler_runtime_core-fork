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

#ifndef PANDA_OBJECT_TYPE_PROPAGATION_H
#define PANDA_OBJECT_TYPE_PROPAGATION_H

#include "optimizer/pass.h"
#include "optimizer/ir/graph.h"
#include "optimizer/ir/graph_visitor.h"

namespace panda::compiler {
// NOLINTNEXTLINE(fuchsia-multiple-inheritance)
class ObjectTypePropagation final : public Analysis, public GraphVisitor {
public:
    explicit ObjectTypePropagation(Graph *graph) : Analysis(graph), visited_phis_(graph->GetLocalAllocator()->Adapter())
    {
    }
    NO_MOVE_SEMANTIC(ObjectTypePropagation);
    NO_COPY_SEMANTIC(ObjectTypePropagation);
    ~ObjectTypePropagation() override = default;

    const ArenaVector<BasicBlock *> &GetBlocksToVisit() const override
    {
        return GetGraph()->GetBlocksRPO();
    }

    const char *GetPassName() const override
    {
        return "ObjectTypePropagation";
    }

    bool RunImpl() override;

protected:
    static void VisitNewObject(GraphVisitor *v, Inst *inst);
    static void VisitParameter(GraphVisitor *v, Inst *inst);
    static void VisitNewArray(GraphVisitor *v, Inst *inst);
    static void VisitLoadArray(GraphVisitor *v, Inst *inst);
    static void VisitLoadString(GraphVisitor *v, Inst *inst);
    static void VisitLoadObject(GraphVisitor *v, Inst *inst);
    static void VisitCallStatic(GraphVisitor *v, Inst *inst);
    static void VisitCallVirtual(GraphVisitor *v, Inst *inst);
    static void VisitNullCheck(GraphVisitor *v, Inst *inst);
    static void VisitRefTypeCheck(GraphVisitor *v, Inst *inst);

#include "optimizer/ir/visitor.inc"

private:
    static void ProcessManagedCall(GraphVisitor *v, CallInst *inst);
    ObjectTypeInfo GetPhiTypeInfo(Inst *inst);

private:
    InstVector visited_phis_;
    Marker visited_ {UNDEF_MARKER};
};
}  // namespace panda::compiler

#endif  // PANDA_OBJECT_TYPE_PROPAGATION_H
