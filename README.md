# HexQuest
a HexChat plugin for making MUCKs easy to use through HexChat.
Totally not related to a certain cereal-based shoot-em-up.
Simply add a MUCK to the server list as you would an IRC server.

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
This is written with [SpinDizzy](http://www.spindizzy.org/nwp/) in mind specifically. Also a work in progress.

Use `/hquest account guest guest` to tell HexQuest to use a guest account when connecting. Afterwards, connect with `/server muck.spindizzy.org 7072`. Later when/if you create a character, you can run `/hquest account` again with the new information.

Now you're in! In a MUCK, you're only in one place at a time, therefore most of the activity will take place in the server tab. Messages typed here will be sent as server commands, and do not start with slashes like IRC ones.

If a list of rooms doesn't appear already, use `wa`, and see if there's any with any people in it. You will have to actually travel to that location, and there will usually be a list of directions in the form of a series of commands you should enter. If not, there will be coordinates (such as N0 E0) and you can travel to that location with `luge latitude longitude`.

Now that you're somewhere with people, try talking to the people there. To send a message to the room, prefix a line with `"`, or `ooc ` for an out of character message. Just like on IRC, actions are available too. You may either use `/me` as normal or prefix a line with `:`. OOC messages can contain actions too, as in `ooc :whatever`. You can read a description of the people around you with `look Name`.

There are two major forms of private messages. There's whispering (`wh Name=Text to send`), which only works within the same room, and paging (with HexQuest you can just use `/query Name` and open a standard query window to page) which works across the whole server.

To get your own character, use the form [here](http://www.spindizzy.org/nwp/get-started/) can be slow, or contact a wizard (which is like an administrator) if any are available for a faster response. You can use `wizzes` to see which wizards are available, and you can go `/query` one of them and ask them to make a character for you.

One thing to keep on mind is that, unlike IRC, you cannot just idle all day. There is a timer that will disconnect you if you're idle for too long, so try to connect to the MUCK when you're actually around.

More commands listed at [Things New Folks Should Know](http://www.spindizzy.org/nwp/things-new-folks-should-know/).
