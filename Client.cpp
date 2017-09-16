#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <math.h>
using namespace std;
#define BUFFER_LENGTH 1024

char buf[BUFFER_LENGTH];
//rolling my own
int packData(string data) {
  memset(&buf, 0, sizeof(buf));//clear the buffer
  cout << "packing...data: " << data << endl;
  // msg type
  char arr[1024];
  memset(arr, 0, 1024);
  size_t sz = 0, arrSz =0;
    strcpy(arr, data.c_str());
  sz += sizeof(char); arrSz += sizeof(char);
  memcpy(buf, arr, sz);
  // queue name len
  uint32_t nlength = atoi(arr+1);
  uint32_t nlen = htonl(nlength);
  int nDigits = 0;
  if(nlength)
    nDigits = floor(log10(nlength)) + 1;
  memcpy(buf + sz, &nlen, sizeof(uint32_t));
  sz += sizeof(uint32_t); arrSz += nDigits;
  //cout << "nlen: " << nlength << endl;
  // queue name
  memcpy(buf + sz, arr + arrSz, nlength);
  sz += nlength; arrSz += nlength;
  //cout << "q name: " << endl;
  // msg len
  uint32_t mlen = atoi(arr + arrSz);
  cout << "mesg len: " << mlen << endl;
  int mDigits;
  if(!mlen) return sz;
    mDigits = floor(log10(mlen)) + 1;
  nlen = htonl(mlen);

  memcpy(buf + sz, &nlen, sizeof(uint32_t));
  sz += sizeof(uint32_t); arrSz += mDigits;
  // msg
  memcpy(buf + sz, arr + arrSz, mlen);
  //cout << "sz: " << sz << endl;
  return sz+mlen;
}
//Client side
int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        cerr << "Usage: ip_address port" << endl; exit(0); 
    } //grab the IP address and port number 
    char *serverIp = argv[1]; int port = atoi(argv[2]); 
    //setup a socket and connection tools 
    struct hostent* host = gethostbyname(serverIp); 
    sockaddr_in sendSockAddr;   
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr)); 
    sendSockAddr.sin_family = AF_INET; 
    sendSockAddr.sin_addr.s_addr = 
      inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(port);
    int clientSd = socket(AF_INET, SOCK_STREAM, 0);
    //try to connect...
    int status = connect(clientSd,
                         (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));
    if(status < 0)
    {
        cout<<"Error connecting to socket!"<<endl;
	return -1;
    }
    cout << "Connected to the server!" << endl;
    int bytesRead, bytesWritten = 0;
    struct timeval start1, end1;
    gettimeofday(&start1, NULL);
    while(1)
    {
        cout << ">";
        string data;
        getline(cin, data);
        memset(&buf, 0, sizeof(buf));//clear the buffer
	int len;
	if(data == "produce" || data == "consume") {
	  strcpy(buf, data.c_str());
	  len = 7;
	} else {
	  len = packData(data);
	}
	cout << "len: " << len << endl;
        if(data == "exit")
        {
            send(clientSd, (char*)&buf, len, 0);
            break;
        }
        bytesWritten += send(clientSd, (char*)&buf, len, 0);
        //cout << "Awaiting server response..." << endl;
        memset(&buf, 0, sizeof(buf));//clear the buffer
	if((bytesRead = recv(clientSd, (char*)&buf, sizeof(buf), 0))) {
	  data = string(buf, 4);
	  if(data == "exit")
	    {
	      cout << "Server has quit the session" << endl;
	      break;
	    }
	  cout << "Server: " << buf << endl;
	}
    }
    gettimeofday(&end1, NULL);
    close(clientSd);
    cout << "********Session********" << endl;
    cout << "Bytes written: " << bytesWritten << 
    " Bytes read: " << bytesRead << endl;
    cout << "Elapsed time: " << (end1.tv_sec- start1.tv_sec) 
      << " secs" << endl;
    cout << "Connection closed" << endl;
    return 0;    
}
