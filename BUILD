licenses(["notice"])

cc_library(
  name = "base",
  hdrs = glob([
    "base/*.h"
  ]),
  srcs = glob([
    "base/*.cc",
  ]),
  includes = [
    ".",
  ],
  defines = [
    "HASH_NAMESPACE=__gnu_cxx",
  ],
  deps = [
    "//third_party/glog",
  ],
)

cc_library(
  name = "file",
  hdrs = glob([
    "file/*.h"
  ]),
  srcs = glob([
    "file/*.cc",
  ]),
  includes = [
    ".",
  ],
  deps = [
    ":base",
  ],
)

cc_library(
  name = "strings",
  hdrs = glob([
    "strings/*.h"
  ]),
  srcs = glob([
    "strings/*.cc",
  ]),
  includes = [
    ".",
  ],
  deps = [
    ":base",
  ],
)

cc_library(
  name = "webutil",
  hdrs = glob([
    "webutil/xml/xml_writer.h"
  ]),
  srcs = glob([
    "webutil/xml/xml_writer.cc",
  ]),
  includes = [
    ".",
  ],
  deps = [
    ":base",
    "//third_party/libxml2",
  ],
)

cc_test(
  name = "xmlwrite_test",
  srcs = [
    "webutil/xml/xml_writer_test.cc",
  ],
  deps = [
    ":webutil",
    "//third_party/gtest:gtest_main",
  ],
)

cc_library(
  name = "util",
  hdrs = glob([
    "util/gtl/*.h"
  ]),
  includes = [
    ".",
  ],
)

JS_FILES = [
  "gjstest/internal/js/namespace.js",
  "gjstest/internal/js/run_test.js",
] + glob([
  "gjstest/public/**/*.js",
  "gjstest/internal/js/*.js",
], exclude=[
  "**/*test.js",
  "gjstest/internal/js/namespace.js"
])

genrule(
  name = "gen_js_deps",
  srcs = JS_FILES,
  outs = [
    "use_global_namespace.deps",
  ],
  cmd = "echo  " + " ".join(JS_FILES) + " > $@",
)

genrule(
  name = "generate_builtin_paths",
  tools = [
    "gjstest/internal/cpp/generate_builtin_paths.sh",
  ],
  srcs = [
    ":gen_js_deps",
  ],
  outs = [
    "gjstest/internal/cpp/builtin_paths.generated.h",
  ],
  cmd = "$(location gjstest/internal/cpp/generate_builtin_paths.sh) $(location :gen_js_deps) $@",
)

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "named_scripts",
  src = "gjstest/internal/proto/named_scripts.proto",
  generate_cc = True,
)

filegroup(
  name = "js_files",
  srcs = JS_FILES
)

cc_library(
  name = "gjstest_internal",
  hdrs = [
    "gjstest/internal/cpp/builtin_data.h",
    "gjstest/internal/cpp/run_tests.h",
    "gjstest/internal/cpp/typed_arrays.h",
    "gjstest/internal/cpp/test_case.h",
    "gjstest/internal/cpp/v8_utils.h",
    ":generate_builtin_paths",
  ],
  srcs = [
    "gjstest/internal/cpp/builtin_data.cc",
    "gjstest/internal/cpp/run_tests.cc",
    "gjstest/internal/cpp/test_case.cc",
    "gjstest/internal/cpp/typed_arrays.cc",
    "gjstest/internal/cpp/v8_utils.cc",
  ],
  copts = [
    "-DDEFAULT_DATA_DIR=third_party/gjstest/gjstest",
  ],
  deps = [
    ":named_scripts",
    ":base",
    ":file",
    ":strings",
    ":webutil",
    "//third_party/re2",
    "//third_party/v8",
    "//third_party/v8:v8_libplatform",
  ]
)

cc_binary(
  name = "gjstest",
  data = [
    ":js_files",
  ],
  srcs = [
    "gjstest/internal/cpp/gjstest.cc",
  ],
  deps = [
    ":gjstest_internal",
  ],
)
