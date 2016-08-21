#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

using namespace std;

#define UPLOAD_TYPE 2016
#define DOWNLOAD_TYPE 2017
#define MAX_CLIENTS	5
#define BUFLEN 1024
#define MAX_UPDOWN_BUFLEN 4096
#define MAX_FILENAME_LEN 30
#define MAX_LENGTH_PASS 24
#define MAX_PATH_LEN 50
#define MAX_COMMAND_LEN 20
#define CLI_NOT_AUTH 1
#define OPEN_SESSION 2
#define UP_FAILED 3 // username/password incorrect
#define INEXISTENT_FILE 4
#define FORBIDDEN_DOWN 5
#define ALREADY_SHARED 6
#define ALREADY_PRIVATE 7
#define FILE_ON_SERVER 9
#define FILE_IS_TRANSFERING 10
#define INEXISTENT_USER 11
#define ALREADY_QUIT 12
#define STOP_RECEIVING 13
#define MAX_WAIT_TIME 30 // 30 ms wait time 
char ERROR_OPEN_SOCKET[] = "ERROR opening socket";
char ERROR_CONNECT[] = "ERROR connecting";
char ERROR_WRITE_SOCKET[] = "ERROR writing to socket";
char ERROR_CONNECTION_CLOSED[] = "ERROR Connection closed\n";
char ERROR_BINDING[] = "ERROR on binding";
char ERROR_SELECT[] = "ERROR in select";
char ERROR_ACCEPT[] = "ERROR in accept";
char ERROR_RECV[] = "ERROR in recv";

char success[] = "Success!\n\0";
char wrong_login[] = "User/parola gresita\n";
char open_ses[] = "Sesiune deja deschisa\n";
char client_not_auth[] = "Clientul nu este autentificat\n";
char inexistent_user[] = "Utilizator inexistent\n";
char inexistent_file[] = "Fisier inexistent\n";
char SHARED[] = "SHARED";
char PRIVATE[] = "PRIVATE";
char BYTES[] = " bytes ";
char FINISH[] = "finish";
char FILE_LIST[] = "filelist";
char already_shared_message[] = "Fisier deja partajat\n";
char already_private_message[] = "Fisier deja privat\n";
char file_on_server[] = "Fisier existent pe server\n";
char cannot_download[] = "Descarcare interzisa\n";
char file_is_transfering[] = "Fisier in transfer\n";
char already_quit[] = "Inchidere in curs\n";
char byebye_msg[] = "Bye bye! Sa ai o zi minunata!\n";
char stop_recv[] = "Serverul nu mai poate primi alte comenzi\n";

bool is_already_loged_in = false;
bool wrong_password = false;
bool was_connected = false;
bool server_wants_to_quit = false;

void error(char *msg) {
    perror(msg);
    exit(1);
}

typedef struct user {
	char username[MAX_LENGTH_PASS];
	char password[MAX_LENGTH_PASS];
} user;

typedef struct connected_user {
	int id;
	user current_user;
} connected_user;

typedef struct file {
	char owner[MAX_LENGTH_PASS];
	char type[MAX_FILENAME_LEN];
	char name[BUFLEN];
	long long int dimension;
} file;

typedef struct shared_file {
	char owner[MAX_LENGTH_PASS];
	char name[BUFLEN];
} shared_file;

typedef struct cloud_file {
	file c_file;
	long long int position_left;
	char source[MAX_PATH_LEN];
	char destination[MAX_PATH_LEN];
	int sockfd;
	int type;
} cloud_file;

typedef struct quit_queue {
	int sockfd;
} quit_queue;

void read_users(char *filename, vector<user> &mUsers) {
	ifstream fin_usercfg;
	fin_usercfg.open(filename);

	int N;

	fin_usercfg >> N;

	for(int i = 0; i < N; ++i) {
		user new_user;

		// read username and password from file 
		char username[MAX_LENGTH_PASS], password[MAX_LENGTH_PASS];
		fin_usercfg >> username;
		fin_usercfg >> password;

		strcpy(new_user.username, username);
		strcpy(new_user.password, password);

		mUsers.push_back(new_user);
	}	

	fin_usercfg.close();
}

