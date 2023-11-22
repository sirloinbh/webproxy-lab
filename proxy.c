#include <stdio.h>
#include "csapp.h"

/**
 * web_object_t 구조체: 웹 캐시에 저장되는 객체의 정보를 저장합니다.
 * path: 객체의 URI 경로
 * content_length: 객체의 콘텐츠 길이
 * response_ptr: 객체 콘텐츠를 가리키는 포인터
 * prev, next: 이중 연결 리스트에서의 이전 및 다음 객체를 가리키는 포인터
 */
typedef struct web_object_t
{
  char path[MAXLINE];                 
  int content_length;                 
  char *response_ptr;                 
  struct web_object_t *prev, *next;   
} web_object_t;

// 함수 선언
web_object_t *find_cache(char *path);
void send_cache(web_object_t *web_object, int clientfd);
void read_cache(web_object_t *web_object);
void write_cache(web_object_t *web_object);

// 전역 변수 선언
extern web_object_t *rootp;           // 캐시된 객체의 루트 포인터
extern web_object_t *lastp;           // 캐시된 객체의 마지막 포인터
extern int total_cache_size;          // 현재 캐시의 총 크기

// 캐시 크기 상수 정의
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// 전역 변수 초기화
web_object_t *rootp;
web_object_t *lastp;
int total_cache_size = 0;

/**
 * find_cache 함수: 주어진 경로와 일치하는 캐시된 객체를 찾습니다.
 * path: 찾을 객체의 경로
 * 반환: 찾은 객체의 포인터, 없으면 NULL 반환
 */
web_object_t *find_cache(char *path)
{
  if (!rootp) 
    return NULL;
  web_object_t *current = rootp;      
  while (strcmp(current->path, path)) 
  {
    if (!current->next) 
      return NULL;

    current = current->next;          
    if (!strcmp(current->path, path)) 
      return current;
  }
  return current;
}

/**
 * send_cache 함수: 캐시된 객체를 클라이언트에게 전송합니다.
 * web_object: 전송할 캐시된 객체의 포인터
 * clientfd: 클라이언트 소켓의 파일 디스크립터
 */
void send_cache(web_object_t *web_object, int clientfd)
{
  char buf[MAXLINE];
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                                      
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);                            
  sprintf(buf, "%sConnection: close\r\n", buf);                                  
  sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, web_object->content_length); 
  Rio_writen(clientfd, buf, strlen(buf));

  Rio_writen(clientfd, web_object->response_ptr, web_object->content_length);
}

/**
 * read_cache 함수: 주어진 객체를 캐시의 가장 앞으로 이동시킵니다.
 * web_object: 최근에 사용된 캐시된 객체의 포인터
 */
void read_cache(web_object_t *web_object)
{
  if (web_object == rootp) 
    return;

  if (web_object->next) 
  {

    web_object_t *prev_objtect = web_object->prev;
    web_object_t *next_objtect = web_object->next;
    if (prev_objtect)
      web_object->prev->next = next_objtect;
    web_object->next->prev = prev_objtect;
  }
  else 
  {
    web_object->prev->next = NULL; 
  }

  web_object->next = rootp; 
  rootp = web_object;
}

/**
 * write_cache 함수: 새로운 객체를 캐시에 추가합니다.
 * web_object: 캐시에 추가할 객체의 포인터
 */
void write_cache(web_object_t *web_object)
{
  total_cache_size += web_object->content_length;

  while (total_cache_size > MAX_CACHE_SIZE)
  {
    total_cache_size -= lastp->content_length;
    lastp = lastp->prev; 
    free(lastp->next);   
    lastp->next = NULL;
  }

  if (!rootp) 
    lastp = web_object;


  if (rootp)
  {
    web_object->next = rootp;
    rootp->prev = web_object;
  }
  rootp = web_object;
}

// 함수 선언
void *thread(void *vargp);
void doit(int clientfd);
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);

/**
 * main 함수: 웹 프록시 서버의 메인 함수입니다.
 * 이 함수는 서버의 리스닝 소켓을 설정하고, 클라이언트의 연결 요청을 지속적으로 수락합니다.
 * 각 클라이언트 연결은 별도의 스레드에서 처리됩니다.
 *
 * argc: 명령줄 인자의 개수
 * argv: 명령줄 인자의 배열 (argv[1]은 사용할 포트 번호)
 */
