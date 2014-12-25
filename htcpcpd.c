/* HYPER TEXT COFFEE POT CONTROL PROTOCOL (HTCPCP/1.0)
	RFC2324 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <wiringPi.h>


pid_t* pidchild;
int* sock1;
int* sock2;

void sig_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        kill(*pidchild, SIGQUIT);
        close(*sock1);
        close(*sock2);
        digitalWrite(3, LOW);
        digitalWrite(0, LOW);
        exit(0);
    }
}


typedef struct
{
    int ready;
    time_t lastbrew;
} pot;

pot potinfo;

int brew(void)
{
    if(potinfo.ready == 1)
    {
        potinfo.ready = 0;
        potinfo.lastbrew = time(NULL);
        //brew coffee
        digitalWrite(0, HIGH);
        return 0;
    }
    return 1;
}
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

char* handleHeaders(char *request)
{
    char method[255] ;
    int i = 0;
    for(i = 0; request[i] != '\n' && request[i] != '\0'; i++)
    {
        method[i] = request[i];
    }
    method[i] = '\0';
    if(strcmp(method, "BREW") == 0 || strcmp(method, "POST") == 0 || strcmp(method, "GET") == 0)
    {
        if(brew() == 0)
        {
            return("HTCPCP/1.0 200 OK");
        }
        else
        {
            return("HTCPCP/1.0 510 Pot busy");
        }
    }
    else if(strcmp(method, "PROPFIND") == 0)
    {
        //server metadata
        if(potinfo.ready == 1)
        {
            return("HTCPCP/1.0 200 OK\nContent-type: message/coffeepot\nPot ready to brew");
        }
        else
        {
            return("HTCPCP/1.0 200 OK\nContent-type: message/coffeepot\nPot not ready to brew");
        }
    }
    else if(strcmp(method, "WHEN") == 0)
    {
        //err immidiatly
        return("HTCPCP/1.0 406 Not Acceptable\nAdditions-List:");
    }
    else
    {
        return ("HTCPCP/1.0 418 I'm a teapot");
    }
}

int main(int argc, char *argv[])
{
    printf("HTCPCP/1.0 server starting\n");
    sleep(30);
    potinfo.ready = 1;
    potinfo.lastbrew = time(NULL);
    int sockfd, newsockfd, portno;
    sock1 = &sockfd;
    sock2 = &newsockfd;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if (argc < 2)
    {
        portno = 80;
    }
    else
    {
        portno = atoi(argv[1]);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    signal(SIGINT, sig_handler);
    signal(SIGCHLD,SIG_IGN);
    wiringPiSetup();
    pinMode (0, OUTPUT);
    pinMode (2, INPUT);
    pinMode (3, OUTPUT);
    digitalWrite(3, HIGH);
    //threading time
    pid_t childPID;
    int var_lcl = 0;

    int fd[2];
    pipe(fd);
    fcntl(fd[0], F_SETFL, O_NONBLOCK);

    int potRed[2];
    pipe(potRed);
    fcntl(potRed[0], F_SETFL, O_NONBLOCK);
    fcntl(potRed[1], F_SETFL, O_NONBLOCK);
    childPID = fork();

    if(childPID >= 0) // fork was successful
    {
        daemon(0, 0);
        if(childPID == 0) // child process
        {
            close(fd[1]);
            close(potRed[0]);
            pot potcpy;
            potcpy.ready = 1;
            time_t blink = time(NULL);
            while( 0 == 0 )
            {
                //check shutdown, ready
                read(fd[0], &potcpy, sizeof(pot));
                if(potcpy.ready == 0)
                {
                    //pot is brewing
                    if(time(NULL) - potcpy.lastbrew >= 1800)
                    {
                        //half hour
                        digitalWrite(0, LOW);
                    }
                    if(time(NULL) > blink)
                    {
                        if(digitalRead(3) == HIGH)
                        {
                            digitalWrite(3, LOW);
                        }
                        else if(digitalRead(3) == LOW)
                        {
                            digitalWrite(3, HIGH);
                        }
                        blink = time(NULL);
                    }
                }
                else
                {
                    digitalWrite(3, HIGH);
                }

                if(digitalRead(2) == HIGH)
                {
                    sleep(3);
                    if(potcpy.ready == 0 && time(NULL) - potcpy.lastbrew >= 1800)
                    {
                        potcpy.ready = 1;
                        write(potRed[1], &potcpy, sizeof(pot));
                    }
                    else if(digitalRead(2) == HIGH)
                    {
                        //shutdown
                        system("/usr/bin/poweroff");
                    }
                    else
                    {
                        potcpy.ready = 0;
                        potcpy.lastbrew = time(NULL);
                        //brew coffee
                        digitalWrite(0, HIGH);
                        write(potRed[1], &potcpy, sizeof(pot));
                    }
                }
            }
        }
        else //Parent process
        {
            pidchild = &childPID;
            close(fd[0]);
            close(potRed[1]);
            while(0 == 0)  //listen for connections
            {

                listen(sockfd,5);
                clilen = sizeof(cli_addr);
                newsockfd = accept(sockfd,
                                   (struct sockaddr *) &cli_addr,
                                   &clilen);
                if (newsockfd < 0)
                    error("ERROR on accept");
                bzero(buffer,256);
                n = read(newsockfd,buffer,255);
                if (n < 0) error("ERROR reading from socket");
                read(potRed[0], &potinfo, sizeof(pot));
                char* returnHead = handleHeaders(buffer);
                n = write(newsockfd,returnHead,strlen(returnHead));
                if (n < 0) error("ERROR writing to socket");

                write(fd[1], &potinfo, sizeof(pot));
            }
        }
    }
    else // fork failed
    {
        error("Fork failed\n");
    }

    close(newsockfd);
    close(sockfd);
    return 0;
}
