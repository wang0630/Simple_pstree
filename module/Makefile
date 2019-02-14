MODULE_NAME := ksimple_pstree
obj-m := $(MODULE_NAME).o  # resulting file will be ksimple_pstree.ko
KERN_VER := $(shell uname -r)
KERN_DIR ?= /lib/modules/$(KERN_VER)/build
PWD := $(shell pwd)

default:
	# change directory to KERN_DIR
	# M= options causes the makefile to move back to the current dir 
	# and build the "modules"
	# which refers to the list of modules found in the obj-m variable
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules 
clean: 
	$(MAKE) -C $(KERN_DIR) M=$(PWD) clean
ins:
	sudo insmod $(MODULE_NAME).ko
rm:
	sudo rmmod $(MODULE_NAME)
