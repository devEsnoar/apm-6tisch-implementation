CONTIKI_PROJECT = coap-client
all: $(CONTIKI_PROJECT)

CONTIKI=../../..

MAKE_MAC = MAKE_MAC_TSCH

include $(CONTIKI)/Makefile.dir-variables
include $(CONTIKI)/Makefile.identify-target

CFLAGS += -DWITH_PERIODIC_ROUTES_PRINT=1

MODULES += os/services/shell
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.include