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
{%- for v in  vals %}
/*---
desc: >-
    ArkTS provides a number of operators to act on character values:
    Character operators that produce a value of type char.
---*/

const a: Char[] = new Char[2]

function main() {
    let v1 = new Char({{v.v1}})

    a[0] = ++v1
    v1++
    a[1] = v1

    assert a[0] == {{v.r1}}
    assert a[1] == {{v.r2}}
}
{%- endfor %}
