#include "common.hpp"

//========================================================================
// Registers signal handlers
void register_signal_handlers()
{
	std::signal(SIGTERM, signal_handler);
	std::signal(SIGSEGV, signal_handler);
	std::signal(SIGINT, signal_handler);
	std::signal(SIGILL, signal_handler);
	std::signal(SIGABRT, signal_handler);
	std::signal(SIGFPE, signal_handler);
}

//========================================================================
// Returns an ID corresponding to the last 8 bits of the IP address
int get_ID_from_ip_address(char *ipAddress)
{
	uint8_t octet[4];
	if (!inet_pton(AF_INET, ipAddress, &octet))
	{
		std::perror("Failed to get Axo ID");
		return -1;
	}

	return (int) octet[3];
}

//========================================================================
// Returns milliseconds passed since epoch
uint64_t get_timestamp_in_milliseconds()
{
	struct timeval ts;
	uint64_t timestamp;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t) ts.tv_sec * 1000 + ts.tv_usec / 1000;
	
	return timestamp;
}

//========================================================================
// Opens socket for sending
int open_sender_socket()
{
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		std::perror("sender socket error");

    return sockfd;
}

//========================================================================
// Returns a socket identifier bound to portNumber
int open_receiver_socket(uint16_t portNumber)
{
	struct sockaddr_in myAddress;
    socklen_t slen1 = sizeof(myAddress);
    
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)	
		std::perror("receiver socket error");

    bzero(&myAddress, sizeof(myAddress));
    myAddress.sin_family = AF_INET;
    myAddress.sin_port = htons(portNumber);
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
     
    if (bind(sockfd, (struct sockaddr* ) &myAddress, sizeof(myAddress)) == -1)
    {
    	std::perror("bind error");
    	return -1;
    }

    int one = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one , sizeof(int)) == -1)
    {
    	std::perror("Address reuse error");
    	return -1;
    }

    return sockfd;
}

//========================================================================
//
int fill_sender_socket(sockaddr_in &serverAddress, const char *IPAddress, uint16_t portNumber)
{
    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    
    //Fill port number
    serverAddress.sin_port = htons(portNumber);
    
    //Fill ip address
    if (inet_aton(IPAddress, &serverAddress.sin_addr)==0)
    {
        std::perror("server address error");
        return -1;
    }
    else
    	return 0;
}

//========================================================================
// Returns a multicast socket for listening
int open_multicast_receiver_socket(char* multicastIPAddress, char* localIPAddress, uint16_t listeningPort)
{
	int sockfd;

 	if ((sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		perror("socket() failed");
	}

	int flag_on = 1;

	// set reuse port to on to allow multiple binds per host
	if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) < 0)
	{
		perror("setsockopt() failed");
		return -1;
	}

	struct sockaddr_in multicast_addr;

 	// construct a multicast address structure
	memset(&multicast_addr, 0, sizeof(multicast_addr));
	multicast_addr.sin_family      = AF_INET;
	multicast_addr.sin_addr.s_addr = inet_addr(multicastIPAddress);
	multicast_addr.sin_port        = htons(listeningPort); //listen on the specified port

	//bind to the any address
	if ((bind(sockfd, (struct sockaddr *) &multicast_addr, sizeof(multicast_addr))) < 0)
	{
		perror("bind() failed");
		return -1;
	}

	
	//Construct an IGMP join request structure
	struct ip_mreq mc_req;
	mc_req.imr_multiaddr.s_addr = inet_addr(multicastIPAddress);
	mc_req.imr_interface.s_addr = inet_addr(localIPAddress);

	//send an ADD MEMBERSHIP message via setsockopt
	if ((setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*) &mc_req, sizeof(mc_req))) < 0)
	{
		perror("setsockopt() failed");
		return -1;
	}

	return sockfd;
}

//========================================================================
//
int receive_multicast_data(int sockfd, char * buffer)
{
    bzero(buffer, BUFF_SIZE);

    int size = recv(sockfd, buffer, BUFF_SIZE - 1, 0);

    if (size == -1)
    	std::perror("receive error");

    return size;
}

//========================================================================
//
void drop_multicast_memebership(int sockfd, char* multicastIPAddress, char* localIPAddress)
{
	struct ip_mreq mc_req;

	//Construct an IGMP join request structure
	mc_req.imr_multiaddr.s_addr = inet_addr(multicastIPAddress);
	mc_req.imr_interface.s_addr = inet_addr(localIPAddress);


	//send a DROP MEMBERSHIP message via setsockopt
	if ((setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*) &mc_req, sizeof(mc_req))) < 0)
		perror("setsockopt() failed");
}

//========================================================================
// Returns a multicast socket for sending packets through the provided outgoing ip address
int open_multicast_sender_socket(char* outgoing_ip)
{
	int sockfd;

	// create a socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		std::perror("Socket creation failed");
	}

	// set the TTL (time to live/hop count) for the send
	int multicast_ttl = 255;
	if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (void*) &multicast_ttl, sizeof(multicast_ttl)) < 0)
	{
		std::perror("setting ttl failed");
		return -1;
	}

	// Avoid receiving messages sent my this socket on the multicast group
	int zero = 0;
  	if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, (void *) &zero, sizeof(unsigned char)) < 0)
  	{
  		std::perror("setting loopOff failed");
  		return -1;
  	}

	// set outgoing interface
	struct in_addr iaddr;
	iaddr.s_addr = inet_addr(outgoing_ip);
	if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &iaddr, sizeof(struct in_addr)) < 0)
	{
		std::perror("setting outgoing ip failed");
		return -1;
	}

	return sockfd;
}

//========================================================================
// Takes multicast ip address in human readable form, destination port; fills the sockaddr_in struct
void set_multicast_info(const char *multicast_ip, uint16_t multicast_port, sockaddr_in &multicast_addr)
{
	// set multicast address parameters
	memset(&multicast_addr, 0, sizeof(multicast_addr));
	multicast_addr.sin_family      = AF_INET;
	multicast_addr.sin_addr.s_addr = inet_addr(multicast_ip);
	multicast_addr.sin_port        = htons(multicast_port);
}

//========================================================================
//
void send_multicast(char *buffer, uint32_t size, int sockfd, sockaddr_in destination)
{
    if (sendto(sockfd, buffer, size, 0, (struct sockaddr*) &destination, sizeof(destination)) == -1)
    	std::perror("send error");
}