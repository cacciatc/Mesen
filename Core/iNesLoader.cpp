#include "stdafx.h"
#include "iNesLoader.h"
#include "../Utilities/CRC32.h"
#include "../Utilities/HexUtilities.h"
#include "GameDatabase.h"
#include "EmulationSettings.h"

RomData iNesLoader::LoadRom(vector<uint8_t>& romFile, NESHeader *preloadedHeader)
{
	RomData romData;

	NESHeader header;
	uint8_t* buffer = romFile.data();
	if(preloadedHeader) {
		header = *preloadedHeader;
		header.SanitizeHeader(romFile.size() + sizeof(NESHeader));
	} else {
		memcpy((char*)&header, buffer, sizeof(NESHeader));
		buffer += sizeof(NESHeader);
		header.SanitizeHeader(romFile.size());
	}

	romData.Format = RomFormat::iNes;

	romData.IsNes20Header = (header.GetRomHeaderVersion() == RomHeaderVersion::Nes2_0);
	romData.MapperID = header.GetMapperID();
	romData.SubMapperID = header.GetSubMapper();
	romData.Mirroring = header.GetMirroringType();
	romData.HasBattery = header.HasBattery();
	if(header.IsPalRom()) {
		romData.System = GameSystem::NesPal;
	} else if(header.IsVsSystem()) {
		romData.System = GameSystem::VsUniSystem;
	} else if(header.IsPlaychoice()) {
		romData.System = GameSystem::Playchoice;
	}
	romData.HasTrainer = header.HasTrainer();
	romData.ChrRamSize = header.GetChrRamSize();
	romData.SaveChrRamSize = header.GetSaveChrRamSize();
	romData.WorkRamSize = header.GetWorkRamSize();
	romData.SaveRamSize = header.GetSaveRamSize();
	romData.NesHeader = header;

	if(romData.HasTrainer) {
		//512-byte trainer at $7000-$71FF (stored before PRG data)
		romData.TrainerData.insert(romData.TrainerData.end(), buffer, buffer + 512);
		buffer += 512;
	}

	int bytesRead = buffer - romFile.data();

	uint32_t romCrc = CRC32::GetCRC(buffer, romFile.size() - bytesRead);
	romData.PrgChrCrc32 = romCrc;

	NESHeader dbHeader;
	GameDatabase::GetiNesHeader(romData.PrgChrCrc32, dbHeader);
	if(dbHeader.GetPrgSize() != header.GetPrgSize() || dbHeader.GetChrSize() != header.GetChrSize()) {
		if(dbHeader.GetPrgSize() + dbHeader.GetChrSize() + bytesRead <= romFile.size()) {
			//Use corrected PRG/CHR sizes from the DB when reading from file
			header = dbHeader;
		}
	}

	romData.PrgRom.insert(romData.PrgRom.end(), buffer, buffer + header.GetPrgSize());
	buffer += header.GetPrgSize();
	romData.ChrRom.insert(romData.ChrRom.end(), buffer, buffer + header.GetChrSize());

	romData.PrgCrc32 = CRC32::GetCRC(romData.PrgRom.data(), romData.PrgRom.size());

	MessageManager::Log("PRG+CHR CRC32: 0x" + HexUtilities::ToHex(romData.PrgChrCrc32));

	if(romData.IsNes20Header) {
		MessageManager::Log("[iNes] NES 2.0 file: Yes");
	}
	MessageManager::Log("[iNes] Mapper: " + std::to_string(romData.MapperID) + " Sub:" + std::to_string(romData.SubMapperID));
	MessageManager::Log("[iNes] PRG ROM: " + std::to_string(romData.PrgRom.size()/1024) + " KB");
	MessageManager::Log("[iNes] CHR ROM: " + std::to_string(romData.ChrRom.size()/1024) + " KB");
	if(romData.ChrRamSize > 0 || romData.IsNes20Header) {
		MessageManager::Log("[iNes] CHR RAM: " + std::to_string(romData.ChrRamSize / 1024) + " KB");
	} else if(romData.ChrRom.size() == 0) {
		MessageManager::Log("[iNes] CHR RAM: 8 KB");
	}
	if(romData.WorkRamSize > 0 || romData.IsNes20Header) {
		MessageManager::Log("[iNes] Work RAM: " + std::to_string(romData.WorkRamSize / 1024) + " KB");
	}
	if(romData.SaveRamSize > 0 || romData.IsNes20Header) {
		MessageManager::Log("[iNes] Save RAM: " + std::to_string(romData.SaveRamSize / 1024) + " KB");
	}

	MessageManager::Log("[iNes] Mirroring: " + string(romData.Mirroring == MirroringType::Horizontal ? "Horizontal" : romData.Mirroring == MirroringType::Vertical ? "Vertical" : "Four Screens"));
	MessageManager::Log("[iNes] Battery: " + string(romData.HasBattery ? "Yes" : "No"));
	if(romData.HasTrainer) {
		MessageManager::Log("[iNes] Trainer: Yes");
	}

	GameDatabase::SetGameInfo(romData.PrgChrCrc32, romData, !EmulationSettings::CheckFlag(EmulationFlags::DisableGameDatabase) && header.GetRomHeaderVersion() != RomHeaderVersion::Nes2_0);

	return romData;
}
