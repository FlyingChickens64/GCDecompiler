#pragma once

#include <string>
#include <vector>
#include <map>
#include "types.h"
#include "image.h"

namespace types {

using std::string;

// Forward reference
class PNG;

static std::map<int, string> format_names = {
        {0, "I4"}, {1, "I8"}, {2, "IA4"}, {3, "IA8"}, {4, "RGB565"}, {5, "RGB5A3"}, {6, "RGBA32"}, {8, "C4"}, {9, "C8"},
        {10, "C14x2"}, {14, "CMPR"}
};

static std::map<int, uchar> bits_per_pixel = {
        {0, 4}, {1, 8}, {2, 8}, {3, 16}, {4, 16}, {5, 16}, {6, 32}, {8, 4}, {9, 8}, {10, 16}, {14, 4}
};

static std::map<int, uchar> format_heights = {
        {0, 8}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {8, 8}, {9, 4}, {10, 4}, {14, 8}
};

static std::map<int, uchar> format_widths = {
        {0, 8}, {1, 8}, {2, 8}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {8, 8}, {9, 8}, {10, 4}, {14, 8}
};

//  Mario Kart/Wii games, possibly useful later
struct WiiImageTableEntry {
	uint image_header, palette_header;
};

struct WiiPaletteHeader {
	char unpacked;
	uint entry_count, format, offset;
};

struct WiiImageHeader {
	uchar edge_lod_enable, min_lod, max_lod, unpacked;
    ushort height, width;
	uint format, offset, wrap_s, wrap_t, min_filter, max_filter;
	float lod_bias;
};

//XBox TPL, for SMB deluxe etc.
struct XboxImageTableEntry {
    uint format, offset;
    ushort width, height, mipmaps;
};

struct XboxImageHeader {
    ushort width, height;
    uint format, mipmaps, compression, uncompressed_size, unknown_length;
};

// GC TPL stuff. SMB2 etc
struct GCImageTableEntry {
	uint format, offset;
    ushort width, height, mipmaps;
};

class TPL {

protected:

	uint num_images;
	std::vector<Image> images;
	
	virtual void generate_table_entries() = 0;

public:

	TPL();
	virtual void save(const std::string& filename) const = 0;
	virtual Image get_image(const uint& index) const;
	virtual void add_image(const Image& image);
	virtual PNG* to_png(const int& index);
	uint get_num_images() const;

};

class WiiTPL : public TPL {

protected:

	std::vector<WiiImageTableEntry> image_table;
	std::vector<WiiPaletteHeader> palette_heads;
	std::vector<WiiImageHeader> image_heads;
	uint table_offset;
    
    void generate_table_entries() override;

public:
    
    constexpr static uint IDENTIFIER = 0x0020AF30;

	explicit WiiTPL(std::fstream& input);
	WiiTPL(std::vector<Image> images);
	void save(const std::string& filename) const override;
};

class XboxTPL : public TPL {

protected:
    
    std::vector<XboxImageTableEntry> image_table;
    std::vector<XboxImageHeader> image_heads;
    
    void generate_table_entries() override;
    
public:
    
    constexpr static uint IDENTIFIER = 0x5854504C;
    
    explicit XboxTPL(std::fstream& input);
    XboxTPL(std::vector<Image> images);
	void save(const std::string& filename) const override;
};

class GCTPL : public TPL {

protected:

	std::vector<GCImageTableEntry> image_table;
	
	void generate_table_entries() override;

public:

	explicit GCTPL(std::fstream& input);
	GCTPL(std::vector<Image> images);
	void save(const std::string& filename) const override;
};

TPL* tpl_factory(const std::string& filename);

}
