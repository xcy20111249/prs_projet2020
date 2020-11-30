#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>

#define RCVSIZE 20
#define ACKPORT 12
#define MSGSIZE 1024
#define SEQSIZE 6
#define ACKSIZE 20
#define ALPHA 0.125
#define BETA 0.25
#define MU 1
#define DEE 4

int main(int argc,char* argv[]) {
  if (argc<2) {
    printf("missing arguments\n");
    return -1;
  }

  int port_serverudp=atoi(argv[1]);
  int port_servertcp=8001;
  int domaine=AF_INET;
  int type=SOCK_DGRAM;
  int protocole=0;

  int sockets[2];
  sockets[0]=socket(domaine,type,protocole);
  printf("socket udp est: %d\n", sockets[0]);
  sockets[1]=socket(domaine,type,protocole);
  printf("socket udp-tcp est: %d\n", sockets[1]);

  int reuse=1;
  for (int i = 0; i < 1; i++) {
    setsockopt(sockets[i],SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    if (sockets[i]<0){
      close(sockets[i]);
      printf("le descripteur est %d\n",sockets[i]);
      perror("Cannot create socket\n");
      return -1;
    }
  }

  struct sockaddr_in my_addr1;
  memset((char*)&my_addr1,0,sizeof(my_addr1));
  my_addr1.sin_family=domaine;
  my_addr1.sin_port=htons(port_serverudp);
  my_addr1.sin_addr.s_addr=htonl(INADDR_ANY);

  struct sockaddr_in my_addr2;
  memset((char*)&my_addr2,0,sizeof(my_addr2));
  my_addr2.sin_family=domaine;
  my_addr2.sin_port=htons(port_servertcp);
  my_addr2.sin_addr.s_addr=htonl(INADDR_ANY);

  if(bind(sockets[0],(struct sockaddr*)&my_addr1,sizeof(my_addr1))<0){
    perror("Bind failed\n");
    close(sockets[0]);
    return -1;
  };
  if(bind(sockets[1],(struct sockaddr*)&my_addr2,sizeof(my_addr2))<0){
    perror("Bind failed\n");
    close(sockets[1]);
    return -1;
  };


  struct sockaddr_in client_addr;
  memset((char*)&client_addr,0,sizeof(client_addr));
  socklen_t c_len = sizeof(client_addr);

  printf("waiting for connection\n");
  for (;;) {
    char serverbuffer[RCVSIZE];
    char ackport[ACKPORT];
    ackport[ACKPORT-1]='\0';
    sprintf(ackport,"%s%d","SYN-ACK",port_servertcp);
    printf("%s\n", ackport);
    int goon=1;//control for msg transmission begin
    int con=1;//control of handshake done
    while (con) {
      int cont=0;
      memset(serverbuffer,0,RCVSIZE);
      recvfrom(sockets[0],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
      printf("****************\n");
      if (strcmp(serverbuffer,"SYN")==0) {
        printf("client ask for tcp connection\n");
        char* ip_client_udp=inet_ntoa(client_addr.sin_addr);
        int port_client_udp=ntohs(client_addr.sin_port);
        printf("Client IP is %s\n", ip_client_udp);
        printf("Client port is %d\n", port_client_udp);

        while (1) {
          sendto(sockets[0],ackport,ACKPORT,0,(struct sockaddr*)&client_addr,c_len);
          printf("tcp port info sent\n");
          memset(serverbuffer,0,RCVSIZE);
          recvfrom(sockets[0],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
          if (strcmp(serverbuffer,"ACK")==0) {
            printf("handshake done\n");
            con=0;
            break;
          }
          cont++;
          if (cont>=5) {//5 cercles no response
            goon=0;
            printf("failed\n");
            break;
          }
          sleep(0.5);
        }
      }
    }
    //handshake done, beginning the transmission
    if (goon) {//begin communication
      FILE *fp;
      char ackbuffer[ACKSIZE];//ack msg
      char tembuffer[MSGSIZE];//data of the pic
      char fname[RCVSIZE];
      int len;
      int varmsgsize=MSGSIZE/8;
      fd_set readfds;
      FD_ZERO(&readfds);
      struct timeval timeout,start,end;
      struct timeval RTT, SRTT, DevRTT, RTO;
      RTO.tv_sec=1;
      RTO.tv_usec=500000;
      SRTT.tv_usec=10000;
      SRTT.tv_sec=0;
      long total_us_calcul;

      printf("go on\n");
      int cont=1;
      while (cont) {
        memset(serverbuffer,0,RCVSIZE);
        recvfrom(sockets[1],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
        printf("client wants: %s\n", serverbuffer);
        if (strcmp(serverbuffer,"close")==0) {
          cont=0;
        }
        sprintf(fname,"%s",serverbuffer);

        //client ask for a non exist file
        if ((fp=fopen(fname,"rb"))==NULL) {
          printf("file not found\n");
          exit(1);
        }

        printf("transmission begin\n");

        fflush(stdout);
        int seq=1;
        char ackmsg[10];

        while (!feof(fp)) {
          fd_set readfds;
          FD_ZERO(&readfds);
          char sequence[6];
          memset(sequence,0,6);
          int seqint=seq;
          int r;
          char exchange[2];

          //sequence number from int to char
          for (int i = 0; i < sizeof(sequence); i++) {
            r=seqint%10;
            sprintf(exchange,"%d",r);
            sequence[sizeof(sequence)-1-i]=exchange[0];
            seqint=(seqint-r)/10;
          }

          printf("sequence is %.6s\n",sequence );
          char msgbuffer[SEQSIZE+varmsgsize];//whole msg for transmission
          memset(msgbuffer,0,SEQSIZE+varmsgsize);
          sprintf(ackmsg,"%s%.6s","ACK",sequence);
          ackmsg[sizeof(ackmsg)-1]='\0';
          printf("should receive %s\n", ackmsg);

          memset(tembuffer,0,MSGSIZE);
          len=fread(tembuffer,1,varmsgsize,fp);
          printf("len of tembuffer %d\n", len);
          fflush(stdout);
          sprintf(msgbuffer,"%.6s%s",sequence,tembuffer);
          fflush(stdout);
          printf("package ready\n");

          while (1) {
            FD_SET(sockets[1],&readfds);
            timeout.tv_sec=RTO.tv_sec;
            timeout.tv_usec=RTO.tv_usec;

            //printf("setted\n");
            sendto(sockets[1],msgbuffer,SEQSIZE+len,0,(struct sockaddr*)&client_addr,c_len);
            gettimeofday(&start,NULL);
            //printf("%s\n", msgbuffer);
            int resul=select(sockets[1]+1,&readfds,NULL,NULL,&timeout);

            //sent msg and wait for ack
            if (FD_ISSET(sockets[1],&readfds)) {
              //sleep(0.5);
              memset(ackbuffer,0,ACKSIZE);
              recvfrom(sockets[1],ackbuffer,ACKSIZE,0,(struct sockaddr*)&client_addr,&c_len);
              gettimeofday(&end,NULL);
              total_us_calcul=1e6*(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec);
              RTT.tv_sec=total_us_calcul/1e6;
              RTT.tv_usec=total_us_calcul-RTT.tv_sec;
              printf("RTT is %lds %ldus\n", RTT.tv_sec,RTT.tv_usec);
              if(strcmp(ackbuffer,ackmsg)==0){

                printf("msg %s rcved\n", ackmsg);
                break;
              }
            }
            if(resul==0){
              printf("timeout no response\n");
              sleep(1);
              continue;
            }
          }
          seq++;
          SRTT.tv_sec+=ALPHA*(RTT.tv_sec-SRTT.tv_sec);
          SRTT.tv_usec+=ALPHA*(RTT.tv_usec-SRTT.tv_usec);
          printf("SRTT is %lds %ldus\n", SRTT.tv_sec,SRTT.tv_usec);
          DevRTT.tv_sec = (1-BETA)*DevRTT.tv_sec+BETA*abs(RTT.tv_sec-SRTT.tv_sec);
          DevRTT.tv_usec = (1-BETA)*DevRTT.tv_usec+BETA*abs(RTT.tv_usec-SRTT.tv_usec);
          RTO.tv_sec= MU* SRTT.tv_sec+DEE*DevRTT.tv_sec;
          RTO.tv_usec= MU* SRTT.tv_usec+DEE*DevRTT.tv_usec;
          printf("RTO is %lds %ldus\n", RTO.tv_sec,RTO.tv_usec);
          if(varmsgsize<MSGSIZE){
            varmsgsize*=2;
          }
        }
        sendto(sockets[1],"FIN",3,0,(struct sockaddr*)&client_addr,c_len);
        printf("transmission done\n");
        fclose(fp);
      }
    }
    exit(0);
  }
  for (int i = 0; i < 2; i++) {
    close(sockets[i]);
  }
  return 0;
}
