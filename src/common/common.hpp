#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include <sys/time.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "axoHeader.hpp"
#include "report.hpp"

#define TS_IP_ADDRESS "127.0.0.1"
#define TS_PORT 1002
#define MASKER_PORT 1003 // receiving port
#define DETECTOR_PORT 1004 // receiving port
#define TAGGER_DETECTOR_PORT 1005 // receiver_sender port
#define REBOOTER_DETECTOR_PORT 1006 // receiver_sender port
#define DETECTOR_REBOOTER_PORT 1007 // receiver_sender port
#define REBOOTER_PORT 1008 // receiving_port
#define REBOOTER_LOCAL_PORT 1001 // local port in the rebooter
#define BUFF_SIZE 1024
#define H_MAX 127

// Registers signal handlers
void register_signal_handlers();

// Handles registered signals; closes sockets
void signal_handler(int sig);

// Returns an ID corresponding to the last 8 bits of the IP address
int get_ID_from_ip_address(char *ipAddress);

// Returns milliseconds passed since epoch
uint64_t get_timestamp_in_milliseconds();

// Creates a new UDP sender socket and returns socket identifier
int open_sender_socket();

// Returns a socket identifier bound to portNumber
int open_receiver_socket(uint16_t portNumber);

// Fills address with IP address and port number
int fill_sender_socket(struct sockaddr_in &address, const char * ipaddress, uint16_t portNumber);

// Returns a multicast socket bound to localIPAdress and listeningPort
int open_multicast_receiver_socket(char* multicastIPAddress, char* localIPAddress, uint16_t listeningPort);

// Receives multicast report from masker
int receive_multicast_data(int sockfd, char * buffer);

// Drops multicast membership
void drop_multicast_memebership(int sockfd, char* multicastIPAddress, char* localIPAddress);

// Returns a multicast socket
int open_multicast_sender_socket(char* outgoing_ip);

// Fills multicast address and port number into a struct
void set_multicast_info(const char *multicast_ip, uint16_t multicast_port, sockaddr_in &multicast_addr);

// Sends multicast message
void send_multicast(char *buffer, uint32_t size, int sockfd, sockaddr_in destination);

#endif
