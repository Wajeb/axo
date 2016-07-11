#include "../common/common.hpp"
#include "tagger.hpp"

uint8_t replicaID; // unique replica ID; this is passed as an argument on launch; should correspond to last 8 bits of wlan0 interface ip address
int sockfdDetector; // socket identifier for listening to detector
bool synchronized; // flag specifying whether ptp synchronization is present

uint32_t tsIPAddress;

//========================================================================
//main: creates threads, calls work().
int main(int argc, char **argv)
{
	if(argc < 3)
	{
		std::cout << "Usage: ./tagger <ipaddress> <queue number>" << std::endl;
		return 0;
	}

	// Register signal handlers
	register_signal_handlers();

	int stat;

	if (!inet_pton(AF_INET, TS_IP_ADDRESS, &tsIPAddress))
	{
    	std::cout << "Tagger exiting: inet_pton failed: " << std::endl;
    	exit(EXIT_FAILURE);
	}

	// Initialize replicaID from IP address
	char *ipAddress = argv[1];
	stat = get_ID_from_ip_address(ipAddress);
	if(stat == -1)
	{
		std::perror("Tagger exiting: cannot get axoID");
		exit(EXIT_FAILURE);
	}
	
	replicaID = (uint8_t) stat;

	uint8_t qNum = std::stoi(argv[2]);
	
	// Socket to receive timestamp and health from detector
	sockfdDetector = open_receiver_socket(TAGGER_DETECTOR_PORT);
	if(sockfdDetector == -1)
	{
		std::perror("Tagger exiting: cannot open detector receiver socket");
		exit(EXIT_FAILURE);
	}

	// Create thread for receiving timestamp and health from detector
	std::thread (receive_from_detector, sockfdDetector).detach();

	// Create thread that updates the hooks in mangle ip tables in kernel
	std::thread (monitor_rules, qNum).detach();

	// Create thread that checks for ptp synchronization every second
	synchronized = false;
	std::thread (check_synch).detach();
		
	// Start main work
	work(qNum);

	return 0;
}