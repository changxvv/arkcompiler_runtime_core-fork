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
//

#include "file_ext.h"
#include "panda_file_external.h"
#include "libpandafile/file-inl.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/method_data_accessor-inl.h"
#include "libpandafile/code_data_accessor-inl.h"
#include "libpandafile/debug_helpers.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/os/native_stack.h"
#include <map>

namespace panda::panda_file::ext {

struct MethodSymEntry {
    panda::panda_file::File::EntityId id;
    uint32_t length {0};
    std::string name;
};

}  // namespace panda::panda_file::ext

#ifdef __cplusplus
extern "C" {
#endif

struct PandaFileExt {
public:
    explicit PandaFileExt(std::unique_ptr<const panda::panda_file::File> &&panda_file)
        : panda_file_(std::move(panda_file))
    {
    }

    size_t GetExtFileLineNumber(panda::panda_file::MethodDataAccessor mda, uint32_t bc_offset)
    {
        return panda::panda_file::debug_helpers::GetLineNumber(mda, bc_offset, panda_file_.get());
    }

    panda::panda_file::ext::MethodSymEntry *QueryMethodSymByOffset(uint64_t offset)
    {
        auto it = method_symbols_.upper_bound(offset);
        if (it != method_symbols_.end() && offset >= it->second.id.GetOffset()) {
            return &it->second;
        }

        // Enmuate all methods and put them to local cache.
        panda::panda_file::ext::MethodSymEntry *found = nullptr;
        for (uint32_t id : panda_file_->GetClasses()) {
            if (panda_file_->IsExternal(panda::panda_file::File::EntityId(id))) {
                continue;
            }
            panda::panda_file::ClassDataAccessor cda {*panda_file_, panda::panda_file::File::EntityId(id)};
            cda.EnumerateMethods([&](panda::panda_file::MethodDataAccessor &mda) -> void {
                if (mda.GetCodeId().has_value()) {
                    panda::panda_file::CodeDataAccessor ca {*panda_file_, mda.GetCodeId().value()};
                    panda::panda_file::ext::MethodSymEntry entry;
                    entry.id = mda.GetCodeId().value();
                    entry.length = ca.GetCodeSize();
                    entry.name =
                        std::string(panda::utf::Mutf8AsCString(panda_file_->GetStringData(mda.GetNameId()).data));

                    auto ret = method_symbols_.emplace(offset, entry);
                    if (mda.GetCodeId().value().GetOffset() <= offset &&
                        offset < mda.GetCodeId().value().GetOffset() + ca.GetCodeSize()) {
                        found = &ret.first->second;
                    }
                }
            });
        }
        return found;
    }

    panda::panda_file::ext::MethodSymEntry *QueryMethodSymAndLineByOffset(uint64_t offset)
    {
        auto it = method_symbols_.upper_bound(offset);
        if (it != method_symbols_.end() && offset >= it->second.id.GetOffset()) {
            return &it->second;
        }

        // Enmuate all methods and put them to local cache.
        panda::panda_file::ext::MethodSymEntry *found = nullptr;
        for (uint32_t id : panda_file_->GetClasses()) {
            if (panda_file_->IsExternal(panda::panda_file::File::EntityId(id))) {
                continue;
            }
            panda::panda_file::ClassDataAccessor cda {*panda_file_, panda::panda_file::File::EntityId(id)};
            cda.EnumerateMethods([&](panda::panda_file::MethodDataAccessor &mda) -> void {
                if (mda.GetCodeId().has_value()) {
                    panda::panda_file::CodeDataAccessor ca {*panda_file_, mda.GetCodeId().value()};
                    panda::panda_file::ext::MethodSymEntry entry;
                    entry.id = mda.GetCodeId().value();
                    entry.length = ca.GetCodeSize();
                    entry.name =
                        std::string(panda::utf::Mutf8AsCString(panda_file_->GetStringData(mda.GetNameId()).data));

                    auto ret = method_symbols_.emplace(offset, entry);
                    if (mda.GetCodeId().value().GetOffset() <= offset &&
                        offset < mda.GetCodeId().value().GetOffset() + ca.GetCodeSize()) {
                        size_t line_number = GetExtFileLineNumber(mda, offset - mda.GetCodeId().value().GetOffset());
                        found = &ret.first->second;
                        panda::panda_file::File::EntityId id_new(line_number);
                        auto name_id = cda.GetDescriptor();
                        found->id = id_new;
                        found->name =
                            panda::os::native_stack::ChangeJaveStackFormat(reinterpret_cast<const char *>(name_id)) +
                            "." + found->name;
                    }
                }
            });
        }
        return found;
    }

    std::vector<struct MethodSymInfoExt> QueryAllMethodSyms()
    {
        // Enmuate all methods and put them to local cache.
        std::vector<panda::panda_file::ext::MethodSymEntry> res;
        for (uint32_t id : panda_file_->GetClasses()) {
            if (panda_file_->IsExternal(panda::panda_file::File::EntityId(id))) {
                continue;
            }
            panda::panda_file::ClassDataAccessor cda {*panda_file_, panda::panda_file::File::EntityId(id)};
            cda.EnumerateMethods([&](panda::panda_file::MethodDataAccessor &mda) -> void {
                if (mda.GetCodeId().has_value()) {
                    panda::panda_file::CodeDataAccessor ca {*panda_file_, mda.GetCodeId().value()};
                    std::stringstream ss;
                    std::string_view cname(
                        panda::utf::Mutf8AsCString(panda_file_->GetStringData(mda.GetClassId()).data));
                    if (!cname.empty()) {
                        ss << cname.substr(0, cname.size() - 1);
                    }
                    ss << "." << panda::utf::Mutf8AsCString(panda_file_->GetStringData(mda.GetNameId()).data);
                    res.push_back({mda.GetCodeId().value(), ca.GetCodeSize(), ss.str()});
                }
            });
        }

        std::vector<struct MethodSymInfoExt> method_info;
        method_info.reserve(res.size());
        for (auto const &me : res) {
            struct MethodSymInfoExt msi {};
            msi.offset = me.id.GetOffset();
            msi.length = me.length;
            msi.name = me.name;
            method_info.push_back(msi);
        }
        return method_info;
    }

private:
    std::map<uint64_t, panda::panda_file::ext::MethodSymEntry> method_symbols_;
    std::unique_ptr<const panda::panda_file::File> panda_file_;
};

bool OpenPandafileFromMemoryExt(void *addr, const uint64_t *size, [[maybe_unused]] const std::string &file_name,
                                PandaFileExt **panda_file_ext)
{
    if (panda_file_ext == nullptr) {
        return false;
    }

    panda::os::mem::ConstBytePtr ptr(
        reinterpret_cast<std::byte *>(addr), *size,
        []([[maybe_unused]] std::byte *param_buffer, [[maybe_unused]] size_t param_size) noexcept {});
    auto pf = panda::panda_file::File::OpenFromMemory(std::move(ptr));
    if (pf == nullptr) {
        return false;
    }

    auto pf_ext = std::make_unique<PandaFileExt>(std::move(pf));
    *panda_file_ext = pf_ext.release();
    return true;
}

bool OpenPandafileFromFdExt([[maybe_unused]] int fd, [[maybe_unused]] uint64_t offset, const std::string &file_name,
                            PandaFileExt **panda_file_ext)
{
    auto pf = panda::panda_file::OpenPandaFile(file_name);
    if (pf == nullptr) {
        return false;
    }

    auto pf_ext = std::make_unique<PandaFileExt>(std::move(pf));
    *panda_file_ext = pf_ext.release();
    return true;
}

bool QueryMethodSymByOffsetExt(struct PandaFileExt *pf, uint64_t offset, struct MethodSymInfoExt *method_info)
{
    auto entry = pf->QueryMethodSymByOffset(offset);
    if ((entry != nullptr) && (method_info != nullptr)) {
        method_info->offset = entry->id.GetOffset();
        method_info->length = entry->length;
        method_info->name = entry->name;
        return true;
    }
    return false;
}

bool QueryMethodSymAndLineByOffsetExt(struct PandaFileExt *pf, uint64_t offset, struct MethodSymInfoExt *method_info)
{
    auto entry = pf->QueryMethodSymAndLineByOffset(offset);
    if ((entry != nullptr) && (method_info != nullptr)) {
        method_info->offset = entry->id.GetOffset();
        method_info->length = entry->length;
        method_info->name = entry->name;
        return true;
    }
    return false;
}

void QueryAllMethodSymsExt(PandaFileExt *pf, MethodSymInfoExtCallBack callback, void *user_data)
{
    auto method_infos = pf->QueryAllMethodSyms();
    for (auto mi : method_infos) {
        callback(&mi, user_data);
    }
}

#ifdef __cplusplus
}  // extern "C"
#endif
