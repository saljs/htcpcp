HYPER TEXT COFFEE POT CONTROL PROTOCOL (HTCPCP/1.0)  
    RFC2324
    https://www.ietf.org/rfc/rfc2324.txt
    
HTCPCP is a protocol for the control of coffee pots over the internet. This is an implementation whipped up in C for the Raspberry Pi. It's been tested with Arch Linux on a model B Revision 1 board, but should work on any board with WiringPi installed. 

To compile:
```
    make && make install
```
    
To connect to the server compile the coffee binary and run:
```
    coffee <host> <port> [Message]
```

Example server config file `/etc/htcpcp.conf`:
```
#HTCPCP conf file
time_to_brew = 1800
port = 80
relay_pin = 0
button_pin = 2
led_pin = 3
```
