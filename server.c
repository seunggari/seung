#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <pthread.h>
#include <time.h>
#include <openssl/rsa.h>

#define DBADDRESS "./test.db"
#define BUFSIZE 512
#define MAX 50
#define CLIENTMAX 30

struct client						// 클라이언트 구조체
{									// 접속시 소켓번호와 주소
	int sock;						// 로그인시 ID와 시간을 저장
	struct sockaddr_in sock_addr;	// state를 통하여 접속한 client가 무슨 기능을 하는지 체크
	char id[MAX];
	char login_time[MAX];
	double state;
};

/* 쓰레드 */ 
void* clnt_connection(void * arg);
void* server_cmd(void *arg);

/* SQLITE3 함수 */
int check_member_id(char *id);		
char* find_member_pw(char *id);		
int insert_join_member(char *id, char *pw, char *name, char *loc, int age);	
int edit_user_info(char *name, char *loc, int age, char *id);

/* 채팅 함수 */
void chat_message(char *message, int len, int sock);
void whole_send_message(char *message, int len);

/* 접속자 리스트 */
void add_client(struct client * clnt);
void remove_client(int sock);		
void login_client(int sock, char *id);
void logout_client(int sock);

/* 채팅 리스트 */
char* enter_chat(int sock);
void exit_chat(int sock);

/* TIME */
char* get_time(char* type);

/* 로그파일 */
void add_log(char *log);

void welcome_message();					// 환영메시지
void error_handling(char * message);	// 에러핸들링	

// ====================================================================================== //

/* 로그인 리스트 */
int clnt_number = 0;
struct client* clnt_arr[CLIENTMAX];

/* 채팅 리스트 */
int chat_number = 0;
struct client* chat_arr[CLIENTMAX];

/* SQLITE3 */
sqlite3 *db;
sqlite3_stmt *stmt;

/* TIME */
struct tm *t;
time_t timer;

/* 쓰레드 */
pthread_mutex_t mutx;

/* 암호화 */
RSA *rsa;
FILE *fp;

/* 로그파일 */
FILE *f;
char save_file_name[MAX];
char log_msg[MAX];

// ====================================================================================== //

int main(int argc, char **argv)
{
	int serv_sock, clnt_sock;
	int clnt_addr_size, status; 
	struct sockaddr_in serv_addr, clnt_addr;

	
	pthread_t thread;

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	/* rsa 암호 만드는 중 */
	rsa = RSA_new();
	rsa = RSA_generate_key(1024, 3, NULL, NULL);
	fp = fopen("pri_key", "w");
	PEM_write_RSAPublicKey(f, rsa);
	fclose(fp);
	/* rsa 암호 만드는 중 */

	
	if (pthread_mutex_init(&mutx, NULL)) {
		error_handling("mutex init() error");
	}

	/* socket -> bind -> listen -> sqlite_open */
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);

	if (serv_sock == -1) {
		error_handling("socket() error");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
		error_handling("bind() error");
	}

	if (listen(serv_sock, 5) == -1) {
		error_handling("listen() error");
	}

	if (sqlite3_open(DBADDRESS, &db) != SQLITE_OK) {
		error_handling("create_sqlite3() error");
	}	
	/* socket -> bind -> listen -> sqlite_open */

	/* 로그 파일 생성 */
	sprintf(save_file_name, get_time("date"));
	strcat(save_file_name, ".log");
	add_log("Program Start\n");

	// 환영메시지 출력
	welcome_message();
	printf("Connected port is %d\n", atoi(argv[1]));
	printf("Waiting...\n");

	while(1)
	{
		clnt_addr_size = sizeof(clnt_addr);	
		clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

		if (clnt_sock == -1) {
			continue;
		}

		struct client *clnt = (struct client*)malloc(sizeof(struct client));
		clnt->sock = clnt_sock;
		clnt->sock_addr = clnt_addr;
		clnt->state = 0;

		add_client(clnt);	// 소켓번호와 주소를 접속자 리스트에 입력 

		pthread_create(&thread, NULL, server_cmd, (void*)clnt);			// 서버 명령어 쓰레드
		pthread_create(&thread, NULL, clnt_connection, (void*) clnt);	// 클라이언트 접속 쓰레드
	}

	return 0;
}

