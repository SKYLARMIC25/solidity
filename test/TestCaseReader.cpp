/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#include <test/TestCaseReader.h>
#include <test/Common.h>

#include <libsolidity/parsing/Parser.h>
#include <liblangutil/ErrorReporter.h>
#include <libsolutil/StringUtils.h>
#include <libsolutil/CommonIO.h>

#include <boost/algorithm/string.hpp>
#include <boost/throw_exception.hpp>
#include <boost/filesystem.hpp>

#include <range/v3/view/map.hpp>

using namespace std;
using namespace solidity::langutil;
using namespace solidity::frontend::test;

namespace fs = boost::filesystem;

TestCaseReader::TestCaseReader(string const& _filename): m_fileStream(_filename), m_fileName(_filename)
{
	if (!m_fileStream)
		BOOST_THROW_EXCEPTION(runtime_error("Cannot open file: \"" + _filename + "\"."));
	m_fileStream.exceptions(ios::badbit);

	tie(m_sources, m_lineNumber) = parseSourcesAndSettingsWithLineNumber(m_fileStream);
	m_unreadSettings = m_settings;
}

TestCaseReader::TestCaseReader(istringstream const& _str)
{
	tie(m_sources, m_lineNumber) = parseSourcesAndSettingsWithLineNumber(
		static_cast<istream&>(const_cast<istringstream&>(_str))
	);
}

string const& TestCaseReader::source() const
{
	if (m_sources.sources.size() != 1)
		BOOST_THROW_EXCEPTION(runtime_error("Expected single source definition, but got multiple sources."));
	return m_sources.sources.at(m_sources.mainSourceFile);
}

string TestCaseReader::simpleExpectations()
{
	return parseSimpleExpectations(m_fileStream);
}

bool TestCaseReader::boolSetting(std::string const& _name, bool _defaultValue)
{
	if (m_settings.count(_name) == 0)
		return _defaultValue;

	m_unreadSettings.erase(_name);
	string value = m_settings.at(_name);
	if (value == "false")
		return false;
	if (value == "true")
		return true;

	BOOST_THROW_EXCEPTION(runtime_error("Invalid Boolean value: " + value + "."));
}

size_t TestCaseReader::sizetSetting(std::string const& _name, size_t _defaultValue)
{
	if (m_settings.count(_name) == 0)
		return _defaultValue;

	m_unreadSettings.erase(_name);

	static_assert(sizeof(unsigned long) <= sizeof(size_t));
	return stoul(m_settings.at(_name));
}

string TestCaseReader::stringSetting(string const& _name, string const& _defaultValue)
{
	if (m_settings.count(_name) == 0)
		return _defaultValue;

	m_unreadSettings.erase(_name);
	return m_settings.at(_name);
}

void TestCaseReader::ensureAllSettingsRead() const
{
	if (!m_unreadSettings.empty())
		BOOST_THROW_EXCEPTION(runtime_error(
			"Unknown setting(s): " +
			util::joinHumanReadable(m_unreadSettings | ranges::views::keys)
		));
}

