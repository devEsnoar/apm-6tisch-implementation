<img src="https://github.com/contiki-ng/contiki-ng.github.io/blob/master/images/logo/Contiki_logo_2RGB.png" alt="Logo" width="256">

# Contiki-NG: The OS for Next Generation IoT Devices

[![Github Actions](https://github.com/contiki-ng/contiki-ng/workflows/CI/badge.svg?branch=develop)](https://github.com/contiki-ng/contiki-ng/actions)
[![Documentation Status](https://readthedocs.org/projects/contiki-ng/badge/?version=master)](https://contiki-ng.readthedocs.io/en/master/?badge=master)
[![license](https://img.shields.io/badge/license-3--clause%20bsd-brightgreen.svg)](https://github.com/contiki-ng/contiki-ng/blob/master/LICENSE.md)
[![Latest release](https://img.shields.io/github/release/contiki-ng/contiki-ng.svg)](https://github.com/contiki-ng/contiki-ng/releases/latest)
[![GitHub Release Date](https://img.shields.io/github/release-date/contiki-ng/contiki-ng.svg)](https://github.com/contiki-ng/contiki-ng/releases/latest)
[![Last commit](https://img.shields.io/github/last-commit/contiki-ng/contiki-ng.svg)](https://github.com/contiki-ng/contiki-ng/commit/HEAD)

[![Stack Overflow Tag](https://img.shields.io/badge/Stack%20Overflow%20tag-Contiki--NG-blue?logo=stackoverflow)](https://stackoverflow.com/questions/tagged/contiki-ng)
[![Gitter](https://img.shields.io/badge/Gitter-Contiki--NG-blue?logo=gitter)](https://gitter.im/contiki-ng)
[![Twitter](https://img.shields.io/badge/Twitter-%40contiki__ng-blue?logo=twitter)](https://twitter.com/contiki_ng)

Contiki-NG is an open-source, cross-platform operating system for Next-Generation IoT devices. It focuses on dependable (secure and reliable) low-power communication and standard protocols, such as IPv6/6LoWPAN, 6TiSCH, RPL, and CoAP. Contiki-NG comes with extensive documentation, tutorials, a roadmap, release cycle, and well-defined development flow for smooth integration of community contributions.

Unless explicitly stated otherwise, Contiki-NG sources are distributed under
the terms of the [3-clause BSD license](LICENSE.md). This license gives
everyone the right to use and distribute the code, either in binary or
source code format, as long as the copyright license is retained in
the source code.

Contiki-NG started as a fork of the Contiki OS and retains some of its original features.

Find out more:

* GitHub repository: https://github.com/contiki-ng/contiki-ng
* Documentation: https://docs.contiki-ng.org/
* List of releases and changes: https://github.com/contiki-ng/contiki-ng/releases
* Web site: http://contiki-ng.org

Engage with the community:

* Discussions on GitHub: https://github.com/contiki-ng/contiki-ng/discussions
* Contiki-NG tag on Stack Overflow: https://stackoverflow.com/questions/tagged/contiki-ng
* Gitter: https://gitter.im/contiki-ng
* Twitter: https://twitter.com/contiki_ng

# Master Thesis: APM 6TiSCH evaluation

This repository presents the code utilized to implement the APM methods described in the Master Thesis "Efficient Monitoring of Low-Power Wireless based on In-Band Network Telemetry". This chapter explains how to reproduce the results that led to the data presented in the thesis as well as where the files that implement INT and Piggybacking are located.

## Cooja

The simulations were executed in the Cooja simulator, present in the folder `tools/cooja`. Cooja was executed in a Docker container as it is specificed in the tutorial provided by Contiki-NG:

* [Getting started -> Docker](https://docs.contiki-ng.org/en/develop/doc/getting-started/Docker.html)
* [Running Contiki‐NG in Cooja](https://docs.contiki-ng.org/en/develop/doc/tutorials/Running-Contiki-NG-in-Cooja.html#running-contiking-in-cooja)

## APM Methods Implementation

### In-Band Network Telemetry

INT implementation is located in the folder `os/net/mac/tsch/int`. Explanation about what each file does can be found in the thesis document. Some key configuration parameters:

* `TSCH_CONF_WITH_INT`: Located in `os/net/mac/tsch/tsch-conf.h` enables/disables INT.
* `INT_CONF_TELEMETRY_EXPERIMENT`: Flag to set if 'dummy' values are appended as metrics. 1 if yes.
* `INT_CONF_TELEMETRY_EXPERIMENT_SIZE`: Size of the 'dummy' metrics which is constant during the simulation run.
* `INT_CONF_PROBABILISTIC`: Flag to enable INT probabilistic.
* `TELEMETRY_CONF_COUNTER`: Flag to enable telemetry counter for hybrid approach.
* `INT_CONF_MAX_TELEMETRY_ENTRIES`: Maximum number of telemetry entries that can be saved by the node. Default is 16.
* `INT_ACTIVE_MONITORING_CONF`: Flag to enable the `is_am_message` flag, which excludes embedding INT fields into Active Monitoring packets in the hybrid approach.

### Piggybacking at the CoAP layer

Implemented in the already present CoAP implementation from Contiki-NG, located in the file `os/net/app-layer/coap/coap.c`.

* `COAP_CONF_WITH_PIGGYBACKING`: Enables Piggybacking at the CoAP layer.
* `COAP_CONF_TELEMETRY_SIZE`: Size of the telemetry packet to be piggybacked.

## Applications

The application code for testing the methods are located in the folder `examples/apm-6tisch`. There are always to different firmware files, one for the CoAP server and another for the CoAP client. The application traffic generator is included by the CoAP client Makefiles.

Some general configuration parameters are:

* `ENERGEST_CONF_ON` to turn on the ENERGEST module.
* `ENERGEST_CONF_ADDITIONS` adds `ENERGEST_TYPE_CUSTOM_LISTEN` to count ticks only when a packet is 
* `SIMPLE_ENERGEST_CONF_PERIOD` to set the ENERGEST period to `(CLOCK_SECOND * 5)`
* `LOG_CONF_LEVEL_*` to enable logs at certain levels of the network stack. 

### CoAP Server

The CoAP server firmware code is the same for all the methods, except for the hybrid approach, where a new URL is added to send telemetry data as Active Monitoring packets.

### Application Traffic Generator

`examples/apm-6tisch/fixed-app` contains the code for the application that generates application messages of random sizes. This application is used for the demanding scenario.

* Payload sizes can be adjusted in file `fixed-app.c`, variable `payload_sizes`.
* Transmission frequency can be adjusted in file `fixed-app.h`, parameter `TOGGLE_INTERVAL`.

**Note**: Traffic generator for the non-demanding scenario can be found in branch `exp/piggybacking-vs-int-vs-am`, file `fixed-app.c`.

### Active Monitoring

`examples/apm-6tisch/active-monitoring` contains the firmware files to deploy a network that utilizes the Active Monitoring method. In `common-conf.h`, INT is turned off by resetting `TSCH_CONF_WITH_INT`.

**CoAP Client**

* `coap-client` implements the firmware for the nodes that will generate the traffic and send it to the server.
* `TELEMETRY_EXPERIMENT_SIZE` in file `node-client.c` defines 
* `MONITORING_TOGGLE_INTERVAL` defines the period of transmission.
* Telemetry is sent using the `send/dummy` URL.

### Hybrid Approach

`examples/apm-6tisch/hybrid-approach` contains the files to deploy a network that utilizes the proposed hybrid approach. In the `common-conf.h` file, the following parameters are set:

* `INT_CONF_TELEMETRY_EXPERIMENT` is enabled to use dummy values as telemetry metrics.
* `TELEMETRY_SIZE` to set the size of the telemetry packets in bytes.
* `TSCH_CONF_WITH_INT` to enable INT.
* `INT_CONF_PROBABILISTIC` set to 0 to use INT Opportunistic.
* `TELEMETRY_CONF_COUNTER` set to 1 to count the telemetry bytes appended by INT.

**CoAP Client**

* `INT_ACTIVE_MONITORING_CONF` is set to 1, so the Active Monitoring packets sent by the client can be excluded from adding INT fields.
* `node-client.c` implements the algorithm exposed in the thesis document for the hybrid approach,
* `send/telemetry` URL is used to send the dedicated Active Monitoring packets.

### In-Band Network Telemetry

`examples/apm-6tisch/in-band-network` contains the code to deploy a network that utilizes INT. In the `common-conf.h` file, the following parameters are set:

* `INT_CONF_TELEMETRY_EXPERIMENT` is enabled to use dummy values as telemetry metrics.
* `TELEMETRY_SIZE` to set the size of the telemetry packets in bytes.
* `TSCH_CONF_WITH_INT` to enable INT.
* `INT_CONF_PROBABILISTIC` set to enable/disable INT Probabilistic.

**CoAP Client**

Implements a timer to free the memory blocks (MEMB) used to save the telemetry metrics per node. The frequency in which these metrics
are consumed is defined by `CONSUMING_INTERVAL`.

### INT Probabilistic - Insertion Ratio

`examples/apm-6tisch/int-probabilistic` contains the application code used to reproduce the results presented by Karaagac et. al. where only the last node generates application traffic. The parameters are the same than for In-Band Network Telemetry. The Makefile for this project does not include the application traffic generator described above.

### Piggybacking

`examples/apm-6tisch/piggybacking` contains the application code to run a network utilizing the Piggybacking method. In the `common-conf.h` file, the following parameters are set:

* `TELEMETRY_SIZE` to set the size of the telemetry packets in bytes.
* `TSCH_CONF_WITH_INT` to disable INT.
* `COAP_CONF_WITH_PIGGYBACKING` to enable piggybacking at the CoAP layer.

**CoAP Client**

The client does not implement anything apart from initializing the traffic generator described above.

## Simulation and deployment.

The `*.csc` files that are present in the `examples/apm-6tisch` directory describe the Cooja simulation project files, which can be opened with Cooja. Some key network requirements to reproduce the results from the thesis:

* At least, a network with 3 nodes must be deployed.
* Node ID 1 will always deploy the CoAP server firmware, which is also set as the TSCH coordinator.
* Node ID 3 must be always the last node in the linear network topology or the last node to join the network, since the parser starts recording the data once this node joins the network.
* To deploy in a nRF52840 SoC, the `node_id` variable in the `coap-server.c` application code must be validated against the ID of the server node corresponding to the SoC.

**Disclaimer:** Latest code deployed and tested on the nRF52840 SoC is located in this [commit](https://github.com/devEsnoar/apm-6tisch-implementation/tree/8e15bf7e366e13a1e8018cbaff4e9d3a3ef8986c), `examples/coap-6tisch`. It only implements INT Opportunistic. `SERVER_EP` must be set as the Server IP address, which was extracted from the SoC empirically. The Makefile for this project adds a module provided by Contliki-NG, `shell`, useful for debugging purposes.