void* server_cmd(void *arg)
{
	int i;
	char root_msg[MAX], root_cmd[MAX], root_obj[MAX];
	char help_msg[BUFSIZE] = "KICK\tis [/kick ID]\nQUIT\tis [/quit]";
	char *ptr;
	struct client *clnt = (struct client*) arg;

	/* 명령어 while */
	while(1)
	{
		int cnt = 0;
		memset(root_cmd, 0, MAX);
		fgets(root_msg, BUFSIZE, stdin);
		root_msg[strlen(root_msg) - 1] = '\0';

		/* 서버 명령어 파싱 */
		ptr = strtok(root_msg, " ");
		while(ptr)
		{
			if (cnt == 0) {
				strcpy(root_cmd, ptr);
			}

			else if (cnt == 1) {
				strcpy(root_obj, ptr);
				break;
			}

			cnt++;
			ptr = strtok(NULL, " ");
		}
		/* 서버 명령어 파싱 */


		/* 서버 명령어 */
		if (!(strcmp(root_cmd, "/help"))) {
			printf("%s\n", help_msg);
			continue;
		}

		if (!(strcmp(root_cmd, "/kick"))) {

			for (i=0; i<clnt_number; i++) {
				if (!(strcmp(root_obj, clnt_arr[i]->id))) {
					write(clnt_arr[i]->sock, "kick", MAX);
					break;
				}
			}
			continue;
		}

		if (!(strcmp(root_cmd, "/quit"))) {
			printf("Server is Closed\n");

			for (i=0; i<clnt_number; i++) {
				write(clnt_arr[i]->sock, "\nServer is Closed\n", MAX);
			}

			exit(0);
		}
		/* 서버 명령어 */
	}
	/* 명령어 while */
}

