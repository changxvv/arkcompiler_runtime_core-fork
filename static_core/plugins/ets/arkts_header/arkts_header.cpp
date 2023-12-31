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

#include <iostream>

#include "libpandafile/file.h"
#include "libpandafile/file_reader.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/pandargs.h"
#include "libpandafile/file_writer.h"
#include "libpandabase/utils/utf.h"
#include "runtime/include/mem/panda_string.h"
#include "header_writer.h"

void PrintHelp(panda::PandArgParser &pa_parser)
{
    std::cerr << "Usage:" << std::endl;
    std::cerr << "arkts_header [options] INPUT_FILE OUTPUT_FILE" << std::endl << std::endl;
    std::cerr << "Supported options:" << std::endl << std::endl;
    std::cerr << pa_parser.GetHelpString() << std::endl;
}

bool ProcessArgs(panda::PandArgParser &pa_parser, const panda::PandArg<std::string> &input,
                 panda::PandArg<std::string> &output, const panda::PandArg<bool> &help, int argc, const char **argv)
{
    if (!pa_parser.Parse(argc, argv)) {
        PrintHelp(pa_parser);
        return false;
    }

    if (input.GetValue().empty() || help.GetValue()) {
        PrintHelp(pa_parser);
        return false;
    }

    if (output.GetValue().empty()) {
        std::string output_filename = input.GetValue().substr(0, input.GetValue().find_last_of('.')) + ".h";
        output.SetValue(output_filename);
    }

    panda::Logger::InitializeStdLogging(panda::Logger::Level::ERROR,
                                        panda::Logger::ComponentMask().set(panda::Logger::Component::ETS_NAPI));

    return true;
}

int main(int argc, const char **argv)
{
    panda::PandArg<bool> help("help", false, "Print this message and exit");
    panda::PandArg<std::string> input("INPUT", "", "Input binary file");
    panda::PandArg<std::string> output("OUTPUT", "", "Output header file");

    panda::PandArgParser pa_parser;

    pa_parser.Add(&help);
    pa_parser.PushBackTail(&input);
    pa_parser.PushBackTail(&output);
    pa_parser.EnableTail();

    if (!ProcessArgs(pa_parser, input, output, help, argc, argv)) {
        return 1;
    }

    auto input_file = panda::panda_file::File::Open(input.GetValue());
    if (!input_file) {
        LOG(ERROR, ETS_NAPI) << "Cannot open file '" << input.GetValue() << "'";
        return 1;
    }

    panda::ets::header_writer::HeaderWriter writer(std::move(input_file), output.GetValue());

    auto created_header = writer.PrintFunction();
    if (!created_header) {
        std::cout << "No native functions found in file '" << input.GetValue() << "', header not created" << std::endl;
    }

    return 0;
}