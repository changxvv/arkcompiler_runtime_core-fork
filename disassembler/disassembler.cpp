/*
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

#include "disassembler.h"
#include "mangling.h"
#include "utils/logger.h"

#include <iomanip>

#include "get_language_specific_metadata.inc"

namespace panda::disasm {

void Disassembler::Disassemble(const std::string &filename_in, const bool quiet, const bool skip_strings)
{
    auto file_new = panda_file::File::Open(filename_in);
    file_.swap(file_new);

    if (file_ != nullptr) {
        prog_ = pandasm::Program {};

        record_name_to_id_.clear();
        method_name_to_id_.clear();

        skip_strings_ = skip_strings;
        quiet_ = quiet;

        prog_info_ = ProgInfo {};

        prog_ann_ = ProgAnnotations {};

        GetRecords();
        GetLiteralArrays();

        GetLanguageSpecificMetadata();
    } else {
        LOG(ERROR, DISASSEMBLER) << "> unable to open specified pandafile: <" << filename_in << ">";
    }
}

void Disassembler::CollectInfo()
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting program info]\n";

    debug_info_extractor_ = std::make_unique<panda_file::DebugInfoExtractor>(file_.get());

    for (const auto &pair : record_name_to_id_) {
        GetRecordInfo(pair.second, &prog_info_.records_info[pair.first]);
    }

    for (const auto &pair : method_name_to_id_) {
        GetMethodInfo(pair.second, &prog_info_.methods_info[pair.first]);
    }
}

void Disassembler::Serialize(std::ostream &os, bool add_separators, bool print_information) const
{
    if (os.bad()) {
        LOG(DEBUG, DISASSEMBLER) << "> serialization failed. os bad\n";

        return;
    }

    if (file_ != nullptr) {
        std::string abc_file = GetFileNameByAbsolutePath(file_->GetFilename());
        os << "# source binary: " << abc_file << "\n\n";
    }

    SerializeLanguage(os);

    if (add_separators) {
        os << "# ====================\n"
              "# LITERALS\n\n";
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing literals]";

    for (const auto &[key, lit_arr] : prog_.literalarray_table) {
        Serialize(key, lit_arr, os);
    }

    for (const auto &[module_offset, array_table] : modulearray_table_) {
        Serialize(module_offset, array_table, os);
    }

    os << "\n";

    if (add_separators) {
        os << "# ====================\n"
              "# RECORDS\n\n";
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing records]";

    for (const auto &r : prog_.record_table) {
        Serialize(r.second, os, print_information);
    }

    if (add_separators) {
        os << "# ====================\n"
              "# METHODS\n\n";
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing methods]";

    for (const auto &m : prog_.function_table) {
        Serialize(m.second, os, print_information);
    }
}

inline bool Disassembler::IsSystemType(const std::string &type_name)
{
    bool is_array_type = type_name.find('[') != std::string::npos;
    bool is_global = type_name == "_GLOBAL";

    return is_array_type || is_global;
}

void Disassembler::GetRecord(pandasm::Record *record, const panda_file::File::EntityId &record_id)
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting record]\nid: " << record_id << " (0x" << std::hex << record_id << ")";

    if (record == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but record ptr expected!";

        return;
    }

    record->name = GetFullRecordName(record_id);

    LOG(DEBUG, DISASSEMBLER) << "name: " << record->name;

    GetMetaData(record, record_id);

    if (!file_->IsExternal(record_id)) {
        GetMethods(record_id);
        GetFields(record, record_id);
    }
}

void Disassembler::AddMethodToTables(const panda_file::File::EntityId &method_id)
{
    pandasm::Function new_method("", file_language_);
    GetMethod(&new_method, method_id);

    const auto signature = pandasm::GetFunctionSignatureFromName(new_method.name, new_method.params);
    if (prog_.function_table.find(signature) != prog_.function_table.end()) {
        return;
    }

    method_name_to_id_.emplace(signature, method_id);
    prog_.function_synonyms[new_method.name].push_back(signature);
    prog_.function_table.emplace(signature, std::move(new_method));
}

void Disassembler::GetMethod(pandasm::Function *method, const panda_file::File::EntityId &method_id)
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting method]\nid: " << method_id << " (0x" << std::hex << method_id << ")";

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but method ptr expected!";

        return;
    }

    panda_file::MethodDataAccessor method_accessor(*file_, method_id);

    method->name = GetFullMethodName(method_id);

    LOG(DEBUG, DISASSEMBLER) << "name: " << method->name;

    GetParams(method, method_accessor.GetProtoId());
    GetMetaData(method, method_id);

    if (method_accessor.GetCodeId().has_value()) {
        const IdList id_list = GetInstructions(method, method_id, method_accessor.GetCodeId().value());

        for (const auto &id : id_list) {
            AddMethodToTables(id);
        }
    } else {
        LOG(ERROR, DISASSEMBLER) << "> error encountered at " << method_id << " (0x" << std::hex << method_id
                                 << "). implementation of method expected, but no \'CODE\' tag was found!";

        return;
    }
}

template <typename T>
void Disassembler::FillLiteralArrayData(pandasm::LiteralArray *lit_array, const panda_file::LiteralTag &tag,
                                        const panda_file::LiteralDataAccessor::LiteralValue &value) const
{
    panda_file::File::EntityId id(std::get<uint32_t>(value));
    auto sp = file_->GetSpanFromId(id);
    auto len = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
    if (tag != panda_file::LiteralTag::ARRAY_STRING) {
        for (size_t i = 0; i < len; i++) {
            pandasm::LiteralArray::Literal lit;
            lit.tag_ = tag;
            lit.value_ = bit_cast<T>(panda_file::helpers::Read<sizeof(T)>(&sp));
            lit_array->literals_.push_back(lit);
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        auto str_id = panda_file::helpers::Read<sizeof(T)>(&sp);
        pandasm::LiteralArray::Literal lit;
        lit.tag_ = tag;
        lit.value_ = StringDataToString(file_->GetStringData(panda_file::File::EntityId(str_id)));
        lit_array->literals_.push_back(lit);
    }
}

void Disassembler::FillLiteralData(pandasm::LiteralArray *lit_array,
                                   const panda_file::LiteralDataAccessor::LiteralValue &value,
                                   const panda_file::LiteralTag &tag) const
{
    pandasm::LiteralArray::Literal lit;
    lit.tag_ = tag;
    switch (tag) {
        case panda_file::LiteralTag::BOOL: {
            lit.value_ = std::get<bool>(value);
            break;
        }
        case panda_file::LiteralTag::ACCESSOR:
        case panda_file::LiteralTag::NULLVALUE:
        case panda_file::LiteralTag::BUILTINTYPEINDEX: {
            lit.value_ = std::get<uint8_t>(value);
            break;
        }
        case panda_file::LiteralTag::METHODAFFILIATE: {
            lit.value_ = std::get<uint16_t>(value);
            break;
        }
        case panda_file::LiteralTag::LITERALBUFFERINDEX:
        case panda_file::LiteralTag::INTEGER: {
            lit.value_ = std::get<uint32_t>(value);
            break;
        }
        case panda_file::LiteralTag::DOUBLE: {
            lit.value_ = std::get<double>(value);
            break;
        }
        case panda_file::LiteralTag::STRING: {
            auto str_data = file_->GetStringData(panda_file::File::EntityId(std::get<uint32_t>(value)));
            lit.value_ = StringDataToString(str_data);
            break;
        }
        case panda_file::LiteralTag::METHOD:
        case panda_file::LiteralTag::GENERATORMETHOD: {
            panda_file::MethodDataAccessor mda(*file_, panda_file::File::EntityId(std::get<uint32_t>(value)));
            lit.value_ = StringDataToString(file_->GetStringData(mda.GetNameId()));
            break;
        }
        case panda_file::LiteralTag::LITERALARRAY: {
            std::stringstream ss;
            ss << "0x" << std::hex << std::get<uint32_t>(value);
            lit.value_ = ss.str();
            break;
        }
        case panda_file::LiteralTag::TAGVALUE: {
            return;
        }
        default: {
            UNREACHABLE();
        }
    }
    lit_array->literals_.push_back(lit);
}

void Disassembler::GetLiteralArrayByOffset(pandasm::LiteralArray *lit_array, panda_file::File::EntityId offset) const
{
    panda_file::LiteralDataAccessor lit_array_accessor(*file_, file_->GetLiteralArraysId());
    lit_array_accessor.EnumerateLiteralVals(
        offset, [this, lit_array](const panda_file::LiteralDataAccessor::LiteralValue &value,
                                  const panda_file::LiteralTag &tag) {
            switch (tag) {
                case panda_file::LiteralTag::ARRAY_U1: {
                    FillLiteralArrayData<bool>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I8:
                case panda_file::LiteralTag::ARRAY_U8: {
                    FillLiteralArrayData<uint8_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I16:
                case panda_file::LiteralTag::ARRAY_U16: {
                    FillLiteralArrayData<uint16_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I32:
                case panda_file::LiteralTag::ARRAY_U32: {
                    FillLiteralArrayData<uint32_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I64:
                case panda_file::LiteralTag::ARRAY_U64: {
                    FillLiteralArrayData<uint64_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_F32: {
                    FillLiteralArrayData<float>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_F64: {
                    FillLiteralArrayData<double>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_STRING: {
                    FillLiteralArrayData<uint32_t>(lit_array, tag, value);
                    break;
                }
                default: {
                    FillLiteralData(lit_array, value, tag);
                    break;
                }
            }
        });
}

void Disassembler::GetLiteralArray(pandasm::LiteralArray *lit_array, size_t index) const
{
    panda_file::LiteralDataAccessor lit_array_accessor(*file_, file_->GetLiteralArraysId());
    GetLiteralArrayByOffset(lit_array, lit_array_accessor.GetLiteralArrayId(index));
}

bool Disassembler::IsModuleLiteralOffset(const panda_file::File::EntityId &id) const
{
    return module_literals_.find(id.GetOffset()) != module_literals_.end();
}

void Disassembler::GetLiteralArrays()
{
    const auto lit_arrays_id = file_->GetLiteralArraysId();

    LOG(DEBUG, DISASSEMBLER) << "\n[getting literal arrays]\nid: " << lit_arrays_id << " (0x" << std::hex
                             << lit_arrays_id << ")";

    panda_file::LiteralDataAccessor lda(*file_, lit_arrays_id);
    size_t num_litarrays = lda.GetLiteralNum();
    for (size_t index = 0; index < num_litarrays; index++) {
        auto id = lda.GetLiteralArrayId(index);
        if (IsModuleLiteralOffset(id)) {
            std::stringstream ss;
            ss << index << " 0x" << std::hex << id.GetOffset();
            modulearray_table_.emplace(ss.str(), GetModuleLiteralArray(id));
            continue;
        }
        std::stringstream ss;
        ss << index << " 0x" << std::hex << id.GetOffset();
        panda::pandasm::LiteralArray lit_arr;
        GetLiteralArray(&lit_arr, index);
        prog_.literalarray_table.emplace(ss.str(), lit_arr);
    }
}

std::string Disassembler::ModuleTagToString(panda_file::ModuleTag &tag) const
{
    switch (tag) {
        case panda_file::ModuleTag::REGULAR_IMPORT:
            return "REGULAR_IMPORT";
        case panda_file::ModuleTag::NAMESPACE_IMPORT:
            return "NAMESPACE_IMPORT";
        case panda_file::ModuleTag::LOCAL_EXPORT:
            return "LOCAL_EXPORT";
        case panda_file::ModuleTag::INDIRECT_EXPORT:
            return "INDIRECT_EXPORT";
        case panda_file::ModuleTag::STAR_EXPORT:
            return "STAR_EXPORT";
        default: {
            UNREACHABLE();
            break;
        }
    }
    return "";
}

std::vector<std::string> Disassembler::GetModuleLiteralArray(panda_file::File::EntityId &module_id) const
{
    std::vector<std::string> request_modules;
    panda_file::ModuleDataAccessor mda(*file_, module_id);
    const std::vector<uint32_t> &request_modules_offset = mda.getRequestModules();
    for (size_t idx = 0; idx < request_modules_offset.size(); ++idx) {
        request_modules.emplace_back(GetStringByOffset(request_modules_offset[idx]));
    }

    std::vector<std::string> module_literal_array;
    mda.EnumerateModuleRecord([&](panda_file::ModuleTag tag, uint32_t export_name_offset,
                                  uint32_t request_module_idx, uint32_t import_name_offset,
                                  uint32_t local_name_offset) {
        std::stringstream ss;
        ss << "ModuleTag: " << ModuleTagToString(tag);
        if (IsValidOffset(local_name_offset)) {
            ss << ", local_name: " << GetStringByOffset(local_name_offset);
        }
        if (IsValidOffset(export_name_offset)) {
            ss << ", export_name: " << GetStringByOffset(export_name_offset);
        }
        if (IsValidOffset(import_name_offset)) {
            ss << ", import_name: " << GetStringByOffset(import_name_offset);
        }
        if (request_module_idx < request_modules.size()) {
            auto request_module = request_modules[request_module_idx];
            ASSERT(IsValidOffset(request_module));
            ss << ", module_request: " << request_module;
        }

        module_literal_array.push_back(ss.str());
    });

    return module_literal_array;
}

void Disassembler::GetRecords()
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting records]\n";

    const auto class_idx = file_->GetClasses();

    for (size_t i = 0; i < class_idx.size(); i++) {
        uint32_t class_id = class_idx[i];
        auto class_off = file_->GetHeader()->class_idx_off + sizeof(uint32_t) * i;

        if (class_id > file_->GetHeader()->file_size) {
            LOG(ERROR, DISASSEMBLER) << "> error encountered in record at " << class_off << " (0x" << std::hex
                                     << class_off << "). binary file corrupted. record offset (0x" << class_id
                                     << ") out of bounds (0x" << file_->GetHeader()->file_size << ")!";
            break;
        }

        const panda_file::File::EntityId record_id {class_id};
        auto language = GetRecordLanguage(record_id);

        if (language != file_language_) {
            if (file_language_ == panda_file::SourceLang::PANDA_ASSEMBLY) {
                file_language_ = language;
            } else if (language != panda_file::SourceLang::PANDA_ASSEMBLY) {
                LOG(ERROR, DISASSEMBLER) << "> possible error encountered in record at" << class_off << " (0x"
                                         << std::hex << class_off << "). record's language  ("
                                         << panda_file::LanguageToString(language)
                                         << ")  differs from file's language ("
                                         << panda_file::LanguageToString(file_language_) << ")!";
            }
        }

        pandasm::Record record("", file_language_);
        GetRecord(&record, record_id);

        if (prog_.record_table.find(record.name) == prog_.record_table.end()) {
            record_name_to_id_.emplace(record.name, record_id);
            prog_.record_table.emplace(record.name, std::move(record));
        }
    }
}

void Disassembler::GetFields(pandasm::Record *record, const panda_file::File::EntityId &record_id)
{
    panda_file::ClassDataAccessor class_accessor {*file_, record_id};

    class_accessor.EnumerateFields([&](panda_file::FieldDataAccessor &field_accessor) -> void {
        pandasm::Field field(file_language_);

        panda_file::File::EntityId field_name_id = field_accessor.GetNameId();
        field.name = StringDataToString(file_->GetStringData(field_name_id));

        uint32_t field_type = field_accessor.GetType();
        field.type = FieldTypeToPandasmType(field_type);

        GetMetaData(&field, field_accessor.GetFieldId());

        record->field_list.push_back(std::move(field));
    });
}

void Disassembler::GetMethods(const panda_file::File::EntityId &record_id)
{
    panda_file::ClassDataAccessor class_accessor {*file_, record_id};

    class_accessor.EnumerateMethods([&](panda_file::MethodDataAccessor &method_accessor) -> void {
        AddMethodToTables(method_accessor.GetMethodId());
    });
}

void Disassembler::GetParams(pandasm::Function *method, const panda_file::File::EntityId &proto_id) const
{
    /**
     * frame size - 2^16 - 1
     */
    static const uint32_t MAX_ARG_NUM = 0xFFFF;

    LOG(DEBUG, DISASSEMBLER) << "[getting params]\nproto id: " << proto_id << " (0x" << std::hex << proto_id << ")";

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but method ptr expected!";

        return;
    }

    panda_file::ProtoDataAccessor proto_accessor(*file_, proto_id);

    auto params_num = proto_accessor.GetNumArgs();

    if (params_num > MAX_ARG_NUM) {
        LOG(ERROR, DISASSEMBLER) << "> error encountered at " << proto_id << " (0x" << std::hex << proto_id
                                 << "). number of function's arguments (" << std::dec << params_num
                                 << ") exceeds MAX_ARG_NUM (" << MAX_ARG_NUM << ") !";

        return;
    }

    size_t ref_idx = 0;
    method->return_type = PFTypeToPandasmType(proto_accessor.GetReturnType(), proto_accessor, ref_idx);

    for (uint8_t i = 0; i < params_num; i++) {
        auto arg_type = PFTypeToPandasmType(proto_accessor.GetArgType(i), proto_accessor, ref_idx);
        method->params.push_back(pandasm::Function::Parameter(arg_type, file_language_));
    }
}

