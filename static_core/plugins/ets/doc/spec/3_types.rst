..
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

.. _Types:

Types
#####

.. meta:
    frontend_status: Partly

This chapter introduces the notion of type that is one of the fundamental
concepts of |LANG| and other programming languages.
Type classification as accepted in the language is discussed below---along
with all aspects of using types in programs written in |LANG|.

Conventionally, the type of an entity is defined as the set of *values* the
entity can take, and the set of *operators* applicable to the entity of
a given type.

|LANG| is a statically typed language. It means that the type of every
declared entity and every expression is known at compile time. The type of
an entity is either set explicitly by a developer, or inferred implicitly
by the compiler.

There are two categories of types:

#. *Value types* (see :ref:`Value Types`), and

#. *Reference types* (see :ref:`Reference Types`).

The types integral to |LANG| are called *predefined types* (see
:ref:`Predefined Types`).

The types introduced, declared, and defined by a developer are called
*user-defined types*.
All *user-defined types* must always have a complete type definition
presented as source code in |LANG|.

.. index::
   statically typed language
   expression
   compile time
   value type
   reference type
   implicit inference
   compiler
   predefined type
   user-defined type
   type definition
   source code

|

.. _Predefined Types:

Predefined Types
****************

.. meta:
    frontend_status: Partly
    todo: unsigned types are not supported yet
    todo: void type is not supported yet(void as reference type)

Predefined types include the following:

-  Basic numeric value type: ``number``

-  High-performance value types:

     - Numeric types: ``byte``, ``short``, ``int``, ``long``, ``float``, and ``double``;

     - Character type: ``char``;

     - Boolean type: ``boolean``;


-  Reference types: ``object``, ``string``, ``[]`` (``array``), ``bigint``,
   ``void``, ``never``, and ``undefined``;

-  Class types: ``Object``, ``String``, ``Array<T>``, and ``BigInt``.


Each basic and high-performance value type has a corresponding predefined class
type that wraps the first value: ``Number``, ``Byte``, ``Short``, ``Int``,
``Long``, ``Float``, ``Double``, ``Char``, and ``Boolean``.

The predefined value types are called *primitive types*. Primitive type names
are reserved, and cannot be used for user-defined type names.

Type ``double`` is an alias to ``number``. Type ``Double`` is an alias
to ``Number``.

.. index::
   predefined type
   basic numeric type
   numeric type
   user-defined type
   type definition
   alias
   value type
   class type
   primitive type
   type alias
   wrapping

|

.. _User-Defined Types:

User-Defined Types
******************

.. meta:
    frontend_status: Partly

*User-defined* types include the following:

-  Class types (see :ref:`Classes`);
-  Interface types (see :ref:`Interfaces`);
-  Enumeration types (see :ref:`Enumerations`);
-  Array types (see :ref:`Array Types`);
-  Function types (see :ref:`Function Types`);
-  Tuple types (see :ref:`Tuple Types`);
-  Union types (see :ref:`Union Types`); and
-  Type parameters (see :ref:`Generic Parameters`).

.. index::
   user-defined type
   class type
   interface type
   enumeration type
   array type
   function type
   union type
   type parameter

|

.. _Types by Category:

Types by Category
*****************

.. meta:
    frontend_status: Partly
    todo: nullable types are not supported yet

The following table summarizes all |LANG| types:

+---------------------------------------------------+-----------------------------------+
|              Predefined Types                     |         User-Defined Types        |
+=========================+=========================+================+==================+
| Value Types             |     Reference Types     |   Value Types  |  Reference Types |
| (Primitive Types)       |                         |                |                  |
+-------------------------+-------------------------+----------------+------------------+
|  ``number``, ``byte``,  | ``Number``, ``Byte``,   | ``enum`` types | class types,     |
|  ``short``, ``int``,    | ``Short``, ``Int``,     |                | interface types, |
|  ``long``, ``float``,   | ``Long``, ``Float``,    |                | array types,     |
|  ``double``, ``char``,  | ``Double``, ``Char``,   |                | function types,  |
|  ``boolean``            | ``Boolean``,            |                | tuple types,     |
|                         | ``Object``, ``object``, |                | union types,     |
|                         | ``void``, ``null``,     |                | type parameters  |
|                         | ``String``, ``string``, |                |                  |
|                         | ``BigInt``, ``bigint``, |                |                  |
|                         | ``never``               |                |                  |
+-------------------------+-------------------------+----------------+------------------+

.. index::
   class type
   primitive type
   reference type
   value type
   interface type
   array type
   union type
   tuple type
   type parameter

|

.. _Using Types:

Using Types
***********

.. meta:
    frontend_status: Done

The following can refer to a type in a source code:

-  A reserved name for a primitive type;
-  A type reference for a named type (see :ref:`Named Types`), or a type alias
   (see :ref:`Type Alias Declaration`);
-  An in-place type definition for an array type (see :ref:`Array Types`), a
   function type (see :ref:`Function Types`), a tuple type (see :ref:`Tuple Types`),
   or a union type (see :ref:`Union Types`).

.. index::
   reserved name
   primitive type
   type alias
   type reference
   array type
   function type
   union type

.. code-block:: abnf

    type:
        predefinedType
        | typeReference
        | arrayType
        | tupleType
        | functionType
        | unionType
        | keyofType
        | '(' type ')'
        ;

An example of it is provided below:

.. code-block:: typescript
   :linenos:

    let b: boolean  // using primitive value type name
    let n: number   // using primitive value type name
    let o: Object   // using predefined class type name
    let a: number[] // using array type

Parentheses in types (where a type is a combination of array, function, or
union types) are used to specify the required type structure.
Without parentheses, the symbol '``|``' that constructs a union type
has the lowest precedence.

.. index::
   array type
   function type
   union type
   type structure
   symbol
   construct
   precedence

An example of it is provided below:

.. code-block:: typescript
   :linenos:

    // a nullable array with elements of type string:
    let a: string[] | null
    let s: string[] = []
    a = s    // ok
    a = null // ok, a is nullable

    // an array with elements whose types are string or null:
    let b: (string | null)[]
    b = null // error, b is an array and is not nullable
    b = ["aa", null] // ok

    // a function type that returns string or null
    let c: () => string | null
    c = null // error, c is not nullable
    c = (): string | null => { return null } // ok

    // (a function type that returns string) or null
    let d: (() => string) | null
    d = null // ok, d is nullable
    d = (): string => { return "hi" } // ok



.. _Named Types:

Named Types
***********

.. meta:
    frontend_status: Done

