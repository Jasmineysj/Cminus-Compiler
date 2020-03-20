# Cmuins Compiler

This is a project in [Compiler Principle](http://210.45.114.30/gbxu/notice_board) at the University of Science and Technology of China. 
We have implemented a cminus compiler with Flex, Bison and LLVM. You can find the original repository at [Gitlab](http://210.45.114.30/PB17151774/compiler_cminus)

## Team
- Shujing Yang(Leader)
- [Zhehao Li](https://github.com/Ricahrd-Li)
- [Yuan Kuang](https://github.com/Kelleykuang)

## Environment

- Ubuntu 16.04
- LLVM: 8.0.1

## Usage

*You should first install LLVM* and configure LLVM, say

```shell
export PATH=the_path_to_your_llvm-install_bin:$PATH
source ~/.zshrc
```

You can easily run testcases with `autoTesh.sh`