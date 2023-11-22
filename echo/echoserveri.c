#include "csapp.h" // csapp.h 헤더 파일을 포함합니다. 이 파일은 네트워킹 및 입출력 관련 함수와 매크로를 포함합니다.

// echo 함수: 클라이언트로부터 데이터를 읽어서 다시 그대로 클라이언트에게 보내는 기능을 수행합니다.
void echo(int connfd) {
    size_t n; // 읽은 바이트 수를 저장할 변수
    char buf[MAXLINE]; // 데이터 버퍼
    rio_t rio; // Rio (Robust I/O) 구조체

    // Rio 구조체를 초기화하고 소켓에 연결합니다.
    rio_readinitb(&rio, connfd);

    // 클라이언트로부터 데이터를 읽고, 읽은 데이터가 있으면 다시 클라이언트에게 보냅니다.
    while((n = rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes\n", (int)n); // 서버가 받은 바이트 수를 출력합니다.
        rio_writen(connfd, buf, n); // 읽은 데이터를 클라이언트에게 다시 전송합니다.
    }
}

int main(int argc, char **argv) {
    int listenfd, connfd; // 리스닝 및 연결 소켓의 파일 디스크립터
    socklen_t clientlen; // 클라이언트 주소의 크기
    struct sockaddr_storage clientaddr; // 클라이언트 주소 정보를 저장할 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE]; // 클라이언트의 호스트 이름과 포트 번호를 저장할 버퍼

    // 명령줄 인자를 검사하여 포트 번호가 제대로 입력되었는지 확인합니다.
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법 오류 메시지를 표준 오류(stderr)에 출력합니다.
        exit(0); // 프로그램을 종료합니다.
    }

    // 지정된 포트에서 연결을 수신하기 위한 리스닝 소켓을 생성합니다.
    listenfd = open_listenfd(argv[1]);

    // 무한 루프를 통해 클라이언트의 연결 요청을 계속 수신합니다.
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 클라이언트의 연결을 수락합니다.
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); // 클라이언트의 호스트 이름과 포트 번호를 얻습니다.
        printf("Connected to (%s, %s)\n", client_hostname, client_port); // 연결된 클라이언트의 정보를 출력합니다.
        echo(connfd); // 에코 함수를 호출하여 클라이언트와 통신합니다.
        Close(connfd); // 클라이언트와의 연결을 종료합니다.
    }
    exit(0); // 프로그램을 종료합니다.
}