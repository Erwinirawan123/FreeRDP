# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

define_channel_client_subsystem("audin" "pulse" "")

find_package(PulseAudio REQUIRED)

set(${MODULE_PREFIX}_SRCS audin_pulse.c)

set(${MODULE_PREFIX}_LIBS winpr freerdp ${PULSEAUDIO_LIBRARY} ${PULSEAUDIO_MAINLOOP_LIBRARY})

include_directories(..)
include_directories(SYSTEM ${PULSEAUDIO_INCLUDE_DIR})

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")