void* clnt_connection(void *arg)
{
	int i;
	char message[BUFSIZE], cipher[BUFSIZE];
	struct client *clnt = (struct client*) arg;

	/* 전체 while */
	while(1)
	{
		/* 접속종료 */
		if (clnt->state == -1) {
			remove_client(clnt->sock);	//접속종료시 접속자 리스트에서 제거
			break;
		}
		/* 접속종료 */


		/* 메뉴입력 */ 
		if (clnt->state == 0) {
			read(clnt->sock, message, BUFSIZE);
			clnt->state = atof(message);
		}
		/* 메뉴입력 */


		/* 로그인 */
		{
			char id[MAX], pw[MAX], db_pw[MAX];

			/* ID 입력 */	
			if (clnt->state == 1) {
				int check_id_cnt;

				write(clnt->sock, "Login!\nEnter your ID", BUFSIZE);
				read(clnt->sock, message, BUFSIZE);
				strcpy(id, message);

				check_id_cnt = check_member_id(id);				// ID가 DB테이블에 있는지 체크 

				if (check_id_cnt == 0) {						// ID 존재 X
					write(clnt->sock, "ID wrong", BUFSIZE);
					clnt->state = 0;		// ID 입력 
				}

				else if (check_id_cnt == 1) {					// ID 존재 O
					write(clnt->sock, "ID right", BUFSIZE);
					clnt->state = 1.1;		// PW 입력
				}
			}
			/* ID 입력 */

			/* PW 입력 */
			if (clnt->state == 1.1) {
				write(clnt->sock, "Enter your PW", BUFSIZE);
				read(clnt->sock, message, BUFSIZE);
				strcpy(pw, message);

				strcpy(db_pw, find_member_pw(id));				// 입력한 ID에 해당하는 PW 찾기

				/* 로그인 성공 */
				if (!(strcmp(db_pw, pw))) {	 

					/* 접속자 리스트 while */
					for (i=0; i<clnt_number; i++) {

						if (!(strcmp(id, clnt_arr[i]->id))) {	// 중복 로그인
							write(clnt->sock, "Already Login", BUFSIZE);
							clnt->state = 0;	// 메뉴입력
							break;
						}

						if (i == clnt_number - 1) {				// 처음 로그인
							write(clnt->sock, "Login Success", BUFSIZE);
							login_client(clnt->sock, id);
							clnt->state = 3;	// 기능입력
							break;
						}
					}
					/* 접속자 리스트 while */
				}
				/* 로그인 성공 */


				/* 로그인 실패 */
				else {	
					sprintf(log_msg, "[%s] '%s' PW wrong\n", get_time("all"), id);
					printf("%s", log_msg);
					add_log(log_msg);

					write(clnt->sock, "PW wrong", BUFSIZE);
					clnt->state = 0;	// 메뉴입력 
				}
				/* 로그인 실패 */
			}
			/* PW 입력 */
		}
		/* 로그인 */	


		/* 회원가입 */	
		if (clnt->state == 2) {
			char id[MAX], pw[MAX], name[MAX], loc[MAX];
			int age, result;

			write(clnt->sock, "Join!\nEnter your ID [MIN length 4, MAX length 12]", BUFSIZE);
			read(clnt->sock, message, BUFSIZE);
			strcpy(id, message);

			write(clnt->sock, "Enter your PW [MIN length 4, MAX length 12]", BUFSIZE);
			read(clnt->sock, message, BUFSIZE);
			strcpy(pw, message);

			write(clnt->sock, "Enter your NAME", BUFSIZE);
			read(clnt->sock, message, BUFSIZE);
			strcpy(name, message);

			write(clnt->sock, "Enter your LOC", BUFSIZE);
			read(clnt->sock, message, BUFSIZE);
			strcpy(loc, message);

			write(clnt->sock, "Enter your AGE", BUFSIZE);
			read(clnt->sock, message, BUFSIZE);
			age = atoi(message);

			// 회원가입 쿼리
			result = insert_join_member(id, pw, name, loc, age);	

			if (result == SQLITE_DONE) {		// 회원가입 성공
				sprintf(log_msg, "[%s] '%s' Join Success\n", get_time("all"), id);
				printf("%s", log_msg);
				add_log(log_msg);

				write(clnt->sock, "Join Success", BUFSIZE);
				clnt->state = 0;	// 메뉴입력
			}

			else if (result == SQLITE_ERROR) {	// 회원가입 실패
				sprintf(log_msg, "[%s] '%s' Already Used\n", get_time("all"), id);
				printf("%s", log_msg);
				add_log(log_msg);

				write(clnt->sock, "Already Used", BUFSIZE);
				clnt->state = 0;	// 메뉴입력
			}
		}
		/* 회원가입 */


		/* 기능입력 */	
		if (clnt->state == 3) {
			read(clnt->sock, message, BUFSIZE);
			clnt->state = atof(message) + 3;
		}
		/* 기능입력 */


		/* 접속자 리스트 출력 */
		if (clnt->state == 4) {
			char list[BUFSIZE], temp[BUFSIZE], me_id[MAX];

			memset(list, 0, BUFSIZE);
			sprintf(list, "\n============== Connecting Client List ==============\n");

			for(i=0; i<clnt_number; i++) {
				memset(temp, 0, BUFSIZE);

				if (strcmp(clnt_arr[i]->id,"")) { 

					if (clnt->sock == clnt_arr[i]->sock) {		// 자기자신
						sprintf(temp, "Login_Time : %s, ID : %s [me]\n", clnt_arr[i]->login_time, clnt_arr[i]->id);
						strcpy(me_id, clnt_arr[i]->id);
					}

					else {
						sprintf(temp, "Login_Time : %s, ID : %s\n", clnt_arr[i]->login_time, clnt_arr[i]->id);
					}
					strcat(list, temp);
				}
			}
			write(clnt->sock, list, BUFSIZE);
			clnt->state = 3;	// 기능입력
		}
		/* 접속자 리스트 출력 */


		/* 클라이언트 정보 수정 */
		{
			char now_id[MAX];

			/* 정보수정 전 PW인증 */	
			if (clnt->state == 5) {
				char now_pw[MAX], pw[MAX];

				pthread_mutex_lock(&mutx);
				for (i=0; i<clnt_number; i++) {
					if (clnt->sock == clnt_arr[i]->sock) {
						strcpy(now_id, clnt_arr[i]->id);
					}
				}
				pthread_mutex_unlock(&mutx);

				strcpy(now_pw, find_member_pw(now_id));

				write(clnt->sock, "Enter your PW again", MAX);
				read(clnt->sock, message, BUFSIZE);
				strcpy(pw, message);

				if (!(strcmp(now_pw, pw))) {
					write(clnt->sock, "Confirmed", MAX);
					clnt->state = 5.1;	// PW 인증
				}

				else {
					write(clnt->sock, "Check your PW", MAX);
					clnt->state = 3;	// 기능입력	
				}
			}
			/* 정보수정 전 PW인증 */


			/* 정보수정 */	
			if (clnt->state == 5.1) {

				usleep(500);
				char change_name[MAX], change_loc[MAX];
				int change_age, result;

				write(clnt->sock, "Change your NAME", MAX);
				read(clnt->sock, message, BUFSIZE);
				strcpy(change_name, message);

				write(clnt->sock, "Change your LOC", MAX);
				read(clnt->sock, message, BUFSIZE);
				strcpy(change_loc, message);

				write(clnt->sock, "Change your AGE", MAX);
				read(clnt->sock, message, BUFSIZE);
				change_age = atoi(message);


				// 정보수정 쿼리
				result = edit_user_info(change_name, change_loc, change_age, now_id);

				if (result == SQLITE_DONE) {			// 수정 성공

					write(clnt->sock, "Edit Success", MAX);
					clnt->state = 3;	// 기능입력
				}

				else if (result == SQLITE_ERROR) {		// 수정 실패
					write(clnt->sock, "Edit Fail", MAX);
					clnt->state = 3;	// 기능입력
				}
			}
			/* 정보수정 */
		}
		/* 클라이언트 정보 수정 */


		/* 전체 채팅 */
		{
			char id[MAX];

			/* 채팅방 입장 */ 
			if (clnt->state == 6) {
				strcpy(id, enter_chat(clnt->sock));
				write(clnt->sock, "Enter Chat Room", BUFSIZE);
				usleep(500);
				clnt->state = 6.1;
			}
			/* 채팅방 입장 */

			/* 채팅 */
			if (clnt->state == 6.1) {
				char enter_msg[BUFSIZE], exit_msg[BUFSIZE], whisper_msg[BUFSIZE], list_msg[BUFSIZE];
				char help_msg[BUFSIZE] = "LIST\tis [/list]\nWHISPER\tis [/w ID]\nQUIT\tis [/quit]2";
				char cmd[MAX], obj[MAX], temp[MAX], *ptr;

				sprintf(enter_msg, "[%s] '%s' has entered room1", get_time("time"), id);
				whole_send_message(enter_msg, BUFSIZE);			// 입장메시지

				/* 채팅 반복 */
				while(1)
				{
					int cnt = 0;
					char msg[BUFSIZE];
					memset(msg, 0, BUFSIZE);
					memset(message, 0, BUFSIZE);
					memset(cmd, 0, MAX);

					read(clnt->sock, message, BUFSIZE);
					sprintf(msg, "[%s] %s : ", get_time("time"), id);
					strcat(msg, message);
					strcat(msg, "3");

					if (!(strcmp(message, "kick"))) {
						sprintf(exit_msg, "[%s] '%s' has been kicked by SERVER1", get_time("time"), id);
						whole_send_message(exit_msg, BUFSIZE);	// 퇴장메시지
						exit_chat(clnt->sock);
						logout_client(clnt->sock);
						remove_client(clnt->sock);
						clnt->state = -1;
						break;
					}

					/* 명령어 파싱 */
					ptr = strtok(message, " ");
					while(ptr)
					{
						if (cnt == 0){				// 명령어 부분
							strcpy(cmd, ptr);
						}

						else if (cnt == 1) {		// 상대방 ID
							strcpy(obj, ptr);
						}

						else if (cnt == 2) {		// 전달할 말
							sprintf(whisper_msg, "[%s] [whisper] %s :", get_time("time"), id);
						}
						strcat(whisper_msg, " ");
						strcat(whisper_msg, ptr);
						cnt++;
						ptr = strtok(NULL, " ");
					}
					/* 명령어 파싱 */

					strcat(whisper_msg, "3");

					/* 도움말 */
					if (!(strcmp(cmd, "/help"))) {
						write(clnt->sock, help_msg, BUFSIZE);
						continue;
					}
					/* 도움말 */


					/* 나가기 */
					if (!(strcmp(cmd, "/quit"))) {
						sprintf(exit_msg, "[%s] '%s' has left room1", get_time("time"), id);
						whole_send_message(exit_msg, BUFSIZE);	// 퇴장메시지
						exit_chat(clnt->sock);
						write(clnt->sock, "/quit", BUFSIZE);
						clnt->state = 3;	// 기능입력
						break;
					}
					/* 나가기 */


					/* 귓속말 */
					if (!(strcmp(cmd, "/w"))) {
						for (i=0; i<chat_number; i++) {

							if (!(strcmp(id, obj))) {						// 자기 자신인 경우
								write(clnt->sock, "IT'S YOU2", MAX);
								break;
							}
							else if (!(strcmp(obj, chat_arr[i]->id))) {		// 귓속말 전달
								write(chat_arr[i]->sock, whisper_msg, MAX);
								break;
							}

							else if (i == chat_number -1) {					// 사용자가 채팅방에 없는 경우
								write(clnt->sock, "NOT HERE2", MAX);
								break;
							}
						}
						continue;
					}
					/* 귓속말 */


					/* 리스트 */
					if (!(strcmp(cmd, "/list"))) {
						memset(list_msg, 0, BUFSIZE);
						sprintf(list_msg, "============= Chatting Member =============\n");
						for (i=0; i<chat_number; i++) {
							memset(temp, 0, BUFSIZE);
							
							if (clnt->sock == chat_arr[i]->sock) {
								sprintf(temp, "ID : %s [me]\n", chat_arr[i]->id);
							}

							else {
								sprintf(temp, "ID : %s\n", chat_arr[i]->id);
							}
							
							strcat(list_msg, temp);
						}
						strcat(list_msg, "2");
						write(clnt->sock, list_msg, BUFSIZE);
						continue;
					}
					/* 리스트 */

					if (cmd[0] == '/') {					// 잘못된 명령어일 경우
						write(clnt->sock, "WRONG CMD2", MAX);
						continue;
					}

					if (chat_number == 1) {					// 채팅방 혼자일때
						write(clnt->sock, "ONLY YOU2", strlen("ONLY YOU2"));
						continue;
					}

					if (!(strcmp(message, ""))) {			// 공백문자는 입력 X
						continue;
					}

					chat_message(msg, BUFSIZE, clnt->sock);
				}
				/* 채팅 반복 */
			}
			/* 채팅 */
		}
		/* 전체 채팅 */	


		/* 로그아웃 */	
		if (clnt->state == 7) {
			logout_client(clnt->sock);	// 로그아웃시 접속자 리스트에서 ID 제거
			clnt->state = 0;	// 메뉴입력
		}
		/* 로그아웃 */
	}
	/* 전체 while */

	close(clnt->sock);

	return 0;
}

