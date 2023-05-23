/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Example of reading/writing an external serial flash using the PL022 SPI interface

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#define FLASH_PAGE_SIZE        256
#define FLASH_SECTOR_SIZE      4096

#define FLASH_CMD_PAGE_PROGRAM 0x02
#define FLASH_CMD_READ         0x03
#define FLASH_CMD_STATUS       0x05
#define FLASH_CMD_WRITE_EN     0x06
#define FLASH_CMD_SECTOR_ERASE 0x20

#define FLASH_STATUS_BUSY_MASK 0x01

static inline void cs_select(uint cs_pin);

static inline void cs_deselect(uint cs_pin);

void __not_in_flash_func(flash_read)(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t *buf, size_t len);

void __not_in_flash_func(flash_write_enable)(spi_inst_t *spi, uint cs_pin);

void __not_in_flash_func(flash_wait_done)(spi_inst_t *spi, uint cs_pin);

void __not_in_flash_func(flash_sector_erase)(spi_inst_t *spi, uint cs_pin, uint32_t addr);

void __not_in_flash_func(flash_page_program)(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t data[]);

void printbuf(uint8_t buf[FLASH_PAGE_SIZE]);
