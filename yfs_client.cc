// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#define ROOTINUM 0x00000001
#include <cstdlib>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  
  //add root to extent server
  ec->put(ROOTINUM, std::string());
  
  //create generator
  gen = new generator();
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    fprintf(stderr, "within getdir error");
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int
yfs_client::lookup(std::string name, inum parent, dirent& ent){
  assert(isdir(parent) && parent == (parent & 0xFFFF));
  printf("lookup parent: %016llx name: %s\n", parent, name.c_str());
  string buf;
  assert(ec->get(parent, buf) == extent_protocol::OK);

  Directory parentDir(buf);

  list<dirent>::iterator  it;
  for  (it = parentDir.begin(); it != parentDir.end() && it -> name != name; it++){}

  if (it != parentDir.end()){
    //TODO: make sure this is actually making a copy.
    ent = *it;
    printf("lookup OK: %016llx name %s, ent.inum: %016llx\n", parent, name.c_str(), ent.inum);
    return yfs_client::OK;
  } else {
    printf("lookup NOENT: %016llx name %s\n", parent, name.c_str());
    return yfs_client::NOENT;
  }
 
}

int 
yfs_client::mkfile(std::string name, inum parent, inum& ret){
  assert(isdir(parent) && parent == (parent & 0xFFFF));

  printf("mkfile parent: %016llx name: %s\n", parent,   name.c_str());
  string buf;
  assert(ec->get(parent, buf) == extent_protocol::OK);

  Directory parentDir(buf);
  inum inu = gen->fileinum();
  dirent newfile; 
  newfile.name = name;
  newfile.inum = inu;

  std::list<dirent>::iterator it;
  for  (it = parentDir.begin(); it != parentDir.end() && it -> name != name; it++){}
  
  if (it != parentDir.end()){
    printf("mkfile EXIST\n");
      return yfs_client::EXIST;
  } 

  parentDir.insert_entry(newfile);
  ret = inu;
  std::string ser = parentDir.serialize();
  assert((ec->put(parent, ser)) == extent_protocol::OK);
  assert((ec->put(ret, "")) == extent_protocol::OK);
  printf("mkfile newinum: %016llx name: %s\n", inu, name.c_str());
  return yfs_client::OK;
}

int 
yfs_client::readdir(inum inumber, list<dirent>& dirlist){
  assert(isdir(inumber) && inumber == (inumber & 0xFFFF));
  printf("readdir.  inum: %016llx\n", inumber);  
  string buf;

  assert(ec->get(inumber,buf) == extent_protocol::OK);
  Directory dir(buf);

  dirlist = dir.entries;

  return yfs_client::OK;
}


int
yfs_client::setattr(inum fileinum, unsigned int size){
  assert(isfile(fileinum));
  std::string st;

  int ret;
  assert((ret = ec->get(fileinum, st)) == extent_protocol::OK);
  
  if (st.size() > size){
    st = st.substr(size);
  } else {
    //TODO: is it okay to add 'x' on extension
    st = st + std::string('x', size - st.size());
  }

  assert(ret = (ec->put(fileinum, st) == extent_protocol::OK));
  return ret;
}

std::string
yfs_client::readfile(inum finum, unsigned int size, unsigned int off){
  std::string st;
  assert(ec->get(finum,st));
  assert(isfile(finum));
  std::string ret;

  if (off + size <= st.size()){
    ret = st.substr(off, size);
  } else {
    ret =  st.substr(off, st.size() - off) + std::string('\0', size + off - st.size());
  }
  
  assert(ret.size() == size);
  return ret;
}

int
yfs_client::writefile(inum finum, std::string contents, unsigned int off){
  std::string current;
  assert(ec->get(finum, current) == extent_protocol::OK);
  
  std::string newstring = current;
  int written = min(newstring.size() - off, contents.size());

  newstring.replace(off, written, contents, 0, written);
  assert(ec->put(finum,newstring) == extent_protocol::OK);

  return written;
}

/* apparently not for lab2*/
int 
yfs_client::mkdir(std::string name, inum parent){
  assert(isdir(parent) && parent == (parent & 0xFFFF));
  
  string buf; 
  assert(ec->get(parent, buf) == extent_protocol::OK);

  Directory parentDir(buf);
  dirent newdir(name, gen->dirinum());

  parentDir.insert_entry(newdir);
  assert(ec->put(parent, parentDir.serialize()) == extent_protocol::OK);

  return yfs_client::OK;
}


/* ----------Directory data structure methods ---------------------------*/

yfs_client::Directory::Directory(std::string& serial){
  //parse string and generate structures
  unsigned int pos = 0;
 
  while (pos < serial.size()){
    //read in the name
    unsigned int len;
    sscanf(serial.substr(pos, NAMELENBYTES).c_str(), NAMELENFORMAT, &len);
    pos += NAMELENBYTES;
    string filename = serial.substr(pos, len);
      
    //read in the inum
    pos += len;
    unsigned long long int in;
    sscanf(serial.substr(pos, INUMBYTES).c_str(), INUMFORMAT, &in);

    pos += INUMBYTES;
    dirent entry(filename, (inum)in);
    entries.push_back(entry);
  }

  //  printf("pos: %d. serial.size() %d", pos, serial.size());

  assert(pos == serial.size());

}

yfs_client::Directory::~Directory(){;}

std::string yfs_client::Directory::serialize(){
  // printf("start of serialize\n");
  list<dirent>::iterator it = entries.begin();
  char sizestring[NAMELENBYTES + 1];
  char inumstring[INUMBYTES + 1];

  std::string total;

  // printf("(seralize)after declaration\n");

  while (it != entries.end()){	
    // fprintf(stderr,"within loop\n");
	
    //4bytes
    sprintf(sizestring, NAMELENFORMAT, it->name.size());
    total.append(sizestring);

    //namestring
    total.append(it->name);

    //64bit inum 8 bytes 
    sprintf(inumstring, INUMFORMAT, it->inum);
    total.append(inumstring);
    it++;
  }

  // std::cout << "final string: " <<  total << std::endl;

  return total;
}  

void yfs_client::Directory::insert_entry(dirent&  entry){
  entries.push_back(entry);
}

  //returns iterator to internal list
  //(don't have time to think of how to make a proper iterator right now)
list<yfs_client::dirent>::iterator  yfs_client::Directory::begin(){
  return entries.begin();
}

list<yfs_client::dirent>::iterator yfs_client::Directory::end(){
  return entries.end();
}

/*--------- inum generator methods ------------------*/


yfs_client::generator::generator(){
  srand(0);
}

yfs_client::inum yfs_client::generator::fileinum(){
  return (yfs_client::inum) ((unsigned int)rand() | 0x80000000u);
}

yfs_client::inum yfs_client::generator::dirinum(){
  return (yfs_client::inum) ((unsigned int)rand() & 0x7FFFFFFFu);
}