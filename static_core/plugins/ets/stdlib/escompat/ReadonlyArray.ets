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

interface ReadonlyArray<T> extends ConcatArray<T> {
    // TODO(ivan-tyulyandin): blocked by undefined implementation
    at(index: number): T | null;

    // concat(...items: (T | ConcatArray<T>)[]): T[];
    // concat(...items: (T | ConcatArray<T>)[]): Array<T>;
    // concat(...items: ConcatArray<T>[]): T[];
    // Typing and previous overload have to cover it

    // TODO(ivan-tyulyandin): blocked by tuples implementation
    // entries(): IterableIterator<[number, T]>;

    every(predicate: (value: T, index: number, array: ReadonlyArray<T>) => boolean): boolean;
    every(predicate: (value: T, index: number) => boolean): boolean;
    every(predicate: (value: T) => boolean): boolean;
    // every<S extends T>(predicate: (value: T, index: number, array: readonly T[]) => value is S, thisArg?: any): this is readonly S[];
    // Typing and previous overload have to cover it

    filter(predicate: (value: T, index: number, array: ReadonlyArray<T>) => boolean): Array<T>;
    filter(predicate: (value: T, index: number) => boolean): Array<T>;
    filter(predicate: (value: T) => boolean): Array<T>;
    // filter<S extends T>(predicate: (value: T, index: number, array: readonly T[]) => value is S, thisArg?: any): S[];
    // Typing and previous overload have to cover it

    // TODO(ivan-tyulyandin): blocked by undefined implementation
    find(predicate: (value: T, index: number, obj: ReadonlyArray<T>) => boolean): T | null;
    find(predicate: (value: T, index: number) => boolean): T | null;
    // find<S extends T>(predicate: (value: T, index: number, obj: readonly T[]) => value is S, thisArg?: any): S | undefined;
    // Typing and previous overload have to cover it

    findIndex(predicate: (value: T, index: number, obj: ReadonlyArray<T>) => boolean): number;
    findIndex(predicate: (value: T, index: number) => boolean): number;
    findIndex(predicate: (value: T) => boolean): number;

    // TODO(ivan-tyulyandin): TBD, FlatArray is non-subset type
    // flat<A, D extends number = 1>(
    //     this: A,
    //     depth?: D,
    // ): FlatArray<A, D>[];

    // TODO(ivan-tyulyandin): TBD
    // flatMap<U, This = undefined>(
    //     callback: (this: This, value: T, index: number, array: T[]) => U | ReadonlyArray<U>,
    //     thisArg?: This,
    // ): U[];

    forEach(callbackfn: (value: T, index: number, array: ReadonlyArray<T>) => void): void;
    forEach(callbackfn: (value: T, index: number) => void): void;
    forEach(callbackfn: (value: T) => void): void;

    // includes(searchElement: T, fromIndex?: number): boolean;
    includes(searchElement: T, fromIndex: number): boolean;
    includes(searchElement: T): boolean;

    // indexOf(searchElement: T, fromIndex?: number): number;
    indexOf(searchElement: T, fromIndex: number): number;
    indexOf(searchElement: T): number;

    // join(separator?: string): string;
    join(separator: string): string;
    join(): string;

    // TODO(kprokopenko): blocked by internal issue
    //keys(): IterableIterator<number>;

    // lastIndexOf(searchElement: T, fromIndex?: number): number;
    lastIndexOf(searchElement: T, fromIndex: number): number;
    lastIndexOf(searchElement: T): number;

    map<U>(callbackfn: (value: T, index: number, array: ReadonlyArray<T>) => U): Array<U>;
    map<U>(callbackfn: (value: T, index: number) => U): Array<U>;
    map<U>(callbackfn: (value: T) => U): Array<U>;

    reduce(callbackfn: (previousValue: T, currentValue: T, currentIndex: number, array: ReadonlyArray<T>) => T): T;
    reduce(callbackfn: (previousValue: T, currentValue: T, currentIndex: number) => T): T;
    reduce(callbackfn: (previousValue: T, currentValue: T) => T): T;
    reduce(callbackfn: (previousValue: T, currentValue: T, currentIndex: number, array: ReadonlyArray<T>) => T, initialValue: T): T;
    reduce(callbackfn: (previousValue: T, currentValue: T, currentIndex: number) => T, initialValue: T): T;
    reduce(callbackfn: (previousValue: T, currentValue: T) => T, initialValue: T): T;

    // TODO(ivan-tyulyandin): blocked by internal issue 13952
    //reduce<U>(callbackfn: (previousValue: U, currentValue: T, currentIndex: number, array: ReadonlyArray<T>) => U, initialValue: U): U;
    //reduce<U>(callbackfn: (previousValue: U, currentValue: T, currentIndex: number) => U, initialValue: U): U;
    //reduce<U>(callbackfn: (previousValue: U, currentValue: T) => U, initialValue: U): U;

    reduceRight(callbackfn: (previousValue: T, currentValue: T, currentIndex: number, array: ReadonlyArray<T>) => T): T;
    reduceRight(callbackfn: (previousValue: T, currentValue: T, currentIndex: number) => T): T;
    reduceRight(callbackfn: (previousValue: T, currentValue: T) => T): T;
    reduceRight(callbackfn: (previousValue: T, currentValue: T, currentIndex: number, array: ReadonlyArray<T>) => T, initialValue: T): T;
    reduceRight(callbackfn: (previousValue: T, currentValue: T, currentIndex: number) => T, initialValue: T): T;
    reduceRight(callbackfn: (previousValue: T, currentValue: T) => T, initialValue: T): T;

    // TODO(ivan-tyulyandin): blocked by internal issue 13952
    //reduceRight<U>(callbackfn: (previousValue: U, currentValue: T, currentIndex: number, array: ReadonlyArray<T>) => U, initialValue: U): U;
    //reduceRight<U>(callbackfn: (previousValue: U, currentValue: T, currentIndex: number) => U, initialValue: U): U;
    //reduceRight<U>(callbackfn: (previousValue: U, currentValue: T) => U, initialValue: U): U;

    // slice(start?: number, end?: number): T[];
    slice(start: number, end: number): Array<T>;
    slice(start: number): Array<T>;
    slice(): Array<T>;

    // some(predicate: (value: T, index: number, array: readonly T[]) => unknown, thisArg?: any): boolean;
    some(predicate: (value: T, index: number) => boolean): boolean;
    some(predicate: (value: T) => boolean): boolean;

    toLocaleString(): string;

    // toString(): string;
    // All objects in ArkTS have such method, no need to define it here

    values(): IterableIterator<T>;

    // TODO(ivan-tyulyandin): blocked by internal issue #11979
    //get length(): number;
}