LabelTable Disassembler::GetExceptions(pandasm::Function *method, panda_file::File::EntityId method_id,
                                       panda_file::File::EntityId code_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting exceptions]\ncode id: " << code_id << " (0x" << std::hex << code_id << ")";

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but method ptr expected!\n";
        return LabelTable {};
    }

    panda_file::CodeDataAccessor code_accessor(*file_, code_id);

    const auto bc_ins = BytecodeInstruction(code_accessor.GetInstructions());
    const auto bc_ins_last = bc_ins.JumpTo(code_accessor.GetCodeSize());

    size_t try_idx = 0;
    LabelTable label_table {};
    code_accessor.EnumerateTryBlocks([&](panda_file::CodeDataAccessor::TryBlock &try_block) {
        pandasm::Function::CatchBlock catch_block_pa {};
        if (!LocateTryBlock(bc_ins, bc_ins_last, try_block, &catch_block_pa, &label_table, try_idx)) {
            return false;
        }
        size_t catch_idx = 0;
        try_block.EnumerateCatchBlocks([&](panda_file::CodeDataAccessor::CatchBlock &catch_block) {
            auto class_idx = catch_block.GetTypeIdx();

            if (class_idx == panda_file::INVALID_INDEX) {
                catch_block_pa.exception_record = "";
            } else {
                const auto class_id = file_->ResolveClassIndex(method_id, class_idx);
                catch_block_pa.exception_record = GetFullRecordName(class_id);
            }
            if (!LocateCatchBlock(bc_ins, bc_ins_last, catch_block, &catch_block_pa, &label_table, try_idx,
                                  catch_idx)) {
                return false;
            }

            method->catch_blocks.push_back(catch_block_pa);
            catch_block_pa.catch_begin_label = "";
            catch_block_pa.catch_end_label = "";
            catch_idx++;

            return true;
        });
        try_idx++;

        return true;
    });

    return label_table;
}