pair<SourceMap, size_t> TestCaseReader::parseSourcesAndSettingsWithLineNumber(istream& _stream)
{
	map<string, string> sources;
	string currentSourceName;
	string currentSource;
	string line;
	size_t lineNumber = 1;
	static string const externalSourceDelimiterStart("==== ExternalSource:");
	static string const sourceDelimiterStart("==== Source:");
	static string const sourceDelimiterEnd("====");
	static string const comment("// ");
	static string const settingsDelimiter("// ====");
	static string const delimiter("// ----");
	static langutil::EVMVersion evmVersion = solidity::test::CommonOptions::get().evmVersion();
	bool sourcePart = true;
	while (getline(_stream, line))
	{
		lineNumber++;

		if (boost::algorithm::starts_with(line, delimiter))
			break;
		else if (boost::algorithm::starts_with(line, settingsDelimiter))
			sourcePart = false;
		else if (sourcePart)
		{
			if (boost::algorithm::starts_with(line, sourceDelimiterStart) && boost::algorithm::ends_with(line, sourceDelimiterEnd))
			{
				if (!(currentSourceName.empty() && currentSource.empty()))
					sources[currentSourceName] = std::move(currentSource);
				currentSource = {};
				currentSourceName = boost::trim_copy(line.substr(
					sourceDelimiterStart.size(),
					line.size() - sourceDelimiterEnd.size() - sourceDelimiterStart.size()
				));
				if (sources.count(currentSourceName))
					BOOST_THROW_EXCEPTION(runtime_error("Multiple definitions of test source \"" + currentSourceName + "\"."));
			}
			else if (boost::algorithm::starts_with(line, externalSourceDelimiterStart) && boost::algorithm::ends_with(line, sourceDelimiterEnd))
			{
				string externalSource = boost::trim_copy(line.substr(
					externalSourceDelimiterStart.size(),
					line.size() - sourceDelimiterEnd.size() - externalSourceDelimiterStart.size()
				));
				string externalSourceName = externalSource;

				// Does the external source define a remapping?
				size_t remappingPos = externalSource.find('=');
				if (remappingPos != string::npos)
				{
					externalSourceName = boost::trim_copy(externalSource.substr(0, remappingPos));
					externalSource = boost::trim_copy(externalSource.substr(remappingPos + 1));
				}

				fs::path testCaseParentDir = canonical(fs::path{m_fileName}).parent_path();
				fs::path externalSourcePath(externalSource);
				if (!fs::path(externalSource).is_relative())
					BOOST_THROW_EXCEPTION(runtime_error(string("External Source need to be relative.")));
				fs::path externalSourceFullPath = testCaseParentDir / externalSourcePath;

				string externalSourceContent;
				if (fs::exists(externalSourceFullPath))
					externalSourceContent = util::readFileAsString(externalSourceFullPath.string());
				else
					BOOST_THROW_EXCEPTION(runtime_error("External Source '" + externalSourcePath.string() + "' not found."));

				if (!externalSourceName.empty())
				{
					ErrorList errorList;
					ErrorReporter errorReporter(errorList);
					shared_ptr<Scanner> scanner = make_shared<Scanner>(CharStream(externalSourceContent, externalSourceName));
					ASTPointer<SourceUnit> sourceUnit = Parser(errorReporter, evmVersion).parse(scanner);
					solAssert(sourceUnit != nullptr, "");
					if (sourceUnit)
						for (auto const* import: ASTNode::filteredNodes<ImportDirective>(sourceUnit->nodes()))
						{
							fs::path externalSourceParentDir = fs::path(externalSource).parent_path();
							solAssert(externalSourceParentDir.is_relative() && fs::path(import->path()).is_relative(), "");
							string importedSourceContent = util::readFileAsString(
								(testCaseParentDir / externalSourceParentDir / import->path()).string()
							);
							sources[(externalSourceParentDir / import->path()).generic_string()] = importedSourceContent;
							sources[import->path()] = importedSourceContent;
						}

					sources[externalSourceName] = externalSourceContent;
				}
			}
			else
				currentSource += line + "\n";
		}
		else if (boost::algorithm::starts_with(line, comment))
		{
			size_t colon = line.find(':');
			if (colon == string::npos)
				BOOST_THROW_EXCEPTION(runtime_error(string("Expected \":\" inside setting.")));
			string key = line.substr(comment.size(), colon - comment.size());
			string value = line.substr(colon + 1);
			boost::algorithm::trim(key);
			boost::algorithm::trim(value);
			m_settings[key] = value;
		}
		else
			BOOST_THROW_EXCEPTION(runtime_error(string("Expected \"//\" or \"// ---\" to terminate settings and source.")));
	}
	// Register the last source as the main one
	sources[currentSourceName] = currentSource;
	return {{move(sources), move(currentSourceName)}, lineNumber};
}

string TestCaseReader::parseSimpleExpectations(istream& _file)
{
	string result;
	string line;
	while (getline(_file, line))
		if (boost::algorithm::starts_with(line, "// "))
			result += line.substr(3) + "\n";
		else if (line == "//")
			result += "\n";
		else
			BOOST_THROW_EXCEPTION(runtime_error("Test expectations must start with \"// \"."));
	return result;
}
