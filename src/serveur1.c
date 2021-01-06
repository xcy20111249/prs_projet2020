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
#include <sys/stat.h>

#define RCVSIZE 20
#define ACKPORT 12
#define MSGSIZE 1024
#define SEQSIZE 6
#define ACKSIZE 20
#define ALPHA 0.125
#define BETA 0.25
#define MU 1
#define DEE 4
#define POOLSIZE 100
#define baseport 8000

struct timeval RTT, SRTT, DevRTT, RTO;
int domaine=AF_INET;
int type=SOCK_DGRAM;
int protocole=0;
int ports_pool[POOLSIZE];
struct package_info
{
  char ack_sequence[ACKSIZE], pack_msg[SEQSIZE+MSGSIZE];
  struct timeval t_send, t_rcvd, RTT, SRTT, DevRTT, RTO;
  int pack_ack;
};

void calcul_package_RTO(struct package_info package_info) {/*cette fonction est vue de calculer le RTO*/
  /*initialiser les timers en format int */
  int srtt_us, rtt_us, devrtt_us, rto_us;
  srtt_us=1e6*package_info.SRTT.tv_sec+package_info.SRTT.tv_usec;
  rtt_us=1e6*package_info.RTT.tv_sec+package_info.RTT.tv_usec;
  devrtt_us=1e6*package_info.DevRTT.tv_sec+package_info.DevRTT.tv_usec;
  rto_us=1e6*package_info.RTO.tv_sec+package_info.RTO.tv_usec;

  /*calcul du rto*/
  srtt_us+=ALPHA*(rtt_us-srtt_us);
  package_info.SRTT.tv_sec=srtt_us/1e6;
  package_info.SRTT.tv_usec=srtt_us%(int)1e6;
  printf("SRTT is %lds %ldus\n", package_info.SRTT.tv_sec,package_info.SRTT.tv_usec);
  devrtt_us=(1-BETA)*devrtt_us+BETA*abs(rtt_us-srtt_us);
  package_info.DevRTT.tv_sec = devrtt_us/1e6;
  package_info.DevRTT.tv_usec = devrtt_us%(int)1e6;
  rto_us=MU*srtt_us+DEE*devrtt_us;
  package_info.RTO.tv_sec= rto_us/1e6;
  package_info.RTO.tv_usec= rto_us%(int)1e6;
  printf("RTO is %lds %ldus\n", package_info.RTO.tv_sec,package_info.RTO.tv_usec);
}

int get_available_port(){/*find a port which is available for info transmission*/
  int port;
  for(int i=0; i<POOLSIZE; i++){
    if (ports_pool[i]) {
      printf("%d\n", i);
      ports_pool[i]=0;
      port=i+baseport;
      printf("%d\n", port);
      return port;
    }
  }
  printf("no port available, plz wait\n");
  sleep(5);
  return get_available_port();
}

void calcul_RTO(/* arguments */) {/*cette fonction est vue de calculer le RTO*/
  /*initialiser les timers en format int */
  int srtt_us, rtt_us, devrtt_us, rto_us;
  srtt_us=1e6*SRTT.tv_sec+SRTT.tv_usec;
  rtt_us=1e6*RTT.tv_sec+RTT.tv_usec;
  devrtt_us=1e6*DevRTT.tv_sec+DevRTT.tv_usec;
  rto_us=1e6*RTO.tv_sec+RTO.tv_usec;

  /*calcul du rto*/
  srtt_us+=ALPHA*(rtt_us-srtt_us);
  SRTT.tv_sec=srtt_us/1e6;
  SRTT.tv_usec=srtt_us%(int)1e6;
  printf("SRTT is %lds %ldus\n", SRTT.tv_sec,SRTT.tv_usec);
  devrtt_us=(1-BETA)*devrtt_us+BETA*abs(rtt_us-srtt_us);
  DevRTT.tv_sec = devrtt_us/1e6;
  DevRTT.tv_usec = devrtt_us%(int)1e6;
  rto_us=MU*srtt_us+DEE*devrtt_us;
  RTO.tv_sec= rto_us/1e6;
  RTO.tv_usec= rto_us%(int)1e6;
  printf("RTO is %lds %ldus\n", RTO.tv_sec,RTO.tv_usec);
}


