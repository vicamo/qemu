#
# Copyright (C) 2013 You-Sheng Yang
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

EMULATOR_TARGET_CPUS := arm i386

EMULATOR_INTERMEDIATES_DIR := $(call intermediates-dir-for,EXECUTABLES,qemu,host,)

.PHONY: qemu-configure qemu-build qemu-install
qemu-configure: $(EMULATOR_INTERMEDIATES_DIR)/qemu-configure-stamp
qemu-install: $(EMULATOR_INTERMEDIATES_DIR)/qemu-install-stamp

$(EMULATOR_INTERMEDIATES_DIR)/qemu-configure-stamp: PRIVATE_CONFIGURE := \
    $(ANDROID_BUILD_TOP)/$(LOCAL_PATH)/configure \
    --prefix=$(ANDROID_BUILD_TOP)/$(HOST_OUT) \
    --target-list=$(subst $(space),$(comma),$(foreach target,$(EMULATOR_TARGET_CPUS),$(target)-softmmu)) \
    --bindir=$(ANDROID_BUILD_TOP)/$(HOST_OUT_EXECUTABLES) \
    --libexecdir=$(ANDROID_BUILD_TOP)/$(HOST_OUT_EXECUTABLES) \
    --libdir=$(ANDROID_BUILD_TOP)/$(HOST_OUT_SHARED_LIBRARIES) \
    --datadir=$(ANDROID_BUILD_TOP)/$(HOST_OUT)/usr/share \
    --source-path=$(ANDROID_BUILD_TOP)/$(LOCAL_PATH) \
    --cc=$(HOST_CC) \
    --disable-user \
    --disable-docs
$(EMULATOR_INTERMEDIATES_DIR)/qemu-configure-stamp:
	@mkdir -p $(dir $@)
	@echo "host qemu-configure <= $(PRIVATE_CONFIGURE)"
	$(hide) cd $(dir $@) && $(PRIVATE_CONFIGURE)
	$(hide) touch $@

qemu-build: $(EMULATOR_INTERMEDIATES_DIR)/qemu-build-stamp
$(EMULATOR_INTERMEDIATES_DIR)/qemu-build-stamp: qemu-configure
	$(hide) cd $(dir $@) && $(MAKE) $(MAKEFLAGS)
	$(hide) touch $@

$(EMULATOR_INTERMEDIATES_DIR)/qemu-install-stamp: qemu-build
	$(hide) cd $(dir $@) && $(MAKE) $(MAKEFLAGS) install
	$(hide) touch $@
