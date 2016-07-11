#include "../common/common.hpp"
#include "tagger.hpp"
#include "../common/sendRaw.hpp"

uint64_t controllerTimestamp; // timestamp sent by controller to tagger; corresponds to next received setpoints
uint64_t detectorTimestamp = 0; // timestamp sent by detector to tagger; corresponds to max received timestamp by detector
int8_t detectorHealth = H_MAX; // health sent by detector to tagger; corresponds to health of this replica (on which tagger resides)
extern uint8_t replicaID; // unique replica ID
extern bool synchronized; // flag specifying whether ptp synchronization is present

std::mutex detectorTimestampMutex; // mutex for locking the access to detectorTimestamp
std::mutex detectorHealthMutex; // mutex for locking the access to detectorHealth
std::mutex synchronizationMutex; // mutex for locking the access to synchronized

extern int sockfdDetector; // socket identifier for listening to detector
extern uint32_t tsIPAddress;

//========================================================================
// NF_QUEUE callBack
static int Callback(nfq_q_handle *myQueue, struct nfgenmsg *msg, nfq_data *pkt, void *cbData)
{
	uint32_t id = 0;
	nfqnl_msg_packet_hdr *header;

	if ((header = nfq_get_msg_packet_hdr(pkt)))
	{
		id = ntohl(header->packet_id);

	}

	unsigned char *pktData;
	int len = nfq_get_payload(pkt, &pktData); //Requires unsigned char **

	bool isPktATimestamp = is_packet_a_timestamp(pktData);
	
	synchronizationMutex.lock();
	bool drop = !synchronized;
	synchronizationMutex.unlock();

	if(isPktATimestamp)
	{
		// Get timestamp
    	memcpy(&controllerTimestamp, pktData + PAYLOAD_OFFSET, sizeof(uint64_t));
		
		// Discard this packet
		return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
	}
	else if(drop)
	{
		return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
	}
	else 
	{
		axoheader_t sendHeader = populate_header(pktData);

		//Create tagged setpoint
		unsigned char taggedSP[BUFF_SIZE];
		
		// Copying IP and UDP header
		memcpy(taggedSP, pktData, sizeof(uint8_t)*PAYLOAD_OFFSET);
		
		// Change dest port to Masker
		*((uint16_t *) (taggedSP + DEST_PORT_OFFSET)) = htons(MASKER_PORT);

		
		// Change IP total length field
		*((uint16_t *) (taggedSP + IP_TOTAL_LENGTH_OFFSET)) = htons(ntohs(*((uint16_t *) (taggedSP + IP_TOTAL_LENGTH_OFFSET))) + AXO_HEADER_BYTES);

		// Change IP header checksum
		*((uint16_t *) (taggedSP + IP_CHECKSUM_OFFSET)) = (uint16_t) 0;
		*((uint16_t *) (taggedSP + IP_CHECKSUM_OFFSET)) = checksum((uint16_t *)taggedSP, IP4_HDRLEN);

		// Change UDP total length field
		*((uint16_t *) (taggedSP + UDP_TOTAL_LENGTH_OFFSET)) = htons(ntohs(*((uint16_t *) (taggedSP + UDP_TOTAL_LENGTH_OFFSET))) + AXO_HEADER_BYTES);
		
		// Copying Axo header
		memcpy(taggedSP+ PAYLOAD_OFFSET, (const void *) &sendHeader, AXO_HEADER_BYTES);

		// Copying original payload;
		memcpy(taggedSP+ PAYLOAD_OFFSET+ AXO_HEADER_BYTES, pktData + PAYLOAD_OFFSET, sizeof(uint8_t)*(len-PAYLOAD_OFFSET));

	  	//Set bytes 26,27 to 0; To disable checksum verification at the destination
		taggedSP[26] = 0;
		taggedSP[27] = 0;
		
		// Accept this packet with modifications
		return nfq_set_verdict(myQueue, id, NF_ACCEPT, len + AXO_HEADER_BYTES, taggedSP); //Requires unsigned char **			
	}	
}