void welcome_message()
{
	system("clear");
	printf("+====================================+\n");
	printf("+ Chatting Programming! Made by hoon +\n");
	printf("+ Server : Multi-Thread              +\n");
	printf("+ Client : Multi-Processor           +\n");
	printf("+ Here is Server part                +\n");
	printf("+ HELP is [/help]                    +\n");
	printf("+====================================+\n");
}

void login_client(int sock, char *id)
{
	int i;

	pthread_mutex_lock(&mutx);
	for (i=0; i<clnt_number; i++) {						
		if (sock == clnt_arr[i]->sock) {
			sprintf(log_msg, "[%s] '%s' Login Success\n", get_time("all"), id);
			printf("%s", log_msg);
			add_log(log_msg);

			strcpy(clnt_arr[i]->id, id);						// 로그인시 접속자 리스트 ID추가
			strcpy(clnt_arr[i]->login_time, get_time("all"));	// 로그인 시간도 추가
		}
	}
	pthread_mutex_unlock(&mutx);	
}

void logout_client(int sock)
{
	int i;

	pthread_mutex_lock(&mutx);
	for (i=0; i<clnt_number; i++) {
		if (sock == clnt_arr[i]->sock) {
			sprintf(log_msg, "[%s] '%s' logout\n", get_time("all"), clnt_arr[i]->id);	// 로그아웃시 접속자 리스트 ID제거
			printf("%s", log_msg);
			add_log(log_msg);
			strcpy(clnt_arr[i]->id, "");
		}
	}
	pthread_mutex_unlock(&mutx);
}

