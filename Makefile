.PHONY: default clean setup empires

default: genie game
default: setup empires
setup:
	cd setup && $(MAKE) && cd ..
empires: setup
	cd empires && $(MAKE) && cd ..
clean:
	cd setup && $(MAKE) clean && cd ..
	cd empires && $(MAKE) clean && cd ..
