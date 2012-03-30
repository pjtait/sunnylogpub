import sys
import sys
import datetime
import socket

class PowerTotal(object):
    def __init__(self):
        self.lastDate = None
        self.lastPower = None
        
    def getTotal(self):
        now = datetime.datetime.now()
        if self.lastDate and (now.date() == self.lastDate.date) and \
            self.lastPower:
            return self.lastPower
        yesterday = now - datetime.timedelta(days=1)
        filename = "%s/pvdata-%s" % (logdir, yesterday.strftime("%Y%m%d"))
        fh = open(filename, 'r')
        lastdata = fh.readlines()[-1].split(',')
        self.lastDate = now
        self.lastPower = float(lastdata[29]) * 1000.0
        return self.lastPower
        
def proc_data(data):
    # check date
    
    payload = data.split(':')[1]
    fields = payload.split(',')
    print fields
    date = fields[0][0:9]
    pt = "%s:%s" % (fields[0][9:11],fields[0][11:13])
    pp = fields[23]
    print date, pt, pp, (float(fields[29])*1000.0)-powerTot.getTotal()
    
#proc_data("20120329094917,2.00195e+09,,228,,10,,2,,2.73,,2.73,,2,Mpp-Operation,340,,3.028,,240.6,,59.99,,728,,37.9,,2.242,,483.986,,636.388,,655.995,,463,,2.00195e+09,,822.776,,7,Mpp,3,240V,0,-----,0,-------,")
#sys.exit()
logdir = sys.argv[1]
port = 65001
s = socket. socket(socket. AF_INET, socket. SOCK_DGRAM)
# Accept UDP datagrams, on the given port, from any sender
s. bind(("", port) )
print "waiting on port: ", port
powerTot = PowerTotal()
while True:
    # Receive up to 1024 bytes in a datagram
    data, addr = s. recvfrom(1024)
    print data
    if len(data) > 0:
        proc_data(data)
