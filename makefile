# Makefile

CC = g++

OUTPUT = -o

CXX_FLAGS = -std=c++0x

THREAD_LIB = -lpthread

NFQUEUE_LIB = -lnfnetlink -lnetfilter_queue

OUT_DIR = build

TAGGER = src/controller/tagger.cpp

TAGGER_FUNCTIONS = src/controller/taggerFunctions.cpp

COMMON = src/common/common.cpp

SEND_RAW = src/common/sendRaw.cpp

MASKER = src/process-agent/masker.cpp

MASKER_COMM = src/process-agent/maskerCommelec.cpp

MASKER_FUNCTIONS = src/process-agent/maskerFunctions.cpp

default:
	@echo "Usage: make <axo component>"

all: tagger masker

tagger: $(TAGGER) $(TAGGER_FUNCTIONS) $(SEND_RAW) $(COMMON)
	$(CC) $(OUTPUT) $(OUT_DIR)/tagger $(TAGGER) $(TAGGER_FUNCTIONS) $(SEND_RAW) $(COMMON) $(CXX_FLAGS) $(THREAD_LIB) $(NFQUEUE_LIB)

masker: $(MASKER) $(MASKER_FUNCTIONS) $(SEND_RAW) $(COMMON)
	$(CC) $(OUTPUT) $(OUT_DIR)/masker $(MASKER) $(MASKER_FUNCTIONS) $(SEND_RAW) $(COMMON) $(CXX_FLAGS)

clean:
	rm -rf OUT_DIR/tagger
	rm -rf OUT_DIR/masker