Classes, interfaces, enumerations, and unions are named types that are
introduced by class declarations (see :ref:`Classes`), interface
declarations (see :ref:`Interfaces`), enumeration declarations
(see :ref:`Enumerations`), and union declarations (see :ref:`Union Types`)
respectively.

Classes and interfaces with type parameters are *generic types*
(see :ref:`Generics`). Named types without type parameters are
*non-generic types*.

*Type references* (see :ref:`Type References`) refer to named types by
specifying their type names, and (where applicable) type arguments to be
substituted for the type parameters of the named type.

.. index::
   named type
   class
   interface
   enumeration
   union
   class declaration
   interface declaration
   enumeration declaration
   union declaration
   generic type
   generics
   type argument
   type parameter

|

.. _Type References:

Type References
***************

.. meta:
    frontend_status: Done

A type reference uses a type name (either *simple* or *qualified*, see
:ref:`Names`), or a type alias (see :ref:`Type Alias Declaration`) to
refer to a type.

Optionally, each identifier in a name or alias can be followed by type
arguments (see :ref:`Type Arguments`) if the referred type is a class
or an interface type.

.. index::
   type reference
   type name
   simple type name
   qualified type name
   identifier
   type alias
   type argument
   interface type

.. code-block:: abnf

    typeReference:
        typeReferencePart ('.' typeReferencePart)*
        ;

    typeReferencePart:
        Identifier typeArguments?
        ;

.. code-block:: typescript
   :linenos:

    let map: Map<string, number>

|

.. _Value Types:

Value Types
***********

.. meta:
    frontend_status: Partly
    todo: minor issue/feature - float/double literal parser in libc/libstdc++ can't parse everything, eg: 4.9E-324

The predefined integer types (see :ref:`Integer Types and Operations`),
floating-point types (see :ref:`Floating-Point Types and Operations`), the
boolean type (see :ref:`Boolean Types and Operations`), the character type
(see :ref:`Character Types and Operations`), and user-defined enumeration
types (see :ref:`Enumerations`) are *value types*.

Such types’ values do *not* share state with other values.

.. index::
   value type
   predefined type
   integer type
   floating-point type
   boolean type
   character type
   enumeration

|

.. _Integer Types and Operations:

Integer Types and Operations
============================

.. meta:
    frontend_status: Partly

+-----------+--------------------------------------------------------------------+--------------------------+
| Type      | Type's Set of Values                                               | Corresponding Class Type |
+===========+====================================================================+==========================+
| ``byte``  | All signed 8-bit integers (:math:`-2^7` to :math:`2^7-1`)          | ``Byte``                 |
+-----------+--------------------------------------------------------------------+--------------------------+
| ``short`` | All signed 16-bit integers (:math:`-2^{15}` to :math:`2^{15}-1`)   | ``Short``                |
+-----------+--------------------------------------------------------------------+--------------------------+
| ``int``   | All signed 32-bit integers (:math:`-2^{31}` to :math:`2^{31} - 1`) | ``Int``                  |
+-----------+--------------------------------------------------------------------+--------------------------+
| ``long``  | All signed 64-bit integers (:math:`-2^{63}` to :math:`2^{63} - 1`) | ``Long``                 |
+-----------+--------------------------------------------------------------------+--------------------------+
| ``bigint``| All integers with no limits                                        | ``BigInt``               |
+-----------+--------------------------------------------------------------------+--------------------------+

|LANG| provides a number of operators to act on integer values as listed below.

-  Comparison operators that produce a value of type *boolean*:

   +  Numerical comparison operators '<', '<=', '>', and '>=' (see :ref:`Numerical Comparison Operators`);
   +  Numerical equality operators '==' and '!=' (see :ref:`Value Equality Operators`);

-  Numerical operators that produce a value of type ``int``, ``long``, or ``bigint``:

   + Unary plus '+' and minus '-' operators (see :ref:`Unary Plus` and :ref:`Unary Minus`);
   + Multiplicative operators '\*', '/', and '%' (see :ref:`Multiplicative Expressions`);
   + Additive operators '+' and '-' (see :ref:`Additive Expressions`);
   + Increment operator '++' used as prefix (see :ref:`Prefix Increment`)
     or postfix (see :ref:`Postfix Increment`);
   + Decrement operator '--' used as prefix (see :ref:`Prefix Decrement`)
     or postfix (see :ref:`Postfix Decrement`);
   + Signed and unsigned shift operators '<<', '>>', and '>>>' (see 
     :ref:`Shift Expressions`);
   + Bitwise complement operator '~' (see :ref:`Bitwise Complement`);
   + Integer bitwise operators '&', '^', and '\|' (see :ref:`Integer Bitwise Operators`);

-  Conditional operator '?:' (see :ref:`Conditional Expressions`);
-  Cast operator (see :ref:`Cast Expressions`) that converts an integer value
   to a value of any specified numeric type;
-  String concatenation operator '+' (see :ref:`String Concatenation`) that, if
   a ``string`` operand and an ``integer`` operand are both available, converts
   the integer operand to a ``string`` (the character of a ``char``
   operand, or the decimal form of a ``byte``, ``short``, ``int``, or ``long``
   operand), and then creates a concatenation of two strings as a new ``string``.

.. index::
   byte
   short
   int
   long
   bigint
   Byte
   Short
   Int
   Long
   BigInt
   integer value
   comparison operator
   numerical comparison operator
   numerical equality operator
   equality operator
   numerical operator
   type reference
   type name
   simple type name
   qualified type name
   type alias
   type argument
   interface type
   postfix
   prefix
   unary operator
   unary operator
   additive operator
   multiplicative operator
   increment operator
   numerical comparison operator
   numerical equality operator
   decrement operator
   signed shift operator
   unsigned shift operator
   bitwise complement operator
   integer bitwise operator
   conditional operator
   cast operator
   integer value
   specific numeric type
   string concatenation operator
   operand

The classes ``Byte``, ``Short``, ``Int``, and ``Long`` also predefine other
constructors, methods, and constants that are parts of the |LANG| standard
library (see :ref:`Standard Library`).

If one operand is not ``long``, then the numeric promotion (see
:ref:`Primitive Types Conversions`) must be used first to widen
it to type ``long``.

If no operand is of type ``long``, then:

-  The operation implementation uses 32-bit precision;
-  The result of the numerical operator is of type ``int``.


If any operand is not ``int``, then the numeric promotion must be used
first to widen it to type ``int``.

Any integer type value can be cast to or from any numeric type.

