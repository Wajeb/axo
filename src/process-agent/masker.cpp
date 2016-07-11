#include "../common/common.hpp"
#include "masker.hpp"

int rcvSockfd;
int mcastSendSockfd;
int sockfdRaw;

uint8_t *packet;

int main(int argc, char **argv)
{
	if(argc < 4)
	{
		std::cout << "Usage: ./masker <Outgoing IP Address> <Validity horizon in ms> <Synchronization accuracy in ms>" << std::endl;
		return 0;
	}

	// Register signal handlers
	register_signal_handlers();

	// Reading input
	int tau_0 = std::stoi(argv[2]);
	int synchAcc = std::stoi(argv[3]);

	// Initialize adjusted validity horizon
	uint64_t tau = tau_0 - 2 * synchAcc - MASKER_BOUND;

	rcvSockfd = open_receiver_socket(MASKER_PORT);
	if(rcvSockfd == -1)
	{
		std::perror("Masker exiting: cannot open recver socket for tagger");
		exit(EXIT_FAILURE);
	}

	mcastSendSockfd = open_multicast_sender_socket(argv[1]);
	if(mcastSendSockfd == -1)
	{
		std::perror("Masker exiting: cannot open multicast sender socket for detector");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in multicast_addr;
	set_multicast_info(MULTICAST_GROUP, DETECTOR_PORT, multicast_addr);

	struct sockaddr_in controllerAddress;
	struct sockaddr_in paAddress;
	
	char rcvBuffer[BUFF_SIZE];
	char sendBuffer[BUFF_SIZE];
		
	uint32_t rcvSize;
	bool valid;

	axoheader_t rcvHeader;

	//Set up a raw socket
	struct ip iphdr;
	prepare_fixed_ip_header(iphdr);

	sockfdRaw = open_raw_socket();
	if(sockfdRaw == -1)
	{
		std::perror("Masker exiting: cannot open raw socket");
		exit(EXIT_FAILURE);
	}

	// Allocate packet
	packet = allocate_ustrmem (IP_MAXPACKET);
	if(packet == NULL)
	{
		std::perror("Masker exiting: cannot allocate packet memory");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in paDestination;

	uint64_t lastTimestamp = 0;

	int stat;
	
	// Repeat main loop forever
	while(true)
	{
		// Receive setpoints
		rcvSize = receive_setpoint(rcvSockfd, rcvBuffer, controllerAddress);	
		if(rcvSize == -1) continue;
		
		// Extract Axo Header fields from received message
		memcpy(&rcvHeader, rcvBuffer, AXO_HEADER_BYTES);
		
		bzero(packet, IP_MAXPACKET);

		stat = prepare_raw_packet(inet_ntoa(controllerAddress.sin_addr), ntohs(controllerAddress.sin_port), argv[1],
							ntohs(rcvHeader.paPort), rcvBuffer + AXO_HEADER_BYTES, rcvSize - AXO_HEADER_BYTES, iphdr, packet);
		if(stat == -1) continue;

		prepare_pa_destination(paDestination, iphdr);
		
		// Check if setpoint is valid
		valid = isValid(rcvHeader.controllerTimestamp, tau);
	
		if(valid)
		{
			if (sendto (sockfdRaw, packet, IP4_HDRLEN + UDP_HDRLEN + rcvSize - AXO_HEADER_BYTES, 0, (struct sockaddr *) &paDestination, sizeof (struct sockaddr)) < 0) 
			{
				perror ("sendto() failed ");
			}
		}

		// Make detection report
		make_report(rcvHeader.replicaID, rcvHeader.controllerTimestamp, rcvHeader.detectorTimestamp, rcvHeader.health, valid, sendBuffer);

		// Send report to detector
		send_multicast(sendBuffer, sizeof(report_t), mcastSendSockfd, multicast_addr);
	}

	return 0;
}
