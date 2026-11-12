SRC = src
SKETCH = $(SRC)/main.ino
PORT = /dev/ttyUSB0
BOARD = "esp8266:esp8266:generic"

CXXFLAGS = --pref "compiler.cpp.extra_flags=-I$(SRC)"


.PHONY: all verify upload

all: verify

verify:
	arduino --verify \
	        --board $(BOARD) \
	        $(CXXFLAGS) \
	        $(SKETCH)

upload:
	arduino --upload \
	        --board $(BOARD) \
	        --port $(PORT) \
	        $(CXXFLAGS) \
	        $(SKETCH)
