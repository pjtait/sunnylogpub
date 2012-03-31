#!/usr/bin/env python
# post updated status of a PV system to pvoutput.org for a SMA RS-485 interface
# data is received from UDP broadcasts from the 'sunnylogpub' server
# parts Copyright Andrew Tridgell 2010
# Copyright Philip Tait 2012
# released under GNU GPL v3 or later

import sys
import sys
import datetime
import socket
import httplib, urllib
from optparse import OptionParser

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
        lastdata = fh.readlines()[-1].strip().split(',')
        self.lastDate = now
        self.lastPower = float(lastdata[29]) * 1000.0
        return self.lastPower

def post_output(date, time, energy, power):
    dataDict = { 'd' : date, 'pt' : time, 'g' : energy, 'pp' : power}
    urlApp = 'addoutput.jsp'
    post_update(dataDict, urlApp)
    
def post_status(date, time, energy, power):
    dataDict = { 'd' : date, 't' : time, 'v1' : energy, 'v2' : power}
    urlApp = 'addstatus.jsp'
    post_update(dataDict, urlApp)
        
def post_update(dataDict, urlApp):
    '''post status to pvoutput'''
    if opts.url[0:7] != "http://":
        print("URL must be of form http://host/service")
        sys.exit(1)
    url = opts.url[7:].split('/')
    host = url[0]
    service = '/' + '/'.join(url[1:]) + urlApp
    params = urllib.urlencode(dataDict)
    headers = {"Content-type": "application/x-www-form-urlencoded",
               "Accept": "text/plain",
               "X-Pvoutput-SystemId" : opts.systemid,
               "X-Pvoutput-Apikey" : opts.apikey}
    if opts.verbose:
        print("Connecting to %s" % host)
    conn = httplib.HTTPConnection(host)
    if opts.verbose:
        print("sending: %s to '%s'" % (params, service))
    conn.request("POST", service, params, headers)
    response = conn.getresponse()
    if response.status != 200:
        print "POST failed: ", response.status, response.reason, response.read()
        sys.exit(1)
    if opts.verbose:
        print "POST OK: ", response.status, response.reason, response.read()

def proc_data(data):
    payload = data.split(':')[1]
    fields = payload.strip().split(',')
    date = fields[0][0:8]
    pt = "%s:%s" % (fields[0][8:10],fields[0][10:12])
    pp = fields[23]
    g = (float(fields[29])*1000.0)-powerTot.getTotal()
    if opts.dryrun:
        print(date, pt, g, pp)
        return None
    else:
        if pp > 0.0:
            return(date, pt, g, pp)
    
    
#proc_data("20120329094917,2.00195e+09,,228,,10,,2,,2.73,,2.73,,2,Mpp-Operation,340,,3.028,,240.6,,59.99,,728,,37.9,,2.242,,483.986,,636.388,,655.995,,463,,2.00195e+09,,822.776,,7,Mpp,3,240V,0,-----,0,-------,")
#sys.exit()
parser = OptionParser()
parser.add_option("", "--apikey", help="pvoutput API key", default=None)
parser.add_option("", "--systemid", help="pvoutput system ID (an integer)", default=None)
parser.add_option("", "--url", help="PV output status URL",
                  default="http://pvoutput.org/service/r2/")
parser.add_option("", "--verbose", help="be more verbose", action='store_true',
                  default=False)
parser.add_option("", "--dryrun", help="don't send to pvoutput", action='store_true',
                  default=False)
(opts, args) = parser.parse_args()
if opts.systemid is None:
    print("You must provide a system ID")
    sys.exit(1)
if opts.apikey is None:
    print("You must provide an API key")
    sys.exit(1)
logdir = args[0]
port = 65001
s = socket. socket(socket. AF_INET, socket. SOCK_DGRAM)
# Accept UDP datagrams, on the given port, from any sender
s. bind(("", port) )
print "waiting on port: ", port
powerTot = PowerTotal()
# force first update
lastStatusTime = datetime.datetime.now() - datetime.timedelta(minutes=10)
minute = datetime.timedelta(minutes=1)
while True:
    # Receive up to 1024 bytes in a datagram
    data, addr = s. recvfrom(1024)
    if len(data) <= 0:
        continue
    now = datetime.datetime.now()
    if (now - lastStatusTime) < minute:
        continue
    params = proc_data(data)
    if not params:
        continue
    lastStatusTime = now
    lastStatusTime = now
    post_status(*params)
    
