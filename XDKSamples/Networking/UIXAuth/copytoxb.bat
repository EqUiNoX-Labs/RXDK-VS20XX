@rem Run this batch file after building UIXAuth, to copy the necessary resources over.

@echo Creating the sample directory...
@xbmkdir xe:\samples
@xbmkdir xe:\samples\UIXAuth
@xbmkdir xe:\samples\UIXAuth\media

@echo Copying common sound resources...
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xwb" xe:\samples\UIXAuth\media\
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xsb" xe:\samples\UIXAuth\media\

@echo Build the default skin and copy it to the Xbox
@if not exist media mkdir media
@set SampleDir="%cd%"
@cd /d "%xdk%\source\uix"
@"%xdk%"\Xbox\Bin\skinbld default.inx %SampleDir%\media\UIXAuth.uix
@cd /d %SampleDir%
@xbcp /y media\UIXAuth.uix xe:\samples\UIXAuth\media
