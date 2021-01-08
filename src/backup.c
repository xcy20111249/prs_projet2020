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
  struct timeval t_send, t_rcvd;
  /*struct timeval RTT, SRTT, DevRTT, RTO;*/
  int pac_ack, pac_taille;
};
struct rto_info
{
  struct timeval RTT, SRTT, DevRTT, RTO;
};
char whole_file[1000000000];

void calcul_package_RTO(struct rto_info rto_info) {/*cette fonction est vue de calculer le RTO*/
  /*initialiser les timers en format int */
  int srtt_us, rtt_us, devrtt_us, rto_us;
  srtt_us=1e6*rto_info.SRTT.tv_sec+rto_info.SRTT.tv_usec;
  rtt_us=1e6*rto_info.RTT.tv_sec+rto_info.RTT.tv_usec;
  devrtt_us=1e6*rto_info.DevRTT.tv_sec+rto_info.DevRTT.tv_usec;
  rto_us=1e6*rto_info.RTO.tv_sec+rto_info.RTO.tv_usec;

  /*calcul du rto*/
  srtt_us+=ALPHA*(rtt_us-srtt_us);
  rto_info.SRTT.tv_sec=srtt_us/1e6;
  rto_info.SRTT.tv_usec=srtt_us%(int)1e6;
  printf("SRTT is %lds %ldus\n", rto_info.SRTT.tv_sec,rto_info.SRTT.tv_usec);
  devrtt_us=(1-BETA)*devrtt_us+BETA*abs(rtt_us-srtt_us);
  rto_info.DevRTT.tv_sec = devrtt_us/1e6;
  rto_info.DevRTT.tv_usec = devrtt_us%(int)1e6;
  rto_us=MU*srtt_us+DEE*devrtt_us;
  rto_info.RTO.tv_sec= rto_us/1e6;
  rto_info.RTO.tv_usec= rto_us%(int)1e6;
  printf("RTO is %lds %ldus\n", rto_info.RTO.tv_sec,rto_info.RTO.tv_usec);
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
    int rtt_origin_us;

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
          struct timeval start, end;
          sendto(socket_frontdesk,ackport,ACKPORT,0,(struct sockaddr*)&client_addr,c_len);
          gettimeofday(&start,NULL);
          printf("tcp port info sent\n");
          memset(serverbuffer,0,RCVSIZE);
          recvfrom(socket_frontdesk,serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
          gettimeofday(&end,NULL);
          rtt_origin_us=(end.tv_sec-start.tv_sec)*1e6+(end.tv_usec-start.tv_usec);
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

        struct timeval commence_trans, terminus_trans;//calcul debit
        long time_trans;
        int file_size;
        int pak_num;


        if (goon) {//begin communication
          FILE *fp;
          char ackbuffer[ACKSIZE];//ack msg
          char tembuffer[MSGSIZE];//data of the pic
          char fname[RCVSIZE];
          int len;
          int cwnd=50;
          fd_set readfds;
          FD_ZERO(&readfds);
          struct timeval timeout;
          long total_us_calcul;

          printf("go on\n");

          gettimeofday(&commence_trans,NULL);

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
            file_size=statbuf.st_size;
            printf("file size is %d\n", file_size);

            //get package number
            pak_num=file_size/MSGSIZE;
            if (file_size%MSGSIZE!=0){
              pak_num+=1;
            }
            //printf("total package number is %d\n", pak_num);

            struct package_info paquets[pak_num];
            for (int i = 0; i < pak_num; i++) {
              paquets[i].pac_ack=0;
            }
            struct rto_info rto;
            rto.DevRTT.tv_sec=(rtt_origin_us/2)/(int)1e6;
            rto.DevRTT.tv_usec=(rtt_origin_us/2)%(int)1e6;
            rto.SRTT.tv_usec=rtt_origin_us%(int)1e6;
            rto.SRTT.tv_sec=rtt_origin_us/(int)1e6;
            int rto_origin_us=MU*rtt_origin_us+DEE*rtt_origin_us;
            rto.RTO.tv_sec=rto_origin_us/(int)1e6;
            rto.RTO.tv_usec=rto_origin_us%(int)1e6;

            printf("transmission begin\n");
            fflush(stdout);

            memset(whole_file,0,sizeof(whole_file));
            fread (whole_file,1,file_size,fp);

            //prepare all packages for transmission
            for (int i = 0; i < pak_num; i++) {
              len=MSGSIZE;
              if (file_size-i*MSGSIZE<MSGSIZE){
                len=file_size-i*MSGSIZE;
              }
              paquets[i].pac_taille=len+6;
              paquets[i].pac_ack=0;
              //printf("package %.6d ready\n", i+1);
            }

            int file_end=0;
            int last_seq_ack=0;
            int last_seq_env=0;
            int end_window;

            while (!file_end) {
              char msgbuffer[MSGSIZE+6];
              end_window=last_seq_ack+cwnd;
              if (end_window>=pak_num) {
                end_window=pak_num;
                //printf("almost done, pak_num %d\n", pak_num);
              }
              //printf("last_seq_env %d\n", last_seq_env);
              //printf("end window %d\n", end_window);

              //send all packages in slide window
              for (int i = last_seq_env; i < end_window; i++) {
                memset(tembuffer,0,MSGSIZE);
                memcpy(tembuffer,whole_file+i*MSGSIZE,paquets[i].pac_taille-6);
                memset(msgbuffer,0,MSGSIZE+6);
                sprintf(msgbuffer,"%.6d",i+1);
                memcpy(msgbuffer+6,tembuffer,paquets[i].pac_taille-6);
                sendto(socket_transmission,msgbuffer,paquets[i].pac_taille,0,(struct sockaddr*)&client_addr,c_len);
                //printf("package %d send\n", i+1);
                gettimeofday(&paquets[i].t_send,NULL);
                last_seq_env=i+1;
              }

              fd_set readfds;
              FD_ZERO(&readfds);
              FD_SET(socket_transmission,&readfds);
              timeout.tv_sec=rto.RTO.tv_sec;
              timeout.tv_usec=rto.RTO.tv_usec;
              //printf("timeout is %lds %ldus\n", timeout.tv_sec, timeout.tv_usec);
              int resul=select(socket_transmission+1,&readfds,NULL,NULL,&timeout);

              //waiting for ack
              if (FD_ISSET(socket_transmission,&readfds)) {
                memset(ackbuffer,0,ACKSIZE);
                recvfrom(socket_transmission,ackbuffer,ACKSIZE,0,(struct sockaddr*)&client_addr,&c_len);
                //printf("rcvd %s\n", ackbuffer);
                //printf("last pak ack is %d\n", last_seq_ack);
                char sequence[6];
                int seqack;
                memset(sequence,0,6);
                memcpy(sequence,ackbuffer+3,6);
                seqack=atoi(sequence);
                if (seqack==last_seq_ack && paquets[seqack].pac_ack>2) {
                  //printf("last pak ack is %d\n", last_seq_ack);
                  //printf("last pak multi ack, pak lose, retrans pak %d\n",last_seq_ack+1);
                  memset(tembuffer,0,MSGSIZE);
                  memcpy(tembuffer,whole_file+last_seq_ack*MSGSIZE,paquets[last_seq_ack].pac_taille-6);
                  memset(msgbuffer,0,MSGSIZE+6);
                  sprintf(msgbuffer,"%.6d",last_seq_ack+1);
                  memcpy(msgbuffer+6,tembuffer,paquets[last_seq_ack].pac_taille-6);
                  sendto(socket_transmission,msgbuffer,paquets[last_seq_ack].pac_taille,0,(struct sockaddr*)&client_addr,c_len);
                  //printf("package %d resend\n", last_seq_ack+1);
                  gettimeofday(&paquets[last_seq_ack].t_send,NULL);
                  //sleep(0.1);
                }
                if (seqack>last_seq_ack) {
                  last_seq_ack=seqack;

                  //when it's first ack of pak, calcul RTO
                  if (paquets[seqack].pac_ack==0){
                    gettimeofday(&paquets[seqack].t_rcvd,NULL);
                    total_us_calcul=1e6*(paquets[seqack].t_rcvd.tv_sec-paquets[seqack].t_send.tv_sec)+(paquets[seqack].t_rcvd.tv_usec-paquets[seqack].t_send.tv_usec);
                    rto.RTT.tv_sec=total_us_calcul/1e6;
                    rto.RTT.tv_usec=total_us_calcul%(int)1e6;
                    printf("RTT is %lds %ldus\n", rto.RTT.tv_sec,rto.RTT.tv_usec);
                    calcul_package_RTO(rto);
                  }
                }
                paquets[seqack].pac_ack+=1;
              }
              if(resul==0){
                //printf("timeout, retrans pacakge %d\n",last_seq_ack+1);
                memset(tembuffer,0,MSGSIZE);
                memcpy(tembuffer,whole_file+last_seq_ack*MSGSIZE,paquets[last_seq_ack].pac_taille-6);
                memset(msgbuffer,0,MSGSIZE+6);
                sprintf(msgbuffer,"%.6d",last_seq_ack+1);
                memcpy(msgbuffer+6,tembuffer,paquets[last_seq_ack].pac_taille-6);
                sendto(socket_transmission,msgbuffer,paquets[last_seq_ack].pac_taille,0,(struct sockaddr*)&client_addr,c_len);
                //printf("package %d resend\n", last_seq_ack+1);
                gettimeofday(&paquets[last_seq_ack].t_send,NULL);
                //sleep(0.1);
              }

              //all pacakges transed and acked
              if (last_seq_ack==pak_num) {
                file_end=1;
              }
            }

            //sleep(0.5);
            for (int i = 0; i < 5; i++) {
              sendto(socket_transmission,"FIN",3,0,(struct sockaddr*)&client_addr,c_len);
            }
            printf("transmission done\n");
            fclose(fp);
            conter=0;
          }
        }
        gettimeofday(&terminus_trans,NULL);
        time_trans=1e6*(terminus_trans.tv_sec-commence_trans.tv_sec)+(terminus_trans.tv_usec-commence_trans.tv_usec);
        float debit=(float)(file_size)/(float)time_trans;
        printf("start at %lds %ldus, end at %lds %ldus\n", commence_trans.tv_sec,commence_trans.tv_usec,terminus_trans.tv_sec,terminus_trans.tv_usec);
        printf("transmission last %ldus\n", time_trans);
        printf("file size is %dB\n", file_size);
        printf("tatal packages transed %d\n", pak_num);
        printf("le debit est %f MB/s\n", debit);

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
