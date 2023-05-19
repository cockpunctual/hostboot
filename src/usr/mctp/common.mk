# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/usr/mctp/common.mk $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2020,2023
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

EXTRAINCDIR += ${ROOTPATH}/src/usr/mctp
EXTRAINCDIR += ${ROOTPATH}/src/import
EXTRAINCDIR += ${ROOTPATH}/src/include/usr/mctp
EXTRAINCDIR += ${ROOTPATH}/src/subtree/openbmc/libmctp/

OBJS += core.o
OBJS += alloc.o
OBJS += log.o
OBJS += mctp_trace.o
OBJS += mctp_plat_core.o
OBJS += crc32.o

VPATH += ${ROOTPATH}/src/usr/mctp
VPATH += ${ROOTPATH}/src/subtree/openbmc/libmctp/
