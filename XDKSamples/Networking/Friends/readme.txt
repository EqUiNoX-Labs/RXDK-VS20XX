//-----------------------------------------------------------------------------
// Name: Friends
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample illustrates online friends on Xbox. Code shows how
   to add Friends, remove Friends and track Friends status using
   the XOnlineFriends API through a FriendsManager wrapper.  
   It also demonstrates using the XOnlineMutelist and XOnlineFeedback APIs,
   also through the wrapper.


   IMPORTANT NOTE:

   In a real Xbox application, you would have a player list which would provide
   the list of accounts to choose from for muting, making friends and providing
   feedback.

   However, since there is no server here and we are only demonstrating the
   use of the friends APIs, we use the user accounts on the Xbox as the 
   potential friends list and the current friends list as a proxy player list
   for muting and feedback.



Programming Notes
=================
   The XOnlineFeedbackSend function is used to provide feedback for a friend,
   although feedback can be provided for any online user, not just friends.
