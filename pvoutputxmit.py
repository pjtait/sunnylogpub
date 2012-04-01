#!/usr/bin/env python
# post updated status of a PV system to pvoutput.org for a SMA RS-485 interface
# data is received from UDP broadcasts from the 'sunnylogpub' server
# parts Copyright Andrew Tridgell 2010
# Copyright Philip Tait 2012
# released under GNU GPL v3 or later

import sys
import os
import datetime
import time
import socket
import httplib, urllib
from optparse import OptionParser

def getTotal():
    now = datetime.datetime.now()
    yesterday = now - datetime.timedelta(days=1)
    filename = "%s/dailyenergy-%s" % (logdir, yesterday.strftime("%Y%m%d"))
    fh = open(filename, 'r')
    lastdata = fh.readline().strip()
    fh.close()
    return float(lastdata) * 1000.0

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
    fields = data.strip().split(',')
    date = fields[0][0:8]
    pt = "%s:%s" % (fields[0][8:10],fields[0][10:12])
    pp = fields[23]
    g = (float(fields[29])*1000.0)-getTotal()
    if opts.dryrun:
        print(date, pt, g, pp)
        return None
    else:
        if pp > 0.0:
            return(date, pt, g, pp)

def check_send_interval():
    stampfile = 'pvoutlastsend'
    too_soon = True
    try:
        statinfo = os.stat(stampfile)
        if (time.time() - statinfo.st_atime) > 60:
            too_soon = False
    except OSError, e:
        if e.errno == 2:
            too_soon = False
    with file(stampfile, 'w'):
        os.utime(stampfile, None)
    return not too_soon
    
#proc_data("20120329094917,2.00195e+09,,228,,10,,2,,2.73,,2.73,,2,Mpp-Operation,340,,3.028,,240.6,,59.99,,728,,37.9,,2.242,,483.986,,636.388,,655.995,,463,,2.00195e+09,,822.776,,7,Mpp,3,240V,0,-----,0,-------,")
#sys.exit()
if not check_send_interval():
    sys.exit()
parser = OptionParser()
parser.add_option("", "--apikey", help="pvoutput API key")
parser.add_option("", "--systemid", help="pvoutput system ID (an integer)")
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
data = args[1]
if len(data) <= 0:
    sys.exit()
params = proc_data(data)
if not params:
    sys.exit()
post_status(*params)
    
