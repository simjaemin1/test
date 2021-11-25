### OSFALL2021 TEAM10 PROJ3


## 0. How to build kernel

다음과 같은 directory 상황에서 tizen-5.0-rpi3 폴더를 kernel path라고 가정한다.
또 정상적으로 qemu가 돌아가는 상황이라고 가정한다.
```bash
tizen-kernel
├── tizen-5.0-rpi3
├── tizen-image
└── mnt_dir
```
```
cd tizen-kernel/tizen-5.0-rpi3
git pull origin proj3
sudo sh coompile.sh
sudo sh ./qemu.sh
```

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
로 두 함수를 정의해주었다.

### 1.2 include/linux/rotation.h
rotation.h에는 rotlock_t을 정의하고 kernel/rotation.c에서 정의할  함수에 대해 선언했다.

* rotlock_t
rotlock_t는 struct로 rotation lock에 필요한 변수들을 가지고 있다.  
가장 필수적인 것으로 pid, degree, range, condition variable 그리고 waiting/acquired list에 대한 struct list_head가 들어있다.   

### 1.3 kernel/rotation.c
#### 1.3.1 static variable
kernel/rotation.c에는 여러 필요에 의한 static variable을 정의하였다.
1. int rotation  
rotation lock에서 0부터 359까지 rotation하며 lock의 wait/acquire를 결정하는 변수이다.

2. int write_waiting_cnt[360]  
write의 starvation을 방지하기 위해 write lock중 waiting하는 lock이 어떤 degree range를 occupy하는지 그 수를 0~359에 대해 기록한다. read와 write에 대해 acquired list가 동시에 empty가 되는 경우(lock을 잡은 thread가 없는 경우), read lock은 해당 rotation에 대해 write_waiting_cnt[rotation]이 0보다 큰 경우 그 각도에 대해 write lock이 waiting하고 있다는 의미이므로 lock을 잡을 수 없다. 또 reader가 lock을 잡고 있는 경우에도 새로운 reader가 lock을 잡으려 시도할 때에도 이 array를 참조하여 waiting할지, acquire할지 결정한다.

3. struct list_head  
rotation lock implementation에는 4개의 list가 필요하다. waiting하는 R/W에 대한 list, 이미 lock을 잡은 reader 또는 writer에 대한 list이다. R/W는 둘 중 하나만 lock을 잡을 수 있으므로 두 list 중 하나는 반드시 비어있어야 한다. read_acquired list는 다수의 reader가 있을 수 있으나 writer는 lock을 잡고 있다면 반드시 list의 head를 제외한 list_head node가 1개만 존재하여야 한다.

4. mutex  
list나 array, rotation 등에 대한 접근을 exclusive하고 atomic하게 진행하기 위해 mutex의 이용이 필수적이다. better concurrency와 throughput을  위해 다수의 mutex를 사용할 수도 있겠다고 생각했으나, 각 function이나 thread들이 mutex를 필요로 하는 범위가 애매하여 coding & debugging의 단순함을 위해 single mutex를 사용하였다.

5. wait_queue
1개 wait_queue가 필요하다. waiting하고 있는 process의 정보를 가지고 있는 waiting_entry를 wait_queue에 넣고 process를 wakeup하면 finish_wait를 호출하여 wait_queue에 있는 entry를 삭제한다.

#### 1.3.2 int get_lock()
get_lock() 함수는 두 R/W acquired list 모두 비어있는 경우에만 call된다.  
1. mutex_lock을 먼저 잡는다.
2. write starvation 방지를 위해 rotation에 맞는 write_lock에게 우선권을 주기 위해 먼저 write_waiting list를 돌며 node를 찾는다. 1개를 찾았다면 write_waiting_cnt에서 해당 range에서 cnt를 1만큼 내려준 후 wait중인 thread를 깨운다. 이후 loop을 break한다. list를 옮겨주는 작업은 write_lock에서 한다.
3. range 내에 rotation을 둔 write lock이 없다면 read_waiting list를 traverse하며 조건에 맞는 모든 reader를 찾아 해당 thread를 깨운다. 한번에 multiple reading이 가능하므로 iteration을 loop 끝까지 전부 실행하며, 이를 위해 list_for_each_entry_safe를 이용한다.
4. mutex unlock을 하고 loop iteration을 하며 깨운 쓰레드의 수를 cnt 한 후 이를 return한다.