//========================================================================
//
void receive_from_detector(int sockfd)
{
	char buffer[BUFF_SIZE];
	uint32_t size;

	uint64_t tempDetTS;

	while(true)
	{
	    bzero(buffer, BUFF_SIZE);
	    
	    size = recv(sockfd, buffer, BUFF_SIZE - 1, 0);

	    if (size == -1)
	    	std::perror("Tagger: receive from detector error");
	    else
	    {
	    	// Assign received timestamp to detector timestamp
	    	memcpy(&tempDetTS, buffer, sizeof(uint64_t));

	    	detectorTimestampMutex.lock();
	    	detectorTimestamp = std::max(detectorTimestamp, tempDetTS);
	    	detectorTimestampMutex.unlock();

	    	// Assign received health to detector health
	    	detectorHealthMutex.lock();
	    	memcpy(&detectorHealth, buffer + sizeof(uint64_t), sizeof(int8_t));
	    	detectorHealthMutex.unlock();
	    }
	}
}

//========================================================================
// Registers queue for intercepting setpoints and timestamps sent by controller
void work(uint8_t qNum)
{
	struct nfq_handle *nfqHandle;

	struct nfq_q_handle *myQueue;
	struct nfnl_handle *netlinkHandle;

	int fd, res;
	int qStatus;

	char buf[4096];

	// Get a queue connection handle from the module
	if (!(nfqHandle = nfq_open()))
	{
		std::perror("Error in nfq_open()");
		exit(EXIT_FAILURE);
	}

	// Unbind the handler from processing any IP packets
	// Not totally sure why this is done, or if it's necessary...
	if (nfq_unbind_pf(nfqHandle, AF_INET) < 0)
	{
		std::perror("Error in nfq_unbind_pf()");
		exit(EXIT_FAILURE);
	}

	// Bind this handler to process IP packets...
	if (nfq_bind_pf(nfqHandle, AF_INET) < 0)
	{
		std::perror("Error in nfq_bind_pf()");
		exit(EXIT_FAILURE);
	}

	// Install a callback on queue qNum
	if (!(myQueue = nfq_create_queue(nfqHandle, qNum, &Callback, NULL)))
	{
		std::perror("Error in nfq_create_queue()");

		exit(EXIT_FAILURE);
	}

	qStatus = nfq_set_queue_maxlen (myQueue, QUEUE_LENGTH);

	// Turn on packet copy mode
	if (nfq_set_mode(myQueue, NFQNL_COPY_PACKET, 0xffff) < 0)
	{
		std::perror("Could not set packet copy mode");
		exit(EXIT_FAILURE);
	}

	netlinkHandle = nfq_nfnlh(nfqHandle);
	fd = nfnl_fd(netlinkHandle);

	while (res = recv(fd, buf, sizeof(buf), 0))
	{
		if(res >= 0) nfq_handle_packet(nfqHandle, buf, res);
	}
}

//========================================================================
// Builds and returns axoheader from packet contents
axoheader_t populate_header(unsigned  char *pktContents)
{
	axoheader_t builtHeader;

	builtHeader.replicaID = replicaID;
	builtHeader.paPort = (*((uint16_t *) (pktContents + DEST_PORT_OFFSET)));
	builtHeader.controllerTimestamp = controllerTimestamp;
	
	detectorTimestampMutex.lock();
	builtHeader.detectorTimestamp = detectorTimestamp;
	detectorTimestampMutex.unlock();
	
	detectorHealthMutex.lock();
	builtHeader.health = detectorHealth;
	detectorHealthMutex.unlock();

	return builtHeader;
}

//========================================================================
// Checks if the packet is a timestamp or a setpoint
bool is_packet_a_timestamp(unsigned  char *pktContents)
{
	uint32_t destIPAddress = (*((uint32_t *) (pktContents + DEST_IP_OFFSET)));
	uint16_t destPort = ntohs((*((uint16_t *) (pktContents + DEST_PORT_OFFSET))));

	if((destIPAddress == tsIPAddress) && (destPort == TS_PORT))
		return true;
	else
		return false;
}

