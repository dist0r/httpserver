#include <netinet/in.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <ev.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <ev++.h>
#include <thread>
#include <iostream>
#include <fcntl.h>

char *host = 0, *port = 0, *dir = 0;

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);


class Worker
{


    private:
	ev::dynamic_loop  _loop;
	std::thread _thread;
	int _client_sd;
    public:
	bool isWorking;


Worker() : _loop(ev::AUTO)
{
}

~Worker()
{
	close(_client_sd);
}

struct my_watcher : ev_io
{
    Worker* threadWorker;
};


void run( int client_sd)
{
    _client_sd = client_sd;
    struct my_watcher* w_client = (struct my_watcher*) malloc(sizeof (struct my_watcher));
    w_client->threadWorker = this;
    ev_io_init(w_client, read_cb, _client_sd, EV_READ);
    ev_io_start(_loop, w_client);
    isWorking = true;
    _thread = std::thread(&Worker::loop, this);
    _thread.detach();
    
}


void loop()
{
    while(isWorking) { ev_loop( _loop, 0);}
    delete this;
}

};



void accept_cb( struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    int client_sd = accept(watcher->fd, 0, 0);
    Worker* w = new Worker();
    w->run(client_sd);    
}


void extract_path_from_http_get_request(std::string& path, const char* buf, ssize_t len)
{
    std::string request(buf, len);
    std::string s1(" ");
    std::string s2("?");

    // "GET "
    std::size_t pos1 = 4;

    std::size_t pos2 = request.find(s2, 4);
    if (pos2 == std::string::npos)
    {
        pos2 = request.find(s1, 4);
    }

    path = request.substr(4, pos2 - 4);
}

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    struct Worker::my_watcher*  mw = (struct Worker::my_watcher*) watcher;


    char buffer[1024];
    char reply[1024];
    ssize_t r = recv(watcher->fd, buffer, 1024, MSG_NOSIGNAL);
    if (r < 0) {
	return;
    } else if (r == 0) {
	ev_io_stop(loop, watcher);
	free(watcher);
	mw->threadWorker->isWorking = false;
	return;
    } else {

    std::string path;
    extract_path_from_http_get_request(path, buffer, r);

    std::string full_path = std::string(dir) + path;

    std::cout << access(full_path.c_str(), F_OK);
    if (access(full_path.c_str(), F_OK) != -1)
    {
        int fd = open(full_path.c_str(), O_RDONLY);
        int sz = lseek(fd, 0, SEEK_END);;

        sprintf(reply, "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/html\r\n"
                       "Content-length: %d\r\n"
                       "Connection: close\r\n"
                       "\r\n", sz);

        ssize_t send_ret = send(watcher->fd, reply, strlen(reply), MSG_NOSIGNAL);


        off_t offset = 0;
        while (offset < sz)
        {
            offset = sendfile(watcher->fd, fd, &offset, sz - offset);
        }

        close(fd);
    }
    else
    {
        strcpy(reply, "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-length: 107\r\n"
                      "Connection: close\r\n"
                      "\r\n");

        ssize_t send_ret = send(watcher->fd, reply, strlen(reply), MSG_NOSIGNAL);
        strcpy(reply, "<html>\n<head>\n<title>Not Found</title>\n</head>\r\n");
        send_ret = send(watcher->fd, reply, strlen(reply), MSG_NOSIGNAL);
        strcpy(reply, "<body>\n<p>404 Request file not found.</p>\n</body>\n</html>\r\n");
        send_ret = send(watcher->fd, reply, strlen(reply), MSG_NOSIGNAL);
    }    

	ev_io_stop(loop, watcher);
	free(watcher);
	mw->threadWorker->isWorking = false;
    
}
}


int main (int argc, char** argv)
{

    if (daemon(0, 0) == -1)
    {
        std::cout << "daemon error" << std::endl;
        exit(1);
    }

    int res=0;

    while ( (res = getopt(argc,argv,"h:p:d:")) != -1){
	switch (res){
	case 'h': host = optarg;break;
	case 'p': port = optarg; break;
	case 'd': dir = optarg; break;
        };
    };

    struct ev_loop *loop = ev_default_loop(0);

    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, host, &(addr.sin_addr.s_addr));
    bind(sd, (struct sockaddr*)&addr, sizeof(addr));

    listen(sd, SOMAXCONN);

    struct ev_io w_accept;
    ev_io_init(&w_accept, accept_cb, sd, EV_READ);
    ev_io_start(loop, &w_accept);

    while (1) {ev_loop(loop, 0);}
    
    return 0;
}