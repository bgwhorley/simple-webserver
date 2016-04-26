
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";

char* protocol = "HTTP/1.1";

void processRequest( int socket);
void sendNotFound(int socket, char* protocol);
bool endsWith(char* path, char* type);

int main( int argc, char **argv) {
	
	//print usage if not enough arguments
	if(argc < 2){
		fprintf(stderr, "%s", usage);
		exit(-1);
	}

	int port = atoi(argv[1]);

	struct sockaddr_in serverIPAddress; 
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);

	int optval = 1;
	int QueueLength = 5;

	//open passive socket
	int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (masterSocket < 0){
		perror("socket");
		exit(-1);
	}

	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(int));
	int error = bind(masterSocket, (struct sockaddr *)&serverIPAddress, sizeof(serverIPAddress));
	if(error) {
		perror("bind");
		exit(-1);
	}

	error = listen( masterSocket, QueueLength );
	if(error) {
		perror("listen");
		exit(-1);
	}

	while(1) {
		//accept a new TCP connection
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);

		if ( slaveSocket < 0 ) {
			perror( "accept" );
			exit( -1 );
		}
		//read request from TCP connection and parse it
		processRequest(slaveSocket);
		close(slaveSocket);

	}
   

}

void processRequest( int socket ){
	int n = 0; int length = 0; int gotGET = 0; int gotDOC = 0;
	char newChar; char oldChar; char curr_string[1024]; curr_string[0] = '\0';
	char docpath[1024]; docpath[0] = '\0';

	char contentType[1024]; contentType[0] = '\0';
	char filepath[1024]; filepath[0]= '\0';
	while((n = read(socket, &newChar, sizeof(newChar))) > 0){
		length++;
		//printf("newChar: %c\n", newChar);
		if(newChar == ' '){
			//if I have not seen GET
			if(gotGET == 0){
				if(strstr(curr_string, "GET")){
					gotGET = 1;
					//printf("got get\n");
					length = 0;
				}
			}
			////else if I have not seen docPath
			else if(gotDOC == 0 && gotGET == 1) {
				//printf("getting docpath\n");
				curr_string[length-1]='\0';
				strcpy(docpath, curr_string);
				gotDOC = 1;
				printf("docpath: %s\n", docpath);
			}
			length = 0;
		}
		else if (newChar == '\n' && oldChar == '\r') {
			break;		
		}
		else {
			//printf("length: %d\n", length);
			oldChar = newChar;
			curr_string[length-1] = newChar;
			curr_string[length] = '\0';

			printf("current_string: %s\n", curr_string);
		}
	}

	printf("docpath: (%s)\n", docpath);
	int doclen = strlen(docpath);
	printf("docpath length: %d\n", doclen);

	char cwd[1024];
	cwd[0] = '\0';
	getcwd(cwd, 1024);
	
	if(strstr(docpath, "/icons") || strstr(docpath, "/htdocs")) {
		printf("icons/htdocs\n");
		strcat(cwd, "/http-root-dir");
		strcat(cwd, docpath);
		strcpy(filepath, cwd);
	}
	else if((docpath[0] == '/' && docpath[1] == '\0') || docpath[0] == '\0'){
		printf("slash\n");
		strcpy(filepath, cwd);
		strcat(filepath, "/http-root-dir/htdocs/index.html");
	}
	else {
		strcpy(filepath, cwd);
		strcat(filepath, "/http-root-dir/htdocs");
		strcat(filepath, docpath);
	}
	printf("filepath: %s\n",filepath);

	//expand path and find if .. was in the path (not allowed)
	char resolved_path[1024]; resolved_path[0] = 0;
	realpath(filepath, resolved_path);
	printf("filepath: %s\n", filepath);
	printf("resolved_path: %s\n", resolved_path);
	if(strlen(resolved_path) < strlen(filepath)){
		//print error if the path is shorter
		sendNotFound(socket, protocol);
		return;
	}

	printf("check extensions\n");
	//determine the content type
	if(endsWith(filepath, ".html") || endsWith(filepath, ".html/")) {
		printf("here\n");
		strcpy(contentType, "text/html");
		printf("after\n");
	}
	else if(endsWith(filepath, ".gif") || endsWith(filepath, ".gif/")){
		printf("there\n");
		strcpy(contentType, "image/gif");
	}
	else {
		printf("anywhere\n");
		strcpy(contentType, "text/plain");
	}

	printf("printing content type\n");
	printf("content type: %s\n", contentType);

	//open the file
	int file = open(filepath, O_RDONLY);
	if(file < 0){
		//send error 404
		printf("bad file\n");	
		sendNotFound(socket, protocol);
		return;
	}
	else {
		char header[1024];
		printf("file opened\n");
		const char* serverType = "test";
		char* h = "HTTP/1.1 200 Document follows\r\nServer: bwhorley\r\nContent-type: ";
		strcat(header, h);
		strcat(header, contentType);
		strcat(header, "\r\n\r\n");
		write(socket, header, strlen(header));
		int count = 0;
		char* buff[1];
		while((count = read(file, buff, 1) > 0)){
			if(write(socket, buff, 1) != count){
				perror("write");
				break;
			}
		}
		printf("end while\n");
	}

	close(file);
}

void sendNotFound(int socket, char* protocol ){
	const char* notFound = "File Not Found";
	const char* serverType = "TEST";
	write(socket, protocol, strlen(protocol));
	write(socket, " ", 1);
	write(socket, "404 File Not Found\r\n", 17);
	write(socket, "Server:", 7);
	write(socket, " ", 1);
	write(socket, serverType, strlen(serverType));
	write(socket, "\r\n", 2);
	write(socket, "Content-type:", 13);
	write(socket, " ", 1);
	write(socket, "text/html", 9);
	write(socket, "\r\n", 2);
	write(socket, "\r\n", 2);
	write(socket, notFound, strlen(notFound));
}

bool endsWith(char* path, char* type){
	char* match = strstr(path, type);
	if(match) {
		return true;
	}
	return false;
}


