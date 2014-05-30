#include <string>

#include "hash_map.hh"

namespace rusv
{

const unsigned int LISTENQ = 100;

//const unsigned int BUFSIZE = 1024;

const unsigned short int planetlab_port = 8888;

//const char* conf_file = "conf/forwarder.xml";

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
};

}// namespace rusv