char* enter_chat(int sock)
{
	int i;
	char *id;

	pthread_mutex_lock(&mutx);
	for (i=0; i<clnt_number; i++) {
		if (sock == clnt_arr[i]->sock) {
			sprintf(log_msg, "[%s] '%s' enter chat\n", get_time("all"), clnt_arr[i]->id);
			printf("%s", log_msg);
			add_log(log_msg);

			strcpy(id, clnt_arr[i]->id);
			chat_arr[chat_number++] = clnt_arr[i];		// 채팅입장시 채팅 리스트 추가
		}
	}
	pthread_mutex_unlock(&mutx);

	return id;
}

void exit_chat(int sock)
{
	int i;

	pthread_mutex_lock(&mutx);
	for (i=0; i<chat_number; i++) {
		if (sock == chat_arr[i]->sock) {
			sprintf(log_msg, "[%s] '%s' exit chat\n", get_time("all"), clnt_arr[i]->id);
			printf("%s", log_msg);
			add_log(log_msg);

			for ( ; i<chat_number-1; i++) {
				chat_arr[i] = chat_arr[i+1];			// 채팅종료시 채팅 리스트 제거
			}
			break;
		}
	}
	chat_number--;
	pthread_mutex_unlock(&mutx);
}

void add_client(struct client * clnt)
{
	pthread_mutex_lock(&mutx);
	timer = time(NULL);
	t = localtime(&timer);

	sprintf(log_msg, "[%s] Connected! IP : %s\n", get_time("all"), inet_ntoa(clnt->sock_addr.sin_addr));
	printf("%s", log_msg);
	add_log(log_msg);

	clnt_arr[clnt_number++] = clnt;						// 접속시 접속자 리스트 추가
	pthread_mutex_unlock(&mutx);
}

