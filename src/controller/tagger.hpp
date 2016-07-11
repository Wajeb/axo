#ifndef TAGGER_HPP
#define TAGGER_HPP

#include <netinet/in.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <set>

extern "C" {
  #include <linux/netfilter.h>
  #include <libnetfilter_queue/libnetfilter_queue.h>
}

#define DELETE_ALL_RULES "iptables -t mangle -F"
#define PA_FILE "paAddress.conf"
#define SYNCH_SLEEP 1
#define SYNCH_THRESHOLD 1000
#define MONITOR_WAIT_TIME 30
#define QUEUE_LENGTH 100
#define DEST_IP_OFFSET 16
#define DEST_PORT_OFFSET 22
#define PAYLOAD_OFFSET 28
#define IP_TOTAL_LENGTH_OFFSET 2
#define UDP_TOTAL_LENGTH_OFFSET 24
#define IP_CHECKSUM_OFFSET 10
#define IP4_HDRLEN 20

// Receives timestamp and health from detector; stores in global variables
void receive_from_detector(int sockfd);

// Registers queue for intercepting setpoints and timestamps sent by controller
void work(uint8_t qNum);

// Builds and returns axoheader from packet contents
axoheader_t populate_header(unsigned char *pktContents);

// Checks if the packet is a timestamp or a setpoint
bool is_packet_a_timestamp(unsigned char *pktContents);

// Monitors rule changes in config file; updates ip table hooks in kernel
void monitor_rules(uint8_t qNum);

// Checks ptp synchronization
void check_synch();

// Execute bash command
std::string exec(const char* cmd);

#endif