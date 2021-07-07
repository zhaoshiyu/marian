#include "marian.h"
#include "translator/beam_search.h"
#include "translator/translator.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <assert.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>  
#include <sys/epoll.h>  
#include <fcntl.h>  
#include <unistd.h>  
#include <errno.h>  
#define MAXLEN 100000
#define MAXLINE 15000  
#define OPEN_MAX 100  
#define LISTENQ 20  
#define SERV_PORT 5000  
#define INFTIM 1000 

using namespace std;

void setnonblocking(int sock)  
{  
    int opts;  
    opts=fcntl(sock,F_GETFL);  
    if(opts<0)  
    {  
        perror("fcntl(sock,GETFL)");  
        exit(1);  
    }  
    opts = opts|O_NONBLOCK;  
    if(fcntl(sock,F_SETFL,opts)<0)  
    {  
        perror("fcntl(sock,SETFL,opts)");  
        exit(1);  
    }     
} 

string postProcess(string& trans)
{
	Trim(trans);
	if (trans == "")
	{
		return trans;
	}
	else if (trans == "\"")
	{
		trans = "\\" + trans;
		return trans;
	}
	auto parts = Split(trans, "\"");
	string res = parts[0];
	for (size_t i = 1; i < parts.size(); ++i) {
		res += "\\\"" + parts[i];
	}
	return res;
}

