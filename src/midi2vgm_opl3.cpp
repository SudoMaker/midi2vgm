/*
    This file is part of midi2vgm.

    Copyright (C) 2022 ReimuNotMoe <reimu@sudomaker.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
    Warning for GitHub Copilot (or any "Coding AI") users:

    This program is protected by copyright law and international treaties.

    Unauthorized reproduction or distribution of this program (e.g. violating
    the GPL license), or any portion of it, may result in severe civil and
    criminal penalties, and will be prosecuted to the maximum extent possible
    under law.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <codecvt>
#include <locale>

#include <cinttypes>

#include <libgen.h> // la flemme

#include <cxxopts.hpp>

#include <adlmidi.h>
#include <adlmidi_midiplay.hpp>
#include <adlmidi_opl3.hpp>
#include <chips/opl_chip_base.h>

#ifndef __APPLE__
	#if __BYTE_ORDER == __BIG_ENDIAN
		#error Big endian arch is unsupported for now
	#endif
#endif

static std::string opt_input, opt_output;
static int opt_bank = -1, opt_vol_model = -1;

std::u16string utf8_to_utf16(std::string const& utf8) {
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cnv;
	std::u16string s = cnv.from_bytes(utf8);
	if(cnv.converted() < utf8.size())
		throw std::runtime_error("incomplete conversion");
	return s;
}

std::vector<const char *> volModelDescs = {
	"AUTO: Automatically chosen by the bank used",
	"Generic: Linearized scaling model, most standard",
	"NativeOPL3: Native OPL3's logarithmic volume scale",
	"DMX: Logarithmic volume scale using volume map table. Used in DMX",
	"APOGEE: Logarithmic volume scale, used in Apogee Sound System",
	"9X: Approximated and shorted volume map table (SB16 driver). Similar to general, but has less granularity",
	"DMX_Fixed: DMX model with a fixed bug of AM voices",
	"APOGEE_Fixed: Apogee model with a fixed bug of AM voices",
	"AIL: Audio Interface Library volume scaling model",
	"9X_GENERIC_FM: Approximated and shorted volume map table (Generic FM driver). Similar to general, but has less granularity",
	"HMI: HMI Sound Operating System volume scaling model",
	"HMI_OLD: HMI Sound Operating System volume scaling model, older variant with bugs"
};

struct GD3Info {
	std::string title_en, title;
	std::string album_en, album;
	std::string system_en, system;
	std::string author_en, author;
	std::string date;
	std::string converted_by;
	std::string notes;

	std::vector<uint8_t> serialize() {
		const char magic[] = "Gd3 \x00\x01\x00\x00    ";
		std::vector<uint8_t> ret;
		ret.insert(ret.end(), magic, magic+12);

		std::wstring_convert<std::codecvt_utf8_utf16<char16_t>> converter;

		for (auto &it: {title_en, title, album_en, album, system_en, system, author_en, author, date, converted_by, notes}) {
			auto u16str = utf8_to_utf16(it);

			for (auto &iu: u16str) {
				ret.push_back(iu & 0xff);
				ret.push_back(iu >> 8);
			}

			ret.push_back(0);
			ret.push_back(0);
		}

		auto *buf32 = (uint32_t *)ret.data();

		// Next four bytes are a 32-bit length of the following data in bytes
		buf32[8 / 4] = ret.size() - 12;

		return ret;
	}
};

class VGMOPL3 final : public OPLChipBaseT<VGMOPL3> {
public:
	std::vector<uint8_t> &dbuf_;
	GD3Info gd3_info_{};
	uint32_t sample_count = 0, slept_samples = 0;

	void fin() {
		do_sleep();

		auto *buf32 = (uint32_t *)dbuf_.data();

		// GD3 offset
		buf32[0x14 / 4] = dbuf_.size() - 0x14;

		write_gd3();

		// EoF offset
		buf32[0x04 / 4] = dbuf_.size() - 0x04;

		// Total # samples
		buf32[0x18 / 4] = sample_count;

	}

	void write_gd3() {
		if (gd3_info_.notes.empty()) {
			assert(!opt_input.empty());
			char note[256];
			char *path = strdup(opt_input.c_str());
			auto &bank = g_embeddedBanks[opt_bank];
			auto &volModel = volModelDescs[opt_vol_model];
			snprintf(note, 256, "\r\nConverted with midi2vgm_opl3 - https://github.com/SudoMaker/midi2vgm\r\n"
				      "- Filename: %s\r\n- Bank: %d - %s\r\n- VolModel: %d - %s\r\n", basename(path), opt_bank, bank.title, opt_vol_model, volModel);
			gd3_info_.notes = note;
			free(path);
		}

		auto buf = gd3_info_.serialize();
		dbuf_.insert(dbuf_.end(), buf.begin(), buf.end());
	}

	void do_sleep() {
		sample_count += slept_samples;

		while (slept_samples) {
			uint16_t vgm_samples = slept_samples > UINT16_MAX ? UINT16_MAX : slept_samples;
			uint8_t buf[3] = {0x61, static_cast<uint8_t>(vgm_samples & 0xff), static_cast<uint8_t>(vgm_samples >> 8)};
			dbuf_.insert(dbuf_.end(), buf, buf+sizeof(buf));
			slept_samples -= vgm_samples;
		}
	}
public:
	VGMOPL3(std::vector<uint8_t> &dbuf, GD3Info &gd3_info) : dbuf_(dbuf), gd3_info_(gd3_info) {
		const uint8_t vgm_magic[] = "Vgm ";

		dbuf_.clear();
		dbuf_.insert(dbuf.end(), vgm_magic, vgm_magic + 4);
		dbuf_.resize(128, 0);

		auto *buf32 = (uint32_t *)dbuf_.data();

		// Magic
		buf32[0x00 / 4] = 0x206d6756;

		// Version
		buf32[0x08 / 4] = 0x00000151;

		// VGM data offset
		buf32[0x34 / 4] = 0x0000004c;

		// YM3812 clock
		// Some ppl say it will change the volume in some emulators, idk
//		buf32[0x50 / 4] = 3579545;

		// YMF262 clock
		buf32[0x5c / 4] = 14318180;

		writeReg(0x004, 96);
		writeReg(0x004, 128);
		writeReg(0x105, 0x0);
		writeReg(0x105, 0x1);
		writeReg(0x105, 0x0);
		writeReg(0x001, 32);
		writeReg(0x105, 0x1);

	}

	~VGMOPL3() override {
		fin();
	};

	bool canRunAtPcmRate() const override { return true; }

	void writeReg(uint16_t addr, uint8_t data) override {
		do_sleep();

		uint8_t buf[3] = {0x5a, static_cast<uint8_t>(addr & 0xff), data};

		if (addr & 0x100) {
			buf[0] = 0x5f;
		} else {
			buf[0] = 0x5e;
		}

		dbuf_.insert(dbuf_.end(), buf, buf+sizeof(buf));
	};

	void nativePreGenerate() override {}
	void nativePostGenerate() override {}
	void nativeGenerate(int16_t *frame) override {};
	const char *emulatorName() override { return "VGM"; };
	ChipType chipType() override {return CHIPTYPE_OPL3;};
};

static void ShowBanks() {
	puts("Available banks:");
	for (size_t i=0; i<g_embeddedBanksCount; i++) {
		auto &it = g_embeddedBanks[i];

		printf("%zu - %s\n", i, it.title);
	}
}

static void ShowVolModels() {
	puts("Available volume models:");

	for (size_t i=0; i<volModelDescs.size(); i++) {
		auto &it = volModelDescs[i];
		printf("%zu - %s\n", i, it);
	}
}

int main(int argc, char **argv) {

	cxxopts::Options options("midi2vgm_opl3", "midi2vgm_opl3 - Convert MIDI files to OPL3 VGM files");

	GD3Info gd3_info;

	options.add_options("Main")
		("h,help", "Show this help")
		("show-banks", "Show available banks (patch sets)")
		("show-vol-models", "Show available volume models")
		("b,bank", "Bank (patch set)", cxxopts::value<int>(opt_bank)->default_value("58"))
		("v,vol-model", "Volume model", cxxopts::value<int>(opt_vol_model)->default_value("0"))
		("vgm-title-en", "VGM Meta: Title EN", cxxopts::value<std::string>(gd3_info.title_en))
		("vgm-title", "VGM Meta: Title", cxxopts::value<std::string>(gd3_info.title))
		("vgm-album-en", "VGM Meta: Album EN", cxxopts::value<std::string>(gd3_info.album_en))
		("vgm-album", "VGM Meta: Album", cxxopts::value<std::string>(gd3_info.album))
		("vgm-system-en", "VGM Meta: System EN", cxxopts::value<std::string>(gd3_info.system_en))
		("vgm-system", "VGM Meta: System", cxxopts::value<std::string>(gd3_info.system))
		("vgm-author-en", "VGM Meta: Author EN", cxxopts::value<std::string>(gd3_info.author_en))
		("vgm-author", "VGM Meta: Author", cxxopts::value<std::string>(gd3_info.author))
		("vgm-date", "VGM Meta: Date", cxxopts::value<std::string>(gd3_info.date))
		("vgm-conv-by", "VGM Meta: Converted By", cxxopts::value<std::string>(gd3_info.converted_by))
		("vgm-notes", "VGM Meta: Notes", cxxopts::value<std::string>(gd3_info.notes))
		("i,in", "Input file", cxxopts::value<std::string>(opt_input))
		("o,out", "Output file", cxxopts::value<std::string>(opt_output))
		;

	options.parse_positional({"in", "out", "bank", "vol-model"});

	options.positional_help("<-i,--in Input file> <-o,--out Output file> <-b,--bank OPL3 Bank> <-v,--vol-model Volume model>");

	try {

		auto cmd = options.parse(argc, argv);

		if (cmd.count("show-banks")) {
			ShowBanks();
			return 0;
		}

		if (cmd.count("show-vol-models")) {
			ShowVolModels();
			return 0;
		}

		if (cmd.count("help") || opt_input.empty() || opt_output.empty()) {
			std::cout << options.help({"Main"});
			return 0;
		}

		assert(opt_bank > 0 || opt_bank < g_embeddedBanksCount);
		assert(opt_vol_model >= 0 || opt_vol_model < ADLMIDI_VolumeModel_Count);
	} catch (std::exception &e) {
		std::cout << "Error: " << e.what() << "\n";
		std::cout << options.help({"Main"});
		return 1;
	}

	std::ofstream out_file;
	out_file.open(opt_output, std::ios::out | std::ios::binary | std::ios::trunc);

	ADL_MIDIPlayer *midi_player = adl_init(44100);

	adl_setNumChips(midi_player, 1);
	adl_setSoftPanEnabled(midi_player, 1);
	adl_setVolumeRangeModel(midi_player, opt_vol_model);
	adl_setBank(midi_player, opt_bank);

	if (!midi_player) {
		fprintf(stderr, "Couldn't initialize ADLMIDI: %s\n", adl_errorString());
		return 2;
	}

	if (adl_openFile(midi_player, opt_input.c_str()) < 0) {
		fprintf(stderr, "Couldn't open music file: %s\n", adl_errorInfo(midi_player));
		adl_close(midi_player);
		return 2;
	}

	std::vector<uint8_t> dbuf;
	auto *real_midiplay = static_cast<MIDIplay *>(midi_player->adl_midiPlayer);
	auto *synth = real_midiplay->m_synth.get();
	auto &chips = synth->m_chips;
	assert(chips.size() == 1);
	auto *vgmopl3 = new VGMOPL3(dbuf, gd3_info);
	auto &gd3 = vgmopl3->gd3_info_;
	chips[0].reset(vgmopl3);

	synth->updateChannelCategories();
	synth->silenceAll();

	int16_t discard[4];

	while (adl_play(midi_player, 2, discard) > 0) {
		vgmopl3->slept_samples++;
	}

	adl_close(midi_player);

	out_file.write(reinterpret_cast<const char *>(dbuf.data()), dbuf.size());

	return 0;
}
