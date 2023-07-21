SHELL = /bin/zsh
PWD_DIR = "$(shell basename $$(pwd))"
BUILD_DIR = build
INSTALL_DIR = ~/ros2

# ==================================
# Pushes the current directory to remote host.
# ==================================

NUM_PROCS = 4
REMOTE_USER_HOST = "patrick@vm_comp4961_ubuntu1804"
REMOTE_DEST_DIR = "~/remote/$(shell hostname -s)/"

.PHONY: push-remote
push-remote:
	# Make the directory on the remote if it doesn't exist already.
	(ssh -t $(REMOTE_USER_HOST) "mkdir -p $(REMOTE_DEST_DIR)$(PWD_DIR)")
	# Sync our current directory with the remote.
	(rsync -a \
 			--delete \
 			--exclude "build" \
 			--exclude "install" \
 			--exclude "log" \
 			--exclude "build-remote" \
 			--exclude "cmake-build*" \
 			--exclude ".vscode" \
 			--exclude ".idea" \
 			--exclude ".git" \
 			--exclude ".gitignore" \
 			./ $(REMOTE_USER_HOST):$(REMOTE_DEST_DIR)$(PWD_DIR))

# ==================================
# Runs a Make command remotely.
# ==================================

.PHONY: remote
remote: push-remote
	ssh -t $(REMOTE_USER_HOST) "\
		cd $(REMOTE_DEST_DIR)$(PWD_DIR) ; \
		zsh -ilc 'make $(MAKE_CMD)' ; "

# ==================================
# Clean
# ==================================

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(INSTALL_DIR)/lib/librcutils.a
	rm -rf $(INSTALL_DIR)/include/rcutils

# ==================================
# Build
# ==================================

.PHONY: build
build:
	CC=aarch64-none-elf-gcc CXX=aarch64-none-elf-g++ \
		cmake -S . -B $(BUILD_DIR) \
		-DBUILD_TESTING=0 \
		-DCMAKE_C_COMPILER_WORKS=1 \
		-DCMAKE_CXX_COMPILER_WORKS=1 \
		-DRCUTILS_NO_FILESYSTEM=1 \
		-DRCUTILS_AVOID_DYNAMIC_ALLOCATION=1 \
		-DRCUTILS_SEL4CP=1 \
		-DRCUTILS_NO_ASSERT=1 \
		-DRCUTILS_NO_THREAD_SUPPORT=1 \
		-DRCUTILS_DISABLE_FAULT_INJECTION=1 \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
		-DCMAKE_MODULE_PATH=./
	cd $(BUILD_DIR) && $(MAKE) -j $(NUM_PROCS)
	# Install so that we can use the library in other projects.
	cd $(BUILD_DIR) && $(MAKE) install
