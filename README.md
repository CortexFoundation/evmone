#config qemu environment

https://github.com/CortexFoundation/cvm-runtime/blob/riscv/README.md

#prepare
1. mkdir evm
2. git clone git@github.com:CortexFoundation/evmone.git 
3. git clone git@github.com:CortexFoundation/intx.git 
4. git clone git@github.com:CortexFoundation/ethash.git 

#FOR RISCV
1. cd intx && mkdir build && cd build && cmake .. && cmake --build . -- -j
2. cd ethash && mkdir build && cd build && cmake .. && cmake --build .
3. cd evmone && make build && cd build && cmake .. && make 
5. cd evmone && cd mytest && make

#runing on riscv simulator
1. reference: https://github.com/CortexFoundation/cvm-runtime/blob/riscv/README.md
