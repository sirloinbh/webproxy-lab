#include "../csapp.h" // csapp.h 헤더 파일을 상위 디렉토리에서 포함합니다. 이 파일은 네트워킹 및 입출력 관련 함수와 매크로를 포함합니다.

int main(void) {
    char *buf, *p; // 쿼리 문자열을 처리하기 위한 포인터
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE]; // 쿼리 인자와 HTTP 응답 내용을 저장할 버퍼
    int n1=0, n2=0; // 쿼리 인자로부터 추출할 정수 값

    // QUERY_STRING 환경 변수에서 쿼리 문자열을 가져옵니다.
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&'); // '&' 문자를 찾아 쿼리 문자열을 두 부분으로 나눕니다.
        *p = '\0'; // 첫 번째 인자와 두 번째 인자를 분리하기 위해 null 문자로 대체합니다.
        sscanf(buf, "first=%d", &n1); // 첫 번째 인자를 파싱하여 정수로 변환합니다.
        sscanf(p+1, "second=%d", &n2); // 두 번째 인자를 파싱하여 정수로 변환합니다.
    }

    // HTTP 응답 내용을 구성합니다.
    sprintf(content, "Welcome to add.com: "); // 시작 메시지
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content); // 포털 설명 추가
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
        content, n1, n2, n1 + n2); // 계산 결과 추가
    sprintf(content, "%sThanks for visiting!\r\n", content); // 방문에 대한 감사 메시지 추가

    // HTTP 헤더를 출력합니다.
    printf("Connection: close\r\n"); // 연결 종료
    printf("Content-length: %d\r\n", (int)strlen(content)); // 내용 길이
    printf("Content-type: text/html\r\n\r\n"); // 내용 유형 (HTML)

    // 구성된 HTTP 응답 내용을 출력합니다.
    printf("%s", content);
    fflush(stdout); // 출력 버퍼를 비웁니다.

    exit(0); // 프로그램을 종료합니다.
}