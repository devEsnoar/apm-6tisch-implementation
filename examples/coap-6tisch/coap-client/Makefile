CONTIKI_PROJECT = node-client
all: $(CONTIKI_PROJECT)

CONTIKI=../../..

MAKE_MAC = MAKE_MAC_TSCH

include $(CONTIKI)/Makefile.dir-variables
include $(CONTIKI)/Makefile.identify-target

CFLAGS += -DWITH_PERIODIC_ROUTES_PRINT=1

MODULES += os/services/simple-energest
MODULES += os/services/shell
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap
MODULES += $(CONTIKI_NG_MAC_DIR)/tsch/int

include $(CONTIKI)/Makefile.include