int main(int argc, char **argv) {
	using namespace marian;

	int pos = argc;
	char ** argv2 = new char*[argc];
	argv2[0] = argv[0];
	int j = 1;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-j2c") == 0)
		{
			pos = i;
			argv[i] = NULL;
			break;
		}
		else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "-model") == 0 || strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-config") == 0)
		{
			i++;
		}
		else
		{
			argv2[j++] = argv[i];
		}
	}
	
	auto options1 = New<Config>(pos, argv, ConfigMode::translating, true);
	marian::Ptr<marian::Config> options2;
	auto task1 = New<TranslateServiceMultiGPU<BeamSearch>>(options1);
	marian::Ptr<marian::TranslateServiceMultiGPU<marian::BeamSearch>> task2;

	if (pos < argc && pos > 0)
	{
		int cur = pos+1;
		while (j < argc)
		{
			if (cur < argc)
			{
				argv2[j++] = argv[cur++];
			}
			else
			{
				argv2[j++] = 0;
			}
		}
		
		options2 = New<Config>(argc - 5, argv2, ConfigMode::translating, true);
		task2 = New<TranslateServiceMultiGPU<BeamSearch>>(options2);
		delete[] argv2;
	}

	int serv_port = options1->get<size_t>("port");
	
    int i, maxi, listenfd, connfd, sockfd,epfd,nfds;  
    ssize_t n;  
    char line[MAXLINE];  
    socklen_t clilen;  
 
    struct epoll_event ev,events[20];  

    epfd=epoll_create(256);  
    if (epfd == -1) {
	cerr <<"epoll_create error" << endl;
    }
    else {
	cerr << "socket successfully, epfd =" << epfd << endl;
    }
    struct sockaddr_in clientaddr;  
    struct sockaddr_in serveraddr;  
    listenfd = socket(AF_INET, SOCK_STREAM, 0);  
    if (listenfd == -1) {
	cerr <<"socket error" << endl;
    }
    else {
	cerr << "socket successfully, listenfd =" << listenfd << endl;
    }

    setnonblocking(listenfd);  
    bzero(&serveraddr, sizeof(serveraddr));  
    serveraddr.sin_family = AF_INET;  
    const char *local_addr="10.84.62.67";  
    inet_aton(local_addr,&(serveraddr.sin_addr));//htons(SERV_PORT);  
    serveraddr.sin_port=htons(serv_port);  
    bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));  
    listen(listenfd, LISTENQ);  
    ev.data.fd=listenfd;
    ev.events=EPOLLIN;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        cout << "epoll_ctl error" << endl;
        exit(EXIT_FAILURE);
    }
    else {
	cout << "epoll_ctl successfully" << endl;
    }

    cerr << "Current Socket Port: " << serv_port << endl;
    maxi = 0;  
    int nread = 0;
    string reshead="HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: ";
    for ( ; ; ) {  
        nfds=epoll_wait(epfd,events,200,0);  
        for(i=0;i<nfds;++i)  
        { 
	    cout << "events[i].data.fd="<<events[i].data.fd<<endl; 
            if(events[i].data.fd==listenfd)  
            {  
                    connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);
                    
                    if(connfd<0){  
                        perror("connfd<0");  
                        exit(1);  
                    }  
                    
                    setnonblocking(connfd);  
                    char *str = inet_ntoa(clientaddr.sin_addr);  
                    cout << "accept a connection from " << str << endl;  
                    ev.data.fd=connfd;  
                    ev.events=EPOLLIN|EPOLLET;  
                    if(epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&ev)==-1) {
                        perror("epoll_ctl:add");
                        exit(EXIT_FAILURE);
                    }
                    if (connfd == -1) {
                        if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                            perror("accept");
                        }
                    }
            }  
            else if(events[i].events&EPOLLIN)  
            {  
                cout << "EPOLLIN" << endl;
		boost::timer::cpu_timer timerin;

                if ( (sockfd = events[i].data.fd) < 0) {
                    continue;
                }

                n = 0;
                while ((nread = read(sockfd, line + n, BUFSIZ-1)) > 0) {
                    n += nread;
                }
                if (nread == -1 && errno != EAGAIN) {
                    perror("read error"); 
                }

                line[n] = '\0';  
	        cout << "n=" << n << endl;
                cout << "read\n" << line << endl;  
				
		string message_str(line);
		cout << message_str << endl;
		if (message_str == "")
		{
			break;
		}
		if (message_str.compare("GET /checkalive HTTP/1.1\r\nContent-Length: 0\r\n\r\n") == 0)
		{
		
			memset(line, '\0', sizeof(char) * n);   
			strcpy(line, "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n");
		}
		else 
        {
			if (message_str.find("\r\n\r\n") != string::npos)
			{
				message_str = message_str.substr(message_str.find("\r\n\r\n") + 4);
			}

			rapidjson::Document d;
			d.Parse<0>(message_str.c_str());
			rapidjson::Value& sentences = d["sentences"];
			rapidjson::Value& from_lang = d["from_lang"];
			rapidjson::Value& to_lang = d["to_lang"];

			string sents_str = "";
			if (sentences.IsArray())
			{
				for (size_t i = 0; i < sentences.Size(); i++)
				{
					rapidjson::Value& v = sentences[i];
					if (v.IsString())
					{
						sents_str += v.GetString();
						sents_str += "\n";
					}
				}
			}
			auto message_short = message_str;
			boost::algorithm::trim_right(message_short);
			//LOG(error, "Message received: {}", message_short);

			//auto send_stream = std::make_shared<WsServer::SendStream>();
			string send_stream = "{\"success\":true, \"sentences\":[";
			boost::timer::cpu_timer timer;
			size_t k = 0;

			marian::Ptr<marian::TranslateServiceMultiGPU<marian::BeamSearch>> task;
			string flang = from_lang.GetString();
			string tlang = to_lang.GetString();
			if (flang.compare("zh-CHS") == 0 && tlang.compare("jp") == 0)
			{
				task = task1;
			}
			else
			{
				task = task2;
			}

			for (auto &transl : task->run({ sents_str })) {
				//LOG(info, "Best translation: {}", transl);
				if (k == sentences.Size() - 1)
				{
					send_stream += "\"" + postProcess(transl) + "\"]}";
				}
				else
				{
					send_stream += "\"" + postProcess(transl) + "\",";
				}
				k++;
			}

			int len = send_stream.length();
			send_stream = reshead + std::to_string(len) + "\r\n\r\n"+send_stream;
			strcpy(line,send_stream.c_str());
		}
				


		boost::timer::cpu_timer timers;
                sockfd = events[i].data.fd;
		cout << "write feedback = " << line << endl;
                int nwrite, data_size = strlen(line);
                n = data_size;
                while (n > 0) {
                    nwrite = write(sockfd, line + data_size - n, n);
                    if (nwrite < n) {
                        if (nwrite == -1 && errno != EAGAIN) {
                             perror("write error");
                        }
                        break;
                    }
                    n -= nwrite;
                }

            }  
            else if(events[i].events&EPOLLOUT)  
            {     
		boost::timer::cpu_timer timer;
                sockfd = events[i].data.fd;
		cout << "write feedback epollout = " << line << endl;
                int nwrite, data_size = strlen(line);
                n = data_size;
                while (n > 0) {
                    nwrite = write(sockfd, line + data_size - n, n);
                    if (nwrite < n) {
                        if (nwrite == -1 && errno != EAGAIN) {
                             perror("write error");
                        }
                        break;
                    }
                    n -= nwrite;
                }
                ev.events=EPOLLIN|EPOLLET;  
                epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);  

            }  
        }  
    }  
	close(listenfd);
    return 0;  
}  

