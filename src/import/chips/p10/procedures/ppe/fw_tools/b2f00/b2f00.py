#!/usr/bin/env python3
# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/import/chips/p10/procedures/ppe/fw_tools/b2f00/b2f00.py $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2023
# [+] International Business Machines Corp.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# IBM_PROLOG_END_TAG
import json
import sys
import os

from udparsers.helpers.miscUtils import getLid
from udparsers.helpers.hostfw_trace import get_binary_trace_data_as_string
import  udparsers.b2900.pmPpe2fsp

#PARSER Version
parserVersion = 1.0
XGPE_STRING_LID_FILE = "81E00690.lid"

##############################################################################
# Function - Functions - Functions - Functions - Functions
##############################################################################

#verifies the integrity of trace parser buffer
def checkForPpeTraceBuff():
    #buffer magic word
    bufferMagicWord = "xgpe_p10dd"
    bufferMagicWordHexCode = bytes(bufferMagicWord, 'utf-8')

    fBin = open( "/tmp/xgpeElogBin.bin", 'rb' )
    data_read = fBin.read()

    magicWordOffset = data_read.find( bufferMagicWordHexCode )
    if magicWordOffset != -1:
        fBin.seek( magicWordOffset - 4 )
        version = fBin.read(2)
        version = int.from_bytes( version, "big" )

        if version != 0x0002 :
            sys.exit( "Corrupt Trace File Buffer. Ver Read is " + str(version)  )
    else:
        sys.exit("XGPE Trace Buffer Not Found" )

    fBin.close()

    return magicWordOffset

def parseXgpeUserDataSection(data):

    with open( "/tmp/xgpeElogBin.bin", "wb" ) as xgpeTraceBinFile:
        xgpeTraceBinFile.write( data )
    magicWordOffset = checkForPpeTraceBuff()
    cmd = "dd skip=" + str(magicWordOffset - 4 ) + " count=" + str(1080) + " if=" + "/tmp/xgpeElogBin.bin " + " of=" + "/tmp/xgpeTrace.bin" + " bs=1 >/dev/null 2>&1"
    rc = os.system( cmd )
    if( rc ):
        print( "Failed To Extract XGPE Trace Section. RC : " + str( rc ) )


def parseUDToJson(subType, ver, data):
    parseXgpeUserDataSection( data )
    #Execute pmPpe2fsp tool to convert ppe trace to fsp trace.
    #This provide the right path to the tool as per BMC env
    ppeFormatFile = "/tmp/xgpeTrace.bin"
    fspFormatFile = "/tmp/xgpeTrace_fsp.bin"
    udparsers.b2900.pmPpe2fsp.get_pm_trace_data_as_string(ppeFormatFile, fspFormatFile)

    with open( fspFormatFile, 'rb' ) as fspFormat:
        readData = fspFormat.read()

    traceDict = dict()

    traceStringFile = getLid(XGPE_STRING_LID_FILE)

    if traceStringFile == "":
        traceDict["File not found"] = XGPE_STRING_LID_FILE
        traceStr = json.dumps(traceDict)
        return traceStr

    startPos = 0
    numTraces = -1
    ( retVal, traceDataString, warningMessages ) = udparsers.helpers.hostfw_trace.get_binary_trace_data_as_string( readData, startPos, numTraces, traceStringFile )

    traceDict["XGPE Traces" ]  = traceDataString.split( "\n" )
    traceStr = json.dumps(traceDict)

    if os.path.exists( "/tmp/xgpeElogBin.bin" ) :
        os.remove( "/tmp/xgpeElogBin.bin" )
    if os.path.exists( "/tmp/xgpeTrace_fsp.bin" ) :
        os.remove( "/tmp/xgpeTrace_fsp.bin" )
    if os.path.exists( "/tmp/xgpeTrace.bin" ):
        os.remove( "/tmp/xgpeTrace.bin" )

    return traceStr