static size_t getBytecodeInstructionNumber(BytecodeInstruction bc_ins_first, BytecodeInstruction bc_ins_cur)
{
    size_t count = 0;

    while (bc_ins_first.GetAddress() != bc_ins_cur.GetAddress()) {
        count++;
        bc_ins_first = bc_ins_first.GetNext();
        if (bc_ins_first.GetAddress() > bc_ins_cur.GetAddress()) {
            return std::numeric_limits<size_t>::max();
        }
    }

    return count;
}

bool Disassembler::LocateTryBlock(const BytecodeInstruction &bc_ins, const BytecodeInstruction &bc_ins_last,
                                  const panda_file::CodeDataAccessor::TryBlock &try_block,
                                  pandasm::Function::CatchBlock *catch_block_pa, LabelTable *label_table,
                                  size_t try_idx) const
{
    const auto try_begin_bc_ins = bc_ins.JumpTo(try_block.GetStartPc());
    const auto try_end_bc_ins = bc_ins.JumpTo(try_block.GetStartPc() + try_block.GetLength());

    const size_t try_begin_idx = getBytecodeInstructionNumber(bc_ins, try_begin_bc_ins);
    const size_t try_end_idx = getBytecodeInstructionNumber(bc_ins, try_end_bc_ins);

    const bool try_begin_offset_in_range = bc_ins_last.GetAddress() > try_begin_bc_ins.GetAddress();
    const bool try_end_offset_in_range = bc_ins_last.GetAddress() >= try_end_bc_ins.GetAddress();
    const bool try_begin_offset_valid = try_begin_idx != std::numeric_limits<size_t>::max();
    const bool try_end_offset_valid = try_end_idx != std::numeric_limits<size_t>::max();

    if (!try_begin_offset_in_range || !try_begin_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid try block begin offset! address is: 0x" << std::hex
                                 << try_begin_bc_ins.GetAddress();
        return false;
    } else {
        std::stringstream ss {};
        ss << "try_begin_label_" << try_idx;

        LabelTable::iterator it = label_table->find(try_begin_idx);
        if (it == label_table->end()) {
            catch_block_pa->try_begin_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(try_begin_idx, ss.str()));
        } else {
            catch_block_pa->try_begin_label = it->second;
        }
    }

    if (!try_end_offset_in_range || !try_end_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid try block end offset! address is: 0x" << std::hex
                                 << try_end_bc_ins.GetAddress();
        return false;
    } else {
        std::stringstream ss {};
        ss << "try_end_label_" << try_idx;

        LabelTable::iterator it = label_table->find(try_end_idx);
        if (it == label_table->end()) {
            catch_block_pa->try_end_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(try_end_idx, ss.str()));
        } else {
            catch_block_pa->try_end_label = it->second;
        }
    }

    return true;
}

