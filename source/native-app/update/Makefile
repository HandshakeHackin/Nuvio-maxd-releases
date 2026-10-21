# SPDX-License-Identifier: GPL-3.0-or-later
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
OUTPUT ?= ../build/update/nuvio-updater.elf
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -I../third_party
LIBS := $(shell $(PKG_CONFIG) --static --libs libcurl openssl)
$(OUTPUT): helper.cpp core.cpp core.hpp release_wait.hpp ../third_party/json.hpp
	@mkdir -p $(dir $(OUTPUT))
	$(CXX) $(CXXFLAGS) helper.cpp core.cpp -Wl,--build-id=none $(LIBS) -o $@
