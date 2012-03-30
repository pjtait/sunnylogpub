import sys
import socket

def proc_data(data):
    payload = data.split(':')[0]
    fields = payload.split(',')
    date = fields[0][0:8]
    pt = "%s:%s" % (fields[0][8:10],fields[0][12:14])
    pp = fields[23]
    print date, pt, pp
    
#proc_data("20120329094917,2.00195e+09,,228,,10,,2,,2.73,,2.73,,2,Mpp-Operation,340,,3.028,,240.6,,59.99,,728,,37.9,,2.242,,483.986,,636.388,,655.995,,463,,2.00195e+09,,822.776,,7,Mpp,3,240V,0,-----,0,-------,")
#sys.exit()
port = 65001
s = socket. socket(socket. AF_INET, socket. SOCK_DGRAM)
# Accept UDP datagrams, on the given port, from any sender
s. bind(("", port) )
print "waiting on port: ", port
while True:
    # Receive up to 1024 bytes in a datagram
    data, addr = s. recvfrom(1024)
    if len(data) > 0:
        proc_data(data)
