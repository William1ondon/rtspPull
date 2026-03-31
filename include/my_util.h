#pragma once

#include <vector>
#include "common.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <string.h>
#include <signal.h>
#include <arm_neon.h>
#include <string>
#include <fstream>


void save_yuv(const void *yuv_vAddr, char *file_path, int w, int h);

void writeCacheToFile(const void *address, std::size_t size, const std::string &filename);

void MEDIA_MemcpyNeon (void *dest, const void *src, size_t len);

void MEDIA_MemsetNeon (void *dest, int s, size_t len);