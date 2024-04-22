#include "int-engine.h"
#include "int-conf.h"

#include "lib/memb.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "net/mac/framer/framer-802154.h"
#include "net/mac/framer/frame802154e-ie.h"

#include "sys/log.h"
#define LOG_MODULE "INT"
#define LOG_LEVEL LOG_LEVEL_INT

/* Pre allocate space for telemetry entries */
MEMB(telemetry_entries_memb, struct int_telemetry, INT_MAX_TELEMETRY_ENTRIES);

/* Pre allocate space for telemetry contents */
MEMB(int_content_memb, struct int_content, 1);

static struct int_content * int_contents = NULL;

const int INT_SIZE_OVERHEAD = 7;

#define MAX_PAYLOAD_LEN_INT (127 - 2)


extern int is_source_appdata;

int 
int_engine_init(void) {
    memb_init(&telemetry_entries_memb);
    memb_init(&int_content_memb);

    return 0;
}

int 
remove_int_telemetry_entries(struct int_content * int_entry){
    struct int_telemetry * tm_entry;
    while((tm_entry = list_head(int_entry->int_telemetry_list))){
        if(int_entry != NULL && tm_entry != NULL) {
            list_remove(int_entry->int_telemetry_list, tm_entry);
            memb_free(&telemetry_entries_memb, tm_entry);
        }
        else {
            LOG_WARN("INT Engine: int_entry is NULL, nothing to clear ?\n");
            return -1;
        }
    }
    LOG_DBG("INT Engine: Removed INT telemetry entries of a INT content\n");
    return 0;
}

int 
remove_int_contents(void) {
    if(int_contents != NULL){
        remove_int_telemetry_entries(int_contents);
        if(!memb_free(&int_content_memb, int_contents)) {
            LOG_DBG("INT Engine: INT Contents memory freed sucessfully\n");
            int_contents = NULL;
        }
        else {
            LOG_WARN("INT Engine: Memory was not freed sucessfully\n");
            return -1;
        }
    }
    else {
        LOG_WARN("INT Engine: int_contents is NULL, nothing to clear ?\n");
        return -1;
    }
    return 0;
}

int
add_telemetry_entry(void) {

    struct int_telemetry * entry = memb_alloc(&telemetry_entries_memb);
    if(entry != NULL){
        create_telemetry_entry(&entry->telemetry_data, int_contents->int_current_header.int_bitmap);
        list_add(int_contents->int_telemetry_list, entry);
        LOG_DBG("INT Engine: Added telemetry entry to list successfully\n");
        return 0;
    }
    else {
        LOG_ERR("INT Engine: Allocation of telemetry_entry unsuccessful\n");
        return -1;
    }
}


int 
int_engine_input(const uint8_t * buf, uint16_t len){
    
    LOG_INFO("INT Engine: Input len = %d\n", len);
    struct int_content * temp_int_content = memb_alloc(&int_content_memb);
    if(temp_int_content != NULL) {
        int_contents = temp_int_content;
        LOG_DBG("INT Engine: Allocation for int_contents successful\n");
        LIST_STRUCT_INIT(int_contents, int_telemetry_list);
        LOG_DBG("INT Engine: Initialization of INT entries successful\n");

        int_contents->int_current_header.int_subtype = INT_SUBIE_ID;

        int_contents->int_current_header.int_control = *(buf++);
        int_contents->int_current_header.int_seqno = *(buf++);
        int_contents->int_current_header.int_bitmap = *(buf++);

        int size_tm_entries = (len - 3)  / TELEMETRY_MODEL_SIZE;
        LOG_DBG("INT Engine: INT entries present in frame: %d\n", size_tm_entries);

        for(int i = 0; i < size_tm_entries; i++) {
            struct int_telemetry * entry = memb_alloc(&telemetry_entries_memb);
            if(entry != NULL){
                LOG_DBG("INT Engine: Allocation of %d telemetry_entry successful\n", i);
                input_save_telemetry_data(buf, &entry->telemetry_data);
                LOG_DBG("INT Engine: Populated %d telemetry entry successfully\n", i);
                list_add(int_contents->int_telemetry_list, entry);
                LOG_INFO("INT Engine: Added %d telemetry entry to list successfully\n", i);
            }
            else {
                LOG_ERR("INT Engine: Allocation 1st of telemetry_entry unsuccessful\n");
                return -1;
            }
            buf += TELEMETRY_MODEL_SIZE;

        }

        

    }
    else {
        LOG_ERR("INT Engine: Allocation of int_contents unsuccessful, already allocated\n");
        return -1;
    }
    
    return 0;
}

