#ifndef FILE_TYPE_APP
#define FILE_TYPE_APP

#include <Application.h>
#include <FilePanel.h>

class FileTypeWindow;

class FileTypeApp
	: public BApplication
{
public:
					FileTypeApp();
	virtual	void	ArgvReceived(int32 argc, const char *argv[], const char * cwd);
	virtual void	RefsReceived(BMessage *message);
	virtual void	ReadyToRun();

	virtual	void	DispatchMessage(BMessage *an_event, BHandler *handler);

private:
	BFilePanel *	OpenPanel();
	void			PrintUsage(const char * execname);

	FileTypeWindow	* fWindow;
	BFilePanel		* fOpenPanel;

	bool			fArgvOkay;
};

extern FileTypeApp * file_types_app;

#endif // FILE_TYPE_APP