bool Disassembler::LocateCatchBlock(const BytecodeInstruction &bc_ins, const BytecodeInstruction &bc_ins_last,
                                    const panda_file::CodeDataAccessor::CatchBlock &catch_block,
                                    pandasm::Function::CatchBlock *catch_block_pa, LabelTable *label_table,
                                    size_t try_idx, size_t catch_idx) const
{
    const auto handler_begin_offset = catch_block.GetHandlerPc();
    const auto handler_end_offset = handler_begin_offset + catch_block.GetCodeSize();

    const auto handler_begin_bc_ins = bc_ins.JumpTo(handler_begin_offset);
    const auto handler_end_bc_ins = bc_ins.JumpTo(handler_end_offset);

    const size_t handler_begin_idx = getBytecodeInstructionNumber(bc_ins, handler_begin_bc_ins);
    const size_t handler_end_idx = getBytecodeInstructionNumber(bc_ins, handler_end_bc_ins);

    const bool handler_begin_offset_in_range = bc_ins_last.GetAddress() > handler_begin_bc_ins.GetAddress();
    const bool handler_end_offset_in_range = bc_ins_last.GetAddress() > handler_end_bc_ins.GetAddress();
    const bool handler_end_present = catch_block.GetCodeSize() != 0;
    const bool handler_begin_offset_valid = handler_begin_idx != std::numeric_limits<size_t>::max();
    const bool handler_end_offset_valid = handler_end_idx != std::numeric_limits<size_t>::max();

    if (!handler_begin_offset_in_range || !handler_begin_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid catch block begin offset! address is: 0x" << std::hex
                                 << handler_begin_bc_ins.GetAddress();
        return false;
    } else {
        std::stringstream ss {};
        ss << "handler_begin_label_" << try_idx << "_" << catch_idx;

        LabelTable::iterator it = label_table->find(handler_begin_idx);
        if (it == label_table->end()) {
            catch_block_pa->catch_begin_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(handler_begin_idx, ss.str()));
        } else {
            catch_block_pa->catch_begin_label = it->second;
        }
    }

    if (!handler_end_offset_in_range || !handler_end_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid catch block end offset! address is: 0x" << std::hex
                                 << handler_end_bc_ins.GetAddress();
        return false;
    } else if (handler_end_present) {
        std::stringstream ss {};
        ss << "handler_end_label_" << try_idx << "_" << catch_idx;

        LabelTable::iterator it = label_table->find(handler_end_idx);
        if (it == label_table->end()) {
            catch_block_pa->catch_end_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(handler_end_idx, ss.str()));
        } else {
            catch_block_pa->catch_end_label = it->second;
        }
    }

    return true;
}

void Disassembler::GetMetaData(pandasm::Function *method, const panda_file::File::EntityId &method_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting metadata]\nmethod id: " << method_id << " (0x" << std::hex << method_id
                             << ")";

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but method ptr expected!";

        return;
    }

    panda_file::MethodDataAccessor method_accessor(*file_, method_id);

    const auto method_name_raw = StringDataToString(file_->GetStringData(method_accessor.GetNameId()));

    if (!method_accessor.IsStatic()) {
        const auto class_name = StringDataToString(file_->GetStringData(method_accessor.GetClassId()));
        auto this_type = pandasm::Type::FromDescriptor(class_name);

        LOG(DEBUG, DISASSEMBLER) << "method (raw: \'" << method_name_raw
                                 << "\') is not static. emplacing self-argument of type " << this_type.GetName();

        method->params.insert(method->params.begin(), pandasm::Function::Parameter(this_type, file_language_));
    } else {
        method->metadata->SetAttribute("static");
    }

    if (file_->IsExternal(method_accessor.GetMethodId())) {
        method->metadata->SetAttribute("external");
    }

    std::string ctor_name = panda::panda_file::GetCtorName(file_language_);
    std::string cctor_name = panda::panda_file::GetCctorName(file_language_);

    const bool is_ctor = (method_name_raw == ctor_name);
    const bool is_cctor = (method_name_raw == cctor_name);

    if (is_ctor) {
        method->metadata->SetAttribute("ctor");
        method->name.replace(method->name.find(ctor_name), ctor_name.length(), "_ctor_");
    } else if (is_cctor) {
        method->metadata->SetAttribute("cctor");
        method->name.replace(method->name.find(cctor_name), cctor_name.length(), "_cctor_");
    }
}

void Disassembler::GetMetaData(pandasm::Record *record, const panda_file::File::EntityId &record_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting metadata]\nrecord id: " << record_id << " (0x" << std::hex << record_id
                             << ")";

    if (record == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but record ptr expected!";

        return;
    }

    if (file_->IsExternal(record_id)) {
        record->metadata->SetAttribute("external");
    }
}

void Disassembler::GetMetaData(pandasm::Field *field, const panda_file::File::EntityId &field_id)
{
    LOG(DEBUG, DISASSEMBLER) << "[getting metadata]\nfield id: " << field_id << " (0x" << std::hex << field_id << ")";

    if (field == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr recieved, but method ptr expected!";

        return;
    }

    panda_file::FieldDataAccessor field_accessor(*file_, field_id);

    if (field_accessor.IsExternal()) {
        field->metadata->SetAttribute("external");
    }

    if (field_accessor.IsStatic()) {
        field->metadata->SetAttribute("static");
    }

    if (field->type.GetId() == panda_file::Type::TypeId::U32) {
        const auto offset = field_accessor.GetValue<uint32_t>().value();
        static const std::string TYPE_SUMMARY_FIELD_NAME = "typeSummaryOffset";
        if (field->name != TYPE_SUMMARY_FIELD_NAME) {
            LOG(DEBUG, DISASSEMBLER) << "Module literalarray " << field->name << " at offset 0x" << std::hex << offset
                                     << " is excluded";
            module_literals_.insert(offset);
        }
        field->metadata->SetValue(pandasm::ScalarValue::Create<pandasm::Value::Type::U32>(offset));
    }
    if (field->type.GetId() == panda_file::Type::TypeId::U8) {
        const auto val = field_accessor.GetValue<uint8_t>().value();
        field->metadata->SetValue(pandasm::ScalarValue::Create<pandasm::Value::Type::U8>(val));
    }
}

std::string Disassembler::AnnotationTagToString(const char tag) const
{
    switch (tag) {
        case '1':
            return "u1";
        case '2':
            return "i8";
        case '3':
            return "u8";
        case '4':
            return "i16";
        case '5':
            return "u16";
        case '6':
            return "i32";
        case '7':
            return "u32";
        case '8':
            return "i64";
        case '9':
            return "u64";
        case 'A':
            return "f32";
        case 'B':
            return "f64";
        case 'C':
            return "string";
        case 'D':
            return "record";
        case 'E':
            return "method";
        case 'F':
            return "enum";
        case 'G':
            return "annotation";
        case 'I':
            return "void";
        case 'J':
            return "method_handle";
        case 'K':
            return "u1[]";
        case 'L':
            return "i8[]";
        case 'M':
            return "u8[]";
        case 'N':
            return "i16[]";
        case 'O':
            return "u16[]";
        case 'P':
            return "i32[]";
        case 'Q':
            return "u32[]";
        case 'R':
            return "i64[]";
        case 'S':
            return "u64[]";
        case 'T':
            return "f32[]";
        case 'U':
            return "f64[]";
        case 'V':
            return "string[]";
        case 'W':
            return "record[]";
        case 'X':
            return "method[]";
        case 'Y':
            return "enum[]";
        case 'Z':
            return "annotation[]";
        case '@':
            return "method_handle[]";
        case '*':
            return "nullptr string";
        default:
            return std::string();
    }
}