int
int_engine_output(){
    LOG_INFO("INT Engine: Output\n");
    int hdr_size = INT_HEADER_SIZE;
    int new_entry_size = TELEMETRY_MODEL_SIZE;
    int total_packet_len = NETSTACK_FRAMER.length() + packetbuf_datalen();
    int required_size_initialize = total_packet_len + hdr_size + INT_SIZE_OVERHEAD;
    
    if(is_source_appdata) {
        LOG_DBG("INT Engine: This node generates app traffic, INT must be empty\n");
        remove_int_contents();
    }

    if(int_contents != NULL) {
        LOG_DBG("INT Engine: INT already present, read when received\n");
        int current_tm_entries_size = (TELEMETRY_MODEL_SIZE * list_length(int_contents->int_telemetry_list));
        LOG_INFO("INT Engine: pkt size (%d) + int_init (%d) + int_current (%d) + new_entry (%d) = %d\n", 
                            total_packet_len, 
                            hdr_size + INT_SIZE_OVERHEAD, 
                            current_tm_entries_size, 
                            new_entry_size, 
                            current_tm_entries_size + hdr_size + INT_SIZE_OVERHEAD + total_packet_len + new_entry_size);

        if(required_size_initialize >= MAX_PAYLOAD_LEN_INT) {
            LOG_WARN("INT Engine: Not enough space for intializing present INT, must be removed, max = %d, current_len = %d, needed = %d\n", MAX_PAYLOAD_LEN_INT, total_packet_len, required_size_initialize );
            
            return 0;
        }
        else {
            LOG_DBG("INT Engine: Enough space to keep intialization hdr\n");
            int required_size = current_tm_entries_size + required_size_initialize;
            if (required_size >= MAX_PAYLOAD_LEN_INT) {
                LOG_WARN("INT Engine: No current space for INT entries, must be removed but hdr stays\n");
                remove_int_telemetry_entries(int_contents);
                int_contents->int_current_header.int_control = INT_HDR_CONTROL_OVERFLOW_MASK & 0xFF;
                return 0;
            }
            else {
                if(int_contents->int_current_header.int_control & INT_HDR_CONTROL_OVERFLOW_MASK ||
                    required_size + new_entry_size >= MAX_PAYLOAD_LEN_INT) {
                    LOG_WARN("INT Engine: INT stays as it was received, no space for new entry: req %d + new %d >= max %d\n", required_size, new_entry_size, MAX_PAYLOAD_LEN_INT);
                    return 0;
                }
                else {
                    LOG_DBG("INT Engine: Add new entry based on present bitmap: req %d + new %d >= max %d\n", required_size, new_entry_size, MAX_PAYLOAD_LEN_INT);
                    return add_telemetry_entry();
                }

            }


        }
    }
    else {
        // INT was not initialized, the node can initialize the transmission

        // Can we at least initialize?
        if(required_size_initialize >= MAX_PAYLOAD_LEN_INT) {
            LOG_WARN("INT Engine: Not enough space for intializing INT, max = %d, current_len = %d, needed = %d\n", MAX_PAYLOAD_LEN_INT, total_packet_len, required_size_initialize );
            return 0;
        }
        else {
            struct int_content * temp_int_content = memb_alloc(&int_content_memb);
            if(temp_int_content != NULL) {
                int_contents = temp_int_content;
                LOG_DBG("INT Engine: Allocation for int_contents successful\n");
                LIST_STRUCT_INIT(int_contents, int_telemetry_list);
                LOG_DBG("INT Engine: Initialization of INT entries successful\n");
                int_contents->int_current_header.int_control = 0xA0;
                int_contents->int_current_header.int_bitmap = 0xFF;
                int_contents->int_current_header.int_subtype = INT_SUBIE_ID;
                int_contents->int_current_header.int_seqno = 0; // TODO: Increase automatically ?

                // Can we also add a new entry?
                int required_size_newentry = required_size_initialize + new_entry_size;
                LOG_INFO("INT Engine: pkt size (%d) + int_init (%d) + new_entry (%d) = %d\n", 
                                total_packet_len, 
                                hdr_size + INT_SIZE_OVERHEAD, 
                                new_entry_size,
                                total_packet_len + hdr_size + INT_SIZE_OVERHEAD + new_entry_size);

                if (required_size_newentry >= MAX_PAYLOAD_LEN_INT) {
                    LOG_WARN("INT Engine: Not enough space for adding INT entry, max = %d, current_len = %d, needed = %d\n", MAX_PAYLOAD_LEN_INT, total_packet_len, required_size_newentry);
                    // Set overflow bit
                    int_contents->int_current_header.int_control = INT_HDR_CONTROL_OVERFLOW_MASK & 0xFF;
                    return 0;
                }
                else {
                    LOG_DBG("INT Engine: Add new entry based on new bitmap\n");
                    return add_telemetry_entry();
                }

            }
            else {
                LOG_ERR("INT Engine: Allocation of int_contents unsuccessful, already allocated\n");
                return -1;
            }

        }

    }
    return 0;
}

