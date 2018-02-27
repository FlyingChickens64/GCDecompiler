
#include <limits.h>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include "rel_reader.h"
#include "types.h"

using std::string;
using std::ios;
using std::vector;
using std::endl;

uint btoi(char *bytes, uint len) {
	uint out = 0;
	for (uint i = 0; i < len; i++) {
		out += bytes[i] << (8 * (len - i - 1));
	}
	return out;
}

const char* itob(uint num, uint length = 4) {
	char *output = new char [length]();
	for (uint i = 0; i < length; i++) {
		output[i] = num >> (8 * (length - i - 1));
	}
	return output;
}

string itoh(uint num) {
	std::stringstream out;
	out << "0x";
	out << std::hex << std::uppercase << num;
	return out.str();
}

uint next_int(std::fstream *file, uint length) {
	char *input = new char [length]();
	file->read(input, length);
	uint out = btoi(input, length);
	delete[] input;
	return out;
}

void write_int(std::fstream *file, uint num, uint length = 4) {
	const char *to_write;
	to_write = itob(num, length);
	file->write(to_write, length);
	delete[] to_write;
}

REL::REL(string filename) {
	std::fstream file_r (filename, ios::binary | ios::in);
	std::fstream *file = &file_r;

	// Read in Header File
	this->id = next_int(file, 4);
	file->seekg(8, ios::cur); // Skip over the next and previous module values
	uint num_sections = next_int(file , 4);
	uint section_offset = next_int(file, 4);
	this->name_offset = next_int(file, 4);
	this->name_size = next_int(file, 4);
	this->version = next_int(file, 4);
	this->bss_size = next_int(file, 4);
	file->seekg(4, ios::cur); // Because we don't need to know the relocation offset
	uint import_offset = next_int(file, 4);
	uint num_imports = next_int(file, 4) / 8; // Convert length of imports to number of imports
	this->prolog_section = next_int(file, 1);
	this->epilog_section = next_int(file, 1);
	this->unresolved_section = next_int(file, 1);
	file->seekg(1, ios::cur); // Skip padding
	this->prolog_offset = next_int(file, 4);
	this->epilog_offset = next_int(file, 4);
	this->unresolved_offset = next_int(file, 4);
	if (this->version >= 2) {
		this->align = next_int(file, 4);
		this->bss_align = next_int(file, 4);
	}
	if (this->version >= 3) {
		this->fix_size = next_int(file, 4);
	}
	this->header_size = (int)file->tellg();

	// Read in Section table
	file->seekg(section_offset, ios::beg);
	for (uint i = 0; i < num_sections; i++) {
		uint offset = next_int(file, 4);
		bool exec = offset & 1;
		offset = offset >> 1 << 1;
		uint length = next_int(file, 4);
		this->sections.push_back(Section(i, offset, exec, length));
	}

	// Read in Section data
	for (vector<Section>::iterator section = this->sections.begin(); section != this->sections.end(); section++) {
		if (section->offset != 0) {
			file->seekg(section->offset, ios::beg);
			char *data = new char[section->length];
			file->read(data, section->length);
			section->data = data;
		}
	}

	// Read in Import table
	file->seekg(import_offset, ios::beg);
	for (uint i = 0; i < num_imports; i++) {
		uint module_id = next_int(file, 4);
		uint offset = next_int(file, 4);
		this->imports.push_back(Import(module_id, offset));
	}

	// Read Relocation table into Import
	for (vector<Import>::iterator imp = this->imports.begin(); imp != this->imports.end(); imp++) {
		file->seekg(imp->offset, ios::beg);
		RelType rel_type = RelType(0);
		while (rel_type != R_RVL_STOP) {
			uint position = file->tellg();
			uint prev_offset = next_int(file, 2);
			rel_type = RelType(next_int(file, 1));
			Section *section = &this->sections.at(next_int(file, 1));
			uint rel_offset = next_int(file, 4);
			imp->add_relocation(rel_type, position, rel_offset, prev_offset, section);
		}
	}
}

uint REL::num_sections() {
	return this->sections.size();
}

uint REL::num_imports() {
	return this->imports.size();
}

uint REL::num_relocations() {
	uint out = 0;
	for (vector<Import>::iterator imp = this->imports.begin(); imp != imports.end(); imp++) {
		out += imp->instructions.size();
	}
	return out;
}

