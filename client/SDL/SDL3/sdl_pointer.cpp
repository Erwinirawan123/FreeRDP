/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Mouse Pointer
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <freerdp/gdi/gdi.h>

#include "sdl_pointer.hpp"
#include "sdl_freerdp.hpp"
#include "sdl_touch.hpp"
#include "sdl_utils.hpp"

#include <SDL3/SDL_mouse.h>

typedef struct
{
	rdpPointer pointer;
	SDL_Cursor* cursor;
	SDL_Surface* image;
	size_t size;
	void* data;
} sdlPointer;

static BOOL sdl_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);

	WINPR_ASSERT(context);
	if (!ptr)
		return FALSE;

	rdpGdi* gdi = context->gdi;
	WINPR_ASSERT(gdi);

	ptr->size = 4ull * pointer->width * pointer->height;
	ptr->data = winpr_aligned_malloc(ptr->size, 16);

	if (!ptr->data)
		return FALSE;

	auto data = static_cast<BYTE*>(ptr->data);
	if (!freerdp_image_copy_from_pointer_data(
	        data, gdi->dstFormat, 0, 0, 0, pointer->width, pointer->height, pointer->xorMaskData,
	        pointer->lengthXorMask, pointer->andMaskData, pointer->lengthAndMask, pointer->xorBpp,
	        &context->gdi->palette))
	{
		winpr_aligned_free(ptr->data);
		return FALSE;
	}

	return TRUE;
}

static void sdl_Pointer_Clear(sdlPointer* ptr)
{
	WINPR_ASSERT(ptr);
	SDL_DestroyCursor(ptr->cursor);
	SDL_DestroySurface(ptr->image);
	ptr->cursor = nullptr;
	ptr->image = nullptr;
}

static void sdl_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);
	WINPR_UNUSED(context);

	if (ptr)
	{
		sdl_Pointer_Clear(ptr);
		winpr_aligned_free(ptr->data);
		ptr->data = nullptr;
	}
}

static BOOL sdl_Pointer_SetDefault(rdpContext* context)
{
	WINPR_UNUSED(context);

	return sdl_push_user_event(SDL_EVENT_USER_POINTER_DEFAULT);
}

static BOOL sdl_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	WINPR_UNUSED(context);
	return sdl_push_user_event(SDL_EVENT_USER_POINTER_SET, pointer);
}

BOOL sdl_Pointer_Set_Process(SdlContext* sdl)
{
	INT32 w = 0;
	INT32 h = 0;

	WINPR_ASSERT(sdl);

	auto context = sdl->context();
	auto pointer = sdl->cursor();
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);
	if (!ptr)
		return TRUE;

	rdpGdi* gdi = context->gdi;
	WINPR_ASSERT(gdi);

	auto x = static_cast<INT32>(pointer->xPos);
	auto y = static_cast<INT32>(pointer->yPos);
	auto sw = w = static_cast<INT32>(pointer->width);
	auto sh = h = static_cast<INT32>(pointer->height);

	SDL_Window* window = SDL_GetMouseFocus();
	if (!window)
		return sdl_Pointer_SetDefault(context);

	const Uint32 id = SDL_GetWindowID(window);

	if (!sdl_scale_coordinates(sdl, id, &x, &y, FALSE, FALSE) ||
	    !sdl_scale_coordinates(sdl, id, &sw, &sh, FALSE, FALSE))
		return FALSE;

	sdl_Pointer_Clear(ptr);

	ptr->image = SDL_CreateSurface(sw, sh, sdl->sdl_pixel_format);
	if (!ptr->image)
		return FALSE;

	SDL_LockSurface(ptr->image);
	auto pixels = static_cast<BYTE*>(ptr->image->pixels);
	auto data = static_cast<const BYTE*>(ptr->data);
	const BOOL rc = freerdp_image_scale(
	    pixels, gdi->dstFormat, static_cast<UINT32>(ptr->image->pitch), 0, 0,
	    static_cast<UINT32>(ptr->image->w), static_cast<UINT32>(ptr->image->h), data,
	    gdi->dstFormat, 0, 0, 0, static_cast<UINT32>(w), static_cast<UINT32>(h));
	SDL_UnlockSurface(ptr->image);
	if (!rc)
		return FALSE;

	// create a cursor image in 100% display scale to trick SDL into creating the cursor with the
	// correct size
	auto it = sdl->windows.begin();
	if (it == sdl->windows.end())
		return FALSE;

	const auto hidpi_scale = SDL_GetWindowDisplayScale(it->second.window());
	auto normal = SDL_CreateSurface(
	    static_cast<int>(static_cast<float>(ptr->image->w) / hidpi_scale),
	    static_cast<int>(static_cast<float>(ptr->image->h) / hidpi_scale), ptr->image->format);
	assert(normal);
	SDL_BlitSurfaceScaled(ptr->image, nullptr, normal, nullptr,
	                      SDL_ScaleMode::SDL_SCALEMODE_LINEAR);
	SDL_AddSurfaceAlternateImage(normal, ptr->image);

	ptr->cursor = SDL_CreateColorCursor(normal, x, y);
	if (!ptr->cursor)
		return FALSE;

	SDL_DestroySurface(normal);

	SDL_SetCursor(ptr->cursor);
	SDL_ShowCursor();
	sdl->setHasCursor(true);
	return TRUE;
}

static BOOL sdl_Pointer_SetNull(rdpContext* context)
{
	WINPR_UNUSED(context);

	return sdl_push_user_event(SDL_EVENT_USER_POINTER_NULL);
}

static BOOL sdl_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);

	return sdl_push_user_event(SDL_EVENT_USER_POINTER_POSITION, x, y);
}

BOOL sdl_register_pointer(rdpGraphics* graphics)
{
	const rdpPointer pointer = { sizeof(sdlPointer),
		                         sdl_Pointer_New,
		                         sdl_Pointer_Free,
		                         sdl_Pointer_Set,
		                         sdl_Pointer_SetNull,
		                         sdl_Pointer_SetDefault,
		                         sdl_Pointer_SetPosition,
		                         {},
		                         0,
		                         0,
		                         0,
		                         0,
		                         0,
		                         0,
		                         0,
		                         nullptr,
		                         nullptr,
		                         {} };
	graphics_register_pointer(graphics, &pointer);
	return TRUE;
}
