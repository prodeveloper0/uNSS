#pragma once
#include <string>
#include <functional>


class ZipWriter
{
private:
    std::string zipPath;
    void* pZip;

public:
    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;

    ZipWriter();
    ZipWriter(ZipWriter&& other);
    ZipWriter(const std::string& zipPath);

    virtual ~ZipWriter();

	ZipWriter& operator=(ZipWriter&& other);

public:
    bool add(const std::string& sourcePath, const std::string& destPath);
    bool add(const std::string& sourcePath, const std::string& destPath, const std::string& comment);
    bool open(const std::string& path);
    void close();

private:
    bool _openZipArchive(const char* path);
    void _closeZipArchive();
};


class ZipReader
{
private:
	std::string zipPath;
	void* pZip;

public:
    using EXTRACT_ONE_FUNC = std::function<bool(const std::string&)>;
	using ARCHIVE_WALK_CALLBACK_FUNC = std::function<bool(const std::string&, EXTRACT_ONE_FUNC)>;

public:
	ZipReader(const ZipReader&) = delete;
	ZipReader& operator=(const ZipReader&) = delete;

    ZipReader();
	ZipReader(ZipReader&& other);
	ZipReader(const std::string& zipPath);

	virtual ~ZipReader();

	ZipReader& operator=(ZipReader&& other);

public:
    bool extractAll(const std::string& path);
    bool extractOne(const std::string& sourcePath, const std::string& destPath);

    void walk(const ARCHIVE_WALK_CALLBACK_FUNC& callback);

    bool open(const std::string& path);
    void close();

private:
	bool _openZipArchive(const char* path);
	void _closeZipArchive();
};
