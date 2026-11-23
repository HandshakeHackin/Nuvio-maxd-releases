# SPDX-License-Identifier: GPL-3.0-or-later
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
OUTPUT ?= ../build/usb/nuvio-usb-reader.elf
$(OUTPUT): helper.cpp protocol.hpp ../src/live_usb.hpp ../src/live_tv.h ../third_party/json.hpp
	@mkdir -p $(dir $(OUTPUT))
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Wno-misleading-indentation -I../third_party -c helper.cpp -o $(OUTPUT).o
	$(CXX) $(OUTPUT).o -Wl,--build-id=none -o $@