uint REL::section_offset() {
	return header_size;
}

uint REL::import_offset() {
	int out = this->relocation_offset();
	for (vector<Import>::iterator imp = this->imports.begin(); imp != imports.end(); imp++) {
		out += imp->instructions.size() * 8;
	}
	return out;
}

uint REL::relocation_offset() {
	int out = 0;
	out += this->header_size;
	out += this->num_sections() * 8;
	for (vector<Section>::iterator section = this->sections.begin(); section != this->sections.end(); section++) {
		if (section->offset != 0) {
			out += section->length;
		}
	}
	out += 16;
	return out;
}

void REL::compile(string filename) {
	std::fstream out_r(filename, ios::out | ios::binary);
	std::fstream *out = &out_r;
	// Recalculate any necessary numbers for offsets

	// Write Header to the file
	write_int(out, this->id);
	write_int(out, 0, 8); // Padding for Prev and Next module addresses.
	write_int(out, this->num_sections());
	write_int(out, this->section_offset());
	write_int(out, this->name_offset);
	write_int(out, this->name_size);
	write_int(out, this->version);
	write_int(out, this->bss_size);
	write_int(out, this->relocation_offset());
	write_int(out, this->import_offset());
	write_int(out, this->num_imports() * 8); // Convert number of imports to length of imports
	write_int(out, this->prolog_section, 1);
	write_int(out, this->epilog_section, 1);
	write_int(out, this->unresolved_section, 1);
	write_int(out, 0, 1); // Padding for 8 byte alignment
	write_int(out, this->prolog_offset);
	write_int(out, this->epilog_offset);
	write_int(out, this->unresolved_offset);
	if (this->version >= 2) {
		write_int(out, this->align);
		write_int(out, this->bss_align);
	}
	if (this->version >= 3) {
		write_int(out, this->fix_size);
	}

	// Write Section Table to the file
	out->seekp(this->section_offset(), ios::beg);
	for (uint i = 0; i < this->num_sections(); i++) {
		Section section = this->sections.at(i);
		write_int(out, section.offset | (int)section.exec); // Add exec bit back in
		write_int(out, section.length);
	}

	// Write actual sections to the file
	for (vector<Section>::iterator section = this->sections.begin(); section != this->sections.end(); section++) {
		if (section->offset != 0) {
			out->seekp(section->offset, ios::beg);
			out->write(section->data, section->length);
		}
	}

	// Write the Import Table
	out->seekp(this->import_offset(), ios::beg);
	for (uint i = 0; i < this->num_imports(); i++) {
		Import imp = this->imports.at(i);
		write_int(out, imp.module);
		write_int(out, imp.offset);
	}

	// Write the Relocation Instructions
	for (vector<Import>::iterator imp = this->imports.begin(); imp != this->imports.end(); imp++) {
		out->seekp(imp->offset, ios::beg);
		uint loc = out->tellp();
		for (vector<Relocation>::iterator reloc = imp->instructions.begin(); reloc != imp->instructions.end(); reloc++) {
			write_int(out, reloc->prev_offset, 2);
			write_int(out, reloc->type, 1);
			write_int(out, reloc->get_src_section().id, 1);
			write_int(out, reloc->relative_offset);
		}
	}
}

string REL::dump_header() {
	std::stringstream out;
	out << "REL Header:" << endl;
	out << "  ID: " << this->id << endl;
	out << "  Version: " << this->version << endl;
	out << "  Name Offset: " << itoh(this->name_offset) << endl;
	out << "  Name Size: " << itoh(this->name_size) << endl;
	out << "  .bss Size: " << itoh(this->bss_size) << endl;
	out << "  Sections Start: " << itoh(this->section_offset()) << endl;
	out << "  Num Sections: " << this->num_sections() << endl;
	out << "  Import Start: " << itoh(this->import_offset()) << endl;
	out << "  Num Imports: " << this->num_imports() << endl;
	out << "  Relocation Start: " << itoh(this->relocation_offset()) << endl;
	out << "  Num Relocations: " << this->num_relocations() << endl;
	out << "  Prolog Index: " << this->prolog_section << endl;
	out << "  Prolog Offset: " << itoh(this->prolog_offset) << endl;
	out << "  Epilog Index: " << this->epilog_section << endl;
	out << "  Epilog Offset: " << itoh(this->epilog_offset) << endl;
	out << "  Unresolved Index: " << this->unresolved_section << endl;
	out << "  Unresolved Offset: " << itoh(this->unresolved_offset) << endl;
	if (this->version >= 2) {
		out << "  Align: " << this->align << endl;
		out << "  .bss Align: " << this->bss_align << endl;
	}
	if (this->version >= 3) {
		out << "  Fix Size: " << this->fix_size << endl;
	}
	return out.str();
}

