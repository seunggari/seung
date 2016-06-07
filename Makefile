all : server client
	@echo "\033[33m컴파일 완료\033[0m"
	@echo "\033[33mserver부터 실행하시고 client를 접속해주세요\033[0m"
	@echo "\033[33m테스트하기 쉽게 기본 ID가 만들어져 있습니다.\033[0m"
	@echo "\033[36mID : a, PW : a\33[0m"
	@echo "\033[36mID : b, PW : b\33[0m"
	@echo "\033[36mID : c, PW : c\33[0m"
	@echo ""
server : server.c
	clear
	gcc -o s server.c -lsqlite3 -lpthread -lssl -lcrypto
	@echo "\033[33mserver 실행방법 ./s [port]\033[0m"
	@echo ""
client : client.c
	gcc -o c client.c -lssl -lcrypto
	@echo "\033[33mclient 실행방법 ./c [port]\033[0m"
	@echo "\033[33mIP는 기본적으로 127.0.0.1입니다\033[0m"
	@echo ""
clean :
	rm -rf s c
	@echo "\033[33mserver와 client 실행파일을 지웁니다.\033[0m"
