#ifndef REPORT_HPP
#define REPORT_HPP

#define REPORT_BYTES sizeof(report_t)

typedef struct {
	uint8_t replicaID;
	uint64_t controllerTimestamp;
	uint64_t detectorTimestamp;
	int8_t health;
	bool valid;
}__attribute__((packed)) report_t;

#endif