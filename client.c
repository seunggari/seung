#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SERVERIP "127.0.0.1"			// IP는 localhost
#define BUFSIZE 512 
#define MAX 50

/* 출력함수 */
double print_select_menu();				// 메뉴출력	
double print_func_menu();				// 기능출력

/* write 함수 */
void scan_write(int sock);				// scanf -> write	
void scan_write_member_join(int sock);	
void send_write_pipe(char *msg, char *cmd, int fd, char *menu);	// pipe 통신

void welcome_message();					// 환영메시지
void error_handling(char *message);		// 에러핸들링	
void z_handler(int sig);				// 시그널함수
static void toggle_echo(int alwayson);	// 비밀번호 감추기


int main(int argc, char **argv)
{
	int i, sock, chat, signal, status, ret;
	int fd[2];	//pipe ipc,  0->read, 1->write
	double state = 0;
	pid_t pid;
	char message[BUFSIZE];
	struct sigaction act;
	struct sockaddr_in serv_addr;
	
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	/* 공유 메모리 */
	void *shmaddr;
	int shmid;

	if (shmid=shmget((key_t)1234, 1024, IPC_CREAT|0666) == -1) {
		error_handling("shmget() error");
	}

	if ((shmaddr=shmat(shmid, (void*)0, 0)) == (void*)-1) {
		error_handling("shmat() error");
	}

	strcpy((char*)shmaddr, "NORMAL");
	/* 공유 메모리 */

	if (signal != 0) {
		error_handling("sigaction() error");
	}

	/* socket -> connect -> pipe */
	sock = socket(PF_INET, SOCK_STREAM, 0);

	if (sock == -1) {
		error_handling("socket() error");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(SERVERIP);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
		error_handling("connect() error");
	}

	if (pipe(fd) < 0) {	// pipe
		error_handling("pipe() error");
	}
	/* socket -> connect -> pipe */

	// 환영메시지 출력
	welcome_message();
	printf("Connected port is %d\n", atoi(argv[1]));

	pid = fork();

	/* child (write) */
	if (pid == 0) {	

		close(fd[1]);	// close pipe write

		/* child while */
		while (1)	
		{
			/* 접속종료 */	
			if (state == -1) {
				break;
			}
			/* 접속종료 */


			/* 메뉴입력 */	
			else if (state == 0) {
				double menu;
				char menus[MAX];

				menu = print_select_menu();
				sprintf(menus, "%lf", menu);	// double -> char*
				write(sock, menus, BUFSIZE);
				state = menu;
			}
			/* 메뉴입력 */


			/* 로그인 (ID입력) */
			else if (state == 1) {
				scan_write(sock);
				usleep(500);	
				memset(message, 0, BUFSIZE);
				read(fd[0], message, BUFSIZE);
				state = atof(message);
			}
			/* 로그인 (ID입력) */


			/* 로그인 (PW입력) */
			else if (state == 1.1) {
				toggle_echo(0);
				scan_write(sock);
				toggle_echo(0);

				usleep(500);	
				memset(message, 0, BUFSIZE);
				read(fd[0], message, BUFSIZE);
				state = atof(message);
			}
			/* 로그인 (PW입력) */


			/* 회원가입 */	
			else if (state == 2) {
				scan_write_member_join(sock);

				toggle_echo(0);			// 비밀번호 감추기
				scan_write_member_join(sock);
				toggle_echo(0);

				for(i=0; i<3; i++) {
					scan_write(sock);	// name, loc, age
				}

				usleep(500);
				memset(message, 0, BUFSIZE);
				read(fd[0], message, BUFSIZE);
				state = atof(message);				
			}
			/* 회원가입 */


			/* 기능입력 */	
			else if (state == 3) {
				double func;
				char funcs[MAX];

				func = print_func_menu();
				sprintf(funcs, "%lf", func);	// double -> char*
				write(sock, funcs, MAX);
				state = func + 3;
			}
			/* 기능입력 */


			/* 접속자 리스트 출력 */
			else if (state == 4) {
				usleep(500);
				state = 3;	// 기능입력
			}
			/* 접속자 리스트 출력 */


			/* 정보수정 전 PW인증 */	
			else if (state == 5) {
				toggle_echo(0);
				scan_write(sock);
				toggle_echo(0);

				usleep(500);
				memset(message, 0, BUFSIZE);
				read(fd[0], message, BUFSIZE);
				state = atof(message);
			}
			/* 정보수정 전 PW인증 */


			/* 클라이언트 정보수정 */
			else if (state == 5.1) {

				for(i=0; i<3; i++) {	// name, loc, age
					scan_write(sock);
				}

				usleep(500);
				memset(message, 0, BUFSIZE);
				read(fd[0], message, BUFSIZE);
				state = atof(message);
			}
			/* 클라이언트 정보수정 */


			/* 채팅방 입장 */
			else if (state == 6) {
				usleep(500);
				memset(message, 0, BUFSIZE);
				read(fd[0], message, BUFSIZE);
				state = atof(message);
			}
			/* 채팅방 입장 */


			/* 채팅 */	
			else if (state == 6.1) {
				printf("\033[36m%s\033[0m\n", "HELP is [/help]");

				/* 채팅반복 */
				while(1)
				{
					char input[BUFSIZE];

					fgets(input, BUFSIZE, stdin);
					input[strlen(input) - 1] = '\0';
					write(sock, input, MAX);

					if (!(strcmp(input, "/quit"))) {		// quit 입력시 채팅종료
						memset(message, 0, BUFSIZE);
						read(fd[0], message, BUFSIZE);
						state = atof(message);
						break;
					}

				}
				/* 채팅반복 */

			}
			/* 채팅 */


			/* 로그아웃 */	
			else if (state == 7) {
				state = 0;	// 메뉴입력
			}
			/* 로그아웃 */
		}
		/* child while */
	}
	/* child (write) */


	/* parent (read) */
	else {	

		close(fd[0]);	//close pipe read

		/* parent while */
		while (1)  	
		{	
			/* 채팅방 입력시 */
			while (chat == 1) 
			{
				memset(message, 0, BUFSIZE);	
				ret = read(sock, message, BUFSIZE);

				if (ret == -1 || ret == 0) {
					strcpy((char*)shmaddr, "ABNORMAL");
					break;
				}

				if (!(strcmp(message, "kick"))) {
					printf("\033[91mYou have been kicked by SERVER\033[0m\n");
					write(sock, "kick",	MAX);
					exit(0);
				}

				if (!(strcmp(message, "/quit"))) {
					write(fd[1], "3", MAX);
					chat = 0;
					break;
				}

				if (message[strlen(message)-1] == '1') {		// 입,퇴장 메세지 -> red
					message[strlen(message)-1] = '\0';
					printf("\033[91m%s\033[0m\n", message);
				}

				if (message[strlen(message)-1] == '2') {		// 상황설명	->	blue
					message[strlen(message)-1] = '\0';
					printf("\033[36m%s\033[0m\n", message);
				}

				if (message[strlen(message)-1] == '3') {		// 일반채팅 -> yellow 
					message[strlen(message)-1] = '\0';
					printf("\033[33m%s\033[0m\n", message);
				}

			}
			/* 채팅방 입력시 */


			/* 일반 입력시 */
			memset(message, 0, BUFSIZE);	
			ret = read(sock, message, BUFSIZE);

			if (ret == -1 || ret == 0) {
				strcpy((char*)shmaddr, "ABNORMAL");
				break;
			}

			printf("%s\n", message);
			/* 일반 입력시 */


			/* PIPE 통신 */
			send_write_pipe(message, "ID right",		fd[1], "1.1");		// PW입력
			send_write_pipe(message, "ID wrong",		fd[1], "0");		// 메뉴입력
			send_write_pipe(message, "Login Success",	fd[1], "3");		// 기능입력
			send_write_pipe(message, "Already Login",	fd[1], "0");		// 메뉴입력
			send_write_pipe(message, "PW wrong",		fd[1], "0");		// 메뉴입력
			send_write_pipe(message, "Join Success",	fd[1], "0");		// 메뉴입력
			send_write_pipe(message, "Already Used",	fd[1], "0");		// 메뉴입력
			send_write_pipe(message, "Confirmed",		fd[1], "5.1");		// PW인증
			send_write_pipe(message, "Check your PW",	fd[1], "3");		// 기능입력
			send_write_pipe(message, "Edit Success",	fd[1], "3");		// 기능입력
			send_write_pipe(message, "Edit Fail",		fd[1], "3");		// 기능입력

			if (!(strcmp(message, "Enter Chat Room"))) {
				system("clear");
				write(fd[1], "6.1", MAX);								// 채팅	
				chat = 1;
			}
			/* PIPE 통신 */

			if (waitpid(pid, &status, WNOHANG) == -1) {	// 자식프로세스 종료시 부모프로세스도 같이 종료
				break;
			}

		}
		/* parent while */

		printf("\nProgram End...\n");

		if (!(strcmp((char*)shmaddr, "ABNORMAL"))) {
			char end_cmd[MAX];
		  sprintf(end_cmd, "kill %d", pid);
		  system(end_cmd);
		 }

		close(sock);
		return 0;	//부모 프로세스 종료시 client.c는 종료
	}
	/* parent (read) */
}

