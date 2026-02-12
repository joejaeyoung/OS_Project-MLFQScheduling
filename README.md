<div align="center">

# ⚙️ XV6 MLFQ Scheduler

**XV6 운영체제 커널의 Round Robin 스케줄러를 Multi-Level Feedback Queue(MLFQ) 알고리즘으로 교체**

[![Hits](https://hits.sh/github.com/joejaeyoung/OS_Project-MLFQScheduling.svg)](https://github.com/joejaeyoung/OS_Project-MLFQScheduling)

</div>

---

## 📋 프로젝트 정보

|    항목     | 내용                  |
| :-------: | :------------------ |
|  **분야**   | OS                  |
| **개발 기간** | 2025.07             |
| **개발 환경** | Ubuntu Linux + QEMU |

---

## 📖 프로젝트 소개

이 프로젝트는 MIT에서 개발한 교육용 운영체제 **XV6**의 기본 **Round Robin 스케줄러**를 **Multi-Level Feedback Queue(MLFQ) 알고리즘**으로 완전히 교체한 프로젝트입니다.

MLFQ는 **4단계 우선순위 큐**를 사용하여 프로세스의 행동 패턴에 따라 동적으로 우선순위를 조정하는 스케줄링 알고리즘입니다. **I/O 집약적 프로세스**에게 높은 우선순위를 부여하여 응답성을 확보하고, **CPU 집약적 프로세스**는 점진적으로 낮은 큐로 이동시켜 공정한 CPU 분배를 실현합니다.

**이중 연결 리스트 기반 우선순위 큐**를 활용하여 O(1)으로 다음 실행 프로세스를 선택하며, **Aging 메커니즘**을 통해 기아(starvation) 현상을 방지합니다.

---

## 🚀 시작 가이드

### Requirements

- GCC (i686-linux-gnu cross compiler)
- GNU Make
- QEMU (qemu-system-i386)

### Installation & Run

```bash
# 1. MLFQ Scheduler 소스 클론 및 덮어쓰기
$ git clone https://github.com/jojaeyoung/OS_Project-MLFQScheduling.git
$ cp OS_Project-MLFQScheduling/srcs/* .

# 2. 빌드 및 실행
$ make qemu
```


### 디버그 모드 실행

```bash
# 기본 디버그 출력
$ make qemu debug=1
```

### 테스트 실행 (XV6 쉘 내부)

```bash
$ scheduler_test
```

---

## 🛠️ Stacks

### Environment

![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![QEMU](https://img.shields.io/badge/QEMU-FF6600?style=for-the-badge&logo=qemu&logoColor=white)

### Development

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![XV6](https://img.shields.io/badge/XV6-000000?style=for-the-badge&logo=mit&logoColor=white)
![GCC](https://img.shields.io/badge/GCC-333333?style=for-the-badge&logo=gnu&logoColor=white)

### Config

![Makefile](https://img.shields.io/badge/Makefile-064F8C?style=for-the-badge&logo=gnu&logoColor=white)

---

## 📺 시스템 콜 인터페이스

### `set_proc_info(int q_level, int cpu_burst, int cpu_wait_time, int io_wait_time, int end_time)`

| 항목 | 설명 |
|:---|:---|
| **시스템 콜 번호** | 23 |
| **첫 번째 인자** | `q_level` — 프로세스의 초기 큐 레벨 (0~3) |
| **두 번째 인자** | `cpu_burst` — 현재 타임 퀀텀 내 CPU 사용량 |
| **세 번째 인자** | `cpu_wait_time` — 큐에서 대기한 시간 |
| **네 번째 인자** | `io_wait_time` — I/O 대기 시간 |
| **다섯 번째 인자** | `end_time` — 프로세스 총 CPU 할당 한도 (틱 수) |
| **반환값** | 성공 시 `0`, 실패 시 `-1` |

#### 사용 예시

```c
// 큐 레벨 2, CPU 할당 한도 300틱
set_proc_info(2, 0, 0, 0, 300);
```

---

## ⭐ 주요 기능

### 1. 4단계 MLFQ 스케줄링

- **Queue 0** (최고 우선순위): 타임 퀀텀 **10 ticks**
- **Queue 1**: 타임 퀀텀 **20 ticks**
- **Queue 2**: 타임 퀀텀 **40 ticks**
- **Queue 3** (최저 우선순위): 타임 퀀텀 **80 ticks**
- 큐 레벨이 높을수록 타임 퀀텀이 길어져 **CPU 집약적 프로세스에 적합**

### 2. 동적 우선순위 조정

- **Demotion (강등)**: 타임 퀀텀을 소진하면 한 단계 낮은 큐로 이동
- **Promotion (승급)**: Aging 메커니즘에 의해 한 단계 높은 큐로 이동
- **I/O 우선 정책**: 같은 큐 내에서 `io_wait_time`이 큰 프로세스 우선 선택

### 3. Aging 기반 기아 방지

- RUNNABLE 상태에서 **250 ticks 이상 대기**한 프로세스를 상위 큐로 승급
- 매 타이머 인터럽트마다 Queue 1~3의 모든 프로세스를 검사
- CPU를 오래 기다린 프로세스가 영원히 실행되지 않는 문제를 방지

### 4. 우선순위 큐 (이중 연결 리스트)

- 큐 내에서 `io_wait_time` 기준 내림차순 정렬 (I/O 집약적 프로세스 우선)
- 동일 시 `queue_entry_time` 기준 오름차순 (FIFO)
- **O(1)** 으로 큐의 head에서 다음 실행 프로세스 선택
- `add_to_queue()` / `remove_from_queue()` 로 동적 관리

### 5. 프로세스 수명 관리

- `end_time` 값이 양수이고 `total_cpu_time >= end_time`인 경우 강제 종료
- CPU 할당 한도를 통해 프로세스 실행 시간 제어 가능

### 6. 특수 프로세스 처리

- **INIT / SHELL 프로세스**: 항상 Queue 3에 고정, 승급/강등 대상에서 제외
- **사용자 프로세스**: Queue 0에서 시작, 행동에 따라 동적으로 큐 이동

---

## 🏗️ 아키텍쳐

### 스케줄러 동작 흐름

![[OS_Project-MLFQScheduling/docs/images/image1.png]]
### 타이머 인터럽트 처리 흐름

![[OS_Project-MLFQScheduling/docs/images/image2.png|500]]

### 핵심 상수 정의

| 상수 | 값 | 설명 |
|:---|:---:|:---|
| `MLFQ 큐 수` | 4 | Queue 0 ~ Queue 3 |
| `Queue 0 타임 퀀텀` | 10 ticks | 최고 우선순위, 짧은 실행 |
| `Queue 1 타임 퀀텀` | 20 ticks | |
| `Queue 2 타임 퀀텀` | 40 ticks | |
| `Queue 3 타임 퀀텀` | 80 ticks | 최저 우선순위, 긴 실행 |
| `Aging 임계값` | 250 ticks | 대기 시간 초과 시 승급 |

### 프로세스 구조체 확장 (proc.h)

| 필드 | 타입 | 기본값 | 설명 |
|:---|:---:|:---:|:---|
| `q_level` | int | 0 | 현재 큐 레벨 (0~3) |
| `cpu_burst` | int | 0 | 현재 타임 퀀텀 내 CPU 사용량 |
| `cpu_wait` | int | 0 | RUNNABLE 상태 대기 시간 |
| `io_wait_time` | int | 0 | SLEEPING 상태 I/O 대기 시간 |
| `end_time` | int | -1 | 프로세스 총 CPU 할당 한도 |
| `total_cpu_time` | int | 0 | 누적 CPU 사용 시간 |
| `queue_entry_time` | uint | 0 | 현재 큐 진입 시각 |
| `next` | proc* | 0 | 큐의 다음 노드 포인터 |
| `prev` | proc* | 0 | 큐의 이전 노드 포인터 |
| `type` | proctype | IDLE | 프로세스 유형 (IDLE/INIT/SHELL) |

### 디렉토리 구조

```
OS_Project-MLFQScheduling/
├── README.md
└── srcs/
    ├── Makefile            # 빌드 설정 (테스트 바이너리 등록)
    ├── proc.h              # 프로세스 구조체 확장 + 큐 구조체 정의
    ├── proc.c              # 스케줄러 핵심 로직 (MLFQ, 큐 관리, Aging, Demotion)
    ├── trap.c              # 타이머 인터럽트 처리 (타이머 갱신, 퀀텀 체크, Aging 호출)
    ├── syscall.h           # 시스템 콜 번호 정의 (SYS_set_proc_info = 23)
    ├── syscall.c           # 시스템 콜 디스패치 테이블 등록
    ├── user.h              # 유저 공간 함수 프로토타입
    ├── usys.S              # 시스템 콜 어셈블리 스텁
    ├── scheduler_test.c    # MLFQ 스케줄러 테스트 프로그램
    └── [xv6 core files]    # 메모리 관리, 파일 시스템 등
```

### 수정 파일 역할 관계

| 파일 | 역할 | 핵심 변경 사항 |
|:---|:---|:---|
| `proc.h` | 데이터 구조 | 구조체 확장 + 큐 구조체 + proctype 열거형 |
| `proc.c` | 스케줄러 핵심 | init_queues, add/remove_queue, get_next_proc, demote/promote, check_aging |
| `trap.c` | 인터럽트 처리 | 타이머 갱신, 퀀텀 체크, 종료 조건, Aging 호출 |
| `syscall.h/c` | 시스템 콜 등록 | set_proc_info 번호/디스패치 등록 |
| `user.h` + `usys.S` | 유저 인터페이스 | 유저 공간에서 호출 가능하도록 등록 |
| `Makefile` | 빌드 설정 | 테스트 바이너리 UPROGS 등록 |
