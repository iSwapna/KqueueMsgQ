#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <unordered_map>
#include "kq.h"

int Kq::socketFd = 0;
int Kq::kQ = 0;
int Kq::eventsUsed = 0;
int Kq::eventsAlloc = 0;
int Kq::portNo = portNo;
int Kq::MAX = 0;
unordered_set<int> Kq::users;
unordered_set<int> Kq::prod;
unordered_set<int> Kq::cons;
struct kevent* Kq::kEvents = NULL;
unordered_map< string, unordered_set<int> > Kq::subs;

//Utility functions
void Kq::dumpQueueStats() {
  DEBUGLOG("***************** " );
  DEBUGLOG("QueueStats: No: "<<subs.size() );
  for(auto &entr: subs) {
    DEBUGLOG("Q Name: "<< entr.first << " :Consumers");
    DEBUGLOG("---------------- " );
    for(auto &fd: entr.second)
      DEBUGLOG(fd);
  }
  DEBUGLOG("---------------- " );
  DEBUGLOG("***************** " );

}

void Kq::createListen() {
  char addr[] = "127.0.0.1"; //localhost
  struct sockaddr_in serv;

  socketFd = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFd == -1)
    err(1, "socket");
  
  int i = 1;
  if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, (void *)&i, (socklen_t)sizeof(i)) == -1)
    err(1, "setsockopt");
  
  memset(&serv, 0, sizeof(struct sockaddr_in));
  serv.sin_family = AF_INET;
  serv.sin_port = htons(portNo);
  serv.sin_addr.s_addr = inet_addr(addr);
  i = ::bind(socketFd, (struct sockaddr *)&serv, (socklen_t)sizeof(serv));
  if (i == -1)
    err(1, "error on bind");
    i = listen(socketFd, 5);
  if (i == -1)
    err(1, "error on listen");
  DEBUGLOG("Listening on...:" <<  portNo);
  
}
void Kq::ackOk(int fd) {
  char buf[1024] = "OK";

  int i = ::write(fd, buf, 2);
  //DEBUGLOG("wrote to: " << fd );
  if (i == -1)
    warn("write failed!");
 
}
void Kq::processMessage(char* buf, int len, int fd) {

  /* CONNECT MSG FORMAT: 
   * +++++++++++++++++++++++++++++++++
   * + 7 Bytes "produce" || "consume"
   * +++++++++++++++++++++++++++++++++*/
  cout << "len: " << len << endl;
  string type = string(buf, len);
  if(type == PRODUCER) {
    DEBUGLOG("Prod: " << type << " " << fd);
    prod.insert(fd);
  } else if(type == CONSUMER) {
    DEBUGLOG("Cons: " << type << " " << fd);
    cons.insert(fd);
  } else if(gotMessage(buf, len, fd)) {
    DEBUGLOG("got a message: " << string(buf, len));
  } else if (gotSubscribe(buf, len, fd)) {
    DEBUGLOG("got a Subscribe: " << string(buf, len));
  } else if (gotUnsubscribe(buf, len, fd)) {
    DEBUGLOG("got an UnSubscribe" << string(buf, len));
  } else {
    DEBUGLOG("UNKNOWN MSG!!: " << string(buf, len));
  }
  ackOk(fd);
}

bool Kq::gotUnsubscribe(char* buf, int len, int fd) {
/* Unsubscribe MSG FORMAT: 
* B1 2345 1234.NLEN  
* +++++++++++++++++++
* +/|NLEN | Q NAME 
* +++++++++++++++++++*/
  if(cons.find(fd) == cons.end()||len == 0 || buf[0] != '/') return false;
  uint32_t nlen = ntohl(*(uint32_t *)(buf + sizeof(char)));
  size_t sz = sizeof(char) + sizeof(uint32_t);
  string qName = string(buf + sz, nlen);
  DEBUGLOG("Unsub: NLEN:"<<nlen <<":qName:"<< qName );
  if(subs.find(qName) != subs.end()) 
    subs[qName].erase(fd);
  dumpQueueStats();
  return true;
}