#### 1.3.3 int check_range(int rotation, int degree, int range)
degree와 range에 대해 rotation이 포함되는지 체크하는 function이다. circular하게 정의되어있기 때문에 range의 low와 high가 0~359의 범위를 넘어간 경우 360에 대해 보정을 해준 후 계산한다.

#### 1.3.4 void modify_waiting_cnt(int degree, int range, int type)
write_waiting list에 node가 추가되거나 빠질 경우 해당 degree, range에 대해 write_waiting_cnt에서 그 node에 대한 count를 increment하거나 decrement한다. 두 경우 모두에 대해 동작할 수 있도록 type이라는 argument를 추가하여 implement 하였다.

#### 1.3.5 void exit_rotlock(task_struct *p)
process가 종료되면서 do_exit()이라는 함수가 call 되는데, 이때 이 process에서 waiting하거나 acquire한 lock을 해제해주기 위한 함수이다. pid를 통해 4개의 list를 모두 traverse 하면서 해제한다.
acquired lock을 해제한 후 해당 list가 빈 경우 get_lock()으로 새로운 lock을 잡도록 한다.

#### 1.3.6 rotlock_t* init_rotlock(int degree, int range, int rw_type)
받은 argument를 통해 rotlock_t를 초기화해주는 간단한 함수이다.

#### 1.3.7 find_node_and_del(int degree, int range, struct list_head* head)
unlock 함수가 call된 경우에 해당 degree, range, list (read 또는 write)에 대한 acquired lock을 찾아 해제한다. 단 다른 process의 것을 해제하는 것을 방지하기 위해 pid를 체크한다. 같은 pid에 대해 여러 lock을 가지고 있다 하더라도 1개의 lock만 해제한다.

====> 수정 + 빈 경우에 get_lock하는것(unlock에서 알아서 부른다) 다른 함수에서도 (exit_rotlock등도 kfree 필요한듯?)
#### 1.3.8 wait(rotlock_t *curr)
wait_entry를 만들고 이를 wait_queue 에 넣는다. 그리고 prepare_to_wait을 호출하여 현재의 process의 상태를 TASK_INTERRUPTIBLE로 바꾼다. 이후 schedule()을 호출하여 현재 process를 runqueue에서 제거한다.
#### 1.3.9 wakeup(rotlock_t *curr)
condition value를 1로 만들고 wake_up_procdss를 호출하여 프로세스를 깨운다.

#### 1.3.9 long set_rotation(int degree)
rotation degree의 값을 update하고 get_lock을 호출하여 바뀐 degree를 포함하며 조건이 맞는 lock을 가지고 있던 쓰레드를 깨운다.
#### 1.3.10 long rotlock_read(int degree, int range)
1. degree 값이 0부터 360사이의 값(360 미포함)인지 확인하고 range값이 0부터 180사이의 값인지 확인한다. 
2. rotlock을 만들고 현재 degree가 해당 lock의 range에 있는지 확인하고 degree를 포함하는 range를 가진 write_lock이 기다리고 있는지 확인한다. write_acquired가 비었는지 확인하고 write_lock이 기다리고 있지 않으며 wrtie_acquired가 비었다면 lock을 hold하며 read_acquired에 해당 entry를 추가한다.
3. 만약 조건이 하나라도 만족되지 않았다면 read_waiting에 해당 entry를 추가하고 wait를 호출하여 현재 쓰레드를 block시킨다. 쓰레드가 wake_up된 후에 lock 잡기에 앞서서 다른곳에서 조건이 바뀌어 다시 wait해야 될 경우를 대비하여 while문 안에서 wait함수가 실행되도록 한다. 
4. 쓰레드가 wake_up되었다면 while문을 탈출하고 해당 rotlock entry를 waiting list에서 제거하고 acquired list에 넣는다.