Casts between the integer types and ``boolean`` are not possible.

The integer operators cannot indicate an overflow or an underflow.

An integer operator can throw errors (see :ref:`Error Handling`) as follows:

-  An integer division operator '/' (see :ref:`Division`), and an
   integer remainder operator '%' (see :ref:`Remainder`) throw the
   ``ArithmeticError`` if their right-hand operand is zero.
-  An increment operator '++' and a decrement operator '--' (see
   :ref:`Additive Expressions`) throw the *OutOfMemoryError* if boxing
   conversion (see :ref:`Primitive Types Conversions`) is required
   but the available memory is not sufficient to perform it.

.. index::
   Byte
   Short
   Int
   Long
   constructor
   method
   constant
   operand
   numeric promotion
   predefined numeric types conversion
   numeric type
   widening
   long
   int
   boolean
   integer type
   numeric type
   cast
   operator
   overflow
   underflow
   division operator
   remainder operator
   error
   increment operator
   decrement operator
   additive expression
   boxing conversion

|

.. _Floating-Point Types and Operations:

Floating-Point Types and Operations
===================================

.. meta:
    frontend_status: Partly

+-------------+-------------------------------------+--------------------------+
| Type        | Type's Set of Values                | Corresponding Class Type |
+=============+=====================================+==========================+
| ``float``   | The set of all IEEE-754 32-bit      | ``Float``                |
|             | floating-point numbers              |                          |
|             | floating-point numbers              |                          |
+-------------+-------------------------------------+--------------------------+
| ``number``, | The set of all IEEE-754 64-bit      | ``Number``               |
| ``double``  | floating-point numbers              | ``Double``               |
+-------------+-------------------------------------+--------------------------+

|LANG| provides a number of operators to act on floating-point type values as
listed below.

-  Comparison operators that produce a value of type ``boolean``:

   - Numerical comparison operators '<', '<=', '>', and '>=' (see
     :ref:`Numerical Comparison Operators`);
   - Numerical equality operators '==' and '!=' (see
     :ref:`Value Equality Operators`);

-  Numerical operators that produce values of type ``float`` or ``double``:

   + Unary plus '+' and minus '-' operators (see :ref:`Unary Plus` and :ref:`Unary Minus`);
   + Multiplicative operators '\*', '/', and '%' (see :ref:`Multiplicative Expressions`);
   + Additive operators '+' and '-' (see :ref:`Additive Expressions`);
   + Increment operator '++' used both as prefix (see :ref:`Prefix Increment`)
     and postfix (see :ref:`Postfix Increment`);
   + Decrement operator '--' used both as prefix (see :ref:`Prefix Decrement`)
     and postfix (see :ref:`Postfix Decrement`);
   + Signed and unsigned shift operators '<<', '>>', and '>>>' (see :ref:`Shift Expressions`);
   + Bitwise complement operator '~' (see :ref:`Bitwise Complement`);
   + Integer bitwise operators '&', '^', and '\|' (see :ref:`Integer Bitwise Operators`);
   
- Conditional operator '?:' (see :ref:`Conditional Expressions`);

-  Cast operator (see :ref:`Cast Expressions`) that converts a floating-point
   value to a value of any specified numeric type;
-  The string concatenation operator '+' (see :ref:`String Concatenation`) that,
   if both a ``string`` operand and a floating-point operand are present,
   converts the floating-point operand to a ``string`` with a value represented
   in the decimal form (without the loss of information), and then creates a
   concatenation of the two strings as a new ``string``.


.. index::
   floating-point type
   floating-point number
   operator
   numerical comparison operator
   numerical equality operator
   comparison operator
   boolean type
   numerical operator
   float
   double
   unary operator
   unary plus operator
   unary minus operator
   multiplicative operator
   additive operator
   prefix
   postfix
   increment operator
   decrement operator
   signed shift operator
   unsigned shift operator
   cast operator
   bitwise complement operator
   integer bitwise operator
   conditional operator
   string concatenation operator
   operand
   numeric type
   string

The classes ``Float`` and ``Double``, and the math library also predefine other
constructors, methods, and constants that are parts of the |LANG| standard
library (see :ref:`Standard Library`).

An operation is called a *floating-point operation* if at least one of the
operands in a binary operator is of the floating-point type (even if the other
operand is integer).

If at least one operand of the numerical operator is of type ``double``,
then the operation implementation uses 64-bit floating-point arithmetic, and
the result of the numerical operator is a value of type ``double``.

If the other operand is not ``double``, then the numeric promotion (see
:ref:`Primitive Types Conversions`) must be used first to widen it
to type ``double``.

If no operand is of type ``double``, then the operation implementation
uses 32-bit floating-point arithmetic, and the result of the numerical
operator is a value of type ``float``.

If the other operand is not ``float``, then the numeric promotion must be
used first to widen it to type ``float``.

Any floating-point type value can be cast to or from any numeric type.

.. index::
   Float
   Double
   class
   constructor
   method
   constant
   operation
   floating-point operation
   predefined numeric types conversion
   numeric type
   operand
   implementation
   float
   double
   numeric promotion
   numerical operator
   binary operator
   floating-point type

Casts between floating-point types and type ``boolean`` are not
possible.

Operators on floating-point numbers (except the remainder operator, see
:ref:`Remainder`) behave in compliance with the IEEE 754 Standard.
For example, |LANG| requires the support of IEEE 754 *denormalized*
floating-point numbers and *gradual underflow* that make it easier to prove
the desirable properties of a particular numerical algorithm. Floating-point
operations do not '*flush to zero*' if the calculated result is a
denormalized number.

|LANG| requires floating-point arithmetic to behave as if the floating-point
result of every floating-point operator is rounded to the result precision. An
*inexact* result is rounded to the representable value nearest to the infinitely
precise result; |LANG| uses the '*round to nearest*' principle (the default
rounding mode in IEEE-754), and prefers the representable value with the least
significant bit zero out of any two equally near representable values.

.. index::
   cast
   floating-point type
   floating-point number
   numeric type
   numeric types conversion
   widening
   operand
   implementation
   numeric promotion
   remainder operator
   gradual underflow
   flush to zero
   round to nearest
   rounding mode
   denormalized number

|LANG| uses '*round toward zero*' to convert a floating-point value to an
integer (see :ref:`Primitive Types Conversions`) that in this case acts as
if the number is truncated, and the mantissa bits are discarded.
The result of *rounding toward zero* is the value of that format that is
closest to an no greater in magnitude than the infinitely precise result.

A floating-point operation with overflow produces a signed infinity.