int embed_int_in_frame() {
    int ret = 0;
    
    if(int_contents == NULL){
        LOG_WARN("INT Engine: No INT to embed. Check previous logs\n");
        ret = -1;
    }
    else {
        LOG_INFO("INT Engine: INT Contents present, embedding into frame\n");
        uint8_t *p;
        struct ieee802154_ies ies; 


        p = packetbuf_hdrptr();


        memset(&ies, 0, sizeof(ies));
        if(packetbuf_hdralloc(2) && frame80215e_create_ie_payload_list_termination(packetbuf_hdrptr(),	2, &ies) < 0) {
				LOG_ERR("INT Engine: embed_int_in_frame() fails because of Payload Termination 1 IE\n");
				ret = -1;
			}

        p = packetbuf_hdrptr();
        LOG_DBG("INT Engine: Payload Termination 1 has been added\n");

        int allocation_size = INT_HEADER_SIZE + (TELEMETRY_MODEL_SIZE * list_length(int_contents->int_telemetry_list));
        packetbuf_hdralloc(allocation_size);
        p = packetbuf_hdrptr();
        *(p++) = int_contents->int_current_header.int_subtype;
        *(p++) = int_contents->int_current_header.int_control;
        *(p++) = int_contents->int_current_header.int_seqno;
        *(p++) = int_contents->int_current_header.int_bitmap;

        struct int_telemetry * tm_entry;
        int i = 1;
        for(tm_entry = list_head(int_contents->int_telemetry_list); tm_entry != NULL; tm_entry = tm_entry->next){
            LOG_DBG("INT Engine: Adding INT Telemetry entries into frame, %d entry\n", i);
            i++;
            if(!memcpy(p, &tm_entry->telemetry_data, TELEMETRY_MODEL_SIZE)) {
                LOG_ERR("INT Engine: embed_int_in_frame() copying data into buffer\n");
                ret = -1;
                break;
            }
            p += TELEMETRY_MODEL_SIZE;
        }

        p = packetbuf_hdrptr();

        memset(&ies, 0, sizeof(ies));
        ies.int_ie_content_len = allocation_size;
        if(packetbuf_hdralloc(2) != 1 ||
        (frame80215e_create_ie_ietf_int(packetbuf_hdrptr(),
                                          2,
                                          &ies)) < 0) {
            LOG_ERR("INT Engine: embed_int_in_frame() fails because of Payload IE Header\n");
            ret = -1;
        }
        
        p = packetbuf_hdrptr();

        memset(&ies, 0, sizeof(ies));
		if(packetbuf_hdralloc(2) &&
				frame80215e_create_ie_header_list_termination_1(packetbuf_hdrptr(),
																												2,
																												&ies) < 0) {
				LOG_ERR("INT: int_output() fails because of Header Termination 1 IE\n");
				ret = -1;
			}
         LOG_DBG("INT: Payload Header 1 has been added\n");


        p = packetbuf_hdrptr();

		if (!ret) packetbuf_set_attr(PACKETBUF_ATTR_MAC_METADATA, 1);

        remove_int_contents();
        
    }
    return ret;
}
