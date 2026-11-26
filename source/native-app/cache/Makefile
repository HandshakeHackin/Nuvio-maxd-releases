# SPDX-License-Identifier: GPL-3.0-or-later
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
OUTPUT ?= ../build/cache/nuvio-usb-cache.elf
$(OUTPUT): helper.cpp protocol.hpp storage.hpp ../third_party/json.hpp
	@mkdir -p $(dir $(OUTPUT))
	$(CXX) -std=c++20 -O2 -Wall -Wextra -I../third_party -c helper.cpp -o $(OUTPUT).o
	$(CXX) $(OUTPUT).o -Wl,--build-id=none -o $@
