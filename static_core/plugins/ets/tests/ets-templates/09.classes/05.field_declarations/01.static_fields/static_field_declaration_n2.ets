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
    A compile-time error occurs if he name of a field declared
    in the body of a class declaration is already used for another
    field or method in the same declaration.
tags: [compile-only, negative]
---*/

class A {
    public static x: int = 42
    public readonly x: number = 42.0 // CTE: name is already used
}

function main() {
    assert A.x == 42
}
