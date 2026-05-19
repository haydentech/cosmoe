// MimeUpdateTest.cpp

#include "MimeUpdateTest.h"

#include <stdlib.h>
#include <string.h>

#include <string>

#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Mime.h>
#include <MimeType.h>
#include <Node.h>
#include <Path.h>
#include <Resources.h>
#include <String.h>

#include <TestUtils.h>


using std::string;


namespace {

const char* kTestDir = "/tmp/mimeUpdateTestDir";


class MimeInfoTestFile {
public:
	MimeInfoTestFile(string fileName, string mimeType, const void* fileData = NULL,
		int32 fileSize = -1)
		:
		name(fileName),
		type(mimeType)
	{
		if (fileData != NULL) {
			int32 size = fileSize == -1 ? strlen((const char*)fileData) : fileSize;
			data.assign((const char*)fileData, size);
		}
	}

	status_t Create() const
	{
		BFile file(name.c_str(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
		status_t status = file.InitCheck();
		if (status == B_OK && !data.empty()) {
			ssize_t written = file.Write(data.data(), data.size());
			if (written < 0)
				status = written;
			else if (written != (ssize_t)data.size())
				status = B_ERROR;
		}
		return status;
	}

	status_t Delete() const
	{
		return BEntry(name.c_str()).Remove();
	}

	string	name;
	string	type;
	string	data;
};


class AppMimeTestFile {
public:
	AppMimeTestFile(string fileName, string signature, const void* miniIconData)
		:
		name(fileName),
		signature(signature),
		miniIcon(NULL)
	{
		if (miniIconData != NULL) {
			miniIcon = new char[256];
			memcpy(miniIcon, miniIconData, 256);
		}
	}

	~AppMimeTestFile()
	{
		delete[] miniIcon;
	}

	status_t Create(bool setAttributes, bool setResources) const
	{
		BFile file(name.c_str(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
		status_t status = file.InitCheck();

		if (status == B_OK && setAttributes) {
			BString type(B_ELF_APP_MIME_TYPE);
			BString sig(signature.c_str());
			status = file.WriteAttrString("BEOS:TYPE", &type);
			if (status == B_OK)
				status = file.WriteAttrString("BEOS:APP_SIG", &sig);
			if (status == B_OK && miniIcon != NULL) {
				ssize_t written = file.WriteAttr("BEOS:M:STD_ICON", 'MICN', 0,
					miniIcon, 256);
				if (written < 0)
					status = written;
				else if (written != 256)
					status = B_ERROR;
			}
		}

		if (status == B_OK && setResources) {
			BResources resources;
			status = resources.SetTo(&file, true);
			if (status == B_OK) {
				status = resources.AddResource(B_STRING_TYPE, 1,
					signature.c_str(), signature.length() + 1, "BEOS:APP_SIG");
			}
			if (status == B_OK && miniIcon != NULL) {
				status = resources.AddResource('MICN', 101, miniIcon, 256,
					"BEOS:M:STD_ICON");
			}
		}

		return status;
	}

	status_t Delete(bool deleteMimeType) const
	{
		status_t status = BEntry(name.c_str()).Remove();
		if (status == B_OK && deleteMimeType) {
			BMimeType type;
			status = type.SetTo(signature.c_str());
			if (status == B_OK && type.IsInstalled())
				status = type.Delete();
			else if (status == B_OK)
				status = B_OK;
		}
		return status;
	}

	string	name;
	string	signature;
	char*	miniIcon;
};

}


CppUnit::Test*
MimeUpdateTest::Suite()
{
	CppUnit::TestSuite* suite = new CppUnit::TestSuite();
	typedef CppUnit::TestCaller<MimeUpdateTest> TC;

	suite->addTest(new TC("update_mime_info() Test",
		&MimeUpdateTest::UpdateMimeInfoTest));
	suite->addTest(new TC("create_app_meta_mime() Test",
		&MimeUpdateTest::CreateAppMetaMimeTest));
	return suite;
}


void
MimeUpdateTest::setUp()
{
	BasicTest::setUp();
	execCommand(string("rm -rf ") + TestDir());
	execCommand(string("mkdir -p ") + TestDir());
}


void
MimeUpdateTest::tearDown()
{
	execCommand(string("rm -rf ") + TestDir());
	BasicTest::tearDown();
}


const char*
MimeUpdateTest::TestDir() const
{
	return kTestDir;
}


status_t
MimeUpdateTest::MimeDatabaseDir(BPath& path) const
{
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path, true);
	if (status == B_OK)
		status = path.Append("mime_db");
	return status;
}


void
MimeUpdateTest::UpdateMimeInfoTest()
{
	execCommand(string("mkdir -p ") + TestDir() + "/subdir1 " + TestDir()
		+ "/subdir2/subsubdir1");

	MimeInfoTestFile files[] = {
		MimeInfoTestFile(string(TestDir()) + "/file1.cpp",
			"application/octet-stream",
			"#include <stdio.h>\nint main() { return 0; }\n"),
		MimeInfoTestFile(string(TestDir()) + "/subdir1/file1.gif",
			"application/octet-stream", "GIF89a", 6),
		MimeInfoTestFile(string(TestDir()) + "/subdir2/subsubdir1/file1",
			"application/octet-stream", "<html>\n<body>\n</body></html>\n")
	};
	const int32 fileCount = sizeof(files) / sizeof(MimeInfoTestFile);

	for (int32 i = 0; i < fileCount; i++) {
		NextSubTest();
		CHK(files[i].Create() == B_OK);
		CHK(update_mime_info(files[i].name.c_str(), false, true, false) == B_OK);

		BNode node(files[i].name.c_str());
		CHK(node.InitCheck() == B_OK);
		BString type;
		CHK(node.ReadAttrString("BEOS:TYPE", &type) == B_OK);
		CHK(type.Length() > 0);
		CHK(files[i].Delete() == B_OK);
	}

	NextSubTest();
	for (int32 i = 0; i < fileCount; i++)
		CHK(files[i].Create() == B_OK);

	CHK(update_mime_info(TestDir(), false, true, false) == B_OK);
	for (int32 i = 0; i < fileCount; i++) {
		BNode node(files[i].name.c_str());
		BString type;
		CHK(node.ReadAttrString("BEOS:TYPE", &type) == B_ENTRY_NOT_FOUND);
	}

	CHK(update_mime_info(TestDir(), true, true, false) == B_OK);
	for (int32 i = 0; i < fileCount; i++) {
		BNode node(files[i].name.c_str());
		BString type;
		CHK(node.ReadAttrString("BEOS:TYPE", &type) == B_OK);
		CHK(type.Length() > 0);
		CHK(files[i].Delete() == B_OK);
	}

	NextSubTest();
	CHK(update_mime_info(string(TestDir() + string("/does-not-exist")).c_str(),
		false, true, false) == B_OK);
}


void
MimeUpdateTest::CreateAppMetaMimeTest()
{
	char miniIcon[256];
	for (int32 i = 0; i < 256; i++)
		miniIcon[i] = (char)i;

	AppMimeTestFile file(string(TestDir()) + "/app-file",
		"application/x-vnd.cosmoe-mime-update-test", miniIcon);
	BMimeType mimeType(file.signature.c_str());
	if (mimeType.InitCheck() == B_OK && mimeType.IsInstalled())
		CHK(mimeType.Delete() == B_OK);

	NextSubTest();
	CHK(file.Create(true, true) == B_OK);
	CHK(create_app_meta_mime(file.name.c_str(), false, true, false) == B_OK);

	BPath mimeDatabaseDir;
	CHK(MimeDatabaseDir(mimeDatabaseDir) == B_OK);

	BNode typeNode;
	CHK(typeNode.SetTo((string(mimeDatabaseDir.Path()) + "/" + file.signature).c_str())
		== B_OK);

	BString ppath;
	CHK(typeNode.ReadAttrString("META:PPATH", &ppath) == B_OK);
	BPath appPath(file.name.c_str(), NULL, true);
	CHK(ppath == appPath.Path());

	CHK(mimeType.SetTo(file.signature.c_str()) == B_OK);
	CHK(mimeType.IsInstalled() == true);

	char shortDescription[B_MIME_TYPE_LENGTH + 1];
	CHK(mimeType.GetShortDescription(shortDescription) == B_OK);
	CHK(file.name == shortDescription);

	char preferredApp[B_MIME_TYPE_LENGTH + 1];
	CHK(mimeType.GetPreferredApp(preferredApp) == B_OK);
	CHK(file.signature == preferredApp);

	BBitmap icon(BRect(0, 0, 15, 15), B_CMAP8);
	CHK(mimeType.GetIcon(&icon, B_MINI_ICON) == B_OK);
	CHK(memcmp(icon.Bits(), miniIcon, 256) == 0);
	CHK(file.Delete(true) == B_OK);

}