std::string Disassembler::ScalarValueToString(const panda_file::ScalarValue &value, const std::string &type)
{
    std::stringstream ss;

    if (type == "i8") {
        int8_t res = value.Get<int8_t>();
        ss << static_cast<int>(res);
    } else if (type == "u1" || type == "u8") {
        uint8_t res = value.Get<uint8_t>();
        ss << static_cast<unsigned int>(res);
    } else if (type == "i16") {
        ss << value.Get<int16_t>();
    } else if (type == "u16") {
        ss << value.Get<uint16_t>();
    } else if (type == "i32") {
        ss << value.Get<int32_t>();
    } else if (type == "u32") {
        ss << value.Get<uint32_t>();
    } else if (type == "i64") {
        ss << value.Get<int64_t>();
    } else if (type == "u64") {
        ss << value.Get<uint64_t>();
    } else if (type == "f32") {
        ss << value.Get<float>();
    } else if (type == "f64") {
        ss << value.Get<double>();
    } else if (type == "string") {
        const auto id = value.Get<panda_file::File::EntityId>();
        ss << "\"" << StringDataToString(file_->GetStringData(id)) << "\"";
    } else if (type == "record") {
        const auto id = value.Get<panda_file::File::EntityId>();
        ss << GetFullRecordName(id);
    } else if (type == "method") {
        const auto id = value.Get<panda_file::File::EntityId>();
        AddMethodToTables(id);
        ss << GetMethodSignature(id);
    } else if (type == "enum") {
        const auto id = value.Get<panda_file::File::EntityId>();
        panda_file::FieldDataAccessor field_accessor(*file_, id);
        ss << GetFullRecordName(field_accessor.GetClassId()) << "."
           << StringDataToString(file_->GetStringData(field_accessor.GetNameId()));
    } else if (type == "annotation") {
        const auto id = value.Get<panda_file::File::EntityId>();
        ss << "id_" << id;
    } else if (type == "void") {
        return std::string();
    } else if (type == "method_handle") {
    }

    return ss.str();
}

std::string Disassembler::ArrayValueToString(const panda_file::ArrayValue &value, const std::string &type,
                                             const size_t idx)
{
    std::stringstream ss;

    if (type == "i8") {
        int8_t res = value.Get<int8_t>(idx);
        ss << static_cast<int>(res);
    } else if (type == "u1" || type == "u8") {
        uint8_t res = value.Get<uint8_t>(idx);
        ss << static_cast<unsigned int>(res);
    } else if (type == "i16") {
        ss << value.Get<int16_t>(idx);
    } else if (type == "u16") {
        ss << value.Get<uint16_t>(idx);
    } else if (type == "i32") {
        ss << value.Get<int32_t>(idx);
    } else if (type == "u32") {
        ss << value.Get<uint32_t>(idx);
    } else if (type == "i64") {
        ss << value.Get<int64_t>(idx);
    } else if (type == "u64") {
        ss << value.Get<uint64_t>(idx);
    } else if (type == "f32") {
        ss << value.Get<float>(idx);
    } else if (type == "f64") {
        ss << value.Get<double>(idx);
    } else if (type == "string") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        ss << '\"' << StringDataToString(file_->GetStringData(id)) << '\"';
    } else if (type == "record") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        ss << GetFullRecordName(id);
    } else if (type == "method") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        AddMethodToTables(id);
        ss << GetMethodSignature(id);
    } else if (type == "enum") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        panda_file::FieldDataAccessor field_accessor(*file_, id);
        ss << GetFullRecordName(field_accessor.GetClassId()) << "."
           << StringDataToString(file_->GetStringData(field_accessor.GetNameId()));
    } else if (type == "annotation") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        ss << "id_" << id;
    } else if (type == "method_handle") {
    } else if (type == "nullptr string") {
    }

    return ss.str();
}

std::string Disassembler::GetFullMethodName(const panda_file::File::EntityId &method_id) const
{
    panda::panda_file::MethodDataAccessor method_accessor(*file_, method_id);

    const auto method_name_raw = StringDataToString(file_->GetStringData(method_accessor.GetNameId()));

    std::string class_name = GetFullRecordName(method_accessor.GetClassId());
    if (IsSystemType(class_name)) {
        class_name = "";
    } else {
        class_name += ".";
    }

    return class_name + method_name_raw;
}

std::string Disassembler::GetMethodSignature(const panda_file::File::EntityId &method_id) const
{
    panda::panda_file::MethodDataAccessor method_accessor(*file_, method_id);

    pandasm::Function method(GetFullMethodName(method_id), file_language_);
    GetParams(&method, method_accessor.GetProtoId());
    GetMetaData(&method, method_id);

    return pandasm::GetFunctionSignatureFromName(method.name, method.params);
}

std::string Disassembler::GetFullRecordName(const panda_file::File::EntityId &class_id) const
{
    std::string name = StringDataToString(file_->GetStringData(class_id));

    auto type = pandasm::Type::FromDescriptor(name);
    type = pandasm::Type(type.GetComponentName(), type.GetRank());

    return type.GetPandasmName();
}

void Disassembler::GetRecordInfo(const panda_file::File::EntityId &record_id, RecordInfo *record_info) const
{
    constexpr size_t DEFAULT_OFFSET_WIDTH = 4;

    if (file_->IsExternal(record_id)) {
        return;
    }

    panda_file::ClassDataAccessor class_accessor {*file_, record_id};
    std::stringstream ss;

    ss << "offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
       << class_accessor.GetClassId() << ", size: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH)
       << class_accessor.GetSize() << " (" << std::dec << class_accessor.GetSize() << ")";

    record_info->record_info = ss.str();
    ss.str(std::string());

    class_accessor.EnumerateFields([&](panda_file::FieldDataAccessor &field_accessor) -> void {
        ss << "offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
           << field_accessor.GetFieldId();

        record_info->fields_info.push_back(ss.str());

        ss.str(std::string());
    });
}

void Disassembler::GetMethodInfo(const panda_file::File::EntityId &method_id, MethodInfo *method_info) const
{
    constexpr size_t DEFAULT_OFFSET_WIDTH = 4;

    panda_file::MethodDataAccessor method_accessor {*file_, method_id};
    std::stringstream ss;

    ss << "offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
       << method_accessor.GetMethodId();

    if (method_accessor.GetCodeId().has_value()) {
        ss << ", code offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
           << method_accessor.GetCodeId().value();

        GetInsInfo(method_accessor.GetCodeId().value(), method_info);
    } else {
        ss << ", <no code>";
    }

    method_info->method_info = ss.str();

    if (method_accessor.GetCodeId()) {
        ASSERT(debug_info_extractor_ != nullptr);
        method_info->line_number_table = debug_info_extractor_->GetLineNumberTable(method_id);
        method_info->local_variable_table = debug_info_extractor_->GetLocalVariableTable(method_id);

        // Add information about parameters into the table
        panda_file::CodeDataAccessor codeda(*file_, method_accessor.GetCodeId().value());
        auto arg_idx = static_cast<int32_t>(codeda.GetNumVregs());
        uint32_t code_size = codeda.GetCodeSize();
        for (auto info : debug_info_extractor_->GetParameterInfo(method_id)) {
            panda_file::LocalVariableInfo arg_info {info.name, info.signature, "", arg_idx++, 0, code_size};
            method_info->local_variable_table.emplace_back(arg_info);
        }
    }
}

