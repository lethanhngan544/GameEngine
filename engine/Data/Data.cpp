#include <Data.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#define STB_DXT_IMPLEMENTATION
#include <stb_dxt.h>

namespace eg::Data
{
	bool LoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
		std::string* warn, int req_width, int req_height,
		const unsigned char* bytes, int size, void* user_data) {
		(void)warn;

		int w = 0, h = 0, comp = 0, req_comp = STBI_rgb_alpha;

		stbi_uc* data = stbi_load_from_memory(bytes, size, &w, &h, &comp, req_comp);
		if (!data) {
			if (err) {
				(*err) +=
					"Unknown image format. STB cannot decode image data for image[" +
					std::to_string(image_idx) + "] name = \"" + image->name + "\".\n";
			}
			return false;
		}



		if ((w < 1) || (h < 1)) {
			stbi_image_free(data);
			if (err) {
				(*err) += "Invalid image data for image[" + std::to_string(image_idx) +
					"] name = \"" + image->name + "\"\n";
			}
			return false;
		}

		if (req_width > 0) {
			if (req_width != w) {
				stbi_image_free(data);
				if (err) {
					(*err) += "Image width mismatch for image[" +
						std::to_string(image_idx) + "] name = \"" + image->name +
						"\"\n";
				}
				return false;
			}
		}

		if (req_height > 0) {
			if (req_height != h) {
				stbi_image_free(data);
				if (err) {
					(*err) += "Image height mismatch. for image[" +
						std::to_string(image_idx) + "] name = \"" + image->name +
						"\"\n";
				}
				return false;
			}
		}



		// Store the decoded image data
		//Compress image using stb_dxt
		std::vector<uint8_t> compressed;

		const int blocksX = (w + 3) / 4;
		const int blocksY = (h + 3) / 4;
		compressed.resize(blocksX * blocksY * 16); // BC1 = 8 bytes per 4x4 block



		uint8_t block[64]; // 4x4 block of RGBA
		for (int by = 0; by < blocksY; ++by) {
			for (int bx = 0; bx < blocksX; ++bx) {
				std::memset(block, 0, sizeof(block));
				for (int y = 0; y < 4; ++y) {
					for (int x = 0; x < 4; ++x) {
						int imgX = bx * 4 + x;
						int imgY = by * 4 + y;
						if (imgX < w && imgY < h) {
							int srcIndex = (imgY * w + imgX) * 4;
							int dstIndex = (y * 4 + x) * 4;
							std::memcpy(&block[dstIndex], &data[srcIndex], 4);
						}
					}
				}
				int blockIndex = by * blocksX + bx;
				stb_compress_dxt_block(&compressed[blockIndex * 16], block, 1, STB_DXT_NORMAL);
			}
		}
		image->width = w;
		image->height = h;
		image->component = 4;
		image->bits = 8;
		image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
		image->as_is = false;
		image->image = std::move(compressed);


		stbi_image_free(data);
		return true;
	}
}