#### 1.3.11 long rotlock_write(int degree, int range)
1. degree 값이 0부터 360사이의 값(360 미포함)인지 확인하고 range값이 0부터 180사이의 값인지 확인한다. 
2. rotlock을 만들고 현재 degree가 해당 lock의 range에 있는지 확인하고 read_acquired, write_acquired가 비었는지 확인하고 w둘다 비었다면  lock을 hold하며 write_acquired에 해당 entry를 추가한다.
3. 만약 조건이 하나라도 만족되지 않았다면 write_waiting에 해당 entry를 추가하고 해당하는 범위의 write_waiting_cnt의 값을 증가시킨 후 wait를 호출하여 현재 쓰레드를 block시킨다. 쓰레드가 wake_up된 후에 lock 잡기에 앞서서 다른곳에서 조건이 바뀌어 다시 wait해야 될 경우를 대비하여 while문 안에서 wait함수가 실행되도록 한다.
4. 쓰레드가 wake_up 되었다면 while문을 탈출하고, 해당 범위의 write_waiting_cnt의 값을 감소시킨 후 해당 entry를 waiting list에서 제거하고 acquired list에 넣는다.

#### 1.3.12 long rotunlock_read(int degree, int range)
1. degree와 range에 대한 값이 올바른지 체크한다.
2. mutex_lock을 잡는다.
3. find_node_and_del 함수를 통해 조건에 맞는 lock을 read_acquired list에서 찾는다.
4. 없는 경우 에러 메세지를 출력하고 리턴한다.
5. 제거한 후에 read_acquired list가 empty인 경우 get_lock() 함수를 call 한다.
6. mutex를 unlock하고 success를 return 한다.

#### 1.3.12 long rotunlock_write(int degree, int range)
1. degree와 range에 대한 값이 올바른지 체크한다.
2. mutex_lock을 잡는다.
3. find_node_and_del 함수를 통해 조건에 맞는 lock을 read_acquired list에서 찾는다.
4. 없는 경우 에러 메세지를 출력하고 리턴한다.
5. 제거하였다면 write_acquired list가 empty라는 것이므로 get_lock() 함수를 call 한다.
6. mutex를 unlock하고 success를 return 한다.

## 2. Evaluation
####2.1 rotd
rotation degree의 값을 0부터 시작해서 2초마다 30씩 증가시키며 330이후는 다시 0으로 만든다.

####2.2 selector
integer의 값을 argument로 받고 이를 while문을 돌면서 1씩 증가시키고 integer파일에 쓴다. integer에 정상적으로 값을 작성하였다면 해당 값을 콘솔에 출력한다. test를 할 때에는 while문을 1초에 1번씩 돌도록 하였다.

####2.3 trial
identifier의 값을 argument로 받는다. while문을 돌면서 integer에 있는 숫자를 읽고 해당 숫자로 prime-factorization을 진행한다. 정상적으로 값을 읽고 prime=factorization을 진행하였다면 해당 결과를 콘솔에 출력한다. test를 할 때에는 while문을 1초에 1번씩 돌도록 하였다.

## 3. Lesson learned
* 수업때 배운 일반적인 lock과 다른 독특한 lock implementation 방식과, 그에 따른 write starvation 해법을 배우고 직접 kernel에 implement해보면서 많은 것을 배울 수 있었다.
* 프로세스를 재우고 깨우는 구체적인 방법을 배울 수 있었다. 커널이 이를 어떠한 방식으로 수행하는지 알 수 있었다. wait_queue에 대해 알게 되었고, task의 state에 어떤 종류가 있고 이것이 나중에 어떤 역할을 하는지 알 수 있었고, scehdule()의 역할 중 일부를 알게 되었다. Linux Kenel Development 문서를 읽으면서 이를 알게 되었는데 커널 코드를 직접 하나하나 들여다보는 것 보다 훨씬 편했던 것 같다.