static bool IsArray(const panda_file::LiteralTag &tag)
{
    switch (tag) {
        case panda_file::LiteralTag::ARRAY_U1:
        case panda_file::LiteralTag::ARRAY_U8:
        case panda_file::LiteralTag::ARRAY_I8:
        case panda_file::LiteralTag::ARRAY_U16:
        case panda_file::LiteralTag::ARRAY_I16:
        case panda_file::LiteralTag::ARRAY_U32:
        case panda_file::LiteralTag::ARRAY_I32:
        case panda_file::LiteralTag::ARRAY_U64:
        case panda_file::LiteralTag::ARRAY_I64:
        case panda_file::LiteralTag::ARRAY_F32:
        case panda_file::LiteralTag::ARRAY_F64:
        case panda_file::LiteralTag::ARRAY_STRING:
            return true;
        default:
            return false;
    }
}

std::string Disassembler::SerializeLiteralArray(const pandasm::LiteralArray &lit_array) const
{
    std::stringstream ret;
    if (lit_array.literals_.empty()) {
        return "";
    }

    std::stringstream ss;
    ss << "{ ";
    const auto &tag = lit_array.literals_[0].tag_;
    if (IsArray(tag)) {
        ss << LiteralTagToString(tag);
    }
    ss << lit_array.literals_.size();
    ss << " [ ";
    SerializeValues(lit_array, ss);
    ss << "]}";
    return ss.str();
}

void Disassembler::Serialize(const std::string &key, const pandasm::LiteralArray &lit_array, std::ostream &os) const
{
    os << key << " ";
    os << SerializeLiteralArray(lit_array);
    os << "\n";
}

void Disassembler::Serialize(const std::string &module_offset, const std::vector<std::string> &module_array,
                             std::ostream &os) const
{
    os << module_offset << " ";
    os << SerializeModuleLiteralArray(module_array);
    os << "\n";
}

std::string Disassembler::SerializeModuleLiteralArray(const std::vector<std::string> &module_array) const
{
    if (module_array.empty()) {
        return "";
    }

    std::stringstream ss;
    ss << "{ ";
    ss << module_array.size();
    ss << " [ ";
    for (size_t index = 0; index < module_array.size(); index++) {
        ss << module_array[index] << "; ";
    }
    ss << "]}";
    return ss.str();
}

std::string Disassembler::LiteralTagToString(const panda_file::LiteralTag &tag) const
{
    switch (tag) {
        case panda_file::LiteralTag::BOOL:
        case panda_file::LiteralTag::ARRAY_U1:
            return "u1";
        case panda_file::LiteralTag::ARRAY_U8:
            return "u8";
        case panda_file::LiteralTag::ARRAY_I8:
            return "i8";
        case panda_file::LiteralTag::ARRAY_U16:
            return "u16";
        case panda_file::LiteralTag::ARRAY_I16:
            return "i16";
        case panda_file::LiteralTag::ARRAY_U32:
            return "u32";
        case panda_file::LiteralTag::INTEGER:
        case panda_file::LiteralTag::ARRAY_I32:
            return "i32";
        case panda_file::LiteralTag::ARRAY_U64:
            return "u64";
        case panda_file::LiteralTag::ARRAY_I64:
            return "i64";
        case panda_file::LiteralTag::ARRAY_F32:
            return "f32";
        case panda_file::LiteralTag::DOUBLE:
        case panda_file::LiteralTag::ARRAY_F64:
            return "f64";
        case panda_file::LiteralTag::STRING:
        case panda_file::LiteralTag::ARRAY_STRING:
            return "string";
        case panda_file::LiteralTag::METHOD:
            return "method";
        case panda_file::LiteralTag::GENERATORMETHOD:
            return "generator_method";
        case panda_file::LiteralTag::ACCESSOR:
            return "accessor";
        case panda_file::LiteralTag::METHODAFFILIATE:
            return "method_affiliate";
        case panda_file::LiteralTag::NULLVALUE:
            return "null_value";
        case panda_file::LiteralTag::TAGVALUE:
            return "tagvalue";
        case panda_file::LiteralTag::LITERALBUFFERINDEX:
            return "lit_index";
        case panda_file::LiteralTag::LITERALARRAY:
            return "lit_offset";
        case panda_file::LiteralTag::BUILTINTYPEINDEX:
            return "builtin_type";
        default:
            UNREACHABLE();
    }
}

template <typename T>
void Disassembler::SerializeValues(const pandasm::LiteralArray &lit_array, T &os) const
{
    switch (lit_array.literals_[0].tag_) {
        case panda_file::LiteralTag::ARRAY_U1: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << std::get<bool>(lit_array.literals_[i].value_) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_U8: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << static_cast<uint16_t>(std::get<uint8_t>(lit_array.literals_[i].value_)) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I8: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << static_cast<int16_t>(bit_cast<int8_t>(std::get<uint8_t>(lit_array.literals_[i].value_))) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_U16: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << std::get<uint16_t>(lit_array.literals_[i].value_) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I16: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << bit_cast<int16_t>(std::get<uint16_t>(lit_array.literals_[i].value_)) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_U32: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << std::get<uint32_t>(lit_array.literals_[i].value_) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I32: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << bit_cast<int32_t>(std::get<uint32_t>(lit_array.literals_[i].value_)) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_U64: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << std::get<uint64_t>(lit_array.literals_[i].value_) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I64: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << bit_cast<int64_t>(std::get<uint64_t>(lit_array.literals_[i].value_)) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_F32: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << std::get<float>(lit_array.literals_[i].value_) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_F64: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << std::get<double>(lit_array.literals_[i].value_) << " ";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_STRING: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\"" << std::get<std::string>(lit_array.literals_[i].value_) << "\" ";
            }
            break;
        }
        default:
            SerializeLiterals(lit_array, os);
    }
}

template <typename T>
void Disassembler::SerializeLiterals(const pandasm::LiteralArray &lit_array, T &os) const
{
    for (size_t i = 0; i < lit_array.literals_.size(); i++) {
        const auto &tag = lit_array.literals_[i].tag_;
        os << LiteralTagToString(tag) << ":";
        const auto &val = lit_array.literals_[i].value_;
        switch (lit_array.literals_[i].tag_) {
            case panda_file::LiteralTag::BOOL: {
                os << std::get<bool>(val);
                break;
            }
            case panda_file::LiteralTag::LITERALBUFFERINDEX:
            case panda_file::LiteralTag::INTEGER: {
                os << bit_cast<int32_t>(std::get<uint32_t>(val));
                break;
            }
            case panda_file::LiteralTag::DOUBLE: {
                os << std::get<double>(val);
                break;
            }
            case panda_file::LiteralTag::STRING: {
                os << "\"" << std::get<std::string>(val) << "\"";
                break;
            }
            case panda_file::LiteralTag::METHOD:
            case panda_file::LiteralTag::GENERATORMETHOD: {
                os << std::get<std::string>(val);
                break;
            }
            case panda_file::LiteralTag::NULLVALUE:
            case panda_file::LiteralTag::ACCESSOR: {
                os << static_cast<int16_t>(bit_cast<int8_t>(std::get<uint8_t>(val)));
                break;
            }
            case panda_file::LiteralTag::METHODAFFILIATE: {
                os << std::get<uint16_t>(val);
                break;
            }
            case panda_file::LiteralTag::LITERALARRAY: {
                os << std::get<std::string>(val);
                break;
            }
            case panda_file::LiteralTag::BUILTINTYPEINDEX: {
                os << static_cast<int16_t>(std::get<uint8_t>(val));
                break;
            }
            default:
                UNREACHABLE();
        }
        os << ", ";
    }
}

