//-----------------------------------------------------------------------------
// Name: Storage Sample
// 
// Copyright (c) 2002-2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The Storage Sample demonstrates a subset of the Xbox Live storage 
   functionality, such as uploading and downloading user settings directly from 
   title server directly from and to memory, respectively; uploading or 
   downloading team-related game content saved temporarily on the user's
   Xbox hard drive from or to the server, which requires an intricate security 
   procedure involving associating Xbox live signatures attached to any 
   hardware-stored content; and allowance of publishers to upload, from memory,
   title related files that could be downloaded by all users of the title.

   Xbox Live storage functionality is useful for customization of the user's experience
   while playing online. Applications for storage include customization of a team's
   jersey, team logo icons, or other variations upon the standard game art.

   Storage facilities are best divided into three catagories.

   The first category is "Per Title Per Team". This allocates space
   for 101 files for each team for a title. Since 100 teammates are
   allowed on a roster it is suggested that one shared resource
   be allowed for the entire team and one resource each per team member be allowed.
   Please refer to the XDK documentation for the current file size maximums.

   The second category is "Per User Per Title". This allows a user to have
   content that is sharable via Xbox Live, independent of other storage facilities.
   Other users have read-access to this content if they know the file name and user
   ID.
 
   The third category is "Publisher/Global". This allows a publisher to upload
   content that can be enumerated and downloaded by all logged-on Xbox Live 
   title users.

   One important aspect of storage is that the name of the file on the server 
   must be known to download the content. It is suggested that games name content 
   in a predictable manner. Suggestions include the ID number of the user with the 
   shared content or a static name such as SETTINGS.DAT.

   Because Storage deals with movement of content across the network, extra 
   security precautions must be taken. Content downloaded to the memory of an Xbox 
   must not be saved to the memory units or hard drive. Signature verification 
   must take place when reading content that will be shared across the network, 
   or when reading data that was retrieved from the network.


Programming Notes
=================

   This sample is structured into three implementation files:
                  
   StorageDemo.cpp       - Contains the main logic to demonstrate user and team
                           storage.

   UserContent.cpp       - Contains structure definition for purposes relating
                           to team content.

   UserSettings.cpp      - Contains structure definition for purposes relating
                           to user settings.


Running the Sample
==================

   Only one Xbox is required to run this sample. At least one Xbox Live account is
   also required to run part of this demo. Having multiple Xbox Live accounts is a 
   positive.  However, using the Teams sample is suggested to allow for the creation
   and managment of teams. For this reason, the Teams sample and the Storage sample 
   share the same Title ID.   
   

