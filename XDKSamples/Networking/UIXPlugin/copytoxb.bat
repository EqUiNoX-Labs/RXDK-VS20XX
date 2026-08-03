@rem Run this batch file after building UIXPlugin, to copy the necessary resources over.

@echo Creating the sample directory...
@xbmkdir xe:\samples
@xbmkdir xe:\samples\UIXPlugin
@xbmkdir xe:\samples\UIXPlugin\media

@echo Copying common sound resources...
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xwb" xe:\samples\UIXPlugin\media\
@xbcp /y "%XDK%\redist\uix\media\UIXDefault.xsb" xe:\samples\UIXPlugin\media\

@echo Build the default skin and copy it to the Xbox
@if not exist media mkdir media
@set SampleDir="%cd%"
@cd /d "%xdk%\source\uix"
@"%xdk%"\Xbox\Bin\skinbld /header default.inx %SampleDir%\media\UIXPlugin.uix
@cd /d %SampleDir%
@rem Copy the resource header file that was built with skinbld /header
@copy "%xdk%\source\uix\sk_res.h" .
@xbcp /y media\UIXPlugin.uix xe:\samples\UIXPlugin\media
