#!/usr/bin/env bash

# check if SGX1/SGX2 are supported by the available CPU
# Source: https://github.com/apache/teaclave-java-tee-sdk/tree/master?tab=readme-ov-file#1-is-sgx2-supported
cpuid -1 -l 0x12
