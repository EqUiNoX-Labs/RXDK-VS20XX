//-----------------------------------------------------------------------------
// Name: DynamicFont Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The DynamicFont sample illustrates how to create a texture-based font on the
   fly. An app would need to do this after downloading a new set of localized
   strings.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Texture-based fonts are great for performance, but they are created at build
   time to contain a fixed set of glyphs. If an online game downloads new
   content that contains new localized text, the existing font may not contain
   all the glyphs to render the new strings.

   One solution is to use the XFONT API. However, the performance of those API
   is not sufficient for real-time use. Therefore, a better solution is to use
   the XFONT API to create a texture-based font immediately after downloading
   any new strings. This sample does exactly that.

   To create a dynamic texture-based font, the app must first create an XFont
   of the appropriate font, size, and style. Then, the app must construct an
   array which indicates which glyphs are valid. This can be done simply by
   walking through the list of localized strings. With this information, the app
   can then create the texture-based font, and draw text with it as normal.




