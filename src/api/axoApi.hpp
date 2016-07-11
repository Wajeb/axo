#ifndef AXO_API_HPP
#define AXO_API_HPP

#include <fstream>
#include <cstdlib>
#include <sys/time.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

#define PTP_OFFSET_FILE "../../tmp/ptpStats.log"
#define TS_IP_ADDRESS "127.0.0.1"
#define TS_PORT 1002
#define BUFF_SIZE 1024
#define T_OFFSET 1000

class axoApi
{
	public:
		
		// Default constructur with ID 0
		axoApi();

		// Constructor which takes IP address as parameter
		axoApi(std::string ipAddress);

		// Destructor
		~axoApi();

		// Returns milliseconds passed since epoch when PTP sychronization is used
		void get_timestamp_ptp();

		// Returns milliseconds passed since epoch when GPS synchronization is used
		void get_timestamp_gps();

		// Sends timestamp to tagger
		void send_timestamp();

	private:
		
		uint8_t axoID;
		uint64_t timestamp;
		double offset;

		int axoSockfd;
		sockaddr_in axoAddress;
		uint64_t lastSyncTime;

		// Returns an ID corresponding to the last 8 bits of the IP address
		int get_ID(std::string ipAddress);

		// Creates a new UDP sender socket and returns socket identifier
		int open_socket();

		// Gets PTP offset for correction
		void get_offset_ptp();

		// Execute bash command
		std::string exec(const char* cmd);
};

#endif
