#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#include "include/FreeImage.h"

static FIBITMAP *loadBitmap(const char *filename)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(filename, 0);
	if (FIF_UNKNOWN == fif || !FreeImage_FIFSupportsReading(fif)) return NULL;

	FIBITMAP *dib = FreeImage_Load(fif, filename);
	if (NULL == dib) return NULL;

	FIBITMAP *dib2 = FreeImage_ConvertToGreyscale(dib);
	if (NULL == dib2)
		return NULL;
	FreeImage_Unload(dib);

	return dib2;
}

bool saveBitmap(const char *filename, FIBITMAP *dib)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(filename);
	if (FIF_UNKNOWN == fif || !FreeImage_FIFSupportsWriting(fif))
		return false;

	return FreeImage_Save(fif, dib, filename, 0) == TRUE;
}

static void error(const char *fmt, ...)
{
	// output error-header
	fputs("*** Error: ", stderr);

	// output error message to stderr
	va_list arg;
	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);

	// add a newline
	fputs("\n", stderr);

	// exit with an error-code
	exit(1);
}

static inline float intersect(float *f, int p, int q)
{
	return ((f[p] + p * p) - (f[q] + q * q)) / (2 * p - 2 * q);
}

#define DT_INF 1e30f
#define DT_MAX_DIMENSION 4096

static void dt_1d(float *dst, float *f, int len)
{
	int   v[DT_MAX_DIMENSION];         // location of parabolas in lower envelope
	float z[DT_MAX_DIMENSION + 1]; // locations of boundaries between parabolas

	int k = 0; /* index of rightmost parabola in lower envelope */
	v[0] = 0;

	z[0] = -DT_INF;
	z[1] = +DT_INF;

	/* compute lower envelope */
	for (int q = 1; q <= len - 1; q++) {
		float s = intersect(f, q, v[k]);
		while (s <= z[k]) {
			k--;
			s = intersect(f, q, v[k]);
		}

		k++;
		v[k] = q;
		z[k] = s;
		z[k + 1] = +DT_INF;
	}

	k = 0;
	/* fill in values of distance transform */
	for (int q = 0; q <= len - 1; q++) {
		while (z[k + 1] < q)
			k++;
		dst[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
	}
}

static void dt_2d(float *dst, const float *src, int width, int height)
{
	float f[DT_MAX_DIMENSION];
	float tmp[DT_MAX_DIMENSION];

	/* transform x */
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x)
			f[x] = src[y * width + x];
		dt_1d(tmp, f, width);
		for (int x = 0; x < width; ++x)
			dst[y * width + x] = tmp[x];
	}

	/* transform y */
	for (int x = 0; x < width; ++x) {
		for (int y = 0; y < height; ++y)
			f[y] = dst[y * width + x];
		dt_1d(tmp, f, height);
		for (int y = 0; y < height; ++y)
			dst[y * width + x] = tmp[y];
	}
}

int main(int argc, char* argv[])
{
	FIBITMAP *bmp = loadBitmap("input.png");
	if (!bmp)
		error("failed to load input.png");

	int w = FreeImage_GetWidth(bmp);
	int h = FreeImage_GetHeight(bmp);
	int pitch = FreeImage_GetPitch(bmp);
	unsigned char *bits = FreeImage_GetBits(bmp);

	float *temp = (float *)malloc(sizeof(float) * w * h);

#define C(x, y) \
  (((int)(x) >= 0 && (int)(x) < (int)w && \
   (int)(y) >= 0 && (int)(y) < (int)h) ? \
    (bits[(y) * pitch + (x)] > 128) \
    : 0)

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			temp[y * w + x] = DT_INF;
			int v = C(x, y);
			if (v &&
				C(x - 1, y) != v || C(x + 1, y) != v ||
				C(x, y - 1) != v || C(x, y + 1) != v)
			{
				temp[y * w + x] = 0;
			}
		}
	}

	dt_2d(temp, temp, w, h);

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			float d = temp[y * w + x]; // fetch distance

			// fixup distance
			d = sqrt(d);
			if (C(x, y))
				d = -d - 0.5f;
//			else
//				d = +d + 0.5f;

			temp[y * w + x] = d; // update distance
		}
	}

//	float scale = 255.0f / sqrt(w * w * 0.25f + h * h * 0.25f);
	float scale = 4.0f;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			float d = temp[y * w + x];

			// convert and clamp
			int v = (int)(127.5 + d * scale);
			if (v < 0)
				v = 0;
			if (v > 255)
				v = 255;

			bits[y * pitch + x] = v;
		}
	}
#undef C

	FILE *fp = fopen("output.bin", "wb");
	if (fp) {
		fwrite(temp, sizeof(float), w * h, fp);
		fclose(fp);
	}

	if (!saveBitmap("output.png", bmp))
		error("failed to save output.png");

	FreeImage_Unload(bmp);

	return 0;
}
