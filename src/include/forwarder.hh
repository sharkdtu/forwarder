#include <string>

#include "hash_map.hh"

namespace rusv
{

const unsigned int LISTENQ = 100;

//const unsigned int BUFSIZE = 1024;

const unsigned short int planetlab_port = 48888;

//const char* conf_file = "conf/forwarder.xml";

enum Recv_state
{
	RECEIVING,
	RECEIVED,
	CLOSED,
	ERROR
};

class Recv_buf
{
public:
	Recv_buf(int fd, int size) : sockfd(fd), bufsize(size), nbyte(0) 
	{
		alloc(size);
	}

	Recv_state recv();

	void alloc(int size)
	{
		if(size > 1024)
			bufptr = new char[size];
		else
			bufptr = buffer;
	}

	void realloc(int size)
	{
		if(size > 1024)
		{
			char* newbuf = new char[size];
			memcpy(newbuf, bufptr, bufsize);
			if(bufptr != buffer)
				delete [] bufptr;
			bufptr = newbuf;
		}
		bufsize = size;
	}

	void dealloc()
	{
		if(bufptr != buffer)
			delete [] bufptr;
		nbyte = 0;
		bufsize = 0;
	}

	void reset(int size)
	{
		if(size > 1024)
		{
			char* newbuf = new char[size];
			memcpy(newbuf, bufptr, bufsize);
			if(bufptr != buffer)
				delete [] bufptr;
			bufptr = newbuf;
		}
		bufsize = size;
		nbyte = 0;
	}

	char* get_bufptr() const
	{
		return bufptr;
	}

	int get_bufsize() const
	{
		return bufsize;
	}

	~Recv_buf()
	{
		if(bufptr != buffer)
			delete [] bufptr;
	}

private:
	int sockfd;
	char* bufptr;
	int bufsize;
	int nbyte;
	char buffer[1024];
};

class Forwarder
{
public:
	Forwarder();
	~Forwarder();

	void run();

private:
	hash_map<std::string, std::string> ip_map;
	hash_map<std::string, int> ip_to_port;
	hash_map<int, int> port_to_sockfd;
	hash_map<int, Recv_buf*> sockfd_to_recvbuf;
};

}// namespace rusv