void Disassembler::Serialize(const pandasm::Record &record, std::ostream &os, bool print_information) const
{
    if (IsSystemType(record.name)) {
        return;
    }

    os << ".record " << record.name;

    const auto record_iter = prog_ann_.record_annotations.find(record.name);
    const bool record_in_table = record_iter != prog_ann_.record_annotations.end();

    if (record_in_table) {
        Serialize(*record.metadata, record_iter->second.ann_list, os);
    } else {
        Serialize(*record.metadata, {}, os);
    }

    if (record.metadata->IsForeign()) {
        os << "\n\n";
        return;
    }

    os << " {";

    if (print_information && prog_info_.records_info.find(record.name) != prog_info_.records_info.end()) {
        os << " # " << prog_info_.records_info.at(record.name).record_info << "\n";
        SerializeFields(record, os, true);
    } else {
        os << "\n";
        SerializeFields(record, os, false);
    }

    os << "}\n\n";
}

void Disassembler::SerializeFields(const pandasm::Record &record, std::ostream &os, bool print_information) const
{
    constexpr size_t INFO_OFFSET = 80;

    const auto record_iter = prog_ann_.record_annotations.find(record.name);
    const bool record_in_table = record_iter != prog_ann_.record_annotations.end();

    const auto rec_inf = (print_information) ? (prog_info_.records_info.at(record.name)) : (RecordInfo {});

    size_t field_idx = 0;

    std::stringstream ss;
    for (const auto &f : record.field_list) {
        std::string file = GetFileNameByAbsolutePath(f.name);
        ss << "\t" << f.type.GetPandasmName() << " " << file;
        if (f.metadata->GetValue().has_value()) {
            if (f.type.GetId() == panda_file::Type::TypeId::U32) {
                ss << " = 0x" << std::hex << f.metadata->GetValue().value().GetValue<uint32_t>();
            }
            if (f.type.GetId() == panda_file::Type::TypeId::U8) {
                ss << " = 0x" << std::hex << static_cast<uint32_t>(f.metadata->GetValue().value().GetValue<uint8_t>());
            }
        }
        if (record_in_table) {
            const auto field_iter = record_iter->second.field_annotations.find(f.name);
            if (field_iter != record_iter->second.field_annotations.end()) {
                Serialize(*f.metadata, field_iter->second, ss);
            } else {
                Serialize(*f.metadata, {}, ss);
            }
        } else {
            Serialize(*f.metadata, {}, ss);
        }

        if (print_information) {
            os << std::setw(INFO_OFFSET) << std::left << ss.str() << " # " << rec_inf.fields_info.at(field_idx) << "\n";
        } else {
            os << ss.str() << "\n";
        }

        ss.str(std::string());
        ss.clear();

        field_idx++;
    }
}

void Disassembler::Serialize(const pandasm::Function &method, std::ostream &os, bool print_information) const
{
    os << ".function " << method.return_type.GetPandasmName() << " " << method.name << "(";

    if (method.params.size() > 0) {
        os << method.params[0].type.GetPandasmName() << " a0";

        for (uint8_t i = 1; i < method.params.size(); i++) {
            os << ", " << method.params[i].type.GetPandasmName() << " a" << (size_t)i;
        }
    }
    os << ")";

    const std::string signature = pandasm::GetFunctionSignatureFromName(method.name, method.params);

    const auto method_iter = prog_ann_.method_annotations.find(signature);
    if (method_iter != prog_ann_.method_annotations.end()) {
        Serialize(*method.metadata, method_iter->second, os);
    } else {
        Serialize(*method.metadata, {}, os);
    }

    auto method_info_it = prog_info_.methods_info.find(signature);
    bool print_method_info = print_information && method_info_it != prog_info_.methods_info.end();
    if (print_method_info) {
        const MethodInfo &method_info = method_info_it->second;

        size_t width = 0;
        for (const auto &i : method.ins) {
            if (i.ToString().size() > width) {
                width = i.ToString().size();
            }
        }

        os << " { # " << method_info.method_info << "\n#   CODE:\n";

        for (size_t i = 0; i < method.ins.size(); i++) {
            os << "\t" << std::setw(width) << std::left << method.ins.at(i).ToString("", true, method.regs_num) << " # "
               << method_info.instructions_info.at(i) << "\n";
        }
    } else {
        os << " {\n";

        for (const auto &i : method.ins) {
            if (i.set_label) {
                std::string ins = i.ToString("", true, method.regs_num);
                std::string delim = ": ";
                size_t pos = ins.find(delim);
                std::string label = ins.substr(0, pos);
                ins.erase(0, pos + delim.length());
                os << label << ":\n\t" << ins << "\n";
            } else {
                os << "\t" << i.ToString("", true, method.regs_num) << "\n";
            }
        }
    }

    if (method.catch_blocks.size() != 0) {
        os << "\n";

        for (const auto &catch_block : method.catch_blocks) {
            Serialize(catch_block, os);

            os << "\n";
        }
    }

    if (print_method_info) {
        const MethodInfo &method_info = method_info_it->second;
        SerializeLineNumberTable(method_info.line_number_table, os);
        SerializeLocalVariableTable(method_info.local_variable_table, method, os);
    }

    os << "}\n\n";
}

void Disassembler::Serialize(const pandasm::Function::CatchBlock &catch_block, std::ostream &os) const
{
    if (catch_block.exception_record == "") {
        os << ".catchall ";
    } else {
        os << ".catch " << catch_block.exception_record << ", ";
    }

    os << catch_block.try_begin_label << ", " << catch_block.try_end_label << ", " << catch_block.catch_begin_label;

    if (catch_block.catch_end_label != "") {
        os << ", " << catch_block.catch_end_label;
    }
}

void Disassembler::Serialize(const pandasm::ItemMetadata &meta, const AnnotationList &ann_list, std::ostream &os) const
{
    auto bool_attributes = meta.GetBoolAttributes();
    auto attributes = meta.GetAttributes();
    if (bool_attributes.empty() && attributes.empty() && ann_list.empty()) {
        return;
    }

    os << " <";

    size_t size = bool_attributes.size();
    size_t idx = 0;
    for (const auto &attr : bool_attributes) {
        os << attr;
        ++idx;

        if (!attributes.empty() || !ann_list.empty() || idx < size) {
            os << ", ";
        }
    }

    size = attributes.size();
    idx = 0;
    for (const auto &[key, values] : attributes) {
        for (size_t i = 0; i < values.size(); i++) {
            os << key << "=" << values[i];

            if (i < values.size() - 1) {
                os << ", ";
            }
        }

        ++idx;

        if (!ann_list.empty() || idx < size) {
            os << ", ";
        }
    }

    size = ann_list.size();
    idx = 0;
    for (const auto &[key, value] : ann_list) {
        os << key << "=" << value;

        ++idx;

        if (idx < size) {
            os << ", ";
        }
    }

    os << ">";
}

void Disassembler::SerializeLineNumberTable(const panda_file::LineNumberTable &line_number_table,
                                            std::ostream &os) const
{
    if (line_number_table.empty()) {
        return;
    }

    os << "\n#   LINE_NUMBER_TABLE:\n";
    for (const auto &line_info : line_number_table) {
        os << "#\tline " << line_info.line << ": " << line_info.offset << "\n";
    }
}

