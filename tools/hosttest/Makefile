# Host-side unit tests for the firmware's pure logic. Native gcc, no ESP-IDF.
#   make test     build + run all host tests
#   make clean
CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11 -O2
MQTT := ../../components/mqtt_client

.PHONY: test clean

test: test_cal_format
	./test_cal_format

test_cal_format: test_cal_format.c $(MQTT)/cal_format.c
	$(CC) $(CFLAGS) -I$(MQTT) -o $@ $^

clean:
	rm -f test_cal_format
