/*---
Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

{% for case in cases %}
/*---
desc: Assignment contexts does not allow the assignment for arrays if one has elements of a primitive types
params: Assignment from '{{case.src_type}}' to '{{case.dst_type}}'
tags:
- negative
- compile-only
---*/

function main(): void {
  let src_{{loop.index}}: {{case.src_type}}[] = new {{case.src_type}}[1];
  let dst_{{loop.index}}: {{case.dst_type}}[] = new {{case.dst_type}}[1];
  dst_{{loop.index}} = src_{{loop.index}};
  return;
  assert false;  // CTE: unreachable statement
}
{% endfor %}
