#include <cassert>
#include <cstdint>
#include <cstring>
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

	template <typename T>
	void
	apply_remap(const T *src, const size_t src_pitch, T *dst, const size_t dst_pitch, const size_t width, const size_t height, const char *remap) {
		for (size_t row = 0; row < height; row++) {
			const T *src_row = reinterpret_cast<const T *>(reinterpret_cast<const uint8_t *>(src) + row * src_pitch);
			T *dst_row = reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(dst) + row * dst_pitch);

			for (size_t col = 0; col < width; col++) {
				dst_row[col] = remap[src_row[col]];
			}
		}
	}

	void RenderToSurface::blit([[maybe_unused]] SDL_Renderer *renderer,
				   const std::unique_ptr<RenderSurface> &src,
				   const SDL_Rect *src_rect,
				   const SDL_Rect *dst_rect,
				   [[maybe_unused]] const bool transparent,
				   const char *remap) {
		auto &src_tex = static_cast<RenderToSurface &>(*src);

		SDL_Surface *source = src_tex.texture;
		if (remap) {
			SDL_Surface *new_src = SDL_CreateSurface(source->w, source->h, source->format);

			SDL_LockSurface(src_tex.texture);
			SDL_LockSurface(new_src);
			apply_remap(static_cast<uint8_t *>(source->pixels),
				    source->pitch,
				    static_cast<uint8_t *>(new_src->pixels),
				    new_src->pitch,
				    source->w,
				    source->h,
				    remap);
			SDL_UnlockSurface(src_tex.texture);
			SDL_UnlockSurface(new_src);

			source = new_src;
		}

		if (src_rect && dst_rect && (src_rect->w != dst_rect->w || src_rect->h != dst_rect->h))
			SDL_BlitSurfaceScaled(source, src_rect, this->texture, dst_rect, SDL_SCALEMODE_LINEAR);
		else
			SDL_BlitSurface(source, src_rect, this->texture, dst_rect);

		if (remap) {
			SDL_DestroySurface(source);
		}
	}

	void RenderToTexture::blit(SDL_Renderer *renderer,
				   const std::unique_ptr<RenderSurface> &src,
				   const SDL_Rect *src_rect,
				   const SDL_Rect *dst_rect,
				   [[maybe_unused]] const bool transparent,
				   const char *remap) {
		auto &src_tex = static_cast<RenderToTexture &>(*src);

		SDL_FRect src_frect = {};
		SDL_FRect *p_src_frect = nullptr;
		if (src_rect) {
			src_frect = SDL_FRect{ (float)src_rect->x, (float)src_rect->y, (float)src_rect->w, (float)src_rect->h };
			p_src_frect = &src_frect;
		}

		SDL_FRect dst_frect = SDL_FRect{ (float)dst_rect->x, (float)dst_rect->y, (float)dst_rect->w, (float)dst_rect->h };

		SDL_Texture *source = src_tex.texture;
		if (remap) {
			void *src_pixels = nullptr;
			int src_pitch = 0;
			src_tex.lock(&src_pixels, &src_pitch);

			SDL_Texture *new_src =
				SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, src_tex.width, src_tex.height);
			void *new_pixels = nullptr;
			int pitch = 0;
			SDL_LockTexture(new_src, nullptr, &new_pixels, &pitch);

			apply_remap(static_cast<uint8_t *>(src_pixels),
				    src_pitch,
				    static_cast<uint8_t *>(new_pixels),
				    pitch,
				    src_tex.width,
				    src_tex.height,
				    remap);

			src_tex.unlock();
			SDL_UnlockTexture(new_src);

			source = new_src;
		}

		// if (transparent)
		// 	SDL_SetTextureBlendMode(source, SDL_BLENDMODE_BLEND);

		SDL_SetRenderTarget(renderer, this->texture);

		SDL_RenderTexture(renderer, source, p_src_frect, &dst_frect);

		// if (transparent)
		// 	SDL_SetTextureBlendMode(source, SDL_BLENDMODE_NONE);
		SDL_SetRenderTarget(renderer, nullptr);

		if (remap) {
			SDL_DestroyTexture(source);
		}
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
		SDL_Color sdl_color = static_cast<SDL_Color>(rgb);

		dst->fill_rect(this->renderer, rect, sdl_color);
	}

	void RenderBackend::blit(const std::unique_ptr<RenderSurface> &src,
				 const SDL_Rect *src_rect,
				 const std::unique_ptr<RenderSurface> &dst,
				 const SDL_Rect *dst_rect,
				 const bool transparent,
				 const char *remap) {
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
		dst->blit(this->renderer, src, src_rect, &dst_rect_new, transparent, remap);
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
