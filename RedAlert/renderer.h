#pragma once

#include <exception>
#include <format>
#include <memory>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include "palettec.h"

#ifndef TIME_PER_TICK
#define TIME_PER_TICK 16
#endif

namespace rendering {
	enum SurfaceType {
		SURFACE,
		TEXTURE,
	};

	using Point = SDL_Point;
	using Rect = SDL_Rect;
} // namespace rendering

template <>
struct std::formatter<rendering::SurfaceType> {
	constexpr auto parse(std::format_parse_context &ctx) {
		return ctx.begin();
	}

	auto format(const rendering::SurfaceType &st, std::format_context &ctx) {
		switch (st) {
			using enum rendering::SurfaceType;
		case TEXTURE:
			return std::format_to(ctx.out(), "{}", "TEXTURE");
		case SURFACE:
			return std::format_to(ctx.out(), "{}", "SURFACE");
		default:
			return std::format_to(ctx.out(), "Unknown");
		}
	}
};

namespace rendering {
	class UnknownRenderMethod : public std::exception {
	private:
		std::string msg;

	public:
		UnknownRenderMethod(SurfaceType type) {
			this->msg = std::format("Unknown rendering surface: {}", type);
		}

		const char *what(void) const noexcept {
			return this->msg.c_str();
		}
	};

	struct RenderBackend;

	class RenderSurface {
	public:
		virtual ~RenderSurface(void) = default;

		virtual SDL_PixelFormat format(void) = 0;

		virtual bool lock(void **pixels, int *pitch) = 0;
		virtual bool unlock(void) = 0;

		virtual void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, const SDL_Color color) = 0;
		virtual void blit(SDL_Renderer *renderer,
				  const std::unique_ptr<RenderSurface> &src,
				  const SDL_Rect *src_rect,
				  const SDL_Rect *dst_rect,
				  const bool transparent = false,
				  const char *remap = nullptr) = 0;

		int width = 0, height = 0, pitch = 0;

	protected:
		RenderSurface(int w, int h) : width(w), height(h) {
		}
	};

	class RenderToSurface : public RenderSurface {
	public:
		virtual ~RenderToSurface(void) {
			SDL_DestroySurface(this->texture);
		}

		virtual inline SDL_PixelFormat format(void) {
			return this->texture->format;
		};

		virtual bool lock(void **pixels, int *pitch);
		virtual bool unlock(void);

		inline operator bool(void) {
			return this->texture != nullptr;
		}

		virtual void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, const SDL_Color color);
		virtual void blit(SDL_Renderer *renderer,
				  const std::unique_ptr<RenderSurface> &src,
				  const SDL_Rect *src_rect,
				  const SDL_Rect *dst_rect,
				  const bool transparent = false,
				  const char *remap = nullptr);

	private:
		RenderToSurface(int w, int h) : RenderSurface(w, h) {
			this->texture = SDL_CreateSurface(this->width, this->height, SDL_PIXELFORMAT_RGBA32);
			this->pitch = this->texture->pitch;
			SDL_SetSurfacePalette(this->texture, static_cast<SDL_Palette *>(PaletteClass::CurrentPalette));
		}

		friend RenderBackend;

	private:
		SDL_Surface *texture = nullptr;
	};

	class RenderToTexture : public RenderSurface {
	public:
		virtual ~RenderToTexture(void) {
			SDL_DestroyTexture(this->texture);
		}

		virtual inline SDL_PixelFormat format(void) {
			return this->texture->format;
		};

		virtual bool lock(void **pixels, int *pitch);
		virtual bool unlock(void);

		inline operator bool(void) {
			return this->texture != nullptr;
		}

		virtual void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, const SDL_Color color);
		virtual void blit(SDL_Renderer *renderer,
				  const std::unique_ptr<RenderSurface> &src,
				  const SDL_Rect *src_rect,
				  const SDL_Rect *dst_rect,
				  const bool transparent = false,
				  const char *remap = nullptr);

	private:
		RenderToTexture(SDL_Renderer *renderer, int w, int h) : RenderSurface(w, h) {
			this->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, this->width, this->height);
			this->pitch = 0;
		}

		friend RenderBackend;

	private:
		SDL_Texture *texture = nullptr;
	};

	struct RenderBackend {
		SDL_Window *window;
		SDL_Renderer *renderer;
		SDL_Palette *current_palette = nullptr;

		std::unique_ptr<RenderSurface> create_surface(int w, int h);
		void destroy_surface(std::unique_ptr<RenderSurface> &s);

		inline void set_palette(const PaletteClass &palette) {
			if (this->current_palette)
				SDL_DestroyPalette(this->current_palette);
			this->current_palette = static_cast<SDL_Palette *>(palette);
		}
		inline SDL_Palette *get_palette(void) {
			return this->current_palette;
		}

		inline void blit(const std::unique_ptr<RenderSurface> &src,
				 const std::unique_ptr<RenderSurface> &dst,
				 int x,
				 int y,
				 const bool transparent = false,
				 const char *remap = nullptr) {
			SDL_Rect dst_rect = SDL_Rect{ x, y, src->width, src->height };
			this->blit(src, nullptr, dst, &dst_rect, transparent, remap);
		};
		inline void blit(const std::unique_ptr<RenderSurface> &src,
				 const SDL_Rect *src_rect,
				 const std::unique_ptr<RenderSurface> &dst,
				 int x,
				 int y,
				 const bool transparent = false,
				 const char *remap = nullptr) {
			SDL_Rect dst_rect = SDL_Rect{ x, y, src_rect->w, src_rect->h };
			this->blit(src, src_rect, dst, &dst_rect, transparent, remap);
		};
		void blit(const std::unique_ptr<RenderSurface> &src,
			  const SDL_Rect *src_rect,
			  const std::unique_ptr<RenderSurface> &dst,
			  const SDL_Rect *dst_rect,
			  const bool transparent = false,
			  const char *remap = nullptr);

		void fill_rect(const std::unique_ptr<RenderSurface> &dst, const SDL_Rect *rect, const uint8_t color);
		void draw_line(const std::unique_ptr<RenderSurface> &dst, const Point start, const Point end, const uint8_t color, const Rect &box);
		void present();

		inline bool lock(const std::unique_ptr<RenderSurface> &s, void **pixels, int *pitch) {
			return s->lock(pixels, pitch);
		};

		inline bool unlock(const std::unique_ptr<RenderSurface> &s) {
			return s->unlock();
		};
	};

	extern RenderBackend *GRenderer; // pointer to direct draw object
	extern SDL_Window *MainWindow; // handle to programs main window
} // namespace rendering
