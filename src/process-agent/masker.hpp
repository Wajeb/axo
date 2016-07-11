#ifndef MASKER_HPP
#define MASKER_HPP

#include "../common/sendRaw.hpp"

#include <fstream>

#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_UDP, INET_ADDRSTRLEN

#define IP4_HDRLEN 20         // IPv4 header length
#define UDP_HDRLEN  8         // UDP header length, excludes data

#define MULTICAST_GROUP "224.1.1.1"
#define MASKER_BOUND 1

// Receives setpoint from tagger
int receive_setpoint(int sockfd, char * buffer, sockaddr_in &fromAddress);

// Checks if setpoint is valid or invalid
bool isValid(uint64_t controllerTimestamp, uint64_t tau);

// Makes report for the detector
void make_report(uint8_t replicaID, uint64_t controllerTimestamp, uint64_t detectorTimestamp, int8_t health, bool valid, char* report);

void prepare_fixed_ip_header(ip &iphdr);

int open_raw_socket();

int prepare_raw_packet(char *src_ip, uint16_t srcPort, char *dst_ip, uint16_t dstPort, char * data, int datalen, ip &iphdr, uint8_t *packet);

void prepare_pa_destination(sockaddr_in &sin, ip &iphdr);

#endif
