#include "axoApi.hpp"

//========================================================================
// Default constructor
axoApi::axoApi()
{
	axoApi("127.0.0.0");
}

//========================================================================
// Constructor
axoApi::axoApi(std::string ipAddress)
{
	// Get axoID from IP address
	axoID = get_ID(ipAddress);
	if(axoID < 0)
	{
		std::perror("Controller exiting: cannot get axoID from IP address");
		exit(EXIT_FAILURE);
	}

	// Open socket for axo
	axoSockfd = open_socket();
	if(axoSockfd == -1)
	{	
		std::perror("Controller exiting: cannot open Tagger socket");
		exit(EXIT_FAILURE);
	}

	// Initialize variables
	lastSyncTime = 0;
}

//========================================================================
// Destructor
axoApi::~axoApi()
{
	close(axoSockfd);
}

//========================================================================
// Returns an ID corresponding to the last 8 bits of the IP address
int axoApi::get_ID(std::string ipAddress)
{
	uint8_t octet[4];
	if (!inet_pton(AF_INET, (char *)ipAddress.c_str(), &octet))
	{
		std::perror("Failed to get Axo ID");
		return -1;
	}

	return (int) octet[3];
}

//========================================================================
// Opens socket for sending timestamp
int axoApi::open_socket()
{
	int fd;

    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		std::perror("sender socket error");

	bzero(&axoAddress, sizeof(axoAddress));
    axoAddress.sin_family = AF_INET;
    
    //Fill port number
    axoAddress.sin_port = htons(TS_PORT);
    
    //Fill ip address
    if (inet_aton(TS_IP_ADDRESS, &axoAddress.sin_addr)==0)
    {
        std::perror("server address error");
        return -1;
    }
    else
    	return fd;
}

//========================================================================
// Returns milliseconds passed since epoch when PTP synchonization is used
void axoApi::get_timestamp_ptp()
{
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t) ts.tv_sec * 1000 + ts.tv_usec / 1000;

	if(timestamp > lastSyncTime + T_OFFSET) 
	{	
		get_offset_ptp();
		lastSyncTime = timestamp;
	}

	timestamp = (uint64_t) ((int64_t) timestamp - (int64_t) (offset * 1000));
}

//========================================================================
// Returns milliseconds passed since epoch when GPS synchonization is used
void axoApi::get_timestamp_gps()
{
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t) ts.tv_sec * 1000 + ts.tv_usec / 1000;
}

//========================================================================
// Get PTP offset
void axoApi::get_offset_ptp()
{
	std::string ptpStats = (std::string) "tail -n1 " + PTP_OFFSET_FILE + " | cut -d ',' -f5";
	std::stringstream sss;
	sss << exec(ptpStats.c_str());
	if(sss.str() != "") sss >> offset;
}

//========================================================================
// Sends timestamp
void axoApi::send_timestamp()
{	
	char buffer[BUFF_SIZE];
	bzero(buffer, sizeof(buffer));

	memcpy(buffer, &timestamp, sizeof(timestamp));

	if (sendto(axoSockfd, buffer, sizeof(timestamp), 0, (struct sockaddr*) &axoAddress, sizeof(axoAddress))==-1)
        std::perror("timestamp sending error");
}

//========================================================================
// Execute bash command
std::string axoApi::exec(const char* cmd) {
    
    char buffer[128];
    std::string result = "";
    
    FILE* pipe = popen(cmd, "r");
    if (!pipe)
    {
    	std::perror("popen() failed!");
    	return "";
    }

    try {
        
        while (!feof(pipe))
            if (fgets(buffer, 128, pipe) != NULL)
                result += buffer;

    } catch (...) {
        pclose(pipe);
        std::perror("fgets() failed!");
        return "";
    }

    pclose(pipe);
    return result;
}