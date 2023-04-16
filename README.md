# Boost Alarm Service
The goal is to implement a TCP server to which alarm clock entries can be sent. 
The client should be notified via a socket connection when the timer has expired. 
Since the service can be used by multiple microservices/clients, we need to add some constraints.

## Limits
Since the server must handle multiple connections, the maximum number should be regulated. We decide to only allow 100 connections. After that we should close our socket acceptor. When a client disconnects, then we should reopen the acceptor, so new clients can connect.

A client could have multiple alarms at the time (100 per client), so we should think about how we could handle this in the future.

## Creating the Alarm
For this we have developed a message format. The client can send this message and should receive the answer when the alarm time has come. 

Given is a format for the creation of an alarm:

Bytes 0...3	            Bytes 4...11    	Bytes 12...15   	    Bytes 16...

Request ID (uint32)	    Due Time (uint64)	Cookie size (uint32)	Cookie data

Due Time is the desired alarm time. It is defined as "the number of seconds elapsed since 00:00 hours, Jan 1, 1970 UTC".
All integers are serialized in Network Byte Order.
When the connected client for example sends Request ID = 1 Due Time: actual timestamp (e.g.: 1652873776) Cookie data: Wake up lovely service.

Then the client should receive a message in the following format:

Bytes 0...3	        Bytes 4...7	            Bytes 8...

Request ID (uint32)	Cookie size (uint32)	Cookie data