void welcome_message()
{
	system("clear");
	printf("+====================================+\n");
	printf("+ Chatting Programming! Made by hoon +\n");
	printf("+ Server : Multi-Thread              +\n");
	printf("+ Client : Multi-Processor           +\n");
	printf("+ Here is Client Part                +\n");
	printf("+====================================+\n");
}

double print_select_menu()
{
	char menus[MAX];
	double menu;

	while(1)
	{
		printf("+=============== MENU ===============+\n");
		printf("+ 1. Login                           +\n");
		printf("+ 2. Join                            +\n");
		printf("+====================================+\n");
		printf("Select the Menu (Exit : -1) : ");
		scanf("%s", menus);

		menu = atof(menus);

		if (menu == 1 || menu  == 2 || menu == -1) {	// 메뉴 이외의 입력 금지
			break;
		}

		printf("Re input...\n\n\n");
	}

	return menu;
}

double print_func_menu()
{
	char funcs[MAX];
	double func;

	while(1)
	{
		printf("+=============== FUNC ===============+\n");
		printf("+ 1. User Info List                  +\n");
		printf("+ 2. User Info Edit                  +\n");
		printf("+ 3. Chat                            +\n");
		printf("+ 4. Logout                          +\n");
		printf("+====================================+\n");
		printf("Select the Func : ");
		scanf("%s", funcs);

		func = atof(funcs);

		if (func == 1 || func == 2 || func == 3 || func == 4) {		// 메뉴 이외의 입력 금지
			break;
		}

		printf("Re input...\n\n\n");
	}

	return func;
}

