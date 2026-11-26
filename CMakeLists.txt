# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(BOARD_FLASH_RUNNER stm32cubeprogrammer)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Lora_network)

target_sources(app PRIVATE 
    src/main.c
    src/Core/LED/led_sts.c
    src/Core/CmdLine/cmd_line.c
    src/Core/CmdLine/cmd.c
    src/Core/LoRa/lora_app.c
    src/BSP/uart_cmd/uart_cmd.c
    src/BSP/lora/lora_radio.c
    src/protocol/common/protocol_packet.c
    src/protocol/common/protocol_config.c
    src/protocol/processor/packet_processor.c
    src/protocol/dispatcher/packet_dispatcher.c
    src/protocol/handler/ping_handler.c
    src/protocol/mesh/mesh_router.c
    src/protocol/mesh/mesh_duplicate.c
)

target_include_directories(app PRIVATE
    src/Core/LED
    src/BSP/uart_cmd
    src/Core/CmdLine
    src/Core/LoRa
    src/BSP/lora
    src/protocol/common
    src/protocol/processor
    src/protocol/dispatcher
    src/protocol/handler
    src/protocol/mesh
)