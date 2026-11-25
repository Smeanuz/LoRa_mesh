# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(BOARD_FLASH_RUNNER stm32cubeprogrammer)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Lora_network)

target_sources(app PRIVATE 
    src/main.c
    Core/LED/led_sts.c
    Core/CmdLine/cmd_line.c
    Core/CmdLine/cmd.c
    Core/LoRa/lora_app.c
    BSP/uart_cmd/uart_cmd.c
    BSP/lora/lora_radio.c
    protocol/common/protocol_packet.c
    protocol/common/protocol_config.c
    protocol/processor/packet_processor.c
    protocol/dispatcher/packet_dispatcher.c
    protocol/handler/ping_handler.c
    protocol/mesh/mesh_router.c
    protocol/mesh/mesh_duplicate.c
)

target_include_directories(app PRIVATE
    Core/LED
    BSP/uart_cmd
    Core/CmdLine
    Core/LoRa
    BSP/lora
    protocol/common
    protocol/processor
    protocol/dispatcher
    protocol/handler
    protocol/mesh
)