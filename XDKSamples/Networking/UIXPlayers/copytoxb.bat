@echo Run this batch file after building UIXPlayers, to copy the necessary
@echo resources over.

@xbmkdir xe:\samples
@xbmkdir xe:\samples\UIXPlayers
@xbmkdir xe:\samples\UIXPlayers\media

@echo Copying common sound resources...
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xwb" xe:\samples\UIXPlayers\media\
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xsb" xe:\samples\UIXPlayers\media\

@echo Build the default skin and copy it to the Xbox
@if not exist media mkdir media
@set SampleDir="%cd%"
@cd /d "%xdk%\source\uix"
@"%xdk%"\Xbox\Bin\skinbld default.inx %SampleDir%\media\UIXPlayers.uix
@cd /d %SampleDir%
@xbcp /y media\UIXPlayers.uix xe:\samples\UIXPlayers\media
