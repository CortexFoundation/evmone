# config qemu environment

https://github.com/CortexFoundation/cvm-runtime/blob/riscv/README.md

# prepare
1. mkdir evm
### googletest
```
1. git clone https://github.com/CortexFoundation/googletest.git
2. cd googletest && git checkout riscv 
3. mkdir build && cd build
4. cmake .. && make 
5. 
```

### intx
```
1. git clone git@github.com:CortexFoundation/intx.git 
2. cd intx && git checkout riscv
3. mkdir build && cd build
4. cmake .. && make
```

### ethash
```
1. git clone git@github.com:CortexFoundation/ethash.git
2. cd ethash 
3. mkdir build && cd build
4. cmake .. && make
5. cp ${googletest}/build/lib/*.a ~/.hunter/_Base/18e57a4/53fea1f/2e6dac8/Install/lib/
6. make
```

### evmone
```
1. git clone git@github.com:CortexFoundation/evmone.git 
2. cd evmone/evmc && git checkout master 
3. cd .. && git checkout riscv
3. mkdir build && cd build
4. cmake .. && make 
5. cp ${googletest}/build/lib/*.a ~/.hunter/_Base/135567a/a355b0d/2b42b22/Install/lib/
6. make
7. cd ../mytest
8. make 
```

# runing on riscv simulator
1. reference: https://github.com/CortexFoundation/cvm-runtime/blob/riscv/README.md
2. cp test_evm_call ${riscv64-linux}/busybear-linux/etc/tests/
3. cd ${riscv64-linux}/busybear-linux/
4. make clean && make && ./scripts/start-qemu.sh
5. log in root, passward is busybear
6. cd /etc/tests && ./test_evm_call
7. get the output:
```
gase use = 1690, expect 1690
```
