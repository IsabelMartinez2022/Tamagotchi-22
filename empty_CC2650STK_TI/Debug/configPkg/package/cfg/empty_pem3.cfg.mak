# invoke SourceDir generated makefile for empty.pem3
empty.pem3: .libraries,empty.pem3
.libraries,empty.pem3: package/cfg/empty_pem3.xdl
	$(MAKE) -f C:\ti\empty_CC2650STK_TI\Tamagotchi-22\empty_CC2650STK_TI/src/makefile.libs

clean::
	$(MAKE) -f C:\ti\empty_CC2650STK_TI\Tamagotchi-22\empty_CC2650STK_TI/src/makefile.libs clean