void remove_client(int sock)
{
	int i;

	pthread_mutex_lock(&mutx);
	for (i=0; i<clnt_number; i++) { 
		if (sock == clnt_arr[i]->sock) {
			sprintf(log_msg, "[%s] Disconnected! IP : %s\n", get_time("all"), inet_ntoa(clnt_arr[i]->sock_addr.sin_addr));
			printf("%s", log_msg);
			add_log(log_msg);

			for ( ; i<clnt_number-1; i++) {
				clnt_arr[i] = clnt_arr[i+1];			// 접속종료시 접속자 리스트 제거
			}
			break;
		}
	}
	clnt_number--;
	pthread_mutex_unlock(&mutx);
}

void chat_message(char *message, int len, int sock)
{
	int i;

	pthread_mutex_lock(&mutx);
	for (i=0; i<chat_number; i++) {
		if (sock == chat_arr[i]->sock) {
			continue;
		}
		write(chat_arr[i]->sock, message, len);
	}
	pthread_mutex_unlock(&mutx);

}

void whole_send_message(char *message, int len)
{
	int i;

	pthread_mutex_lock(&mutx);
	for (i=0; i<chat_number; i++) {
		write(chat_arr[i]->sock, message, len);
	}
	pthread_mutex_unlock(&mutx);
}

