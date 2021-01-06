while (!feof(fp)) {
  fd_set readfds;
  FD_ZERO(&readfds);
  /*char sequence[6];
  memset(sequence,0,6);
  int seqint=seq;
  int r;
  char exchange[2];*/

  for (int j = 0; j < cwnd; j++) {
    //sequence number from int to char
    /*for (int i = 0; i < sizeof(sequence); i++) {
      r=seqint%10;
      sprintf(exchange,"%d",r);
      sequence[sizeof(sequence)-1-i]=exchange[0];
      seqint=(seqint-r)/10;
    }*/

    printf("sequence is %3.9s\n",paquets[seq-1].ack_sequence );
    /*char msgbuffer[SEQSIZE+MSGSIZE];//whole msg for transmission
    memset(msgbuffer,0,SEQSIZE+MSGSIZE);
    sprintf(ackmsg,"%s%.6s","ACK",sequence);
    ackmsg[sizeof(ackmsg)-1]='\0';
    sprintf(paquets[seq-1].ack_sequence,"%s",ackmsg);*/
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
    }
    seq++;
    calcul_RTO();
  }
}
