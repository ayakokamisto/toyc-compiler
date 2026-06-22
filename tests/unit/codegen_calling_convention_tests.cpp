#include "codegen/abi/CallingConvention.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen calling convention test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void testStackArgBytesFor() {
    require(toyc::codegen::CallingConvention::stackArgBytesFor(0) == 0,
            "zero arguments need no stack area");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(8) == 0,
            "eight register arguments need no stack area");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(9) == 16,
            "ninth argument requires 16-byte aligned stack area");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(10) == 16,
            "tenth argument stays within first aligned stack chunk");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(11) == 16,
            "eleventh argument stays within first aligned stack chunk");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(12) == 16,
            "twelfth argument stays within first aligned stack chunk");
    require(toyc::codegen::CallingConvention::stackArgBytesFor(13) == 32,
            "thirteenth argument requires next aligned stack chunk");
}

} // namespace

int main() {
    try {
        testStackArgBytesFor();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
