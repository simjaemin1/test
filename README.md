### OSFALL2021 TEAM10 PROJ3


## 0. How to build kernel

## 1. High Level Implementation
### 1.3 Systemcall : long set_rotation(int degree);
* include/linux/syscalls.h
    가장 아래에 다음을 추가한다.  
    `asmlinkage long sys_set_rotation(int degree);`  
* arch/arm/tools/syscalls.tbl
    가장 아래에 다음을 추가한다.  
    `398 common set_rotation sys_set_rotation`  
* arch/arm64/include/asm/unistd.h
    `#define __NR_compat_syscalls   399`  
    로 수정한다.  
* arch/arm64/include/asm/unistd32.h
    아래 두 줄을 추가한다.
    `#define __NR_set_rotation 398`  
    `__SYSCALL(__NR_set_rotation, sys_set_rotation)`  

이후 kernel/rotation.c에  
`SYSCALL_DEFINE1(set_rotation, int, degree)`  
로 두 함수를 정의해주었다. (아직 안함)

## 2.

## 3. Lesson learned
