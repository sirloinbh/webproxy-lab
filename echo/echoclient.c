#include "csapp.h" // csapp.h 헤더 파일을 포함합니다. 이 파일은 네트워킹, 입출력 등의 유틸리티 함수를 포함합니다.

int main(int argc, char **argv) {
    int clientfd; // 클라이언트 소켓의 파일 디스크립터를 저장할 변수
    char *host, *port, buf[MAXLINE]; // 호스트, 포트 문자열과 입출력에 사용될 버퍼
    rio_t rio; // Rio (Robust I/O) 라이브러리의 입출력 스트림 구조체

    // 명령줄 인자를 검사하여 호스트 이름과 포트 번호가 제대로 입력되었는지 확인합니다.
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); // 사용법 오류 메시지를 표준 오류(stderr)에 출력합니다.
        exit(0); // 프로그램을 종료합니다.
    }
    host = argv[1]; // 첫 번째 인자를 호스트 이름으로 설정합니다.
    port = argv[2]; // 두 번째 인자를 포트 번호로 설정합니다.

    // 서버에 연결하기 위한 클라이언트 소켓을 초기화합니다.
    clientfd = open_clientfd(host, port); 
    Rio_readinitb(&rio, clientfd); // Rio 구조체를 소켓과 연결하여 초기화합니다.

    // 표준 입력(stdin)으로부터 문자열을 받아 서버에 전송하고, 서버의 응답을 받는 루프
    while (fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); // 표준 입력으로부터 받은 데이터를 서버로 전송합니다.
        Rio_readlineb(&rio, buf, MAXLINE); // 서버로부터 한 줄의 응답을 받습니다.
        fputs(buf, stdout); // 받은 응답을 표준 출력(stdout)에 출력합니다.
    }

    // 연결을 종료하고 리소스를 정리합니다.
    Close(clientfd); // 클라이언트 소켓을 닫습니다.
    exit(0); // 프로그램을 종료합니다.
}
