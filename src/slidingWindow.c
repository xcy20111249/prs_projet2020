#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>

#define RCVSIZE 1400
#define MIN_PORT 1000
#define MAX_PORT 9999

char file_buffer_all[3000000000];

char* get_sequence_number(int nb){
  char *sequence_number = malloc(6*sizeof(char));
  sprintf(sequence_number,"%06d",nb);
  return sequence_number;
}


int main(int argc, char *argv []){
  int public_desc,private_desc;
  int public_port = atoi(argv[1]);
  int private_port = 8080;
  int reuse = 1;
  int connected = 0;
  int acked = 1; //if a sequence is acked, 0 if not
  int message_SYNACK_size, send_file_size, cwnd; //cwnd: window size, sstrash: 慢启动阈值;
  int sstrash = 10;
  fd_set rset;
  int nready;


  ssize_t msg_size,file_name_size, receive_size;//msg=之前用来传输的msg,file_request=客户端要求传输的文件

  char buffer[RCVSIZE];
  char receive_buffer[RCVSIZE];
  char* message_SYNACK = "SYN-ACK8080";
  char* message_FIN = "FIN";
  char* sequence_number;

  struct sockaddr_in public_addr, private_addr;

  //Create public socket
  public_desc = socket(AF_INET, SOCK_DGRAM, 0);
  if(public_desc < 0){
    perror("Cannot create public socket\n");
    return -1;
  }
  setsockopt(public_desc,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

 memset(&public_addr,0,sizeof(public_addr));
 memset(&private_addr,0,sizeof(private_addr));

 public_addr.sin_family = AF_INET;
 public_addr.sin_port = htons(public_port);
 public_addr.sin_addr.s_addr = htonl(INADDR_ANY);

 private_addr.sin_family = AF_INET;
 private_addr.sin_port = htons(private_port);
 private_addr.sin_addr.s_addr = htonl(INADDR_ANY);

 struct timeval socket_timeout;
 socket_timeout.tv_sec = 0;
 socket_timeout.tv_usec = 500000;

 //initialize socket, binding public address structure to public_desc
 if (bind(public_desc,(struct sockaddr*) &public_addr, sizeof(public_addr)) == -1){
   perror("BIND failed\n");
   close(public_desc);
   return -1;
 }

 while (1){
   socklen_t public_addr_size = sizeof(public_addr);
   socklen_t private_addr_size = sizeof(private_addr);

   struct timeval start, stop,start_rtt,stop_rtt;
   time_t start_total,stop_total;
   double rtt,rtt_now;
   //connecting & handshakes

   char temp1[15];
   char temp2[15];
   char temp3[15];
   char temp4[15];

   strcpy(temp1,"SYN");
   strcpy(temp2,"ACK");
   bzero(buffer,RCVSIZE);
   msg_size = recvfrom(public_desc,buffer, sizeof(buffer),0,(struct sockaddr*)&public_addr,&public_addr_size);//receiving first ack
   strcpy(temp3, buffer);
   if ((msg_size >= 0) && (strcmp(temp1, temp3) == 0)){
     gettimeofday(&start,NULL);
     message_SYNACK_size = sendto(public_desc,(const char*)message_SYNACK,strlen(message_SYNACK), 0, (struct sockaddr*)&public_addr,public_addr_size);
     printf("connection state: state 1, last msg received:%s \n",buffer);

     bzero(buffer,RCVSIZE);
     msg_size = recvfrom(public_desc, buffer, sizeof(buffer),0,(struct sockaddr*)&public_addr,&public_addr_size);
     strcpy(temp4,buffer);
     if ((msg_size >= 0) && (strcmp(temp2,temp4) == 0)){
       gettimeofday(&stop,NULL);
       printf("connection state: state 2, last msg received:%s \n",buffer);
     }
     // calculate first rtt;
    rtt =  stop.tv_sec - start.tv_sec;
    rtt = rtt*(1E6) + (stop.tv_usec - start.tv_usec);
    rtt_now = rtt;//first time: rtt_now = rtt;

     //set port and selection
     private_desc = socket(AF_INET, SOCK_DGRAM,0);
     if(private_desc < 0){
       perror("Cannot create private socket\n");
       return -1;
     }
     setsockopt(private_desc,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

     private_addr.sin_family = AF_INET;
     private_addr.sin_port = htons(private_port);//*******do sth
     private_addr.sin_addr.s_addr = htonl(INADDR_ANY);

     if (bind(private_desc,(struct sockaddr*) &private_addr, sizeof(private_addr)) == -1){
       perror("BIND failed\n");
       close(private_desc);
       return -1;
     }

     printf("private channel established\n");

     //receiving file name;
     bzero(buffer,RCVSIZE);
     file_name_size =recvfrom(private_desc,buffer,sizeof(buffer),0,(struct sockaddr*)&private_addr, &private_addr_size);
     if (file_name_size <0){
       perror("file name reception failed :( \n");
       return -1;
     }
     printf("file requested by the client is: \n");
     puts(buffer);

     FILE * file_name;
     file_name = fopen(buffer,"r");
     if (file_name == NULL){
       printf("file not found ;( \n");
       exit(1);
     }

     //get file total size
     fseek(file_name, 0, SEEK_END);
     int file_size, total_sequence;
     file_size = ftell(file_name);
     total_sequence = file_size/(RCVSIZE -6) +1; // add 1 because it's not integer
     rewind(file_name);

     bzero(file_buffer_all,file_size);
     fread(&file_buffer_all,1,file_size,file_name);
     fclose(file_name);
     int sizeofbuffer = strlen(file_buffer_all);

     cwnd = 70;
     int ack_received;
     int max_ack_received = 0;
     int last_ack;//temp last ack
     int rep = 0;//a nb to set fast_retrans
     int fast_retrans = 0;
     struct timeval time_array [total_sequence];// getting an array to stock start time
     time(&start_total);


     while (max_ack_received < total_sequence){

       if (fast_retrans ==1){
         memset(buffer,0,RCVSIZE);
         sequence_number = get_sequence_number(last_ack+1);//sending next sequence
         memcpy(&buffer[0],sequence_number,6);
         memcpy(&buffer[6],&file_buffer_all[(last_ack)*(RCVSIZE-6)],RCVSIZE-6);
         gettimeofday(&start_rtt,NULL);
         time_array[last_ack] = start_rtt;
         send_file_size = sendto(private_desc,buffer,RCVSIZE,0,(struct sockaddr*)&private_addr,private_addr_size);
         fast_retrans = 0;
         rep =0;
         last_ack = 0;
       }

       else if ((max_ack_received + cwnd < total_sequence) && (fast_retrans == 0)){
         for (int j = max_ack_received + 1; j <= max_ack_received+cwnd;j++){
           memset(buffer,0,RCVSIZE);
           sequence_number = get_sequence_number(j);
           memcpy(&buffer[0],sequence_number,6);
           memcpy(&buffer[6],&file_buffer_all[(j-1)*(RCVSIZE-6)],RCVSIZE-6);
           gettimeofday(&start_rtt,NULL);
           time_array[j-1] = start_rtt;
           send_file_size = sendto(private_desc,buffer,RCVSIZE,0,(struct sockaddr*)&private_addr,private_addr_size);

         }
       }

       else if(max_ack_received + cwnd >= total_sequence){
         for (int j = max_ack_received + 1; j < total_sequence; j++){
           memset(buffer,0,RCVSIZE);
           sequence_number = get_sequence_number(j);
           memcpy(&buffer[0],sequence_number,6);
           memcpy(&buffer[6],&file_buffer_all[(j-1)*(RCVSIZE-6)],RCVSIZE-6);
           gettimeofday(&start_rtt,NULL);
           time_array[j-1] = start_rtt;
           send_file_size = sendto(private_desc,buffer,RCVSIZE,0,(struct sockaddr*)&private_addr,private_addr_size);
         }

         bzero(buffer,RCVSIZE);
         sequence_number = get_sequence_number(total_sequence);
         memcpy(&buffer[0],sequence_number,6);
         memcpy(&buffer[6],&file_buffer_all[(total_sequence-1)*(RCVSIZE-6)], file_size-(total_sequence-1)*(RCVSIZE-6)+6);
         gettimeofday(&start_rtt,NULL);
         time_array[total_sequence-1] = start_rtt;
         send_file_size = sendto(private_desc,buffer,file_size-(total_sequence-1)*(RCVSIZE-6)+6,0,(struct sockaddr*)&private_addr,private_addr_size);
       }

         nready = 0;
         FD_ZERO(&rset);
         FD_SET(private_desc,&rset);
         socket_timeout.tv_usec = 0*rtt + 1*rtt_now;

         nready = select(private_desc+1,&rset,NULL,NULL,&socket_timeout);

       if (FD_ISSET(private_desc,&rset)){
         recvfrom(private_desc,receive_buffer, sizeof(receive_buffer),0,(struct sockaddr*)&private_addr,&private_addr_size);
         gettimeofday(&stop_rtt,NULL);
         ack_received = atoi(&receive_buffer[3]);
         if (ack_received == last_ack){
           rep++;
           if (rep ==3){
             fast_retrans =1; // received 3 times same ack,considering fast retransmit
           }
         } else {
           rtt = rtt_now;//passing last rtt_now to rtt;
           rep =0;
           last_ack = ack_received;// this ack is becoming to last ack...
           rtt_now = (stop_rtt.tv_sec - time_array[ack_received-1].tv_sec)*1E6 + stop_rtt.tv_usec - time_array[ack_received-1].tv_usec; //calculate rtt only when it is not fast retransmit
           socket_timeout.tv_usec = 0*rtt + 1*rtt_now;

         }
       }

       if (nready == 0){
          printf("select timeout :( \n");
       }

       if (ack_received > max_ack_received){
         max_ack_received = ack_received;
       }
     }

     for (int j = 0; j<= 10000;j++){
       msg_size =sendto(private_desc,(const char*)message_FIN, strlen(message_FIN), 0, (struct sockaddr*)&private_addr,private_addr_size);//sending FIN
     }
     printf("file transmitted, closing \n");
     time(&stop_total);
     double used_time_total;
     used_time_total = difftime(stop_total,start_total);

     printf("debit of this server is: %f\n", file_size/(used_time_total*1E6));
   }
   close(private_desc);
   }
   close(public_desc);
}