//========================================================================
// Monitors rule changes in config file; updates ip table hooks in kernel
void monitor_rules(uint8_t qNum)
{
	std::ifstream file;
	std::set<std::pair<std::string, uint16_t> > oldRules, newRules;
	std::set<std::pair<std::string, uint16_t> >::iterator it;
	std::pair<std::string, uint16_t> rule;
	std::string ip;
	uint16_t port;

	char addRule[BUFF_SIZE];
	char deleteRule[BUFF_SIZE];

	sprintf(addRule, "iptables -t mangle -A PREROUTING -p udp --dst %s --dport %d -j NFQUEUE --queue-num %d", TS_IP_ADDRESS, TS_PORT, qNum);
	std::system(addRule);

	while(true)
	{	
		// Open file of rules
		file.open(PA_FILE);
		
		// Clear newRules set
		newRules.clear();

		// Read set of rules from file and store in newRules
		while(file >> ip)
		{
			if(file.eof())
				break;
			file >> port;
			rule = make_pair(ip, port);
			newRules.insert(rule);
		}
		
		// Clear the input stream from EOF
		file.clear();

		// Close the file
		file.close();

		// Nothing to change
		if(newRules != oldRules)
		{
			// Check which rules in newRules are not in oldRules
			for(it = newRules.begin(); it != newRules.end(); it++)
			{
				rule = *it;
				
				// Add rule to hook
				if(oldRules.count(rule) == 0)
				{
					sprintf(addRule, "iptables -t mangle -A POSTROUTING -p udp --dst %s --dport %d -j NFQUEUE --queue-num %d", rule.first.c_str(), rule.second, qNum);
					std::system(addRule);
				}
			}

			// Check which rules in oldRules are not in newRules
			for(it = oldRules.begin(); it != oldRules.end(); it++)
			{
				rule = *it;
				// Delete rule from hook
				if(newRules.count(rule) == 0)
				{
					sprintf(deleteRule, "iptables -t mangle -D POSTROUTING -p udp --dst %s --dport %d -j NFQUEUE --queue-num %d", rule.first.c_str(), rule.second, qNum);
					std::system(deleteRule);
				}
			}

			// Save newRules as oldRules
			oldRules = newRules;
		}

		// Wait for 30/60 seconds
		sleep(MONITOR_WAIT_TIME);
	}
}

//========================================================================
// Checks ptp synchronization
void check_synch()
{
	std::ifstream ifile;
	std::string temp;
	uint64_t currentTime;
	struct tm t = {0};
	std::string milliseconds;
	uint64_t synchTime;

	std::string ptpCheck = "sh ../scripts/ptpCheck.sh";

	bool synched;

	while(true)
	{
		currentTime = get_timestamp_in_milliseconds();

		std::stringstream sss;
		sss << exec(ptpCheck.c_str());

		sss >> temp;
		if(temp == "S" || temp == "D") 
		{

			sss >> t.tm_year >> t.tm_mon >> t.tm_mday >> t.tm_hour >> t.tm_min >> t.tm_sec >> milliseconds;

			t.tm_year -= 1900;
			t.tm_mon--;
			milliseconds = milliseconds.substr(0, 3);

			synchTime = (uint64_t) mktime(&t);
			synchTime = synchTime * 1000 + std::stoi(milliseconds);

			synched = (synchTime > currentTime - SYNCH_THRESHOLD) && (synchTime < currentTime + SYNCH_THRESHOLD) ;
		}
		else
		{
			synched = false;
		}

		synchronizationMutex.lock();
		synchronized = synched;
		synchronizationMutex.unlock();

		ifile.close();

		sleep(SYNCH_SLEEP);
	}
}

//========================================================================
// Execute bash command
std::string exec(const char* cmd) {
    
    char buffer[128];
    std::string result = "";
    
    FILE* piped = popen(cmd, "r");
    if (!piped)
    {
    	std::perror("popen() failed!");
    	return "";
    }

    try {
        
        while (!feof(piped))
            if (fgets(buffer, 128, piped) != NULL)
                result += buffer;

    } catch (...) {
        pclose(piped);
        std::perror("fgets() failed!");
        return "";
    }

    pclose(piped);
    return result;
}

//========================================================================
// Handles registered signals
void signal_handler(int sig)
{
	// Close sockets
	close(sockfdDetector);

	exit(EXIT_SUCCESS);
}