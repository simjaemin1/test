### OSFALL2021 TEAM10 PROJ3


## 0. How to build kernel

## 1. High Level Implementation
### 1.1 Systemcall implementation
* include/linux/syscalls.h
    가장 아래에 다음을 추가한다. 
    ```
    asmlinkage long sys_set_rotation(int degree);
    asmlinkage long sys_rotlock_read(int degree, int range);
    asmlinkage long sys_rotlock_write(int degree, int range);
    asmlinkage long sys_rotunlock_read(int degree, int range);
    asmlinkage long sys_rotunlock_write(int degree, int range);
    ```
* arch/arm/tools/syscalls.tbl
    가장 아래에 다음을 추가한다.
    ```
    398 common set_rotation sys_set_rotation
    399 common rotlock_read sys_rotlock_read
    400 common rotlock_write sys_rotlock_write
    401 common rotunlock_read sys_rotunlock_read
    402 common rotunlock_write sys_rotunlock_write
    ```
* arch/arm64/include/asm/unistd.h
    `#define __NR_compat_syscalls   403`
    로 수정한다.  
* arch/arm64/include/asm/unistd32.h
    아래 두 줄을 추가한다.  
    ```
    #define __NR_set_rotation 398
    __SYSCALL(__NR_set_rotation, sys_set_rotation)
    #define __NR_rotlock_read 399
    __SYSCALL(__NR_rotlock_read, sys_rotlock_read)
    #define __NR_rotlock_write 400
    __SYSCALL(__NR_rotlock_write, sys_rotlock_write)
    #define __NR_rotunlock_read 401
    __SYSCALL(__NR_rotunlock_read, sys_rotunlock_read)
    #define __NR_rotunlock_write 402
    __SYSCALL(__NR_rotunlock_write, sys_rotunlock_write)
    ```
이후 kernel/rotation.c에  
```
SYSCALL_DEFINE1(set_rotation, int, degree)
SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
```
로 두 함수를 정의해주었다. (아직안함)

## 2.

## 3. Lesson learned

