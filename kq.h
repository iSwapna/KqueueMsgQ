#include<map>
#include<unordered_set>
#include<iostream>

using namespace std;

#define DEBUG 1
#ifdef DEBUG
#define DEBUGLOG(msg) cout << __FILE__ << "("<< __LINE__ <<"): "<< msg << endl;
#else
#define DEBUGLOG(msg)
#endif

#define BUFFER_SIZE 1024

//TBD: should be namespaced out to avoid global scoping
struct eventData {
  char buf[BUFFER_SIZE];
  int bufRead;
  int bufWrite;

  void (*onRead) (struct eventData *self, struct kevent *event);
  void (*onWrite) (struct eventData *self, struct kevent *event);
};
const string PRODUCER = "produce";
const string CONSUMER = "consume";
// define classes
class Kq;

class Kq
{
 public:
 Kq() {
    createListen();
  }
  
  ~Kq() {};
  
  int run(); 

  static int portNo;
  static int socketFd, kQ;
  static struct kevent* kEvents;
  static int eventsUsed;
  static int eventsAlloc;
  static unordered_map< string, unordered_set<int> > subs;
  static unordered_set<int> users;
  static unordered_set<int> prod;
  static unordered_set<int> cons;
  static int MAX; // current no of connections
 private:
  static void handleRead(struct eventData *self, struct kevent *e);
  static void handleWrite(struct eventData *self, struct kevent *e);
  static void handleConnect(struct eventData *self, struct kevent *e);
  static void ackOk(int fd);
  static void processMessage(char* buf, int len, int fd);
  static bool gotUnsubscribe(char* buf, int len, int fd);
  static bool gotMessage(char* buf, int len, int fd);
  static bool gotSubscribe(char* buf, int len, int fd);
  static void eventChange(int ident, int filter, int flags, void* data);
  //void consume();
  static void createListen();
  static void dumpQueueStats();
  
  

};
