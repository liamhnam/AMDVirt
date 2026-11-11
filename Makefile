# Top-level build (needs the Xcode Command Line Tools).
#
#   make test     unit tests, userland only
#   make kext     build/AMDVirt.kext
#   make tools    build/amdvirt-info and the test programs (amdvirt-*-test, hvf-smoke, hvf-ro-map-test, hvf-race-test)
#   make all      kext + tools
#   make clean

BUILD := build
CC    ?= cc
CXX   ?= c++

.PHONY: all test tools kext clean

all: kext tools

$(BUILD):
	mkdir -p $(BUILD)

test: | $(BUILD)
	$(CC) -Wall -Wextra -Werror -o $(BUILD)/host_test tests/host_test.c
	$(BUILD)/host_test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -o $(BUILD)/vmcb_layout tests/vmcb_layout.cpp
	$(BUILD)/vmcb_layout
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -o $(BUILD)/npt_test tests/npt_test.cpp
	$(BUILD)/npt_test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -o $(BUILD)/vmx_map_test tests/vmx_map_test.cpp
	$(BUILD)/vmx_map_test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -o $(BUILD)/vmcs_state_test tests/vmcs_state_test.cpp
	$(BUILD)/vmcs_state_test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -mavx2 -fsanitize=address,undefined -g -o $(BUILD)/fpu_state_test tests/fpu_state_test.cpp
	$(BUILD)/fpu_state_test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -o $(BUILD)/msr_map_test tests/msr_map_test.cpp
	$(BUILD)/msr_map_test

tools: | $(BUILD)
	$(CC) -arch x86_64 -Wall -Wextra -O2 -o $(BUILD)/amdvirt-info tools/amdvirt-info/main.c \
		-framework IOKit -framework CoreFoundation
	$(CC) -arch x86_64 -Wall -Wextra -O2 -o $(BUILD)/amdvirt-guest-test tools/amdvirt-guest-test/main.c \
		-framework IOKit -framework CoreFoundation
	$(CC) -arch x86_64 -Wall -Wextra -O2 -o $(BUILD)/amdvirt-map-test tools/amdvirt-map-test/main.c \
		-framework IOKit -framework CoreFoundation
	$(CC) -arch x86_64 -Wall -Wextra -O2 -o $(BUILD)/amdvirt-vmx-test tools/amdvirt-vmx-test/main.c \
		-framework IOKit -framework CoreFoundation
	$(CC) -arch x86_64 -Wall -Wextra -O2 -o $(BUILD)/amdvirt-long-test tools/amdvirt-long-test/main.c \
		-framework IOKit -framework CoreFoundation
	$(CC) -arch x86_64 -Wall -Wextra -Wno-nullability-completeness -O2 -o $(BUILD)/hvf-smoke tools/hvf-smoke/main.c \
		-framework Hypervisor
	$(CC) -arch x86_64 -Wall -Wextra -Wno-nullability-completeness -O2 -o $(BUILD)/hvf-ro-map-test tools/hvf-ro-map-test/main.c \
		-framework Hypervisor
	codesign -s - --force --entitlements tools/hvf-ro-map-test/hypervisor.entitlements $(BUILD)/hvf-ro-map-test
	# the kext, like AppleHV, lets only processes with the hypervisor entitlement create a VM
	codesign -s - --force --entitlements tools/hvf-ro-map-test/hypervisor.entitlements $(BUILD)/hvf-smoke
	$(CC) -arch x86_64 -Wall -Wextra -Wno-nullability-completeness -O2 -o $(BUILD)/hvf-race-test tools/hvf-race-test/main.c \
		-framework Hypervisor
	codesign -s - --force --entitlements tools/hvf-ro-map-test/hypervisor.entitlements $(BUILD)/hvf-race-test

kext:
	$(MAKE) -C kext

clean:
	rm -rf $(BUILD)
