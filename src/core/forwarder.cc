#include "forwarder.hh"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdexcept>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "vlog.hh"

using namespace std;

namespace rusv
{

struct Pkt_header
{
        uint8_t version;
        uint8_t type;
        uint16_t datalen;
        struct in_addr src_ip, dst_ip;
};


enum Pkt_type
{
        HELLO,
        IP
};

static Vlog_module lg("forwarder");

const char* conf_file = "conf/forwarder.xml";

static void make_sockfd_non_block(int sockfd)
{
	int flags = fcntl(sockfd, F_GETFL, 0);
	if(flags < 0)
        {   
                lg.err("fcntl error(%s)", strerror(errno));
                exit(EXIT_FAILURE);
        }   

	if(fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
                lg.err("fcntl error(%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

Forwarder::Forwarder()
{
	xmlKeepBlanksDefault (0);
	xmlDocPtr doc = xmlParseFile(conf_file); 
	if(doc == NULL)
	{
		lg.err("loading config file failed");
		exit(EXIT_FAILURE);
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if(root == NULL)
	{
		lg.err("config file error");
		xmlFreeDoc(doc);
		exit(EXIT_FAILURE);
	}

	root = root->xmlChildrenNode;
	if(root != NULL)
	{
		xmlNodePtr cur = root->xmlChildrenNode;
		while(cur != NULL)
		{
			xmlNodePtr ipptr = cur->xmlChildrenNode;
			string pkt_src_ip((char*) xmlNodeGetContent(ipptr));
			ipptr = ipptr->next;
			string pkt_dst_ip((char*) xmlNodeGetContent(ipptr));
			ip_map[pkt_src_ip] = pkt_dst_ip;
			ip_map[pkt_dst_ip] = pkt_src_ip;
			ip_to_port[pkt_src_ip] = -1;
			ip_to_port[pkt_dst_ip] = -1;
			cur=cur->next;
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
}


Forwarder::~Forwarder()
{

}

void Forwarder::run()
{

	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0)
	{
		lg.err("socket error(%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(planetlab_port);

	if(::bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0)
	{
		lg.err("bind error(%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}

	make_sockfd_non_block(listenfd);

	if(listen(listenfd, LISTENQ) < 0)
	{
		lg.err("listen error(%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}

	int epollfd = epoll_create(512);
	if(epollfd < 0)
	{
		lg.err("epoll_create error(%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct epoll_event event;
	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLET;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event) < 0)
	{
		lg.err("epoll_ctl error(%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Buffer where events are returned */
	int max_event = ip_map.size() + 1;
	struct epoll_event* events = (struct epoll_event*)calloc(max_event, sizeof(struct epoll_event));

	for(; ;)
	{
		int nready = epoll_wait(epollfd, events, max_event, -1);
		for(int i = 0; i < nready; i++)
		{
			if ((events[i].events & EPOLLERR) || 
				(events[i].events & EPOLLHUP) || 
				(!(events[i].events & EPOLLIN)))
			{

				lg.err("epoll event error(%s)", strerror(errno));
				if (epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]) < 0)
				{
					lg.err("epoll_ctl error(%s)", strerror(errno));
					exit(EXIT_FAILURE);
				}
				close (events[i].data.fd);
				continue;
			}

			else if(events[i].data.fd == listenfd)
			{
				while(1)
				{
					struct sockaddr_in cliaddr;
					socklen_t cliaddrlen = sizeof(struct sockaddr_in);
					int connsockfd = accept(listenfd, (struct sockaddr*) &cliaddr, &cliaddrlen);
					if(connsockfd < 0)
					{
						if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
						{
							lg.err("accept error(%s)", strerror(errno));
						}
						break;
					}

					lg.dbg("new connection from %s", inet_ntoa(cliaddr.sin_addr));

					int port = ntohs(cliaddr.sin_port);
					port_to_sockfd[port] = connsockfd;

					make_sockfd_non_block(connsockfd);

					struct epoll_event ev;
					ev.data.fd = connsockfd;
					ev.events = EPOLLIN | EPOLLET;
					if(epoll_ctl(epollfd, EPOLL_CTL_ADD, connsockfd, &ev) < 0)
					{
						lg.err("epoll_ctl error(%s)", strerror(errno));
						exit(EXIT_FAILURE);
					}
				}
			}
			else
			{
				int sockfd = events[i].data.fd;
				while(1)
				{
					Pkt_header* pkthdr = new Pkt_header;
					int bytes_received = recv(sockfd, pkthdr, sizeof(Pkt_header), 0);
					if(bytes_received <= 0)
					{
						if (errno != EAGAIN && errno != EWOULDBLOCK)
						{
							lg.err("recv error(%s)", strerror(errno));
							if (epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]) < 0)
							{
								lg.err("epoll_ctl error(%s)", strerror(errno));
								exit(EXIT_FAILURE);
							}
							close(sockfd);

							hash_map<int, int>::iterator it = port_to_sockfd.begin();
							while(it != port_to_sockfd.end())
							{
								if(it->second == sockfd)
								{
									port_to_sockfd.erase(it);
									break;
								}
							}
						}
						break;
					}
					else
					{
						if(bytes_received < sizeof(Pkt_header))
						{
							lg.err("packet header error");
						}

						struct sockaddr_in cliaddr;
						socklen_t cliaddrlen = sizeof(cliaddr);
						if(getpeername(sockfd, (struct sockaddr*) &cliaddr, &cliaddrlen) < 0)
						{
							lg.err("getpeername error(%s)", strerror(errno));
							exit(EXIT_FAILURE);
						}
						int port = ntohs(cliaddr.sin_port);
						string pkt_src_ip(inet_ntoa(pkthdr->src_ip));
						ip_to_port[pkt_src_ip] = port;

						if(pkthdr->type == HELLO)
						{
							lg.dbg("send hello to %s", inet_ntoa(pkthdr->src_ip));
							if(send(sockfd, pkthdr, sizeof(Pkt_header), 0) < 0)
							{
								lg.err("send hello error(%s)", strerror(errno));
							}
							continue;
						}
						
						string d_ip(inet_ntoa(pkthdr->dst_ip));
						string s_ip(inet_ntoa(pkthdr->src_ip));
						lg.dbg("from %s to %s %d bytes", s_ip.c_str(), d_ip.c_str(), pkthdr->datalen);

						int bytes_tosend = sizeof(Pkt_header) + pkthdr->datalen;
						char* tosend = (char*)malloc(bytes_tosend);

						memcpy(tosend, pkthdr, sizeof(Pkt_header));

						if(recv(sockfd, tosend+sizeof(Pkt_header), pkthdr->datalen, 0) < pkthdr->datalen)
						{
							lg.err("recv error(%s)", strerror(errno));
							free(tosend);
							continue;
						}

						int tosockfd = port_to_sockfd[ip_to_port[ip_map[pkt_src_ip]]];
						if(tosockfd <= 0)
						{
							lg.err("connection from %s is error", ip_map[pkt_src_ip].c_str());
							free(tosend);
							continue;
						}

						if(send(tosockfd, tosend, bytes_tosend, MSG_NOSIGNAL) < 0)
							lg.err("send to %s error(%s)", ip_map[pkt_src_ip].c_str(), strerror(errno));
						else
							lg.dbg("send to %s successed", ip_map[pkt_src_ip].c_str());

						free(tosend);
					}
				}
			}
		}
	}
}

} // namespace rusv
