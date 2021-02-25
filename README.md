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
 * Set specific words to be highlighted in a different color

## Features
Each can be enabled or disabled with `/hquest feature_name on/off`. For example, to turn `server_flash` on, you'd do `/hquest server_flash on`.

 * `page_tabs` - Moves pages to a separate tab.
 * `auto_quote` - Automatically converts messages typed on the server tab into server commands. Also converts /me to a pose.
 * `ignore_away` - Prevents `/away` and `/back` from being sent to the server.
 * `eat_pages` - If `page_tabs` is on, the pages will only appear in the separate tab, not the server one.
 * `bold_whisper` - Bolds lines containing whispers.
 * `flash_whisper` - (Requires `bold_whisper`) Makes HexChat's tab flash when you get whispered.
 * `server_flash` - Flash HexChat's tab any time the server tab receives text. Not saved in the config file.
 * `zombie_flash` - Flash HexChat's tab any time a zombie receives text. Not saved in the config file.
 * `zombie_print_events` - Use printing instead of privmsg for zombies, so you don't get notifications for every line of text.

Additionally, if `bold_whisper` is enabled, pressing the tab key on the server tab will fill in a whisper addressed to the most recent person you whispered to.

## Configuration

 * `/hquest force` - Forces the current server to be seen as a MUCK one.
 * `/hquest account username password` - Sets the username and password for your character.
 * `/hquest muck_identifier text` - Change the string that identifies the server as a MUCK, defaults to "#$#mcp version:".
 * `/hquest idle_timeout_string text` - Change the string that identifies that the MUCK disconnected you for being idle, and that you should not auto-reconnect.
 * `/hquest echo_cmd number command` - Choose specific commands that should echo the command into the window. This is helpful for making sure uses of "look" and "lookat" actually go into the logs, so it's much easier to find a description in them.

## Highlighting

`/hquest highlight_word number word` will set specific words to highlight with a specific color. `/hquest highlight_color color` will set the 2-digit IRC color code to use, defaulting to 9 (green). The "word" can actually be multiple words, and it is not case sensitive.

`/hquest highlight_level number` will choose how severe the response to those words should be. 0 does no action, 1 colors only the word, 2 recolors the server tab, 3 flashes HexChat's whole tab. You can't select per-word yet, and the default is 1.

## Zombies

You can have 10 zombies by default, numbered 0 to 9. Use `/hquest zombie number prefix name` to specify the in-world name of the zombie as well as the command prefix you have set for the zombie.

For example, I have the command prefix set to `aaa` and my zombie is named `'NovaSquirrel`, so I do `/hquest zombie 0 aaa 'NovaSquirrel`.

Open a query to $Z0, where 0 is replaced by the zombie number, to do actions as that zombie.

## Limitations

 * Multi-person pages are not implemented yet, and they probably just go to the server tab.
 * Only one MUCK connection and character is supported at a time.

## A MUCK tutorial for IRC users
[NovaSquirrel's MUCK tutorial, using HexChat](http://wiki.novasquirrel.com/MUCK%20tutorial).