int check_member_id(char *id)
{
	int id_cnt;
	char *cmd = "select count(*) from user_info where id=?";

	if (sqlite3_prepare(db, cmd, strlen(cmd), &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, id, strlen(id), SQLITE_TRANSIENT);
	}

	sqlite3_step(stmt);
	id_cnt = sqlite3_column_int(stmt, 0);			// 입력한 ID가 있는지 체크
	sqlite3_finalize(stmt);

	return id_cnt;
}

char* find_member_pw(char *id)
{
	char *db_pw;
	char *cmd = "select pw from user_info where id=?";

	if (sqlite3_prepare(db, cmd, strlen(cmd), &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, id, strlen(id), SQLITE_TRANSIENT);
	}

	sqlite3_step(stmt);
	db_pw = (char*)sqlite3_column_text(stmt, 0);	// ID에 따른 PW 반환
	sqlite3_finalize(stmt);

	return db_pw;
}

int insert_join_member(char *id, char *pw, char *name, char *loc, int age)
{
	int insert_result;
	char *cmd = "insert into user_info values(?,?,?,?,?)";

	if (sqlite3_prepare(db, cmd, strlen(cmd), &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, id, strlen(id), SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, pw, strlen(pw), SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, name, strlen(name), SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, loc, strlen(loc), SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, age);						
	}

	insert_result = sqlite3_step(stmt);	// 가입 성공시 SQLITE_DONE, 실패시 SQLITE_ERROR
	sqlite3_finalize(stmt);

	return insert_result; 
}

int edit_user_info(char *name, char *loc, int age, char *id)
{
	int edit_result;
	char *cmd = "update user_info set name=?, loc=?, age=? where id=?";

	if (sqlite3_prepare(db, cmd, strlen(cmd), &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, loc, strlen(loc), SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, age);
		sqlite3_bind_text(stmt, 4, id, strlen(id), SQLITE_TRANSIENT);
	}

	edit_result = sqlite3_step(stmt);	// 수정 성공시 SQLITE_DONE, 실패시 SQLITE_ERROR
	sqlite3_finalize(stmt);

	return edit_result;
}

char* get_time(char* type)
{
	char *now_time = malloc(MAX);

	timer = time(NULL);
	t = localtime(&timer);

	if (!(strcmp(type, "all"))) {		// 날짜 + 시간
		sprintf(now_time, "%04d-%02d-%02d %02d:%02d:%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	}

	else if (!(strcmp(type, "time"))) {	// 시간
		sprintf(now_time, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
	}

	else if (!(strcmp(type, "date"))) {	// 날짜
		sprintf(now_time, "%04d-%02d-%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday);
	}

	return now_time;
}

void add_log(char *log)		// 로그 추가
{
	f = fopen(save_file_name, "a");
	fprintf(f, log);
	fclose(f);
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n',stderr);
	exit(1);
}
