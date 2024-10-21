/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stddef.h>

/** @brief Valid SUIT envelope, based on ../root_candidate_verification_install.yaml
 *
 */
uint8_t manifest_root_candidate_verification_install_buf[] = {
	0xD8, 0x6B, 0xA2, 0x02, 0x58, 0x7A, 0x82, 0x58, 0x24, 0x82, 0x2F, 0x58, 0x20, 0x1A, 0x58,
	0x25, 0xFD, 0x2F, 0x9E, 0xED, 0x27, 0x52, 0x24, 0x7C, 0x01, 0x65, 0xB3, 0xD5, 0x4B, 0x48,
	0xAE, 0xED, 0xDE, 0x9E, 0x4D, 0x7A, 0xEC, 0xB4, 0xC6, 0xD6, 0x35, 0x8E, 0xEF, 0x98, 0x42,
	0x58, 0x51, 0xD2, 0x84, 0x4A, 0xA2, 0x01, 0x26, 0x04, 0x45, 0x1A, 0x40, 0x00, 0x00, 0x00,
	0xA0, 0xF6, 0x58, 0x40, 0xED, 0x9C, 0x73, 0xC0, 0xF9, 0xD3, 0x01, 0x0B, 0x10, 0x34, 0x4E,
	0x69, 0xB5, 0x35, 0xAC, 0xC1, 0x7D, 0x3A, 0x55, 0xD5, 0xD1, 0xBB, 0x1C, 0xFD, 0x69, 0xA4,
	0x10, 0xB1, 0x86, 0xE4, 0xDE, 0xB7, 0xFD, 0x16, 0xBD, 0xED, 0x33, 0xC5, 0xA4, 0x6E, 0xB5,
	0xC2, 0xAF, 0xEF, 0x80, 0xE9, 0x28, 0x46, 0x6A, 0xE9, 0x31, 0xD0, 0xDA, 0x6C, 0x31, 0x26,
	0x3D, 0x81, 0xBD, 0xEA, 0xF3, 0x9C, 0xAB, 0xCF, 0x03, 0x58, 0x84, 0xA6, 0x01, 0x01, 0x02,
	0x01, 0x03, 0x58, 0x52, 0xA3, 0x02, 0x81, 0x82, 0x4A, 0x69, 0x43, 0x41, 0x4E, 0x44, 0x5F,
	0x4D, 0x46, 0x53, 0x54, 0x41, 0x00, 0x04, 0x58, 0x3A, 0x82, 0x14, 0xA3, 0x01, 0x50, 0x76,
	0x17, 0xDA, 0xA5, 0x71, 0xFD, 0x5A, 0x85, 0x8F, 0x94, 0xE2, 0x8D, 0x73, 0x5C, 0xE9, 0xF4,
	0x02, 0x50, 0x97, 0x05, 0x48, 0x23, 0x4C, 0x3D, 0x59, 0xA1, 0x89, 0x86, 0xA5, 0x46, 0x60,
	0xA1, 0x4B, 0x0A, 0x18, 0x18, 0x50, 0x9C, 0x1B, 0x1E, 0x37, 0x2C, 0xB4, 0x5C, 0x33, 0x92,
	0xDD, 0x49, 0x56, 0x6B, 0x18, 0x31, 0x93, 0x01, 0xA1, 0x00, 0xA0, 0x11, 0x43, 0x82, 0x0C,
	0x00, 0x12, 0x43, 0x82, 0x0C, 0x00, 0x05, 0x82, 0x4C, 0x6B, 0x49, 0x4E, 0x53, 0x54, 0x4C,
	0x44, 0x5F, 0x4D, 0x46, 0x53, 0x54, 0x50, 0x97, 0x05, 0x48, 0x23, 0x4C, 0x3D, 0x59, 0xA1,
	0x89, 0x86, 0xA5, 0x46, 0x60, 0xA1, 0x4B, 0x0A};

size_t manifest_root_candidate_verification_install_len =
	sizeof(manifest_root_candidate_verification_install_buf);
