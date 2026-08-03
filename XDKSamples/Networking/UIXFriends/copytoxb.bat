@echo Run this batch file after building UIXFriends, to copy the necessary
@echo resources over.

@xbmkdir xe:\samples
@xbmkdir xe:\samples\UIXFriends
@xbmkdir xe:\samples\UIXFriends\media

@echo Copying common sound resources...
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xwb" xe:\samples\UIXFriends\media\
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xsb" xe:\samples\UIXFriends\media\

@echo Build the default skin and copy it to the Xbox
@if not exist media mkdir media
@set SampleDir="%cd%"
@cd /d "%xdk%\source\uix"
@"%xdk%"\Xbox\Bin\skinbld default.inx %SampleDir%\media\UIXFriends.uix
@cd /d %SampleDir%
@xbcp /y media\UIXFriends.uix xe:\samples\UIXFriends\media
