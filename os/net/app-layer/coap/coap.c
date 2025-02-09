/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      An implementation of the Constrained Application Protocol (RFC).
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 *
 *      Joakim Eriksson, joakim.eriksson@ri.se
 *      Niclas Finne, niclas.finne@ri.se
 */

/**
 * \addtogroup coap
 * @{
 */


#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sys/cc.h"
#include "sys/node-id.h"
#include "lib/random.h"

#include "coap.h"
#include "coap-transactions.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap"
#define LOG_LEVEL  LOG_LEVEL_COAP

/*---------------------------------------------------------------------------*/
/*- Variables ---------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static uint16_t current_mid = 0;

coap_status_t coap_status_code = NO_ERROR;
const char *coap_error_message = "";
/*---------------------------------------------------------------------------*/
/*- Local helper functions --------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static uint16_t
coap_log_2(uint16_t value)
{
  uint16_t result = 0;

  do {
    value = value >> 1;
    result++;
  } while(value);

  return result ? result - 1 : result;
}
/*---------------------------------------------------------------------------*/
static uint32_t
coap_parse_int_option(const uint8_t *bytes, size_t length)
{
  uint32_t var = 0;
  int i = 0;

  while(i < length) {
    var <<= 8;
    var |= bytes[i++];
  }
  return var;
}
/*---------------------------------------------------------------------------*/
static uint8_t
coap_option_nibble(unsigned int value)
{
  if(value < 13) {
    return value;
  } else if(value <= 0xFF + 13) {
    return 13;
  } else {
    return 14;
  }
}
/*---------------------------------------------------------------------------*/
static size_t
coap_set_option_header(unsigned int delta, size_t length, uint8_t *buffer)
{
  size_t written = 0;

  buffer[0] = coap_option_nibble(delta) << 4 | coap_option_nibble(length);

  if(delta > 268) {
    buffer[++written] = ((delta - 269) >> 8) & 0xff;
    buffer[++written] = (delta - 269) & 0xff;
  } else if(delta > 12) {
    buffer[++written] = (delta - 13);
  }

  if(length > 268) {
    buffer[++written] = ((length - 269) >> 8) & 0xff;
    buffer[++written] = (length - 269) & 0xff;
  } else if(length > 12) {
    buffer[++written] = (length - 13);
  }

  LOG_DBG("WRITTEN %zu B opt header\n", 1 + written);

  return ++written;
}
/*---------------------------------------------------------------------------*/
static size_t
coap_serialize_int_option(unsigned int number, unsigned int current_number,
                          uint8_t *buffer, uint32_t value)
{
  size_t i = 0;

  if(0xFF000000 & value) {
    ++i;
  }
  if(0xFFFF0000 & value) {
    ++i;
  }
  if(0xFFFFFF00 & value) {
    ++i;
  }
  if(0xFFFFFFFF & value) {
    ++i;
  }
  LOG_DBG("OPTION %u (delta %u, len %zu)\n", number, number - current_number,
          i);

  i = coap_set_option_header(number - current_number, i, buffer);

  if(0xFF000000 & value) {
    buffer[i++] = (uint8_t)(value >> 24);
  }
  if(0xFFFF0000 & value) {
    buffer[i++] = (uint8_t)(value >> 16);
  }
  if(0xFFFFFF00 & value) {
    buffer[i++] = (uint8_t)(value >> 8);
  }
  if(0xFFFFFFFF & value) {
    buffer[i++] = (uint8_t)(value);
  }
  return i;
}
/*---------------------------------------------------------------------------*/
static size_t
coap_serialize_array_option(unsigned int number, unsigned int current_number,
                            uint8_t *buffer, uint8_t *array, size_t length,
                            char split_char)
{
  size_t i = 0;

  LOG_DBG("ARRAY type %u, len %zu, full [", number, length);
  LOG_DBG_COAP_STRING((const char *)array, length);
  LOG_DBG_("]\n");

  if(split_char != '\0') {
    int j;
    uint8_t *part_start = array;
    uint8_t *part_end = NULL;
    size_t temp_length;

    for(j = 0; j <= length + 1; ++j) {
      LOG_DBG("STEP %u/%zu (%c)\n", j, length, array[j]);
      if(array[j] == split_char || j == length) {
        part_end = array + j;
        temp_length = part_end - part_start;

        i += coap_set_option_header(number - current_number, temp_length,
                                    &buffer[i]);
        memcpy(&buffer[i], part_start, temp_length);
        i += temp_length;

        LOG_DBG("OPTION type %u, delta %u, len %zu, part [", number,
                number - current_number, i);
        LOG_DBG_COAP_STRING((const char *)part_start, temp_length);
        LOG_DBG_("]\n");
        ++j;                    /* skip the splitter */
        current_number = number;
        part_start = array + j;
      }
    }                           /* for */
  } else {
    i += coap_set_option_header(number - current_number, length, &buffer[i]);
    memcpy(&buffer[i], array, length);
    i += length;

    LOG_DBG("OPTION type %u, delta %u, len %zu\n", number,
            number - current_number, length);
  }

  return i;
}
/*---------------------------------------------------------------------------*/
static void
coap_merge_multi_option(char **dst, size_t *dst_len, uint8_t *option,
                        size_t option_len, char separator)
{
  /* merge multiple options */
  if(*dst_len > 0) {
    /* dst already contains an option: concatenate */
    (*dst)[*dst_len] = separator;
    *dst_len += 1;

    /* memmove handles 2-byte option headers */
    memmove((*dst) + (*dst_len), option, option_len);

    *dst_len += option_len;
  } else {
    /* dst is empty: set to option */
    *dst = (char *)option;
    *dst_len = option_len;
  }
}
/*---------------------------------------------------------------------------*/
static int
coap_get_variable(const char *buffer, size_t length, const char *name,
                  const char **output)
{
  const char *start = NULL;
  const char *end = NULL;
  const char *value_end = NULL;
  size_t name_len = 0;

  /*initialize the output buffer first */
  *output = 0;

  name_len = strlen(name);
  end = buffer + length;

  for(start = buffer; start + name_len < end; ++start) {
    if((start == buffer || start[-1] == '&') && start[name_len] == '='
       && strncmp(name, start, name_len) == 0) {

      /* Point start to variable value */
      start += name_len + 1;

      /* Point end to the end of the value */
      value_end = (const char *)memchr(start, '&', end - start);
      if(value_end == NULL) {
        value_end = end;
      }
      *output = start;

      return value_end - start;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_init_connection(void)
{
  /* initialize transaction ID */
  current_mid = random_rand();
}
/*---------------------------------------------------------------------------*/
uint16_t
coap_get_mid()
{
  return ++current_mid;
}
/*---------------------------------------------------------------------------*/
void
coap_init_message(coap_message_t *coap_pkt, coap_message_type_t type,
                  uint8_t code, uint16_t mid)
{
  /* Important thing */
  memset(coap_pkt, 0, sizeof(coap_message_t));

  coap_pkt->type = type;
  coap_pkt->code = code;
  coap_pkt->mid = mid;
}
/*---------------------------------------------------------------------------*/
size_t
coap_serialize_message(coap_message_t *coap_pkt, uint8_t *buffer)
{
  uint8_t *option;
  unsigned int current_number = 0;

  /* Initialize */
  coap_pkt->buffer = buffer;
  coap_pkt->version = 1;

  LOG_DBG("-Serializing MID %u to %p, ", coap_pkt->mid, coap_pkt->buffer);

  /* set header fields */
  coap_pkt->buffer[0] = 0x00;
  coap_pkt->buffer[0] |= COAP_HEADER_VERSION_MASK
    & (coap_pkt->version) << COAP_HEADER_VERSION_POSITION;
  coap_pkt->buffer[0] |= COAP_HEADER_TYPE_MASK
    & (coap_pkt->type) << COAP_HEADER_TYPE_POSITION;
  coap_pkt->buffer[0] |= COAP_HEADER_TOKEN_LEN_MASK
    & (coap_pkt->token_len) << COAP_HEADER_TOKEN_LEN_POSITION;
  coap_pkt->buffer[1] = coap_pkt->code;
  coap_pkt->buffer[2] = (uint8_t)((coap_pkt->mid) >> 8);
  coap_pkt->buffer[3] = (uint8_t)(coap_pkt->mid);

  /* empty message, dont need to do more stuff */
  if(!coap_pkt->code) {
    LOG_DBG_("-Done serializing empty message at %p-\n", coap_pkt->buffer);
    return 4;
  }

  /* set Token */
  LOG_DBG_("Token (len %u)", coap_pkt->token_len);
  option = coap_pkt->buffer + COAP_HEADER_LEN;
  for(current_number = 0; current_number < coap_pkt->token_len;
      ++current_number) {
    LOG_DBG_(" %02X", coap_pkt->token[current_number]);
    *option = coap_pkt->token[current_number];
    ++option;
  }
  LOG_DBG_("-\n");

  /* Serialize options */
  current_number = 0;

  LOG_DBG("-Serializing options at %p-\n", option);

  /* The options must be serialized in the order of their number */
  COAP_SERIALIZE_BYTE_OPTION(COAP_OPTION_IF_MATCH, if_match, "If-Match");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_URI_HOST, uri_host, '\0',
                               "Uri-Host");
  COAP_SERIALIZE_BYTE_OPTION(COAP_OPTION_ETAG, etag, "ETag");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_IF_NONE_MATCH,
                            content_format -
                            coap_pkt->
                            content_format /* hack to get a zero field */,
                            "If-None-Match");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_OBSERVE, observe, "Observe");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_URI_PORT, uri_port, "Uri-Port");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_LOCATION_PATH, location_path, '/',
                               "Location-Path");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_URI_PATH, uri_path, '/',
                               "Uri-Path");
  LOG_DBG("Serialize content format: %d\n", coap_pkt->content_format);
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_CONTENT_FORMAT, content_format,
                            "Content-Format");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_MAX_AGE, max_age, "Max-Age");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_URI_QUERY, uri_query, '&',
                               "Uri-Query");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_ACCEPT, accept, "Accept");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_LOCATION_QUERY, location_query,
                               '&', "Location-Query");
  COAP_SERIALIZE_BLOCK_OPTION(COAP_OPTION_BLOCK2, block2, "Block2");
  COAP_SERIALIZE_BLOCK_OPTION(COAP_OPTION_BLOCK1, block1, "Block1");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_SIZE2, size2, "Size2");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_PROXY_URI, proxy_uri, '\0',
                               "Proxy-Uri");
  COAP_SERIALIZE_STRING_OPTION(COAP_OPTION_PROXY_SCHEME, proxy_scheme, '\0',
                               "Proxy-Scheme");
  COAP_SERIALIZE_INT_OPTION(COAP_OPTION_SIZE1, size1, "Size1");

  LOG_DBG("-Done serializing at %p----\n", option);

  /* Pack payload */
  if((option - coap_pkt->buffer) <= COAP_MAX_HEADER_SIZE) {
    
    #if COAP_WITH_PIGGYBACKING
    if(coap_pkt->type != COAP_TYPE_ACK && coap_pkt->type != COAP_TYPE_RST) {
      if(coap_pkt->payload_len + COAP_TELEMETRY_SIZE + 2 + 11 <= COAP_MAX_CHUNK_SIZE) { // + 11  to compensate for 6LoWPAN compressions and decompressions
        LOG_DBG("-Telemetry data fits\n");
        *option = 0xFA;
        ++option;
        *option = COAP_TELEMETRY_SIZE;
        ++option;
        for(int i = 0; i < COAP_TELEMETRY_SIZE; i++){
          *option = (uint8_t) node_id;
          ++option;
        }
      }
    }
    #endif
    /* Payload marker */
    if(coap_pkt->payload_len) {
      *option = 0xFF;
      ++option;
    }
    memmove(option, coap_pkt->payload, coap_pkt->payload_len);
  } else {
    /* an error occurred: caller must check for !=0 */
    coap_pkt->buffer = NULL;
    coap_error_message = "Serialized header exceeds COAP_MAX_HEADER_SIZE";
    return 0;
  }

  LOG_DBG("-Done %u B (header len %u, payload len %u)-\n",
          (unsigned int)(coap_pkt->payload_len + option - buffer),
          (unsigned int)(option - buffer),
          (unsigned int)coap_pkt->payload_len);

  LOG_DBG("Dump [0x%02X %02X %02X %02X  %02X %02X %02X %02X]\n",
          coap_pkt->buffer[0], coap_pkt->buffer[1], coap_pkt->buffer[2],
          coap_pkt->buffer[3], coap_pkt->buffer[4], coap_pkt->buffer[5],
          coap_pkt->buffer[6], coap_pkt->buffer[7]);

  return (option - buffer) + coap_pkt->payload_len; /* message length */
}
/*---------------------------------------------------------------------------*/
coap_status_t
coap_parse_message(coap_message_t *coap_pkt, uint8_t *data, uint16_t data_len)
{
  if(data_len < COAP_HEADER_LEN) {
    /* Too short - malformed CoAP message */
    LOG_WARN("BAD REQUEST: message too short\n");
    return BAD_REQUEST_4_00;
  }

  /* initialize message */
  memset(coap_pkt, 0, sizeof(coap_message_t));

  /* pointer to message bytes */
  coap_pkt->buffer = data;

  /* parse header fields */
  coap_pkt->version = (COAP_HEADER_VERSION_MASK & coap_pkt->buffer[0])
    >> COAP_HEADER_VERSION_POSITION;
  coap_pkt->type = (COAP_HEADER_TYPE_MASK & coap_pkt->buffer[0])
    >> COAP_HEADER_TYPE_POSITION;
  coap_pkt->token_len = (COAP_HEADER_TOKEN_LEN_MASK & coap_pkt->buffer[0])
    >> COAP_HEADER_TOKEN_LEN_POSITION;
  coap_pkt->code = coap_pkt->buffer[1];
  coap_pkt->mid = coap_pkt->buffer[2] << 8 | coap_pkt->buffer[3];

  if(coap_pkt->version != 1) {
    coap_error_message = "CoAP version must be 1";
    return BAD_REQUEST_4_00;
  }

  if(coap_pkt->token_len > COAP_TOKEN_LEN) {
    coap_error_message = "Token Length must not be more than 8";
    return BAD_REQUEST_4_00;
  }

  uint8_t *current_option = data + COAP_HEADER_LEN;
  if(current_option + coap_pkt->token_len > data + data_len) {
    /* Malformed CoAP message - token length out od message bounds */
    LOG_WARN("BAD REQUEST: token outside message buffer");
    return BAD_REQUEST_4_00;
  }

  memcpy(coap_pkt->token, current_option, coap_pkt->token_len);
  LOG_DBG("Token (len %u) [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
          coap_pkt->token_len, coap_pkt->token[0], coap_pkt->token[1],
          coap_pkt->token[2], coap_pkt->token[3], coap_pkt->token[4],
          coap_pkt->token[5], coap_pkt->token[6], coap_pkt->token[7]
          );                     /* FIXME always prints 8 bytes */

  /* parse options */
  memset(coap_pkt->options, 0, sizeof(coap_pkt->options));
  current_option += coap_pkt->token_len;

  unsigned int option_number = 0;
  unsigned int option_delta = 0;
  size_t option_length = 0;

  while(current_option < data + data_len) {
    /* payload marker 0xFF, currently only checking for 0xF* because rest is reserved */
    if((current_option[0] & 0xF0) == 0xF0) {
    #if COAP_WITH_PIGGYBACKING
      if(current_option[0] == 0xFA){
          uint8_t telemetry_size = current_option[1];
          LOG_WARN("EXPERIMENT: Consumed %d Bytes of telemetry ", telemetry_size);
          for(int i = 0; i < telemetry_size; i++){
            LOG_WARN_("%d",  current_option[2 + i]);
          }
          LOG_WARN_(" \n");
          current_option = current_option + 2 + telemetry_size;
      }
    #endif
    
      coap_pkt->payload = ++current_option;
      coap_pkt->payload_len = data_len - (coap_pkt->payload - data);

      /* also for receiving, the Erbium upper bound is COAP_MAX_CHUNK_SIZE */
      if(coap_pkt->payload_len > COAP_MAX_CHUNK_SIZE) {
        coap_pkt->payload_len = COAP_MAX_CHUNK_SIZE;
        /* null-terminate payload */
      }
      coap_pkt->payload[coap_pkt->payload_len] = '\0';

      break;
    }

    option_delta = current_option[0] >> 4;
    option_length = current_option[0] & 0x0F;
    ++current_option;
    if(current_option >= data + data_len) {
      /* Malformed CoAP - out of bounds */
      LOG_WARN("BAD REQUEST: option delta outside message buffer\n");
      return BAD_REQUEST_4_00;
    }

    if(option_delta == 13) {
      option_delta += current_option[0];
      ++current_option;
    } else if(option_delta == 14) {
      option_delta += 255;
      option_delta += current_option[0] << 8;
      ++current_option;
      if(current_option >= data + data_len) {
        /* Malformed CoAP - out of bounds */
        LOG_WARN("BAD REQUEST: option delta outside message buffer\n");
        return BAD_REQUEST_4_00;
      }
      option_delta += current_option[0];
      ++current_option;
    }

    if(current_option >= data + data_len) {
      /* Malformed CoAP - out of bounds */
      LOG_WARN("BAD REQUEST: option delta outside message buffer\n");
      return BAD_REQUEST_4_00;
    }

    if(option_length == 13) {
      option_length += current_option[0];
      ++current_option;
    } else if(option_length == 14) {
      option_length += 255;
      option_length += current_option[0] << 8;
      ++current_option;
      if(current_option >= data + data_len) {
        /* Malformed CoAP - out of bounds */
        LOG_WARN("BAD REQUEST: option length outside message buffer\n");
        return BAD_REQUEST_4_00;
      }
      option_length += current_option[0];
      ++current_option;
    }

    if(current_option + option_length > data + data_len) {
      /* Malformed CoAP - out of bounds */
      LOG_WARN("BAD REQUEST: options outside data message: %u > %u\n",
               (unsigned)(current_option + option_length - data), data_len);
      return BAD_REQUEST_4_00;
    }

    option_number += option_delta;

    if(option_number > COAP_OPTION_SIZE1) {
      /* Malformed CoAP - out of bounds */
      LOG_WARN("BAD REQUEST: option number too large: %u\n", option_number);
      return BAD_REQUEST_4_00;
    }

    LOG_DBG("OPTION %u (delta %u, len %zu): ", option_number, option_delta,
            option_length);

    coap_set_option(coap_pkt, option_number);

    switch(option_number) {
    case COAP_OPTION_CONTENT_FORMAT:
      coap_pkt->content_format = coap_parse_int_option(current_option,
                                                       option_length);
      LOG_DBG_("Content-Format [%u]\n", coap_pkt->content_format);
      break;
    case COAP_OPTION_MAX_AGE:
      coap_pkt->max_age = coap_parse_int_option(current_option,
                                                option_length);
      LOG_DBG_("Max-Age [%"PRIu32"]\n", coap_pkt->max_age);
      break;
    case COAP_OPTION_ETAG:
      coap_pkt->etag_len = MIN(COAP_ETAG_LEN, option_length);
      memcpy(coap_pkt->etag, current_option, coap_pkt->etag_len);
      LOG_DBG_("ETag %u [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
               coap_pkt->etag_len, coap_pkt->etag[0], coap_pkt->etag[1],
               coap_pkt->etag[2], coap_pkt->etag[3], coap_pkt->etag[4],
               coap_pkt->etag[5], coap_pkt->etag[6], coap_pkt->etag[7]
               );                 /*FIXME always prints 8 bytes */
      break;
    case COAP_OPTION_ACCEPT:
      coap_pkt->accept = coap_parse_int_option(current_option, option_length);
      LOG_DBG_("Accept [%u]\n", coap_pkt->accept);
      break;
    case COAP_OPTION_IF_MATCH:
      /* TODO support multiple ETags */
      coap_pkt->if_match_len = MIN(COAP_ETAG_LEN, option_length);
      memcpy(coap_pkt->if_match, current_option, coap_pkt->if_match_len);
      LOG_DBG_("If-Match %u [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
               coap_pkt->if_match_len, coap_pkt->if_match[0],
               coap_pkt->if_match[1], coap_pkt->if_match[2],
               coap_pkt->if_match[3], coap_pkt->if_match[4],
               coap_pkt->if_match[5], coap_pkt->if_match[6],
               coap_pkt->if_match[7]
               ); /* FIXME always prints 8 bytes */
      break;
    case COAP_OPTION_IF_NONE_MATCH:
      coap_pkt->if_none_match = 1;
      LOG_DBG_("If-None-Match\n");
      break;

    case COAP_OPTION_PROXY_URI:
#if COAP_PROXY_OPTION_PROCESSING
      coap_pkt->proxy_uri = (char *)current_option;
      coap_pkt->proxy_uri_len = option_length;
      LOG_DBG_("Proxy-Uri NOT IMPLEMENTED [");
      LOG_DBG_COAP_STRING(coap_pkt->proxy_uri, coap_pkt->proxy_uri_len);
      LOG_DBG_("]\n");
#endif /* COAP_PROXY_OPTION_PROCESSING */

      coap_error_message = "This is a constrained server (Contiki)";
      return PROXYING_NOT_SUPPORTED_5_05;
      break;
    case COAP_OPTION_PROXY_SCHEME:
#if COAP_PROXY_OPTION_PROCESSING
      coap_pkt->proxy_scheme = (char *)current_option;
      coap_pkt->proxy_scheme_len = option_length;
      LOG_DBG_("Proxy-Scheme NOT IMPLEMENTED [");
      LOG_DBG_COAP_STRING(coap_pkt->proxy_scheme, coap_pkt->proxy_scheme_len);
      LOG_DBG_("]\n");
#endif
      coap_error_message = "This is a constrained server (Contiki)";
      return PROXYING_NOT_SUPPORTED_5_05;
      break;

    case COAP_OPTION_URI_HOST:
      coap_pkt->uri_host = (char *)current_option;
      coap_pkt->uri_host_len = option_length;
      LOG_DBG_("Uri-Host [");
      LOG_DBG_COAP_STRING(coap_pkt->uri_host, coap_pkt->uri_host_len);
      LOG_DBG_("]\n");
      break;
    case COAP_OPTION_URI_PORT:
      coap_pkt->uri_port = coap_parse_int_option(current_option,
                                                 option_length);
      LOG_DBG_("Uri-Port [%u]\n", coap_pkt->uri_port);
      break;
    case COAP_OPTION_URI_PATH:
      /* coap_merge_multi_option() operates in-place on the IPBUF, but final message field should be const string -> cast to string */
      coap_merge_multi_option((char **)&(coap_pkt->uri_path),
                              &(coap_pkt->uri_path_len), current_option,
                              option_length, '/');
      LOG_DBG_("Uri-Path [");
      LOG_DBG_COAP_STRING(coap_pkt->uri_path, coap_pkt->uri_path_len);
      LOG_DBG_("]\n");
      break;
    case COAP_OPTION_URI_QUERY:
      /* coap_merge_multi_option() operates in-place on the IPBUF, but final message field should be const string -> cast to string */
      coap_merge_multi_option((char **)&(coap_pkt->uri_query),
                              &(coap_pkt->uri_query_len), current_option,
                              option_length, '&');
      LOG_DBG_("Uri-Query[");
      LOG_DBG_COAP_STRING(coap_pkt->uri_query, coap_pkt->uri_query_len);
      LOG_DBG_("]\n");
      break;

    case COAP_OPTION_LOCATION_PATH:
      /* coap_merge_multi_option() operates in-place on the IPBUF, but final message field should be const string -> cast to string */
      coap_merge_multi_option((char **)&(coap_pkt->location_path),
                              &(coap_pkt->location_path_len), current_option,
                              option_length, '/');

      LOG_DBG_("Location-Path [");
      LOG_DBG_COAP_STRING(coap_pkt->location_path, coap_pkt->location_path_len);
      LOG_DBG_("]\n");
      break;
    case COAP_OPTION_LOCATION_QUERY:
      /* coap_merge_multi_option() operates in-place on the IPBUF, but final message field should be const string -> cast to string */
      coap_merge_multi_option((char **)&(coap_pkt->location_query),
                              &(coap_pkt->location_query_len), current_option,
                              option_length, '&');
      LOG_DBG_("Location-Query [");
      LOG_DBG_COAP_STRING(coap_pkt->location_query, coap_pkt->location_query_len);
      LOG_DBG_("]\n");
      break;

    case COAP_OPTION_OBSERVE:
      coap_pkt->observe = coap_parse_int_option(current_option,
                                                option_length);
      LOG_DBG_("Observe [%"PRId32"]\n", coap_pkt->observe);
      break;
    case COAP_OPTION_BLOCK2:
      coap_pkt->block2_num = coap_parse_int_option(current_option,
                                                   option_length);
      coap_pkt->block2_more = (coap_pkt->block2_num & 0x08) >> 3;
      coap_pkt->block2_size = 16 << (coap_pkt->block2_num & 0x07);
      coap_pkt->block2_offset = (coap_pkt->block2_num & ~0x0000000F)
        << (coap_pkt->block2_num & 0x07);
      coap_pkt->block2_num >>= 4;
      LOG_DBG_("Block2 [%lu%s (%u B/blk)]\n",
               (unsigned long)coap_pkt->block2_num,
               coap_pkt->block2_more ? "+" : "", coap_pkt->block2_size);
      break;
    case COAP_OPTION_BLOCK1:
      coap_pkt->block1_num = coap_parse_int_option(current_option,
                                                   option_length);
      coap_pkt->block1_more = (coap_pkt->block1_num & 0x08) >> 3;
      coap_pkt->block1_size = 16 << (coap_pkt->block1_num & 0x07);
      coap_pkt->block1_offset = (coap_pkt->block1_num & ~0x0000000F)
        << (coap_pkt->block1_num & 0x07);
      coap_pkt->block1_num >>= 4;
      LOG_DBG_("Block1 [%lu%s (%u B/blk)]\n",
               (unsigned long)coap_pkt->block1_num,
               coap_pkt->block1_more ? "+" : "", coap_pkt->block1_size);
      break;
    case COAP_OPTION_SIZE2:
      coap_pkt->size2 = coap_parse_int_option(current_option, option_length);
      LOG_DBG_("Size2 [%"PRIu32"]\n", coap_pkt->size2);
      break;
    case COAP_OPTION_SIZE1:
      coap_pkt->size1 = coap_parse_int_option(current_option, option_length);
      LOG_DBG_("Size1 [%"PRIu32"]\n", coap_pkt->size1);
      break;
    default:
      LOG_DBG_("unknown (%u)\n", option_number);
      /* check if critical (odd) */
      if(option_number & 1) {
        coap_error_message = "Unsupported critical option";
        return BAD_OPTION_4_02;
      }
    }

    current_option += option_length;
  }                             /* for */
  LOG_DBG("-Done parsing-------\n");

  return NO_ERROR;
}
/*---------------------------------------------------------------------------*/
/*- CoAP Engine API ---------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int
coap_get_query_variable(coap_message_t *coap_pkt,
                        const char *name, const char **output)
{
  if(coap_is_option(coap_pkt, COAP_OPTION_URI_QUERY)) {
    return coap_get_variable(coap_pkt->uri_query, coap_pkt->uri_query_len,
                             name, output);
  }
  return 0;
}
int
coap_get_post_variable(coap_message_t *coap_pkt,
                       const char *name, const char **output)
{
  if(coap_pkt->payload_len) {
    return coap_get_variable((const char *)coap_pkt->payload,
                             coap_pkt->payload_len, name, output);
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
int
coap_set_status_code(coap_message_t *message, unsigned int code)
{
  if(code <= 0xFF) {
    message->code = (uint8_t)code;
    return 1;
  } else {
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
int
coap_set_token(coap_message_t *coap_pkt, const uint8_t *token, size_t token_len)
{
  coap_pkt->token_len = MIN(COAP_TOKEN_LEN, token_len);
  memcpy(coap_pkt->token, token, coap_pkt->token_len);

  return coap_pkt->token_len;
}
/*---------------------------------------------------------------------------*/
/*- CoAP Implementation API -------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int
coap_get_header_content_format(coap_message_t *coap_pkt, unsigned int *format)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_CONTENT_FORMAT)) {
    return 0;
  }
  *format = coap_pkt->content_format;
  return 1;
}
int
coap_set_header_content_format(coap_message_t *coap_pkt, unsigned int format)
{
  coap_pkt->content_format = format;
  coap_set_option(coap_pkt, COAP_OPTION_CONTENT_FORMAT);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_accept(coap_message_t *coap_pkt, unsigned int *accept)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_ACCEPT)) {
    return 0;
  }
  *accept = coap_pkt->accept;
  return 1;
}
int
coap_set_header_accept(coap_message_t *coap_pkt, unsigned int accept)
{
  coap_pkt->accept = accept;
  coap_set_option(coap_pkt, COAP_OPTION_ACCEPT);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_max_age(coap_message_t *coap_pkt, uint32_t *age)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_MAX_AGE)) {
    *age = COAP_DEFAULT_MAX_AGE;
  } else {
    *age = coap_pkt->max_age;
  } return 1;
}
int
coap_set_header_max_age(coap_message_t *coap_pkt, uint32_t age)
{
  coap_pkt->max_age = age;
  coap_set_option(coap_pkt, COAP_OPTION_MAX_AGE);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_etag(coap_message_t *coap_pkt, const uint8_t **etag)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_ETAG)) {
    return 0;
  }
  *etag = coap_pkt->etag;
  return coap_pkt->etag_len;
}
int
coap_set_header_etag(coap_message_t *coap_pkt, const uint8_t *etag, size_t etag_len)
{
  coap_pkt->etag_len = MIN(COAP_ETAG_LEN, etag_len);
  memcpy(coap_pkt->etag, etag, coap_pkt->etag_len);

  coap_set_option(coap_pkt, COAP_OPTION_ETAG);
  return coap_pkt->etag_len;
}
/*---------------------------------------------------------------------------*/
/*FIXME support multiple ETags */
int
coap_get_header_if_match(coap_message_t *coap_pkt, const uint8_t **etag)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_IF_MATCH)) {
    return 0;
  }
  *etag = coap_pkt->if_match;
  return coap_pkt->if_match_len;
}
int
coap_set_header_if_match(coap_message_t *coap_pkt, const uint8_t *etag, size_t etag_len)
{
  coap_pkt->if_match_len = MIN(COAP_ETAG_LEN, etag_len);
  memcpy(coap_pkt->if_match, etag, coap_pkt->if_match_len);

  coap_set_option(coap_pkt, COAP_OPTION_IF_MATCH);
  return coap_pkt->if_match_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_if_none_match(coap_message_t *message)
{
  return coap_is_option(message, COAP_OPTION_IF_NONE_MATCH) ? 1 : 0;
}
int
coap_set_header_if_none_match(coap_message_t *message)
{
  coap_set_option(message, COAP_OPTION_IF_NONE_MATCH);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_proxy_uri(coap_message_t *coap_pkt, const char **uri)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_PROXY_URI)) {
    return 0;
  }
  *uri = coap_pkt->proxy_uri;
  return coap_pkt->proxy_uri_len;
}
int
coap_set_header_proxy_uri(coap_message_t *coap_pkt, const char *uri)
{
  /* TODO Provide alternative that sets Proxy-Scheme and Uri-* options and provide coap-conf define */

  coap_pkt->proxy_uri = uri;
  coap_pkt->proxy_uri_len = strlen(uri);

  coap_set_option(coap_pkt, COAP_OPTION_PROXY_URI);
  return coap_pkt->proxy_uri_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_host(coap_message_t *coap_pkt, const char **host)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_URI_HOST)) {
    return 0;
  }
  *host = coap_pkt->uri_host;
  return coap_pkt->uri_host_len;
}
int
coap_set_header_uri_host(coap_message_t *coap_pkt, const char *host)
{
  coap_pkt->uri_host = host;
  coap_pkt->uri_host_len = strlen(host);

  coap_set_option(coap_pkt, COAP_OPTION_URI_HOST);
  return coap_pkt->uri_host_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_path(coap_message_t *coap_pkt, const char **path)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_URI_PATH)) {
    return 0;
  }
  *path = coap_pkt->uri_path;
  return coap_pkt->uri_path_len;
}
int
coap_set_header_uri_path(coap_message_t *coap_pkt, const char *path)
{
  while(path[0] == '/') {
    ++path;
  }

  coap_pkt->uri_path = path;
  coap_pkt->uri_path_len = strlen(path);

  coap_set_option(coap_pkt, COAP_OPTION_URI_PATH);
  return coap_pkt->uri_path_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_query(coap_message_t *coap_pkt, const char **query)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_URI_QUERY)) {
    return 0;
  }
  *query = coap_pkt->uri_query;
  return coap_pkt->uri_query_len;
}
int
coap_set_header_uri_query(coap_message_t *coap_pkt, const char *query)
{
  while(query[0] == '?') {
    ++query;
  }

  coap_pkt->uri_query = query;
  coap_pkt->uri_query_len = strlen(query);

  coap_set_option(coap_pkt, COAP_OPTION_URI_QUERY);
  return coap_pkt->uri_query_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_location_path(coap_message_t *coap_pkt, const char **path)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_LOCATION_PATH)) {
    return 0;
  }
  *path = coap_pkt->location_path;
  return coap_pkt->location_path_len;
}
int
coap_set_header_location_path(coap_message_t *coap_pkt, const char *path)
{
  char *query;

  while(path[0] == '/') {
    ++path;
  }

  if((query = strchr(path, '?'))) {
    coap_set_header_location_query(coap_pkt, query + 1);
    coap_pkt->location_path_len = query - path;
  } else {
    coap_pkt->location_path_len = strlen(path);
  } coap_pkt->location_path = path;

  if(coap_pkt->location_path_len > 0) {
    coap_set_option(coap_pkt, COAP_OPTION_LOCATION_PATH);
  }
  return coap_pkt->location_path_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_location_query(coap_message_t *coap_pkt, const char **query)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_LOCATION_QUERY)) {
    return 0;
  }
  *query = coap_pkt->location_query;
  return coap_pkt->location_query_len;
}
int
coap_set_header_location_query(coap_message_t *coap_pkt, const char *query)
{
  while(query[0] == '?') {
    ++query;
  }

  coap_pkt->location_query = query;
  coap_pkt->location_query_len = strlen(query);

  coap_set_option(coap_pkt, COAP_OPTION_LOCATION_QUERY);
  return coap_pkt->location_query_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_observe(coap_message_t *coap_pkt, uint32_t *observe)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_OBSERVE)) {
    return 0;
  }
  *observe = coap_pkt->observe;
  return 1;
}
int
coap_set_header_observe(coap_message_t *coap_pkt, uint32_t observe)
{
  coap_pkt->observe = observe;
  coap_set_option(coap_pkt, COAP_OPTION_OBSERVE);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_block2(coap_message_t *coap_pkt, uint32_t *num, uint8_t *more,
                       uint16_t *size, uint32_t *offset)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_BLOCK2)) {
    return 0;
  }
  /* pointers may be NULL to get only specific block parameters */
  if(num != NULL) {
    *num = coap_pkt->block2_num;
  }
  if(more != NULL) {
    *more = coap_pkt->block2_more;
  }
  if(size != NULL) {
    *size = coap_pkt->block2_size;
  }
  if(offset != NULL) {
    *offset = coap_pkt->block2_offset;
  }
  return 1;
}
int
coap_set_header_block2(coap_message_t *coap_pkt, uint32_t num, uint8_t more,
                       uint16_t size)
{
  if(size < 16) {
    return 0;
  }
  if(size > 2048) {
    return 0;
  }
  if(num > 0x0FFFFF) {
    return 0;
  }
  coap_pkt->block2_num = num;
  coap_pkt->block2_more = more ? 1 : 0;
  coap_pkt->block2_size = size;

  coap_set_option(coap_pkt, COAP_OPTION_BLOCK2);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_block1(coap_message_t *coap_pkt, uint32_t *num, uint8_t *more,
                       uint16_t *size, uint32_t *offset)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_BLOCK1)) {
    return 0;
  }
  /* pointers may be NULL to get only specific block parameters */
  if(num != NULL) {
    *num = coap_pkt->block1_num;
  }
  if(more != NULL) {
    *more = coap_pkt->block1_more;
  }
  if(size != NULL) {
    *size = coap_pkt->block1_size;
  }
  if(offset != NULL) {
    *offset = coap_pkt->block1_offset;
  }
  return 1;
}
int
coap_set_header_block1(coap_message_t *coap_pkt, uint32_t num, uint8_t more,
                       uint16_t size)
{
  if(size < 16) {
    return 0;
  }
  if(size > 2048) {
    return 0;
  }
  if(num > 0x0FFFFF) {
    return 0;
  }
  coap_pkt->block1_num = num;
  coap_pkt->block1_more = more;
  coap_pkt->block1_size = size;

  coap_set_option(coap_pkt, COAP_OPTION_BLOCK1);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_size2(coap_message_t *coap_pkt, uint32_t *size)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_SIZE2)) {
    return 0;
  }
  *size = coap_pkt->size2;
  return 1;
}
int
coap_set_header_size2(coap_message_t *coap_pkt, uint32_t size)
{
  coap_pkt->size2 = size;
  coap_set_option(coap_pkt, COAP_OPTION_SIZE2);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_size1(coap_message_t *coap_pkt, uint32_t *size)
{
  if(!coap_is_option(coap_pkt, COAP_OPTION_SIZE1)) {
    return 0;
  }
  *size = coap_pkt->size1;
  return 1;
}
int
coap_set_header_size1(coap_message_t *coap_pkt, uint32_t size)
{
  coap_pkt->size1 = size;
  coap_set_option(coap_pkt, COAP_OPTION_SIZE1);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_payload(coap_message_t *coap_pkt, const uint8_t **payload)
{
  if(payload != NULL) {
    *payload = coap_pkt->payload;
  }
  return coap_pkt->payload != NULL ? coap_pkt->payload_len : 0;
}
int
coap_set_payload(coap_message_t *coap_pkt, const void *payload, size_t length)
{
  coap_pkt->payload = (uint8_t *)payload;
  coap_pkt->payload_len = MIN(COAP_MAX_CHUNK_SIZE, length);

  return coap_pkt->payload_len;
}
/*---------------------------------------------------------------------------*/
/** @} */
