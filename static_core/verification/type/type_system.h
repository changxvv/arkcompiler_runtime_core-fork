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

#ifndef _PANDA_TYPE_SYSTEM_HPP
#define _PANDA_TYPE_SYSTEM_HPP

#include "macros.h"
#include "runtime/include/class.h"
#include "runtime/include/language_context.h"
#include "runtime/include/method.h"
#include "runtime/include/mem/panda_containers.h"

#include "verification/type/type_type.h"
#include "verification/value/variables.h"

#include <memory>
#include <variant>
#include <functional>
#include <algorithm>

namespace panda::verifier::plugin {
class Plugin;
}  // namespace panda::verifier::plugin

namespace panda::verifier {
class VerifierService;

/*
Design decision:
1. There are special initial and final types, named as Bot and Top, and all types are implicitly related
   as Bot <: type <: Top
*/

class TypeSystem {
public:
    // NOTE(vdyadov): change Id to hash from filename_id and entity id
    using TypeId = panda_file::Type::TypeId;

    explicit TypeSystem(VerifierService *service, panda_file::SourceLang lang = panda_file::SourceLang::PANDA_ASSEMBLY);

    NO_COPY_SEMANTIC(TypeSystem);
    DEFAULT_MOVE_SEMANTIC(TypeSystem);
    ~TypeSystem() = default;

    void SetSupertypeOfArray(Type supertype)
    {
        supertype_of_array_ = supertype;
    }

    Type SupertypeOfArray() const
    {
        return supertype_of_array_;
    }
    Type Object() const
    {
        return object_;
    }
    Type ClassClass() const
    {
        return class_;
    }
    Type Throwable() const
    {
        return throwable_;
    }

    Variables::Var NewVar()
    {
        return variables_.NewVar();
    }

    Type NormalizedTypeOf(Type type);
    MethodSignature const *GetMethodSignature(Method const *method);

    PandaUnorderedSet<Type> const *SupertypesOfClass(Class const *klass);

    void DisplayTypeSystem(std::function<void(PandaString const &)> const &handler);

    /* Make TypeSystem remember the class for dumping */
    void MentionClass(Class const *klass);

    Type DescriptorToType(uint8_t const *descr);

    void ResetTypeSpans()
    {
        type_spans_.clear();
    }

    Span<Type const> GetTypeSpan(size_t start, size_t sz) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return {type_spans_.data() + start, sz};
    }

    Span<Type> GetNewTypeSpan(size_t sz)
    {
        size_t start = type_spans_.size();
        type_spans_.resize(start + sz);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return {type_spans_.data() + start, sz};
    }

    size_t GetSpanIndex(Span<Type> span)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return span.begin() - type_spans_.data();
    }

    Class const *DescriptorToClass(uint8_t const *descr);

private:
    VerifierService *service_;
    plugin::Plugin const *plugin_;
    LanguageContext lang_ctx_;
    ClassLinkerContext *linker_ctx_;

    PandaUnorderedMap<Type, Type> normalized_type_of_;
    PandaUnorderedMap<Method::UniqId, Method const *> method_of_id_;
    PandaUnorderedMap<Method::UniqId, MethodSignature> signature_of_method_;
    PandaUnorderedMap<Class const *, PandaUnorderedSet<Type>> supertypes_cache_;
    PandaUnorderedSet<Class const *> known_classes_;

    // Storage for members of intersection and union types.
    PandaVector<Type> type_spans_;

    Type supertype_of_array_;
    Type object_;
    Type class_;
    Type throwable_;
    Variables variables_;

    void ExtendBySupers(PandaUnorderedSet<Type> *set, Class const *klass);

    void DisplayClasses(std::function<void(PandaString const &)> const &handler) const;
    void DisplayMethods(std::function<void(PandaString const &, PandaString const &)> const &handler) const;
    void DisplaySubtyping(std::function<void(PandaString const &, PandaString const &)> const &handler);
};

}  // namespace panda::verifier

#endif  // !_PANDA_TYPE_SYSTEM_HPP
