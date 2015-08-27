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

//set user-set variables
int time_to_brew = 1800;
int port = 80;
int relay_pin = 0;
int button_pin = 2;
int led_pin = 3;

void sig_handler(int signo)  //terminate gracefully
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        kill(*pidchild, SIGQUIT);
        close(*sock1);
        close(*sock2);
        digitalWrite(led_pin, LOW);
        digitalWrite(relay_pin, LOW);
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
        digitalWrite(relay_pin, HIGH);
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

void setVars(char* var, char* val)
{
    if(strstr(var, "time_to_brew") != NULL)
    {
        time_to_brew = atoi(val);
    }
    else if(strstr(var, "port") != NULL)
    {
        port = atoi(val);
    }
    else if(strstr(var, "relay_pin") != NULL)
    {
        relay_pin = atoi(val);
    }
    else if(strstr(var, "button_pin") != NULL)
    {
        button_pin = atoi(val);
    }
    else if(strstr(var, "led_pin") != NULL)
    {
        led_pin = atoi(val);
    }
}


int main(int argc, char *argv[])
{
    printf("HTCPCP/1.0 server starting\n");

    //ignore conf file if it doesn't exist
    if(access( "/etc/htcpcp.conf", F_OK ) != -1)
    {
        //read fom conf file
        FILE* confile = fopen("/etc/htcpcp.conf", "r");
        char* tmp;
        while(fgets(tmp, sizeof(tmp), confile) != NULL)
        {
            //ignore comments
            if(tmp[0] == '#')
            {
                continue;
            }
            else
            {
                //get the variable being set
                char *var = strtok(tmp, "=");
                //get the value it's being set to
                char *val = strtok(tmp, "=");
                //set it
                setVars(var, val);
                free(var);
                free(val);
            }
        }
        free(tmp);
        fclose(confile);
    }
    
    sleep(30); //dirty hack to make sure WiFi is on before the program starts when using Systemd unit file
    potinfo.ready = 1;
    potinfo.lastbrew = time(NULL);
    int sockfd, newsockfd;
    sock1 = &sockfd;
    sock2 = &newsockfd;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if(argc > 2)
    {
        //if the port is specified via command line, override default and conf file
        port = atoi(argv[1]);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("ERROR opening socket");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }

    signal(SIGINT, sig_handler);
    signal(SIGCHLD,SIG_IGN);
    //setup the GPIO pins 
    wiringPiSetup();
    pinMode (relay_pin, OUTPUT);
    pinMode (button_pin, INPUT);
    pinMode (led_pin, OUTPUT);
    digitalWrite(led_pin, HIGH);  //turn on the indicator LED
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
                    if(time(NULL) - potcpy.lastbrew >= time_to_brew)
                    {
                        //half hour
                        digitalWrite(relay_pin, LOW);
                    }
                    if(time(NULL) - potcpy.lastbrew >= time_to_brew && time(NULL) > blink)
                    {
                        if(digitalRead(led_pin) == HIGH)
                        {
                            digitalWrite(led_pin, LOW);
                        }
                        else if(digitalRead(led_pin) == LOW)
                        {
                            digitalWrite(led_pin, HIGH);
                        }
                        blink = time(NULL);
                    }
                    else if(time(NULL) > blink + 4)
                    {
                        if(digitalRead(led_pin) == HIGH)
                        {
                            digitalWrite(led_pin, LOW);
                        }
                        else if(digitalRead(led_pin) == LOW)
                        {
                            digitalWrite(led_pin, HIGH);
                        }
                        blink = time(NULL);
                    }
                }
                else
                {
                    digitalWrite(led_pin, HIGH);
                }

                if(digitalRead(button_pin) == HIGH)
                {
                    sleep(3);
                    if(potcpy.ready == 0 && time(NULL) - potcpy.lastbrew >= time_to_brew)
                    {
                        potcpy.ready = 1;
                        write(potRed[1], &potcpy, sizeof(pot));
                    }
                    else if(digitalRead(button_pin) == HIGH)
                    {
                        //shutdown
                        system("/usr/bin/poweroff");
                    }
                    else
                    {
                        potcpy.ready = 0;
                        potcpy.lastbrew = time(NULL);
                        //brew coffee
                        digitalWrite(relay_pin, HIGH);
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
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                if (newsockfd < 0)
                {
                    error("ERROR on accept");
                }
                bzero(buffer,256);
                n = read(newsockfd,buffer,255);
                if (n < 0)
                {
                    error("ERROR reading from socket");
                }
                read(potRed[0], &potinfo, sizeof(pot));
                char* returnHead = handleHeaders(buffer);
                n = write(newsockfd,returnHead,strlen(returnHead));
                if (n < 0) 
                {
                    error("ERROR writing to socket");
                }

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
    digitalWrite(led_pin, LOW);
    digitalWrite(button_pin, LOW);
    return 0;
}
