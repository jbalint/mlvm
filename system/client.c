/** $RCSfile: client.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: client.c,v 1.2 2003/03/21 13:12:03 gautran Exp $ 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "system.h"
 
#define PORTNUM 2222

char* f[10] = {
	"floattest.lo",
	"str1.lo",
	"strtest.lo",
	"q10k.lo",
	"q50k.lo",
	"q100k.lo",
	"q500k.lo",
	"listtest.lo",
	"calltest.lo",
 	"exit"
};

int main() /* client process */
{
  int socket_id, i, j;
  struct sockaddr_in insock, to;
  ICB buf;


  socket_id=socket(AF_INET,SOCK_DGRAM,0);

  insock.sin_family=AF_INET;
  insock.sin_addr.s_addr= INADDR_ANY;
  insock.sin_port=0;

  bind(socket_id, (struct sockaddr*)&insock,sizeof(insock));;

  to.sin_family=AF_INET;
  to.sin_addr.s_addr= inet_addr("131.104.48.244");
  to.sin_port=PORTNUM;

/*
 while (1){
	printf("Input an imago name: ");
	scanf("%s", buf.name);
	printf("\nInput starting trace-num: ");
	scanf("%d", &buf.start_num);

  	sendto(socket_id, &buf, sizeof(buf), 0, (struct sockaddr*)&to, sizeof(to));
  
	if (strcmp(buf.name, "exit") == 0){
		printf("COMMUNICATOR terminates\n");
		break;
	}
 
  }
*/



  for (i = 0; i < sizeof(f); i++){
 	strcpy(buf.name, f[i]);
	if (strcmp(buf.name, "exit") == 0){
		printf("COMMUNICATOR terminates\n");
		break;
	}
 
  	sendto(socket_id, &buf, sizeof(buf), 0, (struct sockaddr*)&to, sizeof(to));

	j = 5000;
  	while(j--);


  }


  close(socket_id);
  return 0;
}


 