void scan_write(int sock)
{
	char input[MAX];

	scanf("%s", input);
	write(sock, input, MAX);
}

void scan_write_member_join(int sock)
{
	char input[MAX];

	while(1)
	{
		scanf("%s", input);

		if (strlen(input) > 3 && strlen(input) < 13) {
			break;	
		}

		printf("Mininum length is 4, Maxinum length is 12\n");
	}

	write(sock, input, MAX);
}

static void toggle_echo(int alwayson)
{
	struct termios term;

	tcgetattr(fileno(stdin), &term);

	if (alwayson) {
		term.c_lflag |= ECHO;
	} 
	else {
		if (term.c_lflag & ECHO) {
			term.c_lflag &= ~ECHO;
		} 
		else {
			term.c_lflag |= ECHO;
		}
	}

	tcsetattr(fileno(stdin), TCSAFLUSH, &term);
}

void send_write_pipe(char *msg, char *cmd, int fd, char *menu)
{
	if (!(strcmp(msg, cmd))) {		// read한 msg가 특정 cmd와 같은경우
		write(fd, menu, MAX);		// pipe통신으로 write 프로세스에 state를 전달
	}
}

void z_handler(int sig)
{
	pid_t pid;
	int ret;

	pid = waitpid(-1, &ret, WNOHANG);
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