A floating-point operation with underflow produces a denormalized value
or a signed zero.

A floating-point operation with no mathematically definite result
produces NaN.

All numeric operations with a NaN operand result in NaN.

A floating-point operator (the increment '++' operator and decrement '--'
operator, see :ref:`Additive Expressions`) can throw the *OutOfMemoryError*
(see :ref:`Error Handling`) if boxing conversion (see
:ref:`Primitive Types Conversions`) is needed but the available
memory is not sufficient to perform it.

.. index::
   round toward zero
   conversion
   predefined numeric types conversion
   numeric type
   truncation
   truncated number
   rounding toward zero
   denormalized value
   NaN
   numeric operation
   increment operator
   decrement operator
   error
   boxing conversion
   overflow
   underflow
   signed zero
   signed infinity
   integer
   floating-point operation
   floating-point operator
   floating-point value
   throw
   
|

.. _Numeric Types Hierarchies:

Numeric Types Hierarchies
=========================

.. meta:
    frontend_status: Partly

Integer types and floating-point types are the *numeric types*.

Larger types include smaller types or their values:

-  ``double`` > ``float`` > ``long`` > ``int`` > ``short`` > ``byte``
-  ``bigint`` > ``long`` > ``int`` > ``short`` > ``byte``

A smaller type’s value can be assigned to a larger type’s variable.

Type *bigint* is an exception because any integer value or some string type
values can be converted into *bigint* by using built-in functions
*BigInt(anyInteger: long): bigint*, or *BigInt(anyIntegerString: string):
bigint*.

.. index::
   numeric type
   exception
   floating-point type
   assignment
   variable
   double
   float
   long
   int
   short
   byte
   bigint
   long
   int
   short
   byte
   string
   BigInt


|

.. _Boolean Types and Operations:

Boolean Types and Operations
============================

.. meta:
    frontend_status: Done

Type ``boolean`` represents logical values ``true`` and ``false`` that
correspond to the class type ``Boolean``.

The boolean operators are as follows:

-  Relational operators '==' and '!=' (see :ref:`Relational Expressions`);
-  Logical complement operator '!' (see :ref:`Logical Complement`);
-  Logical operators '&', '^', and '``|``' (see :ref:`Integer Bitwise Operators`);
-  Conditional-and operator '&&' (see :ref:`Conditional-And Expression`) and
   conditional-or operator '``||``' (see :ref:`Conditional-Or Expression`);
-  Conditional operator '?:' (see :ref:`Conditional Expressions`);
-  String concatenation operator '+' (see :ref:`String Concatenation`) 
   that converts the boolean operand to a string (``true`` or ``false``),
   and then creates a concatenation of the two strings as a new string.


The conversion of an integer or floating-point expression *x* to a boolean
value must follow the *C* language convention that any nonzero value is
converted to ``true``, and the value of zero is converted to ``false``.
In other words, the result of conversion of expression *x* to the boolean
type is always the same as the result of comparison *x != 0*.

.. index::
   boolean
   Boolean
   relational operator
   complement operator
   logical operator
   conditional-and operator
   conditional-or operator
   conditional operator
   string concatenation operator
   floating-point expression
   comparison
   conversion

|

.. _Character Types and Operations:

Character Types and Operations
==============================

.. meta:
    frontend_status: Partly

+-----------+----------------------------------+--------------------------+
| Type      | Type's Set of Values             | Corresponding Class Type |
+===========+==================================+==========================+
| ``char``  | Symbols with codes from \U+0000  | ``Char``                 |
|           | to \U+FFFF inclusive, that is,   |                          |
|           | from 0 to 65,535                 |                          |
+-----------+----------------------------------+--------------------------+

|LANG| provides a number of operators to act on character values as listed
below.

-  Comparison operators that produce a value of type *boolean*:

   +  Character comparison operators '<', '<=', '>', and '>=' (see :ref:`Numerical Comparison Operators`);
   +  Character equality operators '==' and '!=' (see :ref:`Value Equality Operators`);

-  Character operators that produce a value of type ``char``;

   + Unary plus '+' and minus '-' operators (see :ref:`Unary Plus` and :ref:`Unary Minus`);
   + Additive operators '+' and '-' (see :ref:`Additive Expressions`);
   + Increment operator '++' used both as prefix (see :ref:`Prefix Increment`)
     and postfix (see :ref:`Postfix Increment`);
   + Decrement operator '--' used both as prefix (see :ref:`Prefix Decrement`)
     and postfix (see :ref:`Postfix Decrement`);

-  Conditional operator '?:' (see :ref:`Conditional Expressions`);
-  The string concatenation operator '+' (see :ref:`String Concatenation`) that,
   if a string operand and character operand are available, converts the
   character operand to a string, and then creates a concatenation of the
   two strings as a new ``string``.

The class ``Char`` also provides other constructors, methods, and constants
that are parts of the |LANG| standard library (see :ref:`Standard Library`).

.. index::
   char
   Char
   boolean
   comparison operator
   equality operator
   unary operator
   additive operator
   increment operator
   postfix
   prefix
   decrement operator
   conditional operator
   concatenation operator
   operand
   constructor
   method
   constant

|

.. _Reference Types:

Reference Types
***************

.. meta:
    frontend_status: Partly

*Reference types* can be of the following kinds:

-  Class types (see :ref:`Classes`);
-  Interface types (see :ref:`Interfaces`);
-  Array types (see :ref:`Array Types`);
-  Function types (see :ref:`Function Types`);
-  Union types (see :ref:`Union Types`);
-  String types (see :ref:`String Type`);
-  Never type (see :ref:`never Type`), null type (see :ref:`null Type`), 
   undefined type (see :ref:`undefined Type`), 
   void type (see :ref:`void Type`); and
-  Type parameters (see :ref:`Generic Parameters`).

.. index::
   class type
   interface type
   array type
   function type
   union type
   string type
   never type
   undefined type
   void type
   type parameter

|

.. _Objects:

Objects
=======

.. meta:
    frontend_status: Done

An *object* can be a *class instance*, a *function instance*, or an *array*.

The pointers to these objects are called *references* or reference values.

A class instance creation expression (see :ref:`New Expressions`) explicitly
creates a class instance.

Referring to a declared function by its name, qualified name, or lambda
expression (see :ref:`Lambda Expressions`) explicitly creates a function
instance.

An array creation expression explicitly creates an array (see
:ref:`Array Creation Expressions`).

A string literal initialization explicitly creates a string.

