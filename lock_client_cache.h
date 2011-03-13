// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <pthread.h>
#include <map>

using namespace std;
// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:

  enum st {NONE, ACQUIRING, LOCKED, FREE, RELEASING };
  
  //TODO:clean up uneeded attributes
  //add appropriate constructor defaults
  //what is a good pthread_t default?
  typedef struct local_lock {
    st state;
    bool retry_call_received;
    pthread_mutex_t protecting_mutex;
    pthread_cond_t cond;

  local_lock(): state(NONE), retry_call_received(false){
      pthread_mutex_init(&protecting_mutex,NULL);
      pthread_cond_init(&cond,NULL);
    }

  } local_lock_t;
  
  map<lock_protocol::lockid_t, local_lock> local_lock_table; 
  
  pthread_mutex_t local_table_mutex;
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
