# Copyright (c) 2023 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(plugin_ets_compiler)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})


set(COMPILER_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/optimizer/ir_builder/ets_inst_builder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/optimizer/ets_intrinsics_peephole.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/optimizer/ets_codegen_extensions.cpp
  ${PANDA_ETS_PLUGIN_SOURCE}/compiler/ir_build_intrinsics_ets.cpp
)

if (NOT PANDA_TARGET_WINDOWS AND NOT PANDA_TARGET_MACOS)
  list(APPEND COMPILER_SOURCES
      ${PANDA_ETS_PLUGIN_SOURCE}/compiler/codegen_intrinsics_ets.cpp
  )
endif()

if (PANDA_ETS_INTEROP_JS)
  list(APPEND COMPILER_SOURCES
    ${PANDA_ETS_PLUGIN_SOURCE}/compiler/optimizer/ir_builder/js_interop/js_interop_inst_builder.cpp
  )
endif()

panda_target_sources(arkcompiler PRIVATE ${COMPILER_SOURCES})

panda_target_include_directories(arkcompiler PUBLIC
  ${GENERATED_DIR}
)

add_inst_templates(${CMAKE_CURRENT_SOURCE_DIR}/optimizer/ir_builder/ets_inst_templates.yaml)
