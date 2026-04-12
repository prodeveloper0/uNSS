#include "zipio.hpp"

#include "miniz.h"
#include "fileio.hpp"


ZipWriter::ZipWriter()
{
    this->zipPath = "";
    this->pZip = NULL;
}


ZipWriter::ZipWriter(ZipWriter&& other) noexcept
{
    this->zipPath = std::move(other.zipPath);
    this->pZip = other.pZip;
    other.pZip = NULL;
}


ZipWriter::ZipWriter(const std::string& zipPath)
{
    this->zipPath = zipPath;
    this->pZip = NULL;
    _openZipArchive(zipPath.c_str());
}


ZipWriter::~ZipWriter()
{
    this->close();
}


ZipWriter& ZipWriter::operator=(ZipWriter&& other) noexcept
{
    this->zipPath = std::move(other.zipPath);
    this->pZip = other.pZip;
    other.pZip = NULL;
    return *this;
}


bool ZipWriter::add(const std::string& sourcePath, const std::string& destPath)
{
    return add(sourcePath, destPath, "");
}


bool ZipWriter::add(const std::string& sourcePath, const std::string& destPath, const std::string& comment)
{
    if (mz_zip_writer_add_file((mz_zip_archive*)pZip, destPath.c_str(), sourcePath.c_str(), comment.c_str(), (mz_uint16)comment.size(), MZ_DEFAULT_COMPRESSION))
    {
        return true;
    }
    return false;
}


bool ZipWriter::open(const std::string& path)
{
    close();
    zipPath = path;
    return _openZipArchive(zipPath.c_str());
}


void ZipWriter::close()
{
    _closeZipArchive();
}


bool ZipWriter::_openZipArchive(const char* path)
{
    pZip = (mz_zip_archive*)malloc(sizeof(mz_zip_archive));
    if (pZip != NULL)
    {
        memset(pZip, 0, sizeof(mz_zip_archive));
        mz_zip_writer_init_file((mz_zip_archive*)pZip, path, 0);
        return true;
    }
    return false;
}


void ZipWriter::_closeZipArchive()
{
    if (pZip != NULL)
    {
        if (mz_zip_writer_finalize_archive((mz_zip_archive*)pZip))
        {
            mz_zip_writer_end((mz_zip_archive*)pZip);
        }
        free(pZip);
        pZip = NULL;
    }
}


ZipReader::ZipReader()
{
    this->zipPath = "";
    this->pZip = NULL;
}


ZipReader::ZipReader(ZipReader&& other) noexcept
{
    this->zipPath = std::move(other.zipPath);
    this->pZip = other.pZip;
    other.pZip = NULL;
}


ZipReader::ZipReader(const std::string& zipPath)
{
    this->zipPath = zipPath;
    this->pZip = NULL;
    _openZipArchive(zipPath.c_str());
}


ZipReader::~ZipReader()
{
    this->close();
}


ZipReader& ZipReader::operator=(ZipReader&& other)
{
    this->zipPath = std::move(other.zipPath);
    this->pZip = other.pZip;
    other.pZip = NULL;
    return *this;
}


bool ZipReader::extractAll(const std::string& path)
{
    bool ret = true;
    walk([&](const std::string& fileName, ZipReader::EXTRACT_ONE_FUNC extractOne)
        {
            ret = extractOne(path + "/" + fileName);
            return ret;
        }
    );
    return !ret;
}


bool ZipReader::extractOne(const std::string& sourcePath, const std::string& destPath)
{
    const std::string::size_type lastSlash = destPath.rfind('/');
    if (lastSlash != std::string::npos)
    {
        const std::string parentDir = destPath.substr(0, lastSlash);
        recursiveMkdir(parentDir);
    }

    if (mz_zip_reader_extract_file_to_file((mz_zip_archive*)pZip, sourcePath.c_str(), destPath.c_str(), 0))
    {
        return true;
    }
    return false;
}


void ZipReader::walk(const ARCHIVE_WALK_CALLBACK_FUNC& callback)
{
    const mz_uint numCount = mz_zip_reader_get_num_files((mz_zip_archive*)pZip);
    for (mz_uint i = 0; i < numCount; ++i)
    {
        mz_zip_archive_file_stat fileStat;
        mz_zip_reader_file_stat((mz_zip_archive*)pZip, i, &fileStat);
        std::string fileName(fileStat.m_filename);
        if (!callback(
            fileName,
            [this, fileName](const std::string& destPath)
            {
                return extractOne(fileName, destPath);
            }
        ))
        {
            break;
        }
    }
}


bool ZipReader::open(const std::string& path)
{
    close();
    zipPath = path;
    return _openZipArchive(zipPath.c_str());
}


void ZipReader::close()
{
    _closeZipArchive();
}


bool ZipReader::_openZipArchive(const char* path)
{
    pZip = (mz_zip_archive*)malloc(sizeof(mz_zip_archive));
    if (pZip != NULL)
    {
        memset(pZip, 0, sizeof(mz_zip_archive));
        mz_zip_reader_init_file((mz_zip_archive*)pZip, path, 0);
        return true;
    }
    return false;
}


void ZipReader::_closeZipArchive()
{
    if (pZip != NULL)
    {
        mz_zip_reader_end((mz_zip_archive*)pZip);
        free(pZip);
        pZip = NULL;
    }
}