int main(int argc,char* argv[]) {
  if (argc<2) {
    printf("missing arguments\n");
    return -1;
  }

  int port_serverudp=atoi(argv[1]);
  int port_servertcp;


  int socket_frontdesk;
  socket_frontdesk=socket(domaine,type,protocole);
  printf("socket udp est: %d\n", socket_frontdesk);

  int reuse=1;
  setsockopt(socket_frontdesk,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
  if (socket_frontdesk<0){
    close(socket_frontdesk);
    printf("le descripteur est %d\n",socket_frontdesk);
    perror("Cannot create socket\n");
    return -1;
  }


  struct sockaddr_in my_addr1;
  memset((char*)&my_addr1,0,sizeof(my_addr1));
  my_addr1.sin_family=domaine;
  my_addr1.sin_port=htons(port_serverudp);
  my_addr1.sin_addr.s_addr=htonl(INADDR_ANY);


  if(bind(socket_frontdesk,(struct sockaddr*)&my_addr1,sizeof(my_addr1))<0){
    perror("Bind failed\n");
    close(socket_frontdesk);
    return -1;
  };

  struct sockaddr_in client_addr;
  memset((char*)&client_addr,0,sizeof(client_addr));
  socklen_t c_len = sizeof(client_addr);

  /*initialize a list of POOLSIZE ports for info transmission*/
  for (int i=0;i<POOLSIZE;i++){
    ports_pool[i]=1;
  }

  printf("waiting for connection\n");
  for (;;) {
    char serverbuffer[RCVSIZE];
    char ackport[ACKPORT];
    ackport[ACKPORT-1]='\0';
    int goon=1;//control for msg transmission begin
    int con=1;//control of handshake done
    int socket_transmission;

    while (con) {
      int cont=0;
      memset(serverbuffer,0,RCVSIZE);
      recvfrom(socket_frontdesk,serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);

      //initialize the socket for transmission
      socket_transmission=socket(domaine,type,protocole);
      printf("socket udp-tcp est: %d\n", socket_transmission);
      setsockopt(socket_transmission,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
      if (socket_transmission<0){
        close(socket_transmission);
        printf("le descripteur est %d\n",socket_transmission);
        perror("Cannot create socket\n");
        return -1;
      }

      //get a available port from ports pool
      int portcont=1;
      while (portcont) {
        for(int i=0; i<POOLSIZE; i++){
          if (ports_pool[i]) {
            ports_pool[i]=0;
            port_servertcp=i+baseport;
            portcont=0;
            break;
          }
        }
        if (portcont){
          printf("no port available, plz wait\n");
          sleep(5);
        }
      }

      printf("port for info transmission is %d\n", port_servertcp);
      sprintf(ackport,"%s%d","SYN-ACK",port_servertcp);
      printf("%s\n", ackport);

      struct sockaddr_in my_addr2;
      memset((char*)&my_addr2,0,sizeof(my_addr2));
      my_addr2.sin_family=domaine;
      my_addr2.sin_port=htons(port_servertcp);
      my_addr2.sin_addr.s_addr=htonl(INADDR_ANY);

      if(bind(socket_transmission,(struct sockaddr*)&my_addr2,sizeof(my_addr2))<0){
        perror("Bind failed\n");
        close(socket_transmission);
        return -1;
      };

      //handshake
      printf("****************\n");
      if (strcmp(serverbuffer,"SYN")==0) {//connect the client
        printf("client ask for tcp connection\n");
        char* ip_client_udp=inet_ntoa(client_addr.sin_addr);
        int port_client_udp=ntohs(client_addr.sin_port);
        printf("Client IP is %s\n", ip_client_udp);
        printf("Client port is %d\n", port_client_udp);

        while (1) {
          sendto(socket_frontdesk,ackport,ACKPORT,0,(struct sockaddr*)&client_addr,c_len);
          printf("tcp port info sent\n");
          memset(serverbuffer,0,RCVSIZE);
          recvfrom(socket_frontdesk,serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
          if (strcmp(serverbuffer,"ACK")==0) {
            printf("handshake done\n");
            con=0;
            break;
          }
          cont++;
          if (cont>=5) {//5 cercles no response
            goon=0;
            printf("failed\n");
            close(socket_transmission);
            break;
          }
          sleep(0.5);
        }
      }

      pid_t fpid;
      fpid=fork();
      if (fpid<0) {
        printf("error in fork\n");
      }else if (fpid>0) {//father process close socket for transmission and wait for new connection
        printf("son process pid is %d\n", fpid);
        close(socket_transmission);
      } else if (fpid==0) {
        close(socket_frontdesk);

        if (goon) {//begin communication
          FILE *fp;
          char ackbuffer[ACKSIZE];//ack msg
          char tembuffer[MSGSIZE];//data of the pic
          char fname[RCVSIZE];
          int len;
          int cwnd=1;
          //int varmsgsize=MSGSIZE/8;
          fd_set readfds;
          FD_ZERO(&readfds);
          struct timeval timeout,start,end;
          RTO.tv_sec=1;
          RTO.tv_usec=500000;
          SRTT.tv_usec=10000;
          SRTT.tv_sec=0;
          long total_us_calcul;

          printf("go on\n");
          int conter=1;
          while (conter) {
            memset(serverbuffer,0,RCVSIZE);
            recvfrom(socket_transmission,serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
            printf("client wants: %s\n", serverbuffer);
            if (strcmp(serverbuffer,"close")==0) {
              conter=0;
            }
            sprintf(fname,"%s",serverbuffer);

            //client ask for a non exist file
            if ((fp=fopen(fname,"rb"))==NULL) {
              printf("file not found\n");
              exit(1);
            }

            //get file size
            struct stat statbuf;
            stat(fname,&statbuf);
            int file_size=statbuf.st_size;
            printf("file size is %d\n", file_size);

            //get package number
            int pak_num=file_size/MSGSIZE;
            if (file_size%MSGSIZE!=0){
              pak_num+=1;
            }
            printf("total package number is %d\n", pak_num);

            struct package_info paquets[pak_num];
            for (int i = 0; i < pak_num; i++) {
              paquets[i].pack_ack=0;
            }

            printf("transmission begin\n");

            fflush(stdout);
            int seq=1;
            char ackmsg[10];

            //prepare all packages for transmission
            while (!feof(fp)) {
              //fd_set readfds;
              //FD_ZERO(&readfds);
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
              char msgbuffer[SEQSIZE+MSGSIZE];//whole msg for transmission
              memset(msgbuffer,0,SEQSIZE+MSGSIZE);
              sprintf(ackmsg,"%s%.6s","ACK",sequence);
              ackmsg[sizeof(ackmsg)-1]='\0';
              sprintf(paquets[seq-1].ack_sequence,"%s",ackmsg);
              printf("should receive %s\n", paquets[seq-1].ack_sequence);

              memset(tembuffer,0,MSGSIZE);
              len=fread(tembuffer,1,MSGSIZE,fp);
              printf("len of tembuffer %d\n", len);
              sprintf(msgbuffer,"%.6s%s",sequence,tembuffer);
              sprintf(paquets[seq-1].pack_msg,"%s",msgbuffer);
              printf("package %d ready\n", seq);

              /*while (1) {
                FD_SET(socket_transmission,&readfds);
                timeout.tv_sec=RTO.tv_sec;
                timeout.tv_usec=RTO.tv_usec;

                sendto(socket_transmission,msgbuffer,SEQSIZE+len,0,(struct sockaddr*)&client_addr,c_len);
                gettimeofday(&start,NULL);
                int resul=select(socket_transmission+1,&readfds,NULL,NULL,&timeout);

                //sent msg and wait for ack
                if (FD_ISSET(socket_transmission,&readfds)) {
                  memset(ackbuffer,0,ACKSIZE);
                  recvfrom(socket_transmission,ackbuffer,ACKSIZE,0,(struct sockaddr*)&client_addr,&c_len);
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
              }*/
              seq++;
            }

            seq=1;
            for (int i = 0; i < pak_num; i++) {
              fd_set readfds;
              FD_ZERO(&readfds);
              /*char sequence[6];
              memset(sequence,0,6);
              int seqint=seq;
              int r;
              char exchange[2];*/

              for (int j = 0; j < cwnd; j++) {

                printf("sequence is %.6d\n",seq );
                printf("should receive %s\n", paquets[seq-1].ack_sequence);

                /*memset(tembuffer,0,MSGSIZE);
                len=fread(tembuffer,1,MSGSIZE,fp);
                printf("len of tembuffer %d\n", len);
                sprintf(msgbuffer,"%.6s%s",sequence,tembuffer);
                sprintf(paquets[seq-1].pack_msg,"%s",msgbuffer);
                printf("package ready\n");*/

                while (1) {
                  FD_SET(socket_transmission,&readfds);
                  timeout.tv_sec=RTO.tv_sec;
                  timeout.tv_usec=RTO.tv_usec;

                  sendto(socket_transmission,paquets[seq-1].pack_msg,sizeof(paquets[seq-1].pack_msg),0,(struct sockaddr*)&client_addr,c_len);
                  gettimeofday(&start,NULL);
                  int resul=select(socket_transmission+1,&readfds,NULL,NULL,&timeout);

                  //sent msg and wait for ack
                  if (FD_ISSET(socket_transmission,&readfds)) {
                    memset(ackbuffer,0,ACKSIZE);
                    recvfrom(socket_transmission,ackbuffer,ACKSIZE,0,(struct sockaddr*)&client_addr,&c_len);
                    gettimeofday(&end,NULL);
                    total_us_calcul=1e6*(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec);
                    RTT.tv_sec=total_us_calcul/1e6;
                    RTT.tv_usec=total_us_calcul-RTT.tv_sec;
                    printf("RTT is %lds %ldus\n", RTT.tv_sec,RTT.tv_usec);
                    if(strcmp(ackbuffer,paquets[seq-1].ack_sequence)==0){
                      printf("msg %s rcved\n", ackbuffer);
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
                calcul_RTO();
              }
            }
            sendto(socket_transmission,"FIN",3,0,(struct sockaddr*)&client_addr,c_len);
            printf("transmission done\n");
            fclose(fp);
            conter=0;
          }
        }
        close(socket_transmission);
        ports_pool[port_servertcp-baseport]=1;
        printf("port used %d is now %d\n", port_servertcp, ports_pool[port_servertcp-baseport]);
        printf("port %d closed\n", port_servertcp);
        exit(1);
      }

    }

    //exit(1);
    //sleep(5);
  }

  close(socket_frontdesk);

  return 0;
}
