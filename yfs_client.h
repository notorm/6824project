#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <list>
#include <cstdio>

#define NAMELENFORMAT "%04x"
#define NAMELENBYTES 4

#define INUMFORMAT "%08llx"
#define INUMBYTES 8


using namespace std;

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {

  extent_client *ec;


 public:
  lock_client *lc;

  typedef unsigned long long inum;

  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST, OFFERR };

  typedef int status;

  class generator {
  public:
    generator(int seed);
    yfs_client::inum fileinum();
    yfs_client::inum dirinum();

  };

 private:
  generator *gen;

 public:

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;

    dirent(std::string nm, yfs_client::inum num): name(nm), inum(num){;}
    dirent(){;};
  };


  class ScopedRemoteLock {
  private:
    lock_protocol::lockid_t locknum;
    lock_client* lc;

  public:
  ScopedRemoteLock(inum num, lock_client* client): locknum((lock_protocol::lockid_t) num), lc(client) {
      VERIFY(lc->acquire(locknum) == lock_protocol::OK);
    }

    ~ScopedRemoteLock(){
      VERIFY(lc->release(locknum) == lock_protocol::OK);
    }
  };



  class Directory {


  public:
    std::list<dirent> entries;
    //makes directory data structure from string
    Directory(std::string& serial);

    ~Directory();

    //makes string suitable for extent server
    std::string serialize();

    //inserts node into directory
    void insert_entry(dirent&  entry);

    void lookup(std::string name, std::list<dirent>::iterator& it);

  //begin of list iterator
    std::list<dirent>::iterator begin();

  //end of list iterator
    std::list<dirent>::iterator end();

    void remove_entry(list<dirent>::iterator it);
  
  };


 private:
  static std::string filename(inum);
  static inum n2i(std::string);


 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);

  //resizes files (trims or pads accordingly)
  int setattr(inum fileinum, unsigned int size);
  
  int getdir(inum, dirinfo &);
  
  // generates new file inum and adds relevant directory entry
  int mkfile(std::string name, inum parent, inum& ret);

  //  generates new directory inum, addes entry in parent.
  int mkdir(std::string name, inum parent, inum& ret);

  // deletes a file/directory.
  int unlink(inum parent, std::string name, inum & victim_num);

  //reads file contents
  int readfile(inum finum, unsigned int size, unsigned int off, std::string& ret);

  int writefile(inum finum, std::string contents, unsigned int off);

  //possibly to read the actual dir contents, for FUSE READDIR. need
  //to know how to communicate back to it. 
  int readdir(inum inumber, list<dirent>& dirlist);

  //fills up a dirent if node found, returns NOENT if not found.
  int lookup(std::string name, inum parent, dirent& ent);

  //takes a dir, marshalls it and writes it to extent
  int writedir();


};





#endif 
