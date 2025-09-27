# 개요

도서 'TCP/IP 소켓 프로그래밍'의 내용에 더해, `구조적 리팩토링 (모놀리식->함수화->객체지향)` + `기능 확장` 진행

# ✅ 전체 체크리스트

## 🔹 [A] 네트워크 기능 구현 (기본 완료됨)

- [x] TCP/UDP 멀티 프로토콜 서버 구현
- [x] IPv4/IPv6 듀얼 스택 처리
- [x] `select()` 기반 I/O 다중화
- [x] 넌블로킹 소켓 설정 (UDP, TCP 클라이언트)
- [x] `SocketInfo` 구조체로 소켓 상태 관리
- [x] `HandleTcpReceive()`, `HandleTcpSend()` 등 함수화 완료
- [x] 로그 기반 디버깅 처리
- [x] `AddSocketInfo()`, `RemoveSocketInfo()` 등 안정적 자원 관리

## 🔹 [B] 리팩토링 & 유지보수 전략

- [x] 전체 코드 리팩토링(`모듈화` / ~~`책임 분리`~~ / ~~`모듈 경계 정리`~~) 및 디버깅(GDB)
- [x] 이벤트 처리 `--nready`, break/continue 오류 정리
- [x] Windows 호환 코드로 이식 및 검증

## 🔹 [C] 보안 기능 도입 (**진행 예정**)

- [x] DES 구현 (서버가 키를 가지지 않는 단순 E2EE 구조)
  - [x] encryption 구현
  - [x] decryption 구현
  - [x] const char\* 타입으로 암호화, 복호화, return (그러나 uint8_t 타입이 여러방면으로 장점, return 타입은 void로 수정)
- [x] 구현한 DES를 TCP client&server에 적용하기
  - [x] Wire Protocol 적용 (전역변수 제거 & 정보 구조체화 \_데이터 직렬화, 송수신 안전성, 책임분리 등)
  - [x] client 송수신 구조를 분리 (함수화 및 수신 스레드 사용)
- [ ] ~~AES 구현~~ Openssl AES 암호화 적용
- [ ] ~~대칭키 관리 전략 수립 (고정/동적/세션 기반 등)~~
- [ ] ~~암복호화 유닛 테스트 구현~~

## 🔹 [D] 개발 유틸리티 & 자동화

- [ ] `Makefile` 작성 (Linux 빌드 자동화)
- [ ] `Shell Script` 또는 `bash` 사용한 실행 자동화

## 🔹 [E] 객체지향 구조 도입 및 멀티스레딩

- [ ] 간단한 **멀티스레딩 실험 및 구조 설계**
- [ ] `SocketManager` 클래스화
- [ ] `TcpClient`, `UdpClient` 추상화
- [ ] 메시지 구조 클래스 설계 (`Message`, `User`, `Session` 등)
- [ ] 이벤트 핸들러 클래스로 분리

## 🔹 [G] 추가 검토할 기능

- [ ] **비동기 I/O 모델 실험 (IOCP, io_uring 등)**
- [ ] **채팅 ID 도입** (송/수신자 구분)
- [ ] **1:N 메시지 전송**
- [ ] **N:N 채팅방 구성**
- [ ] **멀티스레딩 구조 완성 (thread pool, 작업 큐 기반 등)**
- [ ] **스레드 동기화 및 race condition 대응 (mutex, condition, atomic 등)**
- [ ] **TCP 연결 종료 상태 대응 (TIME_WAIT, CLOSE_WAIT 등)**
- [ ] **네트워크 예외 이벤트 대응 (RST, SIGPIPE, linger 등)**
- [ ] **프로토콜 계층화 및 메세지 라우팅 구조화**
- [ ] **다중 클라이언트 부하 테스트 및 성능 병목 측정**

- [ ] **파일 전송 기능** (sendfile, chunk, 파일 이름 포함 구조 등)
- [ ] **디버깅 모드 추가** (로그 레벨 구분, 콘솔 출력 제어)
- [ ] **클라이언트 채팅 기록 복원 기능**
- [ ] **소켓 옵션 설정 (keep-alive, reuseaddr 등)**
- [ ] **메시지 전송 방식 개선**
  - [ ] 고정 길이
  - [ ] 가변 길이
  - [ ] 길이 + 메시지 조합
  - [ ] **Client → Server → Client (c-s-c) 구조 구현**
  - [ ] ID 기반 메시지 중계
  - [ ] 메시지 필터링 (방, 대상 ID 등)
  - [ ] 향후 1:N / N:N 확장 기반 확보