void read_shared_files(char *filename, vector<shared_file> &mShared_files) {
	ifstream fin_shared_files;
	fin_shared_files.open(filename);

	int M;

	fin_shared_files >> M;

	for(int i = 0; i < M; ++i) {
		char line[MAX_LENGTH_PASS + 1 + BUFLEN];
		memset(line, 0, MAX_LENGTH_PASS + 1 + BUFLEN);
		char username[MAX_LENGTH_PASS], name[BUFLEN];
		memset(username, 0, MAX_LENGTH_PASS);
		memset(name, 0, BUFLEN);

		fin_shared_files >> line;
		strcpy(username, strtok(line, ":"));
		strcpy(name, strtok(NULL, ":"));

		shared_file sh_file;
		strcpy(sh_file.owner, username);
		strcpy(sh_file.name, name);

		mShared_files.push_back(sh_file);
	}	

	fin_shared_files.close();
}

void read_files_from_dir(char *dir_name, vector<file> &mFiles) {
	DIR *d;
  	struct dirent *dir;
  	char path[MAX_PATH_LEN];
  	memset(path, 0, MAX_PATH_LEN);

  	strcpy(path, "./");
  	strcat(path, dir_name);

  	d = opendir(path);
  	if(d) {
	    while ((dir = readdir(d)) != NULL) {
	    	// skip over . and .. files
	    	if(strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
	    		continue;

		    file new_file;
		    int dim = strlen(path) + strlen(dir->d_name) + 1;
		    char filename[dim];

		    memset(filename, 0, dim);
		    strcpy(filename, path);
		    strcat(filename, "/");
		    strcat(filename, dir->d_name);

		    strcpy(new_file.owner, dir_name);
		    strcpy(new_file.name, dir->d_name);

		    // get file dimension
			struct stat st;
			stat(filename, &st);
			new_file.dimension = st.st_size;

			mFiles.push_back(new_file);
	    }

    	closedir(d);
  	}
}

void print_shared_files(const vector<shared_file> &mShared_files) {
	int sh_size = mShared_files.size();

	for(int i = 0; i < sh_size; ++i) {
		cout << mShared_files[i].owner << " " << mShared_files[i].name << "\n";
	}
}

void print_users(const vector<user> &mUsers) {
	int size = mUsers.size();

	for(int i = 0; i < size; ++i) {
		//cout << mUsers[i].username << " " << mUsers[i].password << "\n";
		cout << mUsers[i].username << "\n";
	}
}

void print_connected_users(const vector<connected_user> &mConnected_users) {
	int size = mConnected_users.size();

	for(int i = 0; i < size; ++i) {
		//cout << mUsers[i].username << " " << mUsers[i].password << "\n";
		cout << mConnected_users[i].id << " " << mConnected_users[i].current_user.username << " " <<  mConnected_users[i].current_user.password << "\n";
	}
}

void print_cloud_files(const vector<cloud_file> &mCloud) {
	int c_size = mCloud.size();

	for(int i = 0; i < c_size; ++i)
		cout << mCloud[i].c_file.owner << " " << mCloud[i].c_file.name << " " 
	   		 << mCloud[i].c_file.dimension << " " << mCloud[i].position_left << " " 
	   		 << mCloud[i].source << " " << mCloud[i].destination << "\n";
}

void create_folders(vector<user> &mUsers) {
	int u_size = mUsers.size();

	for(int i = 0; i < u_size; ++i) {
		struct stat st = {0};

		if (stat(mUsers[i].username, &st) == -1) {
	    		mkdir(mUsers[i].username, 0700);
		}
	}
}

bool verify_credentials(int sockfd, user client_user, const vector<user> &mUsers, const vector<connected_user> &mConnected_users) {
	int u_size = mUsers.size();
	int cu_size = mConnected_users.size();
	// need to reset some variables
	wrong_password = false;
	is_already_loged_in = false;

	for(int i = 0; i < cu_size; ++i) {
		if(mConnected_users[i].id == sockfd) {
			is_already_loged_in = true;
			return false;
		}
	}

	for(int i = 0; i < u_size; ++i) {
		if(strcmp(client_user.username, mUsers[i].username) == 0 &&
			strcmp(client_user.password, mUsers[i].password) == 0)
			return true;
	}

	wrong_password = true;
	return false;
}

void logout_user(int sockfd, vector<connected_user> &mConnected_users) {
	int cu_size = mConnected_users.size();
	int i;
	was_connected = false;

	// verify if the user was connected
	// if not send error message
	for(i = 0; i < cu_size; ++i) {
		// user was loged in
		// delete from connected users
		if(mConnected_users[i].id == sockfd) {
			was_connected = true;
			break;
		}
	}

	// delete that user
	if(was_connected) 
		mConnected_users.erase(mConnected_users.begin() + i);
}

bool check_if_connected(int sockfd, const vector<connected_user> &mConnected_users) {
	int cu_size = mConnected_users.size();

	for(int i = 0; i < cu_size; ++i) 
		if(mConnected_users[i].id == sockfd)
			return true;
	
	return false;
}

bool check_existing_user(char *username, const vector<user> &mUsers) {
	int u_size = mUsers.size();

	for(int i = 0; i < u_size; ++i) 
		if(strcmp(mUsers[i].username, username) == 0)
			return true;
	
	return false;
}

void get_username(int sockfd, const vector<connected_user> &mConnected_users, char *username) {
	memset(username, 0, MAX_LENGTH_PASS);
	int cu_size = mConnected_users.size();

	for(int i = 0; i < cu_size; ++i) {
		if(mConnected_users[i].id == sockfd) {
			strcpy(username, mConnected_users[i].current_user.username);
			break;
		}
	}
}

void send_file_list(int sockfd, vector<file> &mFiles, const vector<shared_file> &mShared_files) {
	int f_size = mFiles.size();
	int shf_size = mShared_files.size();
	bool found;

	for(int i = 0; i < f_size; ++i) { // for each files from directory
		found = false;

		//cout << i << " " << mFiles[i].name << " " << mFiles[i].owner << " " << mFiles[i].dimension << "\n";

		for(int j = 0; j < shf_size; ++j) { // for each shared files
			if(strcmp(mFiles[i].name, mShared_files[j].name) == 0) { // means this is a shared file
				strcpy(mFiles[i].type, SHARED);	
				found = true;
				break;
			}		
		}

		if(!found)
			strcpy(mFiles[i].type, PRIVATE);

		// concat the final message that needs to be sent
		char dim[15];
		sprintf(dim, "%lld", mFiles[i].dimension);
 
		char message[BUFLEN];
		memset(message, 0, BUFLEN);

		strcpy(message, mFiles[i].name);
		strcat(message, " ");
		strcat(message, dim);
		strcat(message, BYTES);
		strcat(message, mFiles[i].type);

		if(i == f_size - 1)
			strcat(message, "\n");

		cout << message;

		send(sockfd, message, strlen(message), 0);
	}
}

bool check_existing_file(char *filename, const vector<file> &mFiles) {
	int f_size = mFiles.size();

	for(int i = 0; i < f_size; ++i) 
		if(strcmp(mFiles[i].name, filename) == 0)
			return true;
	
	return false;
}

bool check_already_shared(char *filename, const vector<shared_file> &mShared_files) {
	int f_size = mShared_files.size();

	for(int i = 0; i < f_size; ++i) 
		if(strcmp(mShared_files[i].name, filename) == 0)
			return true;
	
	return false;
} 

bool check_already_private(char *filename, const vector<shared_file> &mShared_files) {
	int f_size = mShared_files.size();

	for(int i = 0; i < f_size; ++i) 
		if(strcmp(mShared_files[i].name, filename) == 0)
			return false;
	
	return true;
} 

void share_file(char *filename, vector<shared_file> &mShared_files, const vector<file> &mFiles) {
	int f_size = mFiles.size();
	int shf_size = mShared_files.size();
	file aux_file;

	for(int i = 0; i < f_size; ++i) {
		if(strcmp(mFiles[i].name, filename) == 0) {
			strcpy(aux_file.name, filename);
			strcpy(aux_file.owner, mFiles[i].owner);
			break;
		}
	}

	shared_file new_sh_file;
	strcpy(new_sh_file.name, aux_file.name);
	strcpy(new_sh_file.owner, aux_file.owner);

	mShared_files.push_back(new_sh_file);
}

void unshare_file(char *filename, vector<shared_file> &mShared_files) {
	int shf_size = mShared_files.size();
	int i;

	for(i = 0; i < shf_size; ++i) {
		if(strcmp(mShared_files[i].name, filename) == 0) 
			break;
	}

	mShared_files.erase(mShared_files.begin() + i);
}

void update_shared_file(char *shared_filename, vector<shared_file> &mShared_files) {
	int shf_size = mShared_files.size();
	ofstream fout;
	fout.open(shared_filename);

	if(fout.is_open()) {
		fout << shf_size << "\n";

		for(int i = 0; i < shf_size; ++i) {
			fout << mShared_files[i].owner << ":" << mShared_files[i].name << "\n";
		}
	}

	fout.close();
}

void delete_file(char *filename, char *username, vector<shared_file> &mShared_files, char *shared_filename) {
	int shf_size = mShared_files.size();
	int i;
	bool found = false;

	char path[MAX_PATH_LEN];
	memset(path, 0, MAX_PATH_LEN);

  	strcpy(path, "./");
  	strcat(path, username);
  	strcat(path, "/");
  	strcat(path, filename);  	

  	for(i = 0; i < shf_size; ++i) 
  		if(strcmp(mShared_files[i].name, filename) == 0) {
  			found = true;
  			break;
  		}
  	
  	// erase from shared_files
  	if(found) {
  		mShared_files.erase(mShared_files.begin() + i);

  		update_shared_file(shared_filename, mShared_files);
  	}

  	unlink(path); // delete that file
}

bool check_file_current_dir(char *filename) {
	DIR *d;
  	struct dirent *dir;
  	char path[] = "."; // check if file exists in current directory

  	d = opendir(path);
  	if(d) {
	    while ((dir = readdir(d)) != NULL) {
	    	// skip over . and .. files
	    	if(strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
	    		continue;

		    if(strcmp(dir->d_name, filename) == 0) 
		    	return true;
	    }

    	closedir(d);
  	}

  	return false;
}

void upload(int sockfd, char *filename, char *username, vector<cloud_file> &mCloud) {
	cloud_file cfile;

	cfile.position_left = 0;
	strcpy(cfile.source, "./");
	strcat(cfile.source, filename);

	strcpy(cfile.destination, "./");
	strcat(cfile.destination, username);
	strcat(cfile.destination, "/");
	strcat(cfile.destination, filename);

	strcpy(cfile.c_file.owner, username);
	strcpy(cfile.c_file.name, filename);

	cfile.c_file.dimension = 0;
	cfile.sockfd = sockfd;
	cfile.type = UPLOAD_TYPE;

	mCloud.push_back(cfile);
}

bool can_download(char *filename, const vector<shared_file> &mShared_files) {
	int shf_size = mShared_files.size();

	for(int i = 0; i < shf_size; ++i) 
		if(strcmp(mShared_files[i].name, filename) == 0)
			return true;
	
	return false;
}

// username is source and current_username is destination
void download(int sockfd, char *filename, char *username, char *current_username, vector<cloud_file> &mCloud) {
	cloud_file cfile;
	char pid[10];
	sprintf(pid, "%d", sockfd);

	cfile.position_left = 0;
	strcpy(cfile.source, "./");
	strcat(cfile.source, username);
	strcat(cfile.source, "/");
	strcat(cfile.source, filename);

	strcpy(cfile.destination, "./");
	strcat(cfile.destination, current_username);
	strcat(cfile.destination, "/");
	strcat(cfile.destination, pid);
	strcat(cfile.destination, "_");
	strcat(cfile.destination, filename);

	strcpy(cfile.c_file.owner, current_username);
	strcpy(cfile.c_file.name, pid);
	strcat(cfile.c_file.name, "_");
	strcat(cfile.c_file.name, filename);

	cfile.c_file.dimension = 0;
	cfile.sockfd = sockfd;
	cfile.type = DOWNLOAD_TYPE;

	mCloud.push_back(cfile);
}

bool check_in_progress(int sockfd, char *filename, vector<cloud_file> &mCloud) {
	int c_size = mCloud.size();
	char pid[10];
	char pid_filename[MAX_FILENAME_LEN];
	memset(pid, 0, 10);
	memset(pid_filename, 0, MAX_FILENAME_LEN);

	sprintf(pid, "%d", sockfd);
	strcpy(pid_filename, pid);
	strcat(pid_filename, "_");
	strcat(pid_filename, filename);

	for(int i = 0; i < c_size; ++i) 
		if(mCloud[i].sockfd == sockfd && (strcmp(mCloud[i].c_file.name, pid_filename) == 0 || strcmp(mCloud[i].c_file.name, filename) == 0)) 
			return true;
	
	return false;
}

void execute_updown(vector<cloud_file> &mCloud) {
	// start executing the first command from cloud 
	FILE *fin;
	FILE *fout;
	fin = fopen(mCloud[0].source, "rb");
	fout = fopen(mCloud[0].destination, "ab");

	fseek(fin, mCloud[0].position_left, SEEK_SET);
	
	char buffer[MAX_UPDOWN_BUFLEN];
	memset(buffer, 0, MAX_UPDOWN_BUFLEN);
	int frlen = fread(buffer, sizeof(char), MAX_UPDOWN_BUFLEN, fin);

	if(frlen <= 0) {
		char msg[150];
		memset(msg, 0, 150);
		char dimension[15];
		memset(dimension, 0, 15);

		if(mCloud[0].position_left <= MAX_UPDOWN_BUFLEN)
			mCloud[0].position_left += sizeof(buffer) / sizeof(buffer[0]);

		sprintf(dimension, "%lld", mCloud[0].position_left);

		if(mCloud[0].type == UPLOAD_TYPE) 
			strcpy(msg, "Upload finished: ");		
		else if(mCloud[0].type == DOWNLOAD_TYPE) 
			strcpy(msg, "Download finished: ");

		strcat(msg, mCloud[0].c_file.name);
		strcat(msg, " - ");
		strcat(msg, dimension);
		strcat(msg, BYTES);
		strcat(msg, "\n");

		send(mCloud[0].sockfd, msg, 150, 0);

		mCloud.erase(mCloud.begin()); // erase first element if finished
	}
	else {
		fwrite(buffer , sizeof(char), sizeof(buffer), fout);

		mCloud[0].position_left += sizeof(buffer) / sizeof(buffer[0]);
		cloud_file aux;

		aux.position_left = mCloud[0].position_left;
		strcpy(aux.source, mCloud[0].source);
		strcpy(aux.destination, mCloud[0].destination);
		aux.sockfd = mCloud[0].sockfd;
		aux.type = mCloud[0].type;
		strcpy(aux.c_file.owner, mCloud[0].c_file.owner);
		strcpy(aux.c_file.name, mCloud[0].c_file.name);

		mCloud.erase(mCloud.begin()); // erase first element
		mCloud.push_back(aux); // move to end
	}

	fclose(fin);
	fclose(fout);
}

bool check_is_transfering(int sockfd, vector<cloud_file> &mCloud) {
	int c_size = mCloud.size();

	for(int i = 0; i < c_size; ++i)
		if(mCloud[i].sockfd == sockfd)
			return true;

	return false;
}

void quit_connection(vector<quit_queue> &mQuit_queue, vector<cloud_file> &mCloud, vector<connected_user> &mConnected_users, fd_set *read_fds) {
	int pos;
	int q_size = mQuit_queue.size();
	vector<int> pos_q;

	for(int i = 0; i < q_size; ++i) {
		bool check_prog = check_is_transfering(mQuit_queue[i].sockfd, mCloud);
		if(check_prog)
			continue;

		// quit connection with user
		int cu_size = mConnected_users.size();

		for(int j = 0; j < cu_size; ++j) {
			if(mConnected_users[j].id == mQuit_queue[i].sockfd) {
				pos = j;
				break;
			}
		}

		mConnected_users.erase(mConnected_users.begin() + pos); // update connected users list
		pos_q.push_back(i);

		int len = strlen(byebye_msg);
		send(mQuit_queue[i].sockfd, byebye_msg, len, 0);

		close(mQuit_queue[i].sockfd); 
		FD_CLR(mQuit_queue[i].sockfd, read_fds); // clear that file descriptor from read_fds
	}

	// clear disconected users from queue
	int pos_q_size = pos_q.size();
	for(int i = 0; i < pos_q_size; ++i) 
		mQuit_queue.erase(mQuit_queue.begin() + pos_q[i]);
}

bool queue_contains(int sockfd, vector<quit_queue> &mQuit_queue) {
	int q_size = mQuit_queue.size();

	for(int i = 0; i < q_size; ++i)
		if(mQuit_queue[i].sockfd == sockfd)
			return true;

	return false;
}

int main(int argc, char **argv) {
	if (argc < 4) {
        fprintf(stderr,"Argument number insufficient\n");
        exit(1);
    }

    // read existing users from config file
    vector<user> mUsers;
	read_users(argv[2], mUsers);
	int users_size = mUsers.size();
	vector<connected_user> mConnected_users;
	create_folders(mUsers); // creates the neccessary folders for clients on server

	vector<shared_file> mShared_files;
	read_shared_files(argv[3], mShared_files);
	int shared_size = mShared_files.size();	

	vector<cloud_file> mCloud;
	vector<quit_queue> mQuit_queue;

	struct timeval timeout;

	int sockfd, newsockfd, portno, clilen;
	char buffer[BUFLEN];
	char message[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, j;

    fd_set read_fds; // read set used in select()
    fd_set tmp_fds;	 // temporary set 
    int fdmax;		 // maximum value file descriptor for read_fds

    // empty the read descriptors set (read_fds and tmp_fds) 
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
     
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
       error(ERROR_OPEN_SOCKET);
     
    portno = atoi(argv[1]);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;	// uses the IP of machine
    serv_addr.sin_port = htons(portno);
     
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) 
        error(ERROR_BINDING);
     
    listen(sockfd, MAX_CLIENTS); // starts listening to clients

    // add the new file descriptor in read_fds (the listening socket)
    FD_SET(sockfd, &read_fds);
    FD_SET(0, &read_fds); // add socket for stdin
    fdmax = sockfd;

    // main loop
	while (1) {
		tmp_fds = read_fds; 
		timeout.tv_sec = 0; 
    	timeout.tv_usec = MAX_WAIT_TIME;

		if(select(fdmax + 1, &tmp_fds, NULL, NULL, &timeout) <= 0) {
			// execute downloads and uploads in background
			if(mCloud.size() != 0) {
				execute_updown(mCloud);
			}
			if(mQuit_queue.size() != 0){
				quit_connection(mQuit_queue, mCloud, mConnected_users, &read_fds);
			}
			if(server_wants_to_quit)
				if(mCloud.size() == 0)
					break;
			continue;
		} 
	
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
			
				if (i == sockfd) {
					// there is a client that wants to connect
					// so we need to accept that connection request
					clilen = sizeof(cli_addr);

					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen)) == -1) {
						error(ERROR_ACCEPT);
					} 
					else {
						// add the new socket file descriptor to read_fds
						FD_SET(newsockfd, &read_fds);
						// update maximum file descriptors if necessary
						if (newsockfd > fdmax) 
							fdmax = newsockfd;
					}
					printf("New connection from %s, port %d, socket_client %d\n ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				}
				else if (i == 0) {
					// server received something from stdin
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);

					char command[MAX_COMMAND_LEN];
					strcpy(command, strtok(buffer, " \n"));

					if(strcmp(command, "quit") == 0) {
						// server wants to go to sleep
						cout << "Server wants to quit\n";
						server_wants_to_quit = true;
					}
					else {
						cout << "Unknown command\n";
					}
				}
				else {
					// we received something from clients
					memset(buffer, 0, BUFLEN);

					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							// the connection is closed
							printf("selectserver: socket %d hung up\n", i);
						} else {
							error(ERROR_RECV);
						}
						close(i); 
						FD_CLR(i, &read_fds); // clear that file descriptor from read_fds
					} 
					else { 
						// we actually got something
						printf ("Got from client %d, the message: %s\n", i, buffer);
						char msg[BUFLEN];
						memset(msg, 0, BUFLEN);
						int len = 0;

						if(!server_wants_to_quit) {

							char command[MAX_COMMAND_LEN];
							strcpy(command, strtok(buffer, " \n"));

							if(strcmp(command, "login") == 0) {
								user client_user;
								strcpy(client_user.username, strtok(NULL, " \n"));
								strcpy(client_user.password, strtok(NULL, " \n"));

								bool check_login = verify_credentials(i, client_user, mUsers, mConnected_users);

								if(check_login) {
									connected_user con_user;
									con_user.id = i; // client's socket descriptor
									con_user.current_user = client_user;

									// add current client to connected users
									mConnected_users.push_back(con_user);
									
									len = strlen(success);

									// send welcome to that user
									send(i, success, len, 0);
								}
								else {
									if(wrong_password) {
										// login failed 
										sprintf (msg, "%s%d %s", "-", UP_FAILED, wrong_login);
										len = strlen(msg);
									}
									else if(is_already_loged_in) {
										// login failed 
										sprintf (msg, "%s%d %s", "-", OPEN_SESSION, open_ses);
										len = strlen(msg);
									}

									// send login failed message to that user
									send(i, msg, len, 0);
								}
							}
							else if(strcmp(command, "logout") == 0) {
								logout_user(i, mConnected_users);

								if(was_connected) {
									// logout success
									len = strlen(success);
									// send success to that user
									send(i, success, len, 0);
								}
								else {
									// logout failed 
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									// send logout failed message to that user
									send(i, msg, len, 0);
								}
							} 
							else if(strcmp(command, "getuserlist") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char users_list[MAX_CLIENTS * (MAX_LENGTH_PASS + 1)];
									memset(users_list, 0, MAX_CLIENTS * (MAX_LENGTH_PASS + 1));
									strcpy(users_list, mUsers[0].username); // copy first username
									strcat(users_list, "\n");

									for(int k = 1; k < users_size; ++k) {
										strcat(users_list, mUsers[k].username);
										strcat(users_list, "\n");
									}

									len = strlen(users_list);
									// send logout failed message to that user
									send(i, users_list, len, 0);
								}
							}
							else if(strcmp(command, "getfilelist") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char username[MAX_LENGTH_PASS];
									memset(username, 0, MAX_LENGTH_PASS);
									strcpy(username, strtok(NULL, " \n"));

									bool user_exists = check_existing_user(username, mUsers);
									if(!user_exists) {
										// send user inexistent message
										sprintf (msg, "%s%d %s", "-", INEXISTENT_USER, inexistent_user);
										len = strlen(msg);
										send(i, msg, len, 0);
									}
									else {
										vector<file> mFiles;
										read_files_from_dir(username, mFiles);
										send_file_list(i, mFiles, mShared_files);
									}
								}
							}
							else if(strcmp(command, "upload") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char filename[MAX_FILENAME_LEN];
									memset(filename, 0, MAX_FILENAME_LEN);
									strcpy(filename, strtok(NULL, " \n"));

									bool file_exists = check_file_current_dir(filename);
									if(!file_exists) {
										// send file inexistent message
										sprintf (msg, "%s%d %s", "-", INEXISTENT_FILE, inexistent_file);
										len = strlen(msg);
										send(i, msg, len, 0);
									}
									else {
										vector<file> mFiles;
										char username[MAX_LENGTH_PASS];
										memset(username, 0, MAX_LENGTH_PASS);
										get_username(i, mConnected_users, username);
											
										read_files_from_dir(username, mFiles);

										bool in_prog = check_in_progress(i, filename, mCloud);
										if(in_prog) {
											// send file inexistent message
											sprintf (msg, "%s%d %s", "-", FILE_IS_TRANSFERING, file_is_transfering);
											len = strlen(msg);
											send(i, msg, len, 0);
										}
										else {
											bool file_exists_on_server = check_existing_file(filename, mFiles);
											if(file_exists_on_server) {
												// send file already on server message
												sprintf (msg, "%s%d %s", "-", FILE_ON_SERVER, file_on_server);
												len = strlen(msg);
												send(i, msg, len, 0);
											}
											else {
												upload(i, filename, username, mCloud);
												// upload succeeded
												len = strlen(success);
												// send success to that user
												send(i, success, len, 0);
											}
										}
									}
								}
							}
							else if(strcmp(command, "download") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char username[MAX_LENGTH_PASS];
									memset(username, 0, MAX_LENGTH_PASS);
									strcpy(username, strtok(NULL, " \n"));

									char filename[MAX_FILENAME_LEN];
									memset(filename, 0, MAX_FILENAME_LEN);
									strcpy(filename, strtok(NULL, " \n"));

									vector<file> mFiles;									
									read_files_from_dir(username, mFiles);

									bool in_prog = check_in_progress(i, filename, mCloud);
									if(in_prog) {
										// send file in progress message
										sprintf (msg, "%s%d %s", "-", FILE_IS_TRANSFERING, file_is_transfering);
										len = strlen(msg);
										send(i, msg, len, 0);
									}
									else {
										bool file_exists = check_existing_file(filename, mFiles);
										if(!file_exists) {
											// send file inexistent message
											sprintf (msg, "%s%d %s", "-", INEXISTENT_FILE, inexistent_file);
											len = strlen(msg);
											send(i, msg, len, 0);
										}
										else {
											bool can_down = can_download(filename, mShared_files); 
											if(!can_down) {
												// send file can not be downloaded message
												sprintf (msg, "%s%d %s", "-", FORBIDDEN_DOWN, cannot_download);
												len = strlen(msg);
												send(i, msg, len, 0);
											}
											else {
												char current_username[MAX_LENGTH_PASS];
												memset(current_username, 0, MAX_LENGTH_PASS);
												get_username(i, mConnected_users, current_username);

												download(i, filename, username, current_username, mCloud);
												// download succeeded
												len = strlen(success);
												// send success to that user
												send(i, success, len, 0);
											}
										}
									}
								}
							}
							else if(strcmp(command, "share") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char filename[MAX_FILENAME_LEN];
									memset(filename, 0, MAX_FILENAME_LEN);
									strcpy(filename, strtok(NULL, " \n"));

									vector<file> mFiles;
									char username[MAX_LENGTH_PASS];
									memset(username, 0, MAX_LENGTH_PASS);
									get_username(i, mConnected_users, username);
										
									read_files_from_dir(username, mFiles);

									bool file_exists = check_existing_file(filename, mFiles);
									if(!file_exists) {
										// send file inexistent message
										sprintf (msg, "%s%d %s", "-", INEXISTENT_FILE, inexistent_file);
										len = strlen(msg);
										send(i, msg, len, 0);
									}
									else {
										bool already_shared = check_already_shared(filename, mShared_files);
										if(already_shared) {
											// send already shared message
											sprintf (msg, "%s%d %s", "-", ALREADY_SHARED, already_shared_message);
											len = strlen(msg);
											send(i, msg, len, 0);
										}
										else {
											share_file(filename, mShared_files, mFiles);
											// shared succeeded
											len = strlen(success);
											// send success to that user
											send(i, success, len, 0);										
										}
									}
								}
							}
							else if(strcmp(command, "unshare") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char filename[MAX_FILENAME_LEN];
									memset(filename, 0, MAX_FILENAME_LEN);
									strcpy(filename, strtok(NULL, " \n"));

									vector<file> mFiles;
									char username[MAX_LENGTH_PASS];
									memset(username, 0, MAX_LENGTH_PASS);
									get_username(i, mConnected_users, username);
										
									read_files_from_dir(username, mFiles);

									bool file_exists = check_existing_file(filename, mFiles);
									if(!file_exists) {
										// send file inexistent message
										sprintf (msg, "%s%d %s", "-", INEXISTENT_FILE, inexistent_file);
										len = strlen(msg);
										send(i, msg, len, 0);
									}
									else {
										bool already_private = check_already_private(filename, mShared_files);
										if(already_private) {
											// send already shared message
											sprintf (msg, "%s%d %s", "-", ALREADY_PRIVATE, already_private_message);
											len = strlen(msg);
											send(i, msg, len, 0);
										}
										else {
											unshare_file(filename, mShared_files);
											// unshared succeeded
											len = strlen(success);
											// send success to that user
											send(i, success, len, 0);										
										}
									}
								}
							}
							else if(strcmp(command, "delete") == 0) {
								bool check_connection = check_if_connected(i, mConnected_users);

								if(!check_connection) {
									// send client not authenticated message
									sprintf (msg, "%s%d %s", "-", CLI_NOT_AUTH, client_not_auth);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									char filename[MAX_FILENAME_LEN];
									memset(filename, 0, MAX_FILENAME_LEN);
									strcpy(filename, strtok(NULL, " \n"));

									vector<file> mFiles;
									char username[MAX_LENGTH_PASS];
									memset(username, 0, MAX_LENGTH_PASS);
									get_username(i, mConnected_users, username);
										
									read_files_from_dir(username, mFiles);

									bool in_prog = check_in_progress(i, filename, mCloud);
									if(in_prog) {
										// send file inexistent message
										sprintf (msg, "%s%d %s", "-", FILE_IS_TRANSFERING, file_is_transfering);
										len = strlen(msg);
										send(i, msg, len, 0);
									}
									else {
										bool file_exists = check_existing_file(filename, mFiles);
										if(!file_exists) {
											// send file inexistent message
											sprintf (msg, "%s%d %s", "-", INEXISTENT_FILE, inexistent_file);
											len = strlen(msg);
											send(i, msg, len, 0);
										}
										else {
											delete_file(filename, username, mShared_files, argv[3]);
											// delete succeeded
											len = strlen(success);
											// send success to that user
											send(i, success, len, 0);
										}
									}
								}
							}
							else if(strcmp(command, "quit") == 0) {
								bool q_contains = queue_contains(i, mQuit_queue);
								if(q_contains) {
									// send already in quit queue message
									sprintf (msg, "%s%d %s", "-", ALREADY_QUIT, already_quit);
									len = strlen(msg);
									send(i, msg, len, 0);
								}
								else {
									quit_queue qq;
									qq.sockfd = i;
									mQuit_queue.push_back(qq);
								}
							}
							else {
								printf("Unknown command\n");
							}
						}
						else {
							// server does not receive commands anymore 
							sprintf (msg, "%s%d %s", "-", STOP_RECEIVING, stop_recv);
							len = strlen(msg);

							send(i, msg, len, 0);
						}
					}
				} 
			}
		}
     }


     close(sockfd);
   
     return 0; 
}