void REL::dump_header(string filename) {
	std::fstream out(filename, ios::out);
	out << this->dump_header();
}

string REL::dump_sections() {
	std::stringstream out;
	out << "Section Table:" << endl;
	for (vector<Section>::iterator section = this->sections.begin(); section != sections.end(); section++) {
		out << "  Section " << section->id << ":" << endl;
		out << "    Offset: " << itoh(section->offset) << endl;
		out << "    Length: " << itoh(section->length) << endl;
		if (section->offset > 0) {
			out << "    Range: " << itoh(section->offset) << " - " << itoh(section->offset + section->length) << endl;
		}
		out << "    Executable: " << section->exec << endl;
	}
	return out.str();
}

void REL::dump_sections(string filename) {
	std::fstream out(filename, ios::out);
	out << this->dump_sections();
}

string REL::dump_imports() {
	std::stringstream out;
	out << "Import Table:" << endl;
	for (vector<Import>::iterator imp = this->imports.begin(); imp != this->imports.end(); imp++) {
		out << "  Import:" << endl;
		out << "    Module: " << imp->module << endl;
		out << "    Offset: " << itoh(imp->offset) << endl;
		out << "    Relocation Table:" << endl;
		for (vector<Relocation>::iterator reloc = imp->instructions.begin(); reloc != imp->instructions.end(); reloc++) {
			out << "      Relocation:" << endl;
			out << "        Position: " << itoh(reloc->position) << endl;
			out << "        Type: " << RelNames.at(reloc->type) << endl;
			if (reloc->type == R_RVL_STOP) {
				continue;
			}
			if (reloc->type == R_RVL_SECT) {
				out << "        Destination Section: " << reloc->get_dest_section().id << endl;
				continue;
			}
			out << "        Offset from Prev: " << itoh(reloc->prev_offset) << endl;
			out << "        Source: " << reloc->get_src_section().id << " " << itoh(reloc->get_src_offset()) << endl;
			out << "        Destination: " << reloc->get_dest_section().id << " " << itoh(reloc->get_dest_offset())<< endl;
		}
	}
	return out.str();
}

void REL::dump_imports(string filename) {
	std::fstream out(filename, ios::out);
	out << this->dump_imports();
}

string REL::dump_all() {
	std::stringstream out;
	out << this->dump_header() << endl;
	out << this->dump_sections() << endl;
	out << this->dump_imports() << endl;
	return out.str();
}

void REL::dump_all(string filename) {
	std::fstream out(filename, ios::out);
	out << this->dump_header() << endl;
	out << this->dump_sections() << endl;
	out << this->dump_imports() << endl;
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		std::cout << "Usage:" << endl << "GCDecompiler dump <file in> [file out]" << endl;
		std::cout << "GCDecompiler recompile <file in> [file out]" << endl;
	} else if (argc == 2) {
		std::cout << "Need a file in parameter";
	} else {
		string output = "out.txt";
		if (argc > 3) {
			output = argv[3];
		} else {
			if (!std::strcmp(argv[1], "dump")) {
				output = "dump.txt";
			} else if (!std::strcmp(argv[1], "recompile")) {
				output = "recomp.rel";
			}
		}

		REL rel(argv[2]);
		if (!std::strcmp(argv[1], "dump")) {
			std::cout << argv[2] << endl;
			std::cout << "Dumping REL" << endl;
			rel.dump_all(output);
			std::cout << "REL dump complete" << endl;
		} else if (!std::strcmp(argv[0], "recompile")) {
			std::cout << "Recompiling REL" << endl;
			rel.compile(output);
			std::cout << "REL recompile complete" << endl;
		} else {
			std::cout << "Unrecognized Operation" << endl;
		}
	}

	return 0;
}
