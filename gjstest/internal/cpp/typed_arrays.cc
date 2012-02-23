// Copyright 2012 Google Inc. All Rights Reserved.
// Author: jacobsa@google.com (Aaron Jacobs)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// This file contains code imported from the v8 project, at trunk/src/d8.cc, and
// then modified by Aaron Jacobs. The original copyright notice is as follows.

// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gjstest/internal/cpp/typed_arrays.h"

#include "base/logging.h"

using v8::Arguments;
using v8::Array;
using v8::ExternalArrayType;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Int32;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::ReadOnly;
using v8::String;
using v8::True;
using v8::TryCatch;
using v8::Value;
using v8::kExternalByteArray;
using v8::kExternalDoubleArray;
using v8::kExternalFloatArray;
using v8::kExternalIntArray;
using v8::kExternalShortArray;
using v8::kExternalUnsignedByteArray;
using v8::kExternalUnsignedIntArray;
using v8::kExternalUnsignedShortArray;

namespace gjstest {

static const char kArrayBufferReferencePropName[] = "_is_array_buffer_";
static const char kArrayBufferMarkerPropName[] = "_array_buffer_ref_";

static size_t ConvertToUint(
    const Handle<Value>& value_in,
    TryCatch* try_catch) {
  CHECK(!value_in.IsEmpty());

  if (value_in->IsUint32()) {
    return value_in->Uint32Value();
  }

  Local<Value> number = value_in->ToNumber();
  if (try_catch->HasCaught()) return 0;

  CHECK(number->IsNumber());
  Local<Int32> int32 = number->ToInt32();
  if (try_catch->HasCaught() || int32.IsEmpty()) return 0;

  int32_t raw_value = int32->Int32Value();
  if (try_catch->HasCaught()) return 0;

  if (raw_value < 0) {
    ThrowException(String::New("Array length must not be negative."));
    return 0;
  }

  static const int kMaxLength = 0x3fffffff;
  if (raw_value > static_cast<int32_t>(kMaxLength)) {
    ThrowException(
        String::New("Array length exceeds maximum length."));
  }

  return static_cast<size_t>(raw_value);
}

// Called when a weak reference expires. If this is a weak reference to an array
// buffer, we should delete its underlying data.
static void ExternalArrayWeakCallback(
    Persistent<Value> val,
    void* data) {
  HandleScope scope;
  const Handle<Object> object = val->ToObject();

  // Attempt to get the reference to this array's underlying array buffer.
  const Local<Value> buffer_reference =
      object->Get(
          String::New(
              kArrayBufferReferencePropName));

  // Free the data if this is an array buffer, rather than a reference to the
  // array buffer.
  if (data != NULL && !buffer_reference->IsObject()) {
    free(data);
  }

  // Dispose of the persistent reference.
  val.Dispose();
}

// Create an external array from an underlying set of data. Takes ownership of
// the data.
static Handle<Object> CreateExternalArray(
    void* data,
    size_t num_elements,
    size_t element_size,
    ExternalArrayType element_type) {
  Handle<Object> result = Object::New();

  // Create a weak reference to the handle that will delete the underlying data
  // when it is disposed of.
  Persistent<Object> persistent = Persistent<Object>::New(result);
  persistent.MakeWeak(data, ExternalArrayWeakCallback);
  persistent.MarkIndependent();

  // Set the backing store for indexed elements.
  result->SetIndexedPropertiesToExternalArrayData(
      data,
      element_type,
      num_elements);

  // Set up the length and BYTES_PER_ELEMENT properties on the result.
  result->Set(
      String::New("length"),
      Int32::New(num_elements),
      ReadOnly);

  result->Set(
      String::New("BYTES_PER_ELEMENT"),
      Int32::New(element_size));

  return result;
}

static bool IsArrayBuffer(const Handle<Value>& val) {
  return val->IsObject() &&
         val->ToObject()->Get(String::New(kArrayBufferMarkerPropName))->
             IsTrue();
}

// Create a typed array from an existing array buffer. The array_buffer
// reference must be non-empty, but the other two arguments are optional.
static Handle<Value> CreateExternalArrayFromArrayBuffer(
    ExternalArrayType element_type,
    size_t element_size,
    const Handle<Object>& array_buffer,
    const Handle<Value>& byte_offset_arg,
    const Handle<Value>& length_arg) {
  CHECK(!array_buffer.IsEmpty());
  CHECK(IsArrayBuffer(array_buffer));

  TryCatch try_catch;

  // Figure out what the length of the existing array buffer is.
  const Local<Value> array_buffer_length_property =
      array_buffer->Get(String::New("length"));
  CHECK(!array_buffer_length_property.IsEmpty());

  const size_t array_buffer_length =
      ConvertToUint(array_buffer_length_property, &try_catch);

  if (try_catch.HasCaught()) {
    return try_catch.Exception();
  }

  // Figure out what the offset into the array buffer should be.
  size_t byte_offset = 0;
  if (!byte_offset_arg.IsEmpty()) {
    byte_offset = ConvertToUint(byte_offset_arg, &try_catch);
    if (try_catch.HasCaught()) {
      return try_catch.Exception();
    }
  }

  // Make sure the offset is legal.
  if (byte_offset % element_size) {
    return ThrowException(
        String::New("Offset must be a multiple of element size."));
  }

  if (byte_offset > array_buffer_length) {
    return ThrowException(
        String::New("Offset must be less than the array buffer length."));
  }

  // Figure out what the length of the resulting array should be (in elements,
  // not bytes)
  size_t length = 0;
  if (length_arg.IsEmpty()) {
    // If the length arg is omitted, the array spans from the byte offset to the
    // end of the array buffer. The size of the range spanned must be a multiple
    // of the element size.
    if ((array_buffer_length - byte_offset) % element_size) {
      return ThrowException(
          String::New(
              "Array buffer length minus the byte offset must be a "
                  "multiple of the element size"));
    }

    length = (array_buffer_length - byte_offset) / element_size;
  } else {
    length = ConvertToUint(length_arg, &try_catch);
    if (try_catch.HasCaught()) {
      return try_catch.Exception();
    }
  }

  // Make sure the length is legal.
  if (byte_offset + (length * element_size) > array_buffer_length) {
    return ThrowException(
        String::New(
            "length references an area beyond the end of the array buffer."));
  }

  // Grab the data property from the array buffer.
  void* const data = array_buffer->GetIndexedPropertiesExternalArrayData();

  if (!data) {
    return ThrowException(String::New("ArrayBuffer doesn't have data."));
  }

  // Create the resulting object.
  const Handle<Object> result =
      CreateExternalArray(
          static_cast<uint8_t*>(data) + byte_offset,
          length,
          element_size,
          element_type);

  // Hold a reference to the ArrayBuffer so its buffer doesn't get collected.
  result->Set(
      String::New(kArrayBufferReferencePropName),
      array_buffer,
      ReadOnly);

  return result;
}

// Implement the constructor with this signature:
//
//     TypedArray(unsigned long length)
//
static Handle<Value> CreateExternalArrayWithLengthArg(
    ExternalArrayType element_type,
    size_t element_size,
    const Handle<Value>& length_arg) {
  CHECK(!length_arg.IsEmpty());
  CHECK(element_size);

  TryCatch try_catch;

  // Convert the length to a useful value.
  const size_t length = ConvertToUint(length_arg, &try_catch);
  if (try_catch.HasCaught()) {
    return try_catch.Exception();
  }

  // Create the underlying data buffer.
  void* const data = calloc(length, element_size);
  if (!data) {
    return ThrowException(String::New("Memory allocation failed."));
  }

  return CreateExternalArray(data, length, element_size, element_type);
}

// Implement the ArrayBuffer constructor.
static Handle<Value> CreateArrayBuffer(
    const Arguments& args) {
  // Check the number of arguments.
  const size_t num_args = args.Length();
  if (num_args != 1) {
    return ThrowException(String::New("Expected exactly one argument."));
  }

  // Create the underlying array.
  const ExternalArrayType element_type = v8::kExternalByteArray;
  const size_t element_size = 1;

  const Handle<Value> result =
      CreateExternalArrayWithLengthArg(
          element_type,
          element_size,
          args[0]);

  // Mark this as an array buffer, for use by other code in this file.
  result->ToObject()->Set(
      String::New(kArrayBufferMarkerPropName),
      True(),
      ReadOnly);

  return result;
}

// Implement the constructor with this signature:
//
//     TypedArray(type[] array)
//
template <typename T>
static Handle<Value> CreateExternalArrayWithArrayArg(
    ExternalArrayType element_type,
    const Handle<Array>& array) {
  CHECK(!array.IsEmpty());
  TryCatch try_catch;

  const size_t element_size = sizeof(T);
  const uint32_t num_elements = array->Length();

  // Create the underlying data.
  T* const data =
      static_cast<T*>(
          calloc(num_elements, element_size));

  if (!data) {
    return ThrowException(String::New("Memory allocation failed."));
  }

  // Convert each element.
  for (uint32_t i = 0; i < num_elements; ++i) {
    data[i] = array->Get(i)->ToInteger()->Value();
  }

  return CreateExternalArray(
      data,
      num_elements,
      element_size,
      element_type);
}

// Common constructor code for all typed arrays. The following signatures are
// supported:
//
//     TypedArray(
//         ArrayBuffer buffer,
//         optional unsigned long byteOffset,
//         optional unsigned long length)
//
//     TypedArray(unsigned long length)
//
//     TypedArray(type[] array)
//
template <typename T>
static Handle<Value> CreateExternalArray(
    const Arguments& args,
    ExternalArrayType element_type) {
  const size_t element_size = sizeof(T);
  const size_t num_args = args.Length();

  // We only support these element sizes.
  CHECK(
      element_size == 1 ||
      element_size == 2 ||
      element_size == 4 ||
      element_size == 8);

  // We require at least one arg.
  if (num_args == 0) {
    return ThrowException(String::New("Expected at least one argument."));
  }

  // Is this the constructor with the following signature?
  //
  //     TypedArray(
  //         ArrayBuffer buffer,
  //         optional unsigned long byteOffset,
  //         optional unsigned long length)
  //
  if (IsArrayBuffer(args[0])) {
    if (num_args > 3) {
      return ThrowException(
          String::New(
              "Array constructor from ArrayBuffer must have 1-3 parameters."));
    }

    return CreateExternalArrayFromArrayBuffer(
        element_type,
        element_size,
        args[0]->ToObject(),
        num_args > 1 ? args[1] : Handle<Value>(),
        num_args > 2 ? args[2] : Handle<Value>());
  }

  // Otherwise, there should be exactly one argument.
  if (num_args != 1) {
    return ThrowException(String::New("Expected exactly one argument."));
  }

  // Is this the constructor with the following signature?
  //
  //     TypedArray(type[] array)
  //
  if (args[0]->IsArray()) {
    return CreateExternalArrayWithArrayArg<T>(
        element_type,
        Local<Array>::Cast(args[0]));
  }

  // Otherwise, this is the constructor with the following signature:
  //
  //     TypedArray(unsigned long length)
  //
  return CreateExternalArrayWithLengthArg(
      element_type,
      element_size,
      args[0]);
}

Handle<Value> ArrayBuffer(const Arguments& args) {
  return CreateArrayBuffer(args);
}

Handle<Value> Int8Array(const Arguments& args) {
  return CreateExternalArray<int8_t>(
      args,
      kExternalByteArray);
}

Handle<Value> Int16Array(const Arguments& args) {
  return CreateExternalArray<int16_t>(
      args,
      kExternalShortArray);
}

Handle<Value> Int32Array(const Arguments& args) {
  return CreateExternalArray<int32_t>(
      args,
      kExternalIntArray);
}

Handle<Value> Uint8Array(const Arguments& args) {
  return CreateExternalArray<uint8_t>(
      args,
      kExternalUnsignedByteArray);
}

Handle<Value> Uint16Array(const Arguments& args) {
  return CreateExternalArray<uint16_t>(
      args,
      kExternalUnsignedShortArray);
}

Handle<Value> Uint32Array(const Arguments& args) {
  return CreateExternalArray<uint32_t>(
      args,
      kExternalUnsignedIntArray);
}

Handle<Value> Float32Array(const Arguments& args) {
  return CreateExternalArray<float>(
      args,
      kExternalFloatArray);
}

Handle<Value> Float64Array(const Arguments& args) {
  return CreateExternalArray<double>(
      args,
      kExternalDoubleArray);
}

void ExportTypedArrays(
    const Handle<ObjectTemplate>& global_template) {
  global_template->Set(
      String::New("ArrayBuffer"),
      FunctionTemplate::New(ArrayBuffer));

  // Signed integers.
  global_template->Set(
      String::New("Int8Array"),
      FunctionTemplate::New(Int8Array));

  global_template->Set(
      String::New("Int16Array"),
      FunctionTemplate::New(Int16Array));

  global_template->Set(
      String::New("Int32Array"),
      FunctionTemplate::New(Int32Array));

  // Unigned integers.
  global_template->Set(
      String::New("Uint8Array"),
      FunctionTemplate::New(Uint8Array));

  global_template->Set(
      String::New("Uint16Array"),
      FunctionTemplate::New(Uint16Array));

  global_template->Set(
      String::New("Uint32Array"),
      FunctionTemplate::New(Uint32Array));

  // Floats.
  global_template->Set(
      String::New("Float32Array"),
      FunctionTemplate::New(Float32Array));

  global_template->Set(
      String::New("Float64Array"),
      FunctionTemplate::New(Float64Array));
}

}  // namespace gjstest