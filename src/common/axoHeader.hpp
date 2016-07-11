#ifndef AXO_HEADER_HPP
#define AXO_HEADER_HPP

#define AXO_HEADER_BYTES sizeof(axoheader_t)

typedef struct {
	uint8_t replicaID;
	uint16_t paPort;
	uint64_t controllerTimestamp;
	uint64_t detectorTimestamp;
	int8_t health;	
}__attribute__((packed)) axoheader_t;

#endif