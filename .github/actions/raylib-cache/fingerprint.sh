#!/bin/sh
# Prints "id=<16 hex digits>": a fingerprint of everything outside the
# repository that shapes a compiled raylib tree. The raylib-cache action puts
# it in the cache key, so a tree is only reused by a job that would have
# built the same one.
#
#   ImageOS            the runner's operating system release (ubuntu24,
#                      macos15, win25): a new release brings new system
#                      headers and libraries.
#   GITHUB_WORKSPACE   CMake writes absolute paths into its build tree, so a
#                      tree only works at the path it was configured in.
#   compiler, CMake    their full --version text.
#   raylib-options     the Makefile's raylib option definitions.
#
# The runner image's own version (ImageVersion) is deliberately left out. It
# changes about every week, and each change threw away every cache although
# the compiler and CMake, which are what matter and are listed above, had
# usually stayed the same.
#
# MANGO_FINGERPRINT_CC names the compiler. Each command runs on its own line
# so that a missing tool stops the script (set -e) instead of hashing an
# empty answer.
set -eu

compiler=$("$MANGO_FINGERPRINT_CC" --version)
cmake_version=$(cmake --version)
options=$(make -s raylib-options)

# macOS has shasum; Linux and MSYS2 have sha256sum.
if command -v sha256sum >/dev/null 2>&1; then
    digest() { sha256sum; }
else
    digest() { shasum -a 256; }
fi

id=$(printf '%s\n' "${ImageOS:-unknown} ${GITHUB_WORKSPACE:-}" "$compiler" "$cmake_version" "$options" \
    | digest | cut -c1-16)
echo "id=$id"
