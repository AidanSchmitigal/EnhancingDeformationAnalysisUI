#include <OpenGL/Texture.h>

#include <string.h>

#include <glad/glad.h>

#include <stb_image/stb_image.h>

#include <tiffio.h>

Texture::Texture() {
	glGenTextures(1, &m_id);
}

Texture::~Texture() {
	glDeleteTextures(1, &m_id);
}

// assumes we want to use bytes and not floats
void Texture::Load(const char* filename) {
	TIFF* tif = TIFFOpen(filename, "r");
	if (!tif) {
		printf("Could not open file %s\n", filename);
		return;
	}
	size_t npixels;
	uint32_t* raster;

	Bind();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &m_width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &m_height);

	TIFFRGBAImage img;
	char emsg[1024];
	if (!TIFFRGBAImageBegin(&img, tif, 0, emsg)) {
		TIFFError(filename, emsg);
		TIFFClose(tif);
		return;
	}
	npixels = m_width * m_height;
	raster = (uint32_t*)_TIFFmalloc(npixels * sizeof(uint32_t));
	if (raster) {
		if (TIFFRGBAImageGet(&img, raster, m_width, m_height)) {
			printf("Loaded %s, %d x %d\n", filename, m_width, m_height);
			// flip the image
			uint32_t* temp = (uint32_t*)_TIFFmalloc(npixels * sizeof(uint32_t));
			for (int i = 0; i < m_height; i++) {
				memcpy(temp + i * m_width, raster + (m_height - i - 1) * m_width, m_width * sizeof(uint32_t));
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, temp);
			_TIFFfree(temp);
		}
	}
	_TIFFfree(raster);
	TIFFClose(tif);

	Unbind();
}

void Texture::Bind() {
	glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::Unbind() {
	glBindTexture(GL_TEXTURE_2D, 0);
}
