# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/import/hwpf/fapi2/src/fapi2_utils.mk $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2016,2019
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
MODULE=fapi2_utils
$(call ADD_MODULE_SRCDIR,$(MODULE),$(GENPATH))
$(call ADD_MODULE_INCDIR,$(MODULE),$(GENPATH))
$(call ADD_MODULE_INCDIR,$(MODULE),$(FAPI2_PLAT_INCLUDE))
$(call ADD_MODULE_INCDIR,$(MODULE),$(ROOTPATH)/chips/p9/common/include/)
$(call ADD_MODULE_INCDIR,$(MODULE),$(ROOTPATH)/chips/centaur/common/include/)
$(call ADD_MODULE_INCDIR,$(MODULE),$(ROOTPATH)/chips/common/utils/scomt/)
$(call ADD_MODULE_OBJ,$(MODULE),fapi2_utils.o)
$(call ADD_MODULE_OBJ,$(MODULE),fapi2_attribute_service.o)
$(call ADD_MODULE_OBJ,$(MODULE),collect_reg_ffdc_regs.o)
$(call BUILD_MODULE)