static const int is_local_test = 1; 
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, *clientfd; // 리스닝 소켓 및 클라이언트 소켓 파일 디스크립터
  char client_hostname[MAXLINE], client_port[MAXLINE]; // 클라이언트의 호스트 이름과 포트 번호
  socklen_t clientlen; // 클라이언트 주소 구조체의 크기
  struct sockaddr_storage clientaddr; // 클라이언트 주소 정보를 저장하는 구조체
  pthread_t tid; // 스레드 ID
  signal(SIGPIPE, SIG_IGN); // SIGPIPE 시그널을 무시하도록 설정

  // 웹 객체 캐시를 위한 루트 및 마지막 포인터 할당 및 초기화
  rootp = (web_object_t *)calloc(1, sizeof(web_object_t));
  lastp = (web_object_t *)calloc(1, sizeof(web_object_t));

  // 명령줄 인자를 확인하고, 포트 번호가 제공되지 않았다면 사용법을 출력하고 종료
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 리스닝 소켓을 설정하고, 지정된 포트에서 클라이언트의 연결을 기다립니다.
  listenfd = Open_listenfd(argv[1]); 

  // 무한 루프를 통해 지속적으로 클라이언트 연결을 수락하고 스레드를 생성합니다.
  while (1)
  {
    clientlen = sizeof(clientaddr);
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    Pthread_create(&tid, NULL, thread, clientfd);
  }
}

/**
 * thread 함수: 새로운 클라이언트 연결을 처리하는 스레드 함수입니다.
 * 이 함수는 스레드를 분리(detach)하고, 클라이언트와의 통신을 처리한 후 연결을 종료합니다.
 *
 * vargp: 클라이언트 소켓 파일 디스크립터를 가리키는 포인터입니다.
 * 반환 값: NULL을 반환합니다.
 */
void *thread(void *vargp)
{
  int clientfd = *((int *)vargp); // 클라이언트 소켓 파일 디스크립터 추출
  Pthread_detach(pthread_self()); // 현재 스레드를 분리(detach)하여 독립적으로 실행
  Free(vargp); // 동적 할당된 메모리 해제
  doit(clientfd); // 클라이언트 요청 처리
  Close(clientfd); // 클라이언트 소켓 종료
  return NULL;
}

/**
 * doit 함수: 클라이언트로부터의 HTTP 요청을 처리합니다.
 * 이 함수는 클라이언트의 요청을 읽고, 필요에 따라 캐시된 응답을 전송하거나
 * 원격 서버에 요청을 전달하여 새로운 응답을 가져옵니다.
 *
 * clientfd: 클라이언트와의 연결을 나타내는 파일 디스크립터
 */
void doit(int clientfd)
{
  int serverfd, content_length; // 원격 서버의 파일 디스크립터 및 응답 콘텐츠 길이
  char request_buf[MAXLINE], response_buf[MAXLINE]; // HTTP 요청 및 응답 버퍼
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE]; // 파싱된 URI의 구성 요소 및 응답 포인터
  rio_t request_rio, response_rio; // Robust I/O 구조체

  // 클라이언트 요청 읽기 및 출력
  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n %s\n", request_buf);

  // 요청에서 HTTP 메소드와 URI 추출
  sscanf(request_buf, "%s %s", method, uri);
  parse_uri(uri, hostname, port, path);
    printf("Parsed URI: Hostname = %s, Port = %s, Path = %s\n", hostname, port, path);

  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");

  // 지원하지 않는 메소드에 대해 클라이언트에게 오류 메시지 전송
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 캐시된 객체가 있는지 확인하고, 있으면 해당 캐시를 전송
  web_object_t *cached_object = find_cache(path);
  if (cached_object) 
  {
    send_cache(cached_object, clientfd); 
    read_cache(cached_object);           
    return;                              
  }

  // 원격 서버에 연결
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_clientfd("15.164.95.158", port);
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", "📍 Failed to establish connection with the end server");
    return;
  }
  Rio_writen(serverfd, request_buf, strlen(request_buf));

  // 원격 서버에 요청 헤더 전송
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);

  // 원격 서버로부터의 응답 읽기 및 클라이언트에 전송
  Rio_readinitb(&response_rio, serverfd);
  while (strcmp(response_buf, "\r\n"))
  {
    Rio_readlineb(&response_rio, response_buf, MAXLINE);
    if (strstr(response_buf, "Content-length")) 
      content_length = atoi(strchr(response_buf, ':') + 1);
    Rio_writen(clientfd, response_buf, strlen(response_buf));
  }

  // 응답 본문 읽기 및 전송
  response_ptr = malloc(content_length);
  Rio_readnb(&response_rio, response_ptr, content_length);
  Rio_writen(clientfd, response_ptr, content_length); 

  // 응답 크기에 따라 캐시 작업 결정
  if (content_length <= MAX_OBJECT_SIZE) 
  {
    web_object_t *web_object = (web_object_t *)calloc(1, sizeof(web_object_t));
    web_object->response_ptr = response_ptr;
    web_object->content_length = content_length;
    strcpy(web_object->path, path);
    write_cache(web_object); 
  }
  else
    free(response_ptr); 

  // 원격 서버 연결 종료
  Close(serverfd);
}