Other expressions can implicitly create a class instance (see
:ref:`New Expressions`), or an array (see :ref:`Array Creation Expressions`).

.. index::
   object
   instance
   array
   reference value
   function instance
   class instance
   reference
   lambda expression
   qualified name
   name
   declared function
   array creation
   expression
   literal
   initialization

The operators on references to objects of class, interface and type
parameter are as follows:

-  Field access that uses a qualified name or a field access expression (see
   :ref:`Field Access Expressions`);
-  Call expression (see :ref:`Method Call Expression` and :ref:`Function Call Expression`);
-  Cast expression (see :ref:`Cast Expressions`);
-  String concatenation operator (see :ref:`String Concatenation`)
   that---given a *string* operand and a reference---calls the *toString*
   method of the referenced object, converts the reference to a *string*,
   and creates a concatenation of the two strings as a new ``string``;
-  ``instanceof`` operator (see :ref:`InstanceOf Expression`);
-  Reference equality operators '==' and '!=' (see :ref:`Reference Equality Operators`);
-  Conditional operator '?:' (see :ref:`Conditional Expressions`).


Multiple references to an object are possible.

Most objects have state. The state is stored in the field if an object is
an instance of class, or in a variable that is an element of an array object.

If two variables contain references to the same object, and the state of that
object is modified in one variable’s reference, then the state so modified can
be seen in the other variable’s reference.

.. index::
   operator
   object
   class
   interface
   type parameter
   field access
   qualified name
   method call expression
   function call expression
   field access expression
   cast expression
   call expression
   concatenation operator
   conversion
   reference equality operator
   conditional operator
   state
   array element
   variable
   field
   instance
   reference

|

.. _Object Class Type:

``Object`` Class Type
=====================

.. meta:
    frontend_status: Partly

The class ``Object`` is a supertype of all other classes, interfaces, string,
arrays, unions, function types, and enum types. Thus all of them inherit (see
:ref:`Inheritance`) the methods of the class ``Object`` as summarized below:

-  The method ``equals`` defines a notion of object equality, which is
   based on the comparison of values rather than references.
-  The method ``hashCode`` returns the integer hash code of the object.
-  The method ``toString`` returns a string representation of the
   object.

Though the name ``object`` refers to type ``Object``, it is recommended to
use ``Object`` in all cases.

.. index::
   class type
   function call expression
   field access expression
   cast expression
   concatenation operator
   operand
   reference
   method
   object
   object class type
   call expression   
   instanceof operator
   supertype
   interface
   array
   inheritance
   hash code

|

.. _string Type:

``string`` Type
===============

.. meta:
    frontend_status: Done

Type ``string`` is a predefined type. It stores sequences of characters as
Unicode UTF-16 code units.

Type ``string`` includes all string literals, e.g., ``'abc'``. The value
of a string object cannot be changed after it is created, i.e., a string
object is immutable, and can be shared.

Type ``string`` has dual semantics. If a string is assigned or passed as an
argument, then it behaves like a refrence type (see :ref:`Reference Types`),
while all string operations (see :ref:`String Concatenation` and
:ref:`String Comparison Operators`) treat strings as values (see
:ref:`Value Types`).

If the result is not a constant expression (see :ref:`Constant Expressions`),
then the string concatenation operator '+' (see :ref:`String Concatenation`)
implicitly creates a new string object.

Though the name ``String`` refers to the type ``string``, it is recommended to
use ``string`` in all cases.

.. index::
   string type
   Unicode code unit
   compiler
   predefined type
   extended semantics
   literal
   constant expression
   concatenation operator
   alias

|

.. _never Type:

``never`` Type
==============

The class ``never`` is a subclass (see :ref:`Subtyping`) of any other class.

The ``never`` class has no instances; it is used to represent values that
never exist (a function with this return type never returns but only throws
an error or exception).

.. index::
   subtyping
   class
   instance
   error
   exception
   function
   return type
   string literal
   string object
   constant expression
   concatenation operator
   alias
   subclass
   instance
   value

|

.. _void Type:

``void`` Type
=============

The ``void`` type has no instances (no values). It is typically used as the
return type if a function or a method returns no value.

.. code-block:: typescript
   :linenos:

    function foo (): void {}
   
    class C {
        bar(): void {}
    }

A compile-time occurs if:

-  ``void`` is used as type annotation;
-  An expression of the ``void`` type is used as a value.

.. code-block:: typescript
   :linenos:

    let x: void // compile-time error - void used as type annotation

    function foo (): void
    let y = foo()  // void used as a value


Type ``void`` can be used as type argument that instantiates a generic type
where a specific value of type argument is irrelevant. In this case, it
synonymic to type ``undefined`` (see :ref:`undefined Type`):

.. code-block:: typescript
   :linenos:

   class A<T>
   let a = new A<void>() // ok, type parameter is irrelevant
   let a = new A<undefined>() // ok, the same

   function foo<T>(x: T) {}

   foo<void>(undefined) // ok
   foo<void>(void) // compile-time error: void is used as value

.. index::
   return type
   type argument
   instantiation
   generic type
   type parameter argument

|

.. _Array Types:

Array Types
===========

.. meta:
    frontend_status: Partly
    todo: Inherited methods from baseclass - Object can't be invoked now

*Array type* is the built-in type characterized by the following:

-  Any object of array type contains elements indexed by integer position
   starting from 0;
-  Access to any array element is performed within the same time;
-  While being passed to non-|LANG| environment, an array is represented
   as a contiguous memory location;
-  Types of all array elements are upper-bounded by the element type
   specified in the array declaration.

.. index::
   array type
   array element
   access
   array

Two basic operations with array elements take elements out of, and put
elements into an array by using the ``[ ]`` operator and index expression.

Another important operation is the read-only field ``length``. It allows
knowing the number of elements in the array.

The example of syntax for the built-in array type is presented below:

.. index::
   array element
   index expression
   operator

.. code-block:: abnf

    arrayType:
       type '[' ']'
       ;

The family of array types that are parts of the standard library (see
:ref:`Standard Library`), including all available operations, is described
in the library documentation. Common to these types is that the ``[ ]``
operator can be applied to variables of all array types and to their derived
types. It is noteworthy that type ``T[]`` and type ``Array<T>`` are as follows:

-  Equivalent if *T* is a reference type; and
-  Different if *T* is a value type.

.. index::
   array type
   variable
   operator
   reference type
   value type
   derived type
   standard library

Examples are presented below:

