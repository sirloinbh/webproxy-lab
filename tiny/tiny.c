#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* 
 * main 함수: 웹 서버의 메인 루틴입니다.
 * argc: 명령줄 인자의 개수입니다.
 * argv: 명령줄 인자의 배열입니다.
 */
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); 
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  
    Close(connfd); 
  }
}

/* 
 * doit 함수: 클라이언트의 HTTP 요청을 처리합니다.
 * fd: 클라이언트와 연결된 소켓의 파일 디스크립터입니다.
 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  is_static = parse_uri(uri, filename, cgiargs);
  if(stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if(is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

/* 
 * clienterror 함수: 클라이언트에 오류 메시지를 전송합니다.
 * fd: 클라이언트와 연결된 소켓의 파일 디스크립터입니다.
 * cause: 오류의 원인입니다.
 * errnum: HTTP 오류 번호입니다.
 * shortmsg: 간단한 오류 메시지입니다.
 * longmsg: 상세한 오류 메시지입니다.
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor = ""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
  }

/* 
 * read_requesthdrs 함수: HTTP 요청 헤더를 읽습니다.
 * rp: Rio 구조체의 포인터입니다.
 */
  void read_requesthdrs(rio_t *rp)
  {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
      Rio_readlineb(rp, buf, MAXLINE);
      printf("%s", buf);
    }
    return;
  }

/* 
 * parse_uri 함수: URI를 파싱하여 정적/동적 컨텐츠의 경로를 결정합니다.
 * uri: 요청받은 URI입니다.
 * filename: 파싱된 파일 이름입니다.
 * cgiargs: CGI 인자입니다 (동적 컨텐츠의 경우).
 * 반환 값: 정적 컨텐츠인 경우 1, 동적 컨텐츠인 경우 0입니다.
 */
  int parse_uri(char *uri, char *filename, char *cgiargs)
  {
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {
      strcpy(cgiargs, "");
      strcpy(filename, ".");
      strcat(filename, uri);
      if (uri[strlen(uri) -1] =='/')
        strcat(filename, "home.html");
      return 1;
    }
    else {
      ptr = index(uri, '?');
      if (ptr) {
        strcpy(cgiargs, ptr+1);
        *ptr = '\0';
      }
      else
        strcpy(cgiargs, "");
      strcpy(filename, ".");
      strcat(filename, uri);
      return 0;
    }
  }

/* 
 * serve_static 함수: 정적 컨텐츠를 클라이언트에 제공합니다.
 * fd: 클라이언트와 연결된 소켓의 파일 디스크립터입니다.
 * filename: 제공할 파일의 이름입니다.
 * filesize: 파일의 크기입니다.
 * method: HTTP 요청 메소드입니다.
 */
  void serve_static(int fd, char *filename, int filesize, char *method)
  {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf,filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    if (strcasecmp(method, "HEAD") == 0)
      return;

    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
  }

/* 
 * get_filetype 함수: 파일의 MIME 타입을 결정합니다.
 * filename: 파일 이름입니다.
 * filetype: 결정된 파일 타입이 저장될 문자열입니다.
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "video/mpg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}

  void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
  {
    char buf[MAXLINE], *emptylist[] = { NULL };

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) {
      setenv("QUERY_STRING", cgiargs, 1);
      setenv("REQUEST_METHOD", method, 1);
      Dup2(fd, STDOUT_FILENO);
      Execve(filename, emptylist, environ);
    }
    Wait(NULL);
  }

/*
 * 위의 함수들은 Tiny 웹 서버의 핵심 기능을 구현합니다. 각 함수는 HTTP 요청을 처리하고, 
 * URI를 분석하며, 적절한 컨텐츠(정적 또는 동적)를 클라이언트에 제공합니다.
 */