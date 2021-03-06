// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"
#include "core/file_sys/archive_selfncch.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ivfc_archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

enum class SelfNCCHFilePathType : u32 {
    RomFS = 0,
    Code = 1, // This is not supported by SelfNCCHArchive but by archive 0x2345678E
    ExeFS = 2,
    UpdateRomFS = 5, // This is presumably for accessing the RomFS of the update patch.
};

struct SelfNCCHFilePath {
    u32_le type;
    std::array<char, 8> exefs_filename;
};
static_assert(sizeof(SelfNCCHFilePath) == 12, "NCCHFilePath has wrong size!");

// A read-only file created from a block of data. It only allows you to read the entire file at
// once, in a single read operation.
class ExeFSSectionFile final : public FileBackend {
public:
    explicit ExeFSSectionFile(std::shared_ptr<std::vector<u8>> data_) : data(std::move(data_)) {}

    ResultVal<size_t> Read(u64 offset, size_t length, u8* buffer) const override {
        if (offset != 0) {
            LOG_ERROR(Service_FS, "offset must be zero!");
            return ERROR_UNSUPPORTED_OPEN_FLAGS;
        }

        if (length != data->size()) {
            LOG_ERROR(Service_FS, "size must match the file size!");
            return ERROR_INCORRECT_EXEFS_READ_SIZE;
        }

        std::memcpy(buffer, data->data(), data->size());
        return MakeResult<size_t>(data->size());
    }

    ResultVal<size_t> Write(u64 offset, size_t length, bool flush,
                            const u8* buffer) const override {
        LOG_ERROR(Service_FS, "The file is read-only!");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    u64 GetSize() const override {
        return data->size();
    }

    bool SetSize(u64 size) const override {
        return false;
    }

    bool Close() const override {
        return true;
    }

    void Flush() const override {}

private:
    std::shared_ptr<std::vector<u8>> data;
};

// SelfNCCHArchive represents the running application itself. From this archive the application can
// open RomFS and ExeFS, excluding the .code section.
class SelfNCCHArchive final : public ArchiveBackend {
public:
    explicit SelfNCCHArchive(const NCCHData& ncch_data_) : ncch_data(ncch_data_) {}

    std::string GetName() const override {
        return "SelfNCCHArchive";
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path, const Mode&) const override {
        // Note: SelfNCCHArchive doesn't check the open mode.

        if (path.GetType() != LowPathType::Binary) {
            LOG_ERROR(Service_FS, "Path need to be Binary");
            return ERROR_INVALID_PATH;
        }

        std::vector<u8> binary = path.AsBinary();
        if (binary.size() != sizeof(SelfNCCHFilePath)) {
            LOG_ERROR(Service_FS, "Wrong path size %zu", binary.size());
            return ERROR_INVALID_PATH;
        }

        SelfNCCHFilePath file_path;
        std::memcpy(&file_path, binary.data(), sizeof(SelfNCCHFilePath));

        switch (static_cast<SelfNCCHFilePathType>(file_path.type)) {
        case SelfNCCHFilePathType::UpdateRomFS:
            LOG_WARNING(Service_FS, "(STUBBED) open update RomFS");
            return OpenRomFS();

        case SelfNCCHFilePathType::RomFS:
            return OpenRomFS();

        case SelfNCCHFilePathType::Code:
            LOG_ERROR(Service_FS, "Reading the code section is not supported!");
            return ERROR_COMMAND_NOT_ALLOWED;

        case SelfNCCHFilePathType::ExeFS: {
            const auto& raw = file_path.exefs_filename;
            auto end = std::find(raw.begin(), raw.end(), '\0');
            std::string filename(raw.begin(), end);
            return OpenExeFS(filename);
        }
        default:
            LOG_ERROR(Service_FS, "Unknown file type %u!", static_cast<u32>(file_path.type));
            return ERROR_INVALID_PATH;
        }
    }

    ResultCode DeleteFile(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultCode RenameFile(const Path& src_path, const Path& dest_path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultCode DeleteDirectory(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultCode DeleteDirectoryRecursively(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultCode CreateFile(const Path& path, u64 size) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultCode CreateDirectory(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultCode RenameDirectory(const Path& src_path, const Path& dest_path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    u64 GetFreeBytes() const override {
        return 0;
    }

private:
    ResultVal<std::unique_ptr<FileBackend>> OpenRomFS() const {
        if (ncch_data.romfs_file) {
            return MakeResult<std::unique_ptr<FileBackend>>(std::make_unique<IVFCFile>(
                ncch_data.romfs_file, ncch_data.romfs_offset, ncch_data.romfs_size));
        } else {
            LOG_INFO(Service_FS, "Unable to read RomFS");
            return ERROR_ROMFS_NOT_FOUND;
        }
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenExeFS(const std::string& filename) const {
        if (filename == "icon") {
            if (ncch_data.icon) {
                return MakeResult<std::unique_ptr<FileBackend>>(
                    std::make_unique<ExeFSSectionFile>(ncch_data.icon));
            }

            LOG_WARNING(Service_FS, "Unable to read icon");
            return ERROR_EXEFS_SECTION_NOT_FOUND;
        }

        if (filename == "logo") {
            if (ncch_data.logo) {
                return MakeResult<std::unique_ptr<FileBackend>>(
                    std::make_unique<ExeFSSectionFile>(ncch_data.logo));
            }

            LOG_WARNING(Service_FS, "Unable to read logo");
            return ERROR_EXEFS_SECTION_NOT_FOUND;
        }

        if (filename == "banner") {
            if (ncch_data.banner) {
                return MakeResult<std::unique_ptr<FileBackend>>(
                    std::make_unique<ExeFSSectionFile>(ncch_data.banner));
            }

            LOG_WARNING(Service_FS, "Unable to read banner");
            return ERROR_EXEFS_SECTION_NOT_FOUND;
        }

        LOG_ERROR(Service_FS, "Unknown ExeFS section %s!", filename.c_str());
        return ERROR_INVALID_PATH;
    }

    NCCHData ncch_data;
};

ArchiveFactory_SelfNCCH::ArchiveFactory_SelfNCCH(Loader::AppLoader& app_loader) {
    std::shared_ptr<FileUtil::IOFile> romfs_file_;
    if (Loader::ResultStatus::Success ==
        app_loader.ReadRomFS(romfs_file_, ncch_data.romfs_offset, ncch_data.romfs_size)) {

        ncch_data.romfs_file = std::move(romfs_file_);
    }

    std::vector<u8> buffer;

    if (Loader::ResultStatus::Success == app_loader.ReadIcon(buffer))
        ncch_data.icon = std::make_shared<std::vector<u8>>(std::move(buffer));

    buffer.clear();
    if (Loader::ResultStatus::Success == app_loader.ReadLogo(buffer))
        ncch_data.logo = std::make_shared<std::vector<u8>>(std::move(buffer));

    buffer.clear();
    if (Loader::ResultStatus::Success == app_loader.ReadBanner(buffer))
        ncch_data.banner = std::make_shared<std::vector<u8>>(std::move(buffer));
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SelfNCCH::Open(const Path& path) {
    auto archive = std::make_unique<SelfNCCHArchive>(ncch_data);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SelfNCCH::Format(const Path&, const FileSys::ArchiveFormatInfo&) {
    LOG_ERROR(Service_FS, "Attempted to format a SelfNCCH archive.");
    return ERROR_INVALID_PATH;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SelfNCCH::GetFormatInfo(const Path&) const {
    LOG_ERROR(Service_FS, "Attempted to get format info of a SelfNCCH archive");
    return ERROR_INVALID_PATH;
}

} // namespace FileSys
