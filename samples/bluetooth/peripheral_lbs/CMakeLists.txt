#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
)
if (CONFIG_BT_MDS)
target_sources(app PRIVATE
  src/mds.c
)
endif()
# NORDIC SDK APP END
zephyr_library_include_directories(.)

# Substitute our own function in place of the NCS
# memfault_platform_get_device_info() implementation
zephyr_ld_options(-Wl,--wrap=memfault_platform_get_device_info)
zephyr_include_directories(config)
