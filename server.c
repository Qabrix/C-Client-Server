#include <stdio.h>  
#include <string.h>  
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>  
#include <arpa/inet.h>    
#include <sys/types.h>  
#include <sys/socket.h> 
#include <sys/wait.h>
#include <netinet/in.h>  
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#define PORT 59090 

int server_fd, valread, opt_set, serv_bind, listener;
struct sockaddr_in address;
int opt = -1;
int addrlength = sizeof(address);
int maxclients=64;
int clientSocket[64];
int clients=0;
fd_set readfds;
int maxSocketd;
int activity;
int newSocket;
int socketd;
char *cmdBuff[512];
int cmdBufferIndex;
char paths[128][512];
char passBuf[128];
char *password = "psswrd";
struct stat fileStat;
ssize_t length;
off_t offset;
char buff[2048];
char bufferR[2048];

void presetSockets(){
    for(int i=0; i<maxclients; i++){
        clientSocket[i]=0;
    }
}

void setSocket(){
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd==0){
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
}

void setSocketOptions(){
    opt_set = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(opt_set==-1){
        perror("Port attaching failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family=AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
}

void performBinding(){
    serv_bind = bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    if(serv_bind<0){
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }
}

void startListening(){
    listener = listen(server_fd, 3);
    if(listener<0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}

void readFromClient(int socket){    
    read(socket, buff, 512);
}

void writeFromBuffer(){
    printf("%s\n", buff);
}

void sendMess(int socket, char *message){
    send(socket, message, strlen(message), 0);
}

void clearBuffAndCmdBuff(){
     for(int i=0; i<512; i++){
         buff[i]='\0';
         cmdBuff[i]='\0';
     }
     cmdBufferIndex = 0;
}

void processCommand(){
    int buffer_size=strlen(buff);
    cmdBufferIndex=1;
    cmdBuff[0]=&buff[0];
    for(int i=0; i<buffer_size; i++){
        if(buff[i]=='\0'){
            cmdBuff[cmdBufferIndex]=NULL;
            return;
        }
        if(buff[i]=='\n'){
            buff[i]='\0';
        }
        if(buff[i]==' '){
            buff[i]='\0';
            if(buff[i+1]!=' '){
                cmdBuff[cmdBufferIndex]=&buff[i+1];
                cmdBufferIndex++;
            }
        }
    }
}

void WriteCmdBuffer(){
    int i=0;
    while(cmdBuff[i]!=NULL){
        printf("%s \n", cmdBuff[i]);
        i++;
    }
}

void executeCommand(int socket){
    int t;
    int myfork = fork();
    if(myfork==0){
        close(0);
        dup2(socket, 1);
        execvp(cmdBuff[0], cmdBuff);
        exit(1);
    }
    else if(myfork>0){
        wait(&t);
        return;
    }   
}

void changeDirectory(int i){
    chdir(cmdBuff[1]);
    char buff[256];
    getcwd(buff, sizeof(buff));
    memcpy(paths[i], buff, sizeof(buff));

}

void sendPath(int socket, int i){
    char *result = malloc(strlen(paths[i]) + 2);
    strcpy(result, paths[i]);
    strcat(result, "\n");
    send(socket, result, strlen(result), 0);
}

void sendError(int socket){
    char *error = "Cmd not found\n";
    send(socket, error, strlen(error), 0);
}

void clearPassBuf(){
    for(int i=0; i<64; i++){
        passBuf[i]='\0';
    }
}

void getClientsPath(int i){
    chdir(paths[i]);
}

void clearBuffReader(){
    for(int i=0; i<512; i++){
        bufferR[i]='\0';
    }
}

void disconnectClient(int i){
    getpeername(socketd, (struct sockaddr*)&address, (socklen_t*)&addrlength);   
    printf("Client with port %d has disconnected\n", ntohs(address.sin_port));
    close(socketd);   
    clientSocket[i] = 0;
}

void connectCLient(){
    printf("Client connected:  FD %d IP %s Port %d \n" , newSocket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
        sendMess(newSocket, "\n\n\u06DE  Connected to server \u06DE\n\n");
        for (int i = 0; i < maxclients; i++){   
            if(clientSocket[i] == 0){   
                clientSocket[i] = newSocket;
                break;   
            }   
        }
}

void performCmd(int i){
    buff[valread] = '\0';
    getClientsPath(i);
    processCommand();
    if((strcmp(cmdBuff[0], "ls")==0) || (strcmp(cmdBuff[0], "pwd")==0)){
        executeCommand(socketd);
    }
    else if(strcmp(cmdBuff[0], "cd")==0){
        changeDirectory(i);
        sendPath(socketd, i);
    }
    else if(strcmp(cmdBuff[0], "get")==0){
        int fd = open(cmdBuff[1], O_RDONLY);
        char file_size[256];
        fstat(fd, &fileStat);
        sprintf(file_size, "%ld", fileStat.st_size);
        length = send(socketd, file_size, sizeof(file_size), 0);
        int remain_data = fileStat.st_size;
        int sent_bytes=0;
        offset = 0;
        remain_data = fileStat.st_size;
        while (((sent_bytes = sendfile(socketd, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
        {
            remain_data -= sent_bytes;
        }
    }
    else if(strcmp(cmdBuff[0], "put")==0){
        recv(socketd, bufferR, BUFSIZ, 0);
        int file_size = atoi(bufferR);
        FILE *received_file = fopen(cmdBuff[1], "w");
        int remain_data = file_size;
        ssize_t length;
        sendMess(socketd,"Server ready to collect data.\n");
        while ((remain_data > 0) && ((length = recv(socketd, bufferR, BUFSIZ, 0)) > 0)){
            fwrite(bufferR, sizeof(char), length, received_file);
            remain_data -= length;
        }
        fclose(received_file);
        clearBuffReader();
        sendMess(socketd,"File sent\n");
    }
    else if((strcmp(cmdBuff[0], "lls")==0) || (strcmp(cmdBuff[0], "lcd")==0) || (strcmp(cmdBuff[0], "lpwd")==0) ||(strcmp(cmdBuff[0], "clear")==0)){
    sendMess(socketd, "Local cmd\n");
    }
    else if(strcmp(cmdBuff[0], "Done")==0){
        sendMess(socketd, "File downloaded\n");
    }
    else if((strcmp(cmdBuff[0], "cmds")==0)){
        sendMess(socketd, "Local commands: \n \t-lls => list files and sub dirs in current dir.\n \t-lcd => change local directory. \n \t-lpwd => show current dir. \n \t-get => get file from current server dir.\n \t-clear => clear terminal \n \t-put => put file to current server dir.\n Server commands: \n \t-ls => list files and sub dirs in current server dir.\n \t-cd => change server directory. \n \t-pwd => show current server dir.\n");
    }
    else{
        sendError(socketd);
    }
}

int main(int argc, char const *argv[]) {
    presetSockets();
    setSocket();
    setSocketOptions();
    performBinding();
    startListening();
    addrlength=sizeof(address);
    while(1){
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        maxSocketd=server_fd;

        for(int i=0; i<maxclients; i++){
            socketd=clientSocket[i];
            if(socketd>0){
                FD_SET(socketd, &readfds);
            }
            if(socketd>maxSocketd){
                maxSocketd=socketd;
            }
        }
        activity=select(maxSocketd+1, &readfds, NULL, NULL, NULL);
        
        if(activity<0 && errno!=EINTR){
            printf("select error");
        }

        if(FD_ISSET(server_fd, &readfds)){
            if((newSocket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlength))<0){ 
				perror("accept"); 
				exit(EXIT_FAILURE); 
			}
            read(newSocket, passBuf, 16);
            if(strcmp(passBuf, password)!=0){
                sendMess(socketd, "Wrong password!\n");
                close(newSocket);

            }
            else{
                connectCLient();
            }
        }
        clearBuffAndCmdBuff();
        clearBuffReader();

        for(int i=0; i<maxclients; i++){
            socketd=clientSocket[i];
            if(FD_ISSET(socketd, &readfds)){
                if((valread=read(socketd, buff, 512))==0){
                   disconnectClient(i);
                }
                else{
                    performCmd(i);
                }
            }
        }  
    }  
    return 0; 
} 