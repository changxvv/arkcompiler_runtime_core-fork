/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

{% for fd in function_decls %}
/*---
desc: >-
    The ArkTS language allows specifying a function that can be called in
    different ways by writing overload signatures, i.e., by writing several
    function headers that have the same name but different signatures
    followed by one implementation function.
tags: [compile-only, negative]
---*/

{{fd.decl}}

function main() {
    {{fd.call}}
}
{% endfor %}
