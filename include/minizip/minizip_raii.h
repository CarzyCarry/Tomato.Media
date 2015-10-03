//
// Tomato Media Core
// Minizip RAII ����
// ���ߣ�SunnyCase
// ����ʱ�䣺2015-09-30
//
#pragma once
#include "common.h"
#include "unzip.h"

class unzFile_raii
{
public:
	unzFile_raii() noexcept
		: _file(nullptr)
	{

	}

	explicit unzFile_raii(unzFile file) noexcept
		: _file(file)
	{

	}

	~unzFile_raii()
	{
		if (_file)
			unzClose(_file);
	}

	unzFile get() const noexcept
	{
		return _file;
	}

	void reset(unzFile file = nullptr)
	{
		if (_file)
			unzClose(_file);
		_file = file;
	}

	bool valid() const noexcept
	{
		return _file != nullptr;
	}
private:
	unzFile _file;
};