/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

package escompat;

export class IteratorResult<T> {
    done: boolean
    // TODO(ivan-tyulyandin): blocked by undefined implementation
    value: T | null

    constructor() {
        this.done = true
        this.value = null
    }

    constructor(done: boolean, value: T | null) {
        this.done = done
        this.value = value
    }
}

export interface Iterator<T> {
    next(): IteratorResult<T>;
}

export interface Iterable<T> {
    iterator(): Iterator<T>;
}

export interface IterableIterator<T> extends Iterator<T>, Iterable<T> {
}

function iteratorForEach<V>(x: Iterator<V>, fn: (x: V) => void): void {
    while (true) {
        const v = x.next()
        if (v.done) {
            return
        }
        fn(v.value!)
    }
}
