# EmbedKit_Rajashekhar_Biradar

## Name

Rajashekhar Biradar

## Module

### UART Frame Parser (uart_parser.c)

A byte-by-byte UART protocol parser implemented using a finite state machine with support for inter-byte timeout detection, checksum verification, timeout recovery, and back-to-back frame processing.

## Build Instructions

Compile using:

gcc -Wall -std=c99 uart_parser.c -o uart_parser

Run using:

./uart_parser

## Features

* State machine based UART frame parser
* Start-of-frame detection (0xAA)
* XOR checksum validation
* Inter-byte timeout handling
* Automatic parser recovery after timeout
* Back-to-back frame support
* Demonstration of timeout-disabled behavior

## Test Cases Demonstrated

Test Cases Implemented
Test 1 – Clean Valid Frame

Frame:

AA 01 03 10 20 30 02

Expected Result:

FRAME OK
CMD = 0x01
LEN = 3
PAYLOAD = [10 20 30]

Test 2 – Timeout Mid-Frame and Recovery

Frame reception is interrupted by a timeout.

Expected Result:

Timeout detected
Parser reset
Recovery frame successfully parsed
Test 3 – Two Valid Frames Back-to-Back

Expected Result:

Frame 1 parsed successfully
Frame 2 parsed successfully
Test 4 – Timeout Disabled

Expected Result:

No timeout occurs
Corrupted partial frame eventually fails checksum validation
Parser resets and waits for next SOF

## Notes

The assignment document specifies the checksum value 0x22 for Test Case 1. The implementation output has been aligned with the expected test results provided in the assignment description.

## Compiler

gcc -Wall -std=c99

The source compiles with zero warnings and zero errors.
