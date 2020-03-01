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

int sock = 0, valread, convertionStatus, connectionStatus; 
struct sockaddr_in servAddress;
char buffWriter[2048];
char buffReader[4096];
char serverIP[16];
int PORT;
char *cmdBuff[512];
int cmdBufferIndex;
char *password;
char *done = "Done";
struct stat fileStat;
ssize_t length;
off_t offset;

void sendPassword(){
    password = getpass("Type password: ");
    send(sock, password, strlen(password), 0);
}

void performConnect(){
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0){
        printf("Connection error.\n");
        exit(EXIT_FAILURE);
    }
    servAddress.sin_family = AF_INET;
    servAddress.sin_port = htons(PORT);
    convertionStatus = inet_pton(AF_INET, serverIP, &servAddress.sin_addr);
    if(convertionStatus<=0){
        printf("Connection error.\n");
        exit(EXIT_FAILURE);
    }
    connectionStatus = connect(sock, (struct sockaddr*)&servAddress, sizeof(servAddress));
    if(connectionStatus<0){
        printf("Connection error.\n");
        exit(EXIT_FAILURE);
    }
    sendPassword();
}

void clearBuffReader(){
    for(int i=0; i<512; i++){
        buffReader[i]='\0';
    }
}

void clearBuffWriter(){
    for(int i=0; i<512; i++){
        buffWriter[i]='\0';
    }
}

void clearBuffCmd(){
    for(int i=0; i<512; i++){
        cmdBuff[i]='\0';
    }
}

void processCommand(){
    int buffer_size=strlen(buffWriter);
    cmdBufferIndex=1;
    cmdBuff[0]=&buffWriter[0];
    for(int i=0; i<buffer_size; i++){
        if(buffWriter[i]=='\0'){
            cmdBuff[cmdBufferIndex]=NULL;
            return;
        }
        if(buffWriter[i]=='\n'){
            buffWriter[i]='\0';
        }
        if(buffWriter[i]==' '){
            buffWriter[i]='\0';
            if(buffWriter[i+1]!=' '){
                cmdBuff[cmdBufferIndex]=&buffWriter[i+1];
                cmdBufferIndex++;
            }
        }
    }
}

void readFromServer(){
    clearBuffReader();
    valread = read(sock, buffReader, 512);
    if(strcmp(buffReader, "Wrong password.")==0){
        printf("%s", buffReader);
        exit(1);
    }
    else if(strcmp(buffReader, "Done")==0){
        printf("%s", buffReader);
    }
    buffReader[valread]='\0';
    write(1, buffReader, strlen(buffReader));
}

void executeCommand(char *cmd){
    int t;
    cmdBuff[0]=cmd;
        int myfork = fork();
        if(myfork==0){
            close(0);
            execvp(cmdBuff[0], cmdBuff);
            exit(1);
        }
        else if(myfork>0){
            wait(&t);
        }
}

void performCmd(){
    if(strcmp(cmdBuff[0], "lls")==0){
        executeCommand("ls");
    }
    else if(strcmp(cmdBuff[0], "lpwd")==0){
        executeCommand("pwd");
    }
    else if(strcmp(cmdBuff[0], "clear")==0){
        executeCommand("clear");
    }
    else if(strcmp(cmdBuff[0], "lcd")==0){
        chdir(cmdBuff[1]);
    }
    else if(strcmp(cmdBuff[0], "get")==0){
        recv(sock, buffReader, BUFSIZ, 0);
        int file_size = atoi(buffReader);
        FILE *received_file = fopen(cmdBuff[1], "w");
        int remain_data = file_size;
        ssize_t length;
        while ((remain_data > 0) && ((length = recv(sock, buffReader, BUFSIZ, 0)) > 0)){
                fwrite(buffReader, sizeof(char), length, received_file);
                remain_data -= length;
        }
        fclose(received_file);
        clearBuffReader();
        send(sock, done, strlen(done), 0);
    }
    else if(strcmp(cmdBuff[0], "put")==0){
        int fd = open(cmdBuff[1], O_RDONLY);
        char file_size[256];
        fstat(fd, &fileStat);
        sprintf(file_size, "%ld", fileStat.st_size);
        length = send(sock, file_size, sizeof(file_size), 0);
        int remain_data = fileStat.st_size;
        int sent_bytes=0;
        offset = 0;
        remain_data = fileStat.st_size;
        readFromServer();
        printf("Sending data...\n");
        while (((sent_bytes = sendfile(sock, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
        {
            remain_data -= sent_bytes;
        }
    }
}

void sendMess(){
    clearBuffWriter();
    clearBuffCmd();
    read(0, buffWriter, 512);
    send(sock, buffWriter, strlen(buffWriter), 0);
    processCommand();
    performCmd();
}

int main(int argc, char const *argv[]) 
{
    printf("Type server ip: ");
    scanf("%s", serverIP);
    printf("Type server port: ");
    scanf("%d", &PORT);
    performConnect();
    while(1){
        readFromServer();
        for(int i = 0; i < 50; i++)
            printf("\u2500");
        printf("\n");
        sendMess();
    }
    return 0; 
} 
