# TIC80 C Language Template

## Difference comparing to official template

This template **doesn't** require `wasi-sdk`.
All your need is `clang`(C language family frontend for LLVM),
`lld`(Linker from the LLVM project) 
and `tic80` packages.

The advantage of not using `wasi-sdk` is:

* Easy to compile, the wasm part can be compiled within `Termux for Android`.
* Won't invoke `WASI` api by mistake, which is **not** provided by `tic80`.
* No extra dependencies. Have a minimal `libc` (modified from wasi-sdk source).

## Useage

* Clone this template.
* Modify `Makefile`, change `TARGET_NAME` to project name.
* Compile only the wasm part: `make clean && make wasm`
* Compile and make the cart: `make clean && make cart`
* Compile and run the cart: `make clean && make run`

## Limitation

printf function have several limitation:

* Not support float and double (%f and %lf).
* Max text length is 80 ascii characters.

math library only have a subset of math functions.

## Recommanded VSCode extensions

* ms-vscode.cpptools
* ms-vscode.makefile-tools
