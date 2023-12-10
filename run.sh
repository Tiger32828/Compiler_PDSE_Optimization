#!/bin/bash
# Run script for the final project of EECS 583 Fall 2023
# Place this script in the benchmarks folder and run it using the name of the file (without the file type)
# e.g. sh run.sh correct1

# ACTION NEEDED: If the path is different, please update it here.
PATH2LIB="../../build/pdsepass/PDSEPass.so"        # Specify your build directory in the project

# ACTION NEEDED: Choose the correct pass when running.
PASS=pdse-correctness                   # Choose either -pdse-correctness ...
# PASS=pdse-performance                 # ... or -pdse-performance

# Delete outputs from previous runs. Update this when you want to retain some files.
rm -f default.profraw *_prof *_pdse *.bc *.profdata *_output *.ll

# Convert source code to bitcode (IR).
clang -emit-llvm -c ${1}.c -Xclang -disable-O0-optnone -o ${1}.bc

# Canonicalize natural loops (Ref: llvm.org/doxygen/LoopSimplify_8h_source.html)
opt -passes='loop-simplify' ${1}.bc -o ${1}.ls.bc

# Instrument profiler passes.
opt -passes='pgo-instr-gen,instrprof' ${1}.ls.bc -o ${1}.ls.prof.bc

# Note: We are using the New Pass Manager for these passes! 

# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.ls.prof.bc -o ${1}_prof

# When we run the profiler embedded executable, it generates a default.profraw file that contains the profile data.
./${1}_prof > correct_output

# Converting it to LLVM form. This step can also be used to combine multiple profraw files,
# in case you want to include different profile runs together.
llvm-profdata merge -o ${1}.profdata default.profraw

# The "Profile Guided Optimization Use" pass attaches the profile data to the bc file.
opt -passes="pgo-instr-use" -o ${1}.profdata.bc -pgo-test-profile-file=${1}.profdata < ${1}.ls.prof.bc > /dev/null

# We now use the profile augmented bc file as input to your pass.
opt -load-pass-plugin="${PATH2LIB}" -passes="${PASS}" ${1}.profdata.bc -o ${1}.pdse.bc > /dev/null

# Generate binary excutable before PDSE: Unoptimzed code
clang ${1}.ls.bc -o ${1}_no_pdse
# Generate binary executable after PDSE: Optimized code
clang ${1}.pdse.bc -o ${1}_pdse

# Produce output from binary to check correctness
./${1}_pdse > pdse_output

echo -e "\n=== Program Correctness Validation ==="
if [ "$(diff correct_output pdse_output)" != "" ]; then
    echo -e ">> Outputs do not match\n"
else
    echo -e ">> Outputs match\n"
    # # Measure performance
    # echo -e "1. Performance of unoptimized code"
    # time ./${1}_no_pdse > /dev/null
    # echo -e "\n\n"
    # echo -e "2. Performance of optimized code"
    # time ./${1}_pdse > /dev/null
    # echo -e "\n\n"
fi

# Cleanup: Remove this if you want to retain the created files. And you do need to.
# rm -f default.profraw *_prof *_pdse *.bc *.profdata *_output *.ll
