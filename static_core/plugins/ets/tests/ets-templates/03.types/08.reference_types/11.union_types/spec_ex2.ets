/*---
Copyright (c) 2021-2023 Huawei Device Co., Ltd.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
---*/

/*---
desc: >-
    A union type is a reference type created as a combination of other types or values.
    Values of union types can be valid values of all types and values of literals the
    union was created from.
tags: [compile-only, negative]
---*/

type T = 3
let t1: T = 2 // CTE

function main() {
    assert t1 == 2
}
