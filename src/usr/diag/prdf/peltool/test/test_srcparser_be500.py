#!/usr/bin/env python3
# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/usr/diag/prdf/peltool/test/test_srcparser_be500.py $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2021,2023
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
import os
import sys
import unittest
from collections import OrderedDict

import srcparsers.be500.be500


class TestPrdSrcParsing(unittest.TestCase):
    def testOutput(self):
        refcode = "BC23E504"
        hex2 = "000000E0"
        hex3 = "00000B00"
        hex4 = "00000000"
        hex5 = "00200000"
        hex6 = "004B0021"
        hex7 = "00000303"
        hex8 = "83810008"
        hex9 = "A100C708"

        testOut = srcparsers.be500.be500.parseSRCToJson(
            refcode, hex2, hex3, hex4, hex5, hex6, hex7, hex8, hex9
        )

        jsonOut = json.loads(testOut)
        # Print the output for manual testing
        print(json.dumps(jsonOut, indent=4))

        self.assertEqual(jsonOut["Target Desc"], "node 0 ocmb 33")
        self.assertEqual(jsonOut["Signature"], "RDFFIR[8]: Mainline read NCE")
        self.assertEqual(jsonOut["Attn Type"], "RECOVERABLE")

    def testCorefir(self):
        refcode = "BC13E504"
        hex2 = "000000E0"
        hex3 = "00000B00"
        hex4 = "00000000"
        hex5 = "01200000"
        hex6 = "0007002E"
        hex7 = "00000303"
        hex8 = "BB710008"
        hex9 = "00000000"

        testOut = srcparsers.be500.be500.parseSRCToJson(
            refcode, hex2, hex3, hex4, hex5, hex6, hex7, hex8, hex9
        )

        jsonOut = json.loads(testOut)
        # Print the output for manual testing
        print(json.dumps(jsonOut, indent=4))

        self.assertEqual(jsonOut["Target Desc"], "node 0 proc 1 core 14")
        sigCheck = "EQ_L2_FIR[8]: L2 directory CE due to stuck bit"
        self.assertEqual(jsonOut["Signature"], sigCheck)
        self.assertEqual(jsonOut["Attn Type"], "RECOVERABLE")

    def testCodeFail(self):
        refcode = "BC13E580"
        hex2 = "000000E0"
        hex3 = "00000B00"
        hex4 = "00000000"
        hex5 = "01200000"
        hex6 = "0007002E"
        hex7 = "00000303"
        hex8 = "BB710008"
        hex9 = "00000000"

        testOut = srcparsers.be500.be500.parseSRCToJson(
            refcode, hex2, hex3, hex4, hex5, hex6, hex7, hex8, hex9
        )

        jsonOut = json.loads(testOut)
        # Print the output for manual testing
        print(json.dumps(jsonOut, indent=4))

        sigCheck = "PRD Internal Firmware Software Fault"
        self.assertEqual(jsonOut["Signature"], sigCheck)


if __name__ == "__main__":
    test = TestPrdSrcParsing()
    test.testOutput()
    test.testCorefir()
    test.testCodeFail()
