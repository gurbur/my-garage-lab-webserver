<details>
<summary>🇰🇷 한국어</summary>

# My Garage Lab 웹 서버

[My Garage Lab SSG](https://github.com/gurbur/my-garage-lab-ssg) 프로젝트의 결과물을 배포하기 위해 C언어로 처음부터 직접 개발한 **고성능 비동기 멀티스레드 웹 서버**입니다. 이 프로젝트는 시스템 프로그래밍의 핵심 개념을 실제 프로젝트에 적용하며 깊이 있게 이해하는 것을 목표로 합니다.

## ✨ 주요 기능

  * **멀티스레드 아키텍처**: `Acceptor + Worker 스레드 풀` 모델을 채택하여 멀티코어 CPU 환경의 성능을 최대한 활용합니다.
      * Main 스레드는 연결 수락(`accept`)만 전담하고, 실제 I/O 처리는 워커 스레드들에게 분배하여 부하를 분산시킵니다.
  * **고성능 비동기 I/O**: Linux의 `epoll` API를 사용하여 소수의 스레드로 수많은 동시 연결을 효율적으로 처리하는 이벤트 기반(Event-Driven) 구조를 구현했습니다.
  * **정적 파일 서빙**: `ssg_output` 디렉토리의 HTML, CSS, JS, 이미지 등 정적 파일을 올바른 MIME 타입과 함께 클라이언트에 제공합니다.
      * `example.com/post-slug`와 같이 확장자가 생략된 URL을 `post-slug.html`로 자동 매핑하여 처리합니다.
  * **효율적인 연결 관리**:
      * `HTTP Keep-Alive`를 지원하여 TCP 연결을 재사용함으로써 성능을 향상시킵니다.
      * **타이머 휠(Timer Wheel)** 자료구조를 구현하여, 오랫동안 아무 요청이 없는 유휴(idle) 연결을 O(1) 시간 복잡도로 효율적으로 찾아내고 자동으로 종료합니다.
  * **유연한 설정**: `server.conf` 파일을 통해 포트, 워커 스레드 수, 문서 루트 경로 등 서버의 주요 동작을 코드 수정 없이 변경할 수 있습니다.
  * **로깅**: 모든 클라이언트의 요청과 서버의 주요 이벤트를 `server.log` 파일에 기록하여 디버깅 및 분석에 활용할 수 있습니다.

## 🚀 시작하기

### 요구 사항

  * `gcc` 컴파일러
  * `make` 빌드 도구
  * `pthreads` 라이브러리
  * Linux 환경 (`epoll` API 사용)

### 🛠️ 빌드 방법

프로젝트를 컴파일하고 실행 파일을 생성하려면 `make` 명령어를 사용하세요.

1.  **프로젝트 빌드**

    ```bash
    make
    ```

    위 명령어를 실행하면 프로젝트 루트 디렉토리에 `server` 실행 파일이 생성됩니다.

2.  **빌드 결과물 삭제**

    ```bash
    make clean
    ```

    이 명령어는 `server` 실행 파일과 빌드 과정에서 생성된 모든 오브젝트 파일(`obj/` 디렉토리)을 삭제합니다.

### 🏃 사용법

1.  **설정 파일 준비**: 프로젝트 루트에 `server.conf` 파일을 생성하고 아래 예시와 같이 내용을 작성합니다.

2.  **문서 루트 생성**: `server.conf`에 지정된 `document_root` (기본값: `ssg_output`) 디렉토리를 생성하고, 그 안에 `index.html` 등 웹 콘텐츠를 위치시킵니다.

3.  **서버 실행**: 터미널에서 아래 명령어를 실행합니다.

    ```bash
    ./server
    ```

    서버가 정상적으로 실행되면, 웹 브라우저에서 `http://localhost:[포트번호]`로 접속하여 확인할 수 있습니다.

### ⚙️ 설정 (`server.conf`)

`key = value` 형식의 텍스트 파일입니다.

**예시 `server.conf`:**

```ini
# 웹 서버 설정 파일

# 서버가 리스닝할 포트 번호
port = 8080

# 생성할 워커 스레드의 개수 (CPU 코어 수 권장)
num_workers = 4

# 정적 파일을 제공할 루트 디렉토리
document_root = ./ssg_output

# 로그 파일 경로
log_file = server.log
```

</details>

# My Garage Lab Web Server

A **high-performance, asynchronous, multi-threaded web server** built from scratch in C, designed to serve the output of the [My Garage Lab SSG](https://github.com/gurbur/my-garage-lab-ssg) project. This project aims to provide a deep understanding of core systems programming concepts by applying them to a real-world project.

## ✨ Key Features

  * **Multi-Threaded Architecture**: Utilizes an `Acceptor + Worker Thread Pool` model to maximize performance on multi-core CPU environments.
      * The Main thread is dedicated to accepting new connections, while I/O processing is distributed among a pool of worker threads.
  * **High-Performance Asynchronous I/O**: Implements an event-driven model using Linux's `epoll` API, allowing a small number of threads to efficiently handle thousands of concurrent connections.
  * **Static File Serving**: Serves static files such as HTML, CSS, JS, and images from the `ssg_output` directory with correct MIME types.
      * Supports clean URLs by automatically mapping requests like `example.com/post-slug` to the `post-slug.html` file.
  * **Efficient Connection Management**:
      * Supports `HTTP Keep-Alive` to enhance performance by reusing TCP connections.
      * Implements a **Timer Wheel** data structure to efficiently manage and automatically close idle connections with O(1) time complexity.
  * **Flexible Configuration**: Server behavior, such as port, number of worker threads, and document root, can be easily modified via a `server.conf` file without changing the code.
  * **Logging**: Logs all client requests and major server events to `server.log` for debugging and analysis.

## 🚀 Getting Started

### Prerequisites

  * `gcc` compiler
  * `make` build tool
  * `pthreads` library
  * A Linux-based environment (due to the use of the `epoll` API)

### 🛠️ How to Build

Use the `make` command to compile the project and create an executable.

1.  **Build the project**

    ```bash
    make
    ```

    This command will create a `server` executable in the project's root directory.

2.  **Clean build artifacts**

    ```bash
    make clean
    ```

    This command removes the `server` executable and all intermediate object files (the `obj/` directory).

### 🏃 Usage

1.  **Prepare Configuration**: Create a `server.conf` file in the project root. See the example below.

2.  **Create Document Root**: Create the directory specified by `document_root` in your config (default: `ssg_output`) and place your web content (e.g., `index.html`) inside it.

3.  **Run the Server**: Execute the following command in your terminal.

    ```bash
    ./server
    ```

    Once the server is running, you can access it via a web browser at `http://localhost:[port]`.

### ⚙️ Configuration (`server.conf`)

A simple `key = value` text file.

**Example `server.conf`:**

```ini
# Web Server Configuration File

# Port for the server to listen on
port = 8080

# Number of worker threads to create (CPU core count is recommended)
num_workers = 4

# The root directory for serving static files
document_root = ./ssg_output

# Path to the log file
log_file = server.log
```