/**
 * clienterror 함수: 클라이언트에게 HTTP 오류 메시지를 전송합니다.
 * 이 함수는 클라이언트에게 오류에 대한 HTML 형식의 설명을 제공합니다.
 *
 * fd: 클라이언트와의 연결을 나타내는 파일 디스크립터
 * cause: 오류의 원인을 나타내는 문자열
 * errnum: HTTP 오류 번호 (예: "404")
 * shortmsg: 짧은 오류 메시지 (예: "Not Found")
 * longmsg: 긴 오류 설명 (예: "Tiny couldn't find this file")
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];          // HTTP 응답 및 HTML 본문을 위한 버퍼

  // HTML 오류 페이지의 본문을 구성합니다.
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // HTTP 응답 헤더를 구성하고 클라이언트에게 전송합니다.
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // 구성된 HTML 오류 페이지를 클라이언트에게 전송합니다.
  Rio_writen(fd, body, strlen(body));
}

/**
 * parse_uri 함수: 주어진 URI를 파싱하여 호스트명, 포트, 경로를 추출합니다.
 * 이 함수는 URI를 분석하여 웹 요청을 위한 기본적인 구성 요소를 분리합니다.
 *
 * uri: 분석할 전체 URI 문자열
 * hostname: 파싱된 호스트명을 저장할 문자열
 * port: 파싱된 포트 번호를 저장할 문자열
 * path: 파싱된 경로를 저장할 문자열
 */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  // 호스트명 시작 지점을 찾습니다. "//" 뒤에 위치합니다.
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;

  // 포트와 경로의 시작 지점을 찾습니다.
  char *port_ptr = strchr(hostname_ptr, ':'); 
  char *path_ptr = strchr(hostname_ptr, '/'); 

  // 경로를 복사합니다.
  strcpy(path, path_ptr);

  // 포트가 지정된 경우 포트와 호스트명을 추출합니다. 포트가 지정되지 않은 경우, 로컬 테스트 여부에 따라 기본 포트를 설정합니다.
  if (port_ptr) 
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1); 
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr); 
  }
  else 
  {
    if (is_local_test)
      strcpy(port, "80"); 
    else
      strcpy(port, "8000");
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }
}

/**
 * read_requesthdrs 함수: 클라이언트로부터 받은 HTTP 요청 헤더를 읽고 수정한 후 원격 서버에 전송합니다.
 * 이 함수는 특정 헤더를 변경하거나 추가하여 프록시 서버의 요구 사항에 맞게 요청을 조정합니다.
 *
 * request_rio: 클라이언트로부터 읽은 요청을 위한 Robust I/O 스트림 포인터
 * request_buf: 요청 헤더를 저장할 버퍼
 * serverfd: 요청을 전달할 원격 서버의 파일 디스크립터
 * hostname: 요청을 전송할 호스트 이름
 * port: 요청을 전송할 포트 번호
 */
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port)
{
  // 헤더 존재 여부를 추적하는 플래그
  int is_host_exist = 0;
  int is_connection_exist = 0;
  int is_proxy_connection_exist = 0;
  int is_user_agent_exist = 0;

  // 클라이언트로부터 헤더를 읽고, 필요한 수정을 하여 서버에 전달
  Rio_readlineb(request_rio, request_buf, MAXLINE);
  while (strcmp(request_buf, "\r\n"))
  {
    // "Proxy-Connection", "Connection", "User-Agent", "Host" 헤더의 존재 여부 확인 및 수정
    if (strstr(request_buf, "Proxy-Connection") != NULL)
    {
      sprintf(request_buf, "Proxy-Connection: close\r\n");
      is_proxy_connection_exist = 1;
    }
    else if (strstr(request_buf, "Connection") != NULL)
    {
      sprintf(request_buf, "Connection: close\r\n");
      is_connection_exist = 1;
    }
    else if (strstr(request_buf, "User-Agent") != NULL)
    {
      sprintf(request_buf, user_agent_hdr);
      is_user_agent_exist = 1;
    }
    else if (strstr(request_buf, "Host") != NULL)
    {
      is_host_exist = 1;
    }

    // 수정된 헤더를 원격 서버에 전송
    Rio_writen(serverfd, request_buf, strlen(request_buf));
    Rio_readlineb(request_rio, request_buf, MAXLINE);
  }

  // 누락된 헤더를 추가
  if (!is_proxy_connection_exist)
  {
    sprintf(request_buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_connection_exist)
  {
    sprintf(request_buf, "Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_host_exist)
  {
    sprintf(request_buf, "Host: %s:%s\r\n", hostname, port);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_user_agent_exist)
  {
    sprintf(request_buf, user_agent_hdr);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }

  // 헤더의 끝을 나타내는 빈 줄 전송
  sprintf(request_buf, "\r\n");
  Rio_writen(serverfd, request_buf, strlen(request_buf));
}