.. code-block:: typescript
   :linenos:

    let a : number[] = [0, 0, 0, 0, 0] 
      /* allocate array with 5 elements of type number */
    a[1] = 7 /* put 7 as the 2nd element of the array, index of this element is 1 */
    let y = a[4] /* get the last element of array 'a' */
    let count = a.length // get the number of array elements

    let b: Number[] = new Array<Number>
       /* That is a valid code as type used in the 'b' declaration is identical
          to the type used in the new expression */

A type alias can set a name for an array type (see :ref:`Type Alias Declaration`):

.. code-block:: typescript
   :linenos:

    type Matrix = number[][] /* Two-dimensional array */

As an object, an array is assignable to a variable of type *Object*:

.. code-block:: typescript
   :linenos:

    let a: number[] = [1, 2, 3]
    let o: Object = a

.. index::
   alias
   array type
   object
   array
   assignment
   variable

|

.. _Function Types:

Function Types
==============

.. meta:
    frontend_status: Done

A *function type* can be used to express the expected signature of a function.
A function type consists of the following:

-  List of parameters (which can be empty);
-  Optional return type;
-  Optional keyword ``throws``.

.. index::
   array element
   type alias
   array type
   type Object
   keyword throws
   function type
   signature

.. code-block:: abnf

    functionType:
        '(' ftParameterList? ')' ftReturnType 'throws'?
        ;

    ftParameterList:
        ftParameter (',' ftParameter)\* (',' restParameter)?
        | restParameter
        ;

    ftParameter:
        identifier ':' type
        ;

    restParameter:
        '...' ftParameter
        ;

    ftReturnType:
        '=>' type
        ;

The *rest* parameter is described in :ref:`Rest Parameter`.

.. index::
   rest parameter

.. code-block:: typescript
   :linenos:

    let binaryOp: (x: number, y: number) => number
    function evaluate(f: (x: number, y: number) => number) { }

A type alias can set a name for a function type (see :ref:`Type Alias Declaration`).

.. index::
   type alias
   function type

.. code-block:: typescript
   :linenos:

    type BinaryOp = (x: number, y: number) => number
    let op: BinaryOp

If the function type contains the '``throws``' mark (see
:ref:`Throwing Functions`), then it is the *throwing function type* .

Function types assignability is described in :ref:`Assignment and Call Contexts`,
and conversions in :ref:`Function Types Conversions`.

.. index::
   function type
   return type
   type void
   throwing function
   throwing function type
   throws mark

|

.. _null Type:

``null`` Type
=============

The only value of type ``null`` is represented by the keyword  ``null``
(see :ref:`Null Literal`).

Using type ``null`` as type annotation is not recommended, except in
nullish types (see :ref:`Nullish Types`).

.. index::
   type null
   null literal
   keyword null
   type annotation
   nullish type

|

.. _undefined Type:

``undefined`` Type
==================

The only value of type ``undefined`` is represented by the keyword
``undefined`` (see :ref:`Undefined Literal`).

Using type ``undefined`` as type annotation is not recommended,
except in nullish types (see :ref:`Nullish Types`).

The ``undefined`` type can be used as the type argument that instantiates a generic type where a
specific value of the type argument is irrelevant.

.. code-block:: typescript
   :linenos:

   class A<T> {}
   let a = new A<undefined>() // ok, type parameter is irrelevant
   function foo<T>(x: T) {}
   
   foo<undefined>(undefined) // ok


.. index::
   type undefined
   keyword undefined
   literal
   annotation
   nullish type

|

.. _Tuple Types:

Tuple Types
===========

.. code-block:: abnf

    tupleType:
        '[' (type (',' type)*)? ']' 
        ;

A *tuple* type is a reference type created as a fixed set of other types.
The value of a tuple type is a group of values of other types that comprise
the tuple type. Such other types are specified in the same order as declared
within the tuple type declaration. Each tuple element is thus implied to
have its own type.
The operator ``[ ]`` (square brackets) is used to access elements of a tuple
in a manner similar to that used to access elements of an array.

The index expression is of *integer* type, and the index of the 1st tuple
element is *0*. Only constant expressions can be used as index to get access
to tuple elements.

.. code-block:: typescript
   :linenos:

   let tuple: [number, number, string, boolean, Object] = 
              [     6,      7,  "abc",    true,    666]
   tuple[0] = 666
   console.log (tuple[0], tuple[4]) // `666 666` be printed

*Object* (see :ref:`Object Class Type`) is a super type for any tuple type.

An empty tuple is a corner case. It is only added to support compatibility
with |TS|.

.. code-block:: typescript
   :linenos:

   let empty: [] = [] // empty tuple with no elements in it

|

.. _Union Types:

Union Types
===========

.. meta:
   frontend_status: Partly
   todo: support literal in union

.. code-block:: abnf

    unionType:
        type|literal ('|' type|literal)*
        ;

A *union* type is a reference type created as a combination of other
types or values. Values of union types can be valid values of all
types and literals the union is created from.

A :index:`compile-time error` occurs if the type in the right-hand side
of a union type declaration leads to a circular reference.

If a *union* uses a primitive type (see *Primitive types* in
:ref:`Types by Category`), then automatic boxing occurs to keep the reference
nature of the type.

The reduced form of *union* types allows defining a type which has only
one value, as in the example below:

.. index::
   union type
   reference type
   circular reference
   union
   primitive type
   literal
   primitive type
   automatic boxing
   
.. code-block:: typescript
   :linenos:

   type T = 3
   let t1: T = 3 // OK
   let t2: T = 2 // Compile-time error

A typical example of *union* type usage is shown below:

.. code-block:: typescript
   :linenos:

    class Cat {
      // ...
    }
    class Dog {
      // ...
    }
    class Frog {
      // ...
    }
    type Animal = Cat | Dog | Frog | number
    // Cat, Dog, and Frog are some types (class or interface ones)

    let animal: Animal = new Cat()
    animal = new Frog() 
    animal = 42
    // One may assign the variable of the union type with any valid value

Different mechanisms can be used to get values of particular types from a
*union*. For example:

.. code-block:: typescript
   :linenos:

    class Cat { sleep () {}; meow () {} }
    class Dog { sleep () {}; bark () {} }
    class Frog { sleep () {}; leap () {} }

    type Animal = Cat | Dog | Frog | number

    let animal: Animal = new Cat()
    if (animal instanceof Frog) { // animal is of type Frog here
        let frog: Frog = animal as Frog // Use as conversion here
        animal.leap()
        frog.leap()
        // As a result frog leaps twice
    }

    animal.sleep () // Any animal can sleep


Below is an example for primitive types:

