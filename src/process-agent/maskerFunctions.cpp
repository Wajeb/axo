#include "../common/common.hpp"
#include "masker.hpp"

extern int rcvSockfd;
extern int mcastSendSockfd;
extern int sockfdRaw;
extern uint8_t *packet;

//========================================================================
//
int receive_setpoint(int sockfd, char * buffer, sockaddr_in &fromAddress)
{
    bzero(buffer, BUFF_SIZE);
    uint32_t fromLen = sizeof(fromAddress);
    memset(&fromAddress, 0, fromLen);

    int size = recvfrom(sockfd, buffer, BUFF_SIZE - 1, 0, (struct sockaddr*) &fromAddress , &fromLen);

    if (size == -1)
    	std::perror("receive error");

    return size;
}

//========================================================================
//
bool isValid(uint64_t controllerTimestamp, uint64_t tau)
{
	uint64_t currentTime = get_timestamp_in_milliseconds();
	return currentTime <= controllerTimestamp + tau;
}

//========================================================================
//
void make_report(uint8_t replicaID, uint64_t controllerTimestamp, uint64_t detectorTimestamp, int8_t health, bool valid, char* report)
{
	report_t reportForDetector;
	reportForDetector.replicaID = replicaID;
	reportForDetector.controllerTimestamp = controllerTimestamp;
	reportForDetector.detectorTimestamp = detectorTimestamp;
	reportForDetector.health = health;
	reportForDetector.valid = valid;
	memcpy(report, &reportForDetector, sizeof(reportForDetector));
}

//========================================================================
//
void prepare_fixed_ip_header(ip &iphdr)
{
	// IPv4 header length (4 bits): Number of 32-bit words in header = 5
	iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);

	// Internet Protocol version (4 bits): IPv4
	iphdr.ip_v = 4;

	// Type of service (8 bits)
	iphdr.ip_tos = 0;

	// ID sequence number (16 bits): unused, since single datagram
	iphdr.ip_id = htons (0);

	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

	int *ip_flags = allocate_intmem (4);
	if(ip_flags == NULL)
	{	
		std::perror("Masker exiting: cannot allocate ip_flags memory");
		exit(EXIT_FAILURE);	
	}

	// Zero (1 bit)
	ip_flags[0] = 0;

	// Do not fragment flag (1 bit)
	ip_flags[1] = 0;

	// More fragments following flag (1 bit)
	ip_flags[2] = 0;

	// Fragmentation offset (13 bits)
	ip_flags[3] = 0;

	iphdr.ip_off = htons ((ip_flags[0] << 15)
	                  + (ip_flags[1] << 14)
	                  + (ip_flags[2] << 13)
	                  +  ip_flags[3]);

	// Time-to-Live (8 bits): default to maximum value
	iphdr.ip_ttl = 255;

	// Transport layer protocol (8 bits): 17 for UDP
	iphdr.ip_p = IPPROTO_UDP;	

	free(ip_flags);
}

//========================================================================
//
int open_raw_socket()
{
	int on = 1;
	int sd;

	// Submit request for a raw socket descriptor.
	if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
	{
		perror ("socket() failed ");
	}

	// Set flag so socket expects us to provide IPv4 header.
	if (setsockopt (sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof (on)) < 0)
	{
		perror ("setsockopt() failed to set IP_HDRINCL ");
		return -1;
	}

	return sd;
}

//========================================================================
//
int prepare_raw_packet(char  *src_ip, uint16_t srcPort, char *dst_ip, uint16_t dstPort, char * data, int datalen, ip &iphdr, uint8_t *packet)
{
	// Total length of datagram (16 bits): IP header + UDP header + datalen
	iphdr.ip_len = htons (IP4_HDRLEN + UDP_HDRLEN + datalen);

	int status;

	// Source IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1)
	{
		fprintf (stderr, "first inet_pton() failed.\nError message: %s", strerror (status));
		return -1;
	}
	
	//printf("Outside dst_ip: %s\n",dst_ip);
	// Destination IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1)
	{
		fprintf (stderr, "second inet_pton() failed.\nError message: %s", strerror (status));
		//std::cout << "status: " << status << std::endl;
		//printf("dst_ip: %s\n",dst_ip);
		return -1;
	}

	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
	iphdr.ip_sum = 0;
	iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);

	// UDP header
	struct udphdr udphdr;

	// Source port number (16 bits): pick a number
	udphdr.source = htons (srcPort);

	// Destination port number (16 bits): pick a number
	udphdr.dest = htons (dstPort);

	// Length of UDP datagram (16 bits): UDP header + UDP data
	udphdr.len = htons (UDP_HDRLEN + datalen);

	// UDP checksum (16 bits)
	udphdr.check = udp4_checksum (iphdr, udphdr, data, datalen);

	//Prepare packet.

	// First part is an IPv4 header.
	memcpy (packet, &iphdr, IP4_HDRLEN * sizeof (uint8_t));

	// Next part of packet is upper layer protocol header.
	memcpy ((packet + IP4_HDRLEN), &udphdr, UDP_HDRLEN * sizeof (uint8_t));

	// Finally, add the UDP data.
	memcpy (packet + IP4_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));

	return 0;
}

//========================================================================
//
void prepare_pa_destination(sockaddr_in &sin, ip &iphdr)
{
	memset (&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;
}

//========================================================================
//
void signal_handler(int sig)
{
	// Close sockets
	close(rcvSockfd);
	close(mcastSendSockfd);
	close(sockfdRaw);

	// Free allocated memory.
	free(packet);

	exit(EXIT_SUCCESS);
}