void Disassembler::SerializeLocalVariableTable(const panda_file::LocalVariableTable &local_variable_table,
                                               const pandasm::Function &method, std::ostream &os) const
{
    if (local_variable_table.empty()) {
        return;
    }

    os << "\n#   LOCAL_VARIABLE_TABLE:\n";
    os << "#\t Start   End  Register           Name   Signature\n";
    const int START_WIDTH = 5;
    const int END_WIDTH = 4;
    const int REG_WIDTH = 8;
    const int NAME_WIDTH = 14;
    for (const auto &variable_info : local_variable_table) {
        std::ostringstream reg_stream;
        reg_stream << variable_info.reg_number << '(';
        if (variable_info.reg_number < 0) {
            reg_stream << "acc";
        } else {
            uint32_t vreg = variable_info.reg_number;
            uint32_t first_arg_reg = method.GetTotalRegs();
            if (vreg < first_arg_reg) {
                reg_stream << 'v' << vreg;
            } else {
                reg_stream << 'a' << vreg - first_arg_reg;
            }
        }
        reg_stream << ')';

        os << "#\t " << std::setw(START_WIDTH) << std::right << variable_info.start_offset << "  ";
        os << std::setw(END_WIDTH) << std::right << variable_info.end_offset << "  ";
        os << std::setw(REG_WIDTH) << std::right << reg_stream.str() << " ";
        os << std::setw(NAME_WIDTH) << std::right << variable_info.name << "   " << variable_info.type;
        if (!variable_info.type_signature.empty() && variable_info.type_signature != variable_info.type) {
            os << " (" << variable_info.type_signature << ")";
        }
        os << "\n";
    }
}

pandasm::Opcode Disassembler::BytecodeOpcodeToPandasmOpcode(uint8_t o) const
{
    return BytecodeOpcodeToPandasmOpcode(BytecodeInstruction::Opcode(o));
}

std::string Disassembler::IDToString(BytecodeInstruction bc_ins, panda_file::File::EntityId method_id,
                                     size_t idx) const
{
    std::stringstream name;
    const auto offset = file_->ResolveOffsetByIndex(method_id, bc_ins.GetId(idx).AsIndex());

    if (bc_ins.IsIdMatchFlag(idx, BytecodeInstruction::Flags::METHOD_ID)) {
        name << GetMethodSignature(offset);
    } else if (bc_ins.IsIdMatchFlag(idx, BytecodeInstruction::Flags::STRING_ID)) {
        name << '\"';
        name << StringDataToString(file_->GetStringData(offset));
        name << '\"';
    } else {
        ASSERT(bc_ins.IsIdMatchFlag(idx, BytecodeInstruction::Flags::LITERALARRAY_ID));
        pandasm::LiteralArray lit_array;
        GetLiteralArrayByOffset(&lit_array, panda_file::File::EntityId(offset));
        name << SerializeLiteralArray(lit_array);
    }

    return name.str();
}

panda::panda_file::SourceLang Disassembler::GetRecordLanguage(panda_file::File::EntityId class_id) const
{
    if (file_->IsExternal(class_id)) {
        return panda::panda_file::SourceLang::PANDA_ASSEMBLY;
    }

    panda_file::ClassDataAccessor cda(*file_, class_id);
    return cda.GetSourceLang().value_or(panda_file::SourceLang::PANDA_ASSEMBLY);
}

static void translateImmToLabel(pandasm::Ins *pa_ins, LabelTable *label_table, const uint8_t *ins_arr,
                                BytecodeInstruction bc_ins, BytecodeInstruction bc_ins_last,
                                panda_file::File::EntityId code_id)
{
    const int32_t jmp_offset = std::get<int64_t>(pa_ins->imms.at(0));
    const auto bc_ins_dest = bc_ins.JumpTo(jmp_offset);
    if (bc_ins_last.GetAddress() > bc_ins_dest.GetAddress()) {
        size_t idx = getBytecodeInstructionNumber(BytecodeInstruction(ins_arr), bc_ins_dest);

        if (idx != std::numeric_limits<size_t>::max()) {
            if (label_table->find(idx) == label_table->end()) {
                std::stringstream ss {};
                ss << "jump_label_" << label_table->size();
                (*label_table)[idx] = ss.str();
            }

            pa_ins->imms.clear();
            pa_ins->ids.push_back(label_table->at(idx));
        } else {
            LOG(ERROR, DISASSEMBLER) << "> error encountered at " << code_id << " (0x" << std::hex << code_id
                                     << "). incorrect instruction at offset: 0x" << (bc_ins.GetAddress() - ins_arr)
                                     << ": invalid jump offset 0x" << jmp_offset
                                     << " - jumping in the middle of another instruction!";
        }
    } else {
        LOG(ERROR, DISASSEMBLER) << "> error encountered at " << code_id << " (0x" << std::hex << code_id
                                 << "). incorrect instruction at offset: 0x" << (bc_ins.GetAddress() - ins_arr)
                                 << ": invalid jump offset 0x" << jmp_offset << " - jumping out of bounds!";
    }
}

IdList Disassembler::GetInstructions(pandasm::Function *method, panda_file::File::EntityId method_id,
                                     panda_file::File::EntityId code_id) const
{
    panda_file::CodeDataAccessor code_accessor(*file_, code_id);

    const auto ins_sz = code_accessor.GetCodeSize();
    const auto ins_arr = code_accessor.GetInstructions();

    method->regs_num = code_accessor.GetNumVregs();

    auto bc_ins = BytecodeInstruction(ins_arr);
    const auto bc_ins_last = bc_ins.JumpTo(ins_sz);

    LabelTable label_table = GetExceptions(method, method_id, code_id);

    IdList unknown_external_methods {};

    while (bc_ins.GetAddress() != bc_ins_last.GetAddress()) {
        if (bc_ins.GetAddress() > bc_ins_last.GetAddress()) {
            LOG(ERROR, DISASSEMBLER) << "> error encountered at " << code_id << " (0x" << std::hex << code_id
                                     << "). bytecode instructions sequence corrupted for method " << method->name
                                     << "! went out of bounds";

            break;
        }

        auto pa_ins = BytecodeInstructionToPandasmInstruction(bc_ins, method_id);
        if (pa_ins.IsJump()) {
            translateImmToLabel(&pa_ins, &label_table, ins_arr, bc_ins, bc_ins_last, code_id);
        }

        // check if method id is unknown external method. if so, emplace it in table
        if (bc_ins.HasFlag(BytecodeInstruction::Flags::METHOD_ID)) {
            const auto arg_method_idx = bc_ins.GetId().AsIndex();
            const auto arg_method_id = file_->ResolveMethodIndex(method_id, arg_method_idx);

            const auto arg_method_signature = GetMethodSignature(arg_method_id);

            const bool is_present = prog_.function_table.find(arg_method_signature) != prog_.function_table.cend();
            const bool is_external = file_->IsExternal(arg_method_id);

            if (is_external && !is_present) {
                unknown_external_methods.push_back(arg_method_id);
            }
        }

        method->ins.push_back(pa_ins);
        bc_ins = bc_ins.GetNext();
    }

    for (const auto &pair : label_table) {
        method->ins[pair.first].label = pair.second;
        method->ins[pair.first].set_label = true;
    }

    return unknown_external_methods;
}

}  // namespace panda::disasm