.. code-block:: typescript
   :linenos:

    type Primitive = number | boolean
    let p: Primitive = 7
    if (p instanceof Number) { // type of 'p' is Number here
       let i: number = p as number // Explicit conversion from Primitive to number
    }

--- and for values:

.. code-block:: typescript
   :linenos:

    type BMW_ModelCode = 325 | 530 | 735
    let car_code: BMW_ModelCode = 325
    if (car_code == 325){
       car_code = 530
    } else if (car_code == 530){
       car_code = 735
    } else {
       // pension :-)
    }

Please note that it will be a compile-time error to compare a variable of the
union type with the value which does not belong to this union type values

.. code-block:: typescript
   :linenos:

    type BMW_ModelCode = 325 | 530 | 735
    let car_code: BMW_ModelCode = 325
    if (car_code == 666){ ... }
    /*
       compile-time error as 666 does not belong to
       values of type BMW_ModelCode
    */


.. _Union Types Normalization:

Union Types Normalization
-------------------------

Union types normalization allows minimizing the number of types and literals
within a union type while keeping the type's safety. Some types or literals
can also be replaced for more general types.

Formally, union type *T*:sub:`1` | ... | *T*:sub:`N`, where N > 1, can be
reduced to type *U*:sub:`1` | ... | *U*:sub:`M`, where M <= N, or even to
non-union type *V*.

The normalization process presumes performing the following steps one after
another:

.. index::
   union type
   non-union type
   normalization
   literal

#. All nested union types are linearized.
#. Identical types within the union type are replaced for a single type.
#. Identical literals within the union type are replaced for a single literal.
#. If at least one type in the union is *Object*, then the entire union type is
   reduced to *Object* type.
#. If there is at least one numeric type among the numeric union types or
   numeric literals, then all such types or literals are replaced for type
   *number*.
#. If a literal of union type belongs to the values of a type that is part
   of the union, then the literal is removed.
#. This step is performed recursively until no mutually compatible types remain
   (see :ref:`Compatible Types`), or the union type is reduced to a single type:

   -  If a union type includes two types *T*:sub:`i` and *T*:sub:`j` (i != j),
      and *T*:sub:`i` is compatible with *T*:sub:`j` (see
      :ref:`Compatible Types`), then only *T*:sub:`j` remains in the union
      type, and *T*:sub:`i` is removed.
   -  If *T*:sub:`j` is compatible with *T*:sub:`i` (see :ref:`Compatible Types`),
      then *T*:sub:`i` remains in the union type, and *T*:sub:`j` is removed.

.. index::
   union type
   linearization
   literal non-union type
   normalization
   literal
   Object type
   numeric union type
   compatible type
   type compatibility

A normalized union type is the result of this process.

This process is presented in the examples below:

.. code-block:: typescript
   :linenos:

    ( T1 | T2) | (T3 | T4) => T1 | T2 | T3 | T4  // Linearization
    1 | 1 | 1  =>  1                             // Identical values elimination
    number | number => number                    // Identical types elimination
    1 | number | number => number                // Number wins
    int | double | short => number 
    1 | string | number => string | number       // Union value elimination
    1 | Object => Object                         // Object wins
    AnyType | Object | AnyType => Object
    class Base {}
    class Derived1 extends Base {}
    class Derived2 extends Base {}   
    Base | Derived1 => Base                      // Base wins
    Derived1 | Derived2 => Derived1 | Derived2   // End of normalization

The |LANG| compiler applies such normalization while processing union types
and handling the type inference for array literals (see
:ref:`Type Inference from Types of Elements`).

.. index::
   union type
   normalization
   array literal
   type inference
   array literal

.. _Keyof Types:

Keyof Types
-----------

A special form of union types are keyof types. Such types are built with help
of the *keyof* keyword which is applied to the class (see :ref:`Classes`) or
interface (see :ref:`Interfaces`) type. As a result, the new type is the union
of names of all members of the class or interface type. 


.. code-block:: abnf

    keyofType:
        'keyof' typeReference
        ;

It is a compile-time error if *typeReference* is not a class or interface type.

Example below illustrates the semantics of the keyof type.

.. code-block:: typescript
   :linenos:

    class A {
       field: number
       method() {}
    }
    type KeysOfA = keyof A // "field" | "method"
    let a_keys: KeysOfA = "field" // OK
    a_keys = "any string different from field or method"
      // Compile-time error: invalid value for the type KeysOfA

if class or interface is empty then its keyof type is equivalent to never type.

.. code-block:: typescript
   :linenos:

    class A {} // Empty class 
    type KeysOfA = keyof A // never


|

.. _Nullish Types:

Nullish Types
==============

.. meta:
    frontend_status: Partly

|LANG| has nullish types that are in fact a special form of the union types
(see :ref:`Union Types`).

.. code-block:: abnf

    nullishType:
        type '|' ( 'null' | 'undefined' )
        ;

All predefined and user-defined type declarations create non-nullish types.
Non-nullish types cannot have a ``null`` or ``undefined`` value at runtime.

*T* \| ``null`` or *T* \| ``undefined`` can be used as the type to specify a
nullish version of type *T*. 

A variable declared to have the type *T* \| ``null`` can hold values of the
type *T* and its derived types, or the value ``null``. Such type is called
a *nullable type*.

A variable declared to have the type *T* \| ``undefined`` can hold values
of the type *T* and its derived types, or the value ``undefined``.

A nullish type is a reference type (see :ref:`Union Types`).
A reference which is ``null`` or ``undefined`` is called a *nullish* value.

An operation that is safe with no regard to the presence or absence of
nullish values (e.g., re-assigning one nullable value to another) can
be used 'as is' for nullish types.

The following nullish-safe options exist for operations on nullish type *T*
that can potentially violate null safety (e.g., access to a property):

.. index::
   union type
   type inference
   array literal
   nullish type
   nullable type
   non-nullish type
   predefined type declaration
   user-defined type declaration
   undefined value
   runtime
   derived type
   reference type
   nullish value
   nullish-safe option
   null safety
   access
   assignment
   re-assignment

-  Using safe operations:

   -  Safe method call (see :ref:`Method Call Expression` for details);
   -  Safe field access expression (see :ref:`Field Access Expressions`
      for details);
   -  Safe indexing expression (see :ref:``Indexing Expression`` for details);
   -  Safe function call (see :ref:`Function Call Expression` for details);

-  Converting from *T* \| ``null`` or *T* \| ``undefined`` to *T*:

   -  Cast expression (see :ref:`Cast Expressions` for details);
   -  Ensure-not-nullish expression (see :ref:`Ensure-Not-Nullish Expressions`
      for details);

