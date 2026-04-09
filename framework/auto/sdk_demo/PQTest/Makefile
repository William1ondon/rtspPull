Target = pqd

include ../makefile_cfg

SDK_PATH = ../../sdk_lib

SRC = ./trans_info.c \
      ./pq_main.c \
      ./dexxx/de_generic.c \
      ./dexxx/de201_tans.c

CFLAGS+= -Wall -W -fPIC -std=c++11 -std=gnu++11 -lpthread -lm  -std=gnu99  -fpermissive -DRV_INUX -DPQ_DEBUG_LEVEL=2

INC = -I./ \
	-I$(SDK_PATH)/include \
	-I$(SDK_PATH)/include/disp2 \
	-I$(SDK_PATH)/include/utils \
	-I$(SDK_PATH)/include/cutils/ \
	-I$(SDK_PATH)/include/memory/

all: $(Target) 

$(Target) : 
	$(CPP) $(SRC) -o $(Target) $(CFLAGS) $(INC) 
	@echo "generate $(Target) success!!!"
	@cp -f $(Target) $(INSTATLL_PATH)
	@echo -e '\e[1;33m cp -f $(Target) $(INSTATLL_PATH) \e[0m'

.PHONY:clean cleanall install distclean

clean:
	-rm -f $(Target)

cleanall:clean
	-rm -f $(INSTATLL_PATH)/$(Target) 

distclean:cleanall 

install:
	-cp $(Target) $(INSTATLL_PATH)

distclean:cleanall 

install:
	-cp $(Target) $(INSTATLL_PATH)
