#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

#define BUFLEN 1024
#define MAX_LENGTH_PASS 24
#define MAX_COMMAND_LEN 20
#define MAX_FILENAME_LEN 30
#define MAX_WRONG_LOGIN 3
#define BRUTE_FORCE 8
char success[] = "Success!\n\0";
char FINISH[] = "finish";
char FILE_LIST[] = "filelist";
char ERROR_OPEN_SOCKET[] = "ERROR opening socket";
char ERROR_CONNECT[] = "ERROR connecting";
char ERROR_WRITE_SOCKET[] = "ERROR writing to socket";
char ERROR_CONNECTION_CLOSED[] = "Connection closed\n";
char SHARED[] = "SHARED";
char PRIVATE[] = "PRIVATE";
char brute_force[] = "Brute-force detecat\n";

void error(char *msg) {
    perror(msg);
    exit(0);
}

typedef struct user {
    char username[MAX_LENGTH_PASS];
    char password[MAX_LENGTH_PASS];
} user;

user current_user;
char last_command[MAX_COMMAND_LEN];
char username_introduced[MAX_LENGTH_PASS];
char password_introduced[MAX_LENGTH_PASS];
char last_buffer[BUFLEN];

int main(int argc, char *argv[]) {
    if (argc < 3) {
       fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
       exit(0);
    }

    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    fd_set read_fds, temp_fds; // file descriptors used to know from where to read

    char buffer[BUFLEN];
    char filename[MAX_FILENAME_LEN];
    FILE * fout_log;
    int pid;
    int attemps = 0;
    
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error(ERROR_OPEN_SOCKET);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
        error(ERROR_CONNECT);    

    // initialize sockets (stdin and server)
    FD_ZERO(&read_fds);
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);

    // init empty user
    strcpy(current_user.username, "$");
    
    while(1){
    	temp_fds = read_fds;
    	// clear buffer
    	memset(buffer, 0 , BUFLEN);

    	if (select(sockfd + 1, &temp_fds, NULL, NULL, NULL) >= 0) {    		
    		if (FD_ISSET(0, &temp_fds)) { // input from stdin
    			fgets(buffer, BUFLEN - 1, stdin);
                char buff_aux[BUFLEN];
                strcpy(buff_aux, buffer);

                memset(last_buffer, 0, BUFLEN);
                strcpy(last_buffer, buffer);

                // configure last command introduced
                memset(last_command, 0, MAX_COMMAND_LEN);
                strcpy(last_command, strtok(buff_aux, " \n"));
                if(strcmp(last_command, "login") == 0) {
                    memset(username_introduced, 0, MAX_LENGTH_PASS);
                    memset(password_introduced, 0, MAX_LENGTH_PASS);

                    strcpy(username_introduced, strtok(NULL, " \n"));
                    strcpy(password_introduced, strtok(NULL, " \n"));
                }

                if(strcmp(last_command, "getfilelist") == 0
                   || strcmp(last_command, "upload") == 0
                   || strcmp(last_command, "download") == 0
                   || strcmp(last_command, "quit") == 0) {

                    fout_log = fopen(filename, "a");
                    fprintf(fout_log, "%s%s", current_user.username, last_buffer);
                    fclose(fout_log);
                }

    			// send command to server
    			n = send(sockfd, buffer, strlen(buffer), 0);

    			if (n < 0) 
        	 		error(ERROR_WRITE_SOCKET);
    		}
    		if (FD_ISSET(sockfd, &temp_fds)) {
    			// receiving from server
    			n = recv(sockfd, buffer, BUFLEN, 0);

				if(n <= 0) {
					error(ERROR_CONNECTION_CLOSED);
                    memset(buffer, 0, BUFLEN);
                    break;
                }

                if(strcmp(last_command, "login") == 0) {
                    if(strcmp(buffer, success) == 0) {
                        // reset attemps
                        attemps = 0;

                        // update current user
                        memset(current_user.username, 0, MAX_LENGTH_PASS);
                        strcpy(current_user.username, username_introduced);
                        strcat(current_user.username, "> ");

                        pid = getpid();
                        char pidchar[10];
                        sprintf(pidchar, "%d", pid);

                        // construct log filename
                        memset(filename, 0, MAX_FILENAME_LEN);
                        strcpy(filename, username_introduced);
                        strcat(filename, "-");
                        strcat(filename, pidchar);
                        strcat(filename, ".log");
                    }
                    else {
                        attemps++;

                        if(attemps == MAX_WRONG_LOGIN) {
                            // login failed 
                            char msg[BUFLEN];
                            memset(msg, 0, BUFLEN);
                            int len;

                            sprintf (msg, "%s%d %s", "-", BRUTE_FORCE, brute_force);
                            len = strlen(msg);

                            printf("%s", msg);
                            break;
                        }
                    }
                }
                else if(strcmp(last_command, "logout") == 0) {
                    if(strcmp(buffer, success) == 0) {
                        // update current user
                        memset(current_user.username, 0, MAX_LENGTH_PASS);
                        strcpy(current_user.username, "$");
                    }
                }

                if(strcmp(current_user.username, "$") != 0) {
                    // there is an user loged in, so start filling the log file...  
                    
                    // write in log command and result
                    fout_log = fopen(filename, "a");
                    if(strcmp(last_command, "getfilelist") != 0
                       && strcmp(last_command, "upload") != 0
                       && strcmp(last_command, "download") != 0
                       && strcmp(last_command, "quit") != 0) {

                        fprintf(fout_log, "%s%s", current_user.username, last_buffer);
                    }

                    if(strcmp(buffer, success) != 0) 
                        fprintf(fout_log, "%s\n", buffer);

                    fclose(fout_log);
                }

				printf("%s\n", buffer);
    		}
    	}
    }

    close(sockfd);

    return 0;
}