-  Supplying a default value to use if a nullish value is present:

   -  Nullish-coalescing expression (for details see :ref:`Nullish-Coalescing Expression`).

.. index::
   method call
   field access expression
   indexing expression
   function call
   converting
   cast expression
   ensure-not-nullish expression
   nullish-coalescing expression
   nullish value
   cast expression

|

.. _DynamicObject Type:

DynamicObject Type
==================

.. meta:
    frontend_status: None

The interface *DynamicObject* is used to provide seamless interoperability
with dynamic languages (e.g., Javascript and TypeScript), and
to support advanced language features such as *dynamic import*
(see :ref:`Dynamic Import`). This interface is defined in the standard library
(see :ref:`Standard Library`).

It is a common interface for a set of wrappers (also defined in the standard
library) that provide access to underlying objects.
The *DynamicObject* instance cannot be created directly;
only an instance of a specific wrapper object can be instantiated.
For example, a result of the *dynamic import* expression (see
:ref:`Dynamic Import Expression`) is an instance of the dynamic object
implementation class that wraps an object containing exported entities
of the imported module.

*DynamicObject* is a predefined type. When applied to objects of this type,
the following operations are handled by the compiler in a special way:

- Field access;
- Method call;
- Indexing access;
- New;
- Cast.

.. index::
   DynamicObject
   interoperability
   dynamic import
   interface
   wrapper
   access
   underlying object
   instantiation
   export
   entity
   import
   predefined type
   field access
   indexing access
   method call


|

.. _DynamicObject Field Access:

DynamicObject Field Access
--------------------------

.. meta:
    frontend_status: Partly
    todo: now it supports only JSValue, need to add full abstract support
    
The field access expression *D.F*, where *D* is of type *DynamicObject*,
is handled as an access to a property of an underlying object.

If the value of a field access is used, then it is wrapped in the instance of
*DynamicObject*, since the actual type of the field is not known at compile
time.


.. code-block:: typescript
   :linenos:

   function foo(d: DynamicObject) {
      console.log(d.f1) // access of the property named "f1" of underlying object
      d.f1 = 5 // set a value of the property named "f1"
      let y = d.f1 // 'y' is of type DynamicObject
   }

The wrapper can raise an error if:

- No property with the specified name exists in the underlying object; or
- The field access is in the right-hand side of the assignment, and the
  type of the assigned value is not compatible (see :ref:`Compatible Types`)
  with the type of the property.

.. index::
   DynamicObject
   wrapper
   dynamic import
   underlying object
   field access
   property
   instance
   assignment
   assigned value


|

.. _DynamicObject Method Call:

DynamicObject Method Call
-------------------------

.. meta:
    frontend_status: Partly
    todo: now it supports only JSValue, need to add full abstract support
    
The method call expression *D.F()*, where *D* is of type *DynamicObject*,
is handled as a call of the instance method of an underlying object.

If the result of a method call is used, then it is wrapped in the instance
of *DynamicObject*, since the actual type of the returned value is not known
at compile time.

.. code-block:: typescript
   :linenos:

   function foo(d: DynamicObject) {
      d.foo() // call of a method "foo" of underlying object
      let y = d.goo() // 'y' is of type DynamicObject
   }

The wrapper must raise an error if:

- No method with the specified name exists in the underlying object; or
- The signature of the method is not compatible with the types of the
  call arguments.

.. index::
   DynamicObject
   wrapper
   method
   dynamic import
   field access
   property
   instance
   method
   call argument

|

.. _DynamicObject Indexing Access:

DynamicObject Indexing Access
-----------------------------

.. meta:
    frontend_status: Partly
    todo: now it supports only JSValue, need to add full abstract support
    
The indexing access expression *D[index]*, where *D* is of type *DynamicObject*,
is handled as an indexing access to an underlying object.

.. code-block:: typescript
   :linenos:

   function foo(d: DynamicObject) {
      let x = d[0] // call of a method "foo" of underlying object
   }

TBD

.. index::
   DynamicObject
   indexing access
   underlying object

|

.. _Default Values for Types:

Default Values for Types
************************

.. meta:
    frontend_status: Partly

**Note**: This is part of the |LANG|'s experimental mode.

Some types use so-called *default values* for variables without explicit
initialization (see :ref:`Variable Declarations`), including the following:

.. - All primitive types and ``string`` (see the table below).

- All primitive types (see the table below).
- All union types that have at least one nullish (see :ref:`Nullish Types`)
  value, and use an appropriate nullish value as default (see the table below).

.. -  Nullable reference types with the default value ``null`` (see :ref:`Literals`).

All other types, including reference types and enumeration types, have no
default values.

A variables of such a type must be initialized explicitly with a value
before its first use.

.. The default values of primitive types and ``string`` are as follows:

The default values of primitive types are as follows:

.. index::
   default value
   variable
   explicit initialization
   nullable reference type
   primitive type
   reference type
   enumeration type

|

+----------------+--------------------+
|    Data Type   |   Default Value    |
+================+====================+
| ``number``     | 0 as ``number``    |
+----------------+--------------------+
| ``byte``       | 0 as ``byte``      |
+----------------+--------------------+
| ``short``      | 0 as ``short``     |
+----------------+--------------------+
| ``int``        | 0 as ``int``       |
+----------------+--------------------+
| ``long``       | 0 as ``long``      |
+----------------+--------------------+
| ``float``      | +0.0 as ``float``  |
+----------------+--------------------+
| ``double``     | +0.0 as ``double`` |
+----------------+--------------------+
| ``char``       | '\u0000'           |
+----------------+--------------------+
| ``boolean``    | ``false``          |
+----------------+--------------------+

.. | ``string``     | ``""``             |

.. +----------------+--------------------+


The default values of nullish union types are as follows:

+------------------+--------------------+
|    Data Type     |   Default Value    |
+==================+====================+
| type | null      | ``null``           |
+------------------+--------------------+
| type | undefined | ``undefined``      |
+------------------+--------------------+
| null | undefined | ``undefined``      |
+------------------+--------------------+

.. code-block:: typescript
   :linenos:

   class A {
     f1: number|null
     f2: string|undefined
     f3?: boolean
   }
   let a = new A()
   console.log (a.f1, a.f2, a.f3)
   // Output: null, undefined, undefined


.. index::
   number
   byte
   short
   int
   long
   float
   double
   char
   boolean

.. raw:: pdf

   PageBreak


