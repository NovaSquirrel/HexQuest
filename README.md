# HexQuest
a HexChat plugin for making MUCKs easy to use through HexChat.
Totally not related to a certain cereal-based shoot-em-up.
Simply add a MUCK to the server list as you would an IRC server.

This way you get all the features HexChat has while using MUCK servers, and more features:

 * Marker line for new text
 * Spell check
 * Character counter (if using my plugin for that)
 * Separate tabs for pages, where you can just type into them without typing "p name=text"
 * See your previous conversation with someone when you page them
 * Shortcut to speed up whispering to people

## Features
Each can be enabled or disabled with `/hquest feature_name on/off`

 * `page_tabs` - Moves pages to a separate tab.
 * `auto_quote` - Automatically converts messages typed on the server tab into server commands. Also converts /me to a pose.
 * `ignore_away` - Prevents `/away` and `/back` from being sent to the server.
 * `eat_pages` - If `page_tabs` is on, the pages will only appear in the separate tab, not the server one.
 * `bold_whisper` - Bolds lines containing whispers.
 * `flash_whisper` - (Requires `bold_whisper`) Makes HexChat's tab flash when you get whispered.

Additionally, if `bold_whisper` is enabled, pressing the tab key on the server tab will fill in a whisper addressed to the most recent person you whispered to.

## Configuration

 * `/hquest force` - Forces the current server to be seen as a MUCK one.
 * `/hquest account username password` - Sets the username and password for your character.
 * `/hquest muck_identifier text` - Change the string that identifies the server as a MUCK, defaults to "#$#mcp version:".
 * `/hquest idle_timeout_string text` - Change the string that identifies that the MUCK disconnected you for being idle, and that you should not auto-reconnect.

## Limitations

 * Multi-person pages are not implemented yet, and they probably just go to the server tab.
 * Only one MUCK connection and character is supported at a time.
 * No notices when your name is said.

## A MUCK tutorial for IRC users
[NovaSquirrel's MUCK tutorial, using HexChat](http://wiki.novasquirrel.com/MUCK%20tutorial).
