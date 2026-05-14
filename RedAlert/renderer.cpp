#include <cstdint>
#include <memory>
#include "renderer.h"

#include "SDL3/SDL_blendmode.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_surface.h"
#include "line.h"
#include "palettec.h"
#include "rgb.h"

namespace rendering {
	void RenderToSurface::fill_rect([[maybe_unused]] SDL_Renderer *renderer, const SDL_Rect *rect, const SDL_Color color) {
		SDL_FillSurfaceRect(this->texture, rect, SDL_MapSurfaceRGBA(this->texture, color.r, color.g, color.b, color.a));
	}

	void RenderToTexture::fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, const SDL_Color color) {
		SDL_SetRenderTarget(renderer, this->texture);
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
		SDL_FRect frect = SDL_FRect{ (float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h };
		SDL_RenderFillRect(renderer, &frect);
		SDL_SetRenderTarget(renderer, nullptr);
	}

	void RenderToSurface::blit([[maybe_unused]] SDL_Renderer *renderer,
				   const std::unique_ptr<RenderSurface> &src,
				   const SDL_Rect *src_rect,
				   const SDL_Rect *dst_rect,
				   const bool transparent) {
		auto &src_tex = static_cast<RenderToSurface &>(*src);
		if (transparent)
			SDL_SetSurfaceColorKey(src_tex.texture, true, 0);

		if (src_rect && dst_rect && (src_rect->w != dst_rect->w || src_rect->h != dst_rect->h))
			SDL_BlitSurfaceScaled(src_tex.texture, src_rect, this->texture, dst_rect, SDL_SCALEMODE_LINEAR);
		else
			SDL_BlitSurface(src_tex.texture, src_rect, this->texture, dst_rect);

		if (transparent)
			SDL_SetSurfaceColorKey(src_tex.texture, false, 0);
	}

	void RenderToTexture::blit(SDL_Renderer *renderer,
				   const std::unique_ptr<RenderSurface> &src,
				   const SDL_Rect *src_rect,
				   const SDL_Rect *dst_rect,
				   const bool transparent) {
		auto &src_tex = static_cast<RenderToTexture &>(*src);

		SDL_SetRenderTarget(renderer, this->texture);
		if (transparent)
			SDL_SetTextureBlendMode(src_tex.texture, SDL_BLENDMODE_BLEND);

		SDL_FRect src_frect = {};
		SDL_FRect *p_src_frect = nullptr;
		if (src_rect) {
			src_frect = SDL_FRect{ (float)src_rect->x, (float)src_rect->y, (float)src_rect->w, (float)src_rect->h };
			p_src_frect = &src_frect;
		}

		SDL_FRect dst_frect = SDL_FRect{ (float)dst_rect->x, (float)dst_rect->y, (float)dst_rect->w, (float)dst_rect->h };

		SDL_RenderTexture(renderer, src_tex.texture, p_src_frect, &dst_frect);

		if (transparent)
			SDL_SetTextureBlendMode(src_tex.texture, SDL_BLENDMODE_NONE);
		SDL_SetRenderTarget(renderer, nullptr);
	}

	void RenderToSurface::lock(void **pixels, int *pitch) {
		*pixels = this->texture->pixels;
		*pitch = this->texture->pitch;
	}

	void RenderToSurface::unlock(void) {
	}

	void RenderToTexture::lock(void **pixels, int *pitch) {
		SDL_LockTexture(this->texture, nullptr, pixels, pitch);
	}

	void RenderToTexture::unlock(void) {
		SDL_UnlockTexture(this->texture);
	}

	std::unique_ptr<RenderSurface> RenderBackend::create_surface(int w, int h) {
		// Should be taken from a global config struct:
		const SurfaceType type = SurfaceType::TEXTURE;

		switch (type) {
			using enum SurfaceType;
		case TEXTURE:
			return std::unique_ptr<RenderToTexture>(new RenderToTexture(this->renderer, w, h));
		case SURFACE:
			return std::unique_ptr<RenderToSurface>(new RenderToSurface(w, h));
		}
		throw UnknownRenderMethod(type);
	}

	void RenderBackend::fill_rect(const std::unique_ptr<RenderSurface> &dst, const SDL_Rect *rect, const uint8_t color) {
		const RGBClass &rgb = PaletteClass::CurrentPalette[color];
		SDL_Color sdl_color = {
			.r = (uint8_t)rgb.Red_Component(),
			.g = (uint8_t)rgb.Green_Component(),
			.b = (uint8_t)rgb.Blue_Component(),
			.a = 255,
		};

		dst->fill_rect(this->renderer, rect, sdl_color);
	}

	void RenderBackend::blit(const std::unique_ptr<RenderSurface> &src,
				 const SDL_Rect *src_rect,
				 const std::unique_ptr<RenderSurface> &dst,
				 const SDL_Rect *dst_rect,
				 const bool transparent) {
		SDL_Rect dst_rect_new = *dst_rect;
		if (dst_rect->w == 0 && dst_rect->h == 0) {
			if (src_rect) {
				dst_rect_new.w = src_rect->w;
				dst_rect_new.h = src_rect->h;
			} else {
				dst_rect_new.w = src->width;
				dst_rect_new.h = src->height;
			}
		}
		dst->blit(this->renderer, src, src_rect, &dst_rect_new, transparent);
	}

	void RenderBackend::draw_line(const std::unique_ptr<RenderSurface> &dst,
				      const Point start,
				      const Point end,
				      const uint8_t color,
				      const Rect &box) {
		ClipResult clip = clip_line_cohen_sutherland(start, end, box);
		if (!clip.visible)
			return;

		void *pixels = nullptr;
		int pitch = 0;
		this->lock(dst, &pixels, &pitch);

		uint8_t *buffer = static_cast<uint8_t *>(pixels);
		const RGBClass &rgb = PaletteClass::CurrentPalette[color];
		const uint32_t pixel_color = SDL_MapRGBA(SDL_GetPixelFormatDetails(dst->format()),
							 nullptr,
							 (uint8_t)rgb.Red_Component(),
							 (uint8_t)rgb.Green_Component(),
							 (uint8_t)rgb.Blue_Component(),
							 255);
		auto set_pixel = [buffer, pitch, pixel_color](Point p) {
			uint32_t *addr = (uint32_t *)(buffer + p.y * pitch + p.x * 4);
			*addr = pixel_color;
		};

		bresenham_line(clip.start, clip.end, set_pixel);

		this->unlock(dst);
	}
} // namespace rendering
