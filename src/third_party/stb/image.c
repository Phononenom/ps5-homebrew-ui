// Garlic Save Companion - PNG decoding through the pinned stb_image release.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb/stb_image.h"