bool Kq::gotMessage(char* buf, int len, int fd) {
/* MESSAGE MSG FORMAT: 
* B1 2345 1234.NLEN 1234 123...MLEN
* +++++++++++++++++++++++++++++++++
* +*|NLEN | Q NAME  |MLEN| MSG
* +++++++++++++++++++++++++++++++++*/  
  if(prod.find(fd) == prod.end()||len == 0 || buf[0] != '*') return false;
  uint32_t nlen = ntohl(*(uint32_t *)(buf + sizeof(char)));
  size_t sz = sizeof(char) + sizeof(uint32_t);
  string qName = string(buf + sz, nlen);
  sz += nlen;
  DEBUGLOG("Pub: NLEN:"<<nlen<<":qName:"<<qName);
  uint32_t  mlen = ntohl(*(uint32_t *)(buf + sz));

  sz += sizeof(uint32_t);
  string messg = string(buf + sz, mlen);
  DEBUGLOG("Pub: NLEN:"<<nlen<<":qName:"<<qName<<":MLEN:"<<mlen<<":msg:"<< messg);
  
  //trigger write to subscribed consumers
  struct eventData *clData =
    (struct eventData *) malloc(sizeof(struct eventData));
  memset(clData->buf, 0, sizeof(clData->buf));
  memcpy(clData->buf, &messg, sizeof(messg));
  clData->onRead = NULL;
  clData->onWrite = handleWrite;
  //trigger only if we have consumers
  if(subs.find(qName) != subs.end()) {
    for(auto &clfd: subs[qName])
      eventChange(clfd, EVFILT_WRITE, EV_ADD, clData);
  }

  return true;
}
bool Kq::gotSubscribe(char* buf, int len, int fd) {
/* Subscribe MSG FORMAT:
* B1 2345 1234.NLEN
* +++++++++++++++++++
* +>|NLEN | Q NAME
* +++++++++++++++++++*/
  if(cons.find(fd) == cons.end()||len == 0 || buf[0] != '>') return false;
  uint32_t nlen = ntohl(*(uint32_t *)(buf + sizeof(char)));
  size_t sz = sizeof(char) + sizeof(uint32_t);
  string qName = string(buf + sz, nlen);
  DEBUGLOG("Sub: NLEN:"<<nlen <<":qName:"<< qName );
  
  if(subs.end() != subs.find(qName)) {
    subs[qName].insert(fd);
  } else {
    subs.emplace(qName, unordered_set<int>());
    subs[qName].insert(fd);
  }
  dumpQueueStats();
  return true;
}

void Kq::handleWrite(struct eventData *self, struct kevent *e) {
  DEBUGLOG("In handle write" );
  int i = ::write(e->ident, self->buf, BUFFER_SIZE);
  eventChange(e->ident, EVFILT_WRITE, EV_DELETE, self);
  DEBUGLOG("wrote to: " << e->ident );
  if (i == -1)
    warn("write failed!");     
}
void Kq::handleRead(struct eventData *self, struct kevent *e) {
  
  memset(self->buf, 0, sizeof(self->buf));
  int n = ::read(e->ident, self->buf, BUFFER_SIZE);

  if (n < 0) {
    DEBUGLOG("Read failed"); return;
  }
  // TBD: read what could be left in the buffer
  if (n == 0) {
    eventChange(e->ident, EVFILT_READ, EV_DELETE, self);
    free(self);
    close(e->ident);
    users.erase(e->ident);
    return;
  }
  DEBUGLOG("got a message from: " << e->ident);
  processMessage(self->buf, n, e->ident);
}

void Kq::handleConnect(struct eventData *self, struct kevent *e) {
  //server socket, there is a client to accept
  struct sockaddr_in cl;
  socklen_t len = (socklen_t)sizeof(cl);
  int clFd = accept(socketFd, (struct sockaddr *)&cl, &len);
  if (clFd == -1)
    err(1, "accept!");
  
  users.insert(clFd);
  MAX++;
  struct eventData *clData =
    (struct eventData *) malloc(sizeof(struct eventData));
  clData->onRead = handleRead;
  clData->onWrite = handleWrite;

  eventChange(clFd, EVFILT_READ, EV_ADD, clData);
  DEBUGLOG("connection added from: " << clFd);
}

void Kq::eventChange (int ident, int filter, int flags, void *udata) {
  struct kevent *e;

  if (eventsAlloc == 0) {
    eventsAlloc = 64;
    kEvents = (struct kevent *)malloc(eventsAlloc * sizeof(struct kevent));
  }
  if (eventsAlloc <= eventsUsed) {
    eventsAlloc *= 2;
    kEvents = (struct kevent *)realloc(kEvents, eventsAlloc * sizeof(struct kevent));
  }
  
  int index = eventsUsed++;
  e = &kEvents[index];
  e->ident = ident;
  e->filter = filter;
  e->flags = flags;
  e->fflags = 0;
  e->data = 0;
  e->udata = udata;
}

int Kq::run() {

  kQ = kqueue();
  if(kQ == -1)
    err(1, "kqueue could not be created");
  
  struct eventData server = {
    .onRead = handleConnect,
    .onWrite = NULL
  };
  eventChange(socketFd, EVFILT_READ, EV_ADD, &server);
  
  //process events forever!!
  int newEvents;
  
  while (1) {
    newEvents = kevent(kQ, kEvents, eventsUsed, kEvents, eventsAlloc, NULL);
    if (newEvents < 0)
      err(1, "Event loop failed");

    //reset global event count for memory management
    eventsUsed = 0;

    for (int i = 0; i < newEvents; i++) {
      struct kevent *e = &kEvents[i];
      struct eventData *data = (struct eventData *) e->udata;
      if (data == NULL) continue;
      if (data->onWrite != NULL && e->filter == EVFILT_WRITE)
	data->onWrite(data, e);
      if (data->onRead != NULL && e->filter == EVFILT_READ)
	data->onRead(data, e);
    }
  }
  return 0;
}

int main (int argc, char *argv[]) {
  if (argc < 2) {
    cout << " No port no given" << endl;
    exit(1);
  }
  int portNo = atoi(argv[1]);
  Kq::portNo = portNo;
  Kq* kq = new Kq();
  kq->run();
  close(Kq::kQ);
  delete kq;
  cout << " DONE!! " << endl;
  return 